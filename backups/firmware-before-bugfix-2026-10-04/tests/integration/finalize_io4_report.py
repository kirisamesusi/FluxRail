"""Freeze current software evidence; fails on incomplete tests or stale firmware source hashes."""
from datetime import datetime, timedelta, timezone
from pathlib import Path
import hashlib
import json
import subprocess

ROOT = Path(__file__).resolve().parents[2]
HERE = ROOT / "tests/integration"
def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()
def load(path):
    return json.loads(path.read_text(encoding="utf-8"))

firmware = load(HERE / "build/firmware_results.json")
audit = load(HERE / "build/source_link_audit.json")
if firmware["exit_code"] or not audit["passed"] or not audit["storage_partition_reserved"] or not audit["stack_frame_budget_passed"]:
    raise SystemExit("Firmware compile/link/partition audit failed")
for name, digest in firmware["source_sha256"].items():
    if sha(ROOT / name) != digest:
        raise SystemExit("Firmware source changed since build: " + name)
suites = {}
for name, relative in {
    "module1": "tests/module1/build/simulation_results.json",
    "integration": "tests/integration/build/simulation_results.json",
    "transport": "tests/integration/build/transport/simulation_results.json",
    "protocol": "tests/integration/build/protocol/simulation_results.json",
    "storage": "tests/integration/build/storage/simulation_results.json",
    "io4": "tests/integration/build/io4/simulation_results.json",
    "app_storage": "tests/integration/build/app_storage/simulation_results.json",
}.items():
    suites[name] = load(ROOT / relative)
    for file,digest in suites[name]["source_sha256"].items():
        if sha(ROOT / file)!=digest:raise SystemExit("Stale simulation source: "+name+" "+file)
    if not all(row["passed"] and row["completed"] for row in suites[name]["results"]):
        raise SystemExit("Incomplete or failed simulation: " + name)
compiler = Path(next(x["arguments"][0] for x in load(HERE / "build/arduino/compile_commands.json"))).with_suffix(".exe")
symbols = subprocess.check_output([str(compiler.parent / "arm-none-eabi-nm.exe"), "--defined-only", str(HERE / "build/arduino/FluxRail.ino.elf")], text=True)
flash_routines = {line.split()[-1]: int(line.split()[0], 16) for line in symbols.splitlines() if line.split()[-1] in {"flash_range_erase", "flash_range_program"}}
if len(flash_routines) != 2 or not all(0x20000000 <= value < 0x20042000 for value in flash_routines.values()):
    raise SystemExit("Flash erase/program routines are not linked into SRAM")
descriptor = load(HERE / "build/usb_descriptor_results.json")
if descriptor["elf_sha256"] != sha(HERE / "build/arduino/FluxRail.ino.elf") or len(descriptor["results"]) != 2 or not all(x["passed"] for x in descriptor["results"]):
    raise SystemExit("Stale or incomplete formal ELF descriptor validation")
uf2 = HERE / "build/arduino/FluxRail.ino.uf2"
log = (HERE / "build/firmware_compile.log").read_text(encoding="utf-8")
scripts = ["tests/module1/run_tests.py"] + [p.relative_to(ROOT).as_posix() for p in HERE.glob("*.py")]
tests = [p.relative_to(ROOT).as_posix() for p in HERE.glob("*_tests.cpp")]
boundaries = [p.relative_to(ROOT).as_posix() for folder in [HERE / "stubs", HERE / "stubs_app"] for p in folder.rglob("*.h")]
samples = ["tests/integration/cdc_examples.jsonl", "tests/integration/storage_examples.jsonl", "tests/integration/schema1_defaults.bin", "tests/integration/schema2_defaults.bin", "tests/integration/schema2_defaults_phase9.bin", "tests/integration/io4_examples.jsonl"]
previous = load(HERE / "validation_io4_2026-10-03.json")
old_default = HERE / "schema2_defaults_phase9.bin"
new_default = HERE / "schema2_defaults.bin"
if sha(old_default) != previous["test_script_sample_sha256"]["tests/integration/schema2_defaults.bin"]:
    raise SystemExit("Phase9 factory golden changed")
if sha(HERE / "schema1_defaults.bin") != previous["test_script_sample_sha256"]["tests/integration/schema1_defaults.bin"]:
    raise SystemExit("Frozen schema1 golden changed")
old_bytes, new_bytes = old_default.read_bytes(), new_default.read_bytes()
color_offsets = [157, 498, 839, 1180]
colors = [(0, 255, 255), (255, 96, 0), (128, 0, 255), (0, 255, 0)]
allowed = {offset + i for offset in color_offsets for i in range(3)}
if len(old_bytes) != 1387 or len(new_bytes) != 1387 or any(a != b and i not in allowed for i, (a, b) in enumerate(zip(old_bytes, new_bytes))):
    raise SystemExit("Unexpected schema2 factory golden change outside indicator RGB")
if any(new_bytes[offset:offset + 3] != bytes(color) for offset, color in zip(color_offsets, colors)):
    raise SystemExit("Factory indicator colors do not match documented defaults")
regression = load(HERE / "build/module1_mutation/results.json")
if len(regression) != 2 or not all(x["detected"] and x["strict_compile_passed"] for x in regression):
    raise SystemExit("Phase10 regressions did not detect old behavior")
report = {
    "date_local": datetime.now(timezone(timedelta(hours=8))).isoformat(),
    "phase": 10,
    "scope": "module1 completion: keyboard/all supported usages, unified lighting, indicator, safe LED maintenance plus existing public integration",
    "validation": {"suites": suites,
        "regression_detection": regression,
        "groups": sum(len(x["results"]) for x in suites.values()),
        "checks": sum(row["checks"] for x in suites.values() for row in x["results"]),
        "firmware": firmware, "formal_elf_descriptors": descriptor, "stack_usage": audit["stack_usage"],
        "compile_summary": [line for line in log.splitlines() if line.startswith(("Sketch uses", "Global variables use"))],
        "strict_project_units": len(audit["project_translation_units"]),
        "stack_frame_budget_passed": audit["stack_frame_budget_passed"], "flash_sram_symbols": flash_routines,
        "project_warnings": sum(len(x["project_warnings"]) for x in audit["project_translation_units"]),
        "uploaded": False, "hardware_tested": False, "game_tested": False},
    "artifact": {"uf2": uf2.relative_to(ROOT).as_posix(), "bytes": uf2.stat().st_size,
        "sha256": sha(uf2), "uploaded": False},
    "test_script_sample_sha256": {name: sha(ROOT / name) for name in scripts + tests + samples + boundaries},
    "contract_progress_sha256": {name: sha(ROOT / name) for name in [
        "docs/spec/01_公共接口与集成约定.md", "docs/spec/12_完整配置与Flash序列化.md",
        "docs/spec/13_IO4软件契约.md", "docs/progress/IO4.md", "docs/progress/模块1.md",
        "firmware/FluxRail/README.md", "firmware/FluxRail/src/protocol/CDC_PROTOCOL.md",
        "tests/integration/README.md", "tests/integration/HARDWARE_CHECKLIST.md", "dependencies/README.md",
        "docs/spec/02_模块1_键盘与灯光.md", "firmware/FluxRail/src/lighting/README.md",
        "tests/module1/REVIEW_2026-10-03.md"]},
    "storage": {"schema": 2, "profiles": 12, "presets": 4,
        "slot_offsets": [0x3FB000, 0x3FD000], "slot_bytes": 8192,
        "erase_sector_bytes": 4096, "explicit_save": True,
        "calibration_object_memory_written": False,
        "default_payload_bytes": 1387, "default_payload_crc32": 1053904252},
    "review": ["immutable full RAM candidate published after module1 two-core ACK",
        "all IO4 input/output sizes and IDs rejected safely outside exact normalized forms",
        "single HID backend, descriptor callbacks retain audited TinyUSB ownership",
        "saved mode selected before default CDC registration and first port connection",
        "long product/interface strings validated by executing formal ELF callbacks",
        "schema1 migration retains all profiles, IDs, settings and global calibration",
        "reboot requires explicit saved complete snapshot and actual CDC bus completion",
        "deferred mutex cleanup and atomic detach prevent lost release/reset on busy USB",
        "previews never erase/program Flash", "independent sectors, inactive slot only",
        "commit page programmed last after byte readback; final CRC/schema/semantic verification",
        "maintenance release USB completion before Flash, pause ACK, resume ACK after rebuild",
        "CDC disconnect does not cancel accepted apply/save", "whole import revision isolated",
        "original hall/knobs business sources not modified; only their values/validators referenced",
        "actual App/NormalHid simulated with explicit hardware boundary replacements",
        "every supported keyboard usage exercised with twelve overlapping physical sources",
        "IO4 host lights obey applied enabled/brightness via shared scaler and final cap",
        "indicator uses confirmed template/current persona, pending requests do not change it",
        "LED faulted plus busy cannot bypass Flash maintenance gate; timeout recovers input",
        "new preset colors only change factory/restored defaults; saved/imported/migrated colors preserved"],
    "remaining": ["physical Flash/power cut and real dual-core lockout not tested",
        "USB host, Windows Web Serial, RGB/GRB and games not tested",
        "HallModule/KnobModule scheduling, calibration jobs/diagnostics/motion routing not connected",
        "normal absolute X and Hall/Knob physical scheduling not connected; IO4 game compatibility unverified",
        "mouse duplicate strategy/default lights/mode colors remain engineering starting values",
        "module4 capture UI and update flow, module2/3 motion/PUSH physical integration deferred"],
    "io4": {"software_implemented": True, "game_compatible_verified": False,
        "boot_modes": ["NORMAL_NKRO", "IO4"], "io4_configuration_cdc": False,
        "input_payload_bytes": 63, "output_payload_bytes": 63,
        "pure_hid_recovery_source": 9, "detach_interval_us": 250000,
        "module2_module3_scheduling": False, "normal_absolute_x": False,
        "schema1_readonly_migration": True},
    "previous_snapshot": "validation_io4_2026-10-03.json",
}
output = HERE / "validation_module1_2026-10-03.json"
output.write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding="utf-8")
print(f"PASS: {report['validation']['groups']} groups / {report['validation']['checks']:,} checks; current firmware/source hashes frozen; not uploaded")
print("UF2 SHA256=" + report["artifact"]["sha256"])
