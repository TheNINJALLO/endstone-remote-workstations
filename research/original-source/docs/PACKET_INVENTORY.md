# Packet inventory adapter (API 1.1)

The adapter follows Inventory Manager's use of `player.ender_chest`, Backpacks'
guarded page changes, and InventoryUI's packet presentation pattern. It uses the
captured 1.26.45 request/response layout and its own session coordinator. None of
the helper plugins or their global patches are imported.

## Use it as a dependency

```python
from endstone.inventory import ItemStack
from endstone_remote_workstations.api import InventoryMenu, SlotButton, get_api

def on_enable(self):
    self.ui = get_api(self, minimum=(1, 1))
    self.ui.register_action('recipes', self.show_recipes,
                            permission='my_plugin.recipes', export=True)
    self.ui.register_action('ender', lambda ctx: ctx.ui.open_ender_chest(ctx.player))

def open_workshop(self, player):
    return self.ui.show_inventory(player, InventoryMenu('My workshop', (
        SlotButton(10, ItemStack('minecraft:diamond'), 'recipes'),
        SlotButton(13, ItemStack('minecraft:anvil'), 'native:anvil'),
        SlotButton(16, ItemStack('minecraft:ender_chest'), 'ender'),
    )))
```

Declare `depend = ['remote_workstations']` on your Plugin class. Register your
permissions normally. Use server-created ItemStacks; the provider clones complete
NBT when queuing a menu. Slots must be distinct integers from 0 through 26.
Icons cannot be taken, shifted into the real inventory or dropped. A normal click
closes the old screen, then invokes the registered action on a later owner tick.
The action can show another InventoryMenu page, open a form, open a native station,
or call your own recipe/enchantment code. The runnable example demonstrates two pages.

Actions remain scoped to their owning plugin. Other plugins can invoke only
explicit exports. Names such as `native:anvil` retain the native backend's admission
checks. `open_native('enderchest')` remains a native-only request; use
`open_ender_chest()` for the real packet-bound Ender store. `capabilities(player)`
reports its backend as `packet_real_ender`.

## Real item behavior

Each opening reads the requesting player's actual 27 Ender slots. There is no
separate vault, SQLite mirror, hidden backing block or teleport. Only the client
receives a temporary chest block and title at a nearby air position. Cleanup sends
the current block state in the original dimension; world blocks are never written.

Every owned request is decoded and validated before changing a shadow snapshot.
The model enforces stack identities, container roles, counts, slot bounds, maximum
stack sizes and metadata compatibility. Client packets never introduce item types,
NBT or authoritative network IDs. Exact replays do not commit twice; altered or
expired requests are rejected. Every decoded request in a batch receives a response.

Picking up an item reserves it in the displayed inventory. Until the cursor is
empty, the real items remain in their original slots. Completing the gesture
compares the real inventories with the baseline and writes only changed slots.
A forced close or disconnect discards an unfinished reservation. The client may
send a final placement before its normal Escape close; that valid placement commits.
An external inventory change closes the view instead of overwriting newer items.

NBT is cloned in full through Endstone. Display serialization includes the complete
typed compound, including names, enchantments, repair costs, maps and banner patterns.
The admitted native companion refreshes BDS inventories on close, restoring current
native stack IDs without clearing and reinserting real items.

## Candidate limits

Both `[native]` and `[packet_inventory]` require explicit configuration opt-in.
Admission currently requires the exact Windows BDS/runtime hashes and Windows
Bedrock 1.26.45. Packet and native views share permission, protection and lifecycle
checks. Inventory menu actions are deferred, and owner disable prevents callbacks.

Direct dropping inside these packet screens is rejected. Place the item, close the
screen, then drop normally. Shield descriptors and adventure-mode CanPlaceOn/
CanDestroy metadata are refused until their item-specific wire fields are qualified.
The whole presented inventory must fit the bounded packet size. Sessions last at
most five minutes; request queues and rapid window changes are bounded.

This adapter does not register arbitrary native recipes or enchantment types. It
lets plugins present choices and call their own registered functions. General
portable storage, shulkers, processing and contextual native editors remain separate
unfinished capabilities. Two BDS inventory writes are not atomic with a process
crash/save boundary. Crash durability, mobile/controller clients, Linux and broad
plugin interoperability are not certified by the current alpha.
