The structure adapter opens the actual vanilla structure screen through a
server-authorized source. It requires the exact Windows companion, a Creative
operator, `remoteworkstations.admin`, ordinary use/open permissions and the
source permission. It is disabled by default.

```toml
[native.structure]
enabled = true
maximum_volume = 262144
```

Enable `native.enabled` and `native.linked.enabled` as well. A dependency uses:

```python
ticket = ui.open_linked(player, "structure",
    BlockContext(player.dimension.name, (x, y, z), "myplugin.structures"))
```

BDS owns filtering, Save/Load, template storage, rotation, mirroring, animation,
integrity and world changes. The adapter validates bounded controls and
translates the owned display position and selection offsets to the real source. Template requests
follow the real server export/query handler; the client chooses its export
location. Product code never opens an OS file picker or writes a client file.

Offsets shown in the vanilla screen are relative to the displayed preview block.
They can differ from the stored source offsets because the two blocks occupy
different coordinates. The adapter preserves the same absolute selection:
`source + stored_offset = display + shown_offset`. For example, a source at
`(6,100,37)` with offset `(2,-1,0)` appears at display `(23,101,-4)` with shown
offset `(-15,-2,41)`. Both select `(8,99,37)`. Submitted offsets are converted back
before native application. Moving the viewer between sessions changes the shown
offsets while preserving the selected region. The preview can render only client
loaded terrain, as with ordinary distant selections in the vanilla editor.

Native editor-data copies and moves retain revocable source/player authority.
Every final native application rechecks current access. Only one filtered update
may be pending; a second pending update closes and revokes the session to avoid
reordering Save/Load actions. Completed metadata updates remain applied, as in
vanilla. Ticket cancellation stops future updates; it does not roll back completed
edits or world changes. Protection guards authorize the source operation, not
each affected block in its volume.

The packet budget is 64 KiB. X/Z sizes are limited to 64 and Y to 384; native
dimension-height clamps remain active. `maximum_volume` accepts 1 through
1,572,864 blocks. Native resource names exclude OS paths and traversal segments.
Non-finite settings and invalid enums are rejected.

The a15 physical control saved a 5x5x5 structure and exported a file whose parsed
NBT equals the native network template. Installed a16 passed seven cases: remote
Save, export, a one-block Load, cancellation, protection revocation, Creative
revocation and Survival rejection. Inventory and XP stayed unchanged. See
`research/catalog-evidence/structure-editor-index.json` for its exact wheel.
Installed a17 passed the coordinate regression: the displayed selection saved the
actual stone, Load placed it at a destination verified as air beforehand, and the
exported file exactly matched the native response at `(9,99,37)`. Normal Back
saved its metadata but did not release the session because the client sent no
close packet. This failed case is preserved in `structure-preview-index.json`.

The a18 implementation also observes resumed gameplay. After two neutral client
input frames, actual movement releases the remaining editor lease on the server
thread. A submitted native update must finish first. Until gameplay resumes, an
explicit ticket cancellation or normal session timeout can release the lease.
Deferred-filter stress, other devices and Linux remain unqualified.
