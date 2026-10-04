"""严格编译真实App类型与模块3PUSH适配器，不运行App、不链接/上传硬件。"""
from pathlib import Path
import hashlib
import json
import os
import subprocess
HERE = Path(__file__).resolve().parent
ROOT = HERE.parent.parent
SRC = ROOT / "firmware/FluxRail/src"
CORE = Path(os.environ.get("LOCALAPPDATA", str(Path.home()/"AppData/Local"))) / "Arduino15/packages/rp2040/hardware/rp2040/6.1.0"
TOOLS = CORE.parents[2] / "tools/pqt-gcc/5.0.0-9576866/bin"
BUILD = HERE / "build"
LIBRARY = ROOT / "dependencies/generated/Adafruit_TinyUSB_Arduino"
def main():
    BUILD.mkdir(parents=True, exist_ok=True)
    compiler = TOOLS / "arm-none-eabi-g++.exe"
    command = [str(compiler), "-std=c++17", "-Os", "-Wall", "-Wextra", "-Werror", "-Wno-error=missing-field-initializers", "-mcpu=cortex-m0plus", "-mthumb", "-fno-exceptions", "-fno-rtti", "-DARDUINO_ARCH_RP2040", "-DF_CPU=125000000", "-DUSE_TINYUSB", "-iprefix"+CORE.as_posix()+"/"]
    command += ["@"+str(CORE/p) for p in ["lib/rp2040/platform_inc.txt", "lib/core_inc.txt", "lib/rp2040/platform_def.txt", "lib/platform_def.txt"]]
    command += ["-I"+str(p) for p in [SRC, CORE/"include", CORE/"cores/rp2040", CORE/"variants/generic", LIBRARY/"src", CORE/"libraries/Adafruit_TinyUSB_Arduino/src/arduino", CORE/"libraries/SPI/src"]]
    command += ["-c", str(HERE/"app_adapter_compile.cpp"), "-o", str(BUILD/"app_adapter_compile.o")]
    tracked=list(SRC.rglob("*.h"))+[HERE/"app_adapter_compile.cpp"]
    hashes={p.relative_to(ROOT).as_posix():hashlib.sha256(p.read_bytes()).hexdigest() for p in tracked}
    result = subprocess.run(command, cwd=ROOT, capture_output=True, encoding="utf-8", errors="replace")
    log = result.stdout + result.stderr
    project_warnings = [l for l in log.splitlines() if "warning:" in l and "Documents" in l and "dependencies" not in l]
    code = result.returncode or int(bool(project_warnings))
    (BUILD/"app_adapter_compile.log").write_text(log, encoding="utf-8")
    drift=[name for name,digest in hashes.items() if hashlib.sha256((ROOT/name).read_bytes()).hexdigest()!=digest]
    code=code or int(bool(drift))
    report={"validation":"compile only, real App::submitFilteredPush with AppPushSink<app::App>", "core":"6.1.0", "tinyusb":"3.7.7 project startup adapter", "exit_code":code, "project_warnings":project_warnings, "header_snapshot_sha256":hashes, "source_drift":drift, "hardware_tested":False, "game_tested":False}
    (BUILD/"app_adapter_results.json").write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding="utf-8")
    if code:
        print(log)
        raise SystemExit(code)
    print("PASS: strict ARM compilation of real App PUSH adapter; no runtime/USB/board test")
if __name__ == "__main__": main()
