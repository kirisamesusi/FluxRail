"""核对验证源码一致性并保存长期模块3报告；不运行或上传固件。"""
from datetime import datetime,timedelta,timezone
from pathlib import Path
import hashlib
import json
import subprocess
HERE=Path(__file__).resolve().parent
ROOT=HERE.parent.parent
BUILD=HERE/"build"
def digest(path):return hashlib.sha256(path.read_bytes()).hexdigest()
def verify(snapshot):
    for name, expected in snapshot.items():
        if digest(ROOT/name)!=expected:raise SystemExit("Validation source changed: "+name)
def main():
    sim=json.loads((BUILD/"simulation_results.json").read_text(encoding="utf-8"))
    arduino=json.loads((BUILD/"arduino_results.json").read_text(encoding="utf-8"))
    app=json.loads((BUILD/"app_adapter_results.json").read_text(encoding="utf-8"))
    if not sim["results"] or not all(x["completed"] and x["passed"] for x in sim["results"]):raise SystemExit("Simulation incomplete or failed")
    if sim["source_drift"] or arduino["exit_code"] or arduino["source_drift"] or arduino["copied_source_mismatch"] or app["exit_code"] or app["source_drift"]:raise SystemExit("Build/source audit failed")
    verify(sim["source_sha256"]);verify(arduino["source_sha256"]);verify(app["header_snapshot_sha256"])
    uf2=BUILD/"arduino/Module3Compile.ino.uf2"
    artifacts={p.relative_to(ROOT).as_posix():digest(p) for p in [uf2,BUILD/"module3_tests.elf",BUILD/"simulation_results.json",BUILD/"arduino_results.json",BUILD/"app_adapter_results.json"]}
    paths=list((ROOT/"firmware/FluxRail/src/knobs").glob("*"))+list(HERE.glob("*"))
    sources={p.relative_to(ROOT).as_posix():digest(p) for p in paths if p.is_file() and p.suffix in [".h",".cpp",".py",".md"]}
    cli=Path("C:/Program Files/Arduino IDE/resources/app/lib/backend/resources/arduino-cli.exe")
    cli_version=subprocess.check_output([str(cli),"version"],text=True).strip() if cli.exists() else "unavailable"
    report={"date_local":datetime.now(timezone(timedelta(hours=8))).isoformat(),"module":"3 MT6701 knobs and PUSH", "validation":"strict ARM compilation, original C++ ARM simulation, isolated Arduino link, real App PUSH adapter compile only", "cli":cli_version,"simulation":sim,"arduino":arduino,"app_push_adapter":app,"total_checks":sum(x["checks"] for x in sim["results"]),"artifact_sha256":artifacts,"source_and_test_sha256":sources,"uploaded":False,"formal_app_integrated":False,"hardware_tested":False,"usb_host_tested":False,"game_tested":False,"mechanical_thresholds_frozen":False,"public_usb_storage_protocol_modified":False,"review":"two implementation reviews and final review; see docs/progress/模块3.md"}
    (HERE/"validation_2026-10-03.json").write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding="utf-8")
    print("PASS: validated source snapshots, "+str(len(sim["results"]))+" ARM groups, "+str(report["total_checks"])+" checks; no board/game test")
if __name__=="__main__":main()
