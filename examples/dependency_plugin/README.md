> **Legacy Python integration.** Use the [native C++ examples](../../docs/NATIVE_EXAMPLES.md) for v0.5.0-native.1.

# RemoteWorkstations dependency example

This separate Endstone plugin imports only the public RemoteWorkstations API.
It uses the API 1.1 subset, works with provider 0.4.0 / API 1.5, and declares
the Endstone load dependency.

Build from the repository root:

```powershell
.venv\Scripts\python.exe -m build --wheel --no-isolation --outdir dist examples/dependency_plugin
```

Place `endstone_rw_ui_example-0.2.0-py3-none-any.whl` alongside the provider wheel
in the isolated server's `plugins` directory. The provider's documented native
runtime/client admission and opt-in configuration still apply. The example's
`rw_ui_example.use` permission defaults to operators.

Use `/uidemo` in Minecraft to run an exported function, navigate custom button
menus or open available native workstations. `/uidemo open anvil` demonstrates
direct native access and lifecycle callbacks. `/uidemo inventory` opens a protected
slot menu with two pages, exported functions and native/Ender navigation;
`/uidemo ender` opens the player's actual Ender Chest. Both packet commands require
the provider's explicit packet opt-in. `/uidemo status` prints the last
32 lifecycle/action records and also works from the server console.

The public export is `rw_ui_example:hello`. Another enabled dependency consumer
can call `ui.invoke(player, "rw_ui_example:hello")`; the returned Future completes
on a server tick with `"hello-complete"`. Do not wait on it from the game thread.
The example's other actions remain private to its own namespace.

No items or XP are granted. Add gameplay logic to your own registered callbacks;
the protected menu icons select functions and never become player-owned items.
Callbacks do not automatically register new vanilla recipes or enchantments.
See [the API reference](../../docs/DEVELOPER_API.md) for the full contract.
