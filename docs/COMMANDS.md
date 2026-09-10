> **Legacy v0.4.0 documentation.** This page describes the Python/native-companion implementation. For the native prerelease, use [installation](NATIVE_INSTALLATION.md), [examples](NATIVE_EXAMPLES.md) and [SDK 1.5](NATIVE_SDK.md).

# Commands and permissions

| Command | Use |
| --- | --- |
| `/workstations` | Category browser and selector |
| `/craft`, `/workbench` | Native crafting table |
| `/anvil`, `/stonecutter`, `/grindstone`, `/smithing`, `/loom`, `/cartography` | Native workstation |
| `/ec`, `/enderchest` | Actual online Ender Chest |
| `/workstations open <type>` | Open a catalog entry through the same admission rules |
| `/workstations list` | Show the complete catalog and dispositions |
| `/workstations status` | Runtime/backend/session diagnostics for administrators |
| `/workstations diagnostics on` or `off` | Control bounded sanitized diagnostics |
| `/uidemo`, `/uidemo inventory` | Optional separate dependency example |

Native access requires `remoteworkstations.use` and the relevant
`remoteworkstations.open.<type>` permission. Source access additionally requires
its configured permission; omitting one requires `remoteworkstations.contexts`,
whose default is operator access. Editor requirements also retain native
operator/Creative and administrator checks as applicable.

The complete generated permission/alias metadata is in
[`catalog.py`](../src/endstone_remote_workstations/catalog.py) and
[the capability catalog](CAPABILITIES.md). Register your own source/menu/action
permissions in the consuming plugin; permission names in examples are illustrative.

Aliases are configured before plugin load, so restart after changing `[aliases]`.
Use an empty list to suppress a capability's short commands. Alias conflicts
are checked, and vanilla `/enchant` is never replaced. All aliases retain the
same admission checks as the main command.
