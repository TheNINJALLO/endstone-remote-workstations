"""Protocol 2169 riding views; these packets never change server mounts."""
import math
import struct

from .protocol import CodecError, Reader, svar


def identity(value):
    if type(value) is not int or not -(2**63) <= value < 2**63 or value in (-1, 0):
        raise CodecError('Invalid riding actor identity.')
    return value


def runtime(value):
    if type(value) is not int or not 1 <= value < 2**64:
        raise CodecError('Invalid riding runtime identity.')
    return value


def mount_stamp(value):
    if value is None:
        return None
    return identity(value['vehicle_id']), runtime(value['vehicle_runtime_id'])


def native_link(value, passenger_id, passenger_runtime):
    if value is None:
        return None
    value = dict(value)
    mount_stamp(value)
    if (identity(value['passenger_id']) != passenger_id
            or runtime(value['passenger_runtime_id']) != passenger_runtime
            or value['vehicle_id'] == passenger_id or type(value['type']) is not int or value['type'] not in (1, 2)
            or type(value['immediate']) is not bool or type(value['passenger_initiated']) is not bool
            or type(value['angular_velocity']) not in (int, float) or not math.isfinite(value['angular_velocity'])):
        raise CodecError('The native passenger link changed identity or shape.')
    return value


def decode_link(payload):
    reader = Reader(payload, limit=32)
    vehicle, passenger = identity(reader.svar(64)), identity(reader.svar(64))
    kind, immediate, initiated = reader.byte(), reader.boolean(), reader.boolean()
    angular, = struct.unpack('<f', reader.raw(4))
    reader.end()
    if kind not in (0, 1, 2) or vehicle == passenger or not math.isfinite(angular):
        raise CodecError('Invalid actor link.')
    return vehicle, passenger, kind, immediate, initiated, angular


def encode_link(link, *, detached=False):
    return (svar(link['vehicle_id'], 64)+svar(link['passenger_id'], 64)
            + bytes((0 if detached else link['type'], 1, link['passenger_initiated']))
            + struct.pack('<f', link['angular_velocity']))
