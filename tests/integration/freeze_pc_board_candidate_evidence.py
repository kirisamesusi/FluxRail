"""Freeze actual protocol/ROM/ARM/candidate evidence without modifying originals."""
from pathlib import Path
from datetime import datetime,timezone
import json,hashlib
root=Path(__file__).resolve().parents[2]
batch=root/'tests/integration/pc_board_2026-10-04'
out=root/'tests/integration/validation_pc_board_candidate_2026-10-05.json'
assert not out.exists()
evidence=[p for p in batch.rglob('*') if p.is_file()]
tools=[root/'tests/integration'/n for n in ('review_recovery_main.py','test_enter_rom_readonly.ps1','compile_io4_timeout_candidate.py','prepare_candidate_descriptor_check.py','review_rom_and_candidate_main.py','freeze_pc_board_candidate_evidence.py')]
manifest={'time_utc':datetime.now(timezone.utc).isoformat(),'reviewer':'/root for rounds after Luna quota failure; user explicitly authorized 本轮改由主进程审核','batch_directory':'pc_board_2026-10-04 retained across local midnight','evidence_count':len(evidence),'evidence_sha256':{str(p.relative_to(root)):hashlib.sha256(p.read_bytes()).hexdigest() for p in sorted(evidence)},'new_tools_sha256':{str(p.relative_to(root)):hashlib.sha256(p.read_bytes()).hexdigest() for p in tools},'board_final':{'firmware':'0.10.1','serial':'5054316560DD729C','mode':'NORMAL_NKRO','active_profile':4,'revision_applied_saved':9,'dirty':False,'schema_bytes':1387,'schema_crc32':700994562,'original_configuration_restored':True,'serial_closed':True},'candidate':{'version':'0.10.2','target_contract':'4MiB; physical capacity unverified','formal_compile_and_link_audit':'38 project TU passed','formal_ELF_descriptor_ARM':'both personas passed','uploaded':False,'hardware_tested':False,'game_tested':False},'open_failure':'Original 0.10.1 IO4 nonzero timeout expires prematurely; guard ARM passes, fixed hardware behavior unverified','other_dependencies':['ROM MI01 no driver Code28; picotool info exit -7; physical Flash capacity not obtained','normal keyboard/mouse report payload Windows error1 unresolved','actual full IO4 descriptors, 20 mode cold round trips, power interruption during Flash write/commit, external sensors/buttons/lighting, game and electrical health unverified']}
out.write_text(json.dumps(manifest,ensure_ascii=False,indent=2),encoding='utf-8')
print(f'Frozen {len(evidence)} evidence files; board original configuration restored; candidate not uploaded')
