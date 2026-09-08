"""Bounded, typed snapshots of the NBT exposed by Endstone API 0.11.

This is a private journal format, not Bedrock wire NBT. It preserves numeric tag
types and finite floating-point values instead of round-tripping display SNBT.
Unrepresentable tags are refused; no unknown fields are silently discarded.
"""
import json
import math

from .model import Rejected

MAX_BYTES = 1024 * 1024
MAX_NODES = 65536
MAX_DEPTH = 32
INTEGER_TAGS = {'ByteTag': 8, 'ShortTag': 16, 'IntTag': 32, 'LongTag': 64}
ARRAY_TAGS = {'ByteArrayTag': 8, 'IntArrayTag': 32}


class _Budget:
    def __init__(self):
        self.nodes = 0
        self.text_bytes = 0

    def visit(self, depth, count=1):
        self.nodes += count
        if depth > MAX_DEPTH or self.nodes > MAX_NODES:
            raise Rejected('Held-item NBT exceeds nesting or element bounds.')

    def text(self, value):
        if not isinstance(value, str):
            raise Rejected('Invalid NBT string.')
        self.text_bytes += len(value.encode('utf-8'))
        if self.text_bytes > MAX_BYTES:
            raise Rejected('Held-item NBT exceeds the metadata limit.')
        return value


def _integer(value, bits):
    # Endstone 0.11 exposes ByteTag/ByteArrayTag as uint8_t, unlike signed shorts.
    lower, upper = (0, 256) if bits == 8 else (-(2 ** (bits-1)), 2 ** (bits-1))
    if type(value) is not int or not lower <= value < upper:
        raise Rejected('Invalid typed NBT integer.')
    return value


def _record(tag, budget, depth=0):
    from endstone import nbt
    budget.visit(depth)
    name = type(tag).__name__
    if getattr(nbt, name, None) is not type(tag):
        raise Rejected('Unsupported NBT tag; complete metadata cannot be preserved.')
    if name in INTEGER_TAGS:
        value = _integer(tag.value, INTEGER_TAGS[name])
    elif name in ('FloatTag', 'DoubleTag'):
        if not math.isfinite(tag.value):
            raise Rejected('Non-finite NBT floats require native binary serialization.')
        value = tag.value.hex()
    elif name == 'StringTag':
        value = budget.text(tag.value)
    elif name in ARRAY_TAGS:
        budget.visit(depth, len(tag))
        value = [_integer(v, ARRAY_TAGS[name]) for v in tag]
    elif name == 'ListTag':
        if len(tag) > MAX_NODES - budget.nodes:
            raise Rejected('Held-item NBT list exceeds element bounds.')
        value = [_record(v, budget, depth+1) for v in tag]
    elif name == 'CompoundTag':
        if len(tag) > MAX_NODES - budget.nodes:
            raise Rejected('Held-item NBT compound exceeds element bounds.')
        value = {budget.text(k): _record(v, budget, depth+1) for k, v in tag.items()}
    else:
        raise Rejected('Unsupported NBT tag; complete metadata cannot be preserved.')
    return [name, value]


def encode(tag):
    from endstone.nbt import CompoundTag
    if type(tag) is not CompoundTag:
        raise Rejected('Item metadata must be a complete CompoundTag.')
    try:
        raw = json.dumps(_record(tag, _Budget()), sort_keys=True, ensure_ascii=True,
                         separators=(',', ':'), allow_nan=False).encode('ascii')
    except (UnicodeError, RecursionError) as error:
        raise Rejected('Item metadata cannot be encoded losslessly.') from error
    if len(raw) > MAX_BYTES:
        raise Rejected('Held-item NBT exceeds the metadata limit.')
    return raw


def _tag(record, budget, depth=0):
    from endstone import nbt
    budget.visit(depth)
    if not isinstance(record, list) or len(record) != 2 or not isinstance(record[0], str):
        raise Rejected('Invalid typed NBT record.')
    name, value = record
    if name in INTEGER_TAGS:
        value = _integer(value, INTEGER_TAGS[name])
    elif name in ('FloatTag', 'DoubleTag'):
        if not isinstance(value, str) or len(value) > 32:
            raise Rejected('Invalid NBT float.')
        value = float.fromhex(value)
        if not math.isfinite(value):
            raise Rejected('Invalid NBT float.')
    elif name == 'StringTag':
        value = budget.text(value)
    elif name in ARRAY_TAGS:
        if not isinstance(value, list):
            raise Rejected('Invalid NBT array.')
        budget.visit(depth, len(value))
        value = [_integer(v, ARRAY_TAGS[name]) for v in value]
    elif name == 'ListTag':
        if not isinstance(value, list) or len(value) > MAX_NODES - budget.nodes:
            raise Rejected('Invalid NBT list.')
        value = [_tag(v, budget, depth+1) for v in value]
    elif name == 'CompoundTag':
        if not isinstance(value, dict) or len(value) > MAX_NODES - budget.nodes:
            raise Rejected('Invalid NBT compound.')
        value = {budget.text(k): _tag(v, budget, depth+1) for k, v in value.items()}
    else:
        raise Rejected('Unsupported NBT tag in journal.')
    return getattr(nbt, name)(value)


def _unique_object(pairs):
    result = {}
    for key, value in pairs:
        if key in result:
            raise Rejected('Duplicate NBT field in journal.')
        result[key] = value
    return result


def decode(raw):
    if not isinstance(raw, bytes) or not 1 <= len(raw) <= MAX_BYTES:
        raise Rejected('Invalid journal NBT size.')
    try:
        value = _tag(json.loads(raw, object_pairs_hook=_unique_object), _Budget())
        if encode(value) != raw:
            raise Rejected('Non-canonical or lossy journal NBT.')
        return value
    except (ValueError, TypeError, OverflowError, RecursionError) as error:
        raise Rejected('Invalid or unrepresentable journal NBT.') from error
