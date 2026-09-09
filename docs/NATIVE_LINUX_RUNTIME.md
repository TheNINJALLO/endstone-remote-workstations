# Linux runtime development

The local Docker Desktop Linux engine now runs on the Windows development PC.
The pinned Linux build and private ELF inspection have executed locally.
This removes the installation/restart blocker; it does not complete the native
UI migration or qualify a production release.

The Linux provider identifies `/proc/self/exe` and the mapped
`endstone_get_server` module. It hashes bounded regular files with OpenSSL 3,
checks the runtime pathname against the mapped inode, and refuses unknown
fingerprints. The exact allowed files are:

| File | SHA-256 | ELF Build ID |
|---|---|---|
| BDS 1.26.45.1 | `8ba803f23d681816495c7ac83bdba4b9cd7165a3bee5aedd18fa0c8c3d408ec2` | `0b40c886132941f2cd98d45e73062bc75e66726e` |
| Endstone 0.11.10, CPython 3.11 Linux wheel runtime | `ac665adb20c9d5c640da9e88de956d728f6dd7771a4461ce8205824ec9b300bc` | `ba6af2ee50effc20b461a2fbef80e145ad6f6a51` |

Admission enables the public SDK integration. An opt-in experimental adapter
also calls the verified player-inventory opener, seven native workstation
factories and the separate crafting context path. Other Linux catalog adapters remain incomplete. Onistone and other Endstone builds need separate
evidence and admission; a matching version label does not suffice.

The provider promotes its existing loaded shadow copy with `RTLD_NOLOAD` and
`RTLD_NODELETE` before registering callbacks. It does not load a second copy.
Each enable operation also creates a new callback lifetime token, so re-enabling
the same plugin object cannot reactivate an old form callback. The Linux tests
exercise SHA-256 vectors, streaming and bounds, a fake runtime with the expected
filename/export, replacement of a mapped pathname, and retained code after
`dlclose`. These tests do not substitute for real loader/client tests.

## Dependencies and the isolated fixture

The inspected Endstone runtime records Clang **20.1.8** in its ELF compiler
metadata, matching the pinned compiler family. Its wheel bundles renamed
libc++, libc++abi and libunwind libraries. The private Linux fixture resolves
the plugin's standard library names to these same bundled files, avoiding
loading an additional compiler-image C++ runtime into BDS. Actual process maps
confirmed one loaded libc++ family, and both independent native consumers loaded
and executed through the C ABI. Endstone's NumPy and frozenlist dependencies also
load libstdc++; the provider and native consumers do not link against it.

A stock Windows Bedrock 1.26.45 client connected to the isolated Linux server,
rendered the consumer's action form, and invoked its registered action. This
exposed a Linux player-conversion failure: cross-module `dynamic_cast` returned
null. Provider and consumers now use the pinned SDK's virtual `asPlayer()` API.
Full catalog and transaction qualification remain separate from this form test.

The [Linux SDK example](examples/linux-native-sdk.md) includes screenshots,
commands and the final artifact's redacted client smoke record. The verified
form action, close-with-X and disconnect paths return to zero outstanding
tickets. The actual item registry and full inventory now decode with zero
refusals. A zero-initialized container name is accepted only within the bounded
player-inventory observation path; it grants no inventory-write authority.

OpenSSL's `libcrypto.so.3` is now a direct plugin dependency for SHA-256.
The Docker package snapshot supplies OpenSSL 3.0.20 and `libssl-dev` at build
time. OpenSSL 3 uses the Apache-2.0 license; it is dynamically linked rather
than copied into the plugin. Its [digest API](https://docs.openssl.org/3.0/man3/EVP_DigestInit/)
and the Linux [dynamic loader flags](https://man7.org/linux/man-pages/man3/dlopen.3.html)
describe the interfaces used here. Transitive glibc/library requirements still
apply; this is not certification of an arbitrary Pterodactyl image.

The private Endstone loader wheelhouse contains exact versions and hashes for
offline installation. Endstone's own Python loader is distinct from the project
plugin, which remains native C++ with no project Python companion.

The Compose smoke profile uses a dedicated Linux data volume owned by UID
10001, read-only private input mounts, no added capabilities, and loopback UDP
port 29179. The fixture seeds its disposable world from a verified private
archive to avoid Windows bind-mount overhead for thousands of asset files.
It refuses to initialize over an existing unmarked server directory. Both
independent SDK consumer `.so` files accompany the provider in this test.

No private BDS/runtime files, dependency wheelhouse, world volume, or raw runtime
captures belong in the public build context, source repository or release.
The smoke command requires separate confirmation of the supplied server's
license and an authorized private launcher; no script accepts a license.

## Experimental original inventory

Set `experimental_original_linux` to `true` in
`plugins/onistone_vcf/config.json` and restart the server to enable this adapter.
It defaults to false, including when reading an older two-key configuration.
The independent `vcf_native_passthrough` consumer requests `inventory2x2`,
`armor`, `offhand` or `recipebook` with `VCF_REAL_SOURCE`. These are four roles
of the same stock player-inventory screen. The adapter does not select a tab
or replace the player's items, recipes, cursor or inventory ownership.

The [Linux ABI research manifest](../research/native-evidence/linux-native-abi-126451.json)
records the exact compiler/header inputs, independent Linux layouts and complete
function hashes. The adapter verifies the loaded inventory wrapper, native
player linkage, native context readiness, vtable targets and function bytes
before calling BDS's inventory opener. It waits for an ordered client reply
and a chat-close interval, then requires the matching native container-open
packet before reporting an SDK open event. It rechecks permissions and guards
while observing the screen. Only the pinned Windows Bedrock 1.26.45 client
profile is currently admitted.

Closing the SDK ticket relinquishes its observation lease. The stock client
and BDS retain the actual inventory and its closure; the adapter does not
destroy the native manager or manufacture a close packet. Preloads, recipes,
replacement titles, policy changes and unrelated source descriptors refuse.
This original-behavior adapter does not qualify custom transactions, held-item
identity, recovery, stale-close replay or the full catalog. Trading and sign
function identities in the manifest are research candidates with no enabled
calls.

The [inventory SDK example](examples/linux-native-inventory.md) includes six
actual screenshots and the exact `171e0aa` build's client observations. Eight
opens and eight closes returned to zero tickets, with vanilla crafting,
equipment and held-compass interaction tests. The controlled disconnect check
passed; the earlier missing-ingredient check followed by death is retained as
unresolved. None of these records upgrades full-catalog acceptance.

The example's `vcf_native close` console command queues closure of that
consumer's tickets; when run by a player it addresses only that player's
tickets. `vcf_native status` reports outstanding tickets as well as open,
close, failure and guard counters. Terminal tickets are forgotten on the
consumer's scheduler. Neither command takes ownership of another consumer's
handles.

## Experimental original workstations

The same Linux flag now admits `craft`, `anvil`, `smithing`, `stonecutter`, `grindstone`,
`loom`, `cartography` and `enchanting` with `VCF_NATIVE_CONTEXT`. The
[workstation SDK example](examples/linux-native-workstations.md) documents
installation, dependency use, actual screenshots and exact smoke results.
Three-by-three `craft` uses a separate verified Linux ABI path, added in `7aacb36`.

The [workstation ABI manifest](../research/native-evidence/linux-native-workstations-abi-126451.json)
extends the earlier inventory research. Linux ELF RTTI, complete unwind ranges,
caller arguments and independently compiled event layouts establish six
factory identities. Each complete function is hashed before invocation.
BDS allocates and owns the resulting native manager and its original gameplay.

The [crafting ABI manifest](../research/native-evidence/linux-native-crafting-abi-126451.json)
records a 24-byte owner passed by value on the System V stack and a live-verified
context callback receiving a 40-byte descriptor. Crafting uses a BDS-owned
stack context with no container manager. After verifying activation, the
provider sends the matching ContainerOpen packet. Its complete native function
hashes and reentrant ownership checks are separate from the six factories.

The [exact `7aacb36` crafting run](../research/native-evidence/linux-7aacb36-crafting-smoke.json)
passed ordinary and Shift-click recipe output, SDK closure with unused inputs,
compass-triggered opening, ingredient disconnect/reconnect, and `/workbench`.
Stonecutter, shared-inventory closure and the SDK form also passed regression
checks. Four example opens/closes and the separate alias ticket returned to
zero provider sessions. Clean shutdown completed; crash recovery was not tested.

The [enchanting ABI manifest](../research/native-evidence/linux-native-enchanting-abi-126451.json)
adds an independently verified 1345-byte factory with the same three native
caller arguments. Its [exact `e0dbb78` client record](../research/native-evidence/linux-e0dbb78-enchanting-smoke.json)
includes Efficiency I offer selection and delivery, one-lapis/one-level cost,
SDK/X closure, and consumer-permission loss with input return. The deliberate
denial generated one expected failure callback; all tickets retired. Anvil
renaming preserved the enchantment. Linked real-table and custom-offer modes
remain unavailable.

The adapter projects a client-side workstation onto a nearby real-air position,
waits for the ordered client reply, and requires the matching native window,
type and position before the SDK open event. Closure tracks native readiness,
restores owned projections from current world state, and reserves retired window
IDs. Stale-close filtering and competing-projection handling are implemented
but still need adversarial and competing-plugin qualification.

The exact `ca30255` client run exercised all six factories and the four inventory
roles, finishing with 15 opens and 15 closes. Selected vanilla transformations,
SDK close with unused input, compass opening, two controlled disconnect cases
and a form callback passed. Grindstone's first plain-click output was rejected;
later Shift-click and plain-click repeats succeeded, leaving the first cause
unresolved. Custom logic and crash/save recovery remain unqualified.

## Experimental linked furnace sources

The same opt-in Linux adapter accepts nearby `furnace`, `blastfurnace` and
`smoker` sources with `VCF_REAL_SOURCE`. Each factory has its own complete
fingerprint in the [linked-furnace ABI manifest](../research/native-evidence/linux-linked-furnace-abi-126451.json).
An independently compiled header probe and native caller analysis establish
the source accessor, actor position/type and factory arguments. Loaded chunk,
dimension, range, permission, family and captured actor address are rechecked.
These are actual world blocks with vanilla-owned processing and contents.

The [exact `7a938e5` PC run](../research/native-evidence/linux-7a938e5-furnace-smoke.json)
delivered and counted three stone, three iron ingots and three cooked beef.
SDK closure passed while processing and after reopening. Wrong-family and
distant requests refused; removing the empty furnace and revoking permission
closed active views. The smoker retained one unfueled input across permission
loss and returned it after access was restored. Enchanting and crafting close
regressions and the form callback passed. Twelve opens ended in ten normal
closes and two active denials, plus two refusals before opening; zero sessions
remained at clean shutdown. See the [screenshots and developer example](examples/linux-linked-furnaces.md).

The preceding `26528a7` run exposed an ignored SDK ContainerClose packet using
type None. The tested fix sends the active container type. The earlier failure
and successful recovery of all three stones remain in the smoke record.
Custom processing, source-configuration aliases, active source unload/transition
checks and cross-save recovery remain incomplete.

## Experimental linked Ender Chest and barrel

The [storage ABI manifest](../research/native-evidence/linux-linked-storage-abi-126451.json)
identifies a distinct five-argument block-container factory. Linux caller
analysis and a compiled header probe establish its signed container type,
64-bit entity sentinel and unsigned block-actor type. The verified native
resolver selects the player's existing Ender inventory for type 23 and the
real barrel source for type 42. BDS owns all items and transactions.

The [exact `1d88dfa` record](../research/native-evidence/linux-1d88dfa-storage-smoke.json)
passed selected first/last-slot deposits and withdrawals, Ender access through
two source blocks, name/enchantment retention, permission loss, source removal
and controlled barrel disconnect/reconnect. SDK/native closure, empty furnace
regression and the form callback passed. Nine opens, seven normal closes and
three expected denials ended in zero sessions and clean shutdown. See the
[SDK example and screenshots](examples/linux-linked-storage.md). This nearby
linked path does not qualify source-free Ender access, custom storage or
cross-save/crash recovery.

## Experimental linked utilities

The [utility ABI manifest](../research/native-evidence/linux-linked-utility-abi-126451.json)
records five separate factory/model identities and the independently decoded
Linux crafter control format. The [exact `961e9ec` run](../research/native-evidence/linux-961e9ec-utilities-smoke.json)
passed selected dispenser/dropper transfers, brewing output, beacon payment
and crafter toggles/redstone output. Beacon and crafter SDK closure failed and
quarantined the tester; normal closure and reconnect recovery observations are
recorded separately. See the [SDK example and screenshots](examples/linux-linked-utilities.md).
No custom behavior or full-catalog qualification is implied.

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
