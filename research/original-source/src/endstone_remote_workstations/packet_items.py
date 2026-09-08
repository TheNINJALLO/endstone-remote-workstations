"""Complete server ItemStack templates and the real online Ender inventory."""
import json
import struct
from dataclasses import replace

from .model import Item, Snapshot, Rejected
from .packet_inventory import Conflict, equivalent, same_inventory
from .protocol import ItemDescriptor, Reader


def metadata(stack):
    return json.dumps([stack.data, str(stack.nbt)], ensure_ascii=False,
                      separators=(',', ':')).encode('utf-8')


def clone(stack, amount=None):
    if stack is None or stack.type.id == 'minecraft:air':
        return None
    from endstone.inventory import ItemStack
    result = ItemStack(stack.type.id, stack.amount if amount is None else amount, stack.data)
    result.nbt = stack.nbt
    if metadata(result) != metadata(stack):
        raise Rejected('ItemStack NBT clone did not preserve the original metadata.')
    return result


def read_descriptor(reader):
    numeric_id, count = struct.unpack('<hH', reader.raw(4))
    aux = reader.uvar()
    net_id = reader.svar() if reader.boolean() else None
    block = reader.uvar()
    return ItemDescriptor(numeric_id, count, aux, net_id, block, reader.raw(reader.uvar()))


def read_content(payload):
    reader = Reader(payload)
    window, count = reader.uvar(), reader.uvar()
    if count > 54:
        raise Rejected('Inventory content size exceeds owned bounds.')
    return window, tuple(read_descriptor(reader) for _ in range(count))


def read_slot(payload):
    reader = Reader(payload)
    window, slot = reader.byte(), reader.uvar()
    if reader.boolean():
        reader.byte()
        if reader.boolean():
            reader.raw(4)
    if reader.boolean():
        read_descriptor(reader)
    item = read_descriptor(reader)
    reader.end()
    return window, slot, item


def registry(payload):
    # This parser is used only on an outgoing packet from the admitted BDS.
    if len(payload) > 8 * 1024 * 1024:
        raise Rejected('Server registry exceeds bounds.')
    from bstream import ReadOnlyBinaryStream
    from rapidnbt import CompoundTag
    stream = ReadOnlyBinaryStream(payload)
    count = stream.get_unsigned_varint()
    if not 1 <= count <= 32768:
        raise Rejected('Invalid server registry size.')
    result = {}
    for _ in range(count):
        name, number = stream.get_string(), stream.get_signed_short()
        stream.get_bool()
        stream.get_varint()
        CompoundTag().deserialize(stream)
        if name in result or len(name) > 256:
            raise Rejected('Invalid server registry entry.')
        result[name] = number
    return result


class ItemBinding:
    def __init__(self, player, generation, next_id, numbers, icons=None):
        self.player, self.numbers, self.icons = player, numbers, icons
        self.generation, self.next_id = generation, next_id
        self.templates, self.descriptors = {}, {}
        self.initial = self.read()

    def _item(self, stack):
        if stack is None or stack.type.id == 'minecraft:air':
            return None
        data = metadata(stack)
        key = stack.type.id, data
        if key not in self.templates:
            if sum(len(k[1]) for k in self.templates) + len(data) > 1024 * 1024:
                raise Rejected('Inventory metadata exceeds the session limit.')
            self.templates[key] = clone(stack)
        item = Item(stack.type.id, stack.amount, stack.max_stack_size, self.next_id, data)
        self.next_id += 1
        return item

    def read(self):
        return Snapshot(self.generation, 0,
                        tuple(self._item(self.player.inventory.get_item(i)) for i in range(36)),
                        tuple(self._item(i) for i in self.icons) if self.icons is not None else
                        tuple(self._item(self.player.ender_chest.get_item(i)) for i in range(27)),
                        next_net_id=self.next_id)

    def stack(self, item):
        return clone(self.templates[item.type_id, item.metadata], item.count) if item else None

    def descriptor(self, item):
        if item is None:
            return ItemDescriptor()
        key = item.type_id, item.metadata
        if key not in self.descriptors:
            from rapidnbt import CompoundTag
            aux, snbt = json.loads(item.metadata)
            tag = CompoundTag.from_snbt(snbt)
            if tag is None:
                raise Rejected('Cannot serialize the complete item NBT.')
            # Adventure-mode predicates and shield-specific wire tails need their
            # own captured fixtures. Do not silently omit them from a descriptor.
            if item.type_id == 'minecraft:shield' or 'CanPlaceOn' in tag or 'CanDestroy' in tag:
                raise Rejected('This item needs an additional descriptor fixture.')
            body = b'\x00\x00' if tag.empty() else b'\xff\xff\x01' + tag.to_binary_nbt()
            number = self.numbers.get(item.type_id)
            if number is None:
                raise Rejected('Item is absent from the server registry.')
            self.descriptors[key] = ItemDescriptor(number, item.count, aux, item.net_id, 0, body + bytes(8))
        return replace(self.descriptors[key], count=item.count, net_id=item.net_id)

    def name(self, item):
        if item is None:
            return ''
        meta = self.templates[item.type_id, item.metadata].item_meta
        return meta.display_name if meta and meta.has_display_name else ''

    def commit(self, before, after):
        if self.icons is not None:
            raise Rejected('Developer menu icons are not transferable items.')
        if not same_inventory(before, self.read()):
            raise Conflict('The real inventory changed before commit.')
        changes = []
        for area in ('player', 'storage'):
            inventory = self.player.inventory if area == 'player' else self.player.ender_chest
            for index, (old, new) in enumerate(zip(getattr(before, area), getattr(after, area))):
                if not equivalent(old, new):
                    changes.append((inventory, index, self.stack(old), self.stack(new)))
        written = []
        try:
            for inventory, index, old, new in changes:
                # Include an attempted write: a setter can mutate and then throw.
                written.append((inventory, index, old, new))
                inventory.set_item(index, new)
            if not same_inventory(after, self.read()):
                raise Conflict('Inventory verification failed after commit.')
        except Exception:
            for inventory, index, old, new in reversed(written):
                current = inventory.get_item(index)
                def signature(stack):
                    return None if stack is None or stack.type.id == 'minecraft:air' else (
                        stack.type.id, stack.amount, metadata(stack))
                if signature(current) == signature(new):
                    inventory.set_item(index, old)
            raise Conflict('Inventory commit failed; the interface was closed.')
