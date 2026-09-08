"""Build only our exact-build Windows companion; never distribute BDS binaries."""
import argparse
import base64
import csv
import hashlib
import io
import json
import os
from pathlib import Path
import shutil
import struct
import subprocess
import sys
import zipfile

import pefile
import pybind11

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / '.runtime/native-release'
ENDSTONE_HEADERS = Path(os.environ.get('RW_ENDSTONE_HEADERS', str(ROOT/'.research/endstone-0.11.10/include')))
EXPECTED_HEADERS = Path(os.environ.get('RW_EXPECTED_HEADERS', str(ROOT/'.research/expected-lite/include')))
MINHOOK_SOURCE = Path(os.environ.get('RW_MINHOOK_SOURCE', str(ROOT/'.research/minhook')))
INPUT_LOCKS = ROOT/'native/build-inputs'
SYMBOLS = [
    (True, 1093360, 12, 'Player::getInventory; exact runtime PDB and live owner cross-check'),
    (True, 1544320, 1539, 'Endstone native NBT conversion; exact runtime PDB'),
    (True, 1546784, 2032, 'Endstone public-to-native NBT conversion; exact runtime PDB'),
    (True, 823904, 119, 'EndstoneItemStack::fromMinecraft; exact runtime PDB including cleanup funclet'),
    (False, 0x1bc4b20, 311, 'Native stored ItemStack construction; actual inventory loadFromTag caller'),
    (False, 0x2069a0, 1269, 'Inventory::setItemWithForceBalance; live player virtual 13'),
    (False, 0x2ddc80, 73, 'Container::setContainerChanged; live listener invocation and removal'),
    (False, 0x1f41ff0, 488, 'FillingContainer::saveToTag; compiled ABI and live independent copy verification'),
    (False, 0x1bcbdc0, 1088, 'ItemStackBase save; actual native inventory serializer direct call'),
    (False, 0xeac810, 224, 'Native ListTag deleting destructor; live save/copy cleanup'),
    (False, 0x1bfc420, 171, 'Native ListTag equality; live copied inventory comparison'),
    (False, 0x1bfc110, 22, 'Native ListTag copy; compiled Tag virtual 9 and live deep copy'),
    (True, 748752, 109, 'EndstoneActorBase<Player,Player>::getHandle; matching runtime PDB'),
    (True, 0x112a60, 251, 'Actor::getVehicle; matching Endstone PDB leaf; PassengerComponent 0x98e40c0e'),
    (False, 0xde88f0, 231, 'Actor::getRuntimeID; actual AddActor factory caller confirms 64-bit output argument'),
    (False, 0xab8fa0, 4250, 'Native AddActor payload construction; links produced into payload +0xb8'),
    (False, 0xab2970, 369, 'Native AddActorPacket move constructor; link vector at +0xe8'),
    (False, 0xdf2300, 490, 'Native Actor link serialization; 32-byte rows, riding/passenger seat preserved'),
    (False, 0x225ad0, 65, 'native container opening guard'),
    (False, 0x739ab0, 12, 'Level ActorUniqueID lookup leaf thunk; vtable offset 0x1f0, native openTrading call ABI'),
    (False, 0x739f60, 12, 'Level ActorRuntimeID lookup leaf thunk; vtable offset 0x200, native command-minecart handler'),
    (False, 0x14e7fc0, 889, 'Native runtime entity lookup; matched to the source ActorUniqueID and actual command cart'),
    (False, 0x154ea80, 239, 'ActorUniqueID lookup thunk target; excludes removed entities'),
    (False, 0xde8bf0, 282, 'native ActorUniqueID accessor; source identity verification'),
    (False, 0xe1e2c0, 226, 'Actor::openContainerComponent; compiled virtual 109 and live vehicle binding'),
    (False, 0x2486af0, 1463, 'ContainerComponent real open; actual source ActorUniqueID and native container dispatch'),
    (False, 0x24869f0, 247, 'ContainerComponent canOpen; retains native interaction, entity lifecycle and owner-only rules'),
    (False, 0x6326520, 706, 'Horse native equipment opener; retains actual component and active rider checks'),
    (False, 0x1f3d5d0, 29, 'GameMode::interact exact leaf; compiled virtual 14, real merchant interaction stack'),
    (False, 0x1f36410, 2420, 'Normal game-mode entity interaction; retains interaction pipeline and events'),
    (False, 0x24cc800, 760, 'Actual merchant interaction callback; customer identity and native offer preparation'),
    (False, 0x693350, 956, 'Real Player::openTrading called by native interaction; actual economy manager'),
    (False, 0x47135a0, 358, 'Economy trade manager construction with actual source ActorUniqueID'),
    (False, 0x4714920, 895, 'Economy trade isUsable; preserves customer, profession, life and inventory checks while parameterizing distance'),
    (False, 0xdec0c0, 365, 'Actor owner accessor; compared with requesting native Player ActorUniqueID before open and while active'),
    (False, 0xdef160, 408, 'Actor::tryCreateAddActorPacket; compiled virtual 23; live actual source serialization'),
    (False, 0xaf5750, 57, 'Native AddActorPacket deleting destructor; vtable 0xa6154b0'),
    (False, 0x692b40, 681, 'Player::sendInventory(bool); compiled Mob vtable offset 1312; sends real inventories without item writes'),
    (False, 0x693aa0, 429, 'Player::openInventory(); compiled virtual slot 196; real player inventory and embedded 2x2/equipment/recipe UI'),
    (False, 0x693260, 238, 'Player native packet sender; verified virtual slot 228'),
    (False, 0xddfc40, 116, 'native actor block region accessor; exact dispatcher call'),
    (False, 0x14fb650, 309, 'loaded block actor lookup by actual source position'),
    (False, 0x29a4430, 818, 'beacon native update packet; live serialization verified'),
    (False, 0x29235c0, 1126, 'crafter native update packet; live block actor dispatch'),
    (False, 0x290b8c0, 824, 'lectern native book/page update; live source and display serialization'),
    (False, 0x693c50, 331, 'Player::openBook; compiled virtual slot 192; actual lectern actor required'),
    (False, 0x693da0, 436, 'Player::openSign; compiled virtual 206; acquires real editor lock and queues native OpenSign'),
    (False, 0x4627ab0, 996, 'SignBlockActor native authoritative update packet; virtual 18'),
    (False, 0x4627f80, 1683, 'SignBlockActor native update apply; unchanged authoritative state releases editor and marks dirty'),
    (False, 0x462a640, 46, 'Native sign canEdit; exact ActorUniqueID editor match and unwaxed state'),
    (False, 0xa2650, 3, 'Jigsaw inherited canEdit true leaf; remote adapter additionally requires native operator-block admission'),
    (False, 0x2902f00, 118, 'Jigsaw inherited BlockActor apply callback; actual virtual deserialization'),
    (False, 0x5ee64b0, 65, 'Jigsaw native load virtual 1; retained authoritative deserialization'),
    (False, 0x2202f0, 398, 'Native operator-block ability, Creative and interaction permission admission'),
    (False, 0x291f3d0, 865, 'CommandBlockActor authoritative metadata packet; live virtual 18'),
    (False, 0xa64cb0, 2367, 'CommandBlockUpdate native server handler; raw packet reference and block gate a64f3b'),
    (False, 0xaf9d30, 764, 'CommandBlockUpdate payload copy constructor; tracks native deferred filter copies'),
    (False, 0xafa0f0, 207, 'CommandBlockUpdate payload destructor; retires native copy identities'),
    (False, 0xa659a0, 642, 'Native command metadata apply; revalidate owned deferred saves before original execution'),
    (False, 0xaf9840, 1129, 'Native filtered command-name callback; retained player resolution and copied payload ownership'),
    (False, 0x276e6c0, 1191, 'Native command-cart component apply; retains execution/metadata handling'),
    (False, 0x2609f0, 246, 'Native DataItem string setter; validates observed source metadata member layout'),
    (False, 0xa6b3b0, 1485, 'StructureBlockUpdate handler; actual player RSI, packet RDI; scoped distance gate a6b480'),
    (False, 0xafc080, 838, 'StructureEditorData copy; tracks native filter payload ownership'),
    (False, 0xb0acb0, 657, 'StructureEditorData move; preserves revocation across filter closure moves'),
    (False, 0xafc450, 218, 'StructureEditorData destructor; retires payload identity'),
    (False, 0xa6bd40, 2503, 'Structure native apply; context contains region at zero and legacy namespace at eight; four arguments retained'),
    (False, 0xa6bbb0, 260, 'Native structure filter closure factory; moves data to embedded packet plus 0x40'),
    (False, 0xa6efa0, 2117, 'Native structure template request handler; actual export confirmed with packet 133 and matching mcstructure NBT'),
    (False, 0xa935b0, 1822, 'BlockActorData server handler; scoped distance gate at a9368a, original validation/filtering retained'),
    (False, 0xa93f90, 1192, 'Native filtered sign save callback; scoped distance gate at a9405d, original source/owner/apply retained'),
    (False, 0x21f0a0, 370, 'native player interaction permission/mode check; retained from lectern/crafter handlers'),
    (False, 0x1bc7df0, 308, 'native book item validation; original lectern handler call at 0xa96c86'),
    (False, 0x5eeecd0, 99, 'native lectern page setter; actual comparator and dirty state; original handler tail call'),
    (False, 0x3829da0, 93, 'native block-actor packet deleting destructor'),
    (False, 0x693f80, 139, 'workbench screen source and window allocation'),
    (False, 0x274b550, 83, 'ItemStackNetManager context callback; live vtable slot 6'),
    (False, 0x17e9230, 1036, 'anvil native container factory; captured vanilla open stack'),
    (False, 0x17ef3e0, 1036, 'stonecutter native container factory; exact dispatch table'),
    (False, 0x17ed740, 1036, 'grindstone native container factory; exact dispatch table and live disenchant'),
    (False, 0x17e8cc0, 1036, 'smithing native container factory; exact dispatch table and live upgrade'),
    (False, 0x17ee450, 1036, 'loom native container factory; exact dispatch table and live banner patterns'),
    (False, 0x17ea5d0, 1036, 'cartography native container factory; exact dispatch table and live map scale'),
    (False, 0x17eb540, 910, 'real chest/barrel native manager factory; live linked chest round trip'),
    (False, 0x1820dd0, 387, 'real block hopper manager factory; native dispatcher branch and source context'),
    (False, 0x17ed2a0, 892, 'real furnace manager factory; exact dispatch and live manager identity'),
    (False, 0x17e9c90, 908, 'real blast furnace manager factory; exact dispatch and live manager identity'),
    (False, 0x17eef30, 908, 'real smoker manager factory; exact dispatch and live manager identity'),
    (False, 0x17ea140, 876, 'real brewing stand manager factory; exact dispatch and live manager identity'),
    (False, 0x17ece10, 876, 'real enchanting table manager factory; exact dispatch and live manager identity'),
    (False, 0x17eba00, 876, 'real dispenser manager factory; exact dispatch and live manager identity'),
    (False, 0x17ebe90, 876, 'real dropper manager factory; exact dispatch and live manager identity'),
    (False, 0x17e97a0, 972, 'real beacon manager factory; exact dispatch and live manager identity'),
    (False, 0x17ec320, 988, 'real crafter manager factory; exact dispatch and live manager identity'),
    (False, 0x8a003a0, 249, 'chest manager validity; delegates real slot/container checks and parameterized distance'),
    (False, 0x2e02fc0, 886, 'base linked validity; loaded chunk, block actor type, slot validity and parameterized distance'),
    (False, 0x7ec4050, 540, 'furnace manager validity; preserved original checks with extended distance'),
    (False, 0x89fdf10, 564, 'brewing manager validity; preserved original checks with extended distance'),
    (False, 0x2e01750, 379, 'enchanting manager validity; preserved original checks with extended distance'),
    (False, 0x73ca60, 161, 'ILevel::getLevelData at virtual offset 0x560; owner reference and returned data identity checked'),
    (False, 0x92d650, 14417, 'LevelData NBT deserializer: educationFeaturesEnabled normalized byte at 0x4a8; live chemistry world readback'),
    (False, 0x17eab40, 1036, 'Native Compound Creator container factory; exact dispatcher and live manager identity'),
    (False, 0x17ec8a0, 1036, 'Native Element Constructor container factory; genuine hydrogen output transaction'),
    (False, 0x17ee9c0, 1036, 'Native Material Reducer container factory; exact dispatcher'),
    (False, 0x17edfc0, 876, 'Native Lab Table container factory; retains real block actor inputs/reactions'),
    (False, 0x8a01300, 117, 'Compound Creator validity; retain submodel and source checks with extended distance'),
    (False, 0x7ebfe10, 117, 'Element Constructor validity; retain submodel and source checks with extended distance'),
    (False, 0x7ecd2f0, 117, 'Material Reducer validity; retain submodel and source checks with extended distance'),
    (False, 0x7ecade0, 117, 'Lab Table validity; retain submodel and real source checks with extended distance'),
]
LINKED_TABLES = (0xa825c50, 0xa802bd0, 0xa825af0, 0xa6de070, 0xa802d30,
                 0xa825e60, 0xa802a70, 0xa825a40, 0xa825db0, 0xa802de0, 0xa733dc0,
                 0xa825d00, 0xa802b20, 0xa802ff0, 0xa802e90)
BDS_HASH = '92d09c7b74ac6a9805bafc166d8e0a13ac9e5db73dbbb0819e5a14093699d44f'
RUNTIME_HASH = '0c6f0861c5f9a677058b25776d975654a3586a80d2e4421b069b5e98f536f819'


def build():
    OUT.mkdir(parents=True, exist_ok=True)
    generated = OUT/'endstone'
    generated.mkdir(exist_ok=True)
    version = (ENDSTONE_HEADERS/'endstone/version.h.in').read_text()
    for key, value in {'PROJECT_VERSION_MAJOR':'0', 'PROJECT_VERSION_MINOR':'11',
                       'PROJECT_VERSION_PATCH':'10', 'ENDSTONE_VERSION_FULL':'0.11.10'}.items():
        version = version.replace('@'+key+'@', value)
    (generated/'version.h').write_text(version)
    dependency = json.loads((INPUT_LOCKS/'expected-lite.lock.json').read_text())
    header = EXPECTED_HEADERS/'nonstd/expected.hpp'
    if hashlib.sha256(header.read_bytes()).hexdigest() != dependency['sha256']:
        raise RuntimeError('Native header dependency hash mismatch')
    hook_sources = json.loads((INPUT_LOCKS/'minhook-source.lock.json').read_text())
    for name, expected in hook_sources['files'].items():
        if hashlib.sha256((MINHOOK_SOURCE/name).read_bytes()).hexdigest() != expected:
            raise RuntimeError('Pinned MinHook source changed: '+name)
    paths = {False: Path(os.environ.get('RW_BDS_EXE', str(ROOT/'.runtime/windows-smoke-isolated/bedrock_server.exe'))),
             True: Path(os.environ.get('RW_ENDSTONE_RUNTIME', str(ROOT/'.venv/Lib/site-packages/endstone/endstone_runtime.dll')))}
    binaries = {}
    for runtime, path in paths.items():
        expected = RUNTIME_HASH if runtime else BDS_HASH
        if hashlib.file_digest(path.open('rb'), 'sha256').hexdigest() != expected:
            raise RuntimeError(f'Unsupported build: {path.name}')
        binaries[runtime] = pefile.PE(str(path), fast_load=True)
    records = []
    for runtime, rva, size, purpose in SYMBOLS:
        binary = binaries[runtime]
        directory = binary.OPTIONAL_HEADER.DATA_DIRECTORY[3]
        ranges = {start: end for start, end, _ in struct.iter_unpack('<III', binary.get_data(directory.VirtualAddress, directory.Size))}
        held_getter_leaf = (runtime and rva == 1093360 and size == 12
                           and binary.get_data(rva, 16) == bytes.fromhex('488b89b8050000e924cef6ffcccccccc'))
        leaf_thunk = (not runtime and rva == 0x739ab0 and size == 12
                      and binary.get_data(rva, 12) == bytes.fromhex('488b8948040000e9c44fe100')
                      and binary.get_data(rva+12, 4) == b'\xcc'*4)
        leaf_thunk = leaf_thunk or (not runtime and rva == 0x1f3d5d0 and size == 29
                      and binary.get_data(rva, size) == bytes.fromhex('80b9c8000000010f85338effff803d884da20a000f84268effff31c0c3')
                      and binary.get_data(rva+size, 3) == b'\xcc'*3)
        leaf_thunk = leaf_thunk or (not runtime and rva == 0xa2650 and size == 3
                      and binary.get_data(rva, 16) == b'\xb0\x01\xc3'+b'\xcc'*13)
        leaf_thunk = leaf_thunk or (not runtime and rva == 0x739f60 and size == 12
                      and binary.get_data(rva, 16) == bytes.fromhex('488b89b0040000e954e0da00')+b'\xcc'*4)
        leaf_thunk = leaf_thunk or held_getter_leaf
        # The matched PDB span includes the conversion's switch tables after
        # its 1164-byte unwind range. Hash the whole symbol, including tables.
        leaf_thunk = leaf_thunk or (runtime and rva == 1544320 and size == 1539
                                   and ranges.get(rva) == rva + 1164)
        leaf_thunk = leaf_thunk or (runtime and (rva,size,ranges.get(rva,0)-rva) in
                                   ((1546784,2032,1361),(823904,119,69)))
        if ranges.get(rva) != rva + size and not leaf_thunk:
            # Matching runtime PDB records this leaf (no stack/unwind entry).
            # Pin the full body and its boundary, not only the entry bytes.
            leaf_thunk = (runtime and rva == 0x112a60 and size == 251
                          and binary.get_data(rva, 4) == bytes.fromhex('488b5110')
                          and binary.get_data(rva+size-3, 4) == bytes.fromhex('48ffe0cc'))
        if ranges.get(rva) != rva + size and not leaf_thunk:
            raise RuntimeError(f'Unexpected unwind function range: {purpose}')
        records.append(dict(runtime=runtime, rva=rva, size=size, purpose=purpose,
                            sha256=hashlib.sha256(binary.get_data(rva, size)).hexdigest()))
    tables = []
    binary = binaries[False]
    for rva in LINKED_TABLES:
        # Preserve the prefix and pin the adjacent table boundary as well as
        # all 22 entries. Store RVAs so ASLR does not change the check.
        pointers = struct.unpack('<24Q', binary.get_data(rva-8, 24*8))
        relative = [pointer-binary.OPTIONAL_HEADER.ImageBase if pointer else 0 for pointer in pointers]
        if any(not 0 <= value < binary.OPTIONAL_HEADER.SizeOfImage for value in relative):
            raise RuntimeError('Unexpected native manager table layout')
        tables.append(dict(rva=rva, entries=22, pointers=relative))
    manifest = dict(bridge_api=1, platform='windows-x86_64', python='CPython 3.11',
                    endstone_runtime='0.11.10', public_api='0.11', protocol=2169,
                    bds_sha256=BDS_HASH, runtime_sha256=RUNTIME_HASH, symbols=records,
                    ownership='BDS retains container models and signs; bridge retains native shared ownership of submitted sign packets until native callbacks finish; no retained native player or item pointers',
                    hooks={'sign': {'installed_on_first_open': True, 'sites': [0xa9368a, 0xa9405d],
                        'scope': 'Matching authorized player/source/generation and one owned save packet; native canEdit retained; retired owned packets rejected even within ordinary distance',
                        'source': 'native/sign_distance_gate.asm'},
                        'commandblock': {'installed_on_first_open': True,
                            'sites': [0xa64cb0, 0xaf9d30, 0xafa0f0, 0xa659a0, 0xa64f3b, 0xa6500f],
                            'scope': 'One authorized block or command-cart save; entity RuntimeID and ActorUniqueID must resolve the same live actor; native payload copies share a weak revocable player/source/generation lease; native application rechecks actual public player and permissions',
                            'source': 'native/command_bridge.h; native/editor_distance_gate.asm',
                            'unwind': 'Normal CALL frame with MASM unwind metadata; owner-thread installation'},
                        'structure': {'installed_on_first_open': True,
                            'sites': [0xa6b3b0, 0xafc080, 0xb0acb0, 0xafc450, 0xa6bd40, 0xa6b480],
                            'scope': 'Authorized real source and player; copy/move/destructor ownership; native four-argument apply rechecks revocable authority. Repeated updates remain serialized while the editor stays open.',
                            'source': 'native/structure_bridge.h; native/editor_distance_gate.asm',
                            'unwind': 'Normal CALL frame with MASM unwind metadata; owner-thread installation'}},
                    instance_vtable_adapters=True, linked_manager_tables=tables,
                    owned_controls={'lectern': 'Python cancels owned page packets and queues the guarded native page setter; original distance check omitted; all other original checks retained'},
                    source='native/windows_bridge.cpp')
    (OUT/'native-compatibility.json').write_text(json.dumps(manifest, indent=2)+'\n')
    header = ['#pragma once', '#include <cstdint>', '#include <array>',
              'struct NativeSymbol { bool runtime; uintptr_t rva; unsigned long size; const char* sha256; };',
              f'constexpr const char* BDS_SHA256="{BDS_HASH}";',
              f'constexpr const char* RUNTIME_SHA256="{RUNTIME_HASH}";',
              'constexpr NativeSymbol NATIVE_SYMBOLS[] = {']
    header += ['{%s,%d,%d,"%s"},' % (str(r['runtime']).lower(), r['rva'], r['size'], r['sha256']) for r in records]
    header += ['};', 'struct NativeTable { uintptr_t rva; std::array<uintptr_t,24> pointers; };',
               'constexpr NativeTable NATIVE_TABLES[] = {']
    header += ['{%d,{%s}},' % (table['rva'], ','.join(map(str, table['pointers']))) for table in tables]
    (OUT/'native_build.h').write_text('\n'.join(header+['};'])+'\n')
    vcvars = Path(os.environ.get('RW_VCVARS64', r'C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat'))
    setup = subprocess.run(f'cmd.exe /d /s /c ""{vcvars}" >nul && set"', capture_output=True, text=True, check=True)
    env = {key.upper(): value for key, value in os.environ.items()}
    for line in setup.stdout.splitlines():
        if '=' in line:
            key, value = line.split('=', 1)
            if key:
                env[key.upper()] = value
    objects = []
    for name in ('sign_distance_gate', 'editor_distance_gate'):
        object_path = OUT/(name+'.obj')
        assembler = subprocess.run([shutil.which('ml64.exe', path=env['PATH']), '/nologo', '/c',
            '/Fo'+str(object_path), str(ROOT/'native'/(name+'.asm'))],
            cwd=OUT, env=env, capture_output=True, text=True)
        if assembler.returncode:
            raise RuntimeError(assembler.stdout+assembler.stderr)
        # Normalize MASM's COFF timestamp before deterministic linking.
        object_data = bytearray(object_path.read_bytes())
        if object_data[:2] != b'\x64\x86':
            raise RuntimeError('Unexpected MASM COFF object format')
        object_data[4:8] = b'\0'*4
        object_path.write_bytes(object_data)
        objects.append(str(object_path))
    command = [shutil.which('cl.exe', path=env['PATH']), '/nologo', '/LD', '/MD', '/std:c++20', '/EHsc', '/O2', '/W4',
               '/I'+pybind11.get_include(), '/I'+str(Path(sys.base_prefix)/'include'), '/I'+str(OUT),
               '/I'+str(ENDSTONE_HEADERS), '/I'+str(EXPECTED_HEADERS),
               '/I'+str(MINHOOK_SOURCE/'include'), str(ROOT/'native/windows_bridge.cpp'),
               *objects,
               *[str(MINHOOK_SOURCE/'src'/name) for name in ['buffer.c','hook.c','trampoline.c','hde/hde64.c']],
               '/link', '/Brepro', '/LIBPATH:'+str(Path(sys.base_prefix)/'libs'),
               '/OUT:'+str(OUT/'_rw_native.pyd'), 'bcrypt.lib']
    result = subprocess.run(command, cwd=OUT, env=env, capture_output=True, text=True)
    (OUT/'build.log').write_text(result.stdout+result.stderr)
    if result.returncode:
        raise RuntimeError(result.stdout+result.stderr)
    (ROOT/'research').mkdir(exist_ok=True)
    shutil.copyfile(OUT/'native-compatibility.json', ROOT/'research/native-compatibility.json')
    print('Built', OUT/'_rw_native.pyd')


def package(wheel):
    with zipfile.ZipFile(wheel) as archive:
        files = {name: archive.read(name) for name in archive.namelist()}
    dist = next(name.rsplit('/', 1)[0] for name in files if name.endswith('.dist-info/WHEEL'))
    files[dist+'/WHEEL'] = b'Wheel-Version: 1.0\nGenerator: RemoteWorkstations native builder\nRoot-Is-Purelib: false\nTag: cp311-cp311-win_amd64\n'
    files['endstone_remote_workstations/_rw_native.pyd'] = (OUT/'_rw_native.pyd').read_bytes()
    files['endstone_remote_workstations/native-compatibility.json'] = (OUT/'native-compatibility.json').read_bytes()
    del files[dist+'/RECORD']
    record = io.StringIO(newline='')
    writer = csv.writer(record, lineterminator='\n')
    for name, data in sorted(files.items()):
        writer.writerow([name, 'sha256='+base64.urlsafe_b64encode(hashlib.sha256(data).digest()).rstrip(b'=').decode(), len(data)])
    writer.writerow([dist+'/RECORD', '', ''])
    files[dist+'/RECORD'] = record.getvalue().encode()
    target = wheel.with_name(wheel.name.replace('py3-none-any', 'cp311-cp311-win_amd64'))
    with zipfile.ZipFile(target, 'w', zipfile.ZIP_DEFLATED) as archive:
        for name, data in sorted(files.items()):
            info = zipfile.ZipInfo(name, (2026, 1, 1, 0, 0, 0))
            info.compress_type = zipfile.ZIP_DEFLATED
            info.external_attr = 0o644 << 16
            archive.writestr(info, data)
    print('Packaged', target)


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--wheel', type=Path)
    args = parser.parse_args()
    build()
    if args.wheel:
        package(args.wheel)
