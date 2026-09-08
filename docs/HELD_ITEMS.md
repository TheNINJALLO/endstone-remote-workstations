# Held-item development

The `0.5.0.dev1` development branch starts held-item support with actual-item
inspection and a reservation/write-intent journal. The published `0.4.0` release
remains unchanged. No held-item screen or mutating backend is enabled.

## Inspect the actual main-hand item

Dependency API **1.6** adds a synchronous, read-only operation. Call it on the
Endstone game thread after acquiring your plugin's dependency client:

```python
from endstone_remote_workstations.api import get_api, UIError

def describe_held_item(plugin, player):
    ui = get_api(plugin, minimum=(1, 6))
    try:
        held = ui.inspect_held_item(player)
    except UIError as error:
        player.send_error_message(str(error))
        return
    player.send_message(
        f"Holding {held.type_id} in hotbar slot {held.slot + 1}. "
        f"Native opening available: {held.native_open_available}"
    )
```

The immutable `HeldItemInfo` contains `kind`, `type_id`, zero-based `slot`,
`amount`, `digest`, optional `durable_id`, `metadata_bytes` and
`native_open_available`. It contains no raw NBT, book text or stored-item contents.
The corresponding `remoteworkstations.open.<kind>` permission is checked.
Disconnected/dead players, disposed dependency clients and calls from another
thread are refused.

Inspection recognizes built-in colored/undyed shulker boxes, bundles and
written/writable books. Unknown/custom identifiers are refused. It compares the
selected hotbar slot with the main-hand stack and detects a selection change
during the read. It does not tag, move, reserve or open an item. All held-item
catalog entries remain unavailable.

## Snapshot and journal behavior

`held_items.py` snapshots type, amount, auxiliary value and all NBT exposed by
Endstone. Detached ItemStack reconstruction must preserve the snapshot. Custom
tags, names, lore, enchantments and nested contents are retained. The private
journal encoding preserves tag widths and finite floating-point values,
including negative zero. Endstone's byte tags are unsigned. Size, depth and
element bounds reject excessive metadata; unsupported tags and non-finite
floats are refused instead of being discarded or normalized.

The reserved `remote_workstations:held_id` field is a canonical nonzero UUID.
Preparing an identity creates a detached candidate; it never modifies a live
item. A digest identifies content, so identical copies share a digest. Another
server plugin can also copy a UUID-tagged item. Neither value alone establishes
exclusive physical ownership.

`held_journal.py` extends the existing journal on its bounded disk worker:

1. Reserve one identified shulker per player and one lease per item UUID, across
   consumer plugins. Quarantined records continue to block admission.
2. Persist a complete before/after intent with a lease revision. Reject reused,
   stale or mismatched sources and changes to owner, slot, type or identity.
3. Record dispatch before an external apply, then separately record a verified
   in-memory result. These are persistence primitives, not gameplay validation
   or permission to write an inventory.
4. Release a clean reservation idempotently. Quarantine reservations with write
   intents on close and unfinished reservations on restart. Startup includes
   these in recovery reporting. No recovery path issues items or retries a write.

Journal records contain full metadata and stay in the plugin data directory.
Do not publish them as diagnostics or delete quarantines to unlock an item
without reconciling its actual saved state.

## Remaining before `/shulker` can open

- Assign and validate durable identity through an admitted native source, with
  a generation that distinguishes the source from an identical copy.
- Bind held-box contents to the native UI and exclude moves, swaps, drops,
  transfers, placement and concurrent access before BDS accepts a mutation.
  Polling and a SQLite reservation are insufficient.
- Enforce nesting, item conservation and cursor rules in the executor, including
  legitimate batched actions and external inventory-plugin changes.
- Coordinate inventory/box writeback and qualify crash boundaries against BDS
  saves. The current journal cannot establish exactly-once recovery or
  automatically release a write-bearing quarantine.
- Exercise the installed implementation with the Windows Bedrock client.

Books retain their client-side opening gap. Bundle dynamic identities, weights
and nesting need a separate executor. See the [native entry-point audit](NATIVE_ITEM_ENTRY_POINTS.md).

## Source and validation provenance

The [development test report](HELD_ITEM_TEST_REPORT.md) records 637 passing tests,
the 36-type detached runtime probe and the exact development wheel identity.

Research uses Endstone [0.11.0 at f534d435](https://github.com/EndstoneMC/endstone/tree/f534d435043029e6010b435e4480bfdb76fe39ae)
and the pinned [0.11.10 runtime at 8f84d6f5](https://github.com/EndstoneMC/endstone/tree/8f84d6f5b556916597ed5b6b71329b2ed3ca8fc8).
Relevant files are `include/endstone/nbt/tag.h`,
`include/endstone/inventory/player_inventory.h`, and
`src/endstone/core/inventory/item_stack.cpp`. No newer public API is required.

Tests use real Endstone NBT value types, source/API contract fixtures, competing
database connections and five real subprocess deaths. The external file in the
crash tests simulates BDS persistence; these are not live-server crash tests.
The runtime probe uses detached ItemStacks and does not qualify a held-item UI.
