from pathlib import Path
p=Path(__file__).with_name('switch_usb_mode_once.ps1');o=p.with_name('switch_usb_mode_once_v2.ps1')
assert not o.exists()
s=p.read_text(encoding='utf-8')
s=s.replace(" if(Test-Path -LiteralPath \"$PSScriptRoot/pc_board_2026-10-04/short-incident.json\"){throw 'Short incident recorded: hardware inspection and fresh recovery validation required before mode switching. Do not reuse preincident gate.'}", " # Incident evidence is retained. Continued tests are authorized; fresh post-incident functional recovery, not electrical clearance, gates entry.")
s=s.replace('/gp9-normal.actual.json','/gp9-post-short.actual.json')
s=s.replace("if(!$gate.passed){throw 'Physical GP9 recovery preflight must pass first'}", "if(!$gate.passed -or $gate.serial -ne '5054316560DD729C' -or !$gate.forced_recovery -or $gate.revision -ne $gate.saved_revision){throw 'Fresh post-incident GP9 recovery must pass first'}")
needle=" if($Target -eq 'NORMAL_NKRO' -and !$m.result.forced_recovery){throw 'Return must be through physical recovery'}"
s=s.replace(needle," if($Target -eq 'IO4' -and (!$m.result.forced_recovery -or $h.revision -ne $gate.revision)){throw 'Recovery preflight state changed; reread/review required'}\n"+needle)
o.write_text(s,encoding='utf-8')
