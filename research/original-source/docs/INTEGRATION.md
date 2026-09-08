# Remaining runtime and client verification

Windows 26.45 with keyboard/mouse automation and Classic UI has now exercised commands, forms,
native player-inventory smoke checks, and restart/reconnect. See [the client report](WINDOWS_CLIENT_TESTS.md).
Seven native Windows workstations now have selected real gameplay tests; see [current evidence](TEST_REPORT.md).
Mobile touch, controller and Pocket UI remain untested. Native gameplay requires explicit candidate opt-in.
The successful Windows headless smoke covers plugin load/console/shutdown only. Linux hash inspection
and one static signature match are not Linux runtime validation. Do not change a capability to
`implemented-tested` based only on the offline tests or headless smoke.

## Record exact environments

Use an isolated test world. Preserve originals of player/world saves and keep test items distinct.
Recalculate the executable hash with `tools/inspect_bds.py`; use Endstone 0.11.10 for the target BDS.
For each client record game build, negotiated protocol, operating system, input mode and Classic/Pocket
profile. Record behavior/resource pack hashes and the complete plugin list. Do not store login payloads,
access tokens, addresses, book text or unrelated player information in shareable captures.

Create one record per client/input/profile combination:

```text
python tools/integration_harness.py init research/manual-my-client.json
python tools/integration_harness.py validate research/manual-my-client.json
```

The template has 54 individually tracked cases. Its validator requires environment/evidence fields
for passed cases and vanilla comparisons for craft/anvil/processing. It never awards a passing result.

Minimum client paths: 1.26.45 / protocol 2169, plus each admitted 1.26.40, .41, .42, .43 and .44 path
where a client build is available. Endstone's 2168 admission/shims do not certify these packets. Test
Windows mouse/keyboard, mobile touch and controller, Classic and Pocket where actually selectable.
Record unavailable combinations as blocked, not passed or assumed equivalent.

## Verify the current release

1. Copy the wheel into an isolated compatible server's plugins folder. Confirm metadata API 0.11,
   enabled plugin, exact runtime/protocol, journal initialization and clean disable.
2. Check `/workstations`, `list`, `status`, all short aliases and `open` with known/unknown names.
   Members can browse; status/diagnostic access requires permissions; powerful editor requests remain disabled.
3. Pre-register a conflicting short command from a test plugin. Verify Endstone declines the collision;
   remove/rename aliases via config and restart. Confirm vanilla `/enchant` still resolves normally.
4. Open every disabled capability while holding valuable, named, enchanted and NBT-rich items.
   Compare inventories/XP before and after: zero mutations, no temporary blocks or container packets.
5. Turn diagnostics on/off. Confirm only allowlisted packet IDs, scalar open/close payloads or size/timing
   metadata, bounded file rotation and no secrets. Packet interception must remain absent.
6. Test the optional cosmetic RP separately. Check supported slot counts, title/button encoding, lit/unlit
   furnace selectors, texture paths, blank buttons, clicks and normal unrelated forms. It grants no inventory functionality.

## Gates before any live storage implementation

Acquire authoritative BDS stack identities, item descriptors and registry epochs, including block item
runtime IDs, NBT, placement/destruction restrictions, custom items and behavior-pack context. Instrument
exclusive ownership: BDS must not process the same managed request. Session ID/window alone is insufficient
because ItemStackRequest does not carry the open window ID. Determine how delayed old requests, BDS
inventory/cursor IDs and negative request references are fenced across close/reopen/rapid switching.

Capture packet order at low/high latency. Establish graphic/block-actor readiness, open completion,
manual/server close and timeout behavior. Do not use PacketViolationWarning as success or introduce
an unexplained tick delay. Observe actual thread identity. Read latest primary/secondary block layers
and relevant actor data at cleanup in the original dimension; test intervening legitimate block changes.

Perform storage deposit/withdraw round trips before craft/anvil work. Cover full request batches,
split/drag/quick moves, hotbar selection, cursor return with full inventory, invalid counts/IDs/roles,
stale/replayed packets, malformed/truncated packets, unowned actions, rejected requests and consistent
resynchronization. Confirm no world drops occur on aborted commits and every accepted/rejected request
gets the appropriate response. Test custom, renamed, enchanted and nested-data items.

## Vanilla mechanics and persistence comparisons

Follow the crafting and anvil spike requirements in BACKENDS.md. For every action sequence, perform the
same actions at a genuine vanilla station in the same world with the same player state and loaded packs.
Compare input consumption, output count/full metadata, remainders, XP and persistent state.

Then establish actual Ender Chest binding and held-shulker escrow. Attempt movement, swaps, dropping,
nesting, container transfer, death, reconnect and simultaneous access. Verify crash writeback at each
journal/BDS save boundary. Never automatically reissue an ambiguous item.

Only after those gates add processing stations. Measure actual fuel/timing, progress packets, recipes,
XP, online-after-close behavior, offline pause, restart persistence and linked chunk ticking. Validate
maps against actual map state, enchanting against real seeds/offers, beacon effects against real spatial
rules, and crafter disabled slots/activation against native behavior.

Validate real entities/traders/editors against actual ownership, inventory/stock, permissions and lifecycle.
Education interfaces require separate feature-enabled client/world evidence. Internal inventories or
enum names alone never count as proof of a remotely openable native screen.

## Interoperability and performance

Test with actual inventory, protection, anti-cheat, teleport and save/backup plugins enabled. Repeat
concurrent inventory updates between prepare and commit; check event cancellation ordering and cleanup.
An isolated probe plugin has exercised a protection veto. Third-party inventory/protection
packages have not been certified by that test.

Measure server tick percentiles, active sessions, handler time, queue depth, disk latency, dropped
diagnostic samples and memory under realistic concurrency. Scope polling to active sessions/stations.
`research/benchmark.json` measures only a small offline planner operation; it is not a server performance claim.
