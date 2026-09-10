> **Legacy v0.4.0 documentation.** This page describes the Python/native-companion implementation. For the native prerelease, use [installation](NATIVE_INSTALLATION.md), [examples](NATIVE_EXAMPLES.md) and [SDK 1.5](NATIVE_SDK.md).

# Linked block API (1.2)

`open_linked()` opens a real server-authorized block. BDS retains that block's
inventory, native recipes, fuel, progress, XP, bookshelf power and beacon state.
The plugin projects a display near the requesting player while keeping the real
block as BDS's transaction source. The player is never teleported.

```python
from endstone_remote_workstations.api import BlockContext, get_api

def on_enable(self):
    self.ui = get_api(self, minimum=(1, 2))

def open_owned_furnace(self, player, saved_source):
    # Resolve saved_source on the server after checking ownership/protection.
    return self.ui.open_linked(
        player, 'furnace',
        BlockContext(saved_source.dimension, saved_source.position,
                     permission='myplugin.furnace'),
    )
```

The caller must be an enabled Endstone plugin with `remote_workstations` in its
dependencies. Positions come from server configuration or authorized server
logic. Never pass untrusted client coordinates or permission names directly.

Both the UI permission and the source permission are required. If the source
omits a permission, `remoteworkstations.contexts` defaults to operators only.
Protection guards, source type and source permission are checked again during
opening and while a session is active. A changed or unloaded source closes the UI.
Permission/guard revocation is checked at the native backend's 250 ms interval.

Enable `[native] enabled=true` and `[native.linked] enabled=true` only on the
admitted Windows build after reviewing the evidence for the desired interface.
Each short command can use a fixed source:

```toml
[native.linked.sources.furnace]
dimension = "Overworld"
position = [100, 64, 100]
permission = "myplugin.furnace"
```

Sources must be in the player's current dimension and remain loaded. Processing
follows ordinary BDS chunk ticking, including after UI close and while the owner
is offline if the chunk keeps ticking. This is linked world storage, with normal
shared-block access; it is not a separate personal station or an offline timer.
No ticking areas are created automatically. A beacon uses its real pyramid and
spatial effect range. Enchanting uses the actual table's bookshelf power. Crafter
activation remains ordinary world redstone; the UI only edits real inputs and
disabled slots. Trapped chests do not promise virtual redstone at the display.

The exact-build companion installs a per-instance native manager vtable adapter
that widens the usability distance argument. The original BDS implementation
still validates block/container identity and slots. Full function hashes and all
22 vtable entries, prefix and adjacent boundary are checked before adaptation.
The adapter module is pinned for manager lifetime; global BDS tables and code
are not patched. Foreign opens and unrelated control packets are not rewritten.

The catalog distinguishes implemented paths from completed live qualification.
Opening a screen alone does not qualify its processing, persistence or recovery.

## Lecterns and native block state

`open_linked(player, 'lectern', context)` requires a loaded lectern containing a
real book. Its displayed content comes from the actual native block actor update.
Book text is not included in diagnostics. The book remains at the source; this
interface reads it and changes its page. Held-book editing is a separate catalog
capability. Ordinary world interaction controls insertion and removal.

The native page packet handler has an inline physical-distance check. Owned
lectern controls are cancelled and queued on the game thread. The companion
retains the handler's real block/book checks, player interaction check and page
bounds, then calls the exact native page setter. That setter updates the actual
page, comparator behavior and dirty-save state. The client page byte selects a
displayed spread; Classic UI can show two text pages at once. The provided total
must match the current native book. No book content is written by this adapter.

A lectern has no BDS inventory manager. Its managed session therefore remains
active until client dismissal, lifecycle cleanup or a foreign screen. The tested
Windows client sends its unchanged page on dismissal and does not echo
ContainerClose. An unchanged page is a no-op that retires the view. Forced close
uses a session-specific ordered ping after the close and restoration; a foreign
screen retires only the old projection. Recently used book display positions are
retired for sixty seconds to keep delayed page packets out of later views. Page
queues are bounded to sixteen and recheck source access before each native call.

Crafter disabled slots and lectern/beacon state are copied from native
BlockActorData updates. Copies are accepted only after the final outgoing event
confirms delivery at the owned display position. Beacon displays also copy
complete valid tiers of the real pyramid into initially empty client space;
source tier changes close the view. Effects and payment remain bound to the real
beacon and its world range.
