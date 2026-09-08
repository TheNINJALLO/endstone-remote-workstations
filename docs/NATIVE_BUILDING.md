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
by `tools/native/inspect-elf.sh`. GitHub Actions run 34273970612 successfully
executed this Docker build on Linux and exported a real x86-64 ELF plugin.
That artifact directly requires libc++, libc++abi, libunwind, libm, libgcc_s
and libc; it does not link libstdc++. Its directly versioned glibc symbols top
out at GLIBC_2.14, but transitive toolchain-library requirements also apply.
Keep the pinned libc++20 ABI; this symbol report is not a certificate for an
arbitrary Pterodactyl image. Local Windows Docker/WSL remains unavailable.
Linux BDS/loader ABI admission and gameplay are still blocked, independently
of this successful build.

The additional `sanitizer-export` Docker target runs the core, asynchronous
lifecycle and six journal crash-boundary tests under ASan/UBSan, then executes
100,000 bounded libFuzzer inputs each for NBT and the storage request decoder.
Storage seeds come from the exact hashed, sanitized original capture fixture.
The image's distro libFuzzer archive was
built against libstdc++ and failed to link with this project's libc++ build.
The test target therefore builds the official standalone libFuzzer sources at
LLVM 20.1.8 commit `87f0227cb60147a26a1eeb4fb06e3b505e9c7261` with libc++.
It never adds libstdc++ to either the plugin or fuzz process. Run it with:

```sh
docker buildx build --platform linux/amd64 --target sanitizer-export --output type=local,dest=dist/linux-sanitizers .
```

The journal tests terminate a dedicated process at each durable write phase.
They do not establish atomic recovery across BDS world/player saves.

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
