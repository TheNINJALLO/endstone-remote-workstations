import struct
import pytest
from endstone import nbt
from endstone_remote_workstations.held_nbt_wire import encode
from endstone_remote_workstations.held_input import inventory_action
from endstone_remote_workstations.protocol import uvar, svar, WireRef, StorageAdapter, CodecError
from endstone_remote_workstations.model import Rejected, Slot


def test_namespaced_nbt_keys_and_precise_values_have_exact_binary_encoding():
    tag=nbt.CompoundTag({'p:id':nbt.LongTag(2**60+1)})
    assert encode(tag)==b'\x0a\0\0\x04\x04\0p:id'+struct.pack('<q',2**60+1)+b'\0'
    tag=nbt.CompoundTag({'zero':nbt.DoubleTag(-0.0)})
    assert encode(tag)==b'\x0a\0\0\x06\x04\0zero'+struct.pack('<d',-0.0)+b'\0'


def test_binary_nbt_agrees_with_independent_codec():
    from rapidnbt import CompoundTag
    tag=nbt.CompoundTag({'a':nbt.ByteTag(255),'b':nbt.ShortTag(-32768),
        'c':nbt.IntTag(123),'d':nbt.FloatTag(1.25),'e':nbt.ListTag([nbt.StringTag('a'),nbt.StringTag('b')])})
    independent=CompoundTag.from_snbt('{a:-1b,b:-32768s,c:123,d:1.25f,e:["a","b"]}')
    # Compound key ordering can differ; deserialize with the independent parser.
    parsed=CompoundTag.from_binary_nbt(encode(tag))
    assert parsed==independent


def auth(flags=(), optionals=b'\x01\0\x01\0\x01\0'):
    return bytes(32)+b'\x01'+uvar(len(flags))+b''.join(svar(i) for i in flags)+b'\x01\0\0'+bytes(8)+uvar(30)+bytes(12)+optionals


def test_ordinary_authoritative_input_does_not_close_held_screen():
    assert not inventory_action(auth())
    assert not inventory_action(auth((10,51)))


@pytest.mark.parametrize('flag',[34,35,36])
def test_held_guard_rejects_inventory_action_flags(flag):
    assert inventory_action(auth((flag,)))


@pytest.mark.parametrize('optional',[b'\x01\x01',b'\x01\0\x01\x01',b'\x01\0\x01\0\x01\x01'])
def test_hidden_optional_actions_are_rejected_even_without_flags(optional):
    assert inventory_action(auth(optionals=optional))


@pytest.mark.parametrize('data',[b'',bytes(32),auth(optionals=b'\0'),auth((10,10)),auth((66,))])
def test_malformed_input_fails_closed(data):
    with pytest.raises(CodecError):
        inventory_action(data)


def test_captured_shulker_slot_role_is_distinct_from_generic_storage():
    adapter=StorageAdapter('shulker')
    assert adapter.ref(WireRef(30,None,26,17)).slot==Slot('storage',26)
    for ref in (WireRef(7,None,0,17),WireRef(30,None,27,17),WireRef(30,1,0,17)):
        with pytest.raises(Rejected): adapter.ref(ref)
