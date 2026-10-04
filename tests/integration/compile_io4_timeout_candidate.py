"""Compile isolated 4MiB contract candidate; no upload, no frozen artifact overwrite."""
import hashlib, json, sys
from pathlib import Path
from datetime import datetime, timezone
import compile_firmware as formal

output = Path(__file__).resolve().parent / 'pc_board_2026-10-04' / 'io4-timeout-candidate-0.10.2'
assert not output.exists(), 'Candidate evidence already exists'
output.mkdir()
frozen = [formal.HERE/'build/arduino/FluxRail.ino.uf2', formal.HERE/'build/firmware_results.json',formal.HERE/'build/source_link_audit.json']
before = {str(p):hashlib.sha256(p.read_bytes()).hexdigest() for p in frozen}
formal.HERE=output
formal.BUILD=output/'build/arduino'
parser_url=(formal.ROOT/'web/src/update/uf2.js').as_uri()
header_path=str(formal.ROOT/'firmware/FluxRail/src/common/FirmwareInfo.h')
uf2_path=str(formal.BUILD/'FluxRail.ino.uf2')
(output/'check_firmware_version.mjs').write_text(
    "import {readFile} from 'node:fs/promises';\nimport assert from 'node:assert/strict';\n"
    +f"import {{validateUf2}} from {json.dumps(parser_url)};\n"
    +f"const header=await readFile({json.dumps(header_path)},'utf8');\n"
    +"const expected=/^#define FLUXRAIL_FIRMWARE_VERSION \"([^\"]+)\"/m.exec(header)?.[1];\n"
    +f"const checked=validateUf2(new Uint8Array(await readFile({json.dumps(uf2_path)})));\n"
    +"assert.equal(checked.firmware.version,expected);assert.equal(expected,'0.10.2');console.log(JSON.stringify({...checked,verified:true}));\n",encoding='utf-8')
(output/'compile_io4_timeout_candidate.py').write_bytes(Path(__file__).read_bytes())
(output/'compile_firmware.source.py').write_bytes((formal.ROOT/'tests/integration/compile_firmware.py').read_bytes())
result={'time_utc':datetime.now(timezone.utc).isoformat(),'candidate_version':'0.10.2','target_contract':'existing project 4MiB; actual installed board capacity remains unverified','uploaded':False,'hardware_tested':False,'game_tested':False,'frozen_before_sha256':before}
try:
    formal.main()
    uf2=formal.BUILD/'FluxRail.ino.uf2'
    result.update(compile_passed=True,uf2_bytes=uf2.stat().st_size,uf2_sha256=hashlib.sha256(uf2.read_bytes()).hexdigest())
finally:
    after={str(p):hashlib.sha256(p.read_bytes()).hexdigest() for p in frozen}
    result.update(frozen_after_sha256=after,frozen_evidence_unchanged=before==after)
    (output/'candidate-summary.json').write_text(json.dumps(result,ensure_ascii=False,indent=2),encoding='utf-8')
    assert before==after,'Frozen original artifacts changed'
