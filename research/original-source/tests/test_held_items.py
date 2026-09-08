from dataclasses import FrozenInstanceError, replace
import json
from types import SimpleNamespace
import uuid

import pytest
from endstone import nbt

from endstone_remote_workstations import item_nbt
from endstone_remote_workstations.held_items import (
    HeldSelection, HeldStack, IDENTITY_TAG, SHULKERS, inspect_held, kind_for,
)
from endstone_remote_workstations.model import Rejected


class Stack:
    def __init__(self, type_id='minecraft:white_shulker_box', amount=1, data=0):
        self.type = SimpleNamespace(id=type_id)
        self.amount, self.data = amount, data
        self.nbt = nbt.CompoundTag()


def rich_nbt():
    return nbt.CompoundTag({
        'display': nbt.CompoundTag({'Name': nbt.StringTag('Named box \u2603'),
                                  'Lore': nbt.ListTag([nbt.StringTag('Private text')])}),
        'Items': nbt.ListTag([nbt.CompoundTag({
            'Slot': nbt.ByteTag(2), 'Name': nbt.StringTag('minecraft:diamond_sword'),
            'Count': nbt.ByteTag(1), 'Damage': nbt.ShortTag(7),
            'tag': nbt.CompoundTag({'ench': nbt.ListTag([
                nbt.CompoundTag({'id': nbt.ShortTag(9), 'lvl': nbt.ShortTag(3)})])})})]),
        'custom': nbt.CompoundTag({
            'byte': nbt.ByteTag(255), 'short': nbt.ShortTag(-32768),
            'integer': nbt.IntTag(2**31-1), 'long': nbt.LongTag(2**63-1),
            'float': nbt.FloatTag(1.0000001192092896), 'double': nbt.DoubleTag(1.0000000000000002),
            'negative_zero': nbt.DoubleTag(-0.0), 'bytes': nbt.ByteArrayTag([0, 127, 128, 255]),
            'ints': nbt.IntArrayTag([-2**31, 0, 2**31-1]), 'empty': nbt.ListTag(),
            'escaped': nbt.StringTag('quotes" backslash\\ newline\n nul\x00')})})


def selected(stack=None):
    stack = stack if stack is not None else Stack()
    inventory = SimpleNamespace(held_item_slot=3, item_in_main_hand=stack,
                                get_item=lambda slot: stack if slot == 3 else None)
    return SimpleNamespace(unique_id=uuid.uuid4(), is_valid=True, is_dead=False,
                           inventory=inventory, has_permission=lambda p: True)


def test_typed_nbt_preserves_all_exposed_types_custom_fields_and_precision():
    original = rich_nbt()
    raw = item_nbt.encode(original)
    restored = item_nbt.decode(raw)
    assert restored == original
    assert type(restored['custom']['byte']) is nbt.ByteTag
    assert type(restored['custom']['long']) is nbt.LongTag
    assert restored['custom']['double'].value.hex() == '0x1.0000000000001p+0'
    assert restored['custom']['negative_zero'].value.hex() == '-0x0.0p+0'
    assert item_nbt.encode(restored) == raw
    reordered = nbt.CompoundTag(dict(reversed(list(original.items()))))
    assert item_nbt.encode(reordered) == raw


@pytest.mark.parametrize('raw', [
    b'["ByteTag",1]', b'["CompoundTag",{"x":["ByteTag",true]}]',
    b'["CompoundTag",{"x":["ByteTag",256]}]',
    b'["CompoundTag",{"x":["LongArrayTag",[]]}]',
    b'["CompoundTag",{"x":["DoubleTag","nan"]}]',
    b'["CompoundTag",{"x":["ByteTag",1],"x":["ByteTag",2]}]',
    b'["CompoundTag",{"x":["ListTag",[["ByteTag",1],["IntTag",2]]]}]',
    b'["CompoundTag",{"x":["FloatTag","0x1.0000000000001p+0"]}]',
    b'["CompoundTag", {}]', b'{}', b'[]', b'null', b'\xff',
])
def test_invalid_lossy_or_ambiguous_journal_nbt_is_rejected(raw):
    with pytest.raises(Rejected):
        item_nbt.decode(raw)


def test_nbt_depth_size_and_element_budgets_fail_before_inventory_mutation():
    for tag in (nbt.CompoundTag({'text': nbt.StringTag('x' * (item_nbt.MAX_BYTES+1))}),
                nbt.CompoundTag({'data': nbt.IntArrayTag([0]*item_nbt.MAX_NODES)}),
                nbt.CompoundTag({'float': nbt.FloatTag(float('inf'))})):
        with pytest.raises(Rejected):
            item_nbt.encode(tag)
    deep = nbt.CompoundTag()
    for _ in range(item_nbt.MAX_DEPTH+1):
        deep = nbt.CompoundTag({'nested': deep})
    with pytest.raises(Rejected):
        item_nbt.encode(deep)
    with pytest.raises(Rejected):
        item_nbt.decode(b'[' * 2000)


def test_capture_identity_and_materialization_preserve_original_without_mutation():
    stack = Stack()
    stack.nbt = rich_nbt()
    before = HeldStack.capture(stack)
    value = str(uuid.uuid4())
    tagged = before.with_identity(value)
    assert before.durable_id is None and IDENTITY_TAG not in stack.nbt
    assert tagged.durable_id == value and tagged.with_identity(value) == tagged
    assert HeldStack.from_record(tagged.record()) == tagged
    copy = tagged.materialize(Stack)
    assert HeldStack.capture(copy) == tagged
    copy.nbt['custom'] = nbt.CompoundTag()
    assert HeldStack.capture(stack) == before
    assert tagged.digest() != before.digest()
    with pytest.raises(Rejected, match='replace'):
        tagged.with_identity(str(uuid.uuid4()))


def test_materialization_refuses_a_lossy_item_factory():
    class Lossy(Stack):
        @property
        def nbt(self):
            return nbt.CompoundTag()

        @nbt.setter
        def nbt(self, value):
            pass
    stack = Stack()
    stack.nbt = rich_nbt()
    with pytest.raises(Rejected, match='preserve'):
        HeldStack.capture(stack).materialize(Lossy)


@pytest.mark.parametrize('type_id,kind', [*((x, 'shulker') for x in sorted(SHULKERS)),
    ('minecraft:bundle', 'bundle'), ('minecraft:blue_bundle', 'bundle'),
    ('minecraft:written_book', 'writtenbook'), ('minecraft:writable_book', 'bookediting'),
    ('custom:white_shulker_box', None), ('minecraft:fake_shulker_box', None), ('minecraft:stone', None)])
def test_item_classification_uses_exact_builtin_identifiers(type_id, kind):
    assert kind_for(type_id) == kind


@pytest.mark.parametrize('change', ['slot', 'type', 'amount', 'nbt', 'owner', 'death', 'disconnect'])
def test_selected_source_rejects_changes_even_when_hotbar_slot_is_reused(change):
    player = selected()
    source = HeldSelection.capture(player)
    if change == 'slot':
        player.inventory.held_item_slot = 4
    elif change == 'type':
        player.inventory.item_in_main_hand.type.id = 'minecraft:blue_shulker_box'
    elif change == 'amount':
        player.inventory.item_in_main_hand.amount = 2
    elif change == 'nbt':
        player.inventory.item_in_main_hand.nbt = rich_nbt()
    elif change == 'owner':
        player.unique_id = uuid.uuid4()
    else:
        setattr(player, 'is_dead' if change == 'death' else 'is_valid', change == 'death')
    with pytest.raises(Rejected):
        source.verify(player)


def test_selection_detects_change_between_inventory_reads():
    player = selected()
    player.inventory.item_in_main_hand = Stack('minecraft:blue_shulker_box')
    with pytest.raises(Rejected, match='changed during'):
        HeldSelection.capture(player)


@pytest.mark.parametrize('value', [nbt.IntTag(3), nbt.StringTag('not-a-uuid'),
                                  nbt.StringTag(str(uuid.UUID(int=0)))])
def test_reserved_identity_tag_cannot_be_forged_with_an_invalid_record(value):
    player = selected()
    player.inventory.item_in_main_hand.nbt[IDENTITY_TAG] = value
    with pytest.raises(Rejected):
        inspect_held(player)


def test_inspection_is_immutable_sanitized_and_does_not_claim_an_open_backend():
    player = selected()
    player.inventory.item_in_main_hand.nbt = rich_nbt()
    before = HeldSelection.capture(player)
    info = inspect_held(player)
    assert info.kind == 'shulker' and info.slot == 3 and info.durable_id is None
    assert info.metadata_bytes > 0 and len(info.digest) == 64 and not info.native_open_available
    assert 'Private text' not in repr(info) and 'diamond_sword' not in repr(info)
    assert HeldSelection.capture(player) == before
    with pytest.raises(FrozenInstanceError):
        info.slot = 7
    player.has_permission = lambda p: False
    with pytest.raises(Rejected, match='permission'):
        inspect_held(player)
