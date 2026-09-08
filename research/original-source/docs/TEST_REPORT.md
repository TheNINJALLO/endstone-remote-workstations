# Test report: RemoteWorkstations 0.2.0rc1

This is the preserved native-workstation report. For the 0.3.0a2 Ender and inventory
menu adaptations, see [the packet adapter report](PACKET_TEST_REPORT.md). Earlier
case results retain their original artifact hashes and do not certify every later wheel.

The full requested release is **not yet complete**. Seven real Windows native workstation
backends have selected gameplay tests. Native gameplay defaults to disabled and requires
explicit candidate opt-in. Unsupported commands explain their gap and leave inventory alone.

## Verified evidence

| Check | Result and limits | Evidence |
|---|---|---|
| Automated suite | 120 tests passed before final packaging; final build reruns all tests | research/junit.xml, research/tests.log |
| Public API baseline | 44/44 audited symbols present in pinned Endstone 0.11.0 | research/api-baseline-check.json |
| Native bridge | Real Endstone Player crosses the existing CPython 3.11/pybind11 type registry; exact Windows BDS/runtime and called code checked | native/windows_bridge.cpp, research/native-compatibility.json |
| Crafting | Actual 3x3 chest recipe consumed 16 planks for two chests; repeated crafting retained excess plank | live-evidence craft captures; real-block comparison included |
| Anvil | Enchanted sword rename cost one XP level; Sharpness retained; zero-XP output rejected; normal/forced close returned input | live-evidence anvil captures |
| Stonecutter | One stone became one stone brick; excess stone returned, including installed-wheel cursor placement test | live-evidence stonecutter captures |
| Grindstone | Sharpness removed from one iron sword; visible XP gained; subsequent saved state had one XP point | grindstone-native-disenchant.json and isolated post-restart snapshot |
| Smithing | One template, ingot and diamond pickaxe became one Netherite pickaxe | smithing-native-upgrade.json |
| Loom | Two banners plus two red dyes became two red-border banners; pattern NBT persisted | loom-native-banner.json |
| Cartography | Real world map scaled with one paper; excess paper returned; scaled map rendered and survived restart | cartography-native-scale.json |
| Packaged commands | Native stonecutter, grindstone, smithing, loom, cartography and anvil opened from Minecraft chat; input return preserved metadata | rc1-prefixed captures and screenshots |
| Final console checks | 95/95 catalog and alias checks passed on the final candidate | research/live-console-matrix-rc1.json |
| Final wheel crafting | Chat `/craft`, repeated shift-click recipe consumed 16 of 17 planks for two chests; one plank retained; no active session after close | final-wheel-craft.json |
| Final wheel disconnect | Enchanted named sword held in native anvil survived kick/reconnect once, with identical NBT and XP | final-wheel-disconnect-input.json |
| Final wheel shutdown | Sword held in anvil survived clean BDS shutdown (exit 0), restart and reconnect with identical inventory/NBT/XP | final-wheel-shutdown-input.json plus separate final-wheel-shutdown-rejoined.json |
| Final headless test | All 17 commands observed; clean exit 0; portable gameplay disabled | research/windows-headless-smoke-rc1.json |
| Host rejection | Companion refuses module execution outside BDS | research/native-host-rejection.json |
| Admission | Explicit command permission denial and an isolated protection-plugin veto produced no container or transaction packets | rc1-permission-denial.json, rc1-protection-denial.json |
| Teleport cleanup | Same-dimension test teleport closed anvil and returned the unchanged enchanted named sword | rc1-anvil-teleport-return.json |
| Evidence assertions | 23 actual before/after capture comparisons passed; unrelated item counts, slots and NBT hashes unchanged | research/live-evidence/index.json, tools/collect_live_evidence.py |
| Negative acceptance evidence | Two open blockers reproduced and preserved separately: remote Ender destination rejection and transient-input death recovery | research/release-blockers/index.json, tools/collect_release_blockers.py |
| Offline storage/recovery | Atomic batches, replay rejection, SQLite model round trip and four child-process crash boundaries | tests/test_transactions.py, tests/test_recovery.py; these are not BDS storage/crash tests |
| Packaging | Final artifact manifest records wheel/source hashes and rebuild equality; no BDS runtime/world included | dist/artifact-manifest.json |

All native gameplay ran on the allowlisted isolated rw-smoke world, with Endstone 0.11.10,
BDS 1.26.45.1 / protocol 2169, Windows Bedrock 1.26.45, keyboard/mouse and Classic UI.
The companion owns no items; BDS exclusively processes requests and returns native inputs.
No resource pack is required for the native screens.

Development spike captures, Python development refresh tests and installed-wheel tests are
distinguished in the evidence index. The installed-wheel hash for the recorded rc1 operations is
9426a0b627d54c038371c7200aa95709ae0cedae63cb3e8d8ab8210532d05fe2.
The three final-wheel checks above ran on the installed final scheduling build, SHA-256
1df2bab9cfb1299e18e37747d76ec7285bdb16b4697211cedd9df2ad6e37615a.
All 17 installed package files match that wheel. Earlier captures retain their earlier hashes.
Windows foreground activation was recovered with a standalone Alt input followed by
AppActivate; intermittent foreground refusal still occurs and the helper refuses such input.

## Bugs found and addressed

- Sending restoration inside a packet callback corrupted the active outgoing serialization
  buffer. Callbacks now record state only; the scheduler sends restoration and close packets.
- Opening immediately after chat caused stale Close(-1) responses. A captured client latency
  echo after the projection and a measured 500 ms transition interval are both required.
- The Windows latency echo multiplies the timestamp by 1,000,000 and preserves the server bit;
  the actual fixture is tested. Unsolicited or mismatched replies cannot activate a session.
- Permission and protection checks repeat immediately before native creation.
- Native state checks measured about 1.2 ms median and roughly 5 ms p95 for one test player.
  Replacing provider initialization did not materially improve this result. Current scheduling
  uses a fair rotation, a 2 ms soft budget and 250 ms state-check interval; an in-flight native
  call may exceed the budget. Active polling timing is reported by /workstations status.
  These are measurements and bounds, not a multi-player load certification.
- The final-wheel crafting sample retained 1,200 active polls: median 98.8 microseconds,
  p95 5,833.7 microseconds, maximum 14,615.4 microseconds. This confirms that the soft
  budget can be exceeded; it is not a hard 2 ms latency guarantee. See
  research/final-wheel-active-poll.json.

## Release requirements still open

- Safe live storage, actual remote Ender Chest usability and held-shulker identity/escrow/writeback.
- Fuel/timing/persistence for processing stations; contextual entity/trade/editor backends.
- Complete recipe/metadata coverage: anvil repair/merge/prior work, crafting overrides/remainders,
  grindstone curses/XP distribution, smithing trims, loom limits and map copying/locking.
- Full-inventory cursor overflow into durable recovery storage; output abuse, stale/replayed
  live requests, high latency, custom items and rapid switching under load.
- Dimension changes, plugin reload and BDS crash-save boundaries across the complete native
  matrix. Input-held disconnect and clean shutdown passed for anvil; death recovery failed
  the requested durable-return policy, as detailed below.
- Mobile touch, controller, Pocket UI, Linux execution and third-party interoperability/load.

The native Ender Chest experiment bound the actual 27-slot player inventory. Every native
slot model was usable, but the outer model required a real block actor. A development-only
per-instance usability adaptation passed its own owner, distance and binding checks while
BDS still rejected item placement with result 50 (`failedtovalidatedstslot`). The real Ender
Chest accepted the same sword and returned it unchanged. Observational tracing confirmed
the real open context: variant 2, ContainerType 0, actual player owner and BlockPos. No
transaction validator was bypassed, no substitute vault exists, and the experiment is absent
from the release wheel. Remote destination validation remains unresolved.
An additional fresh-process experiment used the actual Ender inventory's native base
lifetime publisher when no real chest owner was bound. It also returned result 50 without
transferring the sword. Both adaptations are disabled in normal probe builds; reproducing
them requires the explicit development-only `--ender-experiment` build flag. They are not
a release backend.

A subsequent read-only response stack located BDS request execution at RVA 0x274bed0.
Static analysis followed the transfer's destination validator into the role-7 container
resolver. Its fallback looks up a world block actor at the screen position before selecting
the player's Ender inventory for actor type 23. The air projection lacks that actor.
This inner branch is a static inference, not an instruction-by-instruction live trace.
The native source constructor also accepts an explicit container map; its ownership and
lifecycle requirements have not been established for a remote binding. Eight complete
function hashes, checked branch instructions and the failed capture are recorded in
research/ender-destination-analysis.json, reproducible with tools/inspect_ender_destination.py.
No validation result, inventory content or real block was changed by this tracing.

With `keepInventory=true`, both the final native anvil and the real vanilla anvil dropped
the held input sword into the world on death. Their post-respawn inventories lacked the
sword. The original dropped entity was recovered separately in each test with its exact NBT;
no replacement was issued. These operator recovery steps do not pass plugin recovery.
Endstone's pinned Player::drop hook only emits PlayerDropItemEvent while the player is alive,
so simply cancelling that public event cannot cover this death path. KeepInventory was
restored to false and the test player ended with the original inventory and no managed session.
Native durable overflow/death recovery and crash recovery beyond BDS saves are not claimed.

The historical 0.1.0 smoke/client reports remain as earlier evidence. Current capabilities and
backend behavior are described by this report, CAPABILITIES.md and NATIVE_WINDOWS.md.
