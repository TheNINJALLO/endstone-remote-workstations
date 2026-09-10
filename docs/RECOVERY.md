> **Legacy v0.4.0 documentation.** This page describes the Python/native-companion implementation. For the native prerelease, use [installation](NATIVE_INSTALLATION.md), [examples](NATIVE_EXAMPLES.md) and [SDK 1.5](NATIVE_SDK.md).

# Transaction and recovery boundaries

The tested storage engine is an **offline model**. Its atomic shadow-state commit is useful groundwork,
but does not coordinate real BDS memory, player saves, world saves or dropped item entities.
`Journal.model_storage` must never be presented as a real Ender Chest.

The planner accepts server-owned immutable snapshots. It validates generation, negative odd request ID,
inventory version, slot role/bounds, pre-request stack identities, counts, stack maxima, metadata equality,
locks and filters. It applies all actions to a shadow copy and produces a plan containing before/after
state and deferred drop intents. XP cannot be changed by this storage planner. Unsupported crafting,
creative/destroy/create actions reject the request without applying earlier actions.

The harness commits only if its current state still equals the planned before state. A bounded response
cache handles exact duplicate requests; changed duplicates fail. A monotonic request watermark rejects
evicted replays. This assumes the model's strict ordering and positive stack IDs. Real negative
request-reference identity semantics, client batching and inventory-role indexing require captures.
Each validly decoded request in a model batch receives a response; wire error/resync integration is not live.

## Two-store journal

`transfers` records owner, session generation, request ID, complete before/after snapshots, intended
effects, phase and notes. SQLite uses WAL and synchronous FULL. A bounded single-worker queue isolates
disk I/O; only immutable values cross the boundary. Production diagnostics submit a startup reconciliation
query and poll one future; no inventories are scanned. Shutdown drains the worker.

Proposed live sequence, not yet connected to BDS:

1. Freeze exclusive session inventory ownership; validate against current state on the server thread.
2. Persist `prepared` before any game-state change. Wait by polling the future on ticks, not blocking the tick thread.
3. Revalidate versions, identities, permissions, player/entity lifecycle and loaded context after disk completion.
4. Apply the complete plan on the game thread using a verified commit mechanism. Only then perform authorized side effects, each with a durable effect identity.
5. Record `applied-memory`. This does **not** prove BDS saved the result.
6. Reconcile using a proven BDS durable-save/escrow marker mechanism before a record may become resolved. That mechanism is missing; no completion/automatic issuance API is implemented here.

There is no cross-store atomicity from two SQLite transactions or an inventory snapshot hash. BDS can
save inventory and drops at different moments, unload entities, or crash between mutations. A matching
item count after restart can also arise independently; it is not a durable transaction identity.

On restart **all prepared or applied-memory transfers become quarantined**. Repeated reconciliation is
idempotent. It never changes BDS/player/world state, creates recovery items, or issues a replacement.
Changing a journaled request's payload under the same identity is rejected.

Offline cleanup first merges/moves cursor items into available model inventory slots. Overflow is held
with full metadata in `recovery`, in the same SQLite transaction as the updated model snapshot.
Idempotency keys prevent duplicate recovery rows and reject conflicting owner/payload reuse. No user
redemption command exists. This describes the offline storage prototype. Native workstation
close now delegates input return to BDS, but durable overflow recovery, held-shulker escrow
and cross-store reconciliation remain unimplemented.

Live acceptance tests on 0.2.0rc1 confirmed anvil input return after disconnect/reconnect
and clean server restart. A separate death test with `keepInventory=true` produced a dropped
item entity instead of inventory return. The same action at a real vanilla anvil produced
the same drop. Endstone's Player::drop hook skips PlayerDropItemEvent when the player is
dead, so the public drop event is insufficient to intercept this recovery boundary.
The original named enchanted sword was retrieved by the test operator with unchanged NBT;
this does not implement or certify durable recovery. Failed snapshots and later retrieval
snapshots remain separate under research/release-blockers/.

## Tested crash boundaries

Tests launch actual child Python processes which exit abruptly after prepare, a partial external-store
write, a completed external-store write, or the applied-memory marker. The external JSON file stands in
for independently saved BDS state; it is explicitly not a BDS crash simulation of full fidelity.
After each death the real SQLite WAL is reopened, the transfer quarantines, external bytes remain
unchanged and no recovery item is issued. This demonstrates conservative journal behavior, not exactly-once delivery.

Session tests cover manual/forced close, timeout, disconnect, death, teleport, dimension transition,
disable/reload/shutdown reasons, generation mismatch and retry after cleanup failure. Those are model
tests. The native backend separately installs lifecycle observers and asks BDS to close its
containers; selected live results and their limits are recorded above and in TEST_REPORT.md.
The offline model retires window IDs 1–99 for the connection. The native backend uses BDS's
own allocation and close acknowledgment instead of applying that model allocator to BDS.
