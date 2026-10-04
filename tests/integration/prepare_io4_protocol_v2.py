from pathlib import Path
p=Path(__file__).with_name('test_io4_protocol_actual.py');o=p.with_name('test_io4_protocol_actual_v2.py')
assert not o.exists()
s=p.read_text(encoding='utf-8')
s=s.replace("args=parser.parse_args()", "parser.add_argument('--preflight-only',action='store_true')\nargs=parser.parse_args()")
s=s.replace('import time\n','import time,os\n')
s=s.replace("pending_keepalive.append((buf,ov));ret=True", """ret=True
                    report['error']=method+' cancellation did not complete'
                    report['process_containment_exit']=True
                    report['handles_closed']=False
                    report['finished']=datetime.now(timezone.utc).isoformat();put()
                    # No Python cleanup may free pending buffer/OVERLAPPED/event. OS process rundown cancels outstanding I/O.
                    os._exit(2)""")
s=s.replace("ctrl=kernel.CreateFileW(r['path'],0,3,None,3,0,None)", "ctrl=kernel.CreateFileW(r['path'],0xC0000000,3,None,3,0,None)")
s=s.replace("    for i in range(8):", "    if args.preflight_only:\n        report['preflight_only']=True;report['write_capable_handles_identity_verified']=True\n    for i in range(0 if args.preflight_only else 8):")
start=s.index('    output(1,4);')
end=s.index('\nexcept BaseException',start)
block=s[start:end]
s=s[:start]+'    if not args.preflight_only:\n'+'\n'.join('    '+x for x in block.split('\n'))+s[end:]
s=s.replace("report.get('idle_restored',False)","(report.get('preflight_only',False) or report.get('idle_restored',False))")
o.write_text(s,encoding='utf-8')
