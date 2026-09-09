# NativePassthrough original-mode regression example

This standalone native plugin uses only the installed public SDK and Endstone's
public API. It compiles separately from the provider. Its original-mode requests
are experimental and do not demonstrate custom station gameplay.

1. Build the native artifacts and copy `endstone_onistone_vcf.dll` and
   `endstone_vcf_native_passthrough.dll` to an isolated Windows server's plugins
   directory. No project wheel is needed. Linux uses the corresponding `.so`
   files and admits eight workstation contexts, four shared inventory roles and
   three nearby linked furnace-family sources, plus linked Ender Chest and barrel;
   see [the Linux example](../../docs/examples/linux-native-workstations.md).
2. After first startup, stop the server and set
   `experimental_original_windows` (or `experimental_original_linux`) to `true` in
   `plugins/onistone_vcf/config.json`. Restart with the exact admitted BDS and
   Endstone binaries. The configuration defaults to disabled.
3. Join using Windows Bedrock 1.26.45 and grant the tester
   `vcf.examples.passthrough` plus the original screen permission. The example's
   permission defaults to operator; it never grants operator privileges.
4. Run `/vcf_native anvil`. The SDK returns a queued ticket; the example reports
   open/close/failure events only when the provider completes them.
5. Run `/vcf_native bind anvil`. While holding an ordinary compass, sneak and
   right-click to request the same screen through a player interaction.
   `/vcf_native unbind` removes that opt-in binding. The example never replaces
   inventory contents or gives itself control of unrelated compass use.
6. Inspect `/vcf_native status` for open, close, refusal and guard callback counts.
   Test each screen separately: `craft`, `anvil`, `grindstone`, `smithing`,
   `stonecutter`, `loom`, `cartography`, `inventory2x2`, `armor`, `offhand`,
   `recipebook`, and `enchanting` (Linux only). Linux also accepts
   `/vcf_native furnace "2,81,5"`, `/vcf_native blastfurnace "2,81,4"` and
   `/vcf_native smoker "2,81,6"`. Replace those positions with your authorized
   existing blocks and retain the literal quotes. Linux also accepts
   `/vcf_native enderchest "2,82,5"` and `/vcf_native barrel "2,82,4"` through
   the [linked-storage adapter](../../docs/examples/linux-linked-storage.md).
   The [linked utility examples](../../docs/examples/linux-linked-utilities.md)
   add Linux `dispenser`, `dropper`, `brewing`, `beacon` and `crafter`, each
   requiring its own explicit source. The [linked hopper adapter](../../docs/examples/linux-linked-hopper.md)
   adds a separate five-slot source path with selected PC transfer, metadata,
   SDK/native closure and source-lifecycle evidence. Three new
   [linked chest paths](../../docs/examples/linux-linked-chests.md) add `chest`,
   `trappedchest` and `doublechest`, with client testing pending. There are 26
   requests, each subject to exact-build admission and qualification.

The operator-only `/vcf_native guard-deny "overworld|2,91,7"` demonstration
denies that source position for this consumer's tickets, including a double
chest requested through its other half. `/vcf_native guard-clear` resets this
in-memory policy. The commands also work from the server console. Replace the
dimension with its exact Endstone name. Production protection plugins should
register their own SDK guards for every source position they protect.

Workstation requests explicitly choose `VCF_NATIVE_CONTEXT`, leaving original
recipes and items owned by BDS. The four player roles explicitly choose
`VCF_REAL_SOURCE`; they share client-owned inventory navigation and closure.
No separate armor/offhand tab selection is promised.

The three Linux linked machines also use `VCF_REAL_SOURCE`, with an explicit
dimension, position and permission. See [the linked-source example](../../docs/examples/linux-linked-furnaces.md)
for source guards and actual processing/cleanup checks. They cannot be bound
to the example compass trigger, which has no source configuration. Windows
does not yet admit these three linked adapters.

For each entry, record the installed provider and consumer SHA-256, client/input
profile, actual ingredient/result behavior, cursor/grid cleanup, rapid reopen,
teleport/disconnect, permission loss, and competing plugin openings. A command
response or counter is not rendering evidence. Current per-entry qualification
is in `research/native-evidence/`; this procedure is not a claim those client
tests passed. Custom recipes, maps, merchants and valuable-item recovery require
their separate implementations and tests.
