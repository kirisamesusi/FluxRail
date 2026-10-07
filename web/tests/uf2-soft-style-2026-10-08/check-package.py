import hashlib
import json
from pathlib import Path
import tarfile

root = Path(__file__).resolve().parents[3]
bundle = root / 'outputs/FluxRail-Ubuntu-Setup-2026-10-08-r6.tar.gz'
manifest = json.loads(Path(str(bundle) + '.manifest.json').read_text(encoding='utf-8'))
sha = hashlib.sha256(bundle.read_bytes()).hexdigest()
assert sha == manifest['sha256'] == Path(str(bundle) + '.sha256').read_text().split()[0]
special = {
    'setup-fluxrail.sh': 'web/deploy/setup-fluxrail.sh',
    'README.zh-CN.md': 'web/deploy/README.setup.md',
    'workflow/deploy-web.yml': '.github/workflows/deploy-web.yml',
    'site/LICENSE.txt': 'LICENSE',
}
with tarfile.open(bundle) as archive:
    members = archive.getmembers()
    assert all(m.isfile() for m in members)
    assert {m.name for m in members} == {
        'fluxrail-setup/' + name for name in manifest['input_files_sha256']
    } | {'fluxrail-setup/site.sha256'}
    for name, expected in manifest['input_files_sha256'].items():
        data = archive.extractfile('fluxrail-setup/' + name).read()
        assert hashlib.sha256(data).hexdigest() == expected, name
        source = special.get(name, 'web/' + name.removeprefix('site/'))
        assert hashlib.sha256((root / source).read_bytes()).hexdigest() == expected, source
    checksums = archive.extractfile('fluxrail-setup/site.sha256').read().decode('ascii')
    assert checksums == ''.join(
        f'{digest}  {name}\n' for name, digest in manifest['input_files_sha256'].items()
        if name.startswith('site/')
    )
report = dict(date_local='2026-10-08', bundle=bundle.name, files=len(members),
              sha256=sha, matches_current_sources=True, passed=True)
Path(__file__).with_name('PACKAGE_CHECK.json').write_text(
    json.dumps(report, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')
print(json.dumps(report))
