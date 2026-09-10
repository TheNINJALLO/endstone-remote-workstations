# Onistone Virtual Container Framework

Native C++20 container framework development for Linux x86-64 and Windows x64.
**The all-UI migration is in progress and is not release-qualified.**

Native builds produce a Windows DLL and a Linux ELF `.so`, a C ABI/C++ SDK and independent native consumers. Windows release/debug and the pinned Linux Docker build pass in CI. The isolated Windows Endstone 0.11.10 server loaded the earlier `171e0aa` provider and both consumer DLLs and verified the Windows function manifest; later CI DLLs still need local runtime qualification. The project plugin has no Python runtime dependency.

All 69 original catalog entries, aliases, permissions and source contracts are preserved in an immutable migration baseline. Catalog retention is distinct from native/custom gameplay implementation; the full-scope acceptance gate currently fails. Local Docker Desktop builds and runs the exact Linux Endstone/BDS fixture. A stock Windows client has tested SDK forms, four shared inventory roles and eight opt-in native workstation contexts: crafting, anvil, smithing, stonecutter, grindstone, loom, cartography and enchanting. Three nearby linked machinesâ€”furnace, blast furnace and smokerâ€”also have processing, delivery and cleanup smoke evidence. Nearby linked Ender Chest and barrel access have item-transfer, selected metadata and cleanup evidence. Five more linked utilities have selected PC evidence: dispenser/dropper transfers, brewing output, beacon payment/effect and crafter toggles/redstone output. The later `d6adf87` build passed repeated beacon/crafter SDK closure and selected item/control restoration; initial unaccepted click attempts remain recorded. A separate linked hopper path has first/last-slot transfer, metadata and selected cleanup evidence. Linked chest, trapped-chest and double-chest paths also passed selected transfers, metadata, close/reopen and paired-source protection checks. Selected vanilla transformations, equipment, recipe selection, a compass trigger and controlled disconnect cleanup have smoke evidence. An initial grindstone output rejection remains unresolved. Custom inventory writes, remaining catalog adapters and recovery remain incomplete. Exact Onistone runtime inputs remain unavailable.

- [Migration audit and remaining work](docs/MIGRATION_AUDIT.md)
- [Current implementation report, tested artifacts and exact remaining scope](docs/NATIVE_IMPLEMENTATION_REPORT.md)
- [Native build and test instructions](docs/NATIVE_BUILDING.md)
- [Windows Docker/WSL setup and private Linux inputs](docs/WINDOWS_DOCKER_SETUP.md)
- [Linux loaded-runtime identities and client evidence](docs/NATIVE_LINUX_RUNTIME.md)
- [Run the Linux SDK form example, with actual client screenshots](docs/examples/linux-native-sdk.md)
- [Open the real inventory and bind a held-item trigger on Linux](docs/examples/linux-native-inventory.md)
- [Use eight Linux native workstation contexts through the SDK, with actual screenshots](docs/examples/linux-native-workstations.md)
- [Open authorized Linux furnace-family sources, with processing and cleanup screenshots](docs/examples/linux-linked-furnaces.md)
- [Access the real Ender inventory and linked barrel through the SDK](docs/examples/linux-linked-storage.md)
- [Use linked utilities, with actual screenshots and close/restore test history](docs/examples/linux-linked-utilities.md)
- [Use the native linked hopper with retained items and metadata](docs/examples/linux-linked-hopper.md)
- [Open linked chests with protection checks for both halves](docs/examples/linux-linked-chests.md)
- [Inspect held shulkers and books through SDK 1.2, with the bundle limitation recorded](docs/NATIVE_HELD_ITEMS.md)
- [Read native saved-item data through SDK 1.3, including bundle contents on Linux](docs/NATIVE_ITEM_SAVE.md)
- [Detect changes across SDK calls with inventory observations in SDK 1.4](docs/NATIVE_ITEM_OBSERVATIONS.md)
- [Develop guarded inventory edits with SDK 1.5 and explicit recovery review](docs/NATIVE_INVENTORY_WRITES.md)
- [Native SDK and compiled consumer](docs/NATIVE_SDK.md)
- [Native storage transactions and current integration boundary](docs/NATIVE_STORAGE.md)
- [NativePassthrough SDK and player-interaction example](examples/native/NativePassthrough.md)
- [All-entry migration](docs/ALL_UI_MIGRATION.md)
- [Capability matrix](docs/CAPABILITY_MATRIX.md)
- [All-entry examples and gaps](docs/ALL_UI_EXAMPLES.md)
- [Frozen original catalog](research/original-ui-catalog.json)
- [Interactive qualification records and release gate](docs/NATIVE_QUALIFICATION_RECORDS.md)

Implemented core work includes staged item transfers, exact metadata policies, recipe/stock revisions, bounded processing, deferred permission-checked actions, consumer-owned session lifetimes and a durable uncertainty journal. SDK 1.5 adds an experimental Linux inventory writer with complete native item reconstruction, guarded batch publication and explicit restart review. Selected live tests preserve filled-bundle contents and named items. Full held editors, screen customization, Windows writer parity, map integration and automatic recovery across BDS and plugin saves remain incomplete. See the [inventory writer guide](docs/NATIVE_INVENTORY_WRITES.md) for the tested boundaries.

An opt-in Windows adapter now orchestrates seven original workstations and the four shared player-inventory entry points in C++. Its asynchronous lifecycle waits for an ordered client handshake and restores client projections from current world state. It is disabled by default while this artifact's client tests proceed; it does not implement custom recipes or replace real inventory contents.

The published [RemoteWorkstations 0.4.0 release](https://github.com/TheNINJALLO/endstone-remote-workstations/releases/tag/v0.4.0) remains unchanged. Its [archived README](research/original-source/README.md), screenshots and validation describe the legacy Python/native-companion implementation, not these DLLs. Do not install both inventory owners together. No new release or tag is created by this migration branch.

Original code is MIT. See LICENSE, NOTICE and the vendored dependency licenses.
