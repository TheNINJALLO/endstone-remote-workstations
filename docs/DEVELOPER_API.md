# Dependency API 1.5 (release 0.4.0)

RemoteWorkstations can now be consumed by other Endstone plugins. The public
Python contract has a separate version from the plugin and native companion.
API 1.1 added real online Ender access and inventory slot menus. API 1.2 added
authorized real block contexts; 1.3 adds real vehicle, equipment and merchant
contexts. API 1.4 adds native front/back sign editing through `open_sign()` and
`SignContext`. API 1.5 adds real NPC dialogue through `open_npc()` and
`NpcDialogue`. Existing consumers retain their form, action and native station contracts.

Declare `depend = ["remote_workstations"]` in the consuming Plugin class, install
both wheels in `plugins`, and acquire a client during `on_enable`:

```python
from endstone.plugin import Plugin
from endstone_remote_workstations.api import Button, Menu, get_api

class MyPlugin(Plugin):
    api_version = "0.11"
    depend = ["remote_workstations"]

    def on_enable(self):
        self.ui = get_api(self, minimum=(1, 0))
        self.ui.register_action("choose_recipe", self.choose_recipe,
                                permission="my_plugin.recipes", export=True)

    def show(self, player):
        return self.ui.show_menu(player, Menu("My workshop", (
            Button("Choose a recipe", "choose_recipe"),
            Button("Open anvil", "native:anvil"),
        ), permission="my_plugin.recipes"))

    def choose_recipe(self, context):
        context.player.send_message("Your plugin handles recipe selection here.")

    def on_disable(self):
        self.ui.dispose()
```

Register the permissions used by your plugin in its normal Endstone permission
metadata. The SDK rechecks menu permission and action permission on selection.
Native actions also retain workstation permissions, protection guards, exact
binary checks, client admission and one-native-container restrictions.

## Calls and lifecycle

| Call | Behavior |
|---|---|
| `open_native(player, "anvil")` | Queues the admitted native BDS screen. Accepts catalog IDs/aliases. |
| `open_linked(player, kind, BlockContext(...))` | Opens a real server-authorized block, retaining its native storage and world ticking. |
| `open_entity(player, kind, EntityContext(...))` | Opens an authorized actual vehicle, equipment source or merchant; never invents stock or contents. |
| `open_npc(player, EntityContext(...), scene="welcome", branches=("next",))` | Opens real NPC dialogue with installed native scenes and guarded scene commands. |
| `show_menu(player, Menu(...))` | Queues a custom ActionForm with named callback buttons and optional texture icons. |
| `open_ender_chest(player)` | Queues the player's real online Ender Chest through the managed packet backend. |
| `show_inventory(player, InventoryMenu(...))` | Queues a 27-slot chest screen with protected server ItemStack icons. |
| `register_action(name, callback, permission=..., export=False)` | Creates a unique `plugin_name:action` owned by that plugin instance. |
| `invoke(player, "other_plugin:action")` | Queues an explicitly exported function and returns a Future. |
| `actions()` | Lists own actions and exports visible to this consumer. |
| `capabilities(player)` | Returns all 69 catalog entries with current availability and refusal reasons. |
| `ticket.info` | Immutable state: queued, opening, open, closed, failed or cancelled. |
| `ticket.cancel()` | Requests cleanup of that generation only. |
| `dispose()` | Revokes owned registrations and requests cancellation of owned views. |

All screen-opening methods accept `on_open(info)` and `on_close(info)`.
`on_close` also receives failed/cancelled terminal results, with `info.reason`.
Callbacks are suppressed after their owner is disposed/disabled or the provider
shuts down; retained tickets still receive a terminal state during cleanup.
The menu `open` event means its outgoing packet was observed, not proof the client
rendered it. Native `open` requires a matching BDS container generation/window.
An unsupported native screen produces a failed ticket without inventory changes.

Calls require the Endstone game thread. Calls made during game events can enqueue
work immediately; screen sends and button callbacks execute on a subsequent tick.
Never wait for a Future on the game thread. Check `done()` or use a short completion
callback. An exported action's return value becomes the Future result; exceptions
become its exception. Game mutations in your callback remain your plugin's responsibility.

Each player may have one dependency UI request. Close/dismiss it before opening
another. A selected menu retires its generation before running its action, so an
action may open a submenu or native station. Existing unrelated native containers
are refused rather than forcibly replaced. Limits: 100 active requests, 54 menu
buttons, 128 actions per plugin, 4,096 total actions, 256 queued invocations and
16 visits/invocations per poll. These are bounds, not a server load certification.

Disconnect, death, teleport, dimension change, owner disable and provider shutdown
invalidate managed views. A native cancellation closes only its matching generation.
For `inventory2x2`, `armor`, `offhand` and `recipebook`, cancellation relinquishes
the plugin lease while BDS keeps the ordinary player inventory and its inputs.
Windows ignores server close packets for that shared screen. The player closes
it normally; the native readiness guard blocks another workstation until then.
Outgoing replacement screens invalidate stale menu callbacks. Duplicate, late,
foreign-player or out-of-range menu selections cannot dispatch an action twice.
Plugin reload requires acquiring a new client; old clients are invalid.

`BlockContext(dimension, position, permission=None)` and
`EntityContext(dimension, actor, permission=None)` must come from authorized server
logic. Both the interface permission and source permission are checked. With no
source permission, `remoteworkstations.contexts` defaults to operators. The API
does not resolve arbitrary client coordinates or grant entity ownership. See
[block contracts](LINKED_BLOCKS.md) and [entity contracts](LINKED_ENTITIES.md).

## Packet inventories and gameplay extensions

Shock95's [InventoryUI](https://github.com/Shock95/endstone-inventoryui) at pinned commit
`a463cf110d2f799b4382ff5fbca829d1f912d2a7` supplies the useful Menu/listener pattern
and supports packet-backed chest, double chest, hopper and dispenser inventories.
The SDK offers ActionForm menus and protected packet inventory menus. Its managed
packet provider adapts the user's helper patterns with authoritative identities,
deferred callbacks and real Ender inventory binding. It does not import helper
listeners or global patches. See [the packet contract](PACKET_INVENTORY.md).

| Extension | Current contract |
|---|---|
| Native crafting, anvil, stonecutter, grindstone, smithing, loom, cartography | Programmatic open through the same admitted BDS backend as commands. |
| Custom menus and exported functions | Available through this SDK; functions run in the owning plugin. |
| New vanilla recipes | Use supported behavior-pack recipes; BDS remains the executor. No public recipe-registration API is fabricated. |
| Custom recipe stations | A callback may open your selector or call your existing gameplay system. Transactional inputs, output and recovery are not supplied by an ActionForm. |
| New enchantment IDs/effects | No arbitrary native enchantment registry is exposed. A plugin can implement its own effects using actual available Endstone events. |
| Packet slot menus and Ender Chest transfers | API 1.1 candidate backend; explicit opt-in and exact-build admission. See packet limits and test evidence. |

Do not install a second packet inventory plugin and assume shared transaction
ownership is automatic. The provider claims only its own sessions and coordinates
with native contexts; wider helper coexistence remains unqualified. This API exposes no unchecked raw pointer,
successful-transaction override, synthetic recipe output or fake Ender vault.

## Runnable consumer

`examples/dependency_plugin` is a complete separate plugin project. Build it with
`python -m build --wheel --no-isolation examples/dependency_plugin` and install its
wheel alongside RemoteWorkstations. `/uidemo` exercises callbacks, submenus and native
navigation; `/uidemo open <type>` opens via the public API, and `/uidemo status`
reports its bounded lifecycle/action log. The example grants no items or XP.
`/uidemo inventory` opens the two-page slot menu; `/uidemo ender` opens the real Ender store.

The SDK's automated checks cover export isolation, permissions, thread admission,
deferred dispatch, stale/duplicate selections, native generations and owner teardown.
See [the SDK test report](DEVELOPER_API_TEST_REPORT.md) for actual installed-wheel client results.
