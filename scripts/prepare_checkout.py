"""Restore a verified historical build baseline for a fresh checkout; no compilation/upload."""
from pathlib import Path
import argparse,hashlib,json,shutil,sys

ROOT=Path(__file__).resolve().parents[1]

def sha(path):return hashlib.sha256(path.read_bytes()).hexdigest()

def prepare(root=ROOT,check_only=False):
    root=Path(root).resolve()
    packages=sorted((root/'backups/firmware-rollback').glob('*/*/manifest.json'),key=lambda p:p.parent.name)
    if not packages:raise RuntimeError('No historical rollback package in checkout')
    manifest_path=packages[-1]
    archive=manifest_path.parent.resolve()
    manifest=json.loads(manifest_path.read_text(encoding='utf-8-sig'))
    for name,digest in manifest['files_sha256'].items():
        path=(archive/name).resolve()
        if not path.is_relative_to(archive) or not path.is_file() or sha(path)!=digest:
            raise RuntimeError('Rollback manifest mismatch: '+name)
    artifacts=archive/'artifacts'
    report=json.loads((artifacts/'firmware_results.json').read_text(encoding='utf-8-sig'))
    metadata=report.get('firmware_metadata',{})
    if report.get('exit_code')!=0 or not metadata.get('verified') or metadata['firmware']['version']!=manifest['previous_version']:
        raise RuntimeError('Historical build is not a verified rollback baseline')
    for name,digest in report['source_sha256'].items():
        if sha(archive/'source'/name)!=digest:
            raise RuntimeError('Historical source does not match build: '+name)
    for suffix in ('.uf2','.elf'):
        if not (artifacts/('FluxRail.ino'+suffix)).is_file():raise RuntimeError('Missing historical '+suffix)
    build=root/'tests/integration/build'
    dependency=root/'dependencies/generated/Adafruit_TinyUSB_Arduino'
    archived_dependency=archive/'dependency/Adafruit_TinyUSB_Arduino'
    if dependency.exists():
        for name,digest in report['dependency_adapter']['generated_sha256'].items():
            if not (dependency/name).is_file() or sha(dependency/name)!=digest:
                raise RuntimeError('Existing private dependency differs; preserve it and inspect before restoring')
    info={'archive':archive.relative_to(root).as_posix(),'historical_version':manifest['previous_version'],
          'manifest_files_verified':len(manifest['files_sha256']),'compiled':False,'uploaded':False}
    if check_only:return {**info,'status':'verified_without_writing'}
    if (build/'firmware_results.json').exists():
        return {**info,'status':'existing_build_preserved','note':'Existing build report not overwritten'}
    targets=[]
    for suffix in ('.uf2','.elf','.bin','.map'):
        source=artifacts/('FluxRail.ino'+suffix)
        if source.is_file():targets.append((source,build/'arduino'/source.name))
    for name in ('source_link_audit.json','firmware_compile.log','usb_descriptor_results.json'):
        source=artifacts/name
        if source.is_file():targets.append((source,build/name))
    if any(target.exists() for _,target in targets):
        raise RuntimeError('Build contains files without a report; preserve and inspect them before bootstrap')
    if not dependency.exists():shutil.copytree(archived_dependency,dependency)
    for source,target in targets:
        target.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(source,target)
        if sha(source)!=sha(target):raise RuntimeError('Baseline copy mismatch: '+target.name)
    shutil.copy2(artifacts/'firmware_results.json',build/'firmware_results.json')
    return {**info,'status':'historical_baseline_restored','note':'Copied historical evidence, not a fresh build/test'}

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--check-only',action='store_true')
    args=parser.parse_args()
    print(json.dumps(prepare(check_only=args.check_only),ensure_ascii=False,indent=2))

if __name__=='__main__':
    sys.stdout.reconfigure(encoding='utf-8')
    main()
