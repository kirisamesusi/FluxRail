"""Package the current browser runtime and interactive Ubuntu setup (no credentials)."""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess
import tarfile
import tempfile

ROOT = Path(__file__).resolve().parents[2]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('output', help='New .tar.gz output path; existing files refused')
    args = parser.parse_args()
    output = Path(args.output).resolve()
    if output.exists():
        raise FileExistsError('Refusing to overwrite an existing deployment bundle')
    if Path(str(output) + '.sha256').exists() or Path(str(output) + '.manifest.json').exists():
        raise FileExistsError('Refusing to overwrite existing bundle metadata')
    output.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix='fluxrail-setup-package-') as temporary:
        folder = Path(temporary)
        site = folder / 'site'
        subprocess.run(['node', str(ROOT / 'web/deploy/prepare-site.mjs'), str(site)], check=True)
        files = {
            'setup-fluxrail.sh': ROOT / 'web/deploy/setup-fluxrail.sh',
            'README.zh-CN.md': ROOT / 'web/deploy/README.setup.md',
            'workflow/deploy-web.yml': ROOT / '.github/workflows/deploy-web.yml',
        }
        files.update({'site/' + p.relative_to(site).as_posix(): p for p in site.rglob('*') if p.is_file()})
        manifest = {name: hashlib.sha256(path.read_bytes()).hexdigest() for name, path in sorted(files.items())}
        checksums = folder / 'site.sha256'
        checksums.write_text(''.join(f'{digest}  {name}\n' for name, digest in manifest.items() if name.startswith('site/')), encoding='ascii', newline='\n')
        files['site.sha256'] = checksums
        # Exclusive creation keeps older deliveries unchanged; no keys/auth state included.
        with tarfile.open(output, 'x:gz') as archive:
            for name, path in sorted(files.items()):
                info = archive.gettarinfo(str(path), arcname='fluxrail-setup/' + name)
                info.uid = info.gid = 0
                info.uname = info.gname = ''
                info.mode = 0o755 if name == 'setup-fluxrail.sh' else 0o644
                with path.open('rb') as stream:
                    archive.addfile(info, stream)
    digest = hashlib.sha256(output.read_bytes()).hexdigest()
    with Path(str(output) + '.sha256').open('x', encoding='ascii') as checksum:
        checksum.write(f'{digest}  {output.name}\n')
    with Path(str(output) + '.manifest.json').open('x', encoding='utf-8') as manifest_file:
        json.dump({'bundle': output.name, 'sha256': digest, 'input_files_sha256': manifest}, manifest_file, ensure_ascii=False, indent=2)
    print(json.dumps({'output': str(output), 'sha256': digest, 'files': len(files)}, ensure_ascii=True))


if __name__ == '__main__':
    main()
