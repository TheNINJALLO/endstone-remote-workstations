# Native held-item inspection

SDK 1.2 introduces `inspect_held` for a connected player's selected hotbar item.
This ports the read-only inspection portion of the held-item contract to C++.
Native held editors, item locking, identity assignment, writeback and crash/save
reconciliation remain incomplete. `native_open_available` is always zero.
A placed shulker block is not a substitute for the catalog's held-item source.

## Call from another plugin

Use the [installed SDK](NATIVE_SDK.md), declare `depend = {"onistone_vcf"}`, and
call on the server thread while your registered consumer is enabled:

```cpp
auto table = oni::vcf::sdk::discover();
oni::vcf::sdk::Client ui(table, "my_plugin");
auto held = ui.inspect_held(player.getUniqueId().str());
// All arrays below belong to this returned value; no provider pointer escapes.
player.sendMessage(std::string(held.canonical_id) + ": " + held.identifier);
// held.slot is zero-based (0..8); held.amount and held.auxiliary describe the item.
// held.durable_id is an empty string if the reserved ID tag is absent.
// held.digest is a 32-byte content digest, not a write authorization.
```

Keep the client as a plugin member for repeated calls and explicitly `dispose()`
on disable, as described in the SDK lifecycle contract. Inspection is synchronous
and allocates no UI ticket. Opening guards apply to sessions; they are not invoked
by this read-only operation. A consumer must apply any additional access policy
before inspecting another player's item.

The provider requires `remoteworkstations.use` and the matching canonical
`remoteworkstations.open.shulker`, `.bundle`, `.writtenbook` or `.bookediting`
permission on the target player. It reads the selected slot, main hand and
selected slot again through Endstone 0.11.10's public API. A changed selection or
different item snapshot refuses with `VCF_STALE`. Empty hands return
`VCF_NOT_FOUND`; unsupported types return `VCF_UNAVAILABLE`. Invalid metadata
returns `VCF_INVALID`, and exceeded bounds return `VCF_CAPACITY`. Invalid,
dead or disconnected players and revoked consumers refuse; off-thread calls
return `VCF_WRONG_THREAD`. C output storage is unchanged on failure.

Recognized identifiers are vanilla written/writable books, base bundles,
undyed/base shulker boxes and the 16 named vanilla colors of shulkers/bundles.
The provider classifies an actual registered item returned by Endstone; it does
not create items from these names. This list is not runtime qualification of
every color on every platform. Arbitrary namespace lookalikes refuse.

## Metadata and identity

The detached snapshot includes the identifier, amount, auxiliary value and
complete metadata supplied by `ItemStack::getNbt()`. Private serialization keeps
tag widths, signed integer bits, finite float/double bits (including negative
zero), compound keys, list element types and nested item contents. Bounds are
64 KiB of encoded metadata, depth 16 and 4,096 tag/array elements. Invalid End
payloads, non-finite numbers and mismatched list elements refuse. Public results
expose the metadata byte count and digest, not raw NBT or private book text.

If the metadata contains `remote_workstations:held_id`, it must be a string
containing a nonzero, lowercase canonical UUID; its value is returned unchanged.
An absent tag remains absent. This operation never assigns an ID, changes a tag,
rewrites metadata, marks a slot dirty, sends an item packet or installs a native
editor. Duplicate IDs and identical content are not distinguished as physical
items; consumers must not use inspection as a reservation or writeback token.

SHA-256 hashes a private versioned frame: ASCII `VCFH`, byte `1`, little-endian
32-bit identifier length, identifier bytes, 32-bit amount, auxiliary bits,
metadata length, then bounded standard little-endian named-compound NBT.
The root name is empty and compound keys follow the public SDK's ordered map.
Slot and player identity are excluded, so moving unchanged content between slots
does not change this digest. The format differs from the legacy Python digest;
do not compare them or persist either as proof of exclusive item ownership.

## Try the compiled example

Install the provider and `endstone_vcf_native_passthrough.so` (Windows uses
`.dll`) from the same development build. Hold a supported item, then run:

```text
/vcf_native inspect-held
/vcf_native status
```

The example requires `vcf.examples.passthrough` in addition to provider
permissions. It prints the type, selected slot, amount, metadata size, ID
presence, editor availability and digest to the requesting player. Status
reports aggregate successes/refusals without logging item metadata.
Inspection does not need the experimental original-screen configuration flag.

The `held_item_snapshots` native test covers nested contents, typed metadata,
negative zero, UUID validity and bounds. C ABI tests protect the older 1.0/1.1
table boundaries; SDK tests cover unchanged failure output, permissions,
revocation, thread rejection and 1.1 guard callback versions. The sanitizer
configuration explicitly includes held snapshot tests. Stock-client and artifact
evidence must be recorded separately; these tests do not qualify a held editor.
