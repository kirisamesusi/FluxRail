"""Read-only integrity/link check for the FluxRail character asset library."""
import hashlib,json,re,struct
from pathlib import Path
from urllib.parse import unquote

ROOT=Path(__file__).resolve().parents[1]
def main():
    manifest=json.loads((ROOT/'manifest.json').read_text(encoding='utf-8'))
    for record in manifest['files']:
        target=(ROOT/record['path']).resolve()
        assert target.is_relative_to(ROOT),record['path']
        data=target.read_bytes()
        assert len(data)==record['bytes'],record['path']
        assert hashlib.sha256(data).hexdigest()==record['sha256'],record['path']
    catalog=json.loads((ROOT/'catalog.json').read_text(encoding='utf-8'))
    ids=[asset['id'] for asset in catalog['assets']]
    assert len(ids)==len(set(ids))
    for asset in catalog['assets']:
        data=(ROOT/asset['path']).read_bytes()
        assert hashlib.sha256(data).hexdigest()==asset['sha256'],asset['id']
        if asset['path'].endswith('.png'):
            assert data[:8]==b'\x89PNG\r\n\x1a\n'
            assert list(struct.unpack('>II',data[16:24]))==[asset['width'],asset['height']]
        else:
            assert data[:6]==b'\x00\x00\x01\x00\x03\x00'
    links=0
    for target in [ROOT/'index.html',ROOT/'README.md',ROOT/'STYLE_GUIDE.md']:
        text=target.read_text(encoding='utf-8')
        refs=re.findall(r'(?:src|href)="([^"]+)"',text) if target.suffix=='.html' else re.findall(r'\[[^\]]*\]\(([^)]+)\)',text)
        for ref in refs:
            if re.match(r'^[A-Za-z][A-Za-z0-9+.-]*:',ref) or ref.startswith('#'):continue
            assert (target.parent/unquote(ref.split('#')[0])).resolve().exists(),ref
            links+=1
    print(json.dumps({'passed':True,'assets':len(ids),'manifest_files':len(manifest['files']),'local_links':links},ensure_ascii=False))
if __name__=='__main__':main()
