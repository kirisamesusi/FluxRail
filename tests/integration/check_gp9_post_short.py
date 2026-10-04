"""Offline validation of user-assisted, post-incident GP9 recovery."""
import json,zlib
from datetime import datetime,timezone
from pathlib import Path
from compare_json_values import same
r=Path(__file__).parent/'pc_board_2026-10-04';load=lambda p:json.loads(p.read_text(encoding='utf-8-sig'))
a=load(r/'webserial-after/baseline.json');b=load(r/'gp9-post-short-after/baseline.json')
x=(r/'webserial-after/original-schema2.bin').read_bytes();y=(r/'gp9-post-short-after/original-schema2.bin').read_bytes()
mon=load(r/'gp9-post-short-monitor/summary.json');confirmation=load(r/'gp9-post-short-user-confirmation.json')
profiles=same(load(r/'webserial-after/original-profiles.json'),load(r/'gp9-post-short-after/original-profiles.json'))
result={'time':datetime.now(timezone.utc).isoformat(),'kind':'post_short_physical_GP9_to_GND_boot_recovery','serial':b['hello']['result']['serial'],'user_confirmation':confirmation,'monitor_disconnect_and_reconnect':mon.get('disconnect_and_reconnect_observed',False),'forced_recovery':b['get_usb_mode']['result']['forced_recovery'],'mode':b['get_usb_mode']['result'],'physical_mask':b['get_status']['result']['physical_mask'],'schema_byte_exact':x==y,'profiles_equal':profiles,'revision':b['hello']['revision'],'saved_revision':b['hello']['saved_revision'],'dirty':b['hello']['dirty'],'crc32':zlib.crc32(y),'device_writes':0,'serial_closed':True,'electrical_health_verified':False,'hardware_flash_capacity_verified':False,'game':'not_run'}
result['passed']=all([result['monitor_disconnect_and_reconnect'],confirmation['completed'],confirmation['jumper_removed'],confirmation['no_obvious_heating_reported'],result['forced_recovery'],result['physical_mask']==0,x==y,profiles,result['revision']==result['saved_revision']==a['hello']['revision'],not result['dirty'],result['mode']['current']==result['mode']['saved_startup']=='NORMAL_NKRO'])
o=r/'gp9-post-short.actual.json';assert not o.exists();o.write_text(json.dumps(result,ensure_ascii=False,indent=2),encoding='utf-8');print(json.dumps(result,ensure_ascii=False));assert result['passed']
