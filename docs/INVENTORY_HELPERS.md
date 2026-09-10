> **Legacy v0.4.0 documentation.** This page describes the Python/native-companion implementation. For the native prerelease, use [installation](NATIVE_INSTALLATION.md), [examples](NATIVE_EXAMPLES.md) and [SDK 1.5](NATIVE_SDK.md).

# User repository inventory-helper review

Inspected on September 6, 2026, using the authenticated GitHub account and exact
public source revisions. The shared library was followed from Backpacks' actual
dependency metadata, rather than treating it as the earlier Shock95 checkout.

| Repository | Revision consulted | Useful implementation |
|---|---|---|
| [Ninjo's Backpacks](https://github.com/TheNINJALLO/endstone-ninjos-backpacks/tree/276f4a87378577224ecee95adcb8bc6baaf8eb18) | `276f4a873785` (1.0.90) | Inventory slot buttons, page persistence/repaint/resync, open-backpack ownership and typed NBT serialization. |
| [Inventory Manager](https://github.com/TheNINJALLO/endstone-inventory-manager/tree/6f02b1a06f9149fcde12c704cfa3fa833800d2b4) | `6f02b1a06f91` (1.0.15) | A packet chest populated from and synchronized to the real online `Player.ender_chest`. |
| [The user's InventoryUI fork](https://github.com/TheNINJALLO/endstone-inventoryui/tree/366c4de0c39852962530ac1e7a2545fd7a18dac8) | `366c4de0c398` (2.0.6; tag and main) | Shared Menu/Inventory/session API, request parsing and response construction used by these projects. |

Archive hashes are retained privately in `research/helper-references.lock.json`.
This research does not modify any GitHub repository or install the helpers on the
test server. Full helper source checkouts remain excluded from release archives.

## Actual Ender Chest route

Inventory Manager's `inventory_manager.py:363` creates a `MenuType.CHEST` and reads
the target's 27 `ender_chest` slots. Its transaction listener schedules synchronization
one tick later, and its close listener synchronizes again. `_sync_online_enderchest`
at line 389 writes the menu contents back through `target.ender_chest.set_item`.
The same approach appears in Backpacks' `inv_manager.py`.

This is access to the real online Ender Chest through the public API. It provides
an alternative implementation path to the native container transaction source
that currently rejects remote transfers. The public property already exists in
the project's required Endstone 0.11.0 baseline. BDS saves still own that inventory;
the helper's SQLite copy is not an atomic part of a BDS player save.

RemoteWorkstations can use this storage source behind its own validated packet
transaction/session layer. Importing an offline database snapshot as a new player
vault is unnecessary. The helper's current full-menu writeback cannot be copied
unchanged: it has no revision check and could overwrite an intervening Ender edit.
Validation and writes must be part of the same owned transaction, not delayed
writeback after the viewer's inventory has already changed.

## Packet compatibility measured against real captures

The fork's metadata targets BDS 1.26.44 and pins packet dependency **0.0.9**.
Its request decoder expects one action-type byte and tagged variable-length stack
IDs. Our actual 1.26.45 captures contain an action variant plus its inner type and
fixed 32-bit request slot IDs. Its response encoder also omits outer optional
markers present in these captured success and error responses.

The comparison used the helper's exact 0.0.9 dependency in an isolated directory.
**None of four original-helper request interpretations or four response encodings
matched the captured 1.26.45 data.** For example, the real Ender deposit is
hotbar role 28, slot 1, stack ID 21 to storage role 7, slot 0. The helper reads its
source as role 1, slot 0, ID -4. These are offline replay findings; no exploit or
live helper corruption is claimed.

The new [compatibility adapter](../integrations/inventoryui_2169.py) decodes the
known transfer fields correctly and reproduces all four responses byte-for-byte,
including when given actual objects created by the helper's response classes.
**15 focused tests** cover the captured transfer fields, response bytes, malformed
and truncated packets, stack-ID validation and bounds. The audit and packet
provenance are retained privately in `research/inventory-helper-audit.json`.
The helper-review-stage workspace suite passed **153 tests**; that run is recorded separately
in `research/helper-tests.log` and `research/helper-junit.xml`. The previous
installed-alpha SDK evidence remains attached to its original wheel and tests.

## Findings addressed by the managed adapter

These are source review findings, separate from the measured codec mismatch:

| Location in the InventoryUI fork | Required adaptation |
|---|---|
| `listener.py` receive handlers | Queue packet sends and game callbacks onto the owner tick; current ping, close and transaction handlers send recursively during receive callbacks. |
| `listener.py:213` | Replace PacketViolationWarning-based open acknowledgement with session-specific ordering/readiness checks and actual client evidence. |
| `container_manager.py:130` | Stage drops until the entire request is accepted; the current drop spawns an entity before the later transaction commit. |
| `item_stack_tracker.py:13` and container mutation methods | Validate client references against authoritative session/item IDs; seeding IDs from requests is not validation. |
| `container_manager.py` mutations/commit | Enforce role bounds, stack maxima, full-batch validation and recheck current authoritative state before any writes. |
| `listener.py` batch rejection path | Account for every request in a batch; returning after the first rejection leaves subsequent requests unprocessed. |
| `session.py:97` | Preserve cursor overflow; `add_item` leftovers are currently discarded. Remove destructor-triggered gameplay cleanup. |
| Session ID/graphics/lifecycle | Coordinate generations with native and custom SDK views, restore current block state in the correct dimension, and handle death/teleport/disable explicitly. |

Backpacks has useful page-change guards and resynchronization. It also applies
global protocol/NBT patches on import and silently catches several serialization
errors, so its entry point should not be imported merely to obtain a serializer.
Keep the reusable storage, page and metadata logic independent of those global
side effects. The user-owned Backpacks project was reviewed; its source was not
vendored. Inventory Manager declares MIT in its metadata; the InventoryUI fork
retains Shock95's MIT license.

The resulting implementation is in packet_backend.py, packet_inventory.py,
packet_items.py and the API 1.1 coordinator. It binds the real online Ender
store and provides protected inventory menu actions and page changes. See
[packet inventory behavior](PACKET_INVENTORY.md) for the implemented contract and
remaining qualification limits. The original 0.3.0a1 wheel remains unchanged.
