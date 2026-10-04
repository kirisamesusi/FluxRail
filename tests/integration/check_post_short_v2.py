"""Functional readback after user-reported short; never proves electrical safety."""
import json
import zlib
from pathlib import Path
from compare_json_values import same
root=Path(__file__).parent/'pc_board_2026-10-04'
out=root/'post-short-readonly.actual.v2.json';assert not out.exists()
load=lambda p:json.loads(p.read_text(encoding='utf-8-sig'))
b=load(root/'post-short-readonly/baseline.json')
x=(root/'gp9-normal-after/original-schema2.bin').read_bytes();y=(root/'post-short-readonly/original-schema2.bin').read_bytes()
equal=same(load(root/'gp9-normal-after/original-profiles.json'),load(root/'post-short-readonly/original-profiles.json'))
rows=[json.loads(line) for line in (root/'post-short-readonly/baseline-cdc.jsonl').read_text(encoding='utf-8-sig').splitlines() if line.strip()]
allowed={'get_config','export_config','hello','get_status','get_device','list_profiles','get_profile','get_calibration','get_motion','get_usb_mode','begin_export','read_export_chunk','finish_export','get_io4_bindings'}
result={'kind':'post_short_readonly_functional_check','user_report':'Cooled and reconnected; no renewed obvious heat or USB error','requests':len(rows),'all_requests_successful':all(r['response']['ok'] for r in rows),'operations':[r['request']['op'] for r in rows],'firmware_serial':b['hello']['result']['serial'],'firmware_version':b['hello']['result']['firmware_version'],'schema_byte_exact_to_preincident':x==y,'profiles_equal':equal,'crc32':zlib.crc32(y),'length':len(y),'revision':b['hello']['revision'],'saved_revision':b['hello']['saved_revision'],'dirty':b['hello']['dirty'],'active_profile':b['get_device']['result']['active_profile'],'mode':b['get_usb_mode']['result'],'health':b['get_status']['result'],'serial_closed':True,'flash_save_reboot_or_mode_writes':0,'electrical_damage_ruled_out':False,'temperature_measured':False,'voltage_current_measured':False,'physical_flash_capacity_verified':False,'firmware_compile':'not_run','simulation':'not_run','game':'not_run'}
result['functional_check_passed']=all([x==y,equal,len(rows)==15,result['all_requests_successful'],all(r['request']['op'] in allowed for r in rows),result['revision']==result['saved_revision']==5,not result['dirty'],result['active_profile']==4,result['health']['physical_mask']==0,result['health']['config_ready'],not result['health']['apply_failed'],result['health']['maintenance']=='idle',result['mode']['current']=='NORMAL_NKRO'])
out.write_text(json.dumps(result,ensure_ascii=False,indent=2),encoding='utf-8');print(json.dumps(result,ensure_ascii=False));assert result['functional_check_passed']
