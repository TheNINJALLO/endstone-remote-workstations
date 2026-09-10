"""Generate native wiki pages without replacing stable v0.4.0 reference pages."""
import argparse,re
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
REPO='https://github.com/TheNINJALLO/endstone-remote-workstations'
TAG='v0.5.0-native.1'
PAGES={'NATIVE_INSTALLATION.md':'Native-Installation','NATIVE_EXAMPLES.md':'Native-Examples',
       'NATIVE_HELD_STORAGE.md':'Native-Held-Shulker','NATIVE_SDK.md':'Native-SDK',
       'NATIVE_BUILDING.md':'Native-Building','NATIVE_INVENTORY_WRITES.md':'Native-Recovery',
       'RELEASE_NATIVE_0_5_0.md':'Native-Release','CAPABILITY_MATRIX.md':'Native-Capabilities'}
def main():
    parser=argparse.ArgumentParser();parser.add_argument('--output',type=Path,default=ROOT/'wiki');args=parser.parse_args()
    folder=args.output;folder.mkdir(parents=True,exist_ok=True)
    for source,title in PAGES.items():
        path=ROOT/'docs'/source
        def link(match):
            url=match.group(1)
            if url.startswith(('https://','http://','#','mailto:')):return match.group(0)
            bare,_,anchor=url.partition('#');target=(path.parent/bare).resolve();suffix='#'+anchor if anchor else ''
            if target.parent==ROOT/'docs' and target.name in PAGES:url=REPO+'/wiki/'+PAGES[target.name]+suffix
            else:
                prefix='https://raw.githubusercontent.com/TheNINJALLO/endstone-remote-workstations/'+TAG if target.suffix in ('.png','.svg') else REPO+'/blob/'+TAG
                url=prefix+'/'+target.relative_to(ROOT).as_posix()+suffix
            return ']('+url+')'
        (folder/(title+'.md')).write_text(re.sub(r'\]\(([^)]+)\)',link,path.read_text(encoding='utf-8')),encoding='utf-8',newline='\n')
    home=f'''![Onistone VCF](https://raw.githubusercontent.com/TheNINJALLO/endstone-remote-workstations/{TAG}/docs/assets/native-banner.svg)

# RemoteWorkstations and Onistone VCF

Choose the documentation for the implementation you installed.

## Native experimental prerelease

[Download {TAG}]({REPO}/releases/tag/{TAG}) · [Install]({REPO}/wiki/Native-Installation) · [Examples]({REPO}/wiki/Native-Examples) · [SDK 1.5]({REPO}/wiki/Native-SDK)

Linux `.so` and Windows `.dll`, standalone native consumers and an experimental Linux held-shulker editor. The full native/custom catalog and automatic cross-save recovery are incomplete. [Read qualification limits]({REPO}/wiki/Native-Release).

## Stable RemoteWorkstations v0.4.0

[Stable download]({REPO}/releases/tag/v0.4.0) · [Versioned documentation]({REPO}/tree/v0.4.0/docs) · [Legacy installation]({REPO}/wiki/Installation)

The stable Python/native-companion APIs and feature counts do not apply to the native prerelease. Use separate server instances.
'''
    (folder/'Home.md').write_text(home,encoding='utf-8',newline='\n')
    titles=['Home',*PAGES.values(),'Installation','Developer-Examples','Commands-and-Permissions','Compatibility-and-Limits']
    (folder/'_Sidebar.md').write_text('\n'.join(f'- [{name.replace("-"," ")}]({REPO}/wiki/{name})' for name in titles)+'\n',encoding='utf-8',newline='\n')
    (folder/'_Footer.md').write_text(f'[Stable v0.4.0]({REPO}/releases/tag/v0.4.0) · [Native prerelease]({REPO}/releases/tag/{TAG}) · [Issues]({REPO}/issues)\n',encoding='utf-8',newline='\n')
    print(f'Generated {len(PAGES)} native pages and shared navigation.')
if __name__=='__main__':main()
