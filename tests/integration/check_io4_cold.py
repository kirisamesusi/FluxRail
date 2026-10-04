import json
from pathlib import Path
from datetime import datetime
r=Path(__file__).parent/'pc_board_2026-10-04';load=lambda p:json.loads(p.read_text(encoding='utf-8-sig'))
m=load(r/'io4-cold-monitor/summary.json');p=load(r/'io4-cold-pnp.json');h=load(r/'io4-cold-report.json');c=load(r/'io4-cold-user-confirmation.json')
events=m['events'];a=next(x['utc'] for x in events if not x['present']);b=next(x['utc'] for x in events if x['present'] and x['utc']>a)
elapsed=(datetime.fromisoformat(b)-datetime.fromisoformat(a)).total_seconds()
v=h[0];normal=[x for x in p if 'VID_239A&PID_CAFE' in x['InstanceId']]
result={'kind':'USB_only_idle_IO4_physical_power_cycle','user_confirmation':c,'pnp_absence_seconds':elapsed,'pnp_after':p,'normal_nodes_after':normal,'target_serial':v['serial'],'input_control_passed':v['control_get_input_report']['passed'],'actual_report_hex':v['control_get_input_report']['hex'],'system_after':v['decoded']['system'],'io4_caps_after':v['caps'],'configuration_snapshot_available_in_pure_io4':False,'brief_normal_enumeration_excluded':False,'flash_writes':0,'game':'not_run'}
result['passed']=elapsed>=5 and c['no_jumpers'] and c['removed_usb_at_least_5_seconds'] and not normal and v['serial']=='5054316560DD729C' and result['input_control_passed'] and result['system_after']==2
o=r/'io4-cold.actual.json';assert not o.exists();o.write_text(json.dumps(result,ensure_ascii=False,indent=2),encoding='utf-8');print(json.dumps(result,ensure_ascii=False));assert result['passed']
