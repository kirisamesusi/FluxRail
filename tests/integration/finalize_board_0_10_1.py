"""Freeze actual 0.10.1 bare-board evidence; never operates the board."""
from pathlib import Path
import datetime
import hashlib
import json

ROOT = Path(__file__).resolve().parents[2]
HERE = Path(__file__).resolve().parent
EVIDENCE = HERE / 'hardware_0_10_1_2026-10-04'
RUN = EVIDENCE / 'regression'


def read(path):
    return json.loads(path.read_text(encoding='utf-8-sig'))


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    baseline = read(RUN / 'baseline.json')
    run = read(RUN / 'report.json')
    assert baseline['hello']['result']['firmware_version'] == '0.10.1'
    serial = baseline['hello']['result']['serial']
    assert run['serial'] == serial
    assert not run.get('failure') and not run.get('restore_error')
    assert run['restored'] and run['port_close_verified']
    assert not run['flash_written'] and not run['game_tested']
    assert len(run['stages']) == 6
    for n, stage in enumerate(run['stages'], 1):
        review = read(RUN / f'{n}.review.json')
        assert stage['review_status'] == 'reviewed'
        assert review['stage'] == n and review['reviewer'] == 'gpt-6-luna'
        assert review['run_started'] == run['started']
        assert review['stage_name'] == stage['name'] and review['continue'] is True
    original = (RUN / 'original-schema2.bin').read_bytes()
    final = (RUN / 'final-schema2.bin').read_bytes()
    assert len(original) == len(final)
    assert original[:4] == final[:4] and original[8:] == final[8:]
    assert int.from_bytes(final[4:8], 'little') == run['final_revision']
    pnp = read(EVIDENCE / 'windows-pnp.json')
    col02 = [d for d in pnp if 'MI_03&COL02' in d['InstanceId'].upper()]
    assert len(col02) == 1 and col02[0]['Status'] == 'OK'
    assert next(p['Data'] for p in col02[0]['Properties']
                if p['KeyName'] == 'DEVPKEY_Device_ProblemCode') == 0
    hid = read(EVIDENCE / 'windows-hid-probe-guarded.json')
    for collection in ['col01', 'col02']:
        matches = [d for d in hid if d.get('serial') == serial
                   and collection in d['path'].lower()]
        assert len(matches) == 1
        assert matches[0]['caps']['InputReportByteLength'] == 6
        assert all(matches[0]['local_parser_zero_report'][a]['status'] == '0x00110000'
                   for a in ['X', 'Y'])
    web = read(RUN / 'web-client-report.json')
    assert not web.get('failure') and web['result']['passed']
    assert web['result']['serial'] == serial
    assert web['result']['version'] == '0.10.1'
    assert web['result']['queued_read_requests'] == 50
    assert not web['browser_ui_tested'] and not web['flash_written']
    web_review = read(RUN / 'web-client.review.json')
    assert web_review['reviewer'] == 'gpt-6-luna' and web_review['continue'] is True
    assert web_review['run_started'] == web['started']
    current_hid = read(EVIDENCE / 'windows-hid-current-report.json')
    current_hid_review = read(EVIDENCE / 'hid-current-report.review.json')
    assert current_hid_review['reviewer'] == 'gpt-6-luna'
    assert current_hid_review['test_passed'] is False
    control_results = []
    for item in current_hid:
        if item.get('serial') != serial:
            continue
        result = item['control_get_input_report']
        assert result['passed'] is False and result['error'] == 1
        control_results.append({'path': item['path'], 'passed': False, 'win32_error': result['error'],
                                'buffer_is_device_report': False,
                                'failure_buffer_excluded_from_device_data': True})
    assert len(control_results) == 3
    post_hid = read(EVIDENCE / 'post-hid-cdc.json')
    for response in post_hid.values():
        assert response['ok'] and response['revision'] == run['final_revision']
    assert post_hid['hello']['result']['serial'] == serial
    assert post_hid['get_device']['result'] == baseline['get_device']['result']
    status = post_hid['get_status']['result']
    assert status['config_ready'] and not status['applying'] and not status['apply_failed']
    assert status['maintenance'] == 'idle' and status['valid_slots'] == 0
    rate = read(RUN / 'hall-rate-report.json')
    rate_summary = {}
    for name, result in rate['rates'].items():
        start, end = result['start'], result['end']
        assert start['sample_revision'] == end['sample_revision']
        assert start['epoch'] == end['epoch']
        us = (end['timestamp_us'] - start['timestamp_us']) & 0xffffffff
        attempts = (end['hall']['adc_attempts'] - start['hall']['adc_attempts']) & 0xffffffff
        assert us == result['device_elapsed_us'] and attempts == result['attempts_added']
        assert abs(attempts * 1e6 / us - result['hall_hz']) < 1e-9
        rate_summary[name] = {
            k: result[k] for k in ['device_elapsed_us', 'attempts_added', 'hall_hz',
                                   'skipped_periods_added', 'samples_added']}
        rate_summary[name]['maxima_before'] = {
            k: start['hall'][k] for k in ['max_lateness_us', 'max_interval_us',
                                         'max_span_us', 'max_service_us']}
        rate_summary[name]['maxima_after'] = {
            k: end['hall'][k] for k in rate_summary[name]['maxima_before']}
    # Describe local host response gaps separately from the full-window poll mean.
    paced = rate['rates']['paced']
    start_motion = paced['start']
    rows = [json.loads(line) for line in
            (RUN / 'automated-cdc.jsonl').read_text(encoding='utf-8').splitlines()]
    def matches_motion(row, motion):
        return row.get('request', {}).get('op') == 'get_motion' and row.get('response', {}).get('result') == motion
    start_index = next(i for i, row in enumerate(rows) if matches_motion(row, start_motion))
    end_index = next(i for i in range(start_index + 1, len(rows))
                     if matches_motion(rows[i], paced['end']))
    response_times = []
    for row in rows[start_index:end_index + 1]:
        if row.get('request', {}).get('op') != 'get_motion':
            continue
        motion = row.get('response', {}).get('result', {})
        if (motion.get('sample_revision'), motion.get('epoch')) != (
                start_motion['sample_revision'], start_motion['epoch']):
            continue
        elapsed = (motion['timestamp_us'] - start_motion['timestamp_us']) & 0xffffffff
        if elapsed <= paced['device_elapsed_us']:
            response_times.append(datetime.datetime.fromisoformat(row['time'].replace('Z', '+00:00')))
    gaps = sorted((b - a).total_seconds() * 1000
                  for a, b in zip(response_times, response_times[1:]))
    assert gaps
    poll_gap_stats = {'basis': 'host completion timestamps between exact full start/end motion response matches in CDC log order, additionally filtered by revision/epoch/device elapsed time',
                      'start_cdc_line': start_index + 1, 'end_cdc_line': end_index + 1,
                      'host_start': rows[start_index]['time'], 'host_end': rows[end_index]['time'],
                      'responses': len(response_times), 'min_ms': gaps[0],
                      'p50_ms': gaps[len(gaps) // 2], 'p95_ms': gaps[int(len(gaps) * .95)],
                      'max_ms': gaps[-1], 'not_a_sensor_or_usb_input_latency': True}
    software = read(HERE / 'validation_bugfix_2026-10-04.json')
    for rel, expected in software['firmware']['source_sha256'].items():
        assert digest(ROOT / rel) == expected, rel
    assert digest(ROOT / software['uf2']['path']) == software['uf2']['sha256']
    aborted = read(EVIDENCE / 'report.json')
    assert aborted.get('failure', {}).get('message', '').startswith('Luna review requires stopping')
    assert aborted['restored'] and aborted['final_revision'] == 1
    assert not aborted['flash_written'] and not aborted.get('restore_error')
    assert (EVIDENCE / 'original-schema2.bin').read_bytes() == (EVIDENCE / 'final-schema2.bin').read_bytes()
    for line in (EVIDENCE / 'automated-cdc.jsonl').read_text(encoding='utf-8').splitlines():
        assert json.loads(line)['request']['op'] in {
            'hello', 'get_status', 'get_device', 'get_profile', 'export_config'}
    tools = ['prepare_board_baseline.ps1', 'serial_bridge.ps1', 'bare_board_test.mjs',
             'web_client_board_test.mjs', 'windows_hid_probe.py',
             'finalize_board_0_10_1.py', 'BARE_BOARD_RUN.md']
    hashes = {p.relative_to(ROOT).as_posix(): digest(p)
              for p in EVIDENCE.rglob('*') if p.is_file()}
    report = {
        'date_local': datetime.datetime.now(datetime.timezone(datetime.timedelta(hours=8))).isoformat(),
        'scope': '0.10.1 same-board Windows enumeration, reversible RAM/protocol/session and bare-board driver/CDC load',
        'firmware_version_readback': '0.10.1', 'serial': serial, 'port': run['port'],
        'new_firmware_compile': False, 'new_arm_simulation': False, 'uploaded': False,
        'historical_software_report': 'tests/integration/validation_bugfix_2026-10-04.json',
        'source_and_local_uf2_unchanged': True, 'board_image_bytes_readback': False,
        'windows_col02_code10_resolved_on_this_host': True,
        'windows_hid_caps_and_local_parser_verified': True,
        'actual_hid_motion_or_buttons_verified': False,
        'hid_control_current_report_read': {'attempted': True, 'passed': False,
                                            'results': control_results,
                                            'review': current_hid_review,
                                            'root_cause': 'unresolved: access-zero handle/host API/driver/device control path not isolated'},
        'cdc_healthy_after_failed_hid_read': True,
        'real_board_cdc_tested': True, 'ram_config_restored_except_revision': True,
        'initial_revision': baseline['hello']['revision'], 'final_revision': run['final_revision'],
        'flash_written': False, 'game_tested': False, 'browser_web_serial_ui_tested': False,
        'physical_usb_unplug_tested': False, 'physical_peripherals_tested': False,
        'physical_flash_capacity_verified': False, 'io4_personality_tested': False,
        'hall_driver_mean_hz': rate_summary, 'load': rate['load'],
        'paced_get_motion_host_response_gaps': poll_gap_stats,
        'stages': run['stages'], 'web_client': web, 'web_client_review': web_review,
        'initial_tool_run_safely_stopped': True,
        'tool_sha256': {f'tests/integration/{p}': digest(HERE / p) for p in tools},
        'tool_hash_scope': 'Current tool sources at freeze time. HID failure-buffer labeling was clarified after the optional failed read; that read was not repeated.',
        'evidence_sha256': hashes,
        'limitations': [
            'Only this Windows host and device instance; no broad Windows certification',
            'No calibrated Hall position, real knob motion/PUSH, Y/button behavior, physical LEDs or games',
            'SSI zero frame with valid CRC does not prove a connected MT6701',
            'Poll rate is a full-window mean; host request latency is not hardware input/USB latency',
            'Hall diagnostics are cumulative maxima and include earlier maintenance/configuration pauses',
            'Enabling drivers without sensors does not create real motion HID bandwidth pressure',
            'No Flash save/power-loss, IO4 switch/recovery or ROM upgrade test',
            'Extra HID current-state API read returned Win32 error 1 on all three collections; initialized failure buffers are not device data',
            'Same device version/serial is not a byte-for-byte board Flash/UF2 comparison'],
    }
    target = HERE / 'validation_bare_board_0_10_1_2026-10-04.json'
    target.write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding='utf-8')
    print(json.dumps({'path': str(target), 'hall_hz': {n: r['hall_hz']
          for n, r in rate_summary.items()}, 'final_revision': run['final_revision']}, ensure_ascii=False))


if __name__ == '__main__':
    main()
