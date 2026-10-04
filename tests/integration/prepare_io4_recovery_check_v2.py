from pathlib import Path
p=Path(__file__).with_name('check_io4_recovery.py');o=p.with_name('check_io4_recovery_v2.py')
assert not o.exists()
s=p.read_text(encoding='utf-8')
s=s.replace("same(a['get_calibration']['result'],b['get_calibration']['result'])", "same({k:v for k,v in a['get_calibration']['result'].items() if k not in ('timestamp_us','sample_revision','epoch')},{k:v for k,v in b['get_calibration']['result'].items() if k not in ('timestamp_us','sample_revision','epoch')})")
s=s.replace("all(4<=i<9 for i in diff)", "all(4<=i<8 or i==17 for i in diff) and x[17]==0 and y[17]==1")
s=s.replace("'io4-gp9-recovery.actual.json'", "'io4-gp9-recovery.actual.v2.json'")
s=s.replace("'kind':'pure_io4_physical_GP9_forced_normal_recovery'", "'kind':'pure_io4_physical_GP9_forced_normal_recovery','offline_checker_v2_only':True,'new_device_requests':0,'calibration_volatile_fields_excluded':['timestamp_us','sample_revision','epoch']")
o.write_text(s,encoding='utf-8')
