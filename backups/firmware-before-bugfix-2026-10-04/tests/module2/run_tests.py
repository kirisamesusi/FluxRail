"""编译模块2原始C++并以ARM指令执行测试；不模拟USB主机或RP2040外设。
执行器沿用tests/module1/run_tests.py的ELF/Unicorn方式，独立输出到module2/build。
工具链与Unicorn均固定版本。--compile-only只做全部6个翻译单元编译。
"""
from pathlib import Path
import argparse
import json
import os
import struct
import subprocess
import sys

HERE = Path(__file__).resolve().parent
ROOT = HERE.parent.parent
SRC = ROOT / "firmware/FluxRail/src"
BUILD = HERE / "build"
CORE_DEFAULT = Path(os.environ.get("LOCALAPPDATA", str(Path.home() / "AppData/Local"))) / "Arduino15/packages/rp2040/hardware/rp2040/6.1.0"
TOOLS_DEFAULT = CORE_DEFAULT.parents[2] / "tools/pqt-gcc/5.0.0-9576866/bin"
PURE = ["hall/" + name + ".cpp" for name in ["HallTypes", "HallPosition", "HallCalibration", "HallModule", "HallSampler"]]
HARDWARE = ["hall/HallAdcRp2040.cpp"]


def run(args):
    subprocess.run([str(a) for a in args], cwd=ROOT, check=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--core", type=Path, default=CORE_DEFAULT)
    parser.add_argument("--tools", type=Path, default=TOOLS_DEFAULT)
    parser.add_argument("--compile-only", action="store_true")
    parser.add_argument("--instruction-limit", type=int, default=200_000_000, help="Per-test ARM instruction budget")
    parser.add_argument("--test-source", type=Path, default=HERE / "module2_tests.cpp")
    parser.add_argument("--extra-source", action="append", default=[])
    parser.add_argument("--extra-include", type=Path, action="append", default=[])
    parser.add_argument("--skip-hardware", action="store_true", help="test-specific mock headers must not shadow real SDK")
    parser.add_argument("--build-dir", type=Path, default=BUILD)
    args = parser.parse_args()
    build = args.build_dir.resolve()
    pure = PURE + args.extra_source
    build.mkdir(parents=True, exist_ok=True)
    compiler = args.tools / "arm-none-eabi-g++.exe"
    base = [compiler, "-std=c++17", "-Wall", "-Wextra", "-Werror", "-mcpu=cortex-m0plus", "-mthumb", "-fno-exceptions", "-fno-rtti", "-ffunction-sections", "-fdata-sections", "-I" + str(SRC), "-Os"]
    base += ["-I" + str(p.resolve()) for p in args.extra_include]
    hardware = [] if args.skip_hardware else HARDWARE
    objects = []
    for name in pure:
        obj = build / (Path(name).stem + ".o")
        run(base + ["-c", SRC / name, "-o", obj])
        objects.append(obj)
    sdk = ["-DARDUINO_ARCH_RP2040", "-DF_CPU=125000000", "-iprefix" + args.core.as_posix() + "/"]
    sdk += ["@" + str(args.core / rel) for rel in ["lib/rp2040/platform_inc.txt", "lib/core_inc.txt", "lib/rp2040/platform_def.txt", "lib/platform_def.txt"]]
    sdk += ["-I" + str(args.core / "include"), "-I" + str(args.core / "cores/rp2040")]
    for name in hardware:
        run(base + sdk + ["-c", SRC / name, "-o", build / (Path(name).stem + ".o")])
    print(f"PASS: strict ARM compilation, {len(pure) + len(hardware)} translation units", flush=True)
    if args.compile_only:
        return
    sys.path.insert(0, str(HERE.parent / "module1/build/python-deps"))
    sys.path.insert(0, str(BUILD / "python-deps"))
    sys.path.insert(0, str(build / "python-deps"))
    try:
        import unicorn
        from unicorn import Uc, UC_ARCH_ARM, UC_MODE_THUMB, UC_MODE_MCLASS
        from unicorn.arm_const import UC_ARM_REG_SP, UC_ARM_REG_LR, UC_ARM_REG_R0, UC_ARM_REG_PC
    except ImportError:
        raise SystemExit("Install test-only unicorn==2.1.3 as documented in README.md; compilation passed, simulation not run")
    if unicorn.__version__ != "2.1.3":
        raise SystemExit("Expected unicorn==2.1.3")
    tests_obj = build / "module2_tests.o"
    run(base + ["-ffreestanding", "-fno-builtin", "-c", args.test_source, "-o", tests_obj])
    elf = build / "module2_tests.elf"
    run(base + ["-nostdlib", "-Wl,-Ttext=0x10000", "-Wl,-Tdata=0x20000000", "-Wl,-e,0x10000", *objects, tests_obj, "-lgcc", "-o", elf])
    symbols = subprocess.check_output([str(args.tools / "arm-none-eabi-nm.exe"), "-n", str(elf)], text=True)
    tests = {}
    checks_addr = None
    layout_addr = None
    for line in symbols.splitlines():
        fields = line.split()
        if len(fields) == 3:
            address, _, name = fields
            if name.startswith("test_"):
                tests[name] = int(address, 16)
            if name == "checks":
                checks_addr = int(address, 16)
            if name == "hall_layout":
                layout_addr = int(address, 16)
    # ELF32小端PT_LOAD读取，不引入另一个解析库；Flash/SRAM区间严格校验。
    raw = elf.read_bytes()
    assert raw[:6] == b"\x7fELF\x01\x01"
    ph_offset = struct.unpack_from("<I", raw, 28)[0]
    ph_size, ph_count = struct.unpack_from("<HH", raw, 42)
    loads = []
    for index in range(ph_count):
        typ, offset, vaddr, _, size, memsize, _, _ = struct.unpack_from("<8I", raw, ph_offset + index * ph_size)
        if typ == 1 and memsize:
            assert (0x10000 <= vaddr and vaddr + memsize <= 0x110000) or (0x20000000 <= vaddr and vaddr + memsize <= 0x20100000)
            loads.append((vaddr, raw[offset:offset + size]))
    results = []
    assert tests and checks_addr is not None
    for name, entry in tests.items():
        machine = Uc(UC_ARCH_ARM, UC_MODE_THUMB | UC_MODE_MCLASS)
        machine.mem_map(0x10000, 0x100000)
        machine.mem_map(0x20000000, 0x100000)
        for addr, data in loads:
            if data:
                machine.mem_write(addr, data)
        machine.reg_write(UC_ARM_REG_SP, 0x200f0000)
        machine.reg_write(UC_ARM_REG_LR, 0xf0001)
        machine.emu_start(entry | 1, 0xf0000, timeout=20_000_000, count=args.instruction_limit)
        result = machine.reg_read(UC_ARM_REG_R0)
        completed = machine.reg_read(UC_ARM_REG_PC) == 0xf0000
        count = struct.unpack("<I", machine.mem_read(checks_addr, 4))[0]
        results.append({"test": name, "completed": completed, "passed": result == 0 and completed, "failed_cpp_line": result if result and completed else None, "checks": count})
        print(("PASS: " if results[-1]["passed"] else "FAIL: ") + name + f" ({count} checks, failure line {result if completed else 'execution limit'})", flush=True)
    report = {"validation": "ARM instruction simulation of original module C++", "compiler": subprocess.check_output([str(compiler), "--version"], text=True).splitlines()[0], "core": "Arduino-Pico 6.1.0", "unicorn": unicorn.__version__, "hardware_tested": False, "game_tested": False, "instruction_limit": args.instruction_limit,
              "object_size_bytes": dict(zip(["Sample", "Estimate", "Calibration", "HallModule", "HallPosition", "HallCalibration"], struct.unpack("<6I", machine.mem_read(layout_addr, 24)))) if layout_addr is not None else {}, "results": results}
    (build / "simulation_results.json").write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding="utf-8")
    if not all(r["passed"] for r in results):
        raise SystemExit(1)


if __name__ == "__main__":
    main()
