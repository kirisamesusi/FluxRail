"""Offline compare real USB-only power-cycle evidence."""
import json
import zlib
from datetime import datetime
from pathlib import Path
from compare_json_values import same

root=Path(__file__).parent/'pc_board_2026-10-04'
out=root/'power-cut.actual.json'
assert not out.exists(), 'Refusing to overwrite evidence'
load=lambda p:json.loads(p.read_text(encoding='utf-8-sig'))
a=load(root/'power-cut-before/baseline.json')
b=load(root/'power-cut-after/baseline.json')
monitor=load(root/'power-cut-monitor/summary.json')
events=monitor['events']
absent=next(e for e in events if not e['present'])
present=next(e for e in events if e['present'] and e['utc']>absent['utc'])
delta=(datetime.fromisoformat(present['utc'].replace('Z','+00:00'))-datetime.fromisoformat(absent['utc'].replace('Z','+00:00'))).total_seconds()
x=(root/'power-cut-before/original-schema2.bin').read_bytes()
y=(root/'power-cut-after/original-schema2.bin').read_bytes()
profiles_equal=same(load(root/'power-cut-before/original-profiles.json'),load(root/'power-cut-after/original-profiles.json'))
result={'kind':'physical_usb_only_power_cycle','user_confirmation':'USB only; unplugged at least 5 seconds and reinserted','pnp_absence_seconds':delta,'pnp_disconnected_and_reconnected':monitor.get('disconnect_and_reconnect_observed',False),'schema_byte_exact':x==y,'length':len(y),'crc32':zlib.crc32(y),'profiles_equal':profiles_equal,'revision_before':a['hello']['revision'],'revision_after':b['hello']['revision'],'saved_revision_after':b['hello']['saved_revision'],'dirty_after':b['hello']['dirty'],'usb_mode_after':b['get_usb_mode']['result'],'status_after':b['get_status']['result'],'serial_closed_by_baseline_finally':True,'device_writes':0,'firmware_compile':'not_run','simulation':'not_run','game':'not_run','commit_interruption':'not_tested'}
result['passed']=all([delta>=5,result['pnp_disconnected_and_reconnected'],x==y,profiles_equal,b['hello']['revision']==b['hello']['saved_revision']==a['hello']['revision'],not b['hello']['dirty'],b['get_usb_mode']['result']['current']=='NORMAL_NKRO',b['get_status']['result']['config_ready'],b['get_status']['result']['physical_mask']==0])
out.write_text(json.dumps(result,ensure_ascii=False,indent=2),encoding='utf-8')
print(json.dumps(result,ensure_ascii=False))
assert result['passed'], 'Power-cycle recovery check failed'
