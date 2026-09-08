"""Development-only seeds from frozen sanitized captures, never private data."""
import hashlib
import json
from pathlib import Path
import sys

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
