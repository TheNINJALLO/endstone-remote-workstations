# Native SDK 1.1 development contract

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

ABI 1.1 appends action discovery and protection guards. The provider still
accepts ABI 1.0 negotiation and descriptors, copies only its original table
prefix, and emits the callback version registered by that older consumer.
The current C++ SDK requires 1.1 and owns its function-table copy, so constructing
`sdk::Client(sdk::discover(), "my_plugin")` is safe. C tests check the old
table's boundary with sentinel bytes. This is compatibility with this project's
development ABI, not with legacy Python imports.

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

A button selection completes the original menu ticket with the action's result;
it does not allocate a second hidden ticket. Poll `session_info` and call
`forget` after that ticket becomes terminal. The compiled consumer demonstrates
this cleanup on its scheduler and reports outstanding tickets with
`/vcf_catalog_showcase status`. Explicit `invoke_action` calls still return
their own caller-owned tickets. Menu guards and action permissions are checked
again on the dispatch tick, including when permission changed after selection.

`ui.actions()` returns detached descriptions of the consumer's own actions
and other consumers' explicit exports. Private actions remain hidden. The C
functions `action_count` and `action_info` use a registry revision and copy
text into caller-provided storage; a changed revision refuses with `VCF_STALE`.
No returned description exposes a callback pointer or another consumer's context.

`ui.guard(name, callback, context, canonical_id)` registers an owned protection
callback. An empty canonical ID applies to all openings, including forms;
otherwise aliases resolve to the exact canonical entry. The callback receives
the requesting consumer, ticket, phase and complete source descriptor. Only
`VCF_OK` permits continuation. Guards run on the server thread before dispatch,
at asynchronous readiness, and during active native adapter checks (at most
250 ms apart). Form selection rechecks guards too. Removing a guard or disabling
its owner revokes it before any later callback dispatch. These checks do not
claim an atomic interception of BDS's unchanged vanilla transaction path.

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
An asynchronous adapter keeps a ticket opening until its native handshake
finishes and closing until its manager is retired. Cancelling before dispatch
does not close another plugin's form. Native opens recheck permissions when
the handshake completes, and pending/closing native leases exclude concurrent
opens for the same player. `VCF_PENDING` is an internal host-adapter result;
public queueing operations still return `VCF_OK` when accepted.
`forget` frees a terminal ticket. Consumers should release terminal handles;
limits refuse further work instead of allowing unbounded growth.

Call `dispose()` from `onDisable` and require `VCF_OK` before unloading.
A call from an executing callback returns `VCF_REENTRANT`; schedule disposal
after returning. Plugin-disable events additionally revoke the named owner.
The Windows and Linux providers are pinned for process lifetime because Endstone may retain
form callback objects. Its disabled callbacks hold only a revoked weak lifetime.
**Hot replacement requires a server restart.**

The ABI header is the authority for functions that exist. Protected-menu
descriptors, editor-specific controls, request-batch publication,
map providers and persistent storage operations are not yet exposed as completed
native SDK functions. Their original contracts remain tracked in
[ALL_UI_MIGRATION.md](ALL_UI_MIGRATION.md).

The experimental Windows original-mode adapter requires an explicit
`experimental_original_windows: true` in `plugins/onistone_vcf/config.json`.
For `craft`, `anvil`, `stonecutter`, `grindstone`, `smithing`, `loom` and
`cartography`, request `VCF_NATIVE_CONTEXT`; BDS retains its original gameplay
ownership. For `inventory2x2`, `armor`, `offhand` and `recipebook`, request
`VCF_REAL_SOURCE`. These four share the real inventory screen and client-owned
navigation/closure. Other backing modes refuse instead of substituting a
screen. This configuration flag does not certify client or custom behavior.

Linux has a separate `experimental_original_linux: true` flag for the four
shared player-inventory roles in `VCF_REAL_SOURCE` mode, plus `craft`, `anvil`, `smithing`,
`stonecutter`, `grindstone`, `loom` and `cartography` in `VCF_NATIVE_CONTEXT`.
It uses independently verified Linux ABI facts, including crafting's by-value
owner argument and BDS-owned stack context.
See the [Linux workstation example](examples/linux-native-workstations.md).

The original-mode adapter rejects preloaded items, changed slot policies,
recipe definitions, replacement titles and unrelated source descriptors. It
never reports success while silently ignoring requested custom behavior.
The provider reclaims its own completed command/catalog tickets; SDK consumers
retain responsibility for forgetting their own terminal tickets.

Native Windows and Linux close leases retain previously owned window IDs for 60 seconds
under this specific client profile. They reject a delayed old close only when
the verified current native manager owns a different window. A legitimate close
for the same window, another player or an unrelated ID passes through. Form
cleanup similarly checks its own lease and observes replacement form/native
opens before issuing Endstone's broad close operation. Packet callbacks record
ownership changes; replacement packets are not sent recursively from them.
