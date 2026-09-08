"""Actual held-item snapshots and preflight; no inventory writes or UI opening.

A content digest is NOT a durable identity. An identity tag is NOT a native
movement lock. Mutation remains disabled until both have qualified executors.
"""
import base64
from dataclasses import dataclass, replace
import hashlib
import json
import re
import uuid

from . import item_nbt
from .model import Rejected

IDENTITY_TAG = 'remote_workstations:held_id'
COLORS = ('white', 'orange', 'magenta', 'light_blue', 'yellow', 'lime', 'pink',
          'gray', 'light_gray', 'cyan', 'purple', 'blue', 'brown', 'green', 'red', 'black')
SHULKERS = frozenset('minecraft:' + name + '_shulker_box' for name in (*COLORS, 'undyed'))
BUNDLES = frozenset(['minecraft:bundle', *('minecraft:' + c + '_bundle' for c in COLORS)])


def kind_for(type_id):
    if type_id in SHULKERS:
        return 'shulker'
    if type_id in BUNDLES:
        return 'bundle'
    return {'minecraft:written_book': 'writtenbook', 'minecraft:writable_book': 'bookediting'}.get(type_id)


def identity(value):
    if not isinstance(value, str) or len(value) != 36:
        raise Rejected('Held-item identity must be a canonical UUID.')
    try:
        if str(uuid.UUID(value)) != value or uuid.UUID(value).int == 0:
            raise ValueError()
    except ValueError as error:
        raise Rejected('Held-item identity must be a canonical UUID.') from error
    return value


@dataclass(frozen=True)
class HeldStack:
    type_id: str
    amount: int
    data: int
    nbt: bytes

    def __post_init__(self):
        if (not isinstance(self.type_id, str) or not re.fullmatch(r'[a-z0-9_.-]+:[a-z0-9_./-]+', self.type_id)
                or len(self.type_id) > 256 or type(self.amount) is not int or not 1 <= self.amount <= 255
                or type(self.data) is not int or not -32768 <= self.data <= 32767):
            raise Rejected('Invalid held-item stack.')
        item_nbt.decode(self.nbt)

    @classmethod
    def capture(cls, stack):
        if stack is None or stack.type.id == 'minecraft:air':
            raise Rejected('Hold a shulker box, bundle or book in your main hand.')
        return cls(stack.type.id, stack.amount, stack.data, item_nbt.encode(stack.nbt))

    @property
    def kind(self):
        return kind_for(self.type_id)

    @property
    def durable_id(self):
        from endstone.nbt import StringTag
        tag = item_nbt.decode(self.nbt)
        if IDENTITY_TAG not in tag:
            return None
        if type(tag[IDENTITY_TAG]) is not StringTag:
            raise Rejected('The reserved held-item identity tag has the wrong type.')
        return identity(tag[IDENTITY_TAG].value)

    def with_identity(self, value):
        """Prepare a detached candidate; assigning it to BDS is not implemented."""
        from endstone.nbt import StringTag
        value = identity(value)
        if self.durable_id not in (None, value):
            raise Rejected('Cannot replace an existing held-item identity.')
        tag = item_nbt.decode(self.nbt)
        tag[IDENTITY_TAG] = StringTag(value)
        return replace(self, nbt=item_nbt.encode(tag))

    def materialize(self, factory=None):
        """Construct a detached copy and verify all public ItemStack fields."""
        if factory is None:
            from endstone.inventory import ItemStack
            factory = ItemStack
        stack = factory(self.type_id, self.amount, self.data)
        stack.nbt = item_nbt.decode(self.nbt)
        if HeldStack.capture(stack) != self:
            raise Rejected('Endstone did not preserve the complete held-item snapshot.')
        return stack

    def record(self):
        return dict(type_id=self.type_id, amount=self.amount, data=self.data,
                    nbt=base64.b64encode(self.nbt).decode('ascii'))

    @classmethod
    def from_record(cls, record):
        if not isinstance(record, dict) or set(record) != {'type_id', 'amount', 'data', 'nbt'}:
            raise Rejected('Invalid held-item journal record.')
        try:
            if not isinstance(record['nbt'], str) or len(record['nbt']) > 4 * ((item_nbt.MAX_BYTES+2)//3):
                raise ValueError()
            return cls(**{**record, 'nbt': base64.b64decode(record['nbt'], validate=True)})
        except (ValueError, TypeError) as error:
            raise Rejected('Invalid held-item journal record.') from error

    def digest(self):
        return hashlib.sha256(json.dumps(self.record(), sort_keys=True,
                             separators=(',', ':')).encode('ascii')).hexdigest()


@dataclass(frozen=True)
class HeldSelection:
    owner: str
    slot: int
    stack: HeldStack

    def __post_init__(self):
        identity(self.owner)
        if type(self.slot) is not int or not 0 <= self.slot <= 8 or not isinstance(self.stack, HeldStack):
            raise Rejected('A real selected hotbar slot is required.')
        if self.stack.kind is None:
            raise Rejected('This held item has no supported item-backed interface.')
        if self.stack.kind != 'writtenbook' and self.stack.amount != 1:
            raise Rejected('The backing item must be a single unstacked item.')
        self.stack.durable_id  # Reject malformed reserved tags even during preflight.

    @classmethod
    def capture(cls, player):
        if not player.is_valid or player.is_dead:
            raise Rejected('A living, connected player is required.')
        inventory = player.inventory
        slot = inventory.held_item_slot
        if type(slot) is not int or not 0 <= slot <= 8:
            raise Rejected('A real selected hotbar slot is required.')
        stack = HeldStack.capture(inventory.get_item(slot))
        selected = HeldStack.capture(inventory.item_in_main_hand)
        if slot != inventory.held_item_slot or stack != selected:
            raise Rejected('The selected item changed during inspection.')
        return cls(str(player.unique_id), slot, stack)

    def verify(self, player):
        if HeldSelection.capture(player) != self:
            raise Rejected('The held item, selected slot or owner changed.')

    def record(self):
        return dict(owner=self.owner, slot=self.slot, stack=self.stack.record())

    @classmethod
    def from_record(cls, record):
        if not isinstance(record, dict) or set(record) != {'owner', 'slot', 'stack'}:
            raise Rejected('Invalid held source record.')
        return cls(record['owner'], record['slot'], HeldStack.from_record(record['stack']))


def inspect_held(player):
    from .api import HeldItemInfo
    source = HeldSelection.capture(player)
    if not player.has_permission('remoteworkstations.open.' + source.stack.kind):
        raise Rejected('You do not have permission to inspect this held interface.')
    return HeldItemInfo(source.stack.kind, source.stack.type_id, source.slot, source.stack.amount,
                        source.stack.digest(), source.stack.durable_id, len(source.stack.nbt))
