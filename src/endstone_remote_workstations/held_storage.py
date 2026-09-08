"""Actual held shulker contents with a native inventory generation and save guard."""
from dataclasses import replace
import uuid

from . import item_nbt
from .held_items import HeldSelection, HeldStack, SHULKERS
from .model import Item, Slot, Snapshot, Rejected
from .packet_inventory import Conflict, same_inventory
from .packet_items import ItemBinding, metadata


def contents(stack, bridge, factory=None):
    """Decode only complete, round-trippable stored stacks; never drop fields."""
    from endstone.nbt import CompoundTag, ListTag, ByteTag, ShortTag, StringTag
    if factory is None:
        from endstone.inventory import ItemStack
        factory = ItemStack
    tag = stack.nbt
    rows = tag['Items'] if 'Items' in tag else ListTag()
    if type(rows) is not ListTag or len(rows) > 27:
        raise Rejected('Malformed shulker contents.')
    result = [None] * 27
    for row in rows:
        if (type(row) is not CompoundTag or 'Slot' not in row or type(row['Slot']) is not ByteTag
                or not 0 <= row['Slot'].value < 27 or result[row['Slot'].value] is not None):
            raise Rejected('Duplicate or invalid shulker slot.')
        slot = row['Slot'].value
        for key, expected in (('Name', StringTag), ('Count', ByteTag), ('Damage', ShortTag)):
            if key not in row or type(row[key]) is not expected:
                raise Rejected('Malformed stored item fields.')
        source = CompoundTag({key: value for key, value in row.items() if key != 'Slot'})
        item_nbt.encode(source)  # Bound native deserialization before calling it.
        item = bridge.held_unpack(source)
        if not 1 <= item.amount <= item.max_stack_size or item.type.id in SHULKERS:
            raise Rejected('Overstacked or nested shulker contents are not permitted.')
        packed = bridge.held_pack(item)
        # Old vanilla saves can omit this default field; retain all other fields.
        if 'WasPickedUp' not in source:
            source['WasPickedUp'] = ByteTag(0)
        if item_nbt.encode(packed) != item_nbt.encode(source):
            raise Rejected('The native codec cannot preserve this stored item completely.')
        result[slot] = item
    return tuple(result)


def with_contents(stack, items, bridge):
    from endstone.nbt import ListTag, ByteTag
    if len(items) != 27:
        raise Rejected('A shulker requires 27 slots.')
    result = bridge.held_clone(stack)
    tag, rows = result.nbt, []
    for slot, item in enumerate(items):
        if item is None:
            continue
        if item.type.id in SHULKERS or not 1 <= item.amount <= item.max_stack_size:
            raise Rejected('Nested or overstacked shulker contents are not permitted.')
        row = bridge.held_pack(item)
        row['Slot'] = ByteTag(slot)
        rows.append(row)
    tag['Items'] = ListTag(rows)
    result.nbt = tag
    # Catch setter normalization before anything enters the real inventory.
    if item_nbt.encode(result.nbt) != item_nbt.encode(tag):
        raise Rejected('Native shulker metadata reconstruction changed its contents.')
    return result


class HeldBinding(ItemBinding):
    kind = 'shulker'
    block_type = 'minecraft:undyed_shulker_box'
    actor_type = 'ShulkerBox'
    container_type = 0

    def __init__(self, player, generation, next_id, numbers, bridge):
        self.bridge, self.token, self.epoch = bridge, None, 0
        self.source = HeldSelection.capture(player)
        if self.source.stack.kind != 'shulker':
            raise Rejected('Hold one shulker box in your selected hotbar slot.')
        self.item_id = self.source.stack.durable_id or str(uuid.uuid4())
        self.player, self.generation, self.next_id = player, generation, next_id
        self.numbers, self.icons, self.templates, self.descriptors = numbers, None, {}, {}
        self.backing = None
        self.initial = self.read()
        self.backing = self.initial.player[self.source.slot]
        self.locked = frozenset((Slot('player', self.source.slot),))

    @staticmethod
    def accept(slot, item):
        return slot.area != 'storage' or item.type_id not in SHULKERS

    def activate(self):
        self.source.verify(self.player)
        self.token = self.bridge.held_watch(self.player, self.item_id)
        try:
            if self.source.stack.durable_id is None:
                before = [self.player.inventory.get_item(i) for i in range(36)]
                after = list(before)
                replacement = self.bridge.held_clone(before[self.source.slot])
                replacement.nbt = item_nbt.decode(self.source.stack.with_identity(self.item_id).nbt)
                after[self.source.slot] = replacement
                self.epoch = self.bridge.held_apply(self.player, self.token, 0, before, after)
                self.source = HeldSelection.capture(self.player)
                if self.source.stack.durable_id != self.item_id:
                    raise Conflict('The native held identity assignment failed.')
        except Exception:
            self.close()
            raise

    def close(self):
        if self.token is not None:
            self.bridge.held_release(self.token)
            self.token = None

    def _item(self, stack):
        if stack is None or stack.type.id == 'minecraft:air':
            return None
        packed = self.bridge.held_pack(stack)
        from endstone.nbt import ByteTag
        packed['Count'] = ByteTag(1)
        data = item_nbt.encode(packed)
        key = stack.type.id, data
        if key not in self.templates:
            if sum(len(k[1]) for k in self.templates) + len(data) > 1024 * 1024:
                raise Rejected('Inventory metadata exceeds the session limit.')
            self.templates[key] = self.bridge.held_clone(stack)
        item = Item(stack.type.id, stack.amount, stack.max_stack_size, self.next_id, data)
        self.next_id += 1
        return item

    def stack(self, item):
        return self.bridge.held_clone(self.templates[item.type_id, item.metadata], item.count) if item else None

    def descriptor(self, item):
        if item is None:
            return super().descriptor(None)
        # Reuse the admitted wire encoder, but retain full native metadata in the
        # authoritative model. Unsupported wire tails remain a preflight refusal.
        from .held_nbt_wire import encode
        from .protocol import ItemDescriptor
        key = item.type_id, item.metadata
        if key not in self.descriptors:
            template = self.templates[key]
            packed = item_nbt.decode(item.metadata)
            if item.type_id == 'minecraft:shield' or 'CanPlaceOn' in packed or 'CanDestroy' in packed:
                raise Rejected('This item needs an additional descriptor fixture.')
            tag = template.nbt
            body = b'\0\0' if not len(tag) else b'\xff\xff\x01'+encode(tag)
            number = self.numbers.get(item.type_id)
            if number is None:
                raise Rejected('Item is absent from the server registry.')
            self.descriptors[key] = ItemDescriptor(number,item.count,template.data,item.net_id,0,body+bytes(8))
        return replace(self.descriptors[key],count=item.count,net_id=item.net_id)

    def read(self):
        self.source.verify(self.player)
        if self.token is not None and self.bridge.held_epoch(self.player, self.token) != self.epoch:
            raise Conflict('The native inventory changed outside this held interface.')
        real = [self.player.inventory.get_item(i) for i in range(36)]
        stored = contents(real[self.source.slot], self.bridge)
        items = tuple(self.backing if i == self.source.slot and self.backing is not None else self._item(stack)
                      for i, stack in enumerate(real))
        return Snapshot(self.generation, 0, items, tuple(self._item(stack) for stack in stored),
                        next_net_id=self.next_id)

    def commit(self, before, after):
        if self.token is None or not same_inventory(before, self.read()):
            raise Conflict('Held inventory ownership changed before commit.')
        if after.cursor[0] is not None or before.player[self.source.slot] != after.player[self.source.slot]:
            raise Rejected('The backing shulker is locked for this interface.')
        real_before = [self.player.inventory.get_item(i) for i in range(36)]
        real_after = [self.stack(item) for item in after.player]
        real_after[self.source.slot] = with_contents(real_before[self.source.slot],
            tuple(self.stack(item) for item in after.storage), self.bridge)
        try:
            self.epoch = self.bridge.held_apply(self.player, self.token, self.epoch, real_before, real_after)
            self.source = HeldSelection.capture(self.player)
            if not same_inventory(after, self.read()):
                raise Conflict('Held write readback changed the authoritative inventory.')
        except Exception as error:
            raise Conflict(str(error)) from error
