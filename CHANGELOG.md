# Changelog

## 0.5.0.dev1 (unreleased)

- Dependency API 1.6 adds read-only inspection of the actual held item.
- Typed, bounded item snapshots preserve complete public NBT for detached copies.
- Exclusive shulker reservations and revisioned write intents extend the journal;
  ambiguous close/restart records quarantine without automatic item issuance.
- Source/API, serialization, journal and subprocess-crash tests cover this first
  development slice. Native identity assignment, movement locks, nesting,
  writeback and interactive held-item UIs remain unfinished and disabled.

See [held-item development](docs/HELD_ITEMS.md).

## 0.4.0

First regular release of the implemented RemoteWorkstations feature set.

- 53 implemented catalog entry points with explicit source/admission contracts.
- Dependency API 1.5: native and source-based screens, NPC dialogue, real online
  Ender Chest access, forms, protected inventory menus and exported callbacks.
- Windows native companion pinned to Endstone 0.11.10 / BDS 1.26.45.1.
- Scoped mounted-entity presentation for merchant/equipment screens.
- Windows native and portable Python wheels; separate runnable consumer example.
- Illustrated README, examples, installation guide, wiki and support matrix.

This release promotes the final a24 implementation without claiming new
held-item, crash-recovery or Linux native support. Gameplay code and native
binary equivalence are checked separately from release metadata. Historical
tests retain their original wheel identity. [Validation](docs/VALIDATION.md)
and [known limitations](docs/SUPPORT.md).
