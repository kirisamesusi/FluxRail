"""从已安装Arduino-Pico 6.1.0生成项目私有TinyUSB3.7.7适配，不修改系统库。
受控修改包含启动前人格选择/可选CDC、长字符串与CDC完成查询；不引入第二个USB栈或另一套descriptor回调。
"""
from pathlib import Path
import argparse
import hashlib
import json
import os
import shutil

ROOT=Path(__file__).resolve().parent.parent
DEFAULT=Path(os.environ.get("LOCALAPPDATA",str(Path.home()/"AppData/Local")))/"Arduino15/packages/rp2040/hardware/rp2040/6.1.0/libraries/Adafruit_TinyUSB_Arduino"
TARGET=ROOT/"dependencies/generated/Adafruit_TinyUSB_Arduino"
# 仅支持本机已经审查过的核心包源码；同版本内容变化也必须重新审查。
AUDITED_SHA256={
    "src/arduino/Adafruit_USBD_Device.cpp":"bf5b5618c8bc6654b0d139349e7a9a68ba051a2146c0b84f0c52e3b7e3122c4c",
    "src/class/cdc/cdc_device.c":"2ca9f63f5812e6fd58405e52b1bb282aa0752d1110fa577aca6442c1f67f4840",
    "src/arduino/Adafruit_USBD_Device.h":"63b7cd70b8428e66057749251b5fe64381301a062b4fe6a378447bd51bdeb468",
}

def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument("--source",type=Path,default=DEFAULT);a=p.parse_args()
    props=(a.source/"library.properties").read_text(encoding="utf-8")
    if "version=3.7.7" not in props.splitlines():raise SystemExit("Expected installed TinyUSB3.7.7")
    cpp_rel="src/arduino/Adafruit_USBD_Device.cpp";h_rel="src/arduino/Adafruit_USBD_Device.h"
    for rel,expected in AUDITED_SHA256.items():
        actual=hashlib.sha256((a.source/rel).read_bytes()).hexdigest()
        if actual!=expected:raise SystemExit("Unaudited installed source: "+rel)
    original=(a.source/cpp_rel).read_text(encoding="utf-8")
    anchor="  SerialTinyUSB.begin(115200);\n\n  // Init device hardware and call tusb_init()"
    if original.count(anchor)!=1:raise SystemExit("Installed library layout differs from audited3.7.7")
    shutil.copytree(a.source/"src",TARGET/"src",dirs_exist_ok=True)
    for name in ["library.properties","LICENSE","license.txt","LICENSE.txt"]:
        source=a.source/name
        if source.is_file():shutil.copy2(source,TARGET/name)
    replacement="""  if (fluxrail_cdc_enabled) SerialTinyUSB.begin(115200);

#if defined(ARDUINO_ARCH_RP2040)
  // FluxRail受控适配：在自动启动的第一次连接前注册本次保存/恢复人格。
  // 项目仅实现此钩子，原有TinyUSB全局描述符回调继续由本库唯一持有。
  extern void fluxrail_usb_configure_before_connect(void) __attribute__((weak));
  if (fluxrail_usb_configure_before_connect) fluxrail_usb_configure_before_connect();
#endif

  // Init device hardware and call tusb_init()"""
    # C++函数内extern与项目extern C不匹配，声明移到文件作用域。
    replacement=replacement.replace("  extern void fluxrail_usb_configure_before_connect(void) __attribute__((weak));\n","")
    patched=original.replace(anchor,replacement)
    declaration='\nextern "C" void fluxrail_usb_configure_before_connect(void) __attribute__((weak));\nextern "C" bool fluxrail_usb_wants_cdc(void) __attribute__((weak));\n'
    patched=patched.replace('#include "Adafruit_USBD_Device.h"','#include "Adafruit_USBD_Device.h"'+declaration,1)
    if declaration not in patched:raise SystemExit("Declaration insertion failed")
    patched=patched.replace("  clearConfiguration();\n", "  clearConfiguration();\n  bool const fluxrail_cdc_enabled = !fluxrail_usb_wants_cdc || fluxrail_usb_wants_cdc();\n",1)
    for field in ["bDeviceClass", "bDeviceSubClass", "bDeviceProtocol"]:
        import re
        patched=re.sub(r"(_desc_device\."+field+r" = )(TUSB_CLASS_MISC|MISC_SUBCLASS_COMMON|MISC_PROTOCOL_IAD)(;)",r"\1fluxrail_cdc_enabled ? \2 : 0\3",patched,count=1)
    patched=patched.replace("serial_utf16 + 1, 32)", "serial_utf16 + 1, 126)").replace("_desc_str + 1, 32)","_desc_str + 1, 126)")
    (TARGET/cpp_rel).write_text(patched,encoding="utf-8")
    h=(a.source/h_rel).read_text(encoding="utf-8")
    (TARGET/h_rel).write_text(h.replace("_desc_str[32 + 1]","_desc_str[126 + 1]")+"\n#define FLUXRAIL_TINYUSB_STARTUP_ADAPTER 2\n",encoding="utf-8")
    cdc_rel="src/class/cdc/cdc_device.c"
    cdc=(a.source/cdc_rel).read_text(encoding="utf-8")
    anchor_cdc="static cdcd_interface_t _cdcd_itf[CFG_TUD_CDC];"
    if cdc.count(anchor_cdc)!=1:raise SystemExit("CDC adapter anchor changed")
    helper="""
// FluxRail private nonblocking drain query. Caller owns the RP2040 USB mutex.
// FIFO availability alone is insufficient: the last packet/ZLP may be in flight.
bool fluxrail_cdc_tx_idle(uint8_t itf) {
  if (itf >= CFG_TUD_CDC || !tud_cdc_n_connected(itf)) return false;
  const cdcd_interface_t *p = &_cdcd_itf[itf];
  return p->tx_stream.ep_addr != 0 && tu_fifo_count(&p->tx_stream.ff) == 0 &&
         !usbd_edpt_busy(p->rhport, p->tx_stream.ep_addr);
}
"""
    (TARGET/cdc_rel).write_text(cdc.replace(anchor_cdc,anchor_cdc+helper),encoding="utf-8")
    manifest={"upstream":"Arduino-Pico6.1.0 bundled Adafruit TinyUSB3.7.7","source":str(a.source),"generated":str(TARGET),"modified":list(AUDITED_SHA256),"source_sha256":{rel:hashlib.sha256((a.source/rel).read_bytes()).hexdigest() for rel in AUDITED_SHA256},"generated_sha256":{rel:hashlib.sha256((TARGET/rel).read_bytes()).hexdigest() for rel in AUDITED_SHA256},"system_library_modified":False}
    (TARGET/"FluxRail-adapter.json").write_text(json.dumps(manifest,ensure_ascii=False,indent=2),encoding="utf-8")
    print(TARGET)
if __name__=="__main__":main()
