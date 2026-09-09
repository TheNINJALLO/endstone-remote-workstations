# Native migration audit — September 8, 2026

This branch is an implementation in progress, **not the completed all-UI framework**.
The published RemoteWorkstations 0.4.0 release remains a separate legacy product.
Do not install its wheel beside the new provider.

The baseline was frozen before native catalog creation from commit
`e96529cc26b204d174581714d87f205a7de5270b` plus local held-item API 1.7 work.
It retains the requested reference `a2a856a8520cdeda255ae12d61679d9327038eec`.
The [baseline](../research/original-ui-catalog.json) contains 69 canonical entries,
aliases, command strings, permissions, original source contracts, configuration,
API types/methods and SHA-256 hashes of 138 source files. Raw copies are under
`research/original-source/`. Its SHA-256 is
`33242db49532f56835d955307491c2aff5d956ac990e0591697a500e6c6c0342`.
Do not regenerate it from the C++ catalog.

## Inspected ownership and migration

| Existing implementation | Native outcome |
|---|---|
| Python catalog/configuration/command registration | Frozen and retained; C++ command names and permission defaults resolve through the native catalog. Configurable legacy alias overrides and repeatable configuration conversion remain unfinished. |
| Python UIService/forms/action registry | C++ consumer registry, deferred actions, exported/private actions, detached action listings, scoped guards, permissions, opening tickets, owned form callbacks, revocation and bounded queues implemented. Full transition/client qualification and richer refusal details remain unfinished. |
| Protected inventory menus / packet backend | Preserve as a separate mode; C++ native inventory rendering and packet executor integration pending. A catalog ActionForm is not its replacement. |
| Original Windows native companion | The existing C++ primitives, exact entry-point hashes, per-instance source checks, editor gates and held save guard compile into the real DLL without pybind11. Their loaded manifest has passed in the isolated server. Most primitives are not connected to the new session owner yet. This is not gameplay qualification. |
| Python item/transaction models | New C++ staged snapshot executor validates complete move batches, policies, exact metadata, capacity and recipe/stock revisions. The storage core now decodes captured requests/responses and validates network identities, replay envelopes and cursor reservations. Native inventory publication, custom result extraction and durable binding integration remain unfinished. |
| Processing logic | A bounded C++ due-queue executes plugin-defined transforms, output capacity and stock. Native furnace/brewing properties, viewers, persistence integration and custom client transactions remain unfinished. |
| SQLite recovery prototypes | New append-only native journal uses checksums, sequence/phase validation and OS durability calls. Uncertain outcomes quarantine; no item issuance. BDS save reconciliation is not implemented and exactly-once delivery is not claimed. |
| Held-item development | All source additions preserved. The full-NBT codec/save-guard primitives compile in the new DLL, but item identity leasing, shulker/bundle sessions and new-artifact crash qualification remain unfinished. |
| Research/build Python scripts | Development only. The CMake plugin and native SDK do not import or link them. Endstone's own Python launcher is outside this plugin's runtime dependency boundary. |
| Release docs/tests/images | Historical evidence remains in the raw baseline and existing legacy docs. None of the earlier 53-entry lifecycle passes or screenshots counts as testing this C++ artifact. |

## Build and runtime evidence

The current [implementation checkpoint](NATIVE_IMPLEMENTATION_REPORT.md)
records source `0a91902`, the exact locally loaded Linux ELF, the newly built
Windows DLL, 13 Linux and 12 Windows native test jobs, three 100,000-input
sanitizer fuzz targets and the remaining all-UI scope. Linux client smoke now
covers SDK form actions, close/disconnect cleanup and passive inventory
observation. Older runs below retain their original provenance.
The new item descriptor/registry observer is passive; it does not complete
native inventory publication or qualify a new custom UI.

The Windows build uses C++20, CMake 3.31.6, MSVC 19.44.35222.0,
toolset 14.44.35207, dynamic MSVC runtime and iterator ABI level 0.
The exact 191 public SDK input files are checked by CMake.
The SDK revision is Endstone `8f84d6f5b556916597ed5b6b71329b2ed3ca8fc8`
(version 0.11.10); public API family remains 0.11.
Using the header-only API does not imply compatibility with every runtime in that family.

A fresh isolated world on port 29169 loaded
`endstone_onistone_vcf.dll` with no old project wheel or Python companion.
A separate `endstone_vcf_catalog_showcase.dll` discovered the provider's shadow
module, negotiated C ABI 1.0, registered its owner and resolved all 69 entries.
Current artifact fingerprints and separate outcome records are in
[Windows evidence](../research/native-evidence/windows-x64.json).

BDS SHA-256:
`92d09c7b74ac6a9805bafc166d8e0a13ac9e5db73dbbb0819e5a14093699d44f`.
Endstone runtime SHA-256:
`0c6f0861c5f9a677058b25776d975654a3586a80d2e4421b069b5e98f536f819`.

At the initial audit, Docker CLI/Desktop and WSL were not installed. Following
explicit installation authorization on September 8, Docker Desktop 4.90.0 and
WSL 2.7.13 were installed and Virtual Machine Platform was enabled without
restarting Windows. The operator subsequently restarted and completed setup.
The Linux engine, local pinned builds, and private BDS/Endstone server now run.
Exact loaded-file admission enables the public SDK; both native consumers load
and a connected stock client has exercised its action form. Private Linux
workstation adapters and inventory publication remain incomplete.
See [the setup checkpoint](WINDOWS_DOCKER_SETUP.md).
The Linux image digest and Clang 20 family are pinned.
The Docker build previously passed on GitHub's Linux runner in
run 34273970612, exporting a real `.so` and ELF dependency report; Windows
release/debug jobs passed separately. The full-scope acceptance job correctly
failed for unqualified per-entry gameplay. Local Linux public SDK progress is
recorded in [NATIVE_LINUX_RUNTIME.md](NATIVE_LINUX_RUNTIME.md). An exact Onistone SDK/runtime was not supplied;
no compatibility is inferred from Endstone's provenance.

Subsequent CI run 34279270106 passed both Windows configurations, the Linux
Docker build, all ten then-current tests under ASan/UBSan, and 100,000 NBT fuzz
inputs using the rebuilt libc++ libFuzzer. The storage/lease checkpoint expands
the suite to eleven jobs: 50,181 core checks and 10,309 storage checks,
plus separate lifecycle, SDK, ABI and crash-boundary tests. CI run 34282759521
then passed both Windows builds, Linux Docker, ASan/UBSan, and 100,000 inputs
each for NBT and storage fuzzing at source revision `404aa913`. The full-scope
gate still correctly fails all 138 canonical/platform and 20 additional
surface/platform qualification outcomes. Detailed implementation
boundaries are in [NATIVE_STORAGE.md](NATIVE_STORAGE.md).

An experimental C++ session adapter now owns the seven original workstation
contexts and four shared player-inventory entry points. It waits for the ordered
projection handshake, observes the actual native open, and retains its lease
through asynchronous close. BDS still owns original recipes and inventory.
These paths require an opt-in configuration flag and Windows 1.26.45 clients;
none counts as a custom implementation. Native tests now include cancellation
before dispatch, concurrent pending opens, permission revocation during opening,
stale completion and consumer disable during a callback.

Windows Minecraft foreground input resumed temporarily but was interrupted
while adding the isolated connection. All new-artifact screen interaction
records remain untested. Additional devices and a second
client are unavailable; prior user instructions waived acquiring those devices,
which does not turn their records into passes.

## MapDisplays investigation

Inspected [MapDisplays at 6dd5f242](https://github.com/TheNINJALLO/endstone-mapdisplays/tree/6dd5f24233600ad9376c7a5faaee350aa31ee623).
Its documented baseline is Endstone 0.11.9 / BDS 1.26.44, distinct from this project.
It creates maps through Endstone, records map IDs and image sources in
`data/displays.json`, attaches a static renderer to a MapView, sends map data,
and reloads saved image sources on startup. Its renderer removes itself after
drawing while relying on the MapView's cached pixels.

These are actual Endstone map identities with plugin-restored rendering, not a
documented external native provider API. Held/framed persistence cannot be
certified from that code inspection. Its current image loading and asynchronous
publication do not provide the new task's bounds, cancellation generation or
transactional durability contract. The native MapProvider boundary and cartography
executor remain implementation work; the separate repository and its live data
were not modified.

## Licensing

Project code remains MIT. Endstone's public SDK is Apache-2.0; expected-lite is
BSL-1.0. The port retains existing source comments and research provenance.
Vendored MinHook 1.3.4 retains its BSD license; nlohmann/json 3.12.0 retains MIT.
MapDisplays is Unlicense and is presently a research reference, not a runtime
dependency. Private server executables, PDBs, captures, worlds and account/item
snapshots are excluded from source control and Docker's public context.

[All-entry migration](ALL_UI_MIGRATION.md) ·
[Capability matrix](CAPABILITY_MATRIX.md) ·
[Examples and missing scenarios](ALL_UI_EXAMPLES.md)
