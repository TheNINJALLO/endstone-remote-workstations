> **Legacy v0.4.0 documentation.** This page describes the Python/native-companion implementation. For the native prerelease, use [installation](NATIVE_INSTALLATION.md), [examples](NATIVE_EXAMPLES.md) and [SDK 1.5](NATIVE_SDK.md).

# Real sign editors

API 1.4 opens an authorized, loaded sign in the player's current dimension:

```python
from endstone_remote_workstations.api import BlockContext, get_api

ui = get_api(self, minimum=(1, 4))
ticket = ui.open_sign(player,
    BlockContext(player.dimension.name, (6, 100, 31), "my_plugin.signs"),
    front=False, on_close=self.sign_closed)
```

Enable `native.enabled`, `native.linked.enabled` and `native.signs.enabled` in
the exact Windows build. Interface permission, source permission, protection
guards, native game-mode checks, real editor ownership and wax state apply.
`front` selects the editor side. There is no synthetic container/window ID.

The native Player method acquires the real editor lock and emits OpenSign later
through the block actor. The display is projected into nearby empty client space.
A strict, bounded parser validates the submitted sign schema before coordinates
are translated back to the real source. BDS retains text validation, filtering,
author identity, persistence and native sign behavior.

Two exact-build hooks wrap the original distance comparisons before and after
text filtering. They admit one authorized save for the player, source and request
generation. Native shared packet ownership prevents pointer reuse while filtering
is pending. A cancelled generation remains rejected even if the player later
moves within vanilla editing distance or opens another sign.

Cancelling a ticket revokes its native lease, applies only the sign's unchanged
authoritative state through the native sign method to release the editor lock,
and restores the temporary client block. Removing that display closes the Windows
sign editor. An ordered client acknowledgement gates subsequent managed opens.
Delayed saves targeting retired display positions are rejected for 60 seconds.
Text and native TextOwner identifiers are excluded from product diagnostics.

Ordinary standing/wall signs are admitted by exact native actor identity. Hanging
signs require their separate native actor contract. Linux/mobile/controller
qualification and installed API evidence remain separate from the successful
native prototype front/back save tests in
`research/catalog-evidence/sign-native-hook-probe-index.json`.
