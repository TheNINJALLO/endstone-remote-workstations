> **Legacy v0.4.0 documentation.** This page describes the Python/native-companion implementation. For the native prerelease, use [installation](NATIVE_INSTALLATION.md), [examples](NATIVE_EXAMPLES.md) and [SDK 1.5](NATIVE_SDK.md).

# Backend decisions

Seven Windows workstation contexts now use a real CPython/pybind11 companion in Endstone's
existing interpreter. Both loaded modules and each called entry point are checked.
See [native evidence](NATIVE_WINDOWS.md).

| Approach | Evidence | Decision |
|---|---|---|
| Simulated inventories | Offline atomic batches, codecs, replay protection and quarantine | Storage prototype only. Live stack identities, exclusive request ownership, cursor and BDS-save reconciliation remain unbound. Vanilla recipes are not simulated. |
| Native managed virtual containers | Real input/output and close tests for seven interfaces; anvil XP and rejection | Selected. BDS owns the actual containers and transactions. Remaining recipe/client/lifecycle/recovery coverage is explicit. |
| Isolated real backing stations | Real crafting/anvil comparison blocks | No production backend yet. Ticking regions, distance, reservations and cleanup remain unresolved. No covert teleports or real blocks placed near players. |

The production companion uses no gameplay hooks and retains no native player, item or model
pointers. Six stations call exact BDS factories; crafting establishes the native workbench
stack context. A packet-only crafting screen rejected real recipe requests in testing.

## Ender Chest and storage

The exact Windows generic container lookup for block-actor type 23 reads the player's actual
Ender Chest at native Player+0x810. Base usability still requires a matching nearby real block
actor. A managed virtual usability solution is required before exposing remote Ender Chest.
No replacement vault or stale closing snapshot is used.

Held shulkers need durable item identity, full NBT, movement/drop/transfer locks, nesting
restrictions and crash-safe reconciliation. The plugin never removes or rewrites the held box
while those guarantees are absent.

## Remaining policies

- Personal processing is intended to continue online after closing and pause offline.
  Configuration declares this policy; no processing timer or output backend currently runs.
- Linked stations require permission and loaded/ticking state. Enchanting needs legitimate
  bookshelf context, beacons a real pyramid, and crafters explicit activation and disabled slots.
- Contextual inventories/trades must bind real permitted entities; offers are never invented.
- Powerful editors remain disabled and administrator-only. Chemistry remains feature-gated.
- Linux hashing/static inspection does not establish a callable SysV ABI. No Linux binary is shipped.

## Recovery

BDS exclusively owns native workstation items. Python never journals or reissues them.
Normal and forced close returned native inputs in live tests. Full-inventory overflow into
durable recovery storage and BDS crash-save boundaries remain release requirements.
Vanilla drops are not represented as a recovery vault; exactly-once persistence is not claimed.