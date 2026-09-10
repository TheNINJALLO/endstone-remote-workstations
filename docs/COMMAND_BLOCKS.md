> **Legacy v0.4.0 documentation.** This page describes the Python/native-companion implementation. For the native prerelease, use [installation](NATIVE_INSTALLATION.md), [examples](NATIVE_EXAMPLES.md) and [SDK 1.5](NATIVE_SDK.md).

# Native command-block editor

The Windows companion exposes the real source through the dependency API:

```python
ticket = ui.open_linked(player, "commandblock",
                        BlockContext("Overworld", (6, 100, 35), "myplugin.editor"))
```

Enable `native.enabled`, `native.linked.enabled` and
`native.commandblock.enabled` in the test server configuration. All default to
false. The player must be a Creative operator with `remoteworkstations.admin`,
`remoteworkstations.use`, `remoteworkstations.open.commandblock` and the source
permission. Protection providers can veto opening and revoke an active editor.

The real block remains at its original position. The client receives the native
source metadata at a temporary display position, followed by the real native
window. The bounded packet-78 decoder translates only that owned display's
position. BDS retains command, name filtering, mode, conditional/redstone state,
tick delay, tracking, execution and persistence behavior. Commands are powerful;
source access means permission to edit that actual command block.

Exact-build hooks track the native command payload constructor, copies,
destructor and application. Deferred copies share revocable session authority.
Application requires the same active source and generation, a living connected
player, current Creative/operator and source permissions, and the native
operator-block check. Cancelling the ticket revokes this authority before
closing and restoring the display. Unmanaged native saves keep their original
processing path. Command text is excluded from product diagnostics.

After the first open, native hooks and their module remain pinned until server
exit so a filter worker cannot resume through a freed trampoline. Plugin shutdown
revokes every lease. Restart the server before re-enabling command editors.

The client projection preserves the actual source block state, including Repeat,
Chain and conditional mode. This adapter covers all three block sources. Installed
a12 tests covered save, cancellation, protection revocation and Creative-mode
revocation. Deferred filter-worker stress remains unqualified.

The command-minecart entry uses an actual live `EntityContext`:

```python
ticket = ui.open_entity(player, "commandblockminecart",
                        EntityContext(player.dimension.name, cart, "myplugin.editor"))
```

Enable `native.entities.enabled` and `native.commandblockminecart.enabled`, with
the same native, Creative, operator and administrator requirements. The opening
permission is `remoteworkstations.open.commandblockminecart`. The actual native
GameMode interaction opens the editor. The open packet retains the persistent
ActorUniqueID; saves retain the runtime ID. Both identifiers must resolve the same
live native command cart on every guarded save. No inventory slots are fabricated.
Native delay values are admitted from 0 through 99,999.

This client sends no close packet when an unchanged command editor is dismissed.
Use `ticket.cancel()` when the consumer finishes. An observed return to movement
after neutral editor input also retires the view, and the configured session
deadline remains the final limit for an idle client. Once a save is submitted,
movement cannot cancel the native application in progress. The same cart has a
60-second reopening delay because entity saves contain neither a window ID nor a
session generation; late saves during that interval are rejected.

The local vanilla control also reopened a command cart with an empty command
field although native metadata retained its saved command. Readable command
prefill remains unqualified. Installed a15 cart save, cancellation, guard
revocation, Creative-mode revocation and Survival rejection passed, alongside
block/sign/jigsaw regressions. The exact-wheel ten-case evidence is in
`research/catalog-evidence/command-editor-index.json`; it does not qualify
deferred filter-worker stress or other clients.
