"""Archive last successful firmware/source and next-source diff before compilation."""
from pathlib import Path
from datetime import datetime,timezone
import argparse,difflib,hashlib,json,re,shutil

ROOT=Path(__file__).resolve().parents[2]
def sha(data):return hashlib.sha256(data).hexdigest()
def read_json(path):return json.loads(path.read_text(encoding='utf-8-sig'))
def latest_build():
    base=ROOT/'tests/integration'
    candidates=[base/'build/firmware_results.json']
    candidates+=list(base.glob('pc_board*/*/build/firmware_results.json'))
    candidates+=list(base.glob('firmware-builds/*/build/firmware_results.json'))
    good=[]
    for path in candidates:
        if not path.exists():continue
        r=read_json(path)
        if r.get('exit_code')==0 and r.get('firmware_metadata',{}).get('verified') and (path.parent/'arduino/FluxRail.ino.uf2').exists():
            good.append((datetime.fromisoformat(r['date_local']),path.parent))
    if not good:raise RuntimeError('No verified previous firmware build; archive it explicitly before compile')
    return max(good,key=lambda pair:pair[0])[1]

def create_rollback(build=None,reason='Mandatory archive before new firmware compile'):
    build=Path(build).resolve() if build else latest_build()
    report=read_json(build/'firmware_results.json')
    assert report['exit_code']==0 and report['firmware_metadata']['verified']
    version=report['firmware_metadata']['firmware']['version']
    assert re.fullmatch(r'[A-Za-z0-9_.-]+',version)
    stamp=datetime.now(timezone.utc).strftime('%Y%m%dT%H%M%S%fZ')
    out=ROOT/'backups/firmware-rollback'/version/stamp
    out.mkdir(parents=True,exist_ok=False)
    # Non-code supporting files are current copies; every old .ino/.h/.cpp is
    # reconstructed separately and matched to the actual successful build SHA.
    shutil.copytree(ROOT/'firmware/FluxRail',out/'source/firmware/FluxRail')
    shutil.copytree(ROOT/'firmware/FluxRail',out/'next-source/firmware/FluxRail')
    provenance={};patch=[];changes=[]
    for name,digest in report['source_sha256'].items():
        relative=Path(name).relative_to('firmware/FluxRail')
        candidates=[ROOT/name,build/'arduino/sketch'/relative]
        candidates+=sorted((ROOT/'backups/firmware-rollback').glob('*/*/source/'+name))
        data=None
        for path in candidates:
            if not path.is_file():continue
            raw=path.read_bytes()
            if path.is_relative_to(build/'arduino/sketch') and raw.startswith(b'#line 1 '):
                raw=raw.split(b'\n',1)[1]
            if sha(raw)==digest:data=raw;provenance[name]=str(path);break
        if data is None:raise RuntimeError('Cannot reconstruct verified previous source: '+name)
        dest=out/'source'/name;dest.parent.mkdir(parents=True,exist_ok=True);dest.write_bytes(data)
        current=(ROOT/name).read_bytes() if (ROOT/name).exists() else b''
        if current!=data:
            changes.append(name)
            patch.extend(difflib.unified_diff(data.decode('utf-8').splitlines(True),current.decode('utf-8').splitlines(True),fromfile='old/'+name,tofile='next/'+name))
    # Explicitly include new code files in the diff.
    for path in sorted((ROOT/'firmware/FluxRail').rglob('*')):
        if path.suffix not in ('.ino','.h','.cpp'):continue
        name=path.relative_to(ROOT).as_posix()
        if name not in report['source_sha256']:
            changes.append(name);patch.extend(difflib.unified_diff([],path.read_text(encoding='utf-8').splitlines(True),fromfile='/dev/null',tofile='next/'+name))
            (out/'source'/name).unlink(missing_ok=True)
    artifact=out/'artifacts';artifact.mkdir()
    for suffix in ('.uf2','.elf','.bin','.map'):
        path=build/'arduino'/('FluxRail.ino'+suffix)
        if path.exists():shutil.copy2(path,artifact/path.name)
    assert (artifact/'FluxRail.ino.uf2').is_file()
    for name in ('firmware_results.json','source_link_audit.json','firmware_compile.log','usb_descriptor_results.json'):
        path=build/name
        if path.exists():shutil.copy2(path,artifact/name)
    dependency=ROOT/'dependencies/generated/Adafruit_TinyUSB_Arduino'
    for name,digest in report['dependency_adapter']['generated_sha256'].items():
        assert sha((dependency/name).read_bytes())==digest,'Previous dependency source changed: '+name
    shutil.copytree(dependency,out/'dependency/Adafruit_TinyUSB_Arduino')
    (out/'changes.patch').write_text(''.join(patch),encoding='utf-8')
    readme=f'''# Firmware rollback archive: {version}

Reason: {reason}

- Previous UF2: artifacts/FluxRail.ino.uf2. This archive does not flash it.
- Previous exact code: source/firmware/FluxRail; all build-manifest code SHA verified.
- Next compile input snapshot: next-source/firmware/FluxRail.
- Changes: changes.patch, compared against previous actual build sources.
- Supporting non-code documents are copied from current checkout, not claimed historical.
- Private TinyUSB adapter archived; Arduino-Pico/tool versions are in firmware_results.json.
- Restore source by copying source/firmware/FluxRail into the project after separately
  backing up current work. Review changes.patch first. A firmware rollback requires
  explicitly choosing a UF2 matching the target board and restoring configuration separately.
- Configuration backup/board validation are independent from code/UF2 rollback.

Changed source files: {', '.join(changes) or '(none; rebuild of same source)'}
'''
    (out/'README.md').write_text(readme,encoding='utf-8')
    files=[p for p in out.rglob('*') if p.is_file()]
    manifest={'time_utc':datetime.now(timezone.utc).isoformat(),'reason':reason,'previous_version':version,'previous_build':str(build),'code_provenance':provenance,'verified_previous_source_sha256':report['source_sha256'],'changed_code_files':changes,'files_sha256':{p.relative_to(out).as_posix():sha(p.read_bytes()) for p in sorted(files)},'uploaded':False}
    (out/'manifest.json').write_text(json.dumps(manifest,ensure_ascii=False,indent=2),encoding='utf-8')
    return out

if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--build',type=Path);parser.add_argument('--reason',default='Retrospective complete rollback archive requested by user')
    args=parser.parse_args();print(create_rollback(args.build,args.reason))
