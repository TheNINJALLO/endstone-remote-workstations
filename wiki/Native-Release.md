# v0.5.0-native.1

This experimental native prerelease is distributed alongside stable RemoteWorkstations v0.4.0. It is not the completed all-UI framework or a drop-in replacement for the Python API.

## Downloads

The [release page](https://github.com/TheNINJALLO/endstone-remote-workstations/releases/tag/v0.5.0-native.1) provides Linux x86-64 and Windows x64 provider packages, separate example/SDK packages, development symbols and SHA-256 checksums. Manifests record source revision, platform, file hashes and qualification. BDS, private worlds, credentials, runtime libraries and raw research captures are excluded.

Provider/example version: `0.5.0-native.1`. Relocatable SDK version: `1.5.0`, matching C ABI 1.5. [Installation](https://github.com/TheNINJALLO/endstone-remote-workstations/wiki/Native-Installation) covers dependencies and flags.

The tested Linux provider retains its own debug information; the separate symbols archive also contains extracted `.debug` files. Windows symbols are PDBs. This preserves the exact tested provider bytes. Build Debug configurations using the documented presets; compiled Windows Debug artifacts are retained in CI.

## Implementation

The shared C++ core provides the frozen catalog, asynchronous sessions, guards, forms/actions, bounded item/packet codecs and durable uncertainty journals. Linux adds selected original contexts, linked sources, complete native item reads/writes and experimental held shulker editing. Windows uses independent ABI admission and typed refusals for unimplemented adapters.

Held storage adds complete saved-item modeling, native descriptor serialization, durable identity/source locks, cursor staging, COMMIT callbacks, projection cleanup and the opening-input handoff.

## Qualification

Package manifests record build and runtime evidence separately. Historical per-UI tests retain their artifact hashes. The full-scope gate remains **not qualified** for all 69 catalog outcomes on both platforms. No baseline entry, platform or extra API surface is removed to publish this prerelease.

The Linux runtime is pinned to Endstone 0.11.10/BDS 1.26.45.1 with the documented Windows PC client profile. Windows packages have build coverage and do not inherit Linux gameplay checks. Other Onistone runtimes, Android/touch, controllers and multiplayer are untested.

Remaining work includes programmable inventory behavior across the catalog, merchants, machines, maps, equipment/editors/chemistry, bundle editing, Windows writer parity and automatic BDS/plugin save reconciliation. The original grindstone result issue remains recorded.

## Upgrade and recovery

Use a separate instance from v0.4.0 and preserve world/plugin backups; no automatic legacy storage migration is provided. Inventory adapters and writes default to disabled. Review surviving edits after restart. Do not delete the journal to clear quarantine. [Recovery contract](https://github.com/TheNINJALLO/endstone-remote-workstations/wiki/Native-Recovery).


**Known prerelease issue:** the held-shulker sneak-and-use binding currently refuses an equipment synchronization packet during preparation. Use the tested `/vcf_native shulker` command. No inventory transfer is performed by the refused opening.


[Exact artifact hashes and release smoke record](https://github.com/TheNINJALLO/endstone-remote-workstations/blob/v0.5.0-native.1/research/native-release-0.5.0-native.1.json). Linux and sanitizer builds passed 23 tests; Windows Release/Debug passed 21 each. Each seeded fuzz target completed 100,000 runs. Linux provider and both consumers loaded with matching exported/installed/mapped hashes. The same provider completed the documented shulker command/transfer/cancel/reopen checks.
