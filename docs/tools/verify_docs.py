"""Verify current/archived documentation without building or contacting hardware."""
from pathlib import Path
import argparse,hashlib,json,re,zipfile,sys
from urllib.parse import unquote
sys.stdout.reconfigure(encoding='utf-8')

ROOT=Path(__file__).resolve().parents[2]
ARCHIVE=ROOT/'docs/archive/2026-10-05'
BASELINE=ROOT/'backups/docs-before-reorganization-2026-10-05'
def digest(path): return hashlib.sha256(path.read_bytes()).hexdigest()

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--audit-snapshot',action='store_true',help='Also enforce the 2026-10-05 reorganization code/evidence baseline')
    args=parser.parse_args()
    documents=list((ROOT/'docs').rglob('*.md'))
    documents += [ROOT/'AGENTS.md',ROOT/'README.md',ROOT/'RUN_INTEGRATED.md',ROOT/'REVIEW_FULL_2026-10-04.md',ROOT/'web/README.md',ROOT/'dependencies/README.md',BASELINE/'BACKUP-README.md']
    documents += list((ROOT/'firmware/FluxRail').rglob('README.md'))
    missing=[];links=0;generated_links=[]
    for doc in documents:
        text=doc.read_text(encoding='utf-8-sig')
        for m in re.finditer(r'\[[^\]]*\]\((<[^>]+>|[^)]+)\)',text):
            target=m.group(1).strip('<>')
            if re.match(r'^[A-Za-z][A-Za-z0-9+.-]*:',target) or target.startswith('#'):continue
            filepart=unquote(target.partition('#')[0])
            if not filepart:continue
            links+=1
            resolved=(doc.parent/filepart).resolve()
            if not resolved.exists():
                record={'document':doc.relative_to(ROOT).as_posix(),'target':target}
                relative=resolved.relative_to(ROOT).parts if resolved.is_relative_to(ROOT) else ()
                if (len(relative)>2 and relative[0]=='tests' and 'build' in relative[1:-1]) or relative[:2]==('dependencies','generated'):
                    generated_links.append(record)
                else:
                    missing.append(record)
    protected=json.loads((BASELINE/'protected-files.json').read_text(encoding='utf-8'))
    # The user added versioned build outputs after the documentation snapshot.
    expected_changed={'tests/integration/compile_firmware.py'}
    changed=[name for name,sha in protected.items() if not (ROOT/name).is_file() or digest(ROOT/name)!=sha]
    unexpected=[name for name in changed if name not in expected_changed]
    manifest=json.loads((ARCHIVE/'ARCHIVE_MANIFEST.json').read_text(encoding='utf-8'))
    archive_errors=[]
    with zipfile.ZipFile(BASELINE/'original-documents.zip') as z:
        zip_error=z.testzip()
        for record in manifest['files']:
            if hashlib.sha256(z.read(record['original_path'])).hexdigest()!=record['original_sha256'] or digest(ROOT/record['archive_path'])!=record['archive_sha256']:
                archive_errors.append(record['original_path'])
    naming=json.loads((ARCHIVE/'ARTIFACT_NAMING_CHECK.json').read_text(encoding='utf-8'))
    naming_ok=naming['passed'] and naming['compile_script_sha256']==digest(ROOT/'tests/integration/compile_firmware.py')
    report={'date_local':'2026-10-05','snapshot_audit':args.audit_snapshot,'document_count':len(documents),'local_links_checked':links,'missing_links':missing,'archived_count':len(manifest['files']),'archive_errors':archive_errors,'snapshot_zip_error':zip_error,'protected_count':len(protected),'changed_existing_code_or_evidence_since_snapshot':changed,'changes_outside_reorganization_scope':unexpected,'new_code':'docs/tools/verify_docs.py (documentation check only)','artifact_naming_check_matches_current_script':naming_ok,'functional_tests_run':False,'compiled':False,'uploaded':False,'passed':not(missing or archive_errors or zip_error) and (not args.audit_snapshot or (not unexpected and naming_ok))}
    report['missing_generated_outputs']=generated_links
    (ARCHIVE/'DOCUMENTATION_CHECK.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
    print(json.dumps(report,ensure_ascii=False,indent=2))
    return 0 if report['passed'] else 1

if __name__=='__main__':raise SystemExit(main())
