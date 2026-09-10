> **Legacy v0.4.0 documentation.** This page describes the Python/native-companion implementation. For the native prerelease, use [installation](NATIVE_INSTALLATION.md), [examples](NATIVE_EXAMPLES.md) and [SDK 1.5](NATIVE_SDK.md).

# Native Agent inventory

The a23 Windows companion opens the actual 27-slot Agent inventory through
`Actor::openContainerComponent`. The native component checks ownership before
creating its normal container manager. Bedrock validates and applies all items.

Enable `native.enabled`, `native.entities.enabled`, and `native.agent.enabled`.
The actual world must have Education features enabled. Supply a loaded Agent
already owned by the requesting player; RemoteWorkstations does not spawn Agents,
assign ownership, or alter world feature flags.

```python
from endstone_remote_workstations.api import EntityContext, get_api

ui = get_api(self, minimum=(1, 5))
ticket = ui.open_entity(player, "agent",
    EntityContext(player.dimension.name, agent, "myplugin.agent"))
```

Configured command sources use `[native.entities.sources.agent]`, with the
dimension, persistent `actor_id`, and source permission. Source and interface
permissions and protection guards apply. Native owner and runtime identity are
checked before opening and again during use. Every item request rechecks current
authority before passing the unchanged payload to Bedrock. Cancellation revokes
storage access while allowing the existing native cursor to return to the player
or drop through the normal native cleanup. The Agent retains its stored items.

The selected development test used the supplied 1.26.45.1 Windows server and
Endstone 0.11.10. The normal summon command excluded Agent, but the public native
actor factory created a disposable fixture in the separate Education test clone.
An unowned open created no manager. Assigning fixture ownership through the real
native setter allowed the manager; this mutation exists only in the test probe.
The remote test deposited a named enchanted sword, closed and reopened the
container, and withdrew the sword with exact inventory/NBT/XP preservation.

The standard Windows client renders this native container as a chest screen.
Its source is the actual Agent, and the source inventory survives view closure.
Installed a23 passed five selected cases: transfer/reopen, API cancellation with
the sword held, native owner revocation with an Agent item held, rejection of an
unowned source, and a protection veto with the sword held. All completed with
exact inventory, item auxiliary data, NBT and XP. Each cancellation/revocation
accepted its one native cursor return. `agent-installed-a23-index.json` binds the
results to the installed wheel and its 34 verified package files. The original
Agent and owner survived the preceding clean restart; this does not qualify crash
persistence, concurrent clients or other devices. No Education coding controls or
Agent movement commands are exposed.
