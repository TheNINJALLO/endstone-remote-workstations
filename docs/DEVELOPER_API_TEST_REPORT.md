# Dependency API 1.0: installed alpha test report

RemoteWorkstations **0.3.0a1** and the separate **rw_ui_example 0.1.0** consumer
passed the selected SDK checks below on September 6, 2026. This is an alpha,
not completion of the original full workstation/storage release matrix.

The environment was Endstone 0.11.10, Windows BDS 1.26.45.1/protocol 2169, and
Windows Bedrock 1.26.45 using Classic UI with keyboard/mouse. Tests ran in the
isolated `rw-smoke` world. The BDS SHA-256 was
`92d09c7b74ac6a9805bafc166d8e0a13ac9e5db73dbbb0819e5a14093699d44f`.

The installed Windows provider wheel SHA-256 is
`ddee854d3be5309696ff4c750e5338f5af3a22bc078143cadaf4861a88abdf8f`.
All **19 package files** match that wheel. The separately installed consumer also
matches its built wheel; its identity and the bounded captures are recorded in
the private `research/sdk-evidence/index.json` SDK evidence index.
The existing native Python backend and C++ bridge were unchanged by this SDK.

## Automated verification

**138 tests passed**, including 18 SDK cases. They cover dependency/version and
thread admission, namespaced registrations, explicit export isolation, execution
permission checks, deferred dispatch, invalid/duplicate/stale/foreign responses,
native generation ownership, owner disable, replacement screens, exceptions,
bounded queues and disconnect/reconnect invalidation. These cases use controlled
test doubles; they do not establish malicious-client or multi-plugin live parity.

All **52 referenced public symbols** are present in the pinned Endstone 0.11.0
baseline. Portable and Windows provider wheels rebuilt byte-for-byte identically.
Neither wheel contains the development probe, BDS executable or Ender experiment.

## Minecraft checks

| Installed-wheel check | Observed result |
|---|---|
| Consumer dependency acquisition | Enabled after the provider and acquired API 1.0. |
| Custom root menu and exported function | Menu rendered; one selection ran `rw_ui_example:hello` once. |
| Submenu navigation | Root menu opened the custom submenu; its button opened the real native anvil. |
| Native anvil input return | Existing `Remote Blade` sword entered the anvil and returned on close with the same Sharpness I, NBT, count and slot. Three captured single-result BDS item responses reported success. |
| Direct `open_native` | Anvil, craft, stonecutter, grindstone, smithing, loom and cartography each rendered and emitted one open and one close callback. Seven outgoing native container opens matched the expected screen types. |
| Unsupported Ender request | `/uidemo open ec` emitted a failed `native:enderchest` ticket with the existing result-50 transaction-binding reason. It created no eighth native screen. |
| Capability menu | The consumer displayed the seven currently available native workstations. |
| Form dismissal/reopen | Escape dismissed a form, and a subsequent menu opened correctly. Six forms opened in total, with four selections and two dismissals. |
| Final inventory and cleanup | All three captures have exactly matching before/after snapshots: item counts, slots, metadata, XP, position and dimension. Native backend inspection found zero sessions, zero pending requests and a ready manager. |

The sword NBT SHA-256 before and after was
`2a07f1397622d79cac90786d96f50639dbe51b8f7cfe2aee003b4d39f83e9472`.
No recipes were executed during the seven direct-open checks. Earlier recipe,
disconnect and shutdown tests remain attached to their original candidate hashes
in [the historical native report](TEST_REPORT.md).

Private evidence includes rendered custom menu, submenu and real anvil input screenshots,
native screen captures, consumer lifecycle log and sanitized packet/inventory JSON.
The probe's allowlist excludes ActionForm packets: form evidence consists of
reviewed screenshots and actual consumer callbacks, not a recorded form wire trace.
Re-run `tools/collect_developer_api_evidence.py` against this preserved live run to
verify package identity and assertions. It does not itself execute new game tests.

## Remaining release work

Custom menus currently use ActionForm buttons. Draggable packet inventories,
actual remote Ender Chest transactions, held shulker escrow, processing, contextual
editors and durable native input recovery remain unavailable. The SDK does not
register new vanilla recipes or enchantment IDs. Registered callbacks can call a
consumer's own gameplay implementation.

Linux runtime, mobile/controller clients, live malformed-client replay, cross-plugin
interoperability and crash/save-boundary qualification remain open. The previous
native timing sample also exceeded its soft 2 ms budget at the 95th percentile;
the new menu SDK has no server-load certification. Existing Ender transaction and
death/recovery blockers remain in the private `research/release-blockers/index.json`.
