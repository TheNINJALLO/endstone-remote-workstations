# Onistone Virtual Container Framework

Native C++20 container framework development for Linux x86-64 and Windows x64.
**The all-UI migration is in progress and is not release-qualified.**

Native builds produce a Windows DLL and a Linux ELF `.so`, a C ABI/C++ SDK and independent native consumers. Windows release/debug and the pinned Linux Docker build pass in CI. The isolated Windows Endstone 0.11.10 server has loaded the current provider and both consumer DLLs and verified the Windows function manifest. The project plugin has no Python runtime dependency.

All 69 original catalog entries, aliases, permissions and source contracts are preserved in an immutable migration baseline. Catalog retention is distinct from native/custom gameplay implementation; the full-scope acceptance gate currently fails. Local Docker Desktop builds and runs the exact Linux Endstone/BDS fixture. A stock Windows client has tested SDK forms, four shared inventory roles and eight opt-in native workstation contexts: crafting, anvil, smithing, stonecutter, grindstone, loom, cartography and enchanting. Three nearby linked machines—furnace, blast furnace and smoker—also have processing, delivery and cleanup smoke evidence. Selected vanilla transformations, equipment, recipe selection, a compass trigger and controlled disconnect cleanup have smoke evidence. An initial grindstone output rejection remains unresolved. Custom inventory writes, remaining catalog adapters and recovery remain incomplete. Exact Onistone runtime inputs remain unavailable.

- [Migration audit and remaining work](docs/MIGRATION_AUDIT.md)
- [Current implementation report, tested artifacts and exact remaining scope](docs/NATIVE_IMPLEMENTATION_REPORT.md)
- [Native build and test instructions](docs/NATIVE_BUILDING.md)
- [Windows Docker/WSL setup and private Linux inputs](docs/WINDOWS_DOCKER_SETUP.md)
- [Linux loaded-runtime identities and client evidence](docs/NATIVE_LINUX_RUNTIME.md)
- [Run the Linux SDK form example, with actual client screenshots](docs/examples/linux-native-sdk.md)
- [Open the real inventory and bind a held-item trigger on Linux](docs/examples/linux-native-inventory.md)
- [Use eight Linux native workstation contexts through the SDK, with actual screenshots](docs/examples/linux-native-workstations.md)
- [Open authorized Linux furnace-family sources, with processing and cleanup screenshots](docs/examples/linux-linked-furnaces.md)
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
