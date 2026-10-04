from pathlib import Path
from datetime import datetime,timezone
import argparse,json,zlib,hashlib
from compare_json_values import same
root=Path(__file__).resolve().parents[2]
batch=root/'tests/integration/pc_board_0_10_2_2026-10-05'
load=lambda p:json.loads(p.read_text(encoding='utf-8-sig'))
parser=argparse.ArgumentParser();parser.add_argument('--stage',choices=['recovery','final'],required=True);args=parser.parse_args()
def snapshot(folder,rev):
    p=batch/folder;b=load(p/'baseline.json');raw=(p/'original-schema2.bin').read_bytes()
    assert b['hello']['result']['serial']=='5054316560DD729C' and b['hello']['result']['firmware_version']=='0.10.2'
    assert b['hello']['revision']==b['hello']['saved_revision']==b['hello']['applied_revision']==rev and not b['hello']['dirty']
    rows=[json.loads(s) for s in (p/'baseline-cdc.jsonl').read_text(encoding='utf-8-sig').splitlines()]
    assert len(rows)==15 and all(r['response']['ok'] and r['request']['id']==r['response']['id'] and r['response']['revision']==rev for r in rows)
    chunks=[r['response']['result'] for r in rows if r['request']['op']=='export_config']
    assert bytes.fromhex(''.join(c['data'] for c in chunks))==raw and len(raw)==1387 and zlib.crc32(raw)==chunks[0]['crc32']
    return b,raw,load(p/'original-profiles.json')
a,x,pa=snapshot('after-user-flash',9)
if args.stage=='recovery':
    h,y,ph=snapshot('gp9-recovery-retry2/held-read',10);b,z,pb=snapshot('gp9-released',10)
    assert h['get_status']['result']['physical_mask']==512 and b['get_status']['result']['physical_mask']==0
    for s in (h,b):
        m=s['get_usb_mode']['result'];assert m['current']=='NORMAL_NKRO' and m['saved_startup']==m['requested_startup']=='IO4' and m['forced_recovery']
    assert y==z and same(pa,ph) and same(pa,pb)
    assert [i for i,(u,v) in enumerate(zip(x,z)) if u!=v]==[4,17] and x[17]==0 and z[17]==1
    result={'read_only_requests':30,'held_mask':512,'released_mask':0,'current':'NORMAL_NKRO','saved_startup':'IO4','forced_recovery':True,'revision':10,'original_profiles_equal':True,'persistent_schema_equal_except_revision_and_mode':True,'schema_crc32':zlib.crc32(z)}
else:
    b,z,pb=snapshot('normal-final',11)
    assert same(pa,pb) and [i for i,(u,v) in enumerate(zip(x,z)) if u!=v]==[4]
    assert b['get_status']['result']['physical_mask']==0 and b['get_status']['result']['maintenance']=='idle' and b['get_status']['result']['valid_slots']==3
    assert b['get_device']['result']['active_profile']==4
    m=b['get_usb_mode']['result'];assert m['current']==m['saved_startup']==m['requested_startup']=='NORMAL_NKRO' and not m['forced_recovery']
    r=load(batch/'normal-return/summary.json');assert r['writes']==3 and r['serial_closed'] and r['saved_revision']==11
    result={'read_only_requests':15,'mode_writes':3,'revision_applied_saved':11,'active_profile':4,'current_requested_saved':'NORMAL_NKRO','forced_recovery':False,'original_profiles_equal':True,'persistent_schema_equal_except_revision':True,'schema_crc32':zlib.crc32(z),'serial_closed':True}
result.update(reviewer='/root main process (ongoing user-authorized substitute)',passed=True,time_utc=datetime.now(timezone.utc).isoformat(),limits=['2MiB test board per user; not final4MiB Flash acceptance','No new compile/simulation/game; physical GP9 and actual configuration return only'])
out=batch/(args.stage+'.main.review.json');assert not out.exists();out.write_text(json.dumps(result,ensure_ascii=False,indent=2),encoding='utf-8')
print(json.dumps(result,ensure_ascii=False))
