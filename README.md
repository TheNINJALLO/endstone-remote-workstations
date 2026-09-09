# Onistone Virtual Container Framework

Native C++20 container framework development for Linux x86-64 and Windows x64.
**The all-UI migration is in progress and is not release-qualified.**

Native builds produce a Windows DLL and a Linux ELF `.so`, a C ABI/C++ SDK and an independent native consumer. Windows release/debug and the pinned Linux Docker build pass in CI. The isolated Windows Endstone 0.11.10 server has loaded both DLLs and verified the reused Windows function manifest. The project plugin has no Python runtime dependency.

All 69 original catalog entries, aliases, permissions and source contracts are preserved in an immutable migration baseline. Catalog retention is distinct from native/custom gameplay implementation; the full-scope acceptance gate currently fails. Local Docker Desktop now builds and runs the exact Linux Endstone/BDS fixture. Both native consumers load, and a stock Windows client has rendered the SDK action form and executed its callback. Linux private workstation adapters and native inventory writes remain incomplete. Exact Onistone runtime inputs remain unavailable.

- [Migration audit and remaining work](docs/MIGRATION_AUDIT.md)
- [Current implementation report, tested artifacts and exact remaining scope](docs/NATIVE_IMPLEMENTATION_REPORT.md)
- [Native build and test instructions](docs/NATIVE_BUILDING.md)
- [Windows Docker/WSL setup and private Linux inputs](docs/WINDOWS_DOCKER_SETUP.md)
- [Linux loaded-runtime identities and client evidence](docs/NATIVE_LINUX_RUNTIME.md)
- [Native SDK and compiled consumer](docs/NATIVE_SDK.md)
- [Native storage transactions and current integration boundary](docs/NATIVE_STORAGE.md)
- [NativePassthrough SDK and player-interaction example](examples/native/NativePassthrough.md)
- [All-entry migration](docs/ALL_UI_MIGRATION.md)
- [Capability matrix](docs/CAPABILITY_MATRIX.md)
- [All-entry examples and gaps](docs/ALL_UI_EXAMPLES.md)
- [Frozen original catalog](research/original-ui-catalog.json)
- [Interactive qualification records and release gate](docs/NATIVE_QUALIFICATION_RECORDS.md)

Implemented core work includes staged item transfers, exact metadata policies, recipe/stock revisions, bounded processing, deferred permission-checked actions, consumer-owned session lifetimes and a durable uncertainty journal. Native inventory publication, screen customization, full SDK parity, map integration and cross-save recovery qualification remain incomplete.

An opt-in Windows adapter now orchestrates seven original workstations and the four shared player-inventory entry points in C++. Its asynchronous lifecycle waits for an ordered client handshake and restores client projections from current world state. It is disabled by default while this artifact's client tests proceed; it does not implement custom recipes or replace real inventory contents.

The published [RemoteWorkstations 0.4.0 release](https://github.com/TheNINJALLO/endstone-remote-workstations/releases/tag/v0.4.0) remains unchanged. Its [archived README](research/original-source/README.md), screenshots and validation describe the legacy Python/native-companion implementation, not these DLLs. Do not install both inventory owners together. No new release or tag is created by this migration branch.

Original code is MIT. See LICENSE, NOTICE and the vendored dependency licenses.
