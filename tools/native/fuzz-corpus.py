"""Development-only seeds from frozen sanitized captures, never private data."""
import hashlib
import json
from pathlib import Path
import sys
import struct

source=Path('tests/native/fixtures/helper-ender-2169.json')
raw=source.read_bytes()
if hashlib.sha256(raw).hexdigest()!='be5edbd322c6afbd222072f084cb376c994b37b8b2df1857060adac23cd5c65d':
    raise SystemExit('Refusing changed capture corpus provenance')
destination=Path(sys.argv[1])
destination.mkdir(parents=True,exist_ok=True)
for i,packet in enumerate(json.loads(raw)['packets']):
    if packet['packet_id'] in (147,148):
        (destination/f'capture-{i}.bin').write_bytes(bytes.fromhex(packet['payload_hex']))

if len(sys.argv)>2:
    source=Path('tests/native/fixtures/item-wire-conformance-2169.json')
    raw=source.read_bytes()
    if hashlib.sha256(raw).hexdigest()!='5376d1c38b47a8f8bc775c9cf7624c145500c2eea35c690ed21ec48d7bd5ae56':
        raise SystemExit('Refusing changed independent item wire fixture')
    destination=Path(sys.argv[2]);destination.mkdir(parents=True,exist_ok=True)
    for i,packet in enumerate(json.loads(raw)['packets']):
        (destination/f'generated-{i}.bin').write_bytes(bytes.fromhex(packet['payload_hex']))

if len(sys.argv)>3:
    # Authored saved compounds: no player inventory or private capture input.
    destination=Path(sys.argv[3]);destination.mkdir(parents=True,exist_ok=True)
    def text(value):
        encoded=value.encode();return struct.pack('<H',len(encoded))+encoded
    def field(kind,name,payload):return bytes([kind])+text(name)+payload
    def item(name,count,tag=b''):
        return (b'\x0a\0\0'+field(1,'Count',bytes([count]))+field(2,'Damage',b'\0\0')
            +field(8,'Name',text(name))+field(1,'WasPickedUp',b'\0')
            +(field(10,'tag',tag) if tag else b'')+b'\0')
    metadata=field(9,'TypedEmpty',b'\x04\0\0\0\0')+field(6,'negative_zero',struct.pack('<d',-0.0))+b'\0'
    stone=item('minecraft:stone',3,metadata)
    row=stone[3:-1]+field(1,'Slot',b'\x1a')+b'\0'
    storage=field(9,'Items',b'\x0a'+struct.pack('<I',1)+row)
    storage+=field(8,'remote_workstations:held_id',text('cb44017a-6817-4e27-8a27-aa91e9e5f6cd'))+b'\0'
    for name,value in {'stone':stone,'filled-shulker':item('minecraft:undyed_shulker_box',1,storage),
                       'empty-shulker':item('minecraft:undyed_shulker_box',1)}.items():
        (destination/(name+'.bin')).write_bytes(value)
    movement=bytes(32)+bytes([1,0,0,0,0])+bytes(8)+b'\0'+bytes(12)+bytes([1,0,1,0,1,0])
    (destination/'movement.bin').write_bytes(movement)
