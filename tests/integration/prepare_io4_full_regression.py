from pathlib import Path
h=Path(__file__).parent
test=h/'io4_timeout_full_tests.cpp';runner=h/'run_io4_timeout_regression.py'
assert not test.exists() and not runner.exists()
s=(h/'io4_timeout_order_tests.cpp').read_text(encoding='utf-8')
test.write_text('#include "io4_tests.cpp"\n'+s[s.index('extern "C" int test_io4_future'):],encoding='utf-8')
s=(h/'run_io4.py').read_text(encoding='utf-8').replace('"io4_tests.cpp"','"io4_timeout_full_tests.cpp"').replace('HERE / "build/io4"','HERE / "pc_board_2026-10-04/io4-timeout-full-arm"')
runner.write_text(s,encoding='utf-8')
