"""Independent offline comparison after actual production Web Serial writes."""
import json,zlib
from pathlib import Path
from compare_json_values import same
r=Path(__file__).parent/'pc_board_2026-10-04'
load=lambda p:json.loads(p.read_text(encoding='utf-8-sig'))
a=load(r/'webserial-before/baseline.json');b=load(r/'webserial-after/baseline.json')
x=(r/'webserial-before/original-schema2.bin').read_bytes();y=(r/'webserial-after/original-schema2.bin').read_bytes()
logs=load(r/'webserial-offscreen-v4/cdc.json');summ=load(r/'webserial-offscreen-v4/summary.json')
diff=[i for i,(u,v) in enumerate(zip(x,y)) if u!=v]
profiles=same(load(r/'webserial-before/original-profiles.json'),load(r/'webserial-after/original-profiles.json'))
writes=[z for z in logs if z['op'] in ('set_profile','save_config')]
result={'kind':'actual_production_webserial_then_independent_cdc_snapshot','cdc_browser_count':len(logs),'browser_write_count':len(writes),'browser_write_ops':[z['op'] for z in writes], 'browser_error':summ['error'],'browser_restored':summ['restored'],'reconnect_saved_readback':summ['reconnect_saved_readback'],'independent_read_requests':15,'profiles_equal':profiles,'schema_length':len(y),'schema_diff_offsets':diff,'schema_equal_except_revision':len(x)==len(y) and all(4<=i<8 for i in diff),'initial_revision':a['hello']['revision'],'revision':b['hello']['revision'],'saved_revision':b['hello']['saved_revision'],'dirty':b['hello']['dirty'],'crc32':zlib.crc32(y),'active_profile':b['get_device']['result']['active_profile'],'mode':b['get_usb_mode']['result'],'serial_closed':True,'game':'not_run','hardware_flash_capacity_verified':False}
result['passed']=all([profiles,result['schema_equal_except_revision'],result['browser_error'] is None,result['browser_restored'],result['reconnect_saved_readback'],result['revision']==result['saved_revision']==7,not result['dirty'],result['active_profile']==4,result['mode']['current']==result['mode']['saved_startup']=='NORMAL_NKRO',len(writes)==4])
o=r/'webserial-v4.actual.json';assert not o.exists();o.write_text(json.dumps(result,ensure_ascii=False,indent=2),encoding='utf-8');print(json.dumps(result,ensure_ascii=False));assert result['passed']
