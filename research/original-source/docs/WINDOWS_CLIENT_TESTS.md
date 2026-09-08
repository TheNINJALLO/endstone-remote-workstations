# Historical 0.1.0 Windows client test — 2026-09-06

This is the earlier baseline report. See [TEST_REPORT.md](TEST_REPORT.md) for the
subsequently implemented seven native workstation backends and current live evidence.

Minecraft was launched, joined the isolated Endstone server, and exercised the features present in
this research release. **There are still no usable remote storage or workstation backends.**
Their gameplay integration cases are blocked, not passed.

## Environment

- Endstone 0.11.10; public plugin API baseline 0.11.0 / metadata `0.11`.
- Supplied Windows BDS 1.26.45.1, SHA-256 `92d09c7b74ac6a9805bafc166d8e0a13ac9e5db73dbbb0819e5a14093699d44f`.
- Minecraft for Windows package 1.26.4501.0, displaying v26.45; installed in `C:\XboxGames\Minecraft for Windows\Content`.
- Server protocol 2169. The matching client build joined successfully; login payloads were not captured.
  This is limited client-path evidence, not certification of arbitrary raw workstation packets.
- Windows keyboard/mouse path through Win32 input automation; Classic inventory UI. No physical mobile/controller run.
- `127.0.0.1:29169`; allowlisted, online authentication, one-player limit, LAN advertisement disabled.
- Disposable world `rw-smoke`, seed 2169; vanilla behavior packs. Only `remote_workstations` was loaded.
- The plugin wheel was installed from the server's `plugins` folder. Global Python plugins were excluded.

## Observed results

| Test | Result | Evidence under `research/` |
|---|---|---|
| Client login | Joined the exact supplied server | `client-connection-result.png`, `windows-live-baseline.log` |
| Standard action form | Category menu and storage submenu rendered; selecting chest returned its disabled explanation | `client-workstations-menu.png`, `client-storage-menu.png`, `client-chest-rejection.png` |
| Member permissions | Status and diagnostics denied; command-block interface denied | `client-member-status.png`, `client-member-diagnostics.png`, `client-member-editor.png` |
| Operator permissions | Status succeeded and diagnostics could be stopped from the client | `client-operator-status.png`, `client-operator-diagnostics-off.png` |
| Unknown type | Explanation returned; visible test stone stack remained 64 | `client-unknown.png` |
| Every catalog entry and short alias | **95/95 console checks** returned the expected disabled explanation | `live-console-matrix.json` |
| Client alias samples | `/craft`, `/anvil`, `/ec`, `/shulker`, `/etable` each returned its own explanation | `client-*-rejection.png` |
| Native player inventory, diagnostics enabled | Move, split, quick-move and close/reopen completed; observed split 48+16 returned to 64 | `client-vanilla-split-confirmed.png`, `client-vanilla-quickmove.png`, `client-inventory-reopened.png` |
| Graceful stop and reconnect | Plugin disabled; server exited 0; restarted client retained 64 stone | `windows-live-baseline-state.json`, `client-reconnected.png` |
| Vanilla enchant preservation | With test-only cheats enabled, `/enchant @s sharpness 1` succeeded; native tooltip displayed Sharpness I | `client-vanilla-enchant-applied.png`, `client-enchanted-item-preserved.png` |
| Optional cosmetic pack | Download/join succeeded; 27-slot selector and category callback rendered. Entries have hover labels and no icons | `client-pack-download.png`, `client-cosmetic-menu.png`, `client-cosmetic-storage.png` |
| Capture controls | 163 sanitized records, including 5 ItemStackRequest and 5 ItemStackResponse size-only observations; 9 raw open/close records | `windows-client-capture-summary.json`, `windows-client-packets.jsonl` |
| Live control codecs | Three captured open/close sequences round-trip through local and pinned dependency codecs | `tests/fixtures/windows-live-controls-2169.json`, `tests/test_protocol.py` |

The native inventory smoke verifies that normal BDS inventory handling continued while the plugin
observed packets. It does not test the plugin's offline transaction planner against live BDS inventories.
The captured player-inventory opens used type byte 255 and the corresponding closes used 247;
the new fixture tests preserve this distinction. Authentication, item NBT and editor payloads were not recorded.
All 163 retained observations reported the owning thread; callbacks skipped by the diagnostic filter are not measured.

A zombie killed the disposable test player during the first inventory attempt. The world was then made
peaceful with daylight and mob spawning disabled, dropped test items were cleared, and a known stack
was supplied for the repeated smoke. This was ordinary vanilla death, not a remote-session recovery test.
The optional pack's blank icons are a presentation limitation; standard menus are restored for the final server.

## Blocked coverage

The 54-case integration record has four completed command/lifecycle cases and 50 blocked cases.
Remote storage transactions, crafting/anvil behavior, Ender Chest/shulker handling, processing,
contextual editors, and BDS crash reconciliation cannot run because their gameplay backends do not exist.
Preexisting-plugin alias collisions, unrelated action forms and inventory/protection plugin interoperability
were not exercised with a second compatible plugin. Mobile, controller, Pocket UI, older clients,
latency/load and Linux runtime tests remain unavailable or blocked. No gameplay capability is promoted.

See `research/windows-client-integration.json` for each case and its reason, and
`research/windows-client-tests.json` for the narrower client smoke results.

## Using the running test server

The final server uses standard menus, diagnostics off, cheats off, peaceful difficulty, online authentication
and the existing allowlisted test account. The test account retains operator access on this isolated server.
The optional resource pack is detached from this test world's pack stack.

```powershell
# Run from the workspace root; these address only the isolated controller.
.venv\Scripts\python.exe tools\live_test_server.py status
.venv\Scripts\python.exe tools\live_test_server.py command "workstations status" --expect "active gameplay sessions:"
.venv\Scripts\python.exe tools\live_test_server.py command stop --expect "Server stop requested"
```

Runtime files stay under `.runtime/windows-smoke-isolated`. The helper uses no service installation,
firewall changes or production paths. Screenshots and full local runtime logs are retained in the workspace;
the source release includes the machine-readable report and small sanitized protocol fixtures.
