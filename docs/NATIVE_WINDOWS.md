> **Legacy v0.4.0 documentation.** This page describes the Python/native-companion implementation. For the native prerelease, use [installation](NATIVE_INSTALLATION.md), [examples](NATIVE_EXAMPLES.md) and [SDK 1.5](NATIVE_SDK.md).

# Windows native implementation and evidence

The companion uses CPython 3.11 and the existing Endstone Python interpreter. A real
`endstone.Player` crosses pybind11's shared registered type system. No second interpreter,
raw address API, retained native player pointers or Python item writes are used.
The sign editor installs two scoped native save hooks; other native containers
retain their per-instance distance adapters. See [sign ownership](SIGNS.md).

Target: Endstone runtime 0.11.10, BDS 1.26.45.1, protocol 2169. The Python public API
baseline remains 0.11.0 (`api_version = "0.11"`); 57 used symbols were checked against its stubs.
The exact EXE and runtime DLL SHA-256 values, entry-point RVAs, function sizes and code
hashes are generated into `research/native-compatibility.json`. Both loaded modules must
match. Called entry points are checked in loaded executable memory before use.
The two sign functions are fully checked before their verified distance blocks
are patched; ordinary source/owner/apply methods remain checked on every call.

The matching Endstone PDB identifies `EndstoneActorBase<Player, Player>::getHandle`
at runtime RVA 748752. Its weak entity handle is resolved on the owner thread for each call.
Independent SDK layout compilation and matching runtime accessor disassembly establish
native Player's container manager at +1440, GameMode at +2720 and item-stack manager at
+2744. The GameMode and stack manager must both point back to that native Player.

The Windows ABI for anvil, stonecutter, grindstone, smithing, loom and cartography factories is
`void(Player*, const BlockPos*, int64_t actor_id)`, with -1 for a block. The native anvil
factory was reached in the captured real-anvil ContainerOpen stack; the exact BDS dispatch
table also maps the station type to that function. BDS allocates and retains the full native
manager, registers its slot containers, sends inventories and processes every request.
Neither Python nor this companion computes recipe output, anvil cost or item metadata.

Workbench opening follows the exact native dispatch branch: allocate a window and establish
the 24-byte BlockPos source variant, notify the native stack manager with its 40-byte context,
then send the native UI's ContainerOpen. The callback's vtable entry and code hash are checked.
This is essential: a crafting screen opened with packets alone rejected real recipe requests.

## Verified gameplay spikes

Tests ran in the explicitly disposable `rw-smoke` world, from a platform over 20 blocks
away from the comparison stations. Temporary representations exist only on that client;
real nearby blocks are never placed or overwritten.

* Crafting: 16 oak planks consumed, two chests created by BDS through the real 3x3 recipe UI.
* Anvil: renamed an enchanted diamond sword to `Remote Blade`; XP level 4 became 3;
  Sharpness I and the single sword were retained. The earlier real-block native comparison
  renamed it to `RW Blade` and charged the same one-level rename cost.
* Stonecutter: 64 stone placed, one stone brick taken, 63 unused stone returned on close.
* Grindstone: one enchanted iron sword became one unenchanted iron sword, with visible XP gain.
  After restart its NBT was `{Damage:0,RepairCost:0}` and the player had one total XP point.
* Smithing: diamond pickaxe, Netherite ingot and upgrade template consumed for one Netherite pickaxe.
* Loom: two black banners and two red dyes consumed for two red-border banners; pattern NBT survived restart.
* Cartography: one real world map scaled, one paper consumed, one excess paper returned; scaled map rendered and survived restart.

Captured evidence is under the isolated probe's data directory; screenshots are in `research/`.
Final release validation must name the packaged wheel tested, because probe success alone
does not validate the plugin's command, form, projection and cleanup orchestration.

## Ordering findings

Sending a restoration packet inside `PacketSendEvent` overwrote the active serialized
packet buffer in a live test. The release backend therefore queues restoration/close work
for its scheduler. Native ItemStackRequests remain executed by BDS; scoped
authorization checks can reject owned requests. The managed packet inventory
backend separately validates and responds to its own requests.

Windows 1.26.45's NetworkStackLatency reply multiplies the sent timestamp by 1,000,000
and preserves the `from server` bit. The actual fixture is
`tests/fixtures/windows-native-controls-2169.json`. The projection is sent before this
round trip; a matching response is required before native opening. This establishes
packet ordering, not proof that a player has visually inspected the UI.

Actual chat opens at roughly 100-150 ms still caused a stale client Close(-1) to close the
new BDS manager. A measured 500 ms transition interval plus the post-projection round trip
passed repeated command opens and menu opens. Both conditions are enforced for this client.

Loaded entry points are hashed on every call. The hash implementation uses Microsoft's
[CNG pseudo-handle support](https://learn.microsoft.com/en-us/windows/win32/api/bcrypt/nf-bcrypt-bcryptcreatehash)
to avoid reopening the SHA-256 provider for every check. Performance records measure
one-player calls, not server capacity.

## Build

Use CPython 3.11, MSVC Build Tools 14.44.35207 and the pinned build/native requirements.
Fetch the pinned Endstone and expected-lite headers with the repository research tools.
Run `tools/fetch_references.py`, then `tools/fetch_windows_native_headers.py`.
Run `tools/fetch_native_headers.py` for pinned MinHook 1.3.4; compiled source
files are checked against `research/minhook-source.lock.json`.
The additional expected-lite 0.9.0 header is verified against its tagged upstream source
and pinned in `research/expected-lite.lock.json`.
Keep the locally supplied, exact BDS EXE in `.runtime/windows-smoke-isolated/` and the
matching Endstone runtime in `.venv/Lib/site-packages/endstone/`.

```
.venv\Scripts\python.exe -m build --wheel --no-isolation
.venv\Scripts\python.exe tools/build_windows_native.py --wheel dist/endstone_remote_workstations-0.2.0rc1-py3-none-any.whl
```

`tools/release.py --native-windows` also builds the source archive, reruns the test suite,
rebuilds both wheels for byte equality, checks for excluded binaries/probes and writes hashes.

Install the Windows platform wheel by itself for RemoteWorkstations; do not also install
the portable wheel as a second plugin. The standalone portable wheel has no native companion.
The development probe and hot refresh commands are never included in either release wheel.

Linux native execution, mobile touch, controllers and the full crash/interoperability matrix
remain separate verification requirements. The existing SQLite storage prototype is not
used to write back native workstation items: BDS owns these inventories and their normal
save behavior. Exactly-once recovery beyond BDS's own save semantics is not claimed.

The 0.3.0a2 companion adds `refresh_inventory(player)`. The pinned layout compile
emits `Mob::sendInventory(false)` at virtual offset 1312 (slot 164). Both observed
Player tables dispatch to RVA `0x692b40`, with an unwind extent of 681 bytes. The
builder pins that entire function hash, and each call checks the live dispatch.
It sends BDS inventory descriptors; it does not clear, reinsert or create items.
The read-only discovery is recorded in `research/packet-inventory-resync-candidates.json`.
