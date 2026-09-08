# Native SDK 1.0 development contract

This ABI is provisional. It is separate from Endstone's public C++ ABI and from
private BDS hooks. Compile consumers with an appropriate Endstone SDK/toolchain
and declare `depend = {"onistone_vcf"}` in their native plugin metadata.

Include `oni/vcf/sdk.hpp` and `oni/vcf/loader.hpp`. The
[compiled consumer](../examples/native/consumer.cpp) is the reference.
`sdk::discover()` resolves the provider's existing loaded module, including
Endstone's shadow copy. Do not load the original file as a second DLL.

All calls except function-table negotiation run on the server owner thread.
Strings and byte spans are borrowed only during a call and copied when retained.
Initialize every descriptor with `sdk::descriptor<T>()`, which fills its size
and version. Opaque integer handles are provider-owned identities, not pointers.
The caller supplies output structures and never frees provider memory.
Capability string views last until provider shutdown.

```cpp
auto table = oni::vcf::sdk::discover();
oni::vcf::sdk::Client ui(table, "my_plugin");
auto ticket = ui.prepare(player_uuid, "chest", VCF_TRANSIENT, "Storage");
ui.set(ticket, 0, "minecraft:stone", 12);
ui.open(ticket); // queued; inspect ui.info(ticket) on a later tick
```

This example compiles. The current native chest renderer refuses with
`VCF_UNAVAILABLE`; preparing model contents is not proof of a working screen.

Ordinary forms use `vcf_menu_desc` and `show_menu`; buttons name actions
registered with `register_action`. Own names resolve as
`consumer:action`; another consumer may invoke only explicitly exported
actions. Permission and registration are rechecked at dispatch, and callbacks
execute on a later tick. Cancellation and double submission invalidate the
session before another action can execute.

`prepare`, `set_item` and `define_rule` configure provider-owned state.
Real-source sessions reject preloading. Native/client-owned state cannot be
silently replaced by changing a backing-mode flag.
Items must be registered with the actual server; counts respect that item's
maximum. Optional NBT uses bounded standard little-endian named compounds.
The parser rejects truncation, duplicate compound keys, non-finite floats,
excessive depth/nodes and trailing bytes. This does not yet qualify full native
item codec round trips or nested-container writes under the new plugin.

Tickets report preparing/opening/active/closing/recovering/terminal state,
revision, generation and terminal status. `close` queues cancellation.
`forget` frees a terminal ticket. Consumers should release terminal handles;
limits refuse further work instead of allowing unbounded growth.

Call `dispose()` from `onDisable` and require `VCF_OK` before unloading.
A call from an executing callback returns `VCF_REENTRANT`; schedule disposal
after returning. Plugin-disable events additionally revoke the named owner.
The Windows provider is pinned for process lifetime because Endstone may retain
form callback objects. Its disabled callbacks hold only a revoked weak lifetime.
**Hot replacement requires a server restart.**

The ABI header is the authority for functions that exist. Action listing,
protected-menu descriptors, editor-specific controls, request-batch publication,
map providers and persistent storage operations are not yet exposed as completed
native SDK functions. Their original contracts remain tracked in
[ALL_UI_MIGRATION.md](ALL_UI_MIGRATION.md).
