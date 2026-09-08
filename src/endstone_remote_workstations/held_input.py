"""Recognize inventory-bearing input inside the admitted PlayerAuthInput layout."""
import math
import struct
from .protocol import Reader, CodecError


def inventory_action(payload):
    reader = Reader(payload)
    if not all(math.isfinite(v) for v in struct.unpack('<8f', reader.raw(32))) or not reader.boolean():
        raise CodecError('Invalid held input prefix.')
    count = reader.uvar()
    if count > 66:
        raise CodecError('Too many held input flags.')
    flags = [reader.svar() for _ in range(count)]
    if len(set(flags)) != count or any(not 0 <= flag <= 65 for flag in flags):
        raise CodecError('Invalid held input flags.')
    reader.uvar()
    reader.uvar()
    reader.svar()
    reader.raw(8)
    reader.uvar(64)
    reader.raw(12)
    # Explicit optional markers are independent of the input flag collection.
    # Reject at the first present payload; its client data is never executed.
    for _ in range(3):
        if not reader.boolean():
            raise CodecError('Invalid held input optional marker.')
        if reader.boolean():
            return True
    return bool(set(flags) & {34, 35, 36})
