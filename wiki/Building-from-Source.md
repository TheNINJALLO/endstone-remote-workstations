# Building from source

## Portable wheel and tests

Use CPython 3.11. From the repository root:

```text
python -m venv .venv
```

Activate `.venv` for your shell, then:

```text
python -m pip install -r requirements-build.lock
python -m pytest -q
python -m build --no-isolation
```

The normal build produces the portable wheel and source archive. It does not
produce or imply a Linux native bridge. Tests use stubs/fixtures; actual server
and client qualification is separate.

## Windows native companion

Use CPython 3.11 x86-64, Visual Studio 2022 Build Tools (qualified compiler:
MSVC 14.44.35207), the Windows SDK, and `requirements-native-windows.lock`.
Obtain Endstone 0.11.10 public headers, expected-lite 0.9.0 and MinHook 1.3.4
from their upstream repositories. Hash locks are in `native/build-inputs/`.
No proprietary server binary is supplied by this repository.

Set the following variables to your local files/directories:

```powershell
$env:RW_BDS_EXE = 'C:/server/bedrock_server.exe'
$env:RW_ENDSTONE_RUNTIME = 'C:/python/Lib/site-packages/endstone/endstone_runtime.dll'
$env:RW_ENDSTONE_HEADERS = 'C:/sources/endstone/include'
$env:RW_EXPECTED_HEADERS = 'C:/sources/expected-lite/include'
$env:RW_MINHOOK_SOURCE = 'C:/sources/minhook'
# Optional if Visual Studio Build Tools is installed elsewhere:
$env:RW_VCVARS64 = 'C:/path/to/VC/Auxiliary/Build/vcvars64.bat'
```

Then:

```text
python -m pip install -r requirements-native-windows.lock
python tools/build_windows_native.py --wheel dist/endstone_remote_workstations-0.4.0-py3-none-any.whl
```

The builder checks exact BDS/runtime hashes, expected-lite and MinHook file
hashes, executable function boundaries and native manager tables. It generates
the compatibility manifest from your verified local binaries, then compiles and
packages the Windows companion. A missing input or mismatched build is an error.
Never distribute the supplied BDS executable, runtime, PDBs or test worlds.

The companion is private-ABI code for the pinned build. Supporting another
server requires renewed ABI research and actual client tests, not changing a
version constant. [Native implementation details](https://github.com/TheNINJALLO/endstone-remote-workstations/blob/main/docs/NATIVE_WINDOWS.md).
