from pathlib import Path
from datetime import datetime,timezone
import ast,hashlib,json,zlib
from compare_json_values import same
root=Path(__file__).resolve().parents[2]
load=lambda p:json.loads(p.read_text(encoding='utf-8-sig'))
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
batch=root/'tests/integration/pc_board_0_10_2_2026-10-05'
old=root/'tests/integration/pc_board_2026-10-04/rom-return-final'
new=batch/'after-user-flash'
a=(old/'original-schema2.bin').read_bytes();b=(new/'original-schema2.bin').read_bytes()
assert a==b and len(b)==1387 and zlib.crc32(b)==700994562
assert same(load(old/'original-profiles.json'),load(new/'original-profiles.json'))
s=load(new/'baseline.json');assert s['hello']['result']['serial']=='5054316560DD729C' and s['hello']['result']['firmware_version']=='0.10.2'
assert s['hello']['revision']==s['hello']['saved_revision']==s['hello']['applied_revision']==9 and not s['hello']['dirty']
assert s['get_status']['result']['maintenance']=='idle' and s['get_status']['result']['physical_mask']==0 and s['get_status']['result']['valid_slots']==3
assert s['get_device']['result']['active_profile']==4
assert s['get_usb_mode']['result']['current']==s['get_usb_mode']['result']['saved_startup']==s['get_usb_mode']['result']['requested_startup']=='NORMAL_NKRO'
rows=[json.loads(r) for r in (new/'baseline-cdc.jsonl').read_text(encoding='utf-8-sig').splitlines()]
assert len(rows)==15 and all(r['response']['ok'] and r['request']['id']==r['response']['id'] and r['response']['revision']==9 for r in rows)
archives=[]
for path in sorted((root/'backups/firmware-rollback').glob('*/*/manifest.json')):
    m=load(path);base=path.parent
    for name,digest in m['files_sha256'].items():assert sha(base/name)==digest,name
    for name,digest in m['verified_previous_source_sha256'].items():assert sha(base/'source'/name)==digest,name
    assert (base/'artifacts/FluxRail.ino.uf2').exists()
    archives.append({'path':str(base),'version':m['previous_version'],'file_count':len(m['files_sha256']),'source_file_count':len(m['verified_previous_source_sha256']),'changed_code_files':m['changed_code_files'],'UF2_SHA256':sha(base/'artifacts/FluxRail.ino.uf2')})
assert len(archives)==2 and {a['version'] for a in archives}=={'0.10.1','0.10.2'}
assert next(a for a in archives if a['version']=='0.10.1')['changed_code_files']==['firmware/FluxRail/src/common/FirmwareInfo.h','firmware/FluxRail/src/usb/Io4Codec.cpp']
compiler=(root/'tests/integration/compile_firmware.py').read_text(encoding='utf-8')
assert compiler.index('rollback = create_rollback()')<compiler.index('subprocess.run([sys.executable')
ast.parse(compiler);ast.parse((root/'tests/integration/firmware_rollback.py').read_text(encoding='utf-8'))
prior=load(root/'tests/integration/pc_board_2026-10-04/io4-timeout-candidate-0.10.2/build/firmware_results.json')
assert prior['source_sha256']['firmware/FluxRail/src/app/App.cpp']==sha(root/'backups/firmware-before-io4-timeout-2026-10-05/App.cpp')
assert load(root/'tests/integration/pc_board_2026-10-04/io4-gp9-recovery.main.review.json')['passed']
r={'reviewer':'/root main process, ongoing round authorized substitute while Luna quota unavailable','time_utc':datetime.now(timezone.utc).isoformat(),'passed':True,'new_version':'0.10.2','revision':9,'serial':'5054316560DD729C','original_schema_byte_exact':True,'crc32':700994562,'profiles_equal':True,'independent_read_only_requests':15,'serial_closed':True,'configuration_writes':0,'archives':archives,'compile_gate':'backup before dependencies/compiler; missing or hash mismatch raises before compile','GPIO_recovery_entry':'Previously physical proven; App.cpp unchanged SHA; no claim new firmware physical recovery retest yet','board_capacity':'User states test board2MiB/final target4MiB; capacity query waived; not independently measured','limits':['No compiler run in this phase','Board hello verifies reported version, not flashed UF2 SHA','IO4 fixed actual behavior still pending']}
out=batch/'backup-baseline.main.review.json';assert not out.exists();out.write_text(json.dumps(r,ensure_ascii=False,indent=2),encoding='utf-8')
print('PASS: full old/new rollback archives and 0.10.2 board configuration; compile backup gate inspected')
