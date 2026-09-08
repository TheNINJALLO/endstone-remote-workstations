# Configuration and real sources

Enable the base native backend before individual linked/entity capabilities.
Flags do not create blocks, entities, ownership or Education features.

| Capability | Required settings and context |
| --- | --- |
| Linked blocks | native + native.linked; actual loaded server-selected BlockContext |
| Entity inventories/equipment/merchants | native + native.entities; actual authorized EntityContext |
| Native editors | The relevant editor flag, native source permission and native privileged admission |
| Chemistry | native + native.linked + native.chemistry; real matching blocks and Education world features |
| Agent | native + native.entities + native.agent; actual native owner and Education features |
| NPC | native + native.npc; real NPC and installed scenes; any native world-feature requirements |
| Protected inventory menus / Ender Chest | packet_inventory; admitted client and permissions |

Use server-owned contexts. A configured source permission is additional to UI
permission; omitting it requires remoteworkstations.contexts. Keep blocks loaded
and obey native owner/type/dimension checks. Native processing follows actual
world ticking; accepted world changes are not undone when a screen closes.

[Block configuration](https://github.com/TheNINJALLO/endstone-remote-workstations/blob/main/docs/LINKED_BLOCKS.md) ·
[Entity configuration](https://github.com/TheNINJALLO/endstone-remote-workstations/blob/main/docs/LINKED_ENTITIES.md) ·
[Chemistry](https://github.com/TheNINJALLO/endstone-remote-workstations/blob/main/docs/CHEMISTRY.md) ·
[NPC dialogue](https://github.com/TheNINJALLO/endstone-remote-workstations/blob/main/docs/NPC_DIALOGUE.md)
