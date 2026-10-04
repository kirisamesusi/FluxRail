"""独立模块3 Arduino编译/完整链接烟测，不上传、不接正式App。"""
from datetime import datetime, timedelta, timezone
from pathlib import Path
import argparse
import hashlib
import json
import os
import re
import shutil
import subprocess

HERE = Path(__file__).resolve().parent
ROOT = HERE.parent.parent
SRC = ROOT / "firmware/FluxRail/src"
BUILD = HERE / "build"
FQBN = "rp2040:rp2040:generic:flash=4194304_0,boot2=boot2_w25q64jv_4_padded_checksum,freq=125,usbstack=tinyusb"
CLI_DEFAULT = Path(os.environ.get("ProgramFiles", "C:/Program Files")) / "Arduino IDE/resources/app/lib/backend/resources/arduino-cli.exe"
SMOKE_SOURCE = r'''// 隔离链接烟测，只验证模块可构建，不上传/不作为正式App入口。
#include <Adafruit_TinyUSB.h>
#include "src/knobs/Mt6701SsiRp2040.h"
#include "src/knobs/KnobModule.h"
using namespace fluxrail::knobs;
Mt6701SsiRp2040 reader;
KnobSampler sampler;
KnobModule knobs;
struct Sink final : PushSink {
  bool submitFilteredPush(uint8_t,bool,uint32_t) override { return true; }
} sink; // 不连接USB输出；真实集成必须转调App的公共入口。
volatile uint32_t compile_command=0;
void setup() {
  reader.begin();
  Settings s[2]={defaults(0),defaults(1)};
  knobs.apply(s,1);
  Discarded dropped[2]{};
  knobs.resume(1,dropped);
}
void loop() {
  Sample s; Motion m;
  for(uint8_t i=0;i<2;++i) {
    if(sampler.poll(i,reader.nowUs(),reader,s)) knobs.sample(i,s,sink);
    knobs.tryTake(i,m);
  }
  if(compile_command==1) {
    knobs.pause(sink,reader.nowUs());
    Discarded dropped[2]={knobs.discardPaused(0),knobs.discardPaused(1)};
    Settings settings[2]={defaults(0),defaults(1)};
    knobs.apply(settings,2);
    knobs.resume(2,dropped);
    sampler.reset();
  }
}
'''


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--cli", type=Path, default=CLI_DEFAULT)
    args = parser.parse_args()
    sketch = BUILD / "Module3Compile"
    sketch.mkdir(parents=True, exist_ok=True)
    (sketch / "Module3Compile.ino").write_text(SMOKE_SOURCE, encoding="utf-8")
    sources = list((SRC / "knobs").glob("*.h")) + list((SRC / "knobs").glob("*.cpp"))
    sources += [SRC / name for name in ["board/BoardConfig.h", "common/InputTypes.h", "realtime/SpscQueue.h"]]
    hashes = {}
    for source in sources:
        target = sketch / "src" / source.relative_to(SRC)
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(source, target)
        hashes[source.relative_to(ROOT).as_posix()] = hashlib.sha256(source.read_bytes()).hexdigest()
    command = [str(args.cli), "compile", "--fqbn", FQBN, "--build-path", str(BUILD / "arduino"),
               "--build-cache-path", str(BUILD / "arduino-cache"), "--warnings", "all", str(sketch)]
    result = subprocess.run(command, cwd=ROOT, capture_output=True, encoding="utf-8", errors="replace")
    log = result.stdout + "\n" + result.stderr
    drift = [name for name, digest in hashes.items() if hashlib.sha256((ROOT/name).read_bytes()).hexdigest()!=digest]
    mismatch = []
    for name, digest in hashes.items():
        relative = (ROOT/name).relative_to(SRC)
        copied = BUILD/"arduino/sketch/src"/relative
        if not copied.exists():
            mismatch.append(name)
            continue
        data = copied.read_bytes()
        if data.startswith(b'#line 1 "'):
            data = data.split(b"\n",1)[1]
        if hashlib.sha256(data).hexdigest()!=digest:
            mismatch.append(name)
    project_warnings = [l for l in log.splitlines() if "warning:" in l and "Documents" in l and "dependencies" not in l]
    flash=re.search(r"Sketch uses (\d+) bytes",result.stdout)
    ram=re.search(r"Global variables use (\d+) bytes",result.stdout)
    code=result.returncode or int(bool(drift or mismatch or project_warnings))
    (BUILD / "arduino_compile.log").write_text(log, encoding="utf-8")
    report = {"date_local": datetime.now(timezone(timedelta(hours=8))).date().isoformat(),
              "validation": "isolated Arduino module3 compile and link smoke test", "fqbn": FQBN,
              "core": "Arduino-Pico 6.1.0", "tinyusb": "3.7.7", "exit_code": code, "source_drift":drift, "copied_source_mismatch":mismatch, "project_warnings":project_warnings,
              "flash_bytes":int(flash.group(1)) if flash else None, "global_ram_bytes":int(ram.group(1)) if ram else None,
              "source_sha256": hashes, "uploaded": False, "production_application": False,
              "hardware_tested": False, "game_tested": False, "log": "arduino_compile.log"}
    (BUILD / "arduino_results.json").write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding="utf-8")
    print(result.stdout)
    print("Full diagnostics: " + str(BUILD / "arduino_compile.log"))
    if code:
        print(result.stderr)
        print("Source/audit errors:",drift,mismatch,project_warnings)
        raise SystemExit(code)


if __name__ == "__main__":
    main()
