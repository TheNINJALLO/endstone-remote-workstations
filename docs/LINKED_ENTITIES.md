# Real entity inventories — dependency API 1.3

`chestminecart`, `hopperminecart` and `chestboat` use their actual loaded BDS
inventories. BDS processes every transfer, preserves item NBT, coordinates world
pickup, and saves the source entity. The plugin does not copy inventory snapshots
or create a second storage container. A temporary nearby client block displays the
inventory while its native server manager stays bound to the real ActorUniqueID.

```python
from endstone_remote_workstations.api import get_api, EntityContext

ui = get_api(self, minimum=(1, 3))
# Resolve and authorize this real Actor on the server's game thread.
ticket = ui.open_entity(player, "chestminecart", EntityContext(
    player.dimension.name, authorized_actor, "my_plugin.vehicle_access"))
```

The dependency must declare `depend = ["remote_workstations"]`. Enable
`native.enabled` and `native.entities.enabled` only on the exact admitted Windows
runtime. Product defaults leave both disabled. Context source permission is
additional to `remoteworkstations.use` and `remoteworkstations.open.<kind>`.
Omitting source permission requires the operator-default
`remoteworkstations.contexts`. Do not pass arbitrary player-provided Actor IDs or
grant broad source permissions as a substitute for your protection plugin's ACL.

A command can use a server-configured persistent ActorUniqueID:

```toml
[native.entities.sources.chestminecart]
dimension = "Overworld"
actor_id = -123456  # Replace with the actual entity's ID, not RuntimeID.
permission = "my_plugin.vehicle_access"
```

Commands resolve that exact loaded Actor once. Active sessions retain only its
public Endstone wrapper and snapshot identity, checking validity, dimension, type,
source permission and protection guards every 250 ms. Identity changes during
opening fail admission. Disappearance, source/player dimension changes, revoked
permission, timeout, death and owner-plugin disposal close the managed view.
Only the synchronous native open containing the owned actor ID is projected.
Foreign opens and item requests are not claimed by this backend. Native actual
contents must match the expected 27/5/27 slots before an open succeeds.

The native entry point preserves `ContainerComponent::canOpen`, including BDS's
interaction, entity lifecycle and configured owner-only checks. Standard vehicle
inventories do not imply pet ownership; source ACLs determine remote access.

The private ABI is pinned in the compiled native manifest: public Actor validation,
Level ActorUniqueID lookup at virtual offset `0x1f0`, actual Actor ID accessor,
`Actor::openContainerComponent` at virtual slot 109, and the original container
manager table. The distance adapter belongs only to the created manager instance.
No source entity is moved, replaced or duplicated to display its inventory.

Installed 0.4.0a4 Windows keyboard/mouse captures show all three vehicles retaining
a renamed Sharpness sword across close/reopen and returning it with identical NBT,
player inventory and XP. Real hopper pickup, chest-boat movement and source removal
also passed. `research/catalog-evidence/entity-index.json` records the immutable
wheel hash and exact test scope. An actual BDS inventory avoids a cross-store item
writeback; it does not certify all server crash/recovery behavior.

## Equipment sources

The same `open_entity()` API admits `horse`, `donkey`, `mule`, `llama`, `traderllama`,
`camel`, `camelhusk`, `zombiehorse`, `nautilus` and `zombienautilus`. These use the real
native equipment screen, actual actor metadata and inventory. Pet access requires
the requesting player's native ActorUniqueID to equal the actual owner before
opening and while active. Camels are naturally tamed shared mounts, so their
server-authorized source permission and native access rules apply. Opening never
tames a source, grants a chest or saddle, changes strength, or assigns an owner.

| Kind | Native slots |
|---|---|
| Horse, zombie horse, nautilus variants | 2 equipment slots |
| Donkey, mule | 1 saddle slot; 16 total with a chest |
| Llama variants | 1 carpet slot; 3 storage slots per strength level with a chest |
| Camel variants | 1 saddle slot |

The client view projects the actual entity near the player. Known client actors
receive a temporary position update and restore their current real position on
close. An untracked source uses BDS's actual AddActor serialization; only that
introduced client view is removed afterward. World tracking transitions close the
view while retaining the legitimate world actor. The source never moves on the
server. Tracking is bounded and fails closed if full or incomplete; players must
rejoin after a plugin reload that occurred while they were already in the world.
An inventory size change closes an existing view rather than retaining stale slots.

All ten equipment screens opened in the isolated development probe. Horse saddle
and armor transfers and chested donkey storage passed native item round trips.
These are probe results; packaged qualification is recorded separately. Skeleton
horses have no vanilla standalone equipment inventory in the pinned build and are
explicitly excluded. The installed a6 evidence is bound to its exact wheel in
`research/catalog-evidence/equipment-index.json`.

## Merchant sources

`villager` and `wanderingtrader` use `open_entity()` with an authorized real merchant.
The native call enters the normal GameMode interaction pipeline, including the
Endstone `PlayerInteractActorEvent`. BDS prepares its actual offers and customer,
opens its economy manager, validates payments, updates stock and XP, and releases
the customer when closed. The per-instance distance adjustment delegates all
remaining native validity checks, including customer identity and profession.
No offers, discounts, payments or outputs are synthesized by this backend.

Only a matching native UpdateTrade for the source ActorUniqueID can establish the
view. The API waits for the actual three-slot contents before reporting it open.
Merchants share the bounded actor-display lifecycle described above. Unavailable,
busy, nontrading or interaction-vetoed merchants fail admission. A protection
plugin can use the standard interaction event or the workstation source guard.
Custom economy overrides must use the server's supported behavior-pack recipes
and trading definitions; this API does not rewrite native offer packets.

Both development merchant captures completed one existing offer, incremented
exactly its use count from zero to one, retained it after reopen and restored the
player inventory and XP after explicit fixture cleanup. See
`research/catalog-evidence/trade-prototype-index.json`. Restock, every profession,
discount variants, multiple clients and broader recovery require separate testing.

Installed a7 also completed both real merchant purchases and retained the use
increment from one to two after reopen. Its capture index verifies the installed
backend/native bytes against the immutable wheel, exact player baseline recovery,
an actual `PlayerInteractActorEvent` veto, and an active horse ownership revocation.
See `research/catalog-evidence/trade-index.json`; these are the packaged results.
