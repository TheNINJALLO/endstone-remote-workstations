#!/bin/sh
set -eu
test "$(uname -m)" = x86_64
test -f /inputs/input-manifest.json
test -f /inputs/SHA256SUMS
mkdir -p /lab
(cd /inputs && sha256sum --strict -c SHA256SUMS) > /lab/input-checksums.txt
python3 - <<'PY'
import hashlib
import json
from pathlib import Path
import subprocess
inputs=Path('/inputs')
manifest=json.loads((inputs/'input-manifest.json').read_text())
for kind in ('bds','runtime'):
    target=(inputs/manifest[kind+'_path']).resolve()
    if not target.is_relative_to(inputs) or not target.is_file():
        raise SystemExit('Private runtime path escapes the authorized mount')
    with target.open('rb') as source:
        digest=hashlib.file_digest(source,'sha256').hexdigest()
    if digest!=manifest[kind+'_sha256']:
        raise SystemExit('Private runtime fingerprint changed')
    with Path('/lab',kind+'-elf.txt').open('w') as report:
        for command in (['file',str(target)],['readelf','-h','-l','-d','-V',str(target)],['nm','-D','--defined-only',str(target)]):
            result=subprocess.run(command,stdout=report,stderr=subprocess.STDOUT,check=False)
            if result.returncode:
                raise SystemExit('ELF inspection failed; consult private /lab report')
print('Private Linux ELF inspection complete; this does not qualify native hooks, loader admission or client gameplay.')
PY
