from pathlib import Path
from datetime import datetime,timezone
import hashlib,json
root=Path(__file__).resolve().parents[2]
batch=root/'tests/integration/pc_board_0_10_2_2026-10-05'
out=root/'tests/integration/validation_io4_0_10_2_actual_2026-10-05.json'
assert not out.exists()
load=lambda p:json.loads(p.read_text(encoding='utf-8-sig'))
for name in ('backup-baseline.main.review.json','io4-entry.main.review.json','protocol-same-tool.main.review.json','timeout-deadlines.main.review.json','recovery.main.review.json','final.main.review.json'):
    assert load(batch/name)['passed']
assert not load(batch/'recovery-attempt1.main.review.json')['passed']
files=sorted(p for p in batch.rglob('*') if p.is_file())
manifest={'time_utc':datetime.now(timezone.utc).isoformat(),'reviewer':'/root ongoing substitute authorized by human while Luna quota unavailable','board_firmware_reported':'0.10.2','uploaded_by':'User; UF2 upload itself/hash not directly observed','actual_protocol':'Same SHA tool as old0.10.1 failed test; complete passed','actual_timeout_windows':'100ms/400ms/25.5s early and pre-deadline system0x30, post-deadline system2; no premature expiration observed','GP9_recovery':'Attempt1 failed preserved; attempt2 held512/released0 forcedNORMAL passed','final':load(batch/'final.main.review.json'),'capacity_scope':'Human confirms current test board2MiB/final target4MiB; query waived; not final4MiB Flash hardware validation','not_tested':['game','external inputs/sensors/lighting','electrical health','exact IRQ accept/tick timestamp trace','long-run heavy IRQ load','normal keyboard/mouse payload Windows error1','full actual IO4 USB descriptors','20 mode cold round trips','Flash interrupted write/commit'],'evidence_count':len(files),'sha256':{p.relative_to(root).as_posix():hashlib.sha256(p.read_bytes()).hexdigest() for p in files},'compile_rollback_archives':load(batch/'backup-baseline.main.review.json')['archives']}
out.write_text(json.dumps(manifest,ensure_ascii=False,indent=2),encoding='utf-8')
print(f'Frozen {len(files)} evidence files; actual IO4 fix regression passed; original config restored saved11')
