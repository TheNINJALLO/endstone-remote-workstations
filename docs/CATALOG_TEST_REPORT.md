> **Legacy v0.4.0 documentation.** This page describes the Python/native-companion implementation. For the native prerelease, use [installation](NATIVE_INSTALLATION.md), [examples](NATIVE_EXAMPLES.md) and [SDK 1.5](NATIVE_SDK.md).

# Catalog candidate 0.4.0a24

All 69 requested entries have an explicit source contract and disposition.
The current source implements 53 entry points; 16 remain unavailable or have no
standalone vanilla UI. Installed a24 passed every implemented entry point's
screen/lifecycle check. Gameplay qualification remains partial; this is an alpha.
The generated [catalog](CAPABILITIES.md) records each interface separately.

The admitted host is Endstone 0.11.10, BDS 1.26.45.1, protocol 2169, CPython 3.11
on Windows x86-64. Live tests use Windows Bedrock 1.26.45 Classic UI and mouse/keyboard.
The public Endstone API baseline is 0.11.0. Linux runtime and other clients are unqualified.

## Current changes and evidence

- Installed a24 passed **53/53 catalog lifecycle cases**: 47 in the standard
  isolated world and six in the Education clone (Agent, NPC and four chemistry
  screens). Every case preserved its pre-test inventory, auxiliary data, NBT,
  XP and position. Four embedded player-inventory views relinquished tracking
  before the player closed the native screen. Six reader/editor screens have
  separate reviewed HUD-close evidence because their cancellation does not
  produce a client ContainerClose acknowledgment. All 35 installed package
  files match the tested native wheel. Evidence:
  `research/catalog-evidence/installed-catalog-index.json`.
- The unchanged separate example plugin 0.2.0 passed seven dependency
  integration checks against a24: protected inventory pages, button callback,
  exported function invocation by another plugin, private action hiding/denial,
  native anvil navigation, actual Ender Chest navigation and complete cleanup.
  Five icon-transfer requests were refused while their intended callbacks ran.
  Final inventory, auxiliary data, NBT and XP were unchanged; no native, packet
  or API sessions remained. The expired first recording and an incorrect anvil
  recorder assertion are retained separately, with corrected repeat evidence.
  Evidence: `research/catalog-evidence/consumer-installed-a24-index.json`.
- a24 adds native riding-state and passenger-link reads for remote actor views.
  It temporarily detaches only the requesting client's passenger link and
  restores current native state on cleanup. Development catalog39 passed a real
  mounted merchant trade and preserved the server's boat relationship. Forty
  focused riding tests initially passed; configuration revocation brings the
  riding suite to 41 tests. Installed a24 passed eight mounted-merchant cases:
  real Survival trade/manual close, Creative open/cancel, mounted open/cancel,
  cursor cancellation, protection/configuration revocation, and native dismount
  with and without automatic reboarding. Ten native item results were accepted;
  exact unchanged items, auxiliary data, NBT and XP were preserved. All 35 installed
  package files match the wheel. The 565 automated tests, 95 console checks and
  17-command headless smoke test passed; both wheels rebuild identically. See
  [native riding views](NATIVE_RIDING_VIEWS.md).
- The actual held book, bundle and shulker use targets were inspected against
  the pinned headers and executable. Both book open calls are client-side gated.
  This adds ABI evidence, not held-item support; see the
  [item entry-point audit](NATIVE_ITEM_ENTRY_POINTS.md).
- The user scoped this qualification run to the available Windows client.
  Multiplayer and other device combinations remain untested.
- a23 adds the existing native Agent inventory to `open_entity`, with actual
  world features, existing owner and source identity checks. Each item request
  rechecks authorization; after revocation only bounded native cursor cleanup
  remains permitted. Development catalog34 passed a named enchanted item round
  trip through the actual Agent, including close/reopen. Installed a23 passed five
  selected Agent cases: transfer/reopen, cursor cancellation, owner revocation,
  denied unowned access and protection revocation. All preserved exact inventory,
  item auxiliary data and XP. All 524 tests, 95 console checks and 17 headless
  checks passed; both wheel rebuilds were identical. Full catalog/device/crash
  qualification remains open.
- a19 adds four native chemistry managers, a verified native world-feature read,
  scoped Lab Table controls, and per-request chemistry authorization checks.
  It also fixes merchant interaction dispatch across Creative/Survival GameMode variants.
- Installed a19 passed hydrogen creation, material reduction, 95 console checks
  and 17 headless checks. Its Compound Creator cancellation blocked the client's
  native input-return batch and did not immediately return 64 test hydrogen.
  An authoritative a20 snapshot confirms it returned after server restart/rejoin.
  This failed cancellation acceptance is
  preserved; a19 is not a usable release candidate.
- a20 permits bounded native input returns after cancellation/revocation while
  rejecting new crafting, combining, deposits and foreign managers. The actual
  failed close payload is a regression fixture. Installed a20 passed that exact
  Compound Creator cursor/input cancellation: native requests succeeded and the
  entire pre-test inventory, item auxiliary data and XP were restored immediately.
- a21 adds real native NPC dialogue to dependency API 1.5, using per-presentation
  scene tokens, source authorization and action bounds. Installed buttons/branching
  worked, but cancellation used an unsigned close discriminator and opened a blank
  dialogue. Native button close also prematurely retired the closing callback.
  Both failures are preserved; a21 is not a usable release candidate.
- a22 uses the captured native signed Close encoding and retains ordinary closing
  callback authority until the client's one matching request. Installed a22 passed
  native button commands, two-scene branching, API cancellation and protection
  revocation, with exact inventory/XP and native tag-state checks. Native tag removal
  avoids the pinned Endstone sparse-index removal defect. All 501 tests, 95 console
  checks and 17 headless checks passed; both wheel rebuilds were byte-identical.
- Installed a22 passed seven selected chemistry transaction/lifecycle checks:
  hydrogen creation, water output/input return, cobblestone reduction, reducer
  preview cancellation, Compound Creator cursor/input cancellation, and valid/
  invalid Lab Table combines. The valid recipe spawned one real ice bomb at its
  distant source. The invalid recipe selected native failure reaction 7; its
  garbage world output was not captured and remains unqualified. The two
  cancellations restored exact inventory, auxiliary data and XP. Evidence:
  `research/catalog-evidence/chemistry-installed-a22-index.json`.
- Development catalog31 passed five native chemistry transaction cases: hydrogen,
  water/input return, cobblestone reduction, invalid lab garbage and valid ice-bomb
  output. Physical compound/lab comparisons matched. See [chemistry](CHEMISTRY.md).
- Installed a18 passed 46 of its 47 entry-point open/lifecycle cases. Villager
  closed immediately after receiving offers. This historical failure is resolved
  in installed a24 by the scoped riding projection and tested Survival/Creative
  dispatch. The failed a18 evidence retains its original identity.
- Installed a18 passed the structure silent-Back cleanup regression. Installed a17
  previously passed real-source preview, Save, one-block Load and native export.
  Its failed silent-Back lease cleanup remains preserved as failed a17 evidence.
- Installed a15 command-block/cart edits and a9 sign edits have separate captures,
  including cancellation and authorization revocation. Each retains its wheel hash.

Local collectors preserve exact identities in chemistry-development-index.json,
structure-lifecycle-index.json, structure-preview-index.json, command-editor-index.json,
and sign-index.json under research/catalog-evidence. Raw captures and player data
are excluded from distributable artifacts. The [a8 historical report](CATALOG_TEST_REPORT_A8.md)
retains the earlier complete 42-entry sweep and selected subsystem transactions.
Historical passes are not silently promoted to later wheels.

## Outstanding work

Held shulker/bundle identity, escrow, nesting and durable writeback are absent.
Held book force-open/editing remains unbound. The native Agent inventory passed
five selected installed a23 cases. Broader NPC lifecycle stress and current a24
chemistry transaction qualification remain open; a22 has seven selected chemistry
cases, including an unverified invalid-Lab world output.
Several other catalog enums describe gameplay interactions or embedded inventories
without standalone vanilla screens; no replacement screen is advertised as vanilla.

Cross-store BDS/SQLite crash reconciliation, personal processing policy,
full-inventory recovery, all recipes/enchantments/merchant
variants and broad plugin coexistence remain unqualified. Native editor gates
also lack hardware shadow-stack and exception-path qualification. See
[recovery](RECOVERY.md), [native tests](TEST_REPORT.md), and [packet tests](PACKET_TEST_REPORT.md).
The command-block minecart's command text remained blank on open in both the
physical and remote comparisons; complete editor prefill parity is unqualified.

Default settings disable gameplay adapters. Completed native world changes are
not undone by closing a screen. A successful open is not complete vanilla parity
or production certification.
