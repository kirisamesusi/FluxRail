"""Offline ETW XML inventory. Makes no device requests or automatic root-cause claim."""
import argparse
from collections import Counter, defaultdict
from datetime import datetime, timedelta, timezone
import hashlib
import json
from pathlib import Path
import xml.etree.ElementTree as ET

NS = '{http://schemas.microsoft.com/win/2004/08/events/event}'


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--xml', type=Path, required=True)
    ap.add_argument('--output', type=Path, required=True)
    args = ap.parse_args()
    assert not args.output.exists(), 'Refusing to overwrite analysis'
    events = list(ET.parse(args.xml).getroot())
    rows = []
    for i, event in enumerate(events):
        system = event.find(NS + 'System')
        provider = system.find(NS + 'Provider')
        data = defaultdict(list)
        payload = event.find(NS + 'EventData')
        if payload is not None:
            for node in payload.iter(NS + 'Data'):
                data[node.attrib.get('Name', '')].append((node.text or '').strip())
        rendering = event.find(NS + 'RenderingInfo')
        task = rendering.findtext(NS + 'Task', default='') if rendering is not None else ''
        rows.append({'index': i, 'provider': provider.attrib.get('Name', provider.attrib.get('Guid')),
                     'event_id': system.findtext(NS + 'EventID'),
                     'opcode': system.findtext(NS + 'Opcode'), 'task': task.strip(),
                     'time': system.find(NS + 'TimeCreated').attrib['SystemTime'], 'fields': dict(data)})
    header = next(r for r in rows if 'StartTime' in r['fields'])
    ticks = int(header['fields']['StartTime'][0])
    anchor_utc = datetime(1601, 1, 1, tzinfo=timezone.utc) + timedelta(microseconds=ticks // 10)
    # tracerpt on this host emits an offset +07:59 although raw FileTime aligns
    # with +08:00. Anchor against raw session FileTime; preserve source strings.
    def wall(value):
        return datetime.fromisoformat(value[:26])
    anchor_wall = wall(header['time'])
    for row in rows:
        row['utc_from_raw_header_anchor'] = (anchor_utc + (wall(row['time']) - anchor_wall)).isoformat()
    targets = []
    target_pointers = set()
    for row in rows:
        d = row['fields']
        if d.get('fid_idVendor') == ['0x239A'] and d.get('fid_idProduct') == ['0xCAFE']:
            assert len(d.get('fid_UsbDevice', [])) == 1
            target_pointers.add(d['fid_UsbDevice'][0].lower())
            targets.append(row)
    assert len(target_pointers) == 1, 'Target descriptor pointer not uniquely anchored'
    target_rows = [r for r in rows if any(x.lower() in target_pointers
                  for x in r['fields'].get('fid_UsbDevice', []))]
    setup_names = ['bmRequestType', 'bRequest', 'wValue', 'wIndex', 'wLength']
    controls = []
    all_controls = []
    for row in rows:
        d = row['fields']
        if not any('Setup' in k for k in d):
            continue
        item = {k: row[k] for k in ['index', 'provider', 'event_id', 'opcode', 'task', 'time', 'utc_from_raw_header_anchor']}
        item['device'] = d.get('fid_UsbDevice', [])
        item['setup'] = {name: d.get('fid_URB_Setup_' + name, []) for name in setup_names}
        item['fields'] = {k: v for k, v in d.items() if any(s in k for s in ['URB', 'IRP', 'PipeHandle', 'UsbDevice', 'UcxController'])}
        all_controls.append(item)
        if row in target_rows:
            controls.append(item)
    incomplete_setups = [r for r in all_controls if any(len(r['setup'][n]) != 1 for n in setup_names)]
    target_gets = [r for r in controls if r['setup']['bmRequestType'] == ['0xA1'] and r['setup']['bRequest'] == ['0x1']]
    functions = Counter(x for r in target_rows for x in r['fields'].get('fid_URB_Hdr_Function', []))
    statuses = Counter(x for r in target_rows for x in r['fields'].get('fid_URB_Hdr_Status', []))
    target_shapes = Counter((r['provider'], r['event_id'], r['task']) for r in target_rows)
    report = {
        'xml_sha256': hashlib.sha256(args.xml.read_bytes()).hexdigest(),
        'event_count': len(rows), 'provider_counts': dict(Counter(r['provider'] for r in rows)),
        'header_events_lost': header['fields'].get('EventsLost'),
        'header_buffers_lost': header['fields'].get('BuffersLost'),
        'raw_anchor_utc': anchor_utc.isoformat(), 'xml_anchor_time_preserved': header['time'],
        'target_pointer': sorted(target_pointers), 'target_descriptor_events': targets,
        'target_event_count': len(target_rows),
        'target_event_shapes': [{'provider': p, 'event_id': i, 'task': t, 'count': n}
                                for (p, i, t), n in target_shapes.items()],
        'target_urb_function_counts': dict(functions), 'target_urb_status_counts': dict(statuses),
        'all_setup_event_count': len(all_controls), 'incomplete_setup_event_count': len(incomplete_setups),
        'target_setup_events': controls, 'target_get_report_events': target_gets,
        'setup_field_names': sorted({k for r in rows for k in r['fields'] if 'Setup' in k}),
        'limitations': ['Offline XML inventory only; schema/positive traffic and window coverage require review.',
                        'Pointer identifies this trace session only; metadata descriptor and CDC identity must be cross-checked.',
                        'No automatic root-cause claim from absence of GET_REPORT.']}
    args.output.write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding='utf-8')
    print(json.dumps({k: report[k] for k in ['event_count', 'target_event_count', 'target_pointer',
         'all_setup_event_count', 'incomplete_setup_event_count', 'target_urb_function_counts',
         'target_urb_status_counts']}, ensure_ascii=True))
    print(json.dumps([{'utc': r['utc_from_raw_header_anchor'], 'task': r['task'], 'setup': r['setup']}
                      for r in controls], ensure_ascii=True))


if __name__ == '__main__':
    main()
