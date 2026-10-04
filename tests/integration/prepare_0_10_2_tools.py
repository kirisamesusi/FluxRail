"""Create separately versioned tools; keep all 0.10.1 failures/sources frozen."""
from pathlib import Path
import ast
here=Path(__file__).resolve().parent
def write(name,text):
    p=here/name;assert not p.exists();p.write_text(text,encoding='utf-8')
s=(here/'switch_usb_mode_once_v3.ps1').read_text(encoding='utf-8')
start=s.index("  $gate=Get-Content")
end=s.index("\n }\n $ports=",start)
s=s[:start]+'''  $gate=Get-Content -LiteralPath "$PSScriptRoot/pc_board_0_10_2_2026-10-05/backup-baseline.main.review.json" -Encoding UTF8 -Raw|ConvertFrom-Json
  if(!$gate.passed -or $gate.serial -ne '5054316560DD729C' -or $gate.new_version -ne '0.10.2' -or $gate.revision -ne 9){throw 'Fresh new-firmware baseline and unchanged recovery code must pass first'}'''+s[end:]
s=s.replace("$h.result.firmware_version -ne '0.10.1'","$h.result.firmware_version -ne '0.10.2'")
s=s.replace("if($Target -eq 'IO4' -and (!$m.result.forced_recovery -or $h.revision -ne $gate.revision))", "if($Target -eq 'IO4' -and $h.revision -ne $gate.revision)")
write('switch_usb_mode_once_0_10_2.ps1',s)
s=(here/'watch_io4_recovery.ps1').read_text(encoding='utf-8').replace('-Version 0.10.1','-Version 0.10.2')
write('watch_io4_recovery_0_10_2.ps1',s)
s=(here/'test_io4_protocol_diagnostic.py').read_text(encoding='utf-8')
old="            output(1,0);time.sleep(.02);assert get_input('disabled_after_'+str(units))['system']==0x30"
assert s.count(old)==1
new='''            assert early==0x30, 'Premature timeout at early sample'
            # The accepted command lies inside the output call. A sample ending
            # before call-start + deadline is definitely before expiration.
            time.sleep(max(0,units*.1-.02-(time.perf_counter()-start)))
            before=get_input('before_deadline_'+str(units))['system']
            before_end=time.perf_counter()-start
            assert before_end<units*.1, 'Host scheduling missed pre-deadline sample; inconclusive timing'
            assert before==0x30, 'Expired before earliest possible deadline'
            # Output completion was included in early elapsed; by this later
            # point even the latest possible accepted-command deadline passed.
            time.sleep(max(0,units*.1+.06-(time.perf_counter()-start)))
            after=get_input('after_deadline_'+str(units))['system']
            after_end=time.perf_counter()-start
            assert after==2, 'Did not expire after deadline'
            report['timing_trials'][-1].update(before_end_seconds=before_end,before_system=before,after_end_seconds=after_end,after_system=after,deadline_window_passed=True)
            put()
            output(1,0);time.sleep(.02);assert get_input('disabled_after_'+str(units))['system']==0x30'''
s=s.replace(old,new)
ast.parse(s);write('test_io4_timeout_fixed_actual.py',s)
s=(here/'run_io4_diag_watchdog.ps1').read_text(encoding='utf-8').replace('test_io4_protocol_diagnostic.py','test_io4_timeout_fixed_actual.py').replace('test_io4_protocol_diagnostic-executed.py','test_io4_timeout_fixed_actual-executed.py')
write('run_io4_timeout_fixed_watchdog.ps1',s)
print('Prepared new version mode/physical recovery and 100ms/400ms/25.5s actual deadline tests')
