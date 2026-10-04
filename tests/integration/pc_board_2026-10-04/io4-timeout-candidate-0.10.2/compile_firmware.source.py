"""固定依赖并编译正式FluxRail普通NKRO原型；绝不上传设备。
使用项目私有TinyUSB启动适配，安装库保持原样；不生成IO4/Flash保存假结果。
"""
from datetime import datetime, timedelta, timezone
from pathlib import Path
import argparse
import hashlib
import json
import os
import subprocess
import sys
sys.stdout.reconfigure(encoding="utf-8",errors="backslashreplace")

HERE=Path(__file__).resolve().parent
ROOT=HERE.parent.parent
BUILD=HERE/"build/arduino"
LIBRARY=ROOT/"dependencies/generated/Adafruit_TinyUSB_Arduino"
FQBN="rp2040:rp2040:generic:flash=4194304_0,boot2=boot2_w25q64jv_4_padded_checksum,freq=125,usbstack=tinyusb"
CLI=Path(os.environ.get("ProgramFiles","C:/Program Files"))/"Arduino IDE/resources/app/lib/backend/resources/arduino-cli.exe"


def audit_sources_and_link():
    """用Arduino实际参数严格编译项目源，并检查唯一USB所有者；不改已链接对象。"""
    database=json.loads((BUILD/"compile_commands.json").read_text(encoding="utf-8"))
    strict=HERE/"build/strict";strict.mkdir(parents=True,exist_ok=True)
    outcomes=[]
    compiler=None
    for entry in database:
        source=Path(entry["file"])
        if not source.is_relative_to(BUILD/"sketch"):continue
        arguments=entry["arguments"][:]
        compiler=Path(arguments[0]).with_suffix(".exe")
        arguments[0]=str(compiler)
        # 必须保留Arduino的头文件搜索顺序；改成-isystem会误混Pico-SDK内另一份TinyUSB。
        # 已知第三方missing-field-initializers仅降为warning；项目中同类warning仍计失败。
        normalized=arguments
        normalized[normalized.index("-o")+1]=str(strict/(source.relative_to(BUILD/"sketch").as_posix().replace("/","_")+".o"))
        normalized.extend(["-fstack-usage","-Werror","-Wno-error=missing-field-initializers"])
        result=subprocess.run(normalized,cwd=entry["directory"],capture_output=True,encoding="utf-8",errors="replace")
        diagnostic=result.stdout+result.stderr
        project_warnings=[line for line in diagnostic.splitlines() if "warning:" in line and "sketch" in line and "dependencies" not in line]
        outcomes.append({"source":str(source),"exit_code":result.returncode or int(bool(project_warnings)),"diagnostics":diagnostic,"project_warnings":project_warnings})
    if not compiler or not outcomes:raise RuntimeError("Project compile database missing")
    nm=compiler.parent/"arm-none-eabi-nm.exe"
    symbols=subprocess.check_output([str(nm),"--defined-only",str(BUILD/"FluxRail.ino.elf")],text=True)
    owned=["fluxrail_usb_configure_before_connect","tud_descriptor_device_cb","tud_descriptor_configuration_cb","tud_descriptor_string_cb","tud_hid_descriptor_report_cb","tud_hid_get_report_cb","tud_hid_set_report_cb","tud_hid_report_complete_cb","tud_hid_report_failed_cb","fluxrail_usb_wants_cdc","fluxrail_cdc_tx_idle"]
    selected={name:[line for line in symbols.splitlines() if line.split()[-1:]==[name]] for name in owned}
    single_strong=all(len(rows)==1 and rows[0].split()[1]=="T" for rows in selected.values())
    # 确认弱声明的setup1/loop1最终链接到项目入口，而不是空的核心缺省函数。
    objdump=compiler.parent/"arm-none-eabi-objdump.exe"
    disassembly=subprocess.check_output([str(objdump),"-d","-C",str(BUILD/"FluxRail.ino.elf")],text=True)
    import re
    entry_blocks={name:re.search(r"<"+name+r">:\n(.*?)(?=\n\n)",disassembly,re.S).group(1) for name in ["setup1","loop1"]}
    core1_entry=all(target in entry_blocks[name] for name,target in [("setup1","beginCore1"),("loop1","serviceCore1")])
    stacks=[]
    for path in strict.glob("*.su"):
        for row in path.read_text().splitlines():
            parts=row.split("\t")
            if len(parts)==3 and parts[1].isdigit():stacks.append({"function":parts[0],"bytes":int(parts[1]),"kind":parts[2]})
    storage_partition=(BUILD/"memmap_default.ld").read_text()
    partition_ok="LENGTH = 4173824" in storage_partition
    stack_budget_ok=bool(stacks) and all(x["bytes"]<=2048 for x in stacks)
    report={"stack_usage":sorted(stacks,key=lambda x:x["bytes"],reverse=True),"storage_partition_reserved":partition_ok,"stack_frame_budget_passed":stack_budget_ok,"project_translation_units":outcomes,"unique_strong_usb_callbacks":selected,"core1_entry_calls_project":core1_entry,"passed":stack_budget_ok and partition_ok and single_strong and core1_entry and all(x["exit_code"]==0 for x in outcomes)}
    (HERE/"build/source_link_audit.json").write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding="utf-8")
    for row in outcomes:
        if row["exit_code"]:print(row["source"]+"\n"+"\n".join(row["diagnostics"].splitlines()[:25]),flush=True)
    return report

def source_hashes():
    return {p.relative_to(ROOT).as_posix(): hashlib.sha256(p.read_bytes()).hexdigest()
            for p in (ROOT / "firmware/FluxRail").rglob("*")
            if p.suffix in [".cpp", ".h", ".ino"]}

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--cli",type=Path,default=CLI)
    args=parser.parse_args()
    # 生成步骤校验源文件SHA256，不能悄悄接受另一版初始化时序。
    subprocess.run([sys.executable,str(ROOT/"dependencies/prepare_tinyusb.py")],cwd=ROOT,check=True)
    BUILD.mkdir(parents=True,exist_ok=True)
    command=[str(args.cli),"compile","--fqbn",FQBN,"--library",str(LIBRARY),"--build-property","build.flash_length=4173824","--build-property","upload.maximum_size=4173824","--build-property","compiler.cpp.extra_flags=-DFLUXRAIL_STORAGE_PARTITION=1 -DFLUXRAIL_IO4_CONFIG_CDC=0","--build-path",str(BUILD),"--build-cache-path",str(HERE/"build/arduino-cache"),"--warnings","all",str(ROOT/"firmware/FluxRail")]
    source_before = source_hashes()
    result=subprocess.run(command,cwd=ROOT,capture_output=True,encoding="utf-8",errors="replace")
    diagnostics=result.stdout+"\n"+result.stderr
    (HERE/"build/firmware_compile.log").write_text(diagnostics,encoding="utf-8")
    sources = source_hashes()
    source_drift = sorted(name for name in set(source_before) | set(sources)
                          if source_before.get(name) != sources.get(name))
    copied_source_mismatch = []
    for name, digest in sources.items():
        relative = Path(name).relative_to("firmware/FluxRail")
        if relative.suffix == ".ino":
            continue  # Arduino converts the entry to a generated .ino.cpp.
        copy = BUILD / "sketch" / relative
        if not copy.exists():
            copied_source_mismatch.append(name)
            continue
        data = copy.read_bytes()
        # Arduino CLI prepends one path-specific #line directive to copied C++/headers.
        expected_line = ("#line 1 " + json.dumps(str(ROOT / name)) + "\n").encode("utf-8")
        if data.startswith(expected_line):
            data = data[len(expected_line):]
        if hashlib.sha256(data).hexdigest() != digest:
            copied_source_mismatch.append(name)
    manifest=json.loads((LIBRARY/"FluxRail-adapter.json").read_text(encoding="utf-8"))
    report={"date_local":datetime.now(timezone(timedelta(hours=8))).isoformat(),"validation":"single firmware NORMAL_NKRO/IO4 compile/link only; no USB host or game verification","fqbn":FQBN,"core":"Arduino-Pico 6.1.0","tinyusb":"3.7.7 project-owned startup adapter","exit_code":result.returncode,"source_sha256":sources,"dependency_adapter":manifest,"uploaded":False,"hardware_tested":False,"game_tested":False,"io4_implemented":True,"flash_storage_implemented":True,"log":"firmware_compile.log","cdc_protocol_version":1,"runtime_config_scope":"device_schema2","flash_slot_offsets":[4173824,4182016],"flash_slot_bytes":8192,"source_drift_during_compile":source_drift,"copied_source_mismatch":copied_source_mismatch}
    (HERE/"build/firmware_results.json").write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding="utf-8")
    print(result.stdout,flush=True)
    print("Full diagnostics: "+str(HERE/"build/firmware_compile.log"),flush=True)
    if result.returncode:
        print(result.stderr,flush=True)
        raise SystemExit(result.returncode)
    if source_drift or copied_source_mismatch:
        report["exit_code"] = 1
        (HERE/"build/firmware_results.json").write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding="utf-8")
        raise SystemExit("Sources changed during compile or Arduino source copy differs; rerun after source settles")
    audit=audit_sources_and_link()
    # Check the actual UF2 with the Web parser; LTO must retain exactly one record.
    try:
        metadata=json.loads(subprocess.check_output(["node",str(HERE/"check_firmware_version.mjs")],cwd=ROOT,text=True,encoding="utf-8"))
    except Exception:
        report["exit_code"]=1
        (HERE/"build/firmware_results.json").write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding="utf-8")
        raise
    report["firmware_metadata"]=metadata
    print("PASS: formal UF2 FluxRail identity and version "+metadata["firmware"]["version"],flush=True)
    report["strict_project_units"]=len(audit["project_translation_units"])
    report["source_link_audit_passed"]=audit["passed"]
    report["exit_code"]=0 if audit["passed"] else 1
    (HERE/"build/firmware_results.json").write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding="utf-8")
    if not audit["passed"]:raise SystemExit("Source/link audit failed; see source_link_audit.json")
    print("PASS: strict project compilation and unique callback/core1-entry link audit ("+str(len(audit["project_translation_units"]))+" units)",flush=True)
    # NormalHid.h的适配宏进一步确保不是setup后注册HID的原版库。
    print("PASS: linked formal FluxRail entry with project TinyUSB startup adapter; not uploaded",flush=True)

if __name__=="__main__":main()
