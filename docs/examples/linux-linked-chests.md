# Linked chest SDK example

The experimental Linux adapter accepts `chest` and `trappedchest` for real,
unpaired 27-slot sources, and `doublechest` for two paired ordinary chests with
54 slots. It targets the exact admitted Endstone 0.11.10 / BDS 1.26.45.1 runtime.
The new paths await stock-client testing. Custom storage and Windows admission
remain incomplete.

Install the provider and native passthrough consumer using the
[Linux setup](linux-native-workstations.md#install-and-open), and enable
`experimental_original_linux`. Supply an existing authorized source within six
blocks, in an already loaded chunk. For a double chest both halves must meet
these requirements. Keep the literal coordinate quotes:

```text
/vcf_native chest "1,91,3"
/vcf_native trappedchest "1,91,5"
/vcf_native doublechest "1,91,7"
/vcf_native close
```

A developer requests the canonical ID through the installed
[CMake SDK dependency](../NATIVE_SDK.md#use-the-installed-cmake-package):

```cpp
auto request = oni::vcf::sdk::descriptor<vcf_session_desc>();
request.player = oni::vcf::sdk::view(player_uuid);
request.canonical_id = oni::vcf::sdk::view("doublechest");
request.mode = VCF_REAL_SOURCE;
request.dimension = oni::vcf::sdk::view(dimension_name);
request.x = source_x;
request.y = source_y;
request.z = source_z;
request.source_permission = oni::vcf::sdk::view("myplugin.chest");
request.callback = on_session_event;
request.context = this;
vcf_handle ticket = 0;
oni::vcf::sdk::checked(ui.api().prepare(ui.owner(), &request, &ticket));
ui.open(ticket);
```

Register the consumer permission, handle lifecycle callbacks and forget terminal
tickets. BDS owns the actual chest items, slot order, names and enchantments.
The adapter refuses preloaded items, changed slot policies, custom rules, wrong
block families and a paired source requested as a single chest. `doublechest`
currently accepts ordinary chests; a paired trapped chest is not admitted.

The [Linux ABI record](../../research/native-evidence/linux-linked-chest-abi-126451.json)
identifies the shared block factory, the actual chest container subobject and
the native 27/54-slot getter. Both actors must point back to one another and
resolve at the captured adjacent positions. The provider rechecks identities,
permissions, distance and dimension during the session, before incoming stack
requests and before publishing matching native contents or slot updates.

## Protect both halves

A registered SDK guard receives separate callbacks for the two source positions
under the same ticket and canonical ID. Check each callback's dimension and
coordinates against your protection policy. Returning anything except `VCF_OK`
denies continuation. Callbacks can repeat; avoid side effects such as charging a
fee every time a guard runs. The session retains its original requested source.

The compiled example includes an operator-only, in-memory demonstration policy:

```text
/vcf_native guard-deny "overworld|2,91,7"
/vcf_native doublechest "1,91,7"
/vcf_native guard-clear
```

Use the exact Endstone dimension name. These commands also work from the server
console. If the denied position is the paired half, the request must refuse even
though the requested half is allowed. Setting the policy while the chest is open
must close that consumer's session. The demonstration policy affects only this
example's tickets, and resets when its plugin restarts. Production protection
plugins should register their own scoped guards.

## Qualification limits

Build and guard tests alone do not establish gameplay or release qualification.
Client transfer, metadata, close/reopen and pair-loss results will be recorded
against the exact loaded binary. Redstone behavior, adversarial pairing changes,
same-address actor reuse, shared access, save/crash recovery and all custom modes
remain separate requirements.
