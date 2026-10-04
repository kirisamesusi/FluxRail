from pathlib import Path
p=Path(__file__).with_name('test_io4_protocol_actual_v2.py');o=p.with_name('test_io4_protocol_diagnostic.py')
assert not o.exists()
s=p.read_text(encoding='utf-8')
s=s.replace("assert get_input('initial_control')['system']==2", "assert get_input('initial_control')['system'] in (0,2)")
s=s.replace("v['system']==2 and v['reserved_hex']", "v['system'] in (0,2) and v['reserved_hex']")
start=s.index('    if not args.preflight_only:')
end=s.index('\nexcept BaseException',start)
s=s[:start]+'''    if not args.preflight_only:
        output(1,0);time.sleep(.02);assert get_input('timeout_disabled_init')['system']==0x30
        report['timing_trials']=[]
        for units in (1,4,255):
            start=time.perf_counter();output(1,units);time.sleep(.02)
            early=get_input('timed_init_'+str(units))['system'];elapsed=time.perf_counter()-start
            report['timing_trials'].append({'timeout_units':units,'expected_timeout_seconds':units*.1,'host_elapsed_seconds':elapsed,'early_system':early,'expired_before_expected_deadline':early==2 and elapsed<units*.1})
            output(1,0);time.sleep(.02);assert get_input('disabled_after_'+str(units))['system']==0x30
        output(3,method='interrupt');time.sleep(.02);assert get_input('interrupt_cmd3_initialized')['system']==0
        output(2,32);time.sleep(.02);assert get_input('control_cmd2_initialized')['system']==0x30
        output(3);time.sleep(.02);assert get_input('control_cmd3_initialized')['system']==0
        output(4);time.sleep(.02);assert get_input('zero_gpio_state')['system']==0
        output(0x85);time.sleep(.02);assert get_input('unknown_0x85_state')['system']==0
        report['interrupt_input_passed']=True;report['interrupt_cmd3_passed']=True;report['control_cmd2_state_transition_passed']=True
        report['zero_gpio_transport_passed']=True;report['unknown_preserved_system_state']=True
        report['premature_timeout_observed']=any(t['expired_before_expected_deadline'] for t in report['timing_trials'])
''' +s[end:]
s=s.replace("output(1,1);output(3);time.sleep(.13);report['idle_restored']=get_input('final_idle')['system']==2", "output(1,0);output(3);time.sleep(.02);report['neutral_initialized_final']=get_input('final_initialized_neutral')['system']==0")
s=s.replace("report.get('idle_restored',False)", "report.get('neutral_initialized_final',False)")
s=s.replace("report['error']=repr(e)", "report['error']=repr(e);report['error_stack']=__import__('traceback').format_exc()")
s=s.replace("'game':'not_run','error':None", "'game':'not_run','kind':'diagnostic_not_full_protocol_acceptance','error':None")
o.write_text(s,encoding='utf-8')
