"""BDS 1.26.45 wire adapter for the reviewed InventoryUI 2.0.6 object contract.

Explicit data conversion only: importing this module registers no listeners,
patches no packages and enables no inventory transactions. See README.md here.
InventoryUI's MIT-licensed object contract was reviewed at commit
366c4de0c39852962530ac1e7a2545fd7a18dac8. Wire rules are checked against our actual
Windows 1.26.45 captures, not inferred from a shared protocol version number.
"""
from dataclasses import dataclass, field
import struct
from types import SimpleNamespace

from endstone_remote_workstations.protocol import (
    CodecError, Reader, decode_storage_requests, full_container, svar, uvar,
)


def decode_request(payload):
    """Return the request_data/request_actions shape consumed by InventoryUI.

    Preserves its existing 'distination' spelling. The strict core decoder rejects
    unsupported actions, incomplete packets and surplus bytes before a helper can
    apply anything. These are decoded requests, not authorized transactions.
    """
    def ref(value):
        return SimpleNamespace(container=SimpleNamespace(container_enum=value.role,
                               dynamic_slot=value.dynamic_id), slot=value.slot,
                               net_id=value.net_id, net_id_variant=0)

    requests = []
    for request in decode_storage_requests(payload):
        actions = []
        for action in request.actions:
            actions.append(SimpleNamespace(
                action_type=('take', 'place', 'swap', 'drop').index(action.kind),
                action_data=SimpleNamespace(amount=action.count, source=ref(action.source),
                                           distination=ref(action.destination) if action.destination else None,
                                           randomly=False)))
        requests.append(SimpleNamespace(client_request_id=request.request_id,
                        request_actions=actions, strings_to_filter=[], strings_to_filter_origin=-1))
    return SimpleNamespace(request_data=requests)


@dataclass
class Container:
    container_enum: int
    dynamic_slot: int | None = None


@dataclass
class ResponseSlot:
    slot: int = 0
    hotbar_slot: int = 0
    count: int = 0
    item_stack_id: int | None = None
    custom_name: str = ''
    filtered_custom_name: str | None = None
    durability_correction: int = 0


@dataclass
class ResponseContainer:
    container: Container
    slots: list[ResponseSlot] = field(default_factory=list)


@dataclass
class Response:
    result: int = 0
    request_id: int = -1
    container_infos: list[ResponseContainer] | None = None


def _request_id(value):
    if type(value) is not int or not -(2**31) < value < 0 or value % 2 != 1:
        raise CodecError('Expected a negative odd client request ID.')


def _string(value):
    if not isinstance(value, str):
        raise CodecError('Expected a response string.')
    raw = value.encode('utf-8')
    if len(raw) > 4096:
        raise CodecError('Response string exceeds bounds.')
    return uvar(len(raw)) + raw


def _bounded_byte(value):
    if type(value) is not int or not 0 <= value <= 255:
        raise CodecError('Invalid response byte.')
    return bytes((value,))


def encode_responses(responses):
    """Encode InventoryUI-shaped server results in the captured 1.26.45 layout.

    This repairs the nested optional fields missing from that helper release.
    It does not decide success or grant client-supplied item IDs authority.
    """
    if not isinstance(responses, (tuple, list)) or not 1 <= len(responses) <= 100:
        raise CodecError('Invalid response count.')
    data = bytearray(uvar(len(responses)))
    for response in responses:
        _request_id(response.request_id)
        data += _bounded_byte(response.result) + svar(response.request_id) + b'\x01'
        # BDS sends a fixed outer true, followed by the containers optional.
        containers = response.container_infos if response.result == 0 else None
        data += bytes((containers is not None,))
        if containers is None:
            continue
        if len(containers) > 100:
            raise CodecError('Too many response containers.')
        data += uvar(len(containers))
        for group in containers:
            data += full_container(int(group.container.container_enum), group.container.dynamic_slot)
            if len(group.slots) > 100:
                raise CodecError('Too many response slots.')
            data += uvar(len(group.slots))
            for slot in group.slots:
                data += _bounded_byte(slot.slot) + _bounded_byte(slot.hotbar_slot) + _bounded_byte(slot.count)
                present = slot.count > 0 and slot.item_stack_id is not None
                if present and (type(slot.item_stack_id) is not int or not 0 < slot.item_stack_id < 2**31):
                    raise CodecError('Invalid authoritative stack ID.')
                if slot.count > 0 and not present:
                    raise CodecError('A nonempty slot requires an authoritative stack ID.')
                data += b'\x01' + bytes((present,))
                if present:
                    data += svar(slot.item_stack_id)
                data += _string(slot.custom_name)
                filtered = slot.filtered_custom_name
                # InventoryUI uses an empty string for "no redacted name".
                has_filtered = bool(filtered) and filtered != slot.custom_name
                data += bytes((has_filtered,))
                if has_filtered:
                    data += _string(filtered)
                data += svar(slot.durability_correction)
                if len(data) > 65536:
                    raise CodecError('Response payload exceeds bounds.')
        if len(data) > 65536:
            raise CodecError('Response payload exceeds bounds.')
    return bytes(data)


def decode_responses(payload):
    """Bounded response reader for capture comparison and helper migration tests."""
    reader = Reader(payload)

    def count():
        n = reader.uvar()
        if n > 100:
            raise CodecError('Response collection exceeds bounds.')
        return n

    def string():
        n = reader.uvar()
        if n > 4096:
            raise CodecError('Response string exceeds bounds.')
        try:
            return reader.raw(n).decode('utf-8')
        except UnicodeDecodeError as error:
            raise CodecError('Invalid response UTF-8.') from error

    responses = []
    n = count()
    if not n:
        raise CodecError('Empty response batch.')
    for _ in range(n):
        result, request_id = reader.byte(), reader.svar()
        _request_id(request_id)
        if not reader.boolean():
            raise CodecError('Missing outer response optional marker.')
        groups = None
        if reader.boolean():
            groups = []
            for _ in range(count()):
                role = reader.byte()
                dynamic = struct.unpack('<I', reader.raw(4))[0] if reader.boolean() else None
                slots = []
                for _ in range(count()):
                    slot, hotbar, amount = reader.byte(), reader.byte(), reader.byte()
                    if not reader.boolean():
                        raise CodecError('Missing outer stack-ID optional marker.')
                    net_id = reader.svar() if reader.boolean() else None
                    name = string()
                    filtered = string() if reader.boolean() else None
                    slots.append(ResponseSlot(slot, hotbar, amount, net_id, name, filtered, reader.svar()))
                groups.append(ResponseContainer(Container(role, dynamic), slots))
        responses.append(Response(result, request_id, groups))
    reader.end()
    return responses


class RequestPacket:
    """Explicit replacement object for InventoryUI's receive-side codec."""
    def __init__(self):
        self.request = SimpleNamespace(request_data=[])

    def deserialize(self, payload):
        self.request = decode_request(bytes(payload))


class ResponsePacket:
    """Explicit replacement object for InventoryUI's send-side codec."""
    def __init__(self, responses):
        self.responses = responses

    def get_packet_id(self):
        return 148

    def serialize(self):
        return encode_responses(self.responses)
