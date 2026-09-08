# NativePassthrough original-mode regression example

This standalone native plugin uses only the installed public SDK and Endstone's
public API. It compiles separately from the provider. Its original-mode requests
are experimental and do not demonstrate custom station gameplay.

1. Build the native artifacts and copy `endstone_onistone_vcf.dll` and
   `endstone_vcf_native_passthrough.dll` to an isolated Windows server's plugins
   directory. No project wheel is needed. The Linux `.so` consumer builds too,
   but Linux private ABI admission currently refuses gameplay.
2. After first startup, stop the server and set
   `experimental_original_windows` to `true` in
   `plugins/onistone_vcf/config.json`. Restart with the exact admitted BDS and
   Endstone binaries. The configuration defaults to disabled.
3. Join using Windows Bedrock 1.26.45 and grant the tester
   `vcf.examples.passthrough` plus the original screen permission. The example's
   permission defaults to operator; it never grants operator privileges.
4. Run `/vcf_native craft`. The SDK returns a queued ticket; the example reports
   open/close/failure events only when the provider completes them.
5. Run `/vcf_native bind craft`. While holding an ordinary compass, sneak and
   right-click to request the same screen through a player interaction.
   `/vcf_native unbind` removes that opt-in binding. The example never replaces
   inventory contents or gives itself control of unrelated compass use.
6. Inspect `/vcf_native status` for open, close, refusal and guard callback counts.
   Test each screen separately: `craft`, `anvil`, `grindstone`, `smithing`,
   `stonecutter`, `loom`, `cartography`, `inventory2x2`, `armor`, `offhand`,
   `recipebook`.

The seven workstations explicitly choose `VCF_NATIVE_CONTEXT`, leaving original
recipes and items owned by BDS. The four player roles explicitly choose
`VCF_REAL_SOURCE`; they share client-owned inventory navigation and closure.
No separate armor/offhand tab selection is promised.

For each entry, record the installed provider and consumer SHA-256, client/input
profile, actual ingredient/result behavior, cursor/grid cleanup, rapid reopen,
teleport/disconnect, permission loss, and competing plugin openings. A command
response or counter is not rendering evidence. Current per-entry qualification
is in `research/native-evidence/`; this procedure is not a claim those client
tests passed. Custom recipes, maps, merchants and valuable-item recovery require
their separate implementations and tests.
