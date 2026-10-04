"""Reuse formal ELF checks in isolated evidence; preserve historical results."""
from pathlib import Path
import hashlib, json, subprocess, sys
root=Path(__file__).resolve().parents[2]
out=root/'tests/integration/pc_board_2026-10-04/io4-timeout-candidate-0.10.2'
original=root/'tests/integration/run_usb_descriptors.py'
historical=root/'tests/integration/build/usb_descriptor_results.json'
before=hashlib.sha256(historical.read_bytes()).hexdigest()
source=original.read_text(encoding='utf-8')
old="ROOT=Path(__file__).resolve().parents[2]\nHERE=ROOT/'tests/integration'"
assert source.count(old)==1
source=source.replace(old,f'ROOT=Path({str(root)!r})\nHERE=Path({str(out)!r})')
script=out/'run_usb_descriptors.isolated.py'
assert not script.exists() and not (out/'build/usb_descriptor_results.json').exists()
script.write_text(source,encoding='utf-8')
r=subprocess.run([sys.executable,str(script)],capture_output=True,encoding='utf-8',errors='replace')
(out/'descriptor-stdout.txt').write_text(r.stdout,encoding='utf-8')
(out/'descriptor-stderr.txt').write_text(r.stderr,encoding='utf-8')
after=hashlib.sha256(historical.read_bytes()).hexdigest()
(out/'descriptor-launch.json').write_text(json.dumps({'exit_code':r.returncode,'original_runner_sha256':hashlib.sha256(original.read_bytes()).hexdigest(),'isolated_runner_sha256':hashlib.sha256(script.read_bytes()).hexdigest(),'changes':'only ROOT/HERE evidence routing','historical_before':before,'historical_after':after,'historical_unchanged':before==after,'hardware_tested':False,'game_tested':False},indent=2),encoding='utf-8')
print(r.stdout,r.stderr)
assert r.returncode==0 and before==after
