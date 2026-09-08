# Native jigsaw metadata editor

The Windows companion exposes the real JigsawBlock editor through
`ui.open_linked(player, "jigsaw", BlockContext("Overworld", (x, y, z)))`.
It uses the loaded source block, preserves its native metadata, registers the
real BDS window/source, and projects a temporary client display nearby.

Enable `[native.jigsaw] enabled = true` together with native and linked support
only on the pinned Endstone 0.11.10 / BDS 1.26.45.1 / protocol 2169 build.
The player must be a Creative operator with `remoteworkstations.admin`,
`remoteworkstations.open.jigsaw`, and the source context permission. Protection
guards run before opening, before submission, and while the lease remains live.
Production configuration leaves this editor disabled.

The bounded packet-56 codec accepts the actual JigsawBlock schema: name, target,
target pool, final block state, joint and selection/placement priorities. It
rejects duplicate/unknown tags, malformed UTF-8, trailing or truncated data,
oversized strings and a mismatched source. Coordinates alone are translated;
BDS validates and applies the metadata. No custom UI or item transaction engine
is substituted. Structure generation and export are separate operations.

The native adapter pins the actual block actor table, loaded source identity,
window allocator, apply/load functions and operator-block admission. It extends
the same two block-actor distance gates used by signs. Only a currently admitted
player/source/generation with one accepted native packet can pass remotely.
The companion retains the real shared packet while a native filter callback is
pending, and rejects retired submissions even after the player moves nearby.
Cancellation revokes the lease before closing the screen and restoring terrain.

Windows screen opening and the real client metadata schema have been observed.
Unit tests cover codec boundaries, source checks, operator/Creative/admin/guard
revocation, duplicate and late packets, cancellation, and native completion.
Installed a10 save/reopen and cancellation qualification is pending. Mobile,
controller, multi-client and delayed-filter stress remain separate tests.
