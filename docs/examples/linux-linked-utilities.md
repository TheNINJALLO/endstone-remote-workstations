# Linked utility-container SDK examples

The experimental Linux adapter accepts `dispenser`, `dropper`, `brewing`,
`beacon` and `crafter` with `VCF_REAL_SOURCE`. Each has a separate fingerprinted
factory and verified native model identity. Stock-client tests are pending.
Windows admission and custom gameplay remain incomplete.

Install the provider and native passthrough consumer with the
[Linux setup](linux-native-workstations.md#install-and-open). Enable
`experimental_original_linux` on the exact admitted Endstone 0.11.10 /
BDS 1.26.45.1 fixture. Replace the following positions with your authorized
existing blocks; retain the literal quotes:

```text
/vcf_native dispenser "2,83,5"
/vcf_native dropper "2,83,6"
/vcf_native brewing "2,83,7"
/vcf_native beacon "2,87,5"
/vcf_native crafter "2,83,8"
/vcf_native close
```

Use the [copied SDK source descriptor](linux-linked-furnaces.md#dependent-plugin)
with an explicit dimension, position and source permission. The source must
be within six blocks, in the player's current dimension and a loaded chunk.
Wrong-family, missing or changed sources refuse or close. Consumers must
keep callbacks alive until revocation completes and forget terminal tickets.

These are real block inventories and vanilla behaviors. Brewing uses the
real stand's fuel, bottles and ingredients. A real beacon retains its pyramid,
sky access, payment and world effects. Crafter slot controls affect the real
crafter; crafting requires its ordinary external trigger. Dispenser and dropper
world actions remain subject to native redstone behavior. The example creates
no blocks and does not activate a machine merely to demonstrate a custom API.

Crafter slot-toggle requests for the owned position receive source and
permission checks before BDS processes them. No packet is redirected to another
block. Preloaded custom items, policy changes, recipes, titles and unrelated
held/entity descriptors refuse. Custom processors, routing and source aliases
remain incomplete.

The [Linux ABI manifest](../../research/native-evidence/linux-linked-utility-abi-126451.json)
records the five distinct factory hashes, caller branches and model RTTI.
Hopper has a different context-creation path and is not admitted by these
factories. An original view or successful packet send does not qualify custom
transactions, recovery or the full catalog.
