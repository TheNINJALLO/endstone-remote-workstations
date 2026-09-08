# Compatibility and support scope

Release **0.4.0** is the regular release of the implemented RemoteWorkstations
feature set. Dependency API **1.5** retains the earlier forms/actions contracts.

## Platform matrix

| Environment | Status |
| --- | --- |
| Windows x86-64 / CPython 3.11 / Endstone 0.11.10 / BDS 1.26.45.1 | Native target; exact loaded hashes required |
| Windows Bedrock 1.26.45 Classic keyboard/mouse | Qualified client profile |
| Linux native workstations | No native companion implemented |
| Portable wheel on Linux | Python code included; Linux runtime and packet screens unqualified |
| Other clients, touch, controller and multiplayer | Untested; not covered by this release's qualification |
| Other BDS/Endstone builds | Native access refused |

## Implemented scope

The catalog has 69 entries: 53 implemented entry points and 16 explicit unavailable
dispositions. Some entries share a native screen, such as the player inventory's
armor/offhand/recipe-book views. Source-based screens require actual authorized
blocks/entities. Education/NPC features retain their native world and source
requirements. Capability flags do not create those prerequisites.

## Known limitations

- Held shulkers, bundles and books are unavailable. No item-backed escrow,
  nesting locks or durable writeback are supplied.
- The journal and BDS saves have no shared durable commit marker. Exactly-once
  recovery after a crash is not provided. Quarantine does not automatically
  return or mint native items.
- Native editor hooks lack hardware shadow-stack and exception-path qualification.
  Their settings remain separate, disabled opt-ins. Command-block minecart
  command text can appear blank on reopening.
- Full recipe/enchantment variants, full-inventory recovery, sustained load and
  broad plugin coexistence are not fully qualified. The API can dispatch your
  custom functions; it does not implement custom recipe/enchantment engines.
- Personal/offline virtual processing is not implemented. Real processing follows
  the actual source's loaded/ticking world state.
- Earlier chemistry tests did not verify the invalid Lab Table recipe's garbage
  world output. Native world effects and accepted NPC commands cannot be rolled
  back by cancelling a UI.
- Player inventory cancellation releases plugin tracking; the player closes the
  shared native inventory normally.

These limits remain visible despite removing the alpha version suffix.
[Validation](VALIDATION.md) records exactly what was exercised.

## Reporting an issue

Include plugin, Endstone, BDS, Python and client versions; server OS; UI kind;
reproduction steps; relevant configuration and a sanitized error excerpt.
State whether another inventory/protection plugin is installed. Remove account
IDs, addresses, commands containing secrets and player item data from reports.
Use [GitHub Issues](https://github.com/TheNINJALLO/endstone-remote-workstations/issues).
