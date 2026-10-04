"""Execute real JSON/runtime codec/protocol, coordinator and CDC transport with ARM instructions.
Only USB FIFO/mutex boundary is mocked; no USB host or dual-core hardware simulation.
"""
from pathlib import Path
import subprocess
import sys
HERE = Path(__file__).resolve().parent
ROOT = HERE.parent.parent
sources = ["realtime/InputWorker.cpp", "app/ConfigCoordinator.cpp", "protocol/Json.cpp", "protocol/RuntimeCodec.cpp", "protocol/ConfigProtocol.cpp", "usb/CdcTransport.cpp", "protocol/ProfileCodec.cpp", "protocol/ConfigProtocolDevice.cpp", "config/ProfileManager.cpp", "config/DeviceCodec.cpp", "config/ConfigStore.cpp", "hall/HallTypes.cpp", "knobs/KnobTypes.cpp", "app/App.cpp", "usb/NormalHid.cpp", "usb/NormalHidCodec.cpp", "usb/Io4Codec.cpp", "usb/UsbBackend.cpp", "app/Acquisition.cpp", "app/MotionRouter.cpp", "hall/HallPosition.cpp", "hall/HallCalibration.cpp", "hall/HallModule.cpp", "hall/HallSampler.cpp", "knobs/KnobMotion.cpp", "knobs/PushFilter.cpp", "knobs/KnobModule.cpp", "knobs/KnobSampler.cpp", "../../../tests/integration/sensor_stubs.cpp"]
command = [sys.executable, str(ROOT / "tests/module1/run_tests.py"), "--instruction-limit", "300000000", "--test-source", str(HERE / "io4_tests.cpp"), "--extra-include", str(HERE / "stubs"), "--skip-hardware", "--extra-flag=-DARDUINO_ARCH_RP2040", "--extra-include", str(HERE / "stubs_app"), "--build-dir", str(HERE / "build/io4")]
for source in sources:
    command += ["--extra-source", source]
subprocess.run(command, cwd=ROOT, check=True)
