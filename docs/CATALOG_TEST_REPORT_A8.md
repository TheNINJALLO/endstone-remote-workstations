> **Legacy v0.4.0 documentation.** This page describes the Python/native-companion implementation. For the native prerelease, use [installation](NATIVE_INSTALLATION.md), [examples](NATIVE_EXAMPLES.md) and [SDK 1.5](NATIVE_SDK.md).

# Catalog candidate 0.4.0a8

All 69 catalog entries have a backend or a precise disabled/feature-gated
disposition, source requirements, API route and researched packet IDs. There are
42 implemented entry points; 27 remain unsupported or require unavailable client
features. This is an alpha, not completion of every requested gameplay mechanic
or production certification. The generated [catalog](CAPABILITIES.md) is the
source of per-interface scope and limitations.

The tested host is Endstone 0.11.10, BDS 1.26.45.1, protocol 2169, CPython 3.11
Windows x86-64. Client tests use Windows Bedrock 1.26.45, Classic UI, keyboard and
mouse. The public API remains Endstone 0.11.0, `api_version = "0.11"`.
The supplied Linux executable hash matched the requested identity; its callable
native ABI and live client path are not qualified.

## Evidence and scope

| Coverage | Evidence | What was verified |
|---|---|---|
| Complete installed API sweep | `research/catalog-evidence/installed-catalog-index.json` | All 42 implemented entry points on the immutable a8 wheel; matching client close or lectern acknowledgment, exact baseline, all 26 package files, 95 console checks |
| Seven direct workstations | `research/live-evidence/index.json` | Selected native recipes, item/XP consumption, metadata and lifecycle; original wheel identities retained |
| All fifteen linked blocks | `research/catalog-evidence/catalog-open-*-0-4-0a7-result.json` | Installed API open, forced close and exact player baseline; this sweep makes no transaction claim |
| Hopper, beacon, crafter | `research/catalog-evidence/packaged-index.json` | Installed a1 named-item transfer, paid Haste, real disabled-slot persistence |
| Lectern | `research/catalog-evidence/lectern-index.json` | Installed a3 real book/page preservation, manual/forced close and rapid switching |
| Three vehicles | `research/catalog-evidence/entity-index.json` | Installed a4 named/enchanted-item round trips, real hopper pickup, boat movement and removal |
| Horse and donkey | `research/catalog-evidence/equipment-index.json` | Installed a6 saddle/armor, attached storage and actual introduced client actor display |
| Two merchants | `research/catalog-evidence/trade-index.json` | Installed a7 real payment/output, persistent use count, interaction-event veto and active pet-owner revocation |
| Other eight equipment variants | Development `*-equipment.json` and installed a8 sweep | Actual native screens, slot layouts and installed lifecycle; no installed item-transfer qualification |
| Real Ender and custom slot menus | `research/packet-evidence/index.json` | Selected transfers, authoritative conflict rejection, protected icons and callback lifecycle |
| Remaining 27 entries | `research/catalog-disposition-audit.json` | Separate schema/component/native evidence and explicit refusal; no fabricated remote functionality |

Each historical capture retains its tested wheel hash. An unchanged subsystem or
a later catalog description does not turn that evidence into an exact later-wheel
test. `tools/qualify_catalog_opens.py` checks every installed package file before
recording its screen/lifecycle cases. Final package verification and artifact
manifests identify the exact candidate that was installed.

## Failures found and handled

The a5 equipment build required inventory contents too early: native UpdateEquip
arrived before the next-tick InventoryContent. The a6 fix waits for a bounded,
validated actual slot count; mismatch, later resizing and timeout fail closed.

The a7 embedded player inventory ignored forced ContainerClose, resulting in a
five-second protective disconnect. Testing the explicit type, special window ID
and response form did not produce a client close. Manual close released BDS's
context and restored the exact baseline. The a8 fix relinquishes the plugin lease
on cancellation, leaving the real grid/cursor with BDS and requiring normal
manual closure before another native screen opens. It never resets native inputs
or invents a close acknowledgment.

All four installed a8 entry points passed lease cancellation followed by manual
native closure without disconnecting. The other 38 entries passed their managed
open/close paths. Fresh empty fixtures replaced expired or removed test entities;
test-only fire resistance protected undead mounts and water breathing protected
the nautilus during inspection. The product never grants these effects or ownership.

An additional installed a8 input test moved the named Sharpness sword into the
real 2x2 grid, cancelled plugin tracking, and confirmed the item remained there
under vanilla control. Normal manual close returned it with identical NBT, items
and XP. Its capture and native accepted responses are included in the installed
catalog index.

Sign projection displayed a real native editor and emitted a translated save
request. The distant save did not modify the source and retained its native editor
lock. Normal-distance cleanup released the lock; text-persistence control was not
qualified. Sign support remains disabled until native save and editor cleanup are
verified. Book, sign, NPC and command text is excluded from production diagnostics.

## Remaining release qualification

Held shulker/bundle identity, escrow, nesting and crash-safe writeback are absent.
Privileged editors have no validated remote save adapter. Education interfaces
require an admitted feature-enabled world/client. Personal processing policy is
not implemented; linked stations use actual BDS chunk ticking, including ordinary
offline world processing when chunks tick.

Full-inventory recovery, cross-store BDS/SQLite crash reconciliation, broad custom
item/recipe coverage, every merchant profession/restock/discount combination,
multiple simultaneous clients, mobile/Pocket/controller paths and broader plugin
coexistence remain separate checks. Native death behavior can drop transient
inputs even with keepInventory; the vanilla comparison reproduced this, and no
durable overflow service is claimed. See [recovery](RECOVERY.md) and preserved
[native](TEST_REPORT.md) and [packet](PACKET_TEST_REPORT.md) reports.

Defaults leave all gameplay adapters disabled. Enabling a candidate backend is
an explicit opt-in; the 69-entry catalog must not be advertised as 69 usable
vanilla screens or a production-complete release.
