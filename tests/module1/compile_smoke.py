"""生成独立的编译烟测sketch并链接模块1，不上传，不作为FluxRail应用。
复制模块1及其公共类型/板级常量；不包含正式App或USB适配。
"""
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
SMOKE_SOURCE = r'''// 仅为模块1的Arduino编译/链接烟测入口，不是FluxRail应用，不上传。
#include <Adafruit_TinyUSB.h>
#include "src/keyboard/KeyScanner.h"
#include "src/keyboard/KeyScannerRp2040.h"
#include "src/keyboard/ActionMapper.h"
#include "src/lighting/KeyLighting.h"
#include "src/lighting/Ws2812OutputRp2040.h"
using namespace fluxrail::keyboard;
using namespace fluxrail::lighting;
struct CompileSink : ActionSink {
    bool tryKeyboard(const KeyboardState&) override {return false;}
    bool tryMouseButtons(uint8_t) override {return false;}
};
struct CompileEdges : EdgeSink {
    bool tryEdge(uint8_t,bool,uint32_t,uint32_t) override {return false;}
};
KeyScanner scanner;
ActionMapper mapper;
Ws2812OutputRp2040 lights;
CompileSink sink;
CompileEdges edges;
void setup() {
    beginKeyPinsRp2040();
    scanner.reset({},readKeyDownMaskRp2040(),micros(),1);
    mapper.reset(defaultKeyboardConfig(),1,0);
    lights.begin({});
}
void loop() {
    const auto now=micros();
    scanner.sample(readKeyDownMaskRp2040(),now,edges);
    mapper.service(now,sink);
    const auto frame=renderLights({},scanner.physicalMask(),millis(),true,{255,255,255});
    lights.trySubmit(frame,now);
}
'''


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--cli", type=Path, default=CLI_DEFAULT)
    args = parser.parse_args()
    sketch = BUILD / "Module1Compile"
    sketch.mkdir(parents=True, exist_ok=True)
    (sketch / "Module1Compile.ino").write_text(SMOKE_SOURCE, encoding="utf-8")
    hashes = {}
    for module in ["keyboard", "lighting", "common", "board"]:
        # 明确只复制当前模块头文件/源码，避免把构建产物或其他业务带入sketch。
        target = sketch / "src" / module
        target.mkdir(parents=True, exist_ok=True)
        for source in (SRC / module).iterdir():
            if source.suffix in [".cpp", ".h"]:
                shutil.copy2(source, target / source.name)
                hashes[source.relative_to(ROOT).as_posix()] = hashlib.sha256(source.read_bytes()).hexdigest()
    command = [str(args.cli), "compile", "--fqbn", FQBN, "--build-path", str(BUILD / "arduino"), "--build-cache-path", str(BUILD / "arduino-cache"), "--warnings", "all", str(sketch)]
    result = subprocess.run(command, cwd=ROOT, capture_output=True, encoding="utf-8", errors="replace")
    (BUILD / "arduino_compile.log").write_text(result.stdout + "\n" + result.stderr, encoding="utf-8")
    report = {"date_local": datetime.now(timezone(timedelta(hours=8))).date().isoformat(), "validation": "isolated Arduino module1 compile and link smoke test", "fqbn": FQBN, "core": "Arduino-Pico 6.1.0", "tinyusb": "3.7.7", "exit_code": result.returncode, "source_sha256": hashes, "uploaded": False, "production_application": False, "log": "arduino_compile.log"}
    (BUILD / "arduino_results.json").write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding="utf-8")
    print(result.stdout)
    print("Full diagnostics: " + str(BUILD / "arduino_compile.log"))
    if result.returncode:
        print(result.stderr)
        raise SystemExit(result.returncode)


if __name__ == "__main__":
    main()
