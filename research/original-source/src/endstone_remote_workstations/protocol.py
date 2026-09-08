"""Bounded r26_u4 storage codec prototype, payloads without packet headers.

Synthetic fixtures establish schema conformance only, not client compatibility.
No live packet handlers import this module in release 0.1.0.
"""
from dataclasses import dataclass
import hashlib
import json
import struct
from .model import Action, Ref, Rejected, Request, Slot


class CodecError(ValueError):
    pass


def uvar(value, bits=32):
    if type(value) is not int or not 0 <= value < 1 << bits:
        raise CodecError("unsigned integer out of range")
    result = bytearray()
    while value >= 128:
        result.append((value & 127) | 128)
        value >>= 7
    result.append(value)
    return bytes(result)


def svar(value, bits=32):
    if type(value) is not int or not -(1 << (bits-1)) <= value < 1 << (bits-1):
        raise CodecError("signed integer out of range")
    return uvar((value << 1) ^ (value >> (bits-1)), bits)


class Reader:
    def __init__(self, payload, limit=65536):
        if not isinstance(payload, bytes) or len(payload) > limit:
            raise CodecError("payload size exceeded")
        self.data, self.offset = payload, 0

    def raw(self, length):
        if length < 0 or self.offset+length > len(self.data):
            raise CodecError("truncated payload")
        value = self.data[self.offset:self.offset+length]
        self.offset += length
        return value

    def byte(self):
        return self.raw(1)[0]

    def boolean(self):
        value = self.byte()
        if value not in (0, 1):
            raise CodecError("noncanonical boolean")
        return bool(value)

    def uvar(self, bits=32):
        start = self.offset
        value = 0
        for shift in range(0, bits, 7):
            byte = self.byte()
            value |= (byte & 127) << shift
            if not byte & 128:
                if value >= 1 << bits or self.data[start:self.offset] != uvar(value, bits):
                    raise CodecError("noncanonical or overflowing varint")
                return value
        raise CodecError("varint too long")

    def svar(self, bits=32):
        value = self.uvar(bits)
        return (value >> 1) ^ -(value & 1)

    def end(self):
        if self.offset != len(self.data):
            raise CodecError("trailing payload data")


@dataclass(frozen=True)
class Open:
    window: int
    container_type: int
    x: int
    y: int
    z: int
    actor_id: int = -1

    def encode(self):
        if not 0 <= self.window <= 255 or not -128 <= self.container_type <= 255:
            raise CodecError("invalid window or container type")
        return bytes((self.window, self.container_type & 255)) + b"".join(
            svar(v) for v in (self.x, self.y, self.z)) + svar(self.actor_id, 64)

    @classmethod
    def decode(cls, payload):
        reader = Reader(payload)
        value = cls(reader.byte(), reader.byte(), reader.svar(), reader.svar(), reader.svar(), reader.svar(64))
        reader.end()
        return value


def close_payload(window, container_type, server_initiated):
    if not 0 <= window <= 255 or not -128 <= container_type <= 255 or type(server_initiated) is not bool:
        raise CodecError("invalid close")
    return bytes((window, container_type & 255, int(server_initiated)))


@dataclass(frozen=True)
class ItemDescriptor:
    """Wire data from an authoritative registry/snapshot, not a creation request.

    user_data retains complete NBT, canPlaceOn, canDestroy and item-specific data.
    Constructing this from ItemStack.nbt alone has NOT been validated.
    """
    numeric_id: int = 0
    count: int = 0
    aux: int = 0
    net_id: int | None = None
    block_runtime_id: int = 0
    user_data: bytes = b""

    def encode(self):
        if not -32768 <= self.numeric_id <= 32767 or not 0 <= self.count <= 255 or not 0 <= self.aux <= 32767:
            raise CodecError("invalid item descriptor")
        if len(self.user_data) > 1024*1024:
            raise CodecError("item user data too large")
        return (struct.pack("<hH", self.numeric_id, self.count) + uvar(self.aux)
                + (b"\x00" if self.net_id is None else b"\x01"+svar(self.net_id))
                + uvar(self.block_runtime_id) + uvar(len(self.user_data)) + self.user_data)


def full_container(role, dynamic_id=None):
    if not 0 <= role <= 255:
        raise CodecError("invalid slot role")
    if dynamic_id is None:
        return bytes((role, 0))
    if not 0 <= dynamic_id < 2**32:
        raise CodecError("invalid dynamic container id")
    return bytes((role, 1)) + struct.pack("<I", dynamic_id)


def inventory_content(window, items, role=7, storage=ItemDescriptor()):
    if not 0 <= window <= 255 or len(items) > 54:
        raise CodecError("invalid inventory content shape")
    return uvar(window)+uvar(len(items))+b"".join(i.encode() for i in items)+full_container(role)+storage.encode()


def inventory_slot(window, slot, item, *, role=None, storage=None):
    if not 0 <= window <= 255 or not 0 <= slot <= 255:
        raise CodecError("invalid inventory slot shape")
    return (bytes((window,))+uvar(slot)+(b"\x00" if role is None else b"\x01"+full_container(role))
            +(b"\x00" if storage is None else b"\x01"+storage.encode())+item.encode())


def error_responses(request_ids):
    if not 1 <= len(request_ids) <= 100:
        raise CodecError("invalid response batch")
    # ItemStackResponseInfo has a fixed true + optional containers, even on error.
    return uvar(len(request_ids))+b"".join(b"\x01"+svar(i)+b"\x01\x00" for i in request_ids)


@dataclass(frozen=True)
class WireRef:
    role: int
    dynamic_id: int | None
    slot: int
    net_id: int


@dataclass(frozen=True)
class WireAction:
    kind: str
    source: WireRef
    destination: WireRef | None
    count: int


@dataclass(frozen=True)
class WireRequest:
    request_id: int
    actions: tuple[WireAction, ...]


def read_ref(reader):
    role = reader.byte()
    dynamic = struct.unpack("<I", reader.raw(4))[0] if reader.boolean() else None
    return WireRef(role, dynamic, reader.byte(), struct.unpack("<i", reader.raw(4))[0])


def decode_storage_requests(payload):
    reader = Reader(payload)
    count = reader.uvar()
    if not 1 <= count <= 100:
        raise CodecError("invalid request count")
    result = []
    for _ in range(count):
        request_id = reader.svar()
        if request_id >= 0 or request_id % 2 != 1:
            raise CodecError("client request id is not negative odd")
        actions_count = reader.uvar()
        if not 1 <= actions_count <= 100:
            raise CodecError("invalid action count")
        actions = []
        for _ in range(actions_count):
            variant, inner_type = reader.uvar(), reader.byte()
            if variant not in (0, 1, 2, 3) or inner_type != variant:
                raise CodecError("unsupported or mismatched action discriminant")
            amount = reader.byte() if variant != 2 else 0
            if variant != 2 and not 1 <= amount <= 64:
                raise CodecError("invalid amount")
            source = read_ref(reader)
            destination = read_ref(reader) if variant != 3 else None
            if variant == 3:
                reader.boolean()  # random drop hint never authorizes a side effect
            actions.append(WireAction(("take", "place", "swap", "drop")[variant], source, destination, amount))
        strings = reader.uvar()
        if strings != 0:
            raise CodecError("storage requests cannot rename or supply filter strings")
        reader.raw(4)  # TextProcessingEventOrigin is fixed int32, not varint
        result.append(WireRequest(request_id, tuple(actions)))
    reader.end()
    return tuple(result)


class StorageAdapter:
    """Maps only the four reference storage screens; station roles fail closed.

    Window ID is NOT present in ItemStackRequest. A real backend must acquire
    transaction ownership and retire old wire identities before using this adapter.
    """
    SIZES = {"chest": 27, "doublechest": 54, "hopper": 5, "dispenser": 9, "shulker": 27}

    def __init__(self, kind):
        if kind not in self.SIZES:
            raise Rejected("no storage mapping for this workstation")
        self.size = self.SIZES[kind]
        self.storage_role = 30 if kind == 'shulker' else 7

    def ref(self, wire):
        if wire.dynamic_id is not None:
            raise Rejected("dynamic containers are not owned by storage")
        if wire.role == self.storage_role:
            area, low, high = "storage", 0, self.size
        elif wire.role == 59:
            area, low, high = "cursor", 0, 1
        elif wire.role == 28:
            area, low, high = "player", 0, 9
        elif wire.role == 29:
            area, low, high = "player", 9, 36
        elif wire.role == 12:
            area, low, high = "player", 0, 36
        else:
            raise Rejected("unowned or workstation slot role")
        if not low <= wire.slot < high:
            raise Rejected("slot outside role bounds; mapping needs client fixtures")
        return Ref(Slot(area, wire.slot), wire.net_id)

    def request(self, wire, state):
        return Request(state.generation, wire.request_id, state.version, tuple(
            Action(a.kind, self.ref(a.source), self.ref(a.destination) if a.destination else None, a.count)
            for a in wire.actions))


class Registry:
    """Server-origin mappings only; a changed epoch invalidates all consumers."""
    def __init__(self):
        self.epoch = None
        self.items = {}
        self.provenance = None

    def replace(self, entries, *, source, bds_hash, protocol, behavior_pack_hash):
        if source != "server-item-registry" or protocol != 2169 or len(bds_hash) != 64 or len(behavior_pack_hash) != 64:
            raise Rejected("authoritative registry provenance required")
        names, ids = set(), set()
        records = []
        for name, runtime_id, maximum in entries:
            if name in names or runtime_id in ids or not isinstance(name, str) or ":" not in name:
                raise Rejected("duplicate or invalid registry mapping")
            if type(runtime_id) is not int or not -32768 <= runtime_id <= 32767 or not 1 <= maximum <= 255:
                raise Rejected("invalid registry metadata")
            names.add(name)
            ids.add(runtime_id)
            records.append((name, runtime_id, maximum))
        if not records or len(records) > 65536:
            raise Rejected("invalid registry size")
        provenance = dict(source=source, bds_hash=bds_hash, protocol=protocol, behavior_pack_hash=behavior_pack_hash)
        epoch = hashlib.sha256(json.dumps([provenance, sorted(records)], sort_keys=True).encode()).hexdigest()
        changed = self.epoch is not None and self.epoch != epoch
        self.items = {name: (runtime_id, maximum) for name, runtime_id, maximum in records}
        self.epoch, self.provenance = epoch, provenance
        return changed
