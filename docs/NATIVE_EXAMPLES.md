# Native examples and how-tos

Install the [provider and matching example consumers](NATIVE_INSTALLATION.md). They use the public SDK and report actual capabilities; a listed entry may return a typed refusal.

## Explore the catalog

Use `/vcf` and the [SDK form walkthrough](examples/linux-native-sdk.md). The catalog retains all 69 IDs. A disabled or refused entry is an implementation gap.

## Original workstation and interaction

Enable the platform's original-adapter flag and run `/vcf_native anvil`. Linux also admits `craft`, `grindstone`, `smithing`, `stonecutter`, `loom`, `cartography` and `enchanting`. Windows does not yet admit enchanting. BDS owns the recipes and outputs.

Run `/vcf_native bind anvil`, hold a compass, then sneak and right-click. `/vcf_native unbind` clears the binding. [Workstation screenshots](examples/linux-native-workstations.md) retain individual checks and the unresolved grindstone output case.

## Existing sources

```text
/vcf_native enderchest "2,82,5"
/vcf_native barrel "2,82,4"
/vcf_native furnace "2,81,5"
```

Replace coordinates with nearby loaded, authorized blocks in the player's current dimension; retain the quotes. Ender Chest access uses that player's actual Ender inventory. These requests do not create replacement sources.

See [storage](examples/linux-linked-storage.md), [chests](examples/linux-linked-chests.md), [hopper](examples/linux-linked-hopper.md), [furnaces](examples/linux-linked-furnaces.md) and [utilities](examples/linux-linked-utilities.md).

## Actual held shulker

Select an existing shulker in the hotbar and run `/vcf_native shulker` with both Linux held-storage/write flags enabled. The source slot is locked. Completed transfers journal the player inventory and that box's contents together.

`/vcf_native bind shulker` binds the example to sneak-and-use with the held box. Read [the held-storage contract](NATIVE_HELD_STORAGE.md) and use disposable test items.

## Your consumer plugin

Link `OnistoneVCF::sdk`, include `oni/vcf/sdk.hpp` and `oni/vcf/loader.hpp`, and declare `depend = {"onistone_vcf"}`. Keep one `sdk::Client` alive for your enabled plugin. [NativePassthrough](../examples/native/native_passthrough.cpp) is the complete compiled reference.

```cpp
auto request = oni::vcf::sdk::descriptor<vcf_session_desc>();
request.player = oni::vcf::sdk::view(player_uuid);
request.canonical_id = oni::vcf::sdk::view("shulker");
request.mode = VCF_REAL_SOURCE;
request.source_permission = oni::vcf::sdk::view("my_plugin.shulker");
request.callback = on_ui_event; // vcf_status VCF_CALL(void*, const vcf_event*)
request.context = this;
vcf_handle ticket = 0;
oni::vcf::sdk::checked(ui.api().prepare(ui.owner(), &request, &ticket));
ui.open(ticket);
```

Observe OPEN, COMMIT, CLOSE and FAILURE events. Release terminal tickets with `forget`. COMMIT reports a completed increasing inventory revision; it does not authorize replay or prove BDS disk durability. Callbacks may request closure. Keep callback contexts alive until the consumer is released.

Forms/actions have their own implemented SDK contract. Custom inventory recipes, enchantments and machine outputs are not integrated across the catalog. `set_item` is refused for `VCF_REAL_SOURCE`. See [SDK 1.5](NATIVE_SDK.md) and [per-entry custom gaps](ALL_UI_EXAMPLES.md).


**Known prerelease issue:** the held-shulker sneak-and-use binding currently refuses an equipment synchronization packet during preparation. Use the tested `/vcf_native shulker` command. No inventory transfer is performed by the refused opening.
