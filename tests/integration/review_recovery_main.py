"""Independent transcript/schema review; no device access, never overwrite evidence."""
import json, hashlib, struct, zlib
from datetime import datetime, timezone
from pathlib import Path
from compare_json_values import same

root = Path(__file__).resolve().parent / 'pc_board_2026-10-04'
load = lambda p: json.loads(p.read_text(encoding='utf-8-sig'))
readonly = {'hello','get_status','get_device','list_profiles','get_config','get_motion','get_calibration','get_usb_mode','get_profile','export_config'}
def snapshot(folder, revision):
    p = root / folder
    rows = [json.loads(s) for s in (p/'baseline-cdc.jsonl').read_text(encoding='utf-8-sig').splitlines()]
    assert len(rows)==15
    for row in rows:
        q,r=row['request'],row['response']
        assert q['op'] in readonly and r['ok'] and q['id']==r['id'] and r['protocol_version']==1 and r['revision']==revision
    b=load(p/'baseline.json')
    assert b['hello']['result']['serial']=='5054316560DD729C'
    assert b['hello']['result']['firmware_version']=='0.10.1'
    assert b['hello']['saved_revision']==b['hello']['applied_revision']==revision and not b['hello']['dirty']
    raw=(p/'original-schema2.bin').read_bytes()
    chunks=[r['response']['result'] for r in rows if r['request']['op']=='export_config']
    assert bytes.fromhex(''.join(c['data'] for c in chunks))==raw
    assert len(raw)==1387 and zlib.crc32(raw)==chunks[0]['crc32'] and struct.unpack_from('<I',raw,4)[0]==revision
    return b, raw, load(p/'original-profiles.json')
def save(name, checks, limitations):
    p=root/name
    assert not p.exists()
    evidence={str(x.relative_to(root)):hashlib.sha256(x.read_bytes()).hexdigest() for x in root.rglob('*') if x.is_file() and any(s in str(x.relative_to(root)) for s in ('io4-gp9-recovery/','io4-gp9-released/','normal-return-v3/','normal-return-final/','io4-gp9-recovery.actual','normal-return-final.actual'))}
    p.write_text(json.dumps({'reviewer':'/root main process','authorization':'Trusted user: 本轮改由主进程审核; Luna quota unavailable','time_utc':datetime.now(timezone.utc).isoformat(),'method':'independently recomputed from original CDC transcripts, raw schema and original profiles; no new device requests','passed':True,'checks':checks,'limitations':limitations,'evidence_sha256':evidence},ensure_ascii=False,indent=2),encoding='utf-8')
a,x,pa=snapshot('webserial-after',7)
h,y,ph=snapshot('io4-gp9-recovery/held-read',8)
b,z,pb=snapshot('io4-gp9-released',8)
assert y==z and same(pa,ph) and same(pa,pb)
assert [i for i,(u,v) in enumerate(zip(x,z)) if u!=v]==[4,17] and x[17]==0 and z[17]==1
assert h['get_status']['result']['physical_mask']==512 and b['get_status']['result']['physical_mask']==0
for s in (h,b):
    m=s['get_usb_mode']['result']
    assert m['forced_recovery'] and m['current']=='NORMAL_NKRO' and m['saved_startup']==m['requested_startup']=='IO4'
assert load(root/'io4-gp9-recovery.actual.json')['passed'] is False
assert load(root/'io4-gp9-recovery.actual.v2.json')['passed'] is True
save('io4-gp9-recovery.main.review.json',{'readonly_responses':30,'held_mask':512,'released_mask':0,'raw_schema_equal_held_released':True,'original_diff_offsets':[4,17],'persistent_calibration_and_profiles_unchanged':True,'v1_false_negative_preserved':'wrong mode offset and comparison of timestamp_us/sample_revision/epoch; V2 correctly excludes volatile telemetry'},['one physical return only; no 20-cycle acceptance','no electrical health or repaired IO4 timeout board verification'])
c,w,pc=snapshot('normal-return-final',9)
assert same(pa,pc) and [i for i,(u,v) in enumerate(zip(x,w)) if u!=v]==[4]
assert c['get_status']['result']['physical_mask']==0 and c['get_status']['result']['maintenance']=='idle' and c['get_status']['result']['valid_slots']==3
assert c['get_device']['result']['active_profile']==4
m=c['get_usb_mode']['result']
assert m['current']==m['requested_startup']==m['saved_startup']=='NORMAL_NKRO' and not m['forced_recovery']
rows=[json.loads(s) for s in (root/'normal-return-v3/cdc.jsonl').read_text(encoding='utf-8-sig').splitlines()]
assert len(rows)==11 and all(r['response']['ok'] and r['request']['id']==r['response']['id'] for r in rows)
assert [r['request']['op'] for r in rows if r['request']['op'] not in readonly]==['set_startup_usb_mode','save_config','reboot_to_saved_mode']
assert rows[-1]['response']['result']['reboot_ready']
assert load(root/'normal-return-v3/summary.json')['serial_closed']
save('normal-return-final.main.review.json',{'mutating_requests':3,'mode_round_responses':11,'independent_readonly_responses':15,'revision_applied_saved':9,'active_profile':4,'schema_crc32':zlib.crc32(w),'original_diff_offsets':[4],'original_profiles_restored':True,'serial_closed':True},['application reboot, not another true power cut','firmware remains original 0.10.1; candidate not flashed; game not tested'])
print('PASS: both actual recovery rounds independently reviewed by main process')
