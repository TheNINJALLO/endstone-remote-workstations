# Native implementation checkpoint — incomplete

The all-UI migration is **not complete or release-ready**. The tested native
source is `577edd781a112625dfb620d45972ba98d743b765` on
`feature/native-virtual-container-framework`. The stable release and `main`
remain unchanged. No new release, tag or production deployment was performed.

## Implemented and tested

The project builds a true C++20 plugin, independently producing a Windows DLL
and Linux ELF `.so`, with no project Python/pybind runtime. Platform-neutral
code owns sessions, deferred callbacks, scoped named/exported actions, action
listing, guards, item policies, bounded transaction models and durable journal
records. C ABI 1.1 preserves the 1.0 function-table prefix and caller buffers.
Public C++ SDK headers and Windows import libraries are installed separately.

Windows binds fingerprinted original native primitives. Eleven original-mode
requests have experimental orchestration: crafting, anvil, grindstone,
smithing, stonecutter, loom, cartography, inventory2x2, armor, offhand and
recipebook. The public default disables this opt-in. Their new native client
behavior remains unqualified. No new custom screen is claimed complete.

Storage code now decodes request/response packets, checks identities and
policies, reserves unfinished cursor gestures, and refuses replay/uncertain
writes. Item wire code preserves complete descriptors and parses bounded
registry/content/slot packets. The plugin observes these packets passively from
the server scheduler and exposes counters through `/vcf diagnose`. This observer
does not write native inventories or authorize transactions. See
[storage implementation and limitations](NATIVE_STORAGE.md).

The isolated Windows server on `127.0.0.1:29169` loaded the current DLL plus two
independent consumers: the original ABI 1.0 UICatalogShowcase binary and ABI 1.1
NativePassthrough. All 69 canonical IDs resolve. The status and item-observation
diagnostics commands ran successfully. There were no connected clients, so
no current-artifact rendering, item interaction or observation of a real player
registry is reported as passed.

## Builds and checks actually run

- `tools/native/build-windows.ps1`: local Windows release build, SDK/consumer
  installation and all 12 CTest jobs passed.
- Native CI [34285780448](https://github.com/TheNINJALLO/endstone-remote-workstations/actions/runs/34285780448):
  Windows release/debug, Linux Docker build and Linux sanitizers passed.
- The Linux Docker sanitizer target passed all 12 CTest jobs and 100,000
  libFuzzer inputs each for NBT, storage and item wire decoding: 300,000 total.
- Test output records 50,181 core checks, 10,309 storage checks and 20,754 item
  wire checks. Six CTest jobs restart a dedicated journal process after a forced
  exit at separate durable boundaries.
- Legacy regression CI [34285780454](https://github.com/TheNINJALLO/endstone-remote-workstations/actions/runs/34285780454)
  passed its 661-test Python suite and portable build checks. This does not
  qualify the new native gameplay implementation.
- The separate full-scope acceptance job failed as required: all 138
  canonical/platform and 20 additional API-surface/platform outcomes remain
  unqualified. The strengthened gate also tests forged green labels, dropped
  aliases/commands and attempts to reuse old captures or build logs.

Raw public CI logs and artifact identities are indexed by
[`checkpoint-577edd7.json`](../research/native-evidence/checkpoint-577edd7.json).
Historical snapshots retain their original revision and hashes.

## Exact development artifacts

| Target | Local artifact | SHA-256 |
|---|---|---|
| Windows x64 | `dist/windows-release/plugins/endstone_onistone_vcf.dll` | `ee0e5fe6155fea9470a4438d935ff5d7a02ea64962685fa7223dc90e978254b3` |
| Linux x64 | `dist/linux-ci-577edd7/plugins/endstone_onistone_vcf.so` | `f79e40db42ed9e55afc44e7a962f3d53018ae06e3116b252c2c0e998db13b5a9` |

Paths are relative to the native feature checkout. The installed isolated
Windows DLL was hash-checked against the first row. Windows PDBs are under
`dist/windows-release/symbols`; SDK files and separately installed example
binaries are in each artifact tree. The Linux ELF still includes its debug
information. These are development outputs, not final release packages.

## Verified identities and practical limits

The public SDK is Endstone 0.11.10 at
`8f84d6f5b556916597ed5b6b71329b2ed3ca8fc8`; public API family 0.11 is not a
certificate for every runtime in that family. The Windows server loaded BDS
1.26.45.1, protocol 2169, and Endstone 0.11.10. BDS SHA-256 is
`92d09c7b74ac6a9805bafc166d8e0a13ac9e5db73dbbb0819e5a14093699d44f`;
the Endstone runtime SHA-256 is
`0c6f0861c5f9a677058b25776d975654a3586a80d2e4421b069b5e98f536f819`.
The private primitive manifest was checked in loaded Windows memory.

Windows uses MSVC 19.44.35222, toolset 14.44.35207, CMake 3.31.6, the dynamic
MSVC runtime and iterator ABI level 0. Linux CI uses the pinned Clang 20.1.8,
libc++/libc++abi toolchain inside Linux Docker. ELF inspection finds libc++,
libc++abi, libunwind, libm, libgcc_s and libc dependencies, with no libstdc++.
Transitive library requirements still apply; arbitrary Pterodactyl images are
not certified. No Linux BDS runtime or Onistone ABI has been qualified.

Windows Bedrock 1.26.45/keyboard-mouse is the retained experimental client
profile. Foreground checks refused the latest capture/input attempt. Old Python
screenshots and sanitized packet captures are not new DLL qualification. No
Android/touch, controller or multiplayer result is marked passed.

## Remaining required work

All 69 original entries, aliases, source contracts and permissions are retained
in the immutable scope and separate Windows/Linux reports. **Retention is 69;
completed custom native entries are zero.** Eleven Windows original-mode paths
are experimental, with zero newly client-qualified entries. Linux UI adapters
remain unavailable. The per-entry record is in
[the capability matrix](CAPABILITY_MATRIX.md) and
[the complete migration table](ALL_UI_MIGRATION.md).

Required implementation still includes the remaining original families and
custom merchant, machine, map-printing, editable/persistent storage, workshop,
equipment/cargo, editor/dialogue, chemistry, held-container and correct-role
services. Protected inventory menus, pagination, native item publication,
durable held identity and the remaining standalone SDK consumers are unfinished.
Configuration migration, complete SDK/CMake package exports and final release
packaging also need work.

The reservation and journal tests prove their isolated model contracts. They
do **not** prove exactly-once delivery or crash recovery across BDS/plugin saves.
Native writes, valuable-item reconciliation and the journal must be connected,
then tested against actual server saves/crashes without overwriting uncertain
items or issuing replacements.

The local host has no Docker CLI/Desktop and WSL is not installed. No system
components were installed and no virtualization settings changed. An accessible
Linux Docker engine plus exact authorized Linux runtime inputs are needed for
local ABI/runtime work. Exact Onistone SDK/runtime inputs remain missing.
Minecraft must be available in the foreground to finish joining the isolated
server and run the current-artifact SDK/client procedures. These prerequisites
block runtime qualification; they do not mean the remaining implementation is
already finished.
