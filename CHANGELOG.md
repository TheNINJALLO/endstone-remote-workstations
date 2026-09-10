# Changelog

## 0.5.0-native.1 — experimental native prerelease

- Native Linux `.so` and Windows `.dll`, C ABI/SDK 1.5 and separate compiled consumers.
- Experimental Linux held shulker editing with complete saved-item metadata, durable identity, guarded cursor transfers and COMMIT callbacks.
- Native inventory writes, explicit restart review, six BDS crash-boundary records and expanded sanitizer/fuzz tests.
- Versioned installation/how-to documentation, screenshots, wiki pages, platform archives and checksums.
- Full catalog/custom behavior, Windows writer parity and automatic cross-save recovery remain incomplete. See [release limits](docs/RELEASE_NATIVE_0_5_0.md).

## Historical Python 0.5.0.dev1 (unreleased)

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
