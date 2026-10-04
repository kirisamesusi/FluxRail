"""Offline analysis of neutral RAM-profile USB capture; does not operate hardware."""
import argparse
from collections import Counter
from datetime import datetime, timedelta, timezone
import hashlib
import json
from pathlib import Path
import xml.etree.ElementTree as ET
import zlib

NS = '{http://schemas.microsoft.com/win/2004/08/events/event}'


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--directory', type=Path, required=True)
    args = ap.parse_args()
    p = args.directory
    out = p / 'interrupt-analysis.json'
    assert not out.exists(), 'Refusing to overwrite evidence'
    rows = []
    for i, e in enumerate(ET.parse(p / 'usb-events.xml').getroot()):
        s = e.find(NS + 'System')
        d = {}
        for node in e.findall('.//' + NS + 'Data'):
            d.setdefault(node.get('Name'), []).append(node.text or '')
        rows.append({'index': i, 'provider': s.find(NS + 'Provider').get('Name'),
                     'event_id': s.findtext(NS + 'EventID'),
                     'time': s.find(NS + 'TimeCreated').get('SystemTime'), 'fields': d})
    header = next(r for r in rows if 'StartTime' in r['fields'])
    anchor = datetime(1601, 1, 1, tzinfo=timezone.utc) + timedelta(
        microseconds=int(header['fields']['StartTime'][0]) // 10)
    wall = lambda s: datetime.fromisoformat(s[:26])
    for r in rows:
        r['utc_from_raw_header_anchor'] = (anchor + (wall(r['time']) - wall(header['time']))).isoformat()
    pointers = {r['fields']['fid_UsbDevice'][0] for r in rows
                if r['fields'].get('fid_idVendor') == ['0x239A']
                and r['fields'].get('fid_idProduct') == ['0xCAFE']}
    assert len(pointers) == 1
    target = [r for r in rows if r['fields'].get('fid_UsbDevice', [''])[0] in pointers]
    pipes = {}
    for r in target:
        d = r['fields']
        if 'fid_PipeHandle' in d and 'fid_bEndpointAddress' in d:
            pipe, ep = d['fid_PipeHandle'][0], d['fid_bEndpointAddress'][0]
            assert pipe not in pipes or pipes[pipe] == ep
            pipes[pipe] = ep
    completions = []
    for r in target:
        d = r['fields']
        endpoint = pipes.get(d.get('fid_PipeHandle', [''])[0])
        if endpoint not in ('0x83', '0x84') or r['event_id'] != '27':
            continue
        assert d['fid_URB_Hdr_Function'] == ['0x9']
        completions.append({**r, 'endpoint': endpoint,
                            'actual_transfer_length': int(d['fid_URB_TransferBufferLength'][0], 16),
                            'urb_status': d['fid_URB_Hdr_Status'][0],
                            'nt_status': d['fid_IRP_NtStatus'][0],
                            'device_report_hex': None,
                            'payload_verified': False})
    assert Counter(r['endpoint'] for r in completions) == {'0x83': 4, '0x84': 4}
    for r in completions:
        assert r['urb_status'] == r['nt_status'] == '0x0'
        assert r['actual_transfer_length'] == (29 if r['endpoint'] == '0x83' else 6)
    records = [json.loads(s) for s in (p / 'action-cdc.jsonl').read_text(encoding='utf-8-sig').splitlines()]
    assert len({r['request']['id'] for r in records}) == len(records)
    assert all(r['response']['ok'] and r['response']['id'] == r['request']['id'] for r in records)
    writes = [r for r in records if r['request']['op'] == 'set_active_profile']
    assert [r['request']['payload']['profile_id'] for r in writes] == [1, 4]
    assert all(r['request']['op'] in ('hello', 'get_device', 'get_status', 'set_active_profile') for r in records)
    action = json.loads((p / 'action-summary.json').read_text(encoding='utf-8-sig'))
    assert action['restored'] and action['serial_closed'] and action['error'] is None
    assert all(datetime.fromisoformat(action['started'].replace('Z', '+00:00')) <=
               datetime.fromisoformat(r['utc_from_raw_header_anchor']) <=
               datetime.fromisoformat(action['finished'].replace('Z', '+00:00')) for r in completions)
    baselines = [json.loads((p / t / 'baseline.json').read_text(encoding='utf-8-sig')) for t in ('before', 'after')]
    bins = [(p / t / 'original-schema2.bin').read_bytes() for t in ('before', 'after')]
    assert len(bins[0]) == len(bins[1]) == 1387
    diff = [i for i, (a, b) in enumerate(zip(*bins)) if a != b]
    assert all(4 <= i <= 7 for i in diff)
    assert [b['hello']['revision'] for b in baselines] == [1, 3]
    assert all(zlib.crc32(data) == b['export_metadata']['crc32'] for data, b in zip(bins, baselines))
    assert all(b['get_device']['result']['active_profile'] == 4 and
               b['get_status']['result']['physical_mask'] == 0 and
               b['get_status']['result']['config_ready'] and not b['get_status']['result']['apply_failed'] and
               b['hello']['saved_revision'] is None and b['get_status']['result']['valid_slots'] == 0 for b in baselines)
    profiles = [json.loads((p / t / 'original-profiles.json').read_text(encoding='utf-8-sig')) for t in ('before', 'after')]
    assert profiles[0] == profiles[1]
    report = {'status': 'neutral_interrupt_transport_verified_payload_not_verified',
              'event_count': len(rows), 'target_event_count': len(target), 'target_pointer': sorted(pointers),
              'events_lost': header['fields']['EventsLost'], 'buffers_lost': header['fields']['BuffersLost'],
              'endpoint_mapping': pipes, 'hid_completions': completions,
              'action_request_count': len(records), 'ram_writes': writes,
              'restored': action['restored'], 'revision_before_after': [1, 3],
              'schema_diff_offsets': diff, 'profiles_unchanged': True,
              'xml_sha256': hashlib.sha256((p / 'usb-events.xml').read_bytes()).hexdigest(),
              'payload_verified': False,
              'limitations': ['Transfer lengths and completion statuses are observed; no report data bytes found in decoded events.',
                              'Zero physical mask/source expectation is not a captured all-zero payload.',
                              'No physical switches, Hall/knob movement, absolute XY, game, firmware compile or ARM simulation validated.']}
    out.write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding='utf-8')
    print(json.dumps({k: report[k] for k in ('status', 'event_count', 'target_event_count',
          'events_lost', 'buffers_lost', 'action_request_count', 'revision_before_after', 'schema_diff_offsets')}))


if __name__ == '__main__':
    main()
