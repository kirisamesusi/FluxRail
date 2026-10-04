from pathlib import Path
p=Path(__file__).with_name('switch_usb_mode_once_v2.ps1');o=p.with_name('switch_usb_mode_once_v3.ps1')
assert not o.exists()
s=p.read_text(encoding='utf-8').replace(' -Raw|ConvertFrom-Json',' -Encoding UTF8 -Raw|ConvertFrom-Json')
o.write_text(s,encoding='utf-8')
