"""Current evidence now includes schema2/IO4; preserve phase8 historical snapshot."""
from pathlib import Path
import runpy
runpy.run_path(str(Path(__file__).with_name("finalize_io4_report.py")),run_name="__main__")
