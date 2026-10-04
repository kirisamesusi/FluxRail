# 模块3验证与复现

2026-10-03。构建/模拟证据与阶段审查见docs/progress/模块3.md，最终源码快照见validation_2026-10-03.json。所有脚本只生成项目tests/module3/build文件，不上传设备。实际SSI/双核/USB/机械/游戏未测。

## 依赖及命令

- 已安装Arduino-Pico6.1.0；ARM工具链pqt-gcc/5.0.0-9576866实际GCC16.1.0；C++17、Cortex-M0+、-Os。
- Arduino CLI0.35.3（Arduino IDE内置）、Generic RP2040/4MiB noFS/W25Q64JV /4/125MHz/Adafruit TinyUSB3.7.7。独立烟测用原板包TinyUSB，真实App适配检查使用现有项目启动适配库，不修改安装库。
- ARM模拟Unicorn2.1.3。目前复用tests/module1/build/python-deps已有环境；新机缺少时只装测试依赖至module3/build/python-deps：python -m pip install --target tests/module3/build/python-deps unicorn==2.1.3。不安装/改动固件依赖。

~~~powershell
python tests/module3/run_tests.py
python tests/module3/compile_smoke.py
python tests/module3/compile_app_adapter.py
python tests/module3/finalize_report.py
~~~

run_tests支持--core、--tools、--compile-only、--instruction-limit；compile_smoke支持--cli。--compile-only仅编译，不当模拟通过。真实App适配检查需要dependencies/generated/Adafruit_TinyUSB_Arduino已有唯一启动适配；若新机缺少，由集成流程运行现有dependencies/prepare_tinyusb.py，不另造库/回调。

## 验证含义

run_tests对5个纯算法/模块.cpp及真实SDK SSI.cpp严格-Wall -Wextra -Werror编译。测试对象在-ffreestanding环境链接原始算法.cpp并由Unicorn执行ARM Thumb指令，结果必须completed并返回0；达到执行上限不是通过。CRC参考是独立多项式长除法、合成24位帧，不证明芯片CRC初始化。

13组：CRC/状态/单bit损坏；跨零正反多圈与gain0.5/1/2；CRC/场错误恢复；半圈/速度/时间回绕；小数/滤波守恒/物理活动与配置；PUSH启动/旋转/UP/超时；PUSH坏帧/不一致/缺采/回绕；两轴队列顺序/独立背压/新epoch/丢弃账本；采样节拍/丢周期/独立失败；参数NaN/Inf及整包应用；公共PUSH适配；单传感器长期CRC故障与同轴来源保留；PUSH满队列/暂停遥测一致性。新增测试仅验证对应算法风险，不把合成曲线当真实机械性能。

compile_smoke复制模块3和所需公共头文件到隔离Arduino sketch，以正式板配置完整链接；同时校验源文件在构建中未变化、Arduino编译副本与原始源码一致及无项目警告。结果明确production_application=false。生成的UF2是隔离编译产物，没有接正式App/HID运动，不用于上传。

compile_app_adapter只严格编译真实app::App与AppPushSink模板，验证App::submitFilteredPush类型/调用链，无运行/链接或USB主机证据。第三方cdc_host.h已知缺省字段warning允许，项目warning仍失败。

finalize_report核对以上各报告哈希与当前源码一致，保留工具版本、检查数、对象大小、Flash/RAM、UF2及测试/代码哈希到长期快照。build中存simulation_results.json、arduino_results.json、arduino_compile.log、app_adapter_results.json及对象/ELF/UF2。实板步骤见HARDWARE_CHECKLIST.md。
