"""Server-owned storage model. Network clients never construct Item records.

This module is a storage prototype, not a BDS inventory or workstation executor.
Opaque metadata is retained byte-for-byte; no name-only reconstruction occurs.
"""
from dataclasses import dataclass, replace
import base64
import hashlib
import json


class Rejected(ValueError):
    pass


@dataclass(frozen=True)
class Item:
    type_id: str
    count: int
    maximum: int
    net_id: int
    metadata: bytes = b""
    durable_id: str | None = None

    def __post_init__(self):
        if (not self.type_id or type(self.count) is not int or type(self.maximum) is not int
                or not 1 <= self.count <= self.maximum <= 255
                or type(self.net_id) is not int or not 1 <= self.net_id < 2**31
                or not isinstance(self.metadata, bytes) or len(self.metadata) > 1024 * 1024):
            raise Rejected("invalid authoritative item")

    def compatible(self, other):
        return (self.type_id, self.metadata, self.maximum, self.durable_id) == (
            other.type_id, other.metadata, other.maximum, other.durable_id)

    def record(self):
        return dict(type_id=self.type_id, count=self.count, maximum=self.maximum, net_id=self.net_id,
                    metadata=base64.b64encode(self.metadata).decode("ascii"), durable_id=self.durable_id)

    @classmethod
    def from_record(cls, value):
        return cls(**{**value, "metadata": base64.b64decode(value["metadata"], validate=True)})


@dataclass(frozen=True, order=True)
class Slot:
    area: str
    index: int


@dataclass(frozen=True)
class Ref:
    slot: Slot
    net_id: int


@dataclass(frozen=True)
class Action:
    kind: str
    source: Ref
    destination: Ref | None = None
    count: int = 0


@dataclass(frozen=True)
class Request:
    generation: str
    request_id: int
    version: int
    actions: tuple[Action, ...]


@dataclass(frozen=True)
class Snapshot:
    generation: str
    version: int
    player: tuple[Item | None, ...]
    storage: tuple[Item | None, ...]
    cursor: tuple[Item | None, ...] = (None,)
    xp: int = 0
    next_net_id: int = 1

    def __post_init__(self):
        if not self.generation or self.version < 0 or self.xp < 0 or len(self.player) != 36 or len(self.cursor) != 1:
            raise Rejected("invalid snapshot shape")
        if not 1 <= len(self.storage) <= 54:
            raise Rejected("invalid storage size")
        ids = [i.net_id for i in (*self.player, *self.storage, *self.cursor) if i]
        durable = [i.durable_id for i in (*self.player, *self.storage, *self.cursor) if i and i.durable_id]
        if len(ids) != len(set(ids)) or len(durable) != len(set(durable)):
            raise Rejected("duplicate server item identity")
        if not max(ids, default=0) < self.next_net_id < 2**31:
            raise Rejected("invalid identity allocator")

    def record(self):
        return dict(generation=self.generation, version=self.version, xp=self.xp, next_net_id=self.next_net_id,
                    **{a: [i.record() if i else None for i in getattr(self, a)]
                       for a in ("player", "storage", "cursor")})

    def serialize(self):
        return json.dumps(self.record(), sort_keys=True, separators=(",", ":"))

    def digest(self):
        return hashlib.sha256(self.serialize().encode()).hexdigest()

    @classmethod
    def deserialize(cls, raw):
        value = json.loads(raw)
        for area in ("player", "storage", "cursor"):
            value[area] = tuple(Item.from_record(i) if i else None for i in value[area])
        return cls(**value)


@dataclass(frozen=True)
class Plan:
    before: Snapshot
    after: Snapshot
    drops: tuple[Item, ...]
    touched: tuple[Slot, ...]


def plan_storage(state: Snapshot, request: Request, *, locked: frozenset[Slot] = frozenset(),
                 accept=lambda slot, item: True) -> Plan:
    """Validate every action against a shadow copy; commit is a separate operation.

    Ref IDs identify the pre-request snapshot, including for later batched actions.
    Negative request-reference IDs require a verified wire resolver and are rejected here.
    """
    if request.generation != state.generation or request.version != state.version:
        raise Rejected("stale session or inventory version")
    if type(request.request_id) is not int or not -(2**31) < request.request_id < 0 or request.request_id % 2 != 1:
        raise Rejected("request id must be a negative odd int32")
    if not 1 <= len(request.actions) <= 100:
        raise Rejected("invalid action count")
    shadow = {a: list(getattr(state, a)) for a in ("player", "storage", "cursor")}
    touched = set()
    drops = []
    next_id = state.next_net_id

    def resolve(ref):
        if not isinstance(ref, Ref) or ref.slot.area not in shadow:
            raise Rejected("unknown slot role")
        slot = ref.slot
        if type(slot.index) is not int or not 0 <= slot.index < len(shadow[slot.area]):
            raise Rejected("slot out of bounds")
        if slot in locked:
            raise Rejected("backing item or slot is locked")
        original = getattr(state, slot.area)[slot.index]
        if type(ref.net_id) is not int or ref.net_id != (original.net_id if original else 0):
            raise Rejected("stack identity mismatch")
        touched.add(slot)
        return shadow[slot.area][slot.index]

    def assign(ref, item):
        if item and not accept(ref.slot, item):
            raise Rejected("slot filter rejected item")
        shadow[ref.slot.area][ref.slot.index] = item

    for action in request.actions:
        if action.kind not in ("take", "place", "swap", "drop"):
            raise Rejected("unsupported action; storage cannot create or craft items")
        source = resolve(action.source)
        if action.kind == "swap":
            if action.destination is None:
                raise Rejected("missing swap destination")
            destination = resolve(action.destination)
            if action.source.slot == action.destination.slot:
                raise Rejected("self swap")
            assign(action.source, destination)
            assign(action.destination, source)
            continue
        count = action.count
        if type(count) is not int or not 1 <= count <= 64 or source is None or count > source.count:
            raise Rejected("invalid transfer count")
        if source.durable_id and count != source.count:
            raise Rejected("cannot split a durable backing identity")
        removed = replace(source, count=count)
        if count < source.count:
            if next_id >= 2**31 - 1:
                raise Rejected("network identity exhausted")
            removed = replace(removed, net_id=next_id)
            next_id += 1
        assign(action.source, replace(source, count=source.count-count) if count < source.count else None)
        if action.kind == "drop":
            if action.destination is not None:
                raise Rejected("drop has a destination")
            drops.append(removed)
            continue
        if action.destination is None or action.destination.slot == action.source.slot:
            raise Rejected("invalid transfer destination")
        destination = resolve(action.destination)
        if destination:
            if not destination.compatible(removed) or destination.count + count > destination.maximum:
                raise Rejected("incompatible or overflowing stack")
            assign(action.destination, replace(destination, count=destination.count + count))
        else:
            assign(action.destination, removed)
    after = replace(state, version=state.version+1, next_net_id=next_id,
                    **{a: tuple(v) for a, v in shadow.items()})
    return Plan(state, after, tuple(drops), tuple(sorted(touched)))


def return_cursor(state):
    """Model-only cleanup; returns durable overflow instead of deleting it."""
    item = state.cursor[0]
    if item is None:
        return state, ()
    player = list(state.player)
    remaining = item.count
    for index, other in enumerate(player):
        if other and other.compatible(item) and other.count < other.maximum:
            moved = min(remaining, other.maximum-other.count)
            player[index] = replace(other, count=other.count+moved)
            remaining -= moved
            if not remaining:
                break
    if remaining:
        for index, other in enumerate(player):
            if other is None:
                player[index] = replace(item, count=remaining)
                remaining = 0
                break
    overflow = (replace(item, count=remaining),) if remaining else ()
    return replace(state, player=tuple(player), cursor=(None,), version=state.version+1), overflow

