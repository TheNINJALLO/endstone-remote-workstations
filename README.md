# Onistone Virtual Container Framework

Native C++20 container framework development for Linux x86-64 and Windows x64.
**The all-UI migration is in progress and is not release-qualified.**

The Windows build produces a real Endstone plugin DLL, a C ABI/C++ SDK, debugging symbols and an independent native consumer. The isolated Endstone 0.11.10 server has loaded both DLLs and verified the reused Windows function manifest. The project plugin has no Python runtime dependency.

All 69 original catalog entries, aliases, permissions and source contracts are preserved in an immutable migration baseline. Catalog retention is distinct from native/custom gameplay implementation; the full-scope acceptance gate currently fails. Linux Docker execution and exact Onistone runtime inputs remain unavailable on the current development host.

- [Migration audit and remaining work](docs/MIGRATION_AUDIT.md)
- [Native build and test instructions](docs/NATIVE_BUILDING.md)
- [Native SDK and compiled consumer](docs/NATIVE_SDK.md)
- [All-entry migration](docs/ALL_UI_MIGRATION.md)
- [Capability matrix](docs/CAPABILITY_MATRIX.md)
- [All-entry examples and gaps](docs/ALL_UI_EXAMPLES.md)
- [Frozen original catalog](research/original-ui-catalog.json)

Implemented core work includes staged item transfers, exact metadata policies, recipe/stock revisions, bounded processing, deferred permission-checked actions, consumer-owned session lifetimes and a durable uncertainty journal. Native inventory publication, screen customization, full SDK parity, map integration and cross-save recovery qualification remain incomplete.

The published [RemoteWorkstations 0.4.0 release](https://github.com/TheNINJALLO/endstone-remote-workstations/releases/tag/v0.4.0) remains unchanged. Its [archived README](research/original-source/README.md), screenshots and validation describe the legacy Python/native-companion implementation, not these DLLs. Do not install both inventory owners together. No new release or tag is created by this migration branch.

Original code is MIT. See LICENSE, NOTICE and the vendored dependency licenses.
