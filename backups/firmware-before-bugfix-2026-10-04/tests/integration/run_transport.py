"""真实NormalHid C++的ARM指令测试；只替换USB端点/互斥锁边界，不模拟主机。"""
from pathlib import Path
import subprocess
import sys
HERE=Path(__file__).resolve().parent
ROOT=HERE.parent.parent
subprocess.run([sys.executable,str(ROOT/"tests/module1/run_tests.py"),"--test-source",str(HERE/"transport_tests.cpp"),"--extra-source","usb/NormalHidCodec.cpp", "--extra-source", "usb/Io4Codec.cpp", "--extra-source", "usb/UsbBackend.cpp","--extra-source","usb/NormalHid.cpp","--extra-include",str(HERE/"stubs"),"--extra-include",str(HERE/"stubs_app"),"--skip-hardware","--build-dir",str(HERE/"build/transport")],cwd=ROOT,check=True)
