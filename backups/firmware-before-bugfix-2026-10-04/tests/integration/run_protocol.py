"""Execute real JSON/runtime codec/protocol, coordinator and CDC transport with ARM instructions.
Only USB FIFO/mutex boundary is mocked; no USB host or dual-core hardware simulation.
"""
from pathlib import Path
import subprocess
import sys
HERE = Path(__file__).resolve().parent
ROOT = HERE.parent.parent
sources = ["realtime/InputWorker.cpp", "app/ConfigCoordinator.cpp", "protocol/Json.cpp", "protocol/RuntimeCodec.cpp", "protocol/ConfigProtocol.cpp", "usb/CdcTransport.cpp", "protocol/ProfileCodec.cpp", "protocol/ConfigProtocolDevice.cpp", "config/ProfileManager.cpp", "config/DeviceCodec.cpp", "config/ConfigStore.cpp", "hall/HallTypes.cpp", "knobs/KnobTypes.cpp"]
command = [sys.executable, str(ROOT / "tests/module1/run_tests.py"), "--instruction-limit", "100000000", "--test-source", str(HERE / "protocol_tests.cpp"), "--extra-include", str(HERE / "stubs"), "--skip-hardware", "--build-dir", str(HERE / "build/protocol")]
for source in sources:
    command += ["--extra-source", source]
subprocess.run(command, cwd=ROOT, check=True)
