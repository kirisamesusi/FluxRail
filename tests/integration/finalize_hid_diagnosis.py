"""Freeze evidence from the 2026-10-04 limited HID investigation, not a HID pass."""
import hashlib
import json
from pathlib import Path
import zlib

HERE = Path(__file__).resolve().parent
RUN = HERE / 'hid_diagnosis_2026-10-04'
OUTPUT = HERE / 'validation_hid_diagnosis_2026-10-04.json'


def read(path):
    return json.loads(path.read_text(encoding='utf-8'))


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    assert not OUTPUT.exists(), 'Refusing to overwrite frozen evidence'
    original = (RUN / 'before/original-schema2.bin').read_bytes()
    original_profiles = read(RUN / 'before/original-profiles.json')
    checks = {}
    allowed = {'hello', 'get_status', 'get_device', 'list_profiles', 'get_config',
               'get_motion', 'get_calibration', 'get_usb_mode', 'get_profile', 'export_config'}
    for name in ['before', 'after-native', 'after-csharp', 'after-csharp-query', 'final', 'after-final-query']:
        folder = RUN / name
        baseline = read(folder / 'baseline.json')
        binary = (folder / 'original-schema2.bin').read_bytes()
        assert binary == original and len(binary) == 1387
        assert read(folder / 'original-profiles.json') == original_profiles
        hello = baseline['hello']
        assert hello['result']['serial'] == '5054316560DD729C'
        assert hello['result']['firmware_version'] == '0.10.1'
        assert hello['result']['usb_mode'] == 'NORMAL_NKRO'
        assert hello['revision'] == hello['applied_revision'] == 1
        status = baseline['get_status']['result']
        assert status['config_ready'] and not status['applying'] and not status['apply_failed']
        assert status['maintenance'] == 'idle' and status['valid_slots'] == 0
        assert status['protocol_rejected'] == 0
        meta = baseline['export_metadata']
        assert meta['revision'] == 1 and meta['length'] == len(binary)
        assert meta['crc32'] == zlib.crc32(binary) == 1053904252
        lines = [json.loads(s) for s in (folder / 'baseline-cdc.jsonl').read_text(encoding='utf-8').splitlines()]
        assert len(lines) == 15
        assert all(s['request']['op'] in allowed and s['response']['ok'] and
                   s['response']['revision'] == 1 and
                   s['response']['id'] == s['request']['id'] and
                   s['response']['protocol_version'] == 1 for s in lines)
        checks[name] = {'requests': len(lines), 'revision': 1,
                        'binary_unchanged': True, 'profiles_unchanged': True, 'crc32': meta['crc32']}
    matrix = read(RUN / 'open-matrix.json')
    assert len(matrix['attempts']) == 8
    assert all(not r['calls'] for r in matrix['attempts'])
    assert all(r['open_error'] == 5 for r in matrix['attempts'] if r['desired_access'] == '0x80000000')
    assert all(r['opened'] and r['handle_closed'] and r['serial'] == '5054316560DD729C'
               for r in matrix['attempts'] if r['desired_access'] == '0x00000000')
    native = read(RUN / 'native-col02.json')
    calls = [c for r in native['attempts'] for c in r['calls']]
    assert len(calls) == 1
    call = calls[0]
    assert call['ntstatus'] == '0xC0000010' and call['error'] == 1
    assert not call['passed'] and not call['buffer_is_device_report'] and call['hex'] is None
    assert call['length'] == 6 and call['report_id'] == 2
    assert all(r['handle_closed'] for r in native['attempts'] if r['opened'])
    csharp = read(RUN / 'csharp-col02.json')
    assert csharp['control_requests'] == 1 and csharp['serial'] == '5054316560DD729C'
    assert csharp['usage'] == 2 and csharp['usage_page'] == 1 and csharp['length'] == 6
    assert not csharp['passed'] and csharp['error'] == 1 and csharp['hex'] is None
    assert not csharp['buffer_is_device_report'] and csharp['handle_closed']
    etw = read(RUN / 'etw-attempt.json')
    assert etw['exit'] == -2147024891
    assert not (RUN / 'usb.etl').exists(), 'Unexpected trace: requires separate review'
    pnp = read(RUN / 'windows-pnp.json')
    assert len(pnp) == 7 and all(r['status'] == 'OK' and
        r['properties']['DEVPKEY_Device_ProblemCode'] == 0 for r in pnp)
    review_names = ['before-baseline', 'open-matrix', 'native-col02',
                    'csharp-first-attempt-aborted', 'csharp-open-aborted',
                    'csharp-open-binding', 'csharp-final-binding-open', 'csharp-col02']
    for name in review_names:
        assert read(RUN / f'{name}.review.json')['reviewer'] in {'Luna', 'gpt-6-luna'}
    evidence = sorted(p for p in RUN.rglob('*') if p.is_file())
    tools = [HERE / name for name in ['diagnose_hid_read.py', 'diagnose_hid_open.ps1',
             'confirm_hid_read.ps1', 'capture_hid_etw.ps1', 'prepare_board_baseline.ps1',
             'windows_hid_probe.py', 'finalize_hid_diagnosis.py']]
    report = {
        'date': '2026-10-04', 'status': 'partial_diagnosis_root_cause_unresolved',
        'identity': {'serial': '5054316560DD729C', 'firmware': '0.10.1', 'mode': 'NORMAL_NKRO'},
        'host_hid_control_query_count': 2,
        'hid_current_report_reads_passed': False,
        'native': {'ntstatus': call['ntstatus'], 'mapped_win32_error': call['error'],
                   'iosb_status': call['iosb_status'], 'iosb_is_completion_evidence': False},
        'independent_csharp': {'passed': False, 'error': csharp['error'], 'closed': True},
        'windows_read_open_error': 5, 'pnp_problem_codes': [0] * 7,
        'usb_bus_trace_available': False, 'etw_permission_denied': True,
        'configuration_checks': checks,
        'firmware_compile': 'not_run', 'arm_simulation': 'not_run', 'game': 'not_run',
        'host_tool_checks': 'Python syntax, PowerShell syntax and runtime C# Add-Type',
        'limitations': [
            'No evidence whether GET_REPORT setup reached USB or which layer rejected it.',
            'System-use keyboard/mouse exclusive access is a candidate, not a proven root cause.',
            'Failed buffers and initial IOSB values are not device reports.',
            'No physical input/events, host interrupt stream or gameplay validated.',
            'Combined C# tool fixes did not isolate the unique cause of earlier open failures.',
            'ETW continuation script statically reviewed only; admin capture not executed.',
            'Revision changed from prior run 56 to this run 1; cause/timing unknown.'
        ],
        'next': 'Run capture_hid_etw.ps1 in administrator environment with a new output directory; verify trace stop and positive USB traffic before analyzing target GET_REPORT.',
        'tools_sha256': {p.relative_to(HERE).as_posix(): sha(p) for p in tools},
        'evidence_sha256': {p.relative_to(HERE).as_posix(): sha(p) for p in evidence}}
    OUTPUT.write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding='utf-8')
    print(f'Frozen {len(evidence)} evidence files; HID reads failed, bus root cause unresolved')


if __name__ == '__main__':
    main()
