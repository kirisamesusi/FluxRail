"""独立模块2 Arduino编译/完整链接烟测，不上传、不接正式App。"""
from datetime import datetime, timedelta, timezone
from pathlib import Path
import argparse
import hashlib
import json
import os
import shutil
import subprocess

HERE = Path(__file__).resolve().parent
ROOT = HERE.parent.parent
SRC = ROOT / "firmware/FluxRail/src"
BUILD = HERE / "build"
FQBN = "rp2040:rp2040:generic:flash=4194304_0,boot2=boot2_w25q64jv_4_padded_checksum,freq=125,usbstack=tinyusb"
CLI_DEFAULT = Path(os.environ.get("ProgramFiles", "C:/Program Files")) / "Arduino IDE/resources/app/lib/backend/resources/arduino-cli.exe"
SMOKE_SOURCE = r'''// 仅隔离编译烟测；不上传，不作为FluxRail正式主入口。
#include <Adafruit_TinyUSB.h>
#include "src/hall/HallAdcRp2040.h"
#include "src/hall/HallModule.h"
using namespace fluxrail::hall;
HallAdcRp2040 adc;
HallSampler sampler;
HallModule hall;
volatile uint32_t compile_command=0; // 保留命令方法用于链接审查，不从外部协议读取。
void setup() {
  adc.begin();
  hall.apply({},1);
  Calibration saved{};
  hall.load(saved); // 无有效校准，不预置虚构机械曲线。
  hall.resume(1);
}
void loop() {
  const uint32_t now=adc.nowUs();
  Sample s;
  if(sampler.poll(now,adc,s))hall.sample(s);
  hall.tick(now);
  Estimate result;
  hall.tryTake(result); // 只消费测试值，没有HID/Flash/Serial动作。
  if(compile_command==1) {hall.pause();hall.discardPaused();hall.startCalibration(1,now,true,-41,39);}
  if(compile_command==2)hall.captureReference(0,now);
  if(compile_command==3)hall.captureNode(-41,now);
  if(compile_command==4)hall.beginSweep(now);
  if(compile_command==5)hall.finishSweep(now);
  if(compile_command==6)hall.cancelCalibration();
}
'''


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--cli", type=Path, default=CLI_DEFAULT)
    args = parser.parse_args()
    sketch = BUILD / "Module2Compile"
    sketch.mkdir(parents=True, exist_ok=True)
    (sketch / "Module2Compile.ino").write_text(SMOKE_SOURCE, encoding="utf-8")
    sources = list((SRC / "hall").glob("*.h")) + list((SRC / "hall").glob("*.cpp"))
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
    (BUILD / "arduino_compile.log").write_text(log, encoding="utf-8")
    report = {"date_local": datetime.now(timezone(timedelta(hours=8))).date().isoformat(),
              "validation": "isolated Arduino module2 compile and link smoke test", "fqbn": FQBN,
              "core": "Arduino-Pico 6.1.0", "tinyusb": "3.7.7", "exit_code": result.returncode,
              "source_sha256": hashes, "uploaded": False, "production_application": False,
              "hardware_tested": False, "game_tested": False, "log": "arduino_compile.log"}
    (BUILD / "arduino_results.json").write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding="utf-8")
    print(result.stdout)
    print("Full diagnostics: " + str(BUILD / "arduino_compile.log"))
    if result.returncode:
        print(result.stderr)
        raise SystemExit(result.returncode)


if __name__ == "__main__":
    main()
