from pathlib import Path
p=Path(__file__).with_name('windows_io4_report_probe.py');o=p.with_name('test_io4_protocol_actual.py')
assert not o.exists()
s=p.read_text(encoding='utf-8').split('args.output.parent.mkdir')[0]
s=s[s.index('import ctypes'):]
s='"""Real IO4 host protocol test. Metadata identity first, bounded input reads, RAM protocol commands; no Flash/config or injected physical input."""\n'+s
s=s.replace("parser.add_argument('--get-input-report',action='store_true',help='Read one current report per expected-serial collection, not an input event stream')",'')
tail=Path(__file__).with_name('io4_protocol_body.py.txt').read_text(encoding='utf-8')
o.write_text(s+tail,encoding='utf-8')
