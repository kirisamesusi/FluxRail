# 模块2验证

2026-10-03。测试原始hall C++源码，不以JS/Python重写定位算法。

## 复现

在项目根目录执行：

~~~powershell
python tests/module2/run_tests.py
python tests/module2/compile_smoke.py
python tests/module2/finalize_report.py
~~~

run_tests.py使用本机Arduino-Pico6.1.0的pqt-gcc5.0.0-9576866（ARM GCC16.1.0），C++17、Cortex-M0+、-Os、-Wall -Wextra -Werror、关闭异常/RTTI。5个纯模块.cpp链接真实源码，另严格编译HallAdcRp2040.cpp及真实Pico SDK头；硬件对象不进入指令模拟。

执行器沿用模块1ELF/Unicorn方式，独立位于本目录，不执行或修改模块1测试。固定Unicorn2.1.3，可复用tests/module1/build/python-deps；没有时可用已本地保留的wheel安装到本模块目录：

~~~powershell
python -m pip install --no-index --target tests/module2/build/python-deps tests/module1/build/unicorn-2.1.3-cp312-cp312-win_amd64.whl
~~~

该wheel只适用于本机Python3.12/Windows amd64；换环境需对应Unicorn2.1.3版本，不能把导入失败记作模拟通过。--compile-only仅编译，--core/--tools/--build-dir可覆盖路径。每组200M ARM指令、20秒模拟超时，completed=false明确属于未完整执行，不是断言失败或通过。没有USB主机/真实双核/ADC外设模拟。

compile_smoke.py调用Arduino IDE自带CLI0.35.3，固定Pico6.1.0、板包TinyUSB3.7.7、Generic RP2040/4MiB no FS/W25Q64JV /4/125MHz。复制hall及公共BoardConfig/InputTypes/SpscQueue到build/Module2Compile，用隔离入口把模块链接完整；不接正式App，不修改安装库/公共依赖，也不上传。烟测里与测试里-41/39等都是合成样本标签，不是设备默认端点，实际固件无有效校准默认禁用。

## 覆盖与实际结果

| 测试 | 主要检查 |
| --- | --- |
| absolute_return_endpoints_gain_invert | 三次双向961点扫描、返回同位置、真实外侧端点、归一化/范围/倍率/反转 |
| failure_reentry_and_parameter_atomicity | 失磁/饱和/采样错/偏离曲线、恢复直接定位、坏参数和NaN不覆盖旧设置/校准 |
| activity_slow_motion_filter_dt_wrap_reverse | 累积慢移活动、快扫/反向、静止超时、micros回绕、活动不随倍率变 |
| saved_calibration_rejection_ambiguity | 版本/缺覆盖/无效范围/噪声/增益/极性/三维重合拒绝、开机直接复用 |
| async_calibration_success_and_reuse | 空场、三参考、7节点、完整扫、混合磁极方向及保存结果复用 |
| calibration_cancel_timeout_noise_coverage_keeps_old | 真实HallModule内部失败/取消/超时/噪声/缺覆盖/饱和/断序保留旧对象 |
| module_calibration_success_atomic_application | 不同基线/增益候选，在完成后下一采样边界替换，恢复无旧活动 |
| module_queue_recovery_revision_calibration_suppression | 64条队列满停止、暂停不可读取、清理/新epoch/版本、校准输出抑制 |
| sampler_budget_cadence_errors_timestamp_wrap | 不补采、尝试计数、失败/越界ADC、250us预算、通道偏差、中点回绕 |
| measured_short_travel_and_maximum_nodes | 真实行程短于传感器跨度、17节点容量、投影边界防越界 |
| calibration_control_cooperative_checks_and_command_timeout | MapCheck每采样仅推进一个检查点、重复采样不累加、弱参考、完成命令超时 |

11组全部完成且通过，共4,615检查。6个真实模块单元严格编译无警告。独立Arduino完整链接通过，Flash85,036字节/RAM22,116字节；日志含2条第三方cdc_host.h缺省字段警告，项目源无警告。

build/simulation_results.json、arduino_results.json、arduino_compile.log是原始结果。finalize_report.py对源码与Arduino实际复制哈希逐项核对，生成长期保存的validation_2026-10-03.json，含源码/测试哈希、完整模拟结果、依赖/构建配置、对象大小和烟测UF2哈希。build可重建；UF2是测试产物，不能作为正式整机版本。

## 不覆盖的项目

实板/游戏均未执行。合成三路曲线的稳定和返回误差不能代表真实49E/ADC/磁铁/装配精度。ADC驱动只以真实SDK编译；ADC READY/复位/模拟噪声、双核竞争、真实最大服务耗时/采样节拍、来源路由、HID绝对X是否改Y、IO4和Flash掉电恢复都未测。本轮没有修改/上传正式App或游戏配置。

实板步骤见[HARDWARE_CHECKLIST.md](HARDWARE_CHECKLIST.md)，公共接入与需要冻结的字段见[hall/README](../../firmware/FluxRail/src/hall/README.md)，最新交接见[模块2进度](../../docs/progress/模块2.md)。
