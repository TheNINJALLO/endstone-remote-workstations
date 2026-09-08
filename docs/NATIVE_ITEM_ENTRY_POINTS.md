# Held item entry-point audit

The next development slice adds [held-item snapshots, inspection and journal
primitives](HELD_ITEMS.md). Native opening and mutation remain unavailable; the
entry-point findings below still apply.

The pinned Endstone Item header compiles `Item::use` at virtual slot 81 and
`Item::getComponent` at slot 24. Read-only inspection of four real inventory
stacks matched their counts and auxiliary values before examining their Item
dispatch tables. No item-use function was invoked by this audit.

| Actual item | BDS use RVA | Finding |
| --- | --- | --- |
| Writable book | `0x335e540` | Screen opening is behind `ILevel::isClientSide()` |
| Written book | `0x33628f0` | Screen opening is behind `ILevel::isClientSide()` |
| Bundle | `0x33cb7a0` | Dispatches native item components |
| White shulker box | `0x24e75c0` | Uses the shared BlockItem use implementation |

The compiled level header identifies `isClientSide` as virtual slot 317. Both
book functions call `Player::openBook` only on that branch. The BDS player book
opener already admitted for lecterns requires a real block actor; this audit
does not establish a force-open path for a held book.

Bundle dynamic-container admission and held shulker identity, durable writeback,
drop/swap/nesting behavior and crash reconciliation remain separate engineering
work. These entries remain unavailable. Their catalog status must not be promoted
because an item has a virtual use function or because a packet schema exists.

Exact function hashes, observed stack types and header identity are recorded in
`research/catalog-evidence/item-native-entry-a23-index.json`.
