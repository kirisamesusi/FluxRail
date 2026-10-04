"""Verify phase10 regressions detect the two pre-fix behaviors in isolated copies.
Never edits production sources or firmware artifacts; failed mutated tests are expected.
"""
from pathlib import Path
import hashlib
import json
import re
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[2]
HERE = ROOT / "tests/integration"
BUILD = HERE / "build/module1_mutation"


def replace_once(text, before, after):
    if text.count(before) != 1:
        raise SystemExit("Mutation source drift; review before changing: " + before)
    return text.replace(before, after)


def main():
    BUILD.mkdir(parents=True, exist_ok=True)
    app = (ROOT / "firmware/FluxRail/src/app/App.cpp").read_text(encoding="utf-8")
    backend = (ROOT / "firmware/FluxRail/src/usb/UsbBackend.cpp").read_text(encoding="utf-8")
    mutations = [
        ("app", "run_app_storage.py", "app_storage_tests.cpp", "app/App.cpp", "App.cpp",
         replace_once(app, "!leds_.busy() && hid.quiescent()", "(!leds_.busy() || leds_.faulted()) && hid.quiescent()"),
         {"test_app_save_faulted_busy_led_no_flash_and_input_recovers"}),
        ("usb", "run_io4.py", "io4_tests.cpp", "usb/UsbBackend.cpp", "UsbBackend.cpp",
         replace_once(backend,
            "    f.keys[i] = config.enabled && lighting::validLightingConfig(config)\n"
            "                    ? lighting::scaleKeyColor(\n"
            "                          {value(2), value(1), value(0)}, 255,\n"
            "                          config.brightness_level)\n"
            "                    : common::Rgb{};",
            "    (void)config;\n    f.keys[i] = {value(2), value(1), value(0)};"),
         {"test_io4_host_lights_follow_user_brightness_disable_and_physical_mapping",
          "test_io4_backend_busy_control_get_motion_and_lights"}),
    ]
    results = []
    for include, runner, test, source, filename, text, expected in mutations:
        copy = BUILD / filename
        copy.write_text(text, encoding="utf-8", newline="\n")
        runner_text = (HERE / runner).read_text(encoding="utf-8")
        # Runners declare a JSON-compatible double-quoted list; do not eval Python.
        sources = json.loads(re.search(r"^sources = (.*)$", runner_text, re.M).group(1))
        command = [sys.executable, str(ROOT / "tests/module1/run_tests.py"),
            "--instruction-limit", "300000000", "--test-source", str(HERE / test),
            "--extra-include", str(HERE / "stubs"), "--extra-include", str(HERE / "stubs_app"),
            "--extra-include", str(ROOT / "firmware/FluxRail/src" / include),
            "--skip-hardware", "--extra-flag=-DARDUINO_ARCH_RP2040", "--build-dir", str(BUILD / include)]
        for name in sources:
            command += ["--extra-source", str(copy) if name == source else name]
        result = subprocess.run(command, cwd=ROOT, capture_output=True, text=True, encoding="utf-8", errors="replace")
        log = result.stdout + result.stderr
        (BUILD / (include + ".log")).write_text(log, encoding="utf-8")
        failed = re.findall(r"^FAIL: (test_\w+)", result.stdout, re.M)
        compiled = "PASS: strict ARM compilation" in result.stdout
        detected = result.returncode == 1 and compiled and set(failed) == expected
        results.append({"restored_old_behavior": source, "exit_code": result.returncode,
            "strict_compile_passed": compiled, "failed_tests": failed, "detected": detected,
            "source_sha256": hashlib.sha256(copy.read_bytes()).hexdigest()})
        if not detected:
            print(log)
            raise SystemExit("Regression did not detect exactly the expected pre-fix behavior: " + source)
        print("PASS: isolated old behavior detected: " + source)
    (BUILD / "results.json").write_text(json.dumps(results, ensure_ascii=False, indent=2), encoding="utf-8")


if __name__ == "__main__":
    main()
