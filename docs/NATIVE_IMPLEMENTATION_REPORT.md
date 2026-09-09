# Native implementation checkpoint — incomplete

The native source checkpoint is `7aacb361a66eb5b927b692899fe3eccd8da43049` on
`feature/native-virtual-container-framework`. **The all-UI migration remains
incomplete and is not a production-qualified release.** RemoteWorkstations
0.4.0 and `main` remain separate from this development work.

## What now runs locally

Docker Desktop 4.90.0 and WSL 2.7.13 now provide a working Linux x86-64 engine
on the Windows development PC. The pinned Clang 20.1.8 image builds the real
native `.so`, runs its tests, inspects the supplied Linux ELF files, and runs
the isolated Endstone 0.11.10 / BDS 1.26.45.1 server on `127.0.0.1:29179`.
The operator explicitly confirmed the supplied server's license before startup.

The exact Linux provider and both independent SDK consumers load and enable.
A stock Windows Bedrock 1.26.45 client joins the server. The public SDK action
form renders and dispatches the consumer's callback. All 69 catalog IDs resolve.
The live inventory observer now reports one registry, one complete snapshot,
zero queued packets and zero refusals. It does not write native inventories.

The opt-in Linux original inventory adapter now opens `inventory2x2`, `armor`,
`offhand` and `recipebook` through independently derived and fingerprinted Linux
ABI facts. It also admits seven original workstations: crafting, anvil, smithing,
stonecutter, grindstone, loom and cartography. The earlier `ca30255` build completed
15 opens and 15 closes with zero failure callbacks and outstanding tickets.
PC tests exercised vanilla transformations, equipment, recipe selection,
SDK cancellation and a held-compass interaction binding. The client/BDS retain
real item ownership. Custom workstation logic remains incomplete.

On Peaceful difficulty, six unused crafting-grid planks and four crafted sticks
survived disconnect/reconnect in `ca30255`. An unfinished stonecutter input also
returned on reconnect. Grindstone's first plain-click extraction did not deliver
its preview; Shift-click and later plain-click repeats succeeded on both test
swords. The original rejection remains unresolved. See the
[workstation example](examples/linux-native-workstations.md) for exact observations.
The older `171e0aa` [inventory example](examples/linux-native-inventory.md) retains
its separate missing-plank observation followed by death and subsequent controlled
passes. Neither checkpoint establishes complete death/drop or crash recovery.

The exact `7aacb36` crafting build passed a 3×3 wooden-pickaxe recipe with
ordinary-click and Shift-click output, unused ingredients returned on SDK
closure, compass opening, ingredient disconnect/reconnect and `/workbench`.
Stonecutter, shared inventory and the form action also passed regression checks.
Four example opens/closes plus the provider-owned alias ticket returned to zero
provider sessions, with a clean server shutdown. The
[crafting record](../research/native-evidence/linux-7aacb36-crafting-smoke.json)
identifies this run separately from the earlier six-workstation results.

Live testing exposed and fixed two SDK issues: Linux cross-module player
conversion now uses Endstone's virtual `asPlayer()` method, and a selected menu
action completes the caller's original ticket instead of leaking a hidden
child ticket. The example consumer collects terminal tickets on its scheduler.
Both platform providers retain their callback code for process lifetime; each
enable operation gets a fresh revocable callback token.

The actual Linux inventory packet uses a zero-initialized container name for
window 0. A new exact empty-inventory regression preserves this observed shape
while rejecting unrelated roles, dynamic containers and wrong slot counts.
The bounded private packet probe used for diagnosis was removed before the
final artifact's runtime test. Private packets and account identifiers are not
published.

## Builds and checks

- Previous local Windows build (`171e0aa`): **12/12 CTest jobs passed**, DLL, SDK, consumers and PDB
  exported. The exact current provider and both consumer DLLs loaded in the
  isolated Windows server, verified their hashes and primitive manifest, and
  shut down cleanly. This Windows build has not had stock-client UI tests.
- Local Linux Docker build: **13/13 CTest jobs passed**, ELF, SDK and consumers
  exported. The extra Linux test covers loaded-file hashes, replaced inodes,
  unknown runtimes and retained callback code after `dlclose`.
- Linux ASan/UBSan at `7aacb36`: its CI job passed **13/13 tests** and 100,000 libFuzzer inputs each for NBT,
  storage and item-wire parsing, **300,000 total**.
- Model checks: 50,181 core, 10,309 storage, 21,813 item-wire checks; 5,000
  menu/select/forget cycles, duplicate selection, revoked permission, pending
  cancellation, action failure, and explicit action-ticket collection.
- Six standalone journal crash-boundary tests passed. These do not establish
  recovery across BDS/player saves.
- Immutable scope and five gate regression tests pass. All 138 canonical/platform
  and 20 additional surface/platform full-qualification outcomes remain
  unqualified; working SDK smoke tests cannot override this gate.

Build logs, module hashes and exact dependency evidence are indexed in
[`checkpoint-7aacb36.json`](../research/native-evidence/checkpoint-7aacb36.json).
Older checkpoints keep their original source and artifact identities.

At `7aacb36`, [native CI run 34307955147](https://github.com/TheNINJALLO/endstone-remote-workstations/actions/runs/34307955147)
passed Linux Docker, Windows Debug/Release and Linux sanitizer jobs. Its overall
result is **failure** because the separate full-scope acceptance gate correctly
refuses the incomplete catalog. [Legacy regression run 34307955129](https://github.com/TheNINJALLO/endstone-remote-workstations/actions/runs/34307955129)
passed on both Windows and Linux. Current local client observations and
screenshots are in the [Linux workstation example](examples/linux-native-workstations.md).
All three Linux CI plugin hashes match the locally tested exports. The Windows
CI binaries were built and tested in CI but have not had local client tests;
the separate Windows startup record still identifies the previous local build.
Sanitizers cover core/model/parser and runtime-identity tests, not an instrumented
proprietary BDS UI process.

## Exact development artifacts

| Target | Export | SHA-256 |
|---|---|---|
| Linux x64 | `dist/linux-x64-dev/plugins/endstone_onistone_vcf.so` | `ccba81331609e9ae98f451840136d3c22ce6334987c4e7a0afd7a75476fc13d7` |
| Windows x64 | `dist/windows-release/plugins/endstone_onistone_vcf.dll` | `07b037e42a832571448022fdea6385b2db08e0839a9e5b611965aeac500e3dfb` |

Paths are relative to the native checkout. The loaded Linux shadow copy and
installed provider match the Linux export byte-for-byte. Linux debug information
is still embedded in this development ELF; Windows PDBs are under `symbols/`.
These are development artifacts, not completed all-UI release packages.

The [Windows startup record](../research/native-evidence/windows-171e0aa-startup-smoke.json)
also verifies the retained two-key configuration and the loaded shadow-copy
identities. Startup evidence is separate from native gameplay qualification.

## Compatibility boundaries

The pinned Endstone SDK is 0.11.10 at
`8f84d6f5b556916597ed5b6b71329b2ed3ca8fc8`. Public API family 0.11 does not
admit other native runtimes. Exact Linux BDS/loader hashes and ELF Build IDs
are in [NATIVE_LINUX_RUNTIME.md](NATIVE_LINUX_RUNTIME.md). No Onistone runtime
has been supplied or qualified.

The Linux provider directly needs `libcrypto.so.3`, libc++20, libc++abi,
libunwind and ordinary C libraries. Its current direct glibc symbol requirement
reaches **GLIBC_2.34**; older GLIBC_2.14 reports apply only to older artifacts.
The tested fixture uses glibc 2.36 and the loader's bundled libc++ family.
Process maps confirm one libc++ family. Endstone's own NumPy/frozenlist modules
also load libstdc++; the provider and native consumers do not link against it.
This is not certification of arbitrary Pterodactyl images.

The project plugin and consumers are native C++, with no project Python wheel,
pybind runtime or Python companion. Endstone's own loader remains outside that
boundary. Test scripts are development tools only.

## Remaining required scope

All 69 original entries, aliases, permissions and source contracts remain in
the frozen baseline and platform reports. **No custom native catalog entry is
fully qualified.** Eleven Windows original-mode paths have experimental
orchestration behind an opt-in flag. Seven Linux workstations and four shared
inventory roles have experimental original adapters; other Linux catalog
adapters remain incomplete.
The current form is an SDK action demonstration; it is not a workstation.

Required work still includes original and custom merchant, machine, map-printing,
editable/persistent storage, workshop, equipment/cargo, editor/dialogue,
chemistry, held-container and correct-role services. Protected inventory menus,
pagination, native item publication, durable held identity, cross-save recovery,
remaining SDK consumers, configuration migration, CMake SDK package exports
and final release packaging remain incomplete. The native transaction model
and journal must be connected to BDS writes and qualified at real save/crash
boundaries before claiming durable delivery.

Client tests use Windows PC keyboard/mouse. Additional devices and multiplayer
are untested; no second client is required from the operator for this checkpoint.
See [the per-entry matrix](CAPABILITY_MATRIX.md) and
[all-UI migration](ALL_UI_MIGRATION.md) for the retained scope.
