# Installation and upgrades

[Home](../README.md) · [Examples](EXAMPLES.md) · [Troubleshooting](TROUBLESHOOTING.md)

## Windows native installation

Use Windows x86-64, CPython 3.11, Endstone **0.11.10**, BDS **1.26.45.1**
and protocol **2169**. Native and packet screens currently admit Windows
Bedrock **1.26.45** clients. A similar protocol number is not sufficient: the
native companion checks the loaded executable and runtime hashes.

1. Stop the server and back up its world and plugin data.
2. Download `endstone_remote_workstations-0.4.0-cp311-cp311-win_amd64.whl`
   from [Releases](https://github.com/TheNINJALLO/endstone-remote-workstations/releases/latest).
3. Keep one RemoteWorkstations wheel in `plugins/`. Remove the previous wheel
   from that directory when upgrading; keep `plugins/remote_workstations/`.
4. Start the server once. Endstone loads the plugin and creates its configuration.
5. Stop and edit the existing `plugins/remote_workstations/config.toml`.
   Set `enabled = true` under `[native]` for native screens and under
   `[packet_inventory]` for the real Ender Chest and protected inventory menus.
   Do not paste duplicate TOML section headings into the generated file.
6. Restart. Join with the admitted client and use `/workstations status`,
   `/craft`, `/anvil` and `/ec` with the appropriate permissions.

These backends are disabled by default so operators choose which capabilities
to expose. Unimplemented interfaces remain unavailable after opting in.

## Runtime dependencies

The wheel declares `bstream==1.0.1` and `rapidnbt==1.3.5`. If they are not
already resolved by your Endstone installation, use **the server's Python**:

```powershell
python -m pip install --ignore-installed --prefix "C:/path/to/server/plugins/.local" bstream==1.0.1 rapidnbt==1.3.5
```

For an offline server, download wheels matching the server's Python/platform
into a wheelhouse on a connected machine, transfer it, then use `--no-index
--find-links <wheelhouse>` with the same install command. Codec wheels belong in
Python's package installation, not directly in Endstone's `plugins/` folder.

## Portable package and Linux

`endstone_remote_workstations-0.4.0-py3-none-any.whl` contains the portable
Python implementation. Install that wheel for Linux evaluation, with matching
Linux builds of its declared dependencies. **There is no Linux native companion.**

The Python API, forms, named actions and capability catalog are included.
Native workstation, source/editor and native NPC entry points remain unavailable.
The packet backend is included but Linux runtime behavior has not been qualified;
it still restricts clients to Windows Bedrock 1.26.45. Keep native features disabled
on Linux. Do not rename or install the Windows wheel as a Linux binary.

## Enable real sources

Additional flags are independent:

| Setting | Enables |
| --- | --- |
| `[native.linked]` | Authorized real block sources |
| `[native.entities]` | Authorized real entity inventories/equipment/merchants |
| `[native.signs]`, `[native.jigsaw]`, `[native.commandblock]`, `[native.commandblockminecart]`, `[native.structure]` | Individual privileged editor adapters |
| `[native.chemistry]` | Four real chemistry tables in a world with Education features |
| `[native.agent]` | The existing owner's Agent inventory, with Education features and entity support |
| `[native.npc]` | Actual NPC dialogue and installed native scenes |

Set a flag's `enabled` key to true only for the features you want. Flags do not
create blocks, entities, ownership or world features. Supply server-selected
contexts through the API or configure real source coordinates/ActorUniqueIDs.
See [linked blocks](LINKED_BLOCKS.md), [entities](LINKED_ENTITIES.md),
[chemistry](CHEMISTRY.md) and [NPC dialogue](NPC_DIALOGUE.md).

## Try the separate example plugin

Copy `endstone_rw_ui_example-0.2.0-py3-none-any.whl` into `plugins/` alongside
the provider and restart. Its explicit dependency is `remote_workstations`.
As an operator, run `/uidemo` for a form or `/uidemo inventory` for two inventory
pages, a callback button, anvil navigation and Ender Chest navigation.
The example's API 1.1 subset works with provider API 1.5.

## Upgrading and rollback

Stop cleanly before replacing a wheel. Preserve worlds and the complete plugin
data directory. Compare new configuration keys with your existing file; existing
settings are not overwritten on enable. Roll back using the old wheel and the
matching pre-upgrade backup when needed. Do not restore only the SQLite journal
against an independently rolled-back world: there is no cross-store crash commit
protocol. [Recovery contract](RECOVERY.md).
