"""One-time packaging of the agreed Web reference; never writes production files."""
import hashlib
import json
import re
import subprocess
import zipfile
from datetime import datetime, timezone
from pathlib import Path
import os

repo = Path(__file__).resolve().parents[3]
template = repo / 'design/web-template-2026-10-05'
preview = repo / 'design/copy-preview-2026-10-05'
manifest_path = template / 'manifest.json'
archive = repo / 'design/FluxRail-Web-template-2026-10-05-v1.zip'
if manifest_path.exists() or archive.exists():
    raise SystemExit('Fixed package exists. Preserve it and create a new version.')
if subprocess.check_output(['git', 'branch', '--show-current'], cwd=repo, text=True).strip() != 'test':
    raise SystemExit('Freeze must use the agreed test branch.')
if not (template / 'reviews/LUNA_REVIEW_重构交接.md').is_file():
    raise SystemExit('Luna handoff review is required before freezing.')

def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()

def snapshot_markdown(source, target):
    text = source.read_text(encoding='utf-8-sig')
    def relocate(match):
        label, dest = match.groups()
        if ':' in dest or dest.startswith(('#', '/')):
            return match.group(0)
        local, sep, fragment = dest.partition('#')
        resolved = (source.parent / local).resolve()
        relative = Path(os.path.relpath(resolved, template)).as_posix()
        return f'[{label}]({relative}{sep}{fragment})'
    target.write_text(re.sub(r'(?<!!)\[([^\]]+)\]\(([^)]+)\)', relocate, text), encoding='utf-8')

snapshot_markdown(repo / 'docs/Web重构交接_2026-10-05.md', template / 'HANDOFF.md')
snapshot_markdown(repo / 'web修改.md', template / 'CHANGELOG.md')
baseline = json.loads((template / 'baseline/production-web-before-preview.json').read_text(encoding='utf-8-sig'))
for entry in baseline:
    if sha(repo / entry['path']) != entry['sha256']:
        raise SystemExit('Production file differs: ' + entry['path'])
same_files = ['index.html', 'style.css', 'preview.css', 'shell.js']
same_files += [p.relative_to(template).as_posix() for p in sorted((template / 'assets').glob('*')) if p.is_file()]
for name in same_files:
    if sha(template / name) != sha(preview / name):
        raise SystemExit('Template no longer matches agreed preview: ' + name)
files = [{'path': p.relative_to(template).as_posix(), 'bytes': p.stat().st_size, 'sha256': sha(p)}
         for p in sorted(template.rglob('*')) if p.is_file()]
git = lambda *args: subprocess.check_output(['git', *args], cwd=repo, text=True).strip()
manifest = {
    'template_id': 'FluxRail-Web-template-2026-10-05-v1',
    'date_local': '2026-10-05', 'timezone': 'Asia/Shanghai',
    'frozen_at_utc': datetime.now(timezone.utc).isoformat(),
    'source_preview': 'design/copy-preview-2026-10-05',
    'production_git_head': git('rev-parse', 'HEAD'),
    'production_branch': git('branch', '--show-current'),
    'production_working_tree_clean': False,
    'production_baseline_files_unchanged': len(baseline),
    'exact_preview_files': same_files,
    'server_difference': 'Default port 4176 and template-specific environment variable/log only.',
    'scope': 'Static design reference with local interactions; no production implementation or device validation.',
    'runtime_dependency': 'Node.js >=20, built-in modules only.',
    'offline_checker_dependency': 'Playwright + installed Edge; supplied as a module path.',
    'evidence': ['evidence/template-check.json', 'evidence/docs-check.json', 'evidence/docs-final-check.json', 'reviews/LUNA_REVIEW_重构交接.md'],
    'manifest_self_excluded': True, 'files': files
}
manifest_path.write_text(json.dumps(manifest, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')
with zipfile.ZipFile(archive, 'x', compression=zipfile.ZIP_DEFLATED) as z:
    for p in sorted(template.rglob('*')):
        if p.is_file():
            z.write(p, arcname=Path(template.name) / p.relative_to(template))
with zipfile.ZipFile(archive) as z:
    if z.testzip() is not None:
        raise SystemExit('Archive CRC check failed')
    for p in sorted(template.rglob('*')):
        if p.is_file() and hashlib.sha256(z.read((Path(template.name) / p.relative_to(template)).as_posix())).hexdigest() != sha(p):
            raise SystemExit('Archive differs: ' + str(p))
archive.with_suffix('.zip.sha256').write_text(sha(archive) + '  ' + archive.name + '\n', encoding='ascii')
print(json.dumps({'manifest_files': len(files), 'production_files_unchanged': len(baseline), 'archive': str(archive), 'zip_bytes_verified': True}))
