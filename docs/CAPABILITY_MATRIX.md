# Native capability matrix

This matrix describes the new C++ artifact only. **Full scope: NOT QUALIFIED.**

The current Windows provider SHA-256 is `07b037e42a832571448022fdea6385b2db08e0839a9e5b611965aeac500e3dfb`. It and both SDK consumers passed [startup and clean shutdown](../research/native-evidence/windows-171e0aa-startup-smoke.json), including loaded-file identities and the native primitive manifest. Stock-client UI tests for this exact Windows build remain outstanding. No catalog screen has completed gameplay qualification under the native Windows plugin.

The Linux Docker build passes locally and in CI and produces a real ELF `.so`. Exact local BDS/Endstone files are admitted for the public SDK. Stock-client forms, passive inventory observation, four shared inventory roles and eight opt-in original workstation contexts have [smoke evidence](examples/linux-native-workstations.md). Three nearby linked machines also have [processing, delivery and cleanup smoke evidence](examples/linux-linked-furnaces.md). Linked Ender Chest and barrel [transfers and cleanup](examples/linux-linked-storage.md) also passed selected PC checks. Controlled ingredient-disconnect checks passed. Grindstone's initial output rejection remains unresolved despite successful repeats; the older missing-plank/death observation remains in its original record. Five [linked utilities](examples/linux-linked-utilities.md) have selected gameplay evidence; the later `d6adf87` build passed repeated beacon/crafter SDK closure and selected restoration, with initial unaccepted inputs retained as limitations. Other Linux catalog adapters remain incomplete. These results do not upgrade any catalog row to full qualification.

| Entry | Family | Windows original/custom | Linux original/custom |
|---|---|---|---|
| `chest` | storage | Not qualified / not implemented | Blocked / not implemented |
| `doublechest` | storage | Not qualified / not implemented | Blocked / not implemented |
| `hopper` | storage | Not qualified / not implemented | Blocked / not implemented |
| `dispenser` | storage | Not qualified / not implemented | Experimental nearby linked source, PC transfers/metadata/closure smoke passed / not implemented |
| `barrel` | storage | Not qualified / not implemented | Experimental nearby linked source, PC transfers/metadata/cleanup smoke passed / not implemented |
| `dropper` | storage | Not qualified / not implemented | Experimental nearby linked source, PC transfers/closure smoke passed / not implemented |
| `trappedchest` | storage | Not qualified / not implemented | Blocked / not implemented |
| `enderchest` | storage | Not qualified / not implemented | Experimental nearby linked source, PC transfers/metadata/cleanup smoke passed / not implemented |
| `shulker` | storage | Not qualified / not implemented | Blocked / not implemented |
| `craft` | workstation | Experimental original adapter, client untested / not implemented | Experimental original, PC 3Ãƒâ€”3 output/closure/reconnect smoke passed / not implemented |
| `anvil` | workstation | Experimental original adapter, client untested / not implemented | Experimental original, PC rename/cost smoke passed / not implemented |
| `grindstone` | workstation | Experimental original adapter, client untested / not implemented | Experimental original, initial output rejection unresolved; repeats passed / not implemented |
| `smithing` | workstation | Experimental original adapter, client untested / not implemented | Experimental original, PC upgrade smoke passed / not implemented |
| `stonecutter` | workstation | Experimental original adapter, client untested / not implemented | Experimental original, PC selection/output smoke passed / not implemented |
| `loom` | workstation | Experimental original adapter, client untested / not implemented | Experimental original, PC pattern/output smoke passed / not implemented |
| `cartography` | workstation | Experimental original adapter, client untested / not implemented | Experimental original, PC map expansion smoke passed / not implemented |
| `enchanting` | workstation | Not qualified / not implemented | Experimental original context, PC offer/cost/delivery and permission-loss smoke passed; linked mode incomplete / not implemented |
| `furnace` | workstation | Not qualified / not implemented | Experimental nearby linked source, PC processing/delivery/closure smoke passed / not implemented |
| `blastfurnace` | workstation | Not qualified / not implemented | Experimental nearby linked source, PC processing/delivery/closure smoke passed / not implemented |
| `smoker` | workstation | Not qualified / not implemented | Experimental nearby linked source, PC processing/delivery/closure and permission-loss retention smoke passed / not implemented |
| `brewing` | workstation | Not qualified / not implemented | Experimental nearby linked source, PC brewing/output/closure smoke passed / not implemented |
| `beacon` | workstation | Not qualified / not implemented | Experimental nearby linked source, PC payment/effect and later repeated SDK close/payment-return smoke passed / not implemented |
| `crafter` | workstation | Not qualified / not implemented | Experimental nearby linked source, PC toggles/output and later repeated SDK close/metadata restoration passed; one Shift-click attempt unresolved / not implemented |
| `lectern` | contextual | Not qualified / not implemented | Blocked / not implemented |
| `writtenbook` | contextual | Not qualified / not implemented | Blocked / not implemented |
| `bookediting` | contextual | Not qualified / not implemented | Blocked / not implemented |
| `villager` | contextual | Not qualified / not implemented | Blocked / not implemented |
| `wanderingtrader` | contextual | Not qualified / not implemented | Blocked / not implemented |
| `horse` | contextual | Not qualified / not implemented | Blocked / not implemented |
| `donkey` | contextual | Not qualified / not implemented | Blocked / not implemented |
| `mule` | contextual | Not qualified / not implemented | Blocked / not implemented |
| `llama` | contextual | Not qualified / not implemented | Blocked / not implemented |
| `traderllama` | contextual | Not qualified / not implemented | Blocked / not implemented |
| `chestboat` | contextual | Not qualified / not implemented | Blocked / not implemented |
| `chestminecart` | contextual | Not qualified / not implemented | Blocked / not implemented |
| `hopperminecart` | contextual | Not qualified / not implemented | Blocked / not implemented |
| `camel` | contextual | Not qualified / not implemented | Blocked / not implemented |
| `camelhusk` | contextual | Not qualified / not implemented | Blocked / not implemented |
| `zombiehorse` | contextual | Not qualified / not implemented | Blocked / not implemented |
| `skeletonhorse` | contextual | Not qualified / not implemented | Blocked / not implemented |
| `nautilus` | contextual | Not qualified / not implemented | Blocked / not implemented |
| `zombienautilus` | contextual | Not qualified / not implemented | Blocked / not implemented |
| `allay` | contextual | Not qualified / not implemented | Blocked / not implemented |
| `panda` | contextual | Not qualified / not implemented | Blocked / not implemented |
| `piglin` | contextual | Not qualified / not implemented | Blocked / not implemented |
| `agent` | contextual | Not qualified / not implemented | Blocked / not implemented |
| `sign` | editor | Not qualified / not implemented | Blocked / not implemented |
| `npc` | editor | Not qualified / not implemented | Blocked / not implemented |
| `commandblock` | editor | Not qualified / not implemented | Blocked / not implemented |
| `commandblockminecart` | editor | Not qualified / not implemented | Blocked / not implemented |
| `structure` | editor | Not qualified / not implemented | Blocked / not implemented |
| `jigsaw` | editor | Not qualified / not implemented | Blocked / not implemented |
| `compoundcreator` | education | Not qualified / not implemented | Blocked / not implemented |
| `elementconstructor` | education | Not qualified / not implemented | Blocked / not implemented |
| `materialreducer` | education | Not qualified / not implemented | Blocked / not implemented |
| `labtable` | education | Not qualified / not implemented | Blocked / not implemented |
| `inventory2x2` | embedded-or-interaction | Experimental original adapter, client untested / not implemented | Experimental original, PC opening/cancellation smoke passed / not implemented |
| `armor` | embedded-or-interaction | Experimental original adapter, client untested / not implemented | Experimental original, PC equipment smoke passed / not implemented |
| `offhand` | embedded-or-interaction | Experimental original adapter, client untested / not implemented | Experimental original, PC equipment smoke passed / not implemented |
| `cursor` | embedded-or-interaction | Not qualified / not implemented | Blocked / not implemented |
| `recipebook` | embedded-or-interaction | Experimental original adapter, client untested / not implemented | Experimental original, PC selection smoke passed / not implemented |
| `bundle` | embedded-or-interaction | Not qualified / not implemented | Blocked / not implemented |
| `hud` | embedded-or-interaction | Not qualified / not implemented | Blocked / not implemented |
| `cauldron` | embedded-or-interaction | Not qualified / not implemented | Blocked / not implemented |
| `jukebox` | embedded-or-interaction | Not qualified / not implemented | Blocked / not implemented |
| `decoratedpot` | embedded-or-interaction | Not qualified / not implemented | Blocked / not implemented |
| `fletching` | embedded-or-interaction | Not qualified / not implemented | Blocked / not implemented |
| `campfire` | embedded-or-interaction | Not qualified / not implemented | Blocked / not implemented |
| `composter` | embedded-or-interaction | Not qualified / not implemented | Blocked / not implemented |
