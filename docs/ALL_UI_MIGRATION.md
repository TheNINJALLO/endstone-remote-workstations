# All UI migration

The immutable floor is [original-ui-catalog.json](../research/original-ui-catalog.json), captured from `e96529cc26b204d174581714d87f205a7de5270b` plus the preserved API 1.7 working tree. Alias resolution and command permissions are retained in C++ registration. A retained name with an unavailable response is **not migrated gameplay**.

| Canonical ID | Original aliases | Permission | Source / native outcome |
|---|---|---|---|
| `chest` | `/chest` | `remoteworkstations.open.chest` | block; C++ session integration pending |
| `doublechest` | `/doublechest` | `remoteworkstations.open.doublechest` | block; C++ session integration pending |
| `hopper` | `/hopper` | `remoteworkstations.open.hopper` | block; C++ session integration pending |
| `dispenser` | `/dispenser` | `remoteworkstations.open.dispenser` | block; C++ session integration pending |
| `barrel` | `/barrel` | `remoteworkstations.open.barrel` | block; C++ session integration pending |
| `dropper` | `/dropper` | `remoteworkstations.open.dropper` | block; C++ session integration pending |
| `trappedchest` | `/trappedchest` | `remoteworkstations.open.trappedchest` | block; C++ session integration pending |
| `enderchest` | `/enderchest`, `/ec` | `remoteworkstations.open.enderchest` | player; C++ session integration pending |
| `shulker` | `/shulker` | `remoteworkstations.open.shulker` | held-item; experimental Linux real-source editor, full qualification pending |
| `craft` | `/craft`, `/workbench` | `remoteworkstations.open.craft` | player; Experimental C++ original session implemented; client and custom qualification pending |
| `anvil` | `/anvil` | `remoteworkstations.open.anvil` | player; Experimental C++ original session implemented; client and custom qualification pending |
| `grindstone` | `/grindstone` | `remoteworkstations.open.grindstone` | player; Experimental C++ original session implemented; client and custom qualification pending |
| `smithing` | `/smithing` | `remoteworkstations.open.smithing` | player; Experimental C++ original session implemented; client and custom qualification pending |
| `stonecutter` | `/stonecutter` | `remoteworkstations.open.stonecutter` | player; Experimental C++ original session implemented; client and custom qualification pending |
| `loom` | `/loom` | `remoteworkstations.open.loom` | player; Experimental C++ original session implemented; client and custom qualification pending |
| `cartography` | `/cartography` | `remoteworkstations.open.cartography` | player; Experimental C++ original session implemented; client and custom qualification pending |
| `enchanting` | `/enchanting`, `/etable` | `remoteworkstations.open.enchanting` | block; C++ session integration pending |
| `furnace` | `/furnace` | `remoteworkstations.open.furnace` | block; C++ session integration pending |
| `blastfurnace` | `/blastfurnace` | `remoteworkstations.open.blastfurnace` | block; C++ session integration pending |
| `smoker` | `/smoker` | `remoteworkstations.open.smoker` | block; C++ session integration pending |
| `brewing` | `/brewing` | `remoteworkstations.open.brewing` | block; C++ session integration pending |
| `beacon` | `/beacon` | `remoteworkstations.open.beacon` | block; C++ session integration pending |
| `crafter` | `/crafter` | `remoteworkstations.open.crafter` | block; C++ session integration pending |
| `lectern` | None | `remoteworkstations.open.lectern` | block; C++ session integration pending |
| `writtenbook` | None | `remoteworkstations.open.writtenbook` | held-item; C++ session integration pending |
| `bookediting` | None | `remoteworkstations.open.bookediting` | held-item; C++ session integration pending |
| `villager` | None | `remoteworkstations.open.villager` | entity; C++ session integration pending |
| `wanderingtrader` | None | `remoteworkstations.open.wanderingtrader` | entity; C++ session integration pending |
| `horse` | None | `remoteworkstations.open.horse` | entity; C++ session integration pending |
| `donkey` | None | `remoteworkstations.open.donkey` | entity; C++ session integration pending |
| `mule` | None | `remoteworkstations.open.mule` | entity; C++ session integration pending |
| `llama` | None | `remoteworkstations.open.llama` | entity; C++ session integration pending |
| `traderllama` | None | `remoteworkstations.open.traderllama` | entity; C++ session integration pending |
| `chestboat` | None | `remoteworkstations.open.chestboat` | entity; C++ session integration pending |
| `chestminecart` | None | `remoteworkstations.open.chestminecart` | entity; C++ session integration pending |
| `hopperminecart` | None | `remoteworkstations.open.hopperminecart` | entity; C++ session integration pending |
| `camel` | None | `remoteworkstations.open.camel` | entity; C++ session integration pending |
| `camelhusk` | None | `remoteworkstations.open.camelhusk` | entity; C++ session integration pending |
| `zombiehorse` | None | `remoteworkstations.open.zombiehorse` | entity; C++ session integration pending |
| `skeletonhorse` | None | `remoteworkstations.open.skeletonhorse` | interaction-only; C++ session integration pending |
| `nautilus` | None | `remoteworkstations.open.nautilus` | entity; C++ session integration pending |
| `zombienautilus` | None | `remoteworkstations.open.zombienautilus` | entity; C++ session integration pending |
| `allay` | None | `remoteworkstations.open.allay` | interaction-only; C++ session integration pending |
| `panda` | None | `remoteworkstations.open.panda` | interaction-only; C++ session integration pending |
| `piglin` | None | `remoteworkstations.open.piglin` | interaction-only; C++ session integration pending |
| `agent` | None | `remoteworkstations.open.agent` | entity; C++ session integration pending |
| `sign` | None | `remoteworkstations.open.sign` | block-editor; C++ session integration pending |
| `npc` | None | `remoteworkstations.open.npc` | entity-dialogue; C++ session integration pending |
| `commandblock` | None | `remoteworkstations.open.commandblock` | block-editor; C++ session integration pending |
| `commandblockminecart` | None | `remoteworkstations.open.commandblockminecart` | entity-editor; C++ session integration pending |
| `structure` | None | `remoteworkstations.open.structure` | block-editor; C++ session integration pending |
| `jigsaw` | None | `remoteworkstations.open.jigsaw` | block-editor; C++ session integration pending |
| `compoundcreator` | None | `remoteworkstations.open.compoundcreator` | block; C++ session integration pending |
| `elementconstructor` | None | `remoteworkstations.open.elementconstructor` | block; C++ session integration pending |
| `materialreducer` | None | `remoteworkstations.open.materialreducer` | block; C++ session integration pending |
| `labtable` | None | `remoteworkstations.open.labtable` | block; C++ session integration pending |
| `inventory2x2` | None | `remoteworkstations.open.inventory2x2` | embedded; Experimental C++ original session implemented; client and custom qualification pending |
| `armor` | None | `remoteworkstations.open.armor` | embedded; Experimental C++ original session implemented; client and custom qualification pending |
| `offhand` | None | `remoteworkstations.open.offhand` | embedded; Experimental C++ original session implemented; client and custom qualification pending |
| `cursor` | None | `remoteworkstations.open.cursor` | embedded; C++ session integration pending |
| `recipebook` | None | `remoteworkstations.open.recipebook` | embedded; Experimental C++ original session implemented; client and custom qualification pending |
| `bundle` | None | `remoteworkstations.open.bundle` | held-item; C++ session integration pending |
| `hud` | None | `remoteworkstations.open.hud` | embedded; C++ session integration pending |
| `cauldron` | None | `remoteworkstations.open.cauldron` | interaction-only; C++ session integration pending |
| `jukebox` | None | `remoteworkstations.open.jukebox` | interaction-only; C++ session integration pending |
| `decoratedpot` | None | `remoteworkstations.open.decoratedpot` | interaction-only; C++ session integration pending |
| `fletching` | None | `remoteworkstations.open.fletching` | interaction-only; C++ session integration pending |
| `campfire` | None | `remoteworkstations.open.campfire` | interaction-only; C++ session integration pending |
| `composter` | None | `remoteworkstations.open.composter` | interaction-only; C++ session integration pending |

## Original SDK surfaces

| Original type / function | Native contract and current gap |
|---|---|
| `UIError` | vcf_status structured errors; finer diagnostics pending. |
| `HeldItemInfo` | Inspection contract and C ABI equivalent pending. |
| `Button` | `vcf_button`; label, named action and texture icon. |
| `Menu` | `vcf_menu_desc` / show_menu; ordinary ActionForm through C++; client qualification pending. |
| `SlotButton` | Protected item icon/click contract pending. |
| `InventoryMenu` | Protected 27-slot native inventory adapter pending; no form substitution. |
| `EnderChest` | Real-source enderchest request retained; native C++ session implementation pending. |
| `HeldItem` | Real-source shulker request retained; experimental Linux identity/source locks and journaled transfers implemented; Windows and full qualification pending. |
| `BlockContext` | vcf_session_desc dimension/position/source_permission; source validation/opening pending. |
| `SignContext` | Sign-specific typed fields/side and editor callbacks pending. |
| `LinkedBlock` | Canonical ID plus real-source descriptor; binding pending. |
| `EntityContext` | vcf_session_desc entity_id/source_permission; source validation/opening pending. |
| `LinkedEntity` | Canonical ID plus real-source descriptor; binding pending. |
| `NpcDialogue` | Typed NPC branches and native scene lifecycle pending. |
| `SessionInfo` | vcf_session_info; immutable caller-owned copy. |
| `ActionContext` | vcf_event with ticket/player/result/revision/detail and consumer-owned context. |
| `Ticket` | Opaque session handle with session_info, close and forget. On-open/failure/close callbacks and deferred dispatch implemented; full native window lifecycle pending. |
| `UIClient` | `vcf_api` / `sdk::Client`; register_action, unregister_action, action listing, scoped protection guards, invoke, prepare/open/close, capability and dispose exist. Protected inventory menus, held inspection and typed editor hooks remain missing. |
| `get_api` | oni_vcf_get_api negotiated function table; sdk::discover resolves the loaded shadow module. |
