# Native development builds

Use a separate checkout and disposable server data. These outputs are development
artifacts and must not be described as the completed all-UI release.

On Windows, install no additional system components automatically. With existing
VS 2022 Build Tools, fetch the pinned public SDK once and build:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File tools/native/fetch-sdk.ps1 -Destination .native-deps
$env:VCF_ENDSTONE_ROOT="$pwd/.native-deps/endstone"
$env:VCF_EXPECTED_ROOT="$pwd/.native-deps/expected-lite"
powershell -NoProfile -ExecutionPolicy Bypass -File tools/native/build-windows.ps1
```

The script imports Visual Studio's development environment in its own process,
uses CMake/Ninja, runs native tests and installs the DLL, SDK import library,
public headers, PDB and separate consumer into `dist/windows-release/`.
Use `-Preset windows-debug` for the separate debug build tree.
No project wheel or Python package participates in native loading.

Linux is the primary deployment target, but its runtime implementation remains
incomplete. With an existing Linux Docker engine reachable from the host:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File tools/native/build-linux.ps1
```

Linux contributors use `sh tools/native/build-linux.sh`.
The digest-pinned Clang 20/libc++/libc++abi container has separate toolchain,
unit-test, abi-lab, runtime-smoke and artifact-export targets.
The final export goes to `dist/linux-x64-dev/`; ELF requirements are recorded
by `tools/native/inspect-elf.sh`. The image has not yet been executed on the
current Windows host. Neither its glibc compatibility nor a Linux plugin
artifact has been qualified there.

The research profile takes explicit read-only private inputs:
`docker compose --profile research run --rm abi-lab`.
Only that profile grants ptrace/seccomp debugging allowances. Ordinary smoke
containers drop all capabilities, run without root, and never mount the Docker
socket. Set `VCF_LINUX_INPUTS` to a dedicated authorized directory. Runtime
smoke additionally requires separately confirmed license handling, exact input
hashes and a private launcher that uses the disposable `/data` world. Missing
inputs exit with a blocked status; the script never downloads BDS or accepts
licenses for the operator.

`python tools/native/scope.py` checks the immutable catalog, source hashes,
aliases, permissions and both platform reports. Python here is a development
test tool. `--require-qualified` is a distinct acceptance gate and currently
fails because native/custom/client requirements are incomplete. Passing a
compiler or core test job cannot override it.

Historical build instructions in BUILDING.md and NATIVE_WINDOWS.md describe
the old wheel/companion. Do not use them to package this native branch.
