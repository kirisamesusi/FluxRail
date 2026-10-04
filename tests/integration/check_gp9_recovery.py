"""Offline check real GP9 grounded-at-boot recovery evidence."""
import json
import zlib
from pathlib import Path
from compare_json_values import same
root=Path(__file__).parent/'pc_board_2026-10-04'
out=root/'gp9-normal.actual.json'
assert not out.exists(), 'Evidence exists'
load=lambda p:json.loads(p.read_text(encoding='utf-8-sig'))
a=load(root/'power-cut-after/baseline.json');b=load(root/'gp9-normal-after/baseline.json')
x=(root/'power-cut-after/original-schema2.bin').read_bytes();y=(root/'gp9-normal-after/original-schema2.bin').read_bytes()
equal=same(load(root/'power-cut-after/original-profiles.json'),load(root/'gp9-normal-after/original-profiles.json'))
result={'kind':'physical_GP9_to_GND_at_boot','user_confirmation':'USB removed; GP9 grounded during USB-only power-on; jumper removed after 3 seconds','forced_recovery':b['get_usb_mode']['result']['forced_recovery'],'mode':b['get_usb_mode']['result'],'physical_mask_after':b['get_status']['result']['physical_mask'],'schema_byte_exact':x==y,'profiles_equal':equal,'crc32':zlib.crc32(y),'revision':b['hello']['revision'],'saved_revision':b['hello']['saved_revision'],'dirty':b['hello']['dirty'],'active_profile':b['get_device']['result']['active_profile'],'device_writes':0,'serial_closed':True,'hardware_flash_capacity_verified':False,'game':'not_run'}
result['passed']=all([result['forced_recovery'],x==y,equal,result['physical_mask_after']==0,result['revision']==result['saved_revision']==5,not result['dirty'],result['active_profile']==4,result['mode']['current']==result['mode']['saved_startup']=='NORMAL_NKRO'])
out.write_text(json.dumps(result,ensure_ascii=False,indent=2),encoding='utf-8');print(json.dumps(result,ensure_ascii=False));assert result['passed']
