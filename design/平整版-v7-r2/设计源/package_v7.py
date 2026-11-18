"""Check release cross-references, then pack the trial with its hash ledger."""
from collections import Counter
import base64
import csv
import gzip
import hashlib
import json
from pathlib import Path
from xml.etree import ElementTree
import zipfile

R=Path(__file__).resolve().parents[1]


def read(name):
    return json.loads((R/name).read_text(encoding='utf-8-sig'))


def sha(path):
    with path.open('rb') as f:
        return hashlib.file_digest(f,'sha256').hexdigest()


def write(name,data):
    (R/name).write_text(json.dumps(data,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')


def check_xlsx(catalog):
    ns={'m':'http://schemas.openxmlformats.org/spreadsheetml/2006/main'}
    with zipfile.ZipFile(R/'BOM.xlsx') as z:
        shared=[]
        if 'xl/sharedStrings.xml' in z.namelist():
            root=ElementTree.fromstring(z.read('xl/sharedStrings.xml'))
            shared=[''.join(t.text or '' for t in si.findall('.//m:t',ns)) for si in root.findall('m:si',ns)]
        root=ElementTree.fromstring(z.read('xl/worksheets/sheet1.xml'))
    got={}
    for row in root.findall('.//m:row',ns):
        if int(row.attrib['r'])<6: continue
        values=[]
        for cell in row.findall('m:c',ns):
            t=cell.attrib.get('t')
            v=cell.findtext('m:v','',ns)
            if t=='inlineStr': v=''.join(x.text or '' for x in cell.findall('.//m:t',ns))
            if t=='s': v=shared[int(v)]
            values.append(v)
        got[values[0]]=values[1:]
    assert set(got)==set(catalog),(set(got)^set(catalog))
    for key,r in catalog.items():
        assert got[key]==[str(r[k]) for k in ('name','obtain','qtyKnob','qtyINF','qtyOngeki','spec','note','v6Source')],key


def audit():
    catalog={r['id']:r for r in read('部件清单.json')}
    assert len(catalog)==76
    parts=read('整机实例映射.json')
    assert len({p['id'] for p in parts})==len(parts)==269
    for variant,col in [('INF','qtyINF'),('音击','qtyOngeki')]:
        chosen=[p for p in parts if p['variant'] in ('通用',variant)]
        count=Counter(p['partKey'] for p in chosen)
        for key,n in count.items():
            if key.startswith('bearing-'): n//=3
            assert n==catalog[key][col],(variant,key,n,catalog[key][col])
        assert len(chosen)==268
    assert not any('_W_brace_' in p['id'] for p in parts)
    assert catalog['m2-washer']['qtyKnob']==4
    purchases={'bearing-686':2,'pom':4,'wave':2,'hub-grub':2,'brake-adjust':2}
    assert {k:v['qtyINF'] for k,v in catalog.items() if v['obtain']=='新购'}==purchases
    text=(R/'零件数据.js').read_text(encoding='utf-8')
    assets=json.loads(text.removeprefix('globalThis.FluxRailPartFiles=').rstrip().removesuffix(';'))
    for r in catalog.values():
        for key in ('step','stl'):
            path=R/r[key]
            assert sha(path)==r[key+'SHA256'],path
            assert gzip.decompress(base64.b64decode(assets[r[key]]))==path.read_bytes(),path
    assert len(assets)==152
    assert sum(r['printable'] for r in catalog.values())==19
    with (R/'打印清单.csv').open(encoding='utf-8-sig',newline='') as f:
        prints=list(csv.DictReader(f))
    assert {r['编号'] for r in prints}=={k for k,v in catalog.items() if v['printable']}
    for r in prints:
        assert (R/r['STL']).is_file()
    check_xlsx(catalog)
    assert read('验证/离线网页.json')['allLocalLinks']
    assert len(read('验证/STEP装配.json'))==6
    assert len(read('验证/转动与行程.json'))==24
    assert len(read('验证/整机.json'))==3
    rebuilt=read('验证/隔离重建.json')['result']
    assert rebuilt['knobSolids']==63 and rebuilt['machineViewer']==269
    contact=read('验证/阻尼受力与净空.json')
    assert len(contact)==4 and all(r['magnetGapMM']==.6 for r in contact)
    assert len(read('验证/工具与拆装.json'))==37
    for name in ('转动与行程','整机'):
        assert all(not r['invalid'] and not r['unintendedIntersections'] for r in read(f'验证/{name}.json'))
    assert all(not r['collisions'] for r in read('验证/原接口与整套上取.json')['lifts'])
    return dict(catalog=76,independentFiles=152,printable=19,instanceMap=269,worksheetRows=76,
                adjustmentScrewPerKnob=1,newPurchaseQtyForTwo=purchases,
                cadDigestChecks=152,browserBinaryPayloadMatches=152,stepAssemblies=6,
                processDxf=['r2封盖-01-通切.dxf','r2封盖-02-二次加工.dxf','r2黄铜摩擦盘.dxf','r2-POM缺口.dxf'])


def file_list():
    return sorted(p for p in R.rglob('*') if p.is_file() and p.name!='文件校验.json'
                  and p.suffix!='.pyc' and '__pycache__' not in p.parts)


def main():
    report=audit()
    report['archiveVerifiedByZipTestAndHashes']=True
    write('验证/包检查.json',report)
    files=file_list()
    entries=[dict(path=p.relative_to(R).as_posix(),bytes=p.stat().st_size,sha256=sha(p)) for p in files]
    write('文件校验.json',entries)
    output=R.parent/'FluxRail-平整旋钮-v7-可调阻尼-r2-20261106.zip'
    with zipfile.ZipFile(output,'w',zipfile.ZIP_DEFLATED,compresslevel=6) as z:
        for p in files+[R/'文件校验.json']:
            z.write(p,f'{R.name}/{p.relative_to(R).as_posix()}')
    with zipfile.ZipFile(output) as z:
        assert z.testzip() is None and len(z.namelist())==len(files)+1
        for e in entries:
            assert hashlib.sha256(z.read(f'{R.name}/{e["path"]}')).hexdigest()==e['sha256'],e
    digest=sha(output)
    output.with_suffix('.zip.sha256').write_text(f'{digest}  {output.name}\n',encoding='utf-8')
    print(json.dumps(dict(zip=output.name,bytes=output.stat().st_size,sha256=digest,files=len(files)+1),
                     ensure_ascii=False),flush=True)


if __name__=='__main__':
    main()
