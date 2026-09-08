# Native NPC dialogue

API 1.5 opens the actual NPC dialogue screen with a server-selected `EntityContext`:

```python
from endstone_remote_workstations.api import EntityContext, get_api

ui = get_api(self, minimum=(1, 5))
ticket = ui.open_npc(player,
    EntityContext(player.dimension.name, npc_actor, "myplugin.dialogue"),
    scene="welcome", branches=("recipes", "enchantments"))
```

Enable `[native]` and `[native.npc]` on the admitted exact Windows build, with
native world Education features enabled. The scene must already exist in an
installed behavior pack, or use `scene=""` for the real NPC's default dialogue.
The optional `[native.npc.source]` supplies the same context for
`/workstations open npc`. Source and interface permissions plus protection guards
are checked before opening, on each request and during the session.

BDS resolves the real NPC and sends its own dialogue, portrait and action list.
Temporary random tags address the exact actor and player for the native
`dialogue open` command; both tags are removed immediately afterward. Player
names, client selectors and client command text are never interpolated.
Removal uses BDS's native tag command because the pinned Endstone implementation
updates sparse tag indices incorrectly when removing a non-final entry.

The packet adapter replaces only the scene identifier with a fresh random token
for each presentation, including scene transitions. The client returns that token
in native opening, button and closing requests. The adapter checks source identity,
permissions, lifecycle and the actual native button index, then restores the real
scene name for BDS. Repeated requests, earlier tokens, foreign actors, privileged
editor operations and client-authored action text are rejected.

`on_open` requires the client's matching native opening request. Dependency
callbacks run on a subsequent server tick. Native scene commands remain native
commands; they may call commands registered by another plugin. Existing exported
dependency functions remain available through `UIClient.invoke()`.

Cancellation sends the NPC close packet and revokes subsequent commands, including
the scene's closing commands. It cannot undo commands BDS already accepted. Native
button actions can modify the world; authorize the actual source and its installed
scenes accordingly. An unrelated replacement scene is left to its owner.

Installed a22 passed native button commands, a two-scene transition, forced close
and protection revocation. Native command results, unique tokens, unchanged
inventory/XP and temporary-tag removal are bound to the exact wheel in
`research/catalog-evidence/npc-installed-a22-index.json`. Broader lifecycle stress
and other clients remain unqualified.
This feature remains disabled by default and is not production certified.

Native scene syntax and behavior follow the
[Minecraft Creator NPC dialogue documentation](https://learn.microsoft.com/en-us/minecraft/creator/documents/npcdialogue?view=minecraft-bedrock-stable).
