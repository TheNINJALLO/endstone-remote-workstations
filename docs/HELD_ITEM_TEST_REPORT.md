# Held-item foundation validation: 0.5.0.dev1

This development build adds inspection, detached snapshots and journal
primitives. It does not enable held-item UIs or native inventory mutation.

| Check | Result |
| --- | --- |
| Full automated suite | 637 passed, zero failures/errors/skips |
| New held-item tests | 72 passed |
| Subprocess deaths against a simulated external save | 5 boundaries passed |
| Installed provider command smoke | 17 commands; clean shutdown |
| Real runtime detached ItemStack round trips | 36 built-in held-item types passed |
| Installed/source/wheel identity | All 36 package files matched |
| Wheel RECORD | All member sizes and hashes verified |
| Published 0.4.0 files | All seven release asset hashes unchanged |

The runtime probe covered 17 colored/undyed shulkers, 17 bundles and two books.
It constructed detached real Endstone ItemStacks with names, lore, nested
contents/enchantments, custom numeric/string/array tags and identity candidates.
Reconstruction retained all tested public fields and metadata; the originals
were unchanged. The probe acquired dependency API 1.6 during `on_enable`.

Source/API tests cover permission, lifecycle, owner-thread admission, source
replacement, journal ownership conflicts, stale revisions, replay, queue record
detachment and idempotent cleanup. The crash tests kill real Python subprocesses
while a separate file simulates BDS persistence. Recovery never rewrites that
file or issues items.

Artifact: `endstone_remote_workstations-0.5.0.dev1-py3-none-any.whl`

SHA-256: `0d1f3f0d2fa552e251bbf57d2c55ff55d22992dafcef03044abf2aa5075df71c`

Server: BDS 1.26.45.1, Endstone 0.11.10, Windows x86-64 / Python 3.11.
The headless runtime loader hash is
`914aa15d7d8f627b77ff6c73ddc587b5347050221846fa3d2c5bf553baa83c59`;
the BDS executable hash is
`92d09c7b74ac6a9805bafc166d8e0a13ac9e5db73dbbb0819e5a14093699d44f`.
These tests exercise public Endstone ItemStack/NBT APIs, not new native hooks.

No live held-item UI, physical inventory lock, nesting executor, item writeback,
native identity assignment or actual BDS crash boundary is qualified. No client
joined this headless test, and no player inventory was modified. Linux native
support is unchanged. See [implementation and remaining work](HELD_ITEMS.md).

Private evidence is retained locally in `research/held-foundation-validation.json`,
the referenced test XML/headless logs and the detached-runtime report. It is
excluded from source distributions.
