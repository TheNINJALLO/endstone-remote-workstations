"""Build versioned GitHub wiki pages from the maintained user documentation."""
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[1]
REPO = 'https://github.com/TheNINJALLO/endstone-remote-workstations'
RAW = 'https://raw.githubusercontent.com/TheNINJALLO/endstone-remote-workstations/main'
PAGES = {'INSTALLATION.md':'Installation','EXAMPLES.md':'Developer-Examples',
    'COMMANDS.md':'Commands-and-Permissions','SUPPORT.md':'Compatibility-and-Limits',
    'TROUBLESHOOTING.md':'Troubleshooting','BUILDING.md':'Building-from-Source',
    'VALIDATION.md':'Validation'}


def main():
    folder = ROOT/'wiki'
    folder.mkdir(exist_ok=True)
    for source,title in PAGES.items():
        path = ROOT/'docs'/source
        def replace(match):
            url = match.group(1)
            if url.startswith(('https://','http://','#','mailto:')):
                return match.group(0)
            target = (path.parent/url.split('#')[0]).resolve()
            anchor = '#'+url.split('#',1)[1] if '#' in url else ''
            if target.parent == ROOT/'docs' and target.name in PAGES:
                url = REPO+'/wiki/'+PAGES[target.name]+anchor
            else:
                prefix = RAW if target.suffix in ('.png','.svg') else REPO+'/blob/main'
                url = prefix+'/'+target.relative_to(ROOT).as_posix()+anchor
            return ']('+url+')'
        text = re.sub(r'\]\(([^)]+)\)',replace,path.read_text(encoding='utf-8'))
        (folder/(title+'.md')).write_text(text,encoding='utf-8')
    home = f'''![RemoteWorkstations]({RAW}/docs/assets/banner.svg)

# RemoteWorkstations wiki

Native Bedrock workstations and dependency API 1.5 for Endstone.

**[Download the release]({REPO}/releases/latest)** · **[Repository]({REPO})**

## For server owners

- [Installation]({REPO}/wiki/Installation): choose the correct wheel, enable backends and upgrade.
- [Commands and permissions]({REPO}/wiki/Commands-and-Permissions): expose only authorized capabilities.
- [Configuration and real sources]({REPO}/wiki/Configuration-and-Sources): loaded blocks, actual entities and world features.
- [Troubleshooting]({REPO}/wiki/Troubleshooting): diagnose admission, dependencies and lifecycle issues.

## For plugin developers

- [Developer examples]({REPO}/wiki/Developer-Examples): native screens, Ender Chest, protected menus, exports, blocks, entities and NPCs.
- [API reference]({REPO}/blob/main/docs/DEVELOPER_API.md): methods, tickets, ownership and limits.
- [Building from source]({REPO}/wiki/Building-from-Source).
- [Runnable example plugin]({REPO}/tree/main/examples/dependency_plugin).

## Know the supported scope

The catalog has 53 implemented entry points. Windows native execution is pinned
to Endstone 0.11.10 / BDS 1.26.45.1 and the admitted Windows client. The portable
wheel contains no Linux native companion. Held-item interfaces and exactly-once
crash recovery are not supplied.

[Compatibility and limits]({REPO}/wiki/Compatibility-and-Limits) · [Validation]({REPO}/wiki/Validation)
'''
    (folder/'Home.md').write_text(home,encoding='utf-8')
    sources = f'''# Configuration and real sources

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

[Block configuration]({REPO}/blob/main/docs/LINKED_BLOCKS.md) ·
[Entity configuration]({REPO}/blob/main/docs/LINKED_ENTITIES.md) ·
[Chemistry]({REPO}/blob/main/docs/CHEMISTRY.md) ·
[NPC dialogue]({REPO}/blob/main/docs/NPC_DIALOGUE.md)
'''
    (folder/'Configuration-and-Sources.md').write_text(sources,encoding='utf-8')
    titles = ['Home','Installation','Developer-Examples','Commands-and-Permissions',
              'Configuration-and-Sources','Compatibility-and-Limits','Troubleshooting','Building-from-Source','Validation']
    (folder/'_Sidebar.md').write_text('\n'.join(f'- [{t.replace("-"," ")}]({REPO}/wiki/{t})' for t in titles)+'\n')
    (folder/'_Footer.md').write_text(f'[Repository]({REPO}) · [Releases]({REPO}/releases) · [Report an issue]({REPO}/issues)\n')
    print(f'Generated {len(titles)} wiki pages plus navigation.')


if __name__ == '__main__':
    main()
