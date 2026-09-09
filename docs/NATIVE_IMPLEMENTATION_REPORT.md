# Native implementation checkpoint â€” incomplete

The latest recorded native source checkpoint is `a3fbf78f77640ac29757986bfa7532bde3789376` on
`feature/native-virtual-container-framework`. **The all-UI migration remains
incomplete and is not a production-qualified release.** RemoteWorkstations
0.4.0 and `main` remain separate from this development work.

SDK 1.3 adds [native saved-item reads](NATIVE_ITEM_SAVE.md). The exact public
Linux binaries passed nine client reads, including a bundle containing six
stones, signed-book and shulker saves, two expected empty-slot refusals and
byte-identical bundle restoration. The run stopped cleanly. Current-artifact
chest/form regressions remain unrun because Windows blocked Minecraft focus.
The Windows saved-item adapter, held editors, writeback and recovery remain
incomplete. See the [runtime record](../research/native-evidence/linux-a3fbf78-saved-item-smoke.json).

SDK 1.2 adds [read-only held inspection](NATIVE_HELD_ITEMS.md), with selected
shulker and book PC evidence. The initial bundle test exposed unchanged metadata
despite stored contents; the corrected code refuses bundle snapshots pending
complete source access. Native held editors, writeback and recovery remain
incomplete. The corrected public build passed two bundle refusals, five supported
held inspections, the double-chest partner guard/SDK-close regression and a form
callback, with no outstanding tickets. Its passive packet observer reproduced
the earlier refusal immediately after login, before new item operations. See
the [recorded run](../research/native-evidence/linux-f1bbfbd-held-smoke.json).
A later [private diagnostic](../research/native-evidence/linux-login-dynamic-container-diagnostic.json)
identified a 64-slot dynamic registry packet rejected by the old universal
54-slot limit. The parser correction preserves the separate window and the
player baseline. The [public-only `170c27b` run](../research/native-evidence/linux-170c27b-dynamic-smoke.json)
then retained one registry/one complete player snapshot/zero refusals at login,
through insertion and extraction of six real bundle contents, and during native
double-chest and form regressions. Both empty/filled bundle inspection calls
still refused safely. All six stones returned and no UI tickets remained open.

## What now runs locally

Docker Desktop 4.90.0 and WSL 2.7.13 now provide a working Linux x86-64 engine
on the Windows development PC. The pinned Clang 20.1.8 image builds the real
native `.so`, runs its tests, inspects the supplied Linux ELF files, and runs
the isolated Endstone 0.11.10 / BDS 1.26.45.1 server on `127.0.0.1:29179`.
The operator explicitly confirmed the supplied server's license before startup.

The exact Linux provider and both independent SDK consumers load and enable.
A stock Windows Bedrock 1.26.45 client joins the server. The public SDK action
form renders and dispatches the consumer's callback. All 69 catalog IDs resolve.
The earlier `b0ebf22` inventory observer reported one registry, one complete
snapshot and zero refusals. Both earlier held-inspection runs instead ended with
zero registries/snapshots and one refusal. The corrected `170c27b` run restored
the complete baseline with zero refusals across the tested operations.
The observer does not write native inventories.

The opt-in Linux original inventory adapter now opens `inventory2x2`, `armor`,
`offhand` and `recipebook` through independently derived and fingerprinted Linux
ABI facts. It also admits eight original workstation contexts: crafting, anvil, smithing,
stonecutter, grindstone, loom, cartography and enchanting. The earlier `ca30255` build completed
15 opens and 15 closes with zero failure callbacks and outstanding tickets.
PC tests exercised vanilla transformations, equipment, recipe selection,
SDK cancellation and a held-compass interaction binding. The client/BDS retain
real item ownership. Custom workstation logic remains incomplete.

On Peaceful difficulty, six unused crafting-grid planks and four crafted sticks
survived disconnect/reconnect in `ca30255`. An unfinished stonecutter input also
returned on reconnect. Grindstone's first plain-click extraction did not deliver
its preview; Shift-click and later plain-click repeats succeeded on both test
swords. The original rejection remains unresolved. See the
[workstation example](examples/linux-native-workstations.md) for exact observations.
The older `171e0aa` [inventory example](examples/linux-native-inventory.md) retains
its separate missing-plank observation followed by death and subsequent controlled
passes. Neither checkpoint establishes complete death/drop or crash recovery.

The exact `7aacb36` crafting build passed a 3Ã—3 wooden-pickaxe recipe with
ordinary-click and Shift-click output, unused ingredients returned on SDK
closure, compass opening, ingredient disconnect/reconnect and `/workbench`.
Stonecutter, shared inventory and the form action also passed regression checks.
Four example opens/closes plus the provider-owned alias ticket returned to zero
provider sessions, with a clean server shutdown. The
[crafting record](../research/native-evidence/linux-7aacb36-crafting-smoke.json)
identifies this run separately from the earlier six-workstation results.

The exact `e0dbb78` enchanting build delivered Efficiency I on a wooden
pickaxe for one lapis and one level. SDK closure, native X closure and
intentional consumer-permission loss returned unused inputs. Restoring the
permission allowed another opening. Anvil renaming preserved the enchantment;
crafting, `/etable` and the SDK form also passed their recorded checks. Five
example opens ended in four normal closes and one expected permission-denial
failure, with zero outstanding tickets or provider sessions and a clean
shutdown. The [enchanting record](../research/native-evidence/linux-e0dbb78-enchanting-smoke.json)
includes six actual screenshots. Linked real-table access, custom offers and
crash/save recovery remain incomplete.

The exact `7a938e5` linked-source build processed and delivered three stone,
three iron ingots and three cooked beef through the actual furnace, blast
furnace and smoker screens. SDK closure while processing, reopening and native
closure passed. Wrong-family and distant-source requests refused. Removing
the empty furnace closed its view; permission loss closed the smoker while
retaining its one unfueled raw-beef input for recovery after access returned.
Enchanting and crafting SDK-close regressions and a form callback also passed.
Twelve opens ended in ten normal closes and two active denials, plus two
refusals before opening; all tickets and sessions retired before clean shutdown.
The [linked-machine example](examples/linux-linked-furnaces.md) includes actual
screenshots and the [exact record](../research/native-evidence/linux-7a938e5-furnace-smoke.json).
The earlier `26528a7` SDK-close timeout exposed a ContainerClose type mismatch;
this build sends the active type and passed the follow-up checks. Custom
processing, source-configuration aliases and crash/save recovery remain incomplete.

The exact `1d88dfa` linked-storage build passed Ender Chest and barrel
first/last-slot transfers. The named Efficiency I pickaxe retained its metadata
when recovered through a second Ender source and after a barrel disconnect.
Ender permission loss and source removal closed their views while preserving
the player's retained input. Wrong-family barrel access refused. Nine opens
ended in seven normal closes and two active denials, plus one refusal before
opening; zero tickets or sessions remained at clean shutdown. Empty-furnace
closure and the SDK form passed regression checks. The [storage example](examples/linux-linked-storage.md)
and [exact record](../research/native-evidence/linux-1d88dfa-storage-smoke.json)
include nine actual screenshots. Source-free Ender access, custom storage,
virtual vaults and save/crash recovery remain incomplete.

The [exact `961e9ec` utility run](../research/native-evidence/linux-961e9ec-utilities-smoke.json)
passed dispenser/dropper transfers, brewing three awkward potions, one-ingot
beacon payment/Haste, persisted crafter edge toggles and a redstone-triggered
one-log/four-plank recipe. **Beacon and crafter SDK closure failed**, triggering
five-second disconnect quarantines. Reconnect returned the unused beacon ingot
and retained crafter settings. Native closure passed. Ten opens ended in eight
normal closes and two quarantines, plus one wrong-family refusal; zero sessions
remained at clean shutdown. See the [screenshots and SDK example](examples/linux-linked-utilities.md).

The later [exact `d6adf87` close/restore run](../research/native-evidence/linux-d6adf87-close-smoke.json)
used only the three public plugins. Beacon returned unused payments on two SDK
closes and on intentional permission loss; repeat opens retained Haste and
accepted payment input. Crafter retained disabled edge slots and an enchanted,
named pickaxe across SDK closure. Ordinary withdrawal returned its metadata;
a second close and third opening retained re-enabled controls. Removing the
verified-empty source closed with the expected denial. Empty dispenser, dropper
and brewing close regressions and the form callback passed. Nine opens ended in
seven normal closes and two expected denials, zero tickets and clean shutdown.
One crafter Shift-click attempt had no effect, and an initial beacon sequence
needed cursor-state inspection before successful deposit. These observations
remain limitations; this run does not qualify every input path or custom mode.

The [exact `9a656e3` hopper run](../research/native-evidence/linux-9a656e3-hopper-smoke.json)
passed first/last-slot transfers, native contents on reopen, name/enchantment
retention, SDK and native X closure, stored-ingot retention across permission
loss, empty-source removal/recreation and wrong-family refusal. Empty beacon
and crafter SDK-close regressions and the form callback passed. Six opens ended
in four normal closes and three expected denials (two active, one pre-open),
zero tickets/provider sessions and clean shutdown. The [hopper SDK example](examples/linux-linked-hopper.md)
includes five actual screenshots. Native world automation, custom routing and
cross-save/crash recovery remain incomplete.

Live testing exposed and fixed two SDK issues: Linux cross-module player
conversion now uses Endstone's virtual `asPlayer()` method, and a selected menu
action completes the caller's original ticket instead of leaking a hidden
child ticket. The example consumer collects terminal tickets on its scheduler.
Both platform providers retain their callback code for process lifetime; each
enable operation gets a fresh revocable callback token.

The actual Linux inventory packet uses a zero-initialized container name for
window 0. A new exact empty-inventory regression preserves this observed shape
while rejecting unrelated roles, dynamic containers and wrong slot counts.
The bounded private packet probe used for diagnosis was removed before the
final artifact's runtime test. Private packets and account identifiers are not
published.

## Builds and checks

SDK packaging source `2711123` adds the relocatable CMake dependency package.
Its local Linux suite passes **14/14** tests; the new standalone package test
also passes locally with MSVC. CI passes Linux Docker, Windows Debug/Release,
14 Linux sanitizer tests and 300,000 fuzz inputs. The
[SDK package checkpoint](../research/native-evidence/sdk-package-2711123.json)
records the exported files and CI identities. All three Linux plugin binaries
remain byte-for-byte identical to the `7aacb36` client-tested exports. These
packaging checks do not upgrade native gameplay qualification.

- Previous local Windows build (`171e0aa`): **12/12 CTest jobs passed**, DLL, SDK, consumers and PDB
  exported. The exact current provider and both consumer DLLs loaded in the
  isolated Windows server, verified their hashes and primitive manifest, and
  shut down cleanly. This Windows build has not had stock-client UI tests.
- Local Linux Docker build at `a3fbf78`: **16/16 CTest jobs passed**, ELF, SDK and consumers
  exported. The extra Linux test covers loaded-file hashes, replaced inodes,
  unknown runtimes and retained callback code after `dlclose`.
- Linux ASan/UBSan at `a3fbf78`: its CI job passed **16/16 tests**, including native saved-item reads and ABI compatibility, and 100,000 libFuzzer inputs each for NBT,
  storage and item-wire parsing, **300,000 total**.
- Model checks: 50,181 core, 10,309 storage, 23,371 item-wire checks; 5,000
  menu/select/forget cycles, duplicate selection, revoked permission, pending
  cancellation, action failure, and explicit action-ticket collection.
- Six standalone journal crash-boundary tests passed. These do not establish
  recovery across BDS/player saves.
- Immutable scope and five gate regression tests pass. All 138 canonical/platform
  and 20 additional surface/platform full-qualification outcomes remain
  unqualified; working SDK smoke tests cannot override this gate.

Build logs, module hashes and exact dependency evidence are indexed in
[`checkpoint-a3fbf78.json`](../research/native-evidence/checkpoint-a3fbf78.json).
Older checkpoints keep their original source and artifact identities.

At `a3fbf78`, [native CI run 34361320872](https://github.com/TheNINJALLO/endstone-remote-workstations/actions/runs/34361320872)
passed Linux Docker, Windows Debug/Release and Linux sanitizer jobs. Its overall
result is **failure** because the separate full-scope acceptance gate correctly
refuses the incomplete catalog. [Legacy regression run 34361320762](https://github.com/TheNINJALLO/endstone-remote-workstations/actions/runs/34361320762)
passed on both Windows and Linux. Current local client observations and
screenshots are in the [Linux workstation example](examples/linux-native-workstations.md).
All three Linux CI plugin hashes match the locally tested exports. The Windows
CI binaries were built and tested in CI but have not had local client tests;
the separate Windows startup record still identifies the previous local build.
Sanitizers cover core/model/parser and runtime-identity tests, not an instrumented
proprietary BDS UI process.

## Exact development artifacts

| Target | Export | SHA-256 |
|---|---|---|
| Linux x64 | `dist/linux-x64-dev/plugins/endstone_onistone_vcf.so` | `08acb14aaa315378e3089edaed0a2824d708b30f2c829e5fb882db76c78075d4` |
| Windows x64 | `dist/windows-release/plugins/endstone_onistone_vcf.dll` | `07b037e42a832571448022fdea6385b2db08e0839a9e5b611965aeac500e3dfb` |

Paths are relative to the native checkout. The loaded Linux shadow copy and
installed provider match the Linux export byte-for-byte. Linux debug information
is still embedded in this development ELF; Windows PDBs are under `symbols/`.
These are development artifacts, not completed all-UI release packages.

The [Windows startup record](../research/native-evidence/windows-171e0aa-startup-smoke.json)
also verifies the retained two-key configuration and the loaded shadow-copy
identities. Startup evidence is separate from native gameplay qualification.

## Compatibility boundaries

The pinned Endstone SDK is 0.11.10 at
`8f84d6f5b556916597ed5b6b71329b2ed3ca8fc8`. Public API family 0.11 does not
admit other native runtimes. Exact Linux BDS/loader hashes and ELF Build IDs
are in [NATIVE_LINUX_RUNTIME.md](NATIVE_LINUX_RUNTIME.md). No Onistone runtime
has been supplied or qualified.

The Linux provider directly needs `libcrypto.so.3`, libc++20, libc++abi,
libunwind and ordinary C libraries. Its current direct glibc symbol requirement
reaches **GLIBC_2.34**; older GLIBC_2.14 reports apply only to older artifacts.
The tested fixture uses glibc 2.36 and the loader's bundled libc++ family.
Process maps confirm one libc++ family. Endstone's own NumPy/frozenlist modules
also load libstdc++; the provider and native consumers do not link against it.
This is not certification of arbitrary Pterodactyl images.

The project plugin and consumers are native C++, with no project Python wheel,
pybind runtime or Python companion. Endstone's own loader remains outside that
boundary. Test scripts are development tools only.

## Remaining required scope

All 69 original entries, aliases, permissions and source contracts remain in
the frozen baseline and platform reports. **No custom native catalog entry is
fully qualified.** Eleven Windows original-mode paths have experimental
orchestration behind an opt-in flag. Eight Linux workstation contexts, four shared
inventory roles, three nearby linked machines and two nearby linked storage entries
have experimental original adapters. Five additional linked utilities have
selected PC evidence, including later repeated beacon/crafter SDK closure and
item/control restoration. Initial unaccepted input attempts remain recorded. The separate linked hopper path now has
selected transfer, metadata and cleanup evidence. Other Linux catalog adapters remain incomplete.
The current form is an SDK action demonstration; it is not a workstation.

Required work still includes original and custom merchant, machine, map-printing,
editable/persistent storage, workshop, equipment/cargo, editor/dialogue,
chemistry, held-container and correct-role services. Protected inventory menus,
pagination, native item publication, durable held identity, cross-save recovery,
remaining SDK consumers, configuration migration and final release packaging
remain incomplete. The installable CMake SDK package is documented in
[the SDK setup](NATIVE_SDK.md#use-the-installed-cmake-package). The native transaction model
and journal must be connected to BDS writes and qualified at real save/crash
boundaries before claiming durable delivery.

Client tests use Windows PC keyboard/mouse. Additional devices and multiplayer
are untested; no second client is required from the operator for this checkpoint.
See [the per-entry matrix](CAPABILITY_MATRIX.md) and
[all-UI migration](ALL_UI_MIGRATION.md) for the retained scope.


## Experimental linked chests

The [exact `b0ebf22` chest run](../research/native-evidence/linux-b0ebf22-chest-smoke.json)
passed original chest/trapped-chest transfers and metadata across SDK closure,
with permission-loss retention for the trapped source. Ordinary double chests
preserved slots 0, 26, 27 and 53 when reopened through the other half. A guard
denying only the partner closed an active view and refused a new opening; clearing
it allowed full recovery. Removing the verified-empty partner also closed the
session. Recreating it restored the native 54-slot view. Wrong-family and wrong
single/paired-shape requests refused before opening.

Empty hopper, beacon and crafter SDK-close regressions and a form callback passed.
Twelve opens ended in nine normal closes, three active failures and four separate
pre-open refusals. No tickets, sessions or queued inventory packets remained at
clean shutdown. All three Linux CI binaries matched the loaded public artifacts;
14 sanitizer tests and 300,000 fuzz inputs passed. The first sanitizer attempt
failed downloading CMake before tests; its targeted retry passed. The
[SDK example](examples/linux-linked-chests.md) includes ten reviewed screenshots.
Exact Endstone dimension names are case-sensitive: this fixture uses `Overworld`.
Paired trapped chests, automation, custom storage, adversarial source races and
cross-save/crash recovery remain incomplete.
