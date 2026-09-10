# All UI examples

The separately compiled `endstone_vcf_catalog_showcase.dll` uses only the public SDK and loader headers. Its form/action demonstration is implemented; native workstation requests currently refuse. The console `UICatalogShowcase` program is a scope checker with a test host, not a gameplay consumer.

The separately compiled [NativePassthrough](../examples/native/NativePassthrough.md)
consumer now issues the eleven experimental original-mode requests through the
public SDK, from commands or an opt-in compass interaction. It includes source
permissions, guard callbacks, terminal-event reporting and ticket cleanup.
Its current-artifact client gameplay tests remain unqualified.

The requested CustomMerchant, CustomMachine, MapPrinter, VirtualStorage,
CustomWorkshop and other family custom gameplay consumers are still incomplete.
The table below records outstanding scenarios; a generic example is not screen
coverage. For the eleven NativePassthrough entries, original-mode orchestration
is implemented experimentally; the missing scenario is actual client regression
qualification plus separate custom behavior.

| Entry | Current selectable SDK request | Missing working scenario |
|---|---|---|
| `chest` | `prepare(player, "chest", mode)` | Original source behavior and per-feature custom behavior are not yet executable through the native adapter. |
| `doublechest` | `prepare(player, "doublechest", mode)` | Original source behavior and per-feature custom behavior are not yet executable through the native adapter. |
| `hopper` | `prepare(player, "hopper", mode)` | Original source behavior and per-feature custom behavior are not yet executable through the native adapter. |
| `dispenser` | `prepare(player, "dispenser", mode)` | Original source behavior and per-feature custom behavior are not yet executable through the native adapter. |
| `barrel` | `prepare(player, "barrel", mode)` | Original source behavior and per-feature custom behavior are not yet executable through the native adapter. |
| `dropper` | `prepare(player, "dropper", mode)` | Original source behavior and per-feature custom behavior are not yet executable through the native adapter. |
| `trappedchest` | `prepare(player, "trappedchest", mode)` | Original source behavior and per-feature custom behavior are not yet executable through the native adapter. |
| `enderchest` | `prepare(player, "enderchest", mode)` | Original source behavior and per-feature custom behavior are not yet executable through the native adapter. |
| `shulker` | `prepare(player, "shulker", VCF_REAL_SOURCE)` | [Linux held editor](NATIVE_HELD_STORAGE.md): selected command/transfer/cancel checks passed; interaction binding refuses equipment sync. Custom behavior and full qualification remain incomplete. |
| `craft` | `prepare(player, "craft", mode)` | Original source behavior and per-feature custom behavior are not yet executable through the native adapter. |
| `anvil` | `prepare(player, "anvil", mode)` | Original source behavior and per-feature custom behavior are not yet executable through the native adapter. |
| `grindstone` | `prepare(player, "grindstone", mode)` | Original source behavior and per-feature custom behavior are not yet executable through the native adapter. |
| `smithing` | `prepare(player, "smithing", mode)` | Original source behavior and per-feature custom behavior are not yet executable through the native adapter. |
| `stonecutter` | `prepare(player, "stonecutter", mode)` | Original source behavior and per-feature custom behavior are not yet executable through the native adapter. |
| `loom` | `prepare(player, "loom", mode)` | Original source behavior and per-feature custom behavior are not yet executable through the native adapter. |
| `cartography` | `prepare(player, "cartography", mode)` | Original source behavior and per-feature custom behavior are not yet executable through the native adapter. |
| `enchanting` | `prepare(player, "enchanting", mode)` | Original source behavior and per-feature custom behavior are not yet executable through the native adapter. |
| `furnace` | `prepare(player, "furnace", mode)` | Original source behavior and per-feature custom behavior are not yet executable through the native adapter. |
| `blastfurnace` | `prepare(player, "blastfurnace", mode)` | Original source behavior and per-feature custom behavior are not yet executable through the native adapter. |
| `smoker` | `prepare(player, "smoker", mode)` | Original source behavior and per-feature custom behavior are not yet executable through the native adapter. |
| `brewing` | `prepare(player, "brewing", mode)` | Original source behavior and per-feature custom behavior are not yet executable through the native adapter. |
| `beacon` | `prepare(player, "beacon", mode)` | Original source behavior and per-feature custom behavior are not yet executable through the native adapter. |
| `crafter` | `prepare(player, "crafter", mode)` | Original source behavior and per-feature custom behavior are not yet executable through the native adapter. |
| `lectern` | `prepare(player, "lectern", mode)` | Original source behavior and per-feature custom behavior are not yet executable through the native adapter. |
| `writtenbook` | `prepare(player, "writtenbook", mode)` | Original source behavior and per-feature custom behavior are not yet executable through the native adapter. |
| `bookediting` | `prepare(player, "bookediting", mode)` | Original source behavior and per-feature custom behavior are not yet executable through the native adapter. |
| `villager` | `prepare(player, "villager", mode)` | Original source behavior and per-feature custom behavior are not yet executable through the native adapter. |
| `wanderingtrader` | `prepare(player, "wanderingtrader", mode)` | Original source behavior and per-feature custom behavior are not yet executable through the native adapter. |
| `horse` | `prepare(player, "horse", mode)` | Original source behavior and per-feature custom behavior are not yet executable through the native adapter. |
| `donkey` | `prepare(player, "donkey", mode)` | Original source behavior and per-feature custom behavior are not yet executable through the native adapter. |
| `mule` | `prepare(player, "mule", mode)` | Original source behavior and per-feature custom behavior are not yet executable through the native adapter. |
| `llama` | `prepare(player, "llama", mode)` | Original source behavior and per-feature custom behavior are not yet executable through the native adapter. |
| `traderllama` | `prepare(player, "traderllama", mode)` | Original source behavior and per-feature custom behavior are not yet executable through the native adapter. |
| `chestboat` | `prepare(player, "chestboat", mode)` | Original source behavior and per-feature custom behavior are not yet executable through the native adapter. |
| `chestminecart` | `prepare(player, "chestminecart", mode)` | Original source behavior and per-feature custom behavior are not yet executable through the native adapter. |
| `hopperminecart` | `prepare(player, "hopperminecart", mode)` | Original source behavior and per-feature custom behavior are not yet executable through the native adapter. |
| `camel` | `prepare(player, "camel", mode)` | Original source behavior and per-feature custom behavior are not yet executable through the native adapter. |
| `camelhusk` | `prepare(player, "camelhusk", mode)` | Original source behavior and per-feature custom behavior are not yet executable through the native adapter. |
| `zombiehorse` | `prepare(player, "zombiehorse", mode)` | Original source behavior and per-feature custom behavior are not yet executable through the native adapter. |
| `skeletonhorse` | `prepare(player, "skeletonhorse", mode)` | Original source behavior and per-feature custom behavior are not yet executable through the native adapter. |
| `nautilus` | `prepare(player, "nautilus", mode)` | Original source behavior and per-feature custom behavior are not yet executable through the native adapter. |
| `zombienautilus` | `prepare(player, "zombienautilus", mode)` | Original source behavior and per-feature custom behavior are not yet executable through the native adapter. |
| `allay` | `prepare(player, "allay", mode)` | Original source behavior and per-feature custom behavior are not yet executable through the native adapter. |
| `panda` | `prepare(player, "panda", mode)` | Original source behavior and per-feature custom behavior are not yet executable through the native adapter. |
| `piglin` | `prepare(player, "piglin", mode)` | Original source behavior and per-feature custom behavior are not yet executable through the native adapter. |
| `agent` | `prepare(player, "agent", mode)` | Original source behavior and per-feature custom behavior are not yet executable through the native adapter. |
| `sign` | `prepare(player, "sign", mode)` | Original source behavior and per-feature custom behavior are not yet executable through the native adapter. |
| `npc` | `prepare(player, "npc", mode)` | Original source behavior and per-feature custom behavior are not yet executable through the native adapter. |
| `commandblock` | `prepare(player, "commandblock", mode)` | Original source behavior and per-feature custom behavior are not yet executable through the native adapter. |
| `commandblockminecart` | `prepare(player, "commandblockminecart", mode)` | Original source behavior and per-feature custom behavior are not yet executable through the native adapter. |
| `structure` | `prepare(player, "structure", mode)` | Original source behavior and per-feature custom behavior are not yet executable through the native adapter. |
| `jigsaw` | `prepare(player, "jigsaw", mode)` | Original source behavior and per-feature custom behavior are not yet executable through the native adapter. |
| `compoundcreator` | `prepare(player, "compoundcreator", mode)` | Original source behavior and per-feature custom behavior are not yet executable through the native adapter. |
| `elementconstructor` | `prepare(player, "elementconstructor", mode)` | Original source behavior and per-feature custom behavior are not yet executable through the native adapter. |
| `materialreducer` | `prepare(player, "materialreducer", mode)` | Original source behavior and per-feature custom behavior are not yet executable through the native adapter. |
| `labtable` | `prepare(player, "labtable", mode)` | Original source behavior and per-feature custom behavior are not yet executable through the native adapter. |
| `inventory2x2` | `prepare(player, "inventory2x2", mode)` | Original source behavior and per-feature custom behavior are not yet executable through the native adapter. |
| `armor` | `prepare(player, "armor", mode)` | Original source behavior and per-feature custom behavior are not yet executable through the native adapter. |
| `offhand` | `prepare(player, "offhand", mode)` | Original source behavior and per-feature custom behavior are not yet executable through the native adapter. |
| `cursor` | `prepare(player, "cursor", mode)` | Original source behavior and per-feature custom behavior are not yet executable through the native adapter. |
| `recipebook` | `prepare(player, "recipebook", mode)` | Original source behavior and per-feature custom behavior are not yet executable through the native adapter. |
| `bundle` | `prepare(player, "bundle", mode)` | Original source behavior and per-feature custom behavior are not yet executable through the native adapter. |
| `hud` | `prepare(player, "hud", mode)` | Original source behavior and per-feature custom behavior are not yet executable through the native adapter. |
| `cauldron` | `prepare(player, "cauldron", mode)` | Original source behavior and per-feature custom behavior are not yet executable through the native adapter. |
| `jukebox` | `prepare(player, "jukebox", mode)` | Original source behavior and per-feature custom behavior are not yet executable through the native adapter. |
| `decoratedpot` | `prepare(player, "decoratedpot", mode)` | Original source behavior and per-feature custom behavior are not yet executable through the native adapter. |
| `fletching` | `prepare(player, "fletching", mode)` | Original source behavior and per-feature custom behavior are not yet executable through the native adapter. |
| `campfire` | `prepare(player, "campfire", mode)` | Original source behavior and per-feature custom behavior are not yet executable through the native adapter. |
| `composter` | `prepare(player, "composter", mode)` | Original source behavior and per-feature custom behavior are not yet executable through the native adapter. |
