"""保存可交接验证快照，核对实际构建使用的源哈希；不运行/上传固件。"""
from datetime import datetime, timedelta, timezone
from pathlib import Path
import hashlib
import json
import re

HERE = Path(__file__).resolve().parent
ROOT = HERE.parent.parent
BUILD = HERE / "build"


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    simulation = json.loads((BUILD / "simulation_results.json").read_text(encoding="utf-8"))
    arduino = json.loads((BUILD / "arduino_results.json").read_text(encoding="utf-8"))
    if arduino["exit_code"] or not simulation["results"] or not all(row["passed"] and row["completed"] for row in simulation["results"]):
        raise SystemExit("Cannot snapshot incomplete/failed validation")
    for relative, expected in arduino["source_sha256"].items():
        if digest(ROOT / relative) != expected:
            raise SystemExit("Build stale: " + relative)
    log = (BUILD / "arduino_compile.log").read_text(encoding="utf-8")
    warning_lines = [line for line in log.splitlines() if "warning:" in line]
    project_warnings = [line for line in warning_lines if "/hall/" in line.replace("\\", "/") or "Module2Compile.ino" in line]
    if project_warnings:
        raise SystemExit("Project warnings must be reviewed before final snapshot")
    test_files = [path for path in HERE.iterdir() if path.suffix in [".py", ".cpp", ".md"]]
    uf2 = BUILD / "arduino/Module2Compile.ino.uf2"
    report = {
        "date_local": datetime.now(timezone(timedelta(hours=8))).isoformat(),
        "module": "FluxRail module2 three Hall absolute rail",
        "source_scope": ["firmware/FluxRail/src/hall", "tests/module2", "docs/progress/模块2.md"],
        "strict_arm_translation_units": 6,
        "simulation": simulation,
        "checks": sum(row["checks"] for row in simulation["results"]),
        "arduino_smoke": arduino,
        "flash_bytes": int(re.search(r"Sketch uses (\d+) bytes", log).group(1)),
        "global_ram_bytes": int(re.search(r"Global variables use (\d+) bytes", log).group(1)),
        "project_warnings": project_warnings,
        "third_party_warnings": warning_lines,
        "test_source_sha256": {path.relative_to(ROOT).as_posix(): digest(path) for path in sorted(test_files)},
        "smoke_uf2_sha256": digest(uf2),
        "production_app_connected": False,
        "hardware_tested": False,
        "host_tested": False,
        "game_tested": False,
        "uploaded": False,
        "not_verified": ["actual mechanical endpoints/field curves", "ADC peripheral timing/noise", "worst algorithm service time",
                         "dual-core concurrency", "ordinary absolute X and independent Y", "IO4/game compatibility", "global flash persistence"]
    }
    target = HERE / "validation_2026-10-03.json"
    target.write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding="utf-8")
    print("PASS: source hashes match Arduino build; snapshot " + str(target))


if __name__ == "__main__":
    main()
