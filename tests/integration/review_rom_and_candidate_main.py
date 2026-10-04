"""Parent review authorized by user for this round; original transcripts/hashes."""
from pathlib import Path
from datetime import datetime, timezone
import hashlib,json,struct,zlib
from compare_json_values import same
root=Path(__file__).resolve().parents[2]
batch=root/'tests/integration/pc_board_2026-10-04'
load=lambda p:json.loads(p.read_text(encoding='utf-8-sig'))
def emit(path, checks, limitations, evidence):
    assert not path.exists()
    path.write_text(json.dumps({'reviewer':'/root main process','authorization':'Trusted user: 本轮改由主进程审核; Luna quota unavailable','time_utc':datetime.now(timezone.utc).isoformat(),'passed':True,'checks':checks,'limitations':limitations,'evidence_sha256':{str(p.relative_to(root)):hashlib.sha256(p.read_bytes()).hexdigest() for p in evidence}},ensure_ascii=False,indent=2),encoding='utf-8')
rows=[json.loads(s) for s in (batch/'rom-entry/cdc.jsonl').read_text(encoding='utf-8-sig').splitlines()]
assert [r['request']['op'] for r in rows]==['hello','get_status','get_usb_mode','get_device','enter_bootloader']
assert all(r['response']['ok'] and r['request']['id']==r['response']['id'] for r in rows)
assert rows[-1]['request']['payload']=={'expected_revision':9} and rows[-1]['response']['result']['reboot_ready']
assert load(batch/'rom-entry/summary.json')['serial_closed']
rom=load(batch/'rom-entry/pnp-after.json')
assert len(rom)==1 and rom[0]['Status']=='OK' and rom[0]['InstanceId']==r'USB\VID_2E8A&PID_0003\E0C9125B0D9B'
assert load(batch/'rom-info/summary.json')['exit_code']==-7 and load(batch/'rom-info/vidpid-summary.json')['exit_code']==-7
nodes=load(batch/'rom-info/driver-details.v2.json')
ctrl=next(n for n in nodes if '&MI_01' in n['InstanceId'])
props={p['KeyName']:p['Data'] for p in ctrl['properties']}
assert ctrl['Status']=='Error' and props['DEVPKEY_Device_ProblemCode']==28 and props['DEVPKEY_Device_Service'] is None and props['DEVPKEY_Device_DriverInfPath'] is None
a=load(batch/'normal-return-final/baseline.json');b=load(batch/'rom-return-final/baseline.json')
x=(batch/'normal-return-final/original-schema2.bin').read_bytes();y=(batch/'rom-return-final/original-schema2.bin').read_bytes()
assert x==y and zlib.crc32(y)==700994562 and len(y)==1387
assert same(load(batch/'normal-return-final/original-profiles.json'),load(batch/'rom-return-final/original-profiles.json'))
rrows=[json.loads(s) for s in (batch/'rom-return-final/baseline-cdc.jsonl').read_text(encoding='utf-8-sig').splitlines()]
assert len(rrows)==15 and all(r['response']['ok'] and r['response']['revision']==9 for r in rrows)
assert b['hello']['result']['serial']=='5054316560DD729C' and b['hello']['result']['firmware_version']=='0.10.1'
assert b['hello']['revision']==b['hello']['applied_revision']==b['hello']['saved_revision']==9 and not b['hello']['dirty']
assert b['get_status']['result']['physical_mask']==0 and b['get_status']['result']['maintenance']=='idle'
m=b['get_usb_mode']['result'];assert m['current']==m['requested_startup']==m['saved_startup']=='NORMAL_NKRO' and not m['forced_recovery']
emit(batch/'rom-recovery.main.review.json',{'ROM_enter_ack_and_unique_enumeration':True,'enter_requests':1,'Flash_config_writes':0,'ROM_info':'failed; two attempts exit -7; MI01 missing driver Code28','USB_return_original_schema_byte_exact':True,'schema_crc32':700994562,'active_profile':b['get_device']['result']['active_profile'],'revision_applied_saved':9,'independent_readonly_requests':15},['Flash capacity not obtained; compile identity does not measure physical size','No picotool reboot executed because info access unavailable; actual return via user power cycle','No electrical or game verification'],[p for folder in ('rom-entry','rom-info','rom-return-final') for p in (batch/folder).rglob('*') if p.is_file()])
out=batch/'io4-timeout-candidate-0.10.2'
f=load(out/'build/firmware_results.json');s=load(out/'candidate-summary.json');audit=load(out/'build/source_link_audit.json');desc=load(out/'build/usb_descriptor_results.json')
assert f['exit_code']==0 and audit['passed'] and f['strict_project_units']==38
assert len(audit['project_translation_units'])==38 and all(t['exit_code']==0 and not t['project_warnings'] for t in audit['project_translation_units'])
assert not f['source_drift_during_compile'] and not f['copied_source_mismatch']
old=load(root/'tests/integration/build/firmware_results.json')
changed=sorted(k for k in f['source_sha256'] if f['source_sha256'][k]!=old['source_sha256'].get(k))
assert changed==['firmware/FluxRail/src/common/FirmwareInfo.h','firmware/FluxRail/src/usb/Io4Codec.cpp'],changed
for k,v in f['source_sha256'].items():assert hashlib.sha256((root/k).read_bytes()).hexdigest()==v
assert f['source_sha256']['firmware/FluxRail/src/usb/Io4Codec.cpp']==load(batch/'io4-timeout-full-arm.actual.review.json')['verified']['Io4Codec_candidate_source_sha256']
assert f['firmware_metadata']['firmware']['version']=='0.10.2' and f['firmware_metadata']['firmware']['board']=='fluxrail-rp2040-4mib'
assert s['frozen_evidence_unchanged'] and s['frozen_before_sha256']==s['frozen_after_sha256']
for k,v in s['frozen_before_sha256'].items():assert hashlib.sha256(Path(k).read_bytes()).hexdigest()==v
assert s['uf2_sha256']==hashlib.sha256((out/'build/arduino/FluxRail.ino.uf2').read_bytes()).hexdigest() and not s['uploaded']
assert desc['elf_sha256']==hashlib.sha256((out/'build/arduino/FluxRail.ino.elf').read_bytes()).hexdigest()
assert len(desc['results'])==2 and all(d['passed'] for d in desc['results']) and not desc['hardware_tested']
assert load(out/'descriptor-launch.json')['historical_unchanged']
emit(out/'candidate.main.review.json',{'formal_compile_and_strict_link_audit':'passed 38 project translation units','source_changes':changed,'timing_source_matches_Luna_reviewed_ARM_regression':True,'formal_ELF_descriptor_ARM':'NORMAL/IO4 both passed; modeled hardware boundaries','version':'0.10.2','uf2_bytes':s['uf2_bytes'],'uf2_sha256':s['uf2_sha256'],'frozen_original_artifacts_unchanged':True},['4MiB contract candidate only; physical Flash capacity still unverified','Candidate not uploaded or tested on board/game','Old 0.10.1 premature timeout failure remains open until approved update and board retest'],[p for p in (out/'build/firmware_results.json',out/'build/source_link_audit.json',out/'build/usb_descriptor_results.json',out/'candidate-summary.json',out/'descriptor-launch.json',out/'build/arduino/FluxRail.ino.uf2',out/'build/arduino/FluxRail.ino.elf')])
print('PASS: main review ROM entry/physical return and isolated candidate build; capacity/fixed board behavior unverified')
