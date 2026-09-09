# Linked hopper SDK example

The experimental Linux adapter accepts `hopper` with `VCF_REAL_SOURCE` on the
exact admitted Endstone 0.11.10 / BDS 1.26.45.1 runtime. It uses the real hopper's
five slots. BDS owns its items, normal transfers and world automation. Custom
routing, virtual inventories and Windows admission remain incomplete.
Stock-client tests of this adapter are pending.

Install the provider and native passthrough consumer using the
[Linux setup](linux-native-workstations.md#install-and-open). Enable
`experimental_original_linux`, then supply an existing authorized hopper within
six blocks in an already loaded chunk. Keep the literal coordinate quotes:

```text
/vcf_native hopper "4,91,8"
/vcf_native close
```

An independent native consumer uses the installed
[CMake SDK dependency](../NATIVE_SDK.md#use-the-installed-cmake-package):

```cpp
auto request = oni::vcf::sdk::descriptor<vcf_session_desc>();
request.player = oni::vcf::sdk::view(player_uuid);
request.canonical_id = oni::vcf::sdk::view("hopper");
request.mode = VCF_REAL_SOURCE;
request.dimension = oni::vcf::sdk::view(dimension_name);
request.x = source_x;
request.y = source_y;
request.z = source_z;
request.source_permission = oni::vcf::sdk::view("myplugin.hopper");
request.callback = on_session_event;
request.context = this;
vcf_handle ticket = 0;
oni::vcf::sdk::checked(ui.api().prepare(ui.owner(), &request, &ticket));
ui.open(ticket);
```

The consumer registers its permission and callback, checks capabilities, and
forgets its terminal tickets. The player needs the original catalog permission
as well as the consumer's source permission. The provider rechecks permission,
source identity, distance and dimension during the active session. Changed slot
policies, preloaded items or custom rules are refused for this original mode.

The [Linux ABI record](../../research/native-evidence/linux-linked-hopper-abi-126451.json)
identifies a distinct two-pointer block factory and a separately compiled native
container refresh virtual method. The provider verifies the created manager and
its source, sends the open packet, rechecks ownership, then asks BDS to publish
its current contents. An observed matching five-slot content packet is required
before the SDK opening event. Hopper minecarts use a different factory and are
not admitted by this block-source path.
