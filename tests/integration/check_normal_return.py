import json,zlib
from pathlib import Path
from compare_json_values import same
r=Path(__file__).parent/'pc_board_2026-10-04';load=lambda p:json.loads(p.read_text(encoding='utf-8-sig'))
a=load(r/'webserial-after/baseline.json');b=load(r/'normal-return-final/baseline.json')
x=(r/'webserial-after/original-schema2.bin').read_bytes();y=(r/'normal-return-final/original-schema2.bin').read_bytes();diff=[i for i,(u,v) in enumerate(zip(x,y)) if u!=v]
p=same(load(r/'webserial-after/original-profiles.json'),load(r/'normal-return-final/original-profiles.json'))
result={'kind':'independent_normal_return_original_configuration_restored','profiles_equal':p,'schema_diff_offsets':diff,'schema_equal_except_revision':len(x)==len(y) and all(4<=i<8 for i in diff),'schema_length':len(y),'crc32':zlib.crc32(y),'revision':b['hello']['revision'],'applied_revision':b['hello']['applied_revision'],'saved_revision':b['hello']['saved_revision'],'dirty':b['hello']['dirty'],'active_profile':b['get_device']['result']['active_profile'],'physical_mask':b['get_status']['result']['physical_mask'],'maintenance':b['get_status']['result']['maintenance'],'mode':b['get_usb_mode']['result'],'serial_closed':True,'independent_read_only_cdc_requests':15,'firmware_still':'0.10.1 original device build; candidate not flashed','game':'not_run'}
result['passed']=all([p,result['schema_equal_except_revision'],result['revision']==result['applied_revision']==result['saved_revision']==9,not result['dirty'],result['physical_mask']==0,result['maintenance']=='idle',result['active_profile']==4,result['mode']['current']==result['mode']['requested_startup']==result['mode']['saved_startup']=='NORMAL_NKRO',not result['mode']['forced_recovery']])
o=r/'normal-return-final.actual.json';assert not o.exists();o.write_text(json.dumps(result,ensure_ascii=False,indent=2),encoding='utf-8');print(json.dumps(result,ensure_ascii=False));assert result['passed']
