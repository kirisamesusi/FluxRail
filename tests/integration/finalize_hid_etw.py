"""Freeze actual ETW continuation evidence; no hardware operations."""
from pathlib import Path
import hashlib
import json

ROOT = Path(__file__).resolve().parent


def read(relative):
    return json.loads((ROOT / relative).read_text(encoding='utf-8-sig'))


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    out = ROOT / 'validation_hid_etw_2026-10-04.json'
    assert not out.exists(), 'Refusing to overwrite frozen report'
    previous = read('validation_hid_diagnosis_2026-10-04.json')
    old = {**previous['tools_sha256'], **previous['evidence_sha256']}
    assert all(digest(ROOT / name).lower() == value.lower() for name, value in old.items())
    control = read('hid_etw_run_2026-10-04_retry1/xml-lr-analysis.json')
    interrupt = read('hid_interrupt_run_2026-10-04/interrupt-analysis.json')
    assert control['target_get_report_events'] == []
    assert control['incomplete_setup_event_count'] == 0
    assert interrupt['payload_verified'] is False and len(interrupt['hid_completions']) == 8
    reviews = ['hid_etw_launch_2026-10-04_200244/capture-aborted.review.json',
               'hid_etw_launch_2026-10-04_retry1/capture.review.json',
               'hid_etw_run_2026-10-04_retry1/usb-control-path-analysis.review.json',
               'hid_interrupt_launch_2026-10-04/preflight.review.json',
               'hid_interrupt_run_2026-10-04/stage.review.json']
    for name in reviews:
        assert read(name)['reviewer'] in ('Luna', 'gpt-6-luna')
    for launch, run, mask in [('hid_etw_launch_2026-10-04_retry1', 'hid_etw_run_2026-10-04_retry1', '0x8041'),
                              ('hid_interrupt_launch_2026-10-04', 'hid_interrupt_run_2026-10-04', '0x8081')]:
        completed = read(launch + '/completed.json')
        assert completed['administrator'] and completed['exit_code'] == 0
        commands = read(run + '/capture-commands.json')
        assert len(commands) == 5 and all(c['exit'] == 0 for c in commands)
        assert commands[-1]['arguments'][0] == 'stop'
        assert all(mask in c['arguments'] for c in commands[:3])
    directories = ['hid_etw_launch_2026-10-04_200244', 'hid_etw_run_2026-10-04_200244',
                   'hid_etw_launch_2026-10-04_retry1', 'hid_etw_run_2026-10-04_retry1',
                   'hid_interrupt_launch_2026-10-04', 'hid_interrupt_run_2026-10-04']
    files = [p for d in directories for p in (ROOT / d).rglob('*') if p.is_file()]
    tools = ['capture_hid_etw_v2.ps1', 'analyze_usb_hid_trace.py', 'trigger_neutral_hid.ps1',
             'capture_hid_interrupt.ps1', 'analyze_hid_interrupt_trace.py', 'finalize_hid_etw.py', 'HID_ETW_2026-10-04.md']
    files.extend(ROOT / t for t in tools)
    report = {'date': '2026-10-04', 'status': 'host_path_bounded_and_interrupt_transport_verified_payload_unverified',
              'identity': {'serial': '5054316560DD729C', 'firmware': '0.10.1', 'mode': 'NORMAL_NKRO'},
              'new_host_hid_control_queries': 1, 'hid_get_report_passed': False,
              'control_conclusion': 'No corresponding GET_REPORT reached observed UCX/xHCI in this C# call; rejecting component and device control callback remain unverified.',
              'neutral_interrupt_completions': {'keyboard_29_bytes': 4, 'mouse_6_bytes': 4},
              'interrupt_transport_verified': True, 'report_bytes_verified': False,
              'new_cdc_requests': {'baseline_readonly': 75, 'action_readonly': 12, 'ram_writes': 2, 'total': 89},
              'configuration': {'active_restored': 4, 'revision': [1, 3], 'schema_diff_offsets': [4], 'profiles_unchanged': True, 'flash_writes': 0, 'serial_closed': True},
              'etw': {'control_events': 5006, 'neutral_events': 5183, 'events_lost': 0, 'buffers_lost': 0, 'both_stopped': True},
              'firmware_compile': 'not_run', 'arm_simulation': 'not_run', 'physical_input': 'not_run', 'game': 'not_run',
              'old_frozen_hashes_verified': len(old), 'actual_reviews': reviews,
              'evidence_integrity_limitation': 'Original retry1 preflight review overwritten by Luna and unavailable; marker and misplaced neutral content preserved. This is not counted as recovered original review.',
              'limitations': ['ETW software-stack observation is not physical-wire proof.', 'Payload bytes and report IDs not captured in decoded events.', 'No unique Windows rejecting component identified.', 'Bare board cannot verify physical Hall/knob/buttons, XY/Y retention or games.'],
              'next': 'Use target-payload-capable passive capture or independent host, then physical peripherals/game validation.',
              'evidence_sha256': {str(p.relative_to(ROOT)).replace('\\', '/'): digest(p) for p in sorted(files)}}
    out.write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding='utf-8')
    print(json.dumps({'report': str(out), 'files_frozen': len(files), 'old_hashes_verified': len(old)}))


if __name__ == '__main__':
    main()
