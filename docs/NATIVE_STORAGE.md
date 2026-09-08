# Native storage transaction core

`framework/storage.cpp` ports the bounded protocol-2169 request contract and
cursor reservation model retained in the immutable source baseline. It is a
native core implementation; no active server packet handler or valuable-item
writer uses it yet. Storage, Ender Chest, shulker and bundle gameplay therefore
remain unqualified.

The decoder accepts take/place/swap/drop-shaped storage actions, validates both
action discriminants, canonical integers/booleans, slot-reference fields and
negative odd request IDs, and rejects filter strings, trailing data and partial
packets. Bounds are 64 KiB, 100 requests and 100 actions across a payload. Drop
actions can be decoded for a clean refusal; the transaction planner rejects
them because entity creation has a separate durability boundary.

Response decoding/encoding preserves the captured nested optional fields,
container groups, authoritative stack IDs, names, filtered names and durability
corrections. Malformed UTF-8 and noncanonical optional markers refuse. The
sanitized successful and failed server responses round-trip byte-for-byte;
fuzzing also checks that any accepted response re-encodes identically.

Storage topology comes from the retained catalog capacities. The explicit
storage role maps to a storage slot, player hotbar/main inventory or the active
cursor; dynamic containers and workstation roles refuse. Retaining a mapping
for barrel/dropper/trapped chest does not substitute for their separate client
layout tests. Held-shulker role 30 is distinct; no bundle mapping is invented.

The planner checks the session generation, revision, original stack identities,
source/destination policies, exact metadata and stack capacity. It stages every
action in a request before returning a new state. Split stacks receive new
identities; whole-stack transfers and swaps preserve identity. Preview and
result slots cannot pass through this ordinary transfer path. Custom recipe
execution and repeated result extraction need their own integration.

`storage::Reservation` compares the actual inventory with its committed
baseline before each request. An unfinished cursor gesture changes only its
presented snapshot; cancellation restores that baseline without issuing items.
A completed gesture can call a supplied writer. Replays with identical requests
do not call the writer again; changed, old or out-of-order requests refuse.
The replay cache is bounded to 128 accepted requests.

A writer must report a clean pre-write conflict separately from uncertainty.
After any possible mutation, exceptions, failed verification and allocation
failures keep the reservation quarantined and closed. Cancellation then retains
the reconciliation evidence; it never overwrites native inventory or mints a
replacement. This contract is **not** an atomic BDS/plugin save mechanism.
The durable journal and the verified native save interception still need to
be connected and qualified at actual server crash boundaries.

The C++ capture test reads the exact frozen sanitized Ender Chest fixture
(`be5edbd322c6afbd222072f084cb376c994b37b8b2df1857060adac23cd5c65d`).
It checks all request prefixes, a real request sequence's role/identity shapes,
replay and changed replay, an unfinished cursor gesture, failed post-write
reads and the captured wrong-identity request. Test item metadata is synthetic;
these tests are not a new live ItemStack codec qualification. Another 10,000
generated transfer/swap cases check conservation and identity uniqueness.

The Docker sanitizer target adds 100,000 coverage-guided storage decoder inputs,
seeded from that hashed fixture, beside the NBT fuzz target. Logs identify the
tested source revision. Neither synthetic fuzz data nor historical captures
count as interactive qualification of the new plugin.
