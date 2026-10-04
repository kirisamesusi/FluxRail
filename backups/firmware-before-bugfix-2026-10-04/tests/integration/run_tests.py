"""使用模块1的固定ARM编译/指令模拟驱动执行公共集成状态机测试。"""
from pathlib import Path
import subprocess
import sys
HERE=Path(__file__).resolve().parent
ROOT=HERE.parent.parent
subprocess.run([sys.executable,str(ROOT/"tests/module1/run_tests.py"),"--test-source",str(HERE/"integration_tests.cpp"),"--extra-source","realtime/InputWorker.cpp","--extra-source","app/ConfigCoordinator.cpp","--extra-source","usb/NormalHidCodec.cpp","--build-dir",str(HERE/"build")],cwd=ROOT,check=True)
