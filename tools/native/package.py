"""Package only explicitly selected native prerelease artifacts; never a world/BDS."""
import argparse,hashlib,json,re,zipfile
from pathlib import Path,PurePosixPath
ROOT=Path(__file__).resolve().parents[2]
VERSION='0.5.0-native.1'
def digest(data):return hashlib.sha256(data).hexdigest()
def main():
    parser=argparse.ArgumentParser();parser.add_argument('--output',type=Path,required=True)
    parser.add_argument('--revision',required=True);parser.add_argument('--qualification',type=Path,required=True)
    args=parser.parse_args();assert re.fullmatch(r'[0-9a-f]{40}',args.revision)
    qualification=json.loads(args.qualification.read_text(encoding='utf-8'))
    assert qualification['version']==VERSION and qualification['all_ui_acceptance']=='not-qualified'
    assert qualification['builds']['linux-x64']['tests_passed']==23
    assert qualification['builds']['windows-x64']['tests_passed']==21
    assert qualification['builds']['windows-debug']['tests_passed']==21
    assert qualification['sanitizers']['tests_passed']==23 and qualification['sanitizers']['fuzz_runs']==[100000,100000,100000]
    args.output.mkdir(parents=True,exist_ok=True);archives=[]
    def read(path):
        assert path.is_file() and not path.is_symlink(),path
        data=path.read_bytes();assert len(data)<=64*1024*1024;return data
    def tree(path,prefix):
        return {str(PurePosixPath(prefix)/file.relative_to(path).as_posix()):read(file)
                for file in sorted(path.rglob('*')) if file.is_file()}
    common={name:read(ROOT/name) for name in ['LICENSE','NOTICE']}
    common['QUALIFICATION.json']=(json.dumps(qualification,indent=2)+'\n').encode()
    common['README.md']=('Onistone VCF '+VERSION+' — EXPERIMENTAL PRERELEASE\n\n'
        'Not production-qualified. Use a separate Endstone instance from v0.4.0.\n'
        'Install provider binaries under plugins/. Examples and SDK are separate.\n'
        'https://github.com/TheNINJALLO/endstone-remote-workstations/blob/v'+VERSION+'/docs/NATIVE_INSTALLATION.md\n').encode()
    for platform,folder,extension in [('linux-x64','linux-x64-dev','so'),('windows-x64','windows-release','dll')]:
        base=ROOT/'dist'/folder;provider='endstone_onistone_vcf.'+extension
        names=[provider,'endstone_vcf_catalog_showcase.'+extension,'endstone_vcf_native_passthrough.'+extension]
        for name in names:
            location='plugins' if name==provider else 'examples';raw=read(base/location/name)
            assert raw.startswith(b'\x7fELF' if extension=='so' else b'MZ')
            assert digest(raw)==qualification['artifacts'][platform][name],name
        packages={'plugin':{'plugins/'+provider:read(base/'plugins'/provider)},
                  'examples':tree(base/'examples','examples'),'sdk':tree(base/'sdk','sdk'),
                  'symbols':tree(base/'symbols','symbols')}
        assert packages['symbols'],'Missing own debug symbols'
        config={'schema_version':1,'experimental_original_linux':False,'experimental_original_windows':False,
                'experimental_inventory_writes':False,'experimental_held_storage':False}
        packages['plugin']['config.example.json']=(json.dumps(config,indent=2)+'\n').encode()
        if extension=='so':packages['plugin']['elf-report.txt']=read(base/'elf-report.txt')
        for kind,files in packages.items():
            files.update(common);files.update(tree(base/'licenses','licenses'))
            filename=f'onistone-vcf-{VERSION}-{platform}-{kind}.zip';target=args.output/filename
            assert not target.exists(),'Refusing to overwrite an existing release archive'
            manifest={'version':VERSION,'sdk_version':'1.5.0','source_revision':args.revision,'platform':platform,
                      'package':kind,'prerelease':True,'all_ui_acceptance':'not-qualified',
                      'files':{name:digest(value) for name,value in sorted(files.items())}}
            files['MANIFEST.json']=(json.dumps(manifest,indent=2)+'\n').encode()
            with zipfile.ZipFile(target,'x',compression=zipfile.ZIP_DEFLATED,compresslevel=9) as archive:
                for name,data in sorted(files.items()):
                    path=PurePosixPath(name);assert not path.is_absolute() and '..' not in path.parts
                    assert not any(p in {'.git','.runtime','.research','worlds','__pycache__'} for p in path.parts)
                    info=zipfile.ZipInfo('onistone-vcf-'+VERSION+'/'+name,(2026,9,10,0,0,0))
                    info.external_attr=0o100644<<16;info.compress_type=zipfile.ZIP_DEFLATED
                    archive.writestr(info,data)
            with zipfile.ZipFile(target) as archive:
                assert archive.testzip() is None
                for name,expected in manifest['files'].items():assert digest(archive.read('onistone-vcf-'+VERSION+'/'+name))==expected
            archives.append((filename,digest(target.read_bytes())))
    (args.output/'SHA256SUMS').write_text(''.join(f'{hash_value}  {name}\n' for name,hash_value in archives),encoding='ascii')
    (args.output/'release-manifest.json').write_text(json.dumps({'version':VERSION,'source_revision':args.revision,
        'prerelease':True,'all_ui_acceptance':'not-qualified','archives':dict(archives)},indent=2)+'\n',encoding='utf-8')
    print(f'Validated {len(archives)} native archives, embedded manifests and SHA256SUMS.')
if __name__=='__main__':main()
