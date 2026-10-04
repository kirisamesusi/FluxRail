"""Keep failed round1 frozen; independently validate actual held/removed round2."""
import json,zlib
from datetime import datetime,timezone
from pathlib import Path
from compare_json_values import same
r=Path(__file__).parent/'pc_board_2026-10-04';load=lambda p:json.loads(p.read_text(encoding='utf-8-sig'))
a=load(r/'webserial-after/baseline.json');h=load(r/'gp9-post-short-retry2/held-read/baseline.json');b=load(r/'gp9-post-short-retry2-after/baseline.json')
mon=load(r/'gp9-post-short-retry2/monitor.json');c=load(r/'gp9-retry2-user-confirmation.json')
x=(r/'webserial-after/original-schema2.bin').read_bytes();y=(r/'gp9-post-short-retry2-after/original-schema2.bin').read_bytes();held=(r/'gp9-post-short-retry2/held-read/original-schema2.bin').read_bytes()
profiles=same(load(r/'webserial-after/original-profiles.json'),load(r/'gp9-post-short-retry2-after/original-profiles.json'))
result={'time':datetime.now(timezone.utc).isoformat(),'kind':'post_incident_GP9_held_10s_boot_then_released','serial':b['hello']['result']['serial'],'user_confirmation':c,'monitor':mon,'physical_mask_held':h['get_status']['result']['physical_mask'],'physical_mask_released':b['get_status']['result']['physical_mask'],'held_forced_recovery':h['get_usb_mode']['result']['forced_recovery'],'forced_recovery':b['get_usb_mode']['result']['forced_recovery'],'mode':b['get_usb_mode']['result'],'schema_byte_exact':x==y==held,'profiles_equal':profiles,'revision':b['hello']['revision'],'saved_revision':b['hello']['saved_revision'],'dirty':b['hello']['dirty'],'crc32':zlib.crc32(y),'device_writes':0,'serial_closed':True,'electrical_health_verified':False,'hardware_flash_capacity_verified':False,'game':'not_run'}
result['passed']=all([mon.get('read_completed'),c['completed'],c['jumper_removed'],result['physical_mask_held']==512,result['physical_mask_released']==0,result['held_forced_recovery'],result['forced_recovery'],x==y==held,profiles,result['revision']==result['saved_revision']==a['hello']['revision'],not result['dirty'],result['mode']['current']==result['mode']['saved_startup']=='NORMAL_NKRO'])
o=r/'gp9-post-short-retry2.actual.json';assert not o.exists();o.write_text(json.dumps(result,ensure_ascii=False,indent=2),encoding='utf-8');print(json.dumps(result,ensure_ascii=False));assert result['passed']
