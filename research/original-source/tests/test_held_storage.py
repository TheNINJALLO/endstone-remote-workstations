from dataclasses import replace
from types import SimpleNamespace
import uuid

import pytest
from endstone import nbt

from endstone_remote_workstations import item_nbt
from endstone_remote_workstations.held_items import IDENTITY_TAG
from endstone_remote_workstations.held_storage import HeldBinding, contents, with_contents
from endstone_remote_workstations.model import Request, Action, Ref, Slot, Rejected
from endstone_remote_workstations.packet_inventory import ReservedInventory, Conflict


class Stack:
    def __init__(self, name, amount=1, data=0, tag=None):
        self.type=SimpleNamespace(id=name)
        self.amount,self.data=amount,data
        self.max_stack_size=1 if name.endswith(('shulker_box','sword')) else 64
        self.nbt=tag if tag is not None else nbt.CompoundTag()
        self.extra={}


class Bridge:
    def __init__(self):
        self.epoch=0
        self.active=False
        self.writes=0

    def held_pack(self, item):
        value=nbt.CompoundTag({'Name':nbt.StringTag(item.type.id),'Count':nbt.ByteTag(item.amount),
            'Damage':nbt.ShortTag(item.data),'WasPickedUp':nbt.ByteTag(0),**item.extra})
        if len(item.nbt):
            value['tag']=item.nbt
        return value

    def held_unpack(self, value):
        item=Stack(value['Name'].value,value['Count'].value,value['Damage'].value,
                   value['tag'] if 'tag' in value else None)
        item.extra={k:v for k,v in value.items() if k not in ('Name','Count','Damage','WasPickedUp','tag')}
        return item

    def held_clone(self, item, amount=-1):
        result=self.held_unpack(item_nbt.decode(item_nbt.encode(self.held_pack(item))))
        if amount!=-1:
            result.amount=amount
        return result

    def held_watch(self, player, item_id):
        assert not self.active
        self.active=True
        return 1

    def held_epoch(self, player, token):
        assert self.active and token==1
        return self.epoch

    def held_release(self, token):
        assert token==1
        self.active=False

    def held_apply(self, player, token, epoch, before, after):
        assert self.active and token==1 and epoch==self.epoch
        assert all(a is b for a,b in zip(before,player.inventory.items))
        self.writes+=1
        player.inventory.items=list(after)
        self.epoch+=1
        return self.epoch


def setup():
    bridge=Bridge()
    sword=Stack('minecraft:diamond_sword',tag=nbt.CompoundTag({'Damage':nbt.IntTag(17),
        'display':nbt.CompoundTag({'Name':nbt.StringTag('Named sword')}),
        'plugin:extra':nbt.LongTag(2**60+1)}))
    box=with_contents(Stack('minecraft:purple_shulker_box'),(sword,)+(None,)*26,bridge)
    items=[None]*36
    items[3]=box
    items[10]=Stack('minecraft:stone',12)
    inv=SimpleNamespace(items=items,held_item_slot=3)
    inv.get_item=lambda i: inv.items[i]
    class Inventory:
        @property
        def held_item_slot(self): return inv.held_item_slot
        @property
        def item_in_main_hand(self): return inv.items[inv.held_item_slot]
        @property
        def items(self): return inv.items
        @items.setter
        def items(self, value): inv.items=value
        def get_item(self,i): return inv.items[i]
    player=SimpleNamespace(inventory=Inventory(),unique_id=uuid.uuid4(),is_valid=True,is_dead=False)
    binding=HeldBinding(player,'held',1000,{},bridge)
    binding.activate()
    engine=ReservedInventory(binding.initial,binding.read,binding.commit,locked=binding.locked,accept=binding.accept)
    return bridge,player,binding,engine


def move(engine, request_id, source, destination, amount):
    a=Slot(*source); b=Slot(*destination)
    old=getattr(engine.state,a.area)[a.index]
    dest=getattr(engine.state,b.area)[b.index]
    return Request('held',request_id,engine.state.version,(Action('place',Ref(a,old.net_id if old else 0),
        Ref(b,dest.net_id if dest else 0),amount),))


def test_actual_box_tag_assignment_and_completed_transfer_use_one_native_batch():
    bridge,player,binding,engine=setup()
    identity=player.inventory.get_item(3).nbt[IDENTITY_TAG].value
    assert str(uuid.UUID(identity))==identity and bridge.writes==1
    engine.apply(move(engine,-1,('player',10),('storage',1),12))
    assert player.inventory.get_item(10) is None and bridge.writes==2
    stored=contents(player.inventory.get_item(3),bridge)
    assert stored[1].amount==12
    assert stored[0].nbt['plugin:extra'].value==2**60+1
    assert player.inventory.get_item(3).nbt[IDENTITY_TAG].value==identity
    engine.check()
    binding.close()
    assert not bridge.active


def test_unfinished_cursor_is_not_persistent_and_forced_close_discards_it():
    bridge,player,binding,engine=setup()
    before=item_nbt.encode(bridge.held_pack(player.inventory.get_item(3)))
    engine.apply(move(engine,-1,('storage',0),('cursor',0),1))
    assert engine.state.cursor[0] is not None and bridge.writes==1
    engine.close(); binding.close()
    assert item_nbt.encode(bridge.held_pack(player.inventory.get_item(3)))==before


def test_identical_inventory_replacement_invalidates_native_epoch_before_write():
    bridge,player,binding,engine=setup()
    bridge.epoch+=1
    with pytest.raises(Conflict,match='native inventory changed'):
        engine.apply(move(engine,-1,('player',10),('storage',1),12))
    assert bridge.writes==1 and player.inventory.get_item(10).amount==12


@pytest.mark.parametrize('destination',[('cursor',0),('storage',1),('player',11)])
def test_every_transfer_of_actual_backing_slot_is_refused(destination):
    bridge,player,binding,engine=setup()
    with pytest.raises(Rejected):
        engine.apply(move(engine,-1,('player',3),destination,1))
    assert bridge.writes==1 and engine.state.cursor[0] is None


def test_nested_shulkers_refused_and_other_native_fields_preserved():
    bridge=Bridge()
    box=Stack('minecraft:purple_shulker_box')
    with pytest.raises(Rejected,match='Nested'):
        with_contents(box,(Stack('minecraft:red_shulker_box'),)+(None,)*26,bridge)
    block=Stack('minecraft:chest',3)
    block.extra={'Block':nbt.CompoundTag({'name':nbt.StringTag('minecraft:chest'),
        'states':nbt.CompoundTag({'minecraft:cardinal_direction':nbt.StringTag('north')})})}
    result=contents(with_contents(box,(block,)+(None,)*26,bridge),bridge)[0]
    assert bridge.held_pack(result)==bridge.held_pack(block)


def test_duplicate_or_malformed_stored_slots_do_not_enter_native_apply():
    bridge,player,binding,engine=setup()
    box=bridge.held_clone(player.inventory.get_item(3))
    tag=box.nbt
    tag['Items']=nbt.ListTag([tag['Items'][0],tag['Items'][0]])
    box.nbt=tag
    with pytest.raises(Rejected,match='Duplicate'):
        contents(box,bridge)
    assert bridge.writes==1


def test_native_loader_normalization_cannot_silently_drop_unknown_fields():
    bridge,player,binding,engine=setup()
    original=bridge.held_unpack
    def lossy(value):
        item=original(value)
        item.nbt=nbt.CompoundTag()
        return item
    bridge.held_unpack=lossy
    with pytest.raises(Rejected,match='preserve'):
        contents(player.inventory.get_item(3),bridge)
