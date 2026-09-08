"""Typed little-endian item NBT, including namespaced compound keys."""
import struct
from . import item_nbt
from .model import Rejected

IDS = {'ByteTag':1,'ShortTag':2,'IntTag':3,'LongTag':4,'FloatTag':5,
       'DoubleTag':6,'ByteArrayTag':7,'StringTag':8,'ListTag':9,'CompoundTag':10,'IntArrayTag':11}
FORMATS = {'ByteTag':'B','ShortTag':'h','IntTag':'i','LongTag':'q','FloatTag':'f','DoubleTag':'d'}


def text(value):
    raw=value.encode('utf-8')
    if len(raw)>65535:
        raise Rejected('NBT string exceeds the item wire limit.')
    return struct.pack('<H',len(raw))+raw


def _body(tag):
    name=type(tag).__name__
    if name in FORMATS:
        return struct.pack('<'+FORMATS[name],tag.value)
    if name=='StringTag':
        return text(tag.value)
    if name=='ByteArrayTag':
        return struct.pack('<i',len(tag))+bytes(tag)
    if name=='IntArrayTag':
        return struct.pack('<i',len(tag))+b''.join(struct.pack('<i',value) for value in tag)
    if name=='ListTag':
        values=list(tag)
        kind=IDS[type(values[0]).__name__] if values else 0
        if any(IDS[type(v).__name__]!=kind for v in values):
            raise Rejected('An item NBT list must have one element type.')
        return bytes((kind,))+struct.pack('<i',len(values))+b''.join(_body(v) for v in values)
    if name=='CompoundTag':
        return b''.join(bytes((IDS[type(value).__name__],))+text(key)+_body(value)
                        for key,value in tag.items())+b'\0'
    raise Rejected('Unsupported item NBT tag.')


def encode(tag):
    item_nbt.encode(tag)
    return b'\x0a\0\0'+_body(tag)
