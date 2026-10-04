# 模块1编译与模拟验证

这些是模块1专用测试；使用统一common/board头，但没有接入其他业务算法目录。build/为可再生输出，不提交第三方wheel、DLL、临时sketch或UF2。

## 固定工具

- Arduino-Pico 6.1.0，包内ARM GCC 16.1.0（pqt-gcc 5.0.0-9576866）。
- Arduino CLI 0.35.3，本机随Arduino IDE安装；TinyUSB使用板卡包自带3.7.7。
- Python 3.12；纯C++测试用Unicorn 2.1.3执行ARM Thumb指令，不另写一个算法模型替代源码。
- 所有模块源码以C++17、Cortex-M0+、-Wall -Wextra -Werror编译；正式Arduino核心使用其C++23构建规则。

在项目根目录运行（可把python换成已有Python3.12的绝对路径）：

~~~powershell
python -m pip install --target tests/module1/build/python-deps unicorn==2.1.3
python tests/module1/run_tests.py
python tests/module1/compile_smoke.py
~~~

仅做严格编译：python tests/module1/run_tests.py --compile-only。
工具链路径不同可用--core、--tools；烟测CLI路径可用--cli。不下载/更新Arduino核心，不修改安装库，也不执行upload。

本次pip下载未完成；已从PyPI获取对应Windows wheel、校验SHA256后只解压至build/python-deps。离线复现可使用同一wheel：

- 文件：unicorn-2.1.3-cp312-cp312-win_amd64.whl。
- SHA256：1b23084ab42fa32ce8758004ce8f3eebbd70106ca6c6deb438a292d5107f01d6。
- 发布来源：https://pypi.org/project/unicorn/2.1.3/。

## 输出及解释

- validation_2026-10-03.json：阶段3历史验证快照，含当时模块源码SHA256、测试与审查摘要；不依赖build目录留存。
- build/simulation_results.json：当前十一组实际C++测试，失败给出module1_tests.cpp行号；每次重新编译原始模块源码。
- build/arduino_results.json：固定FQBN、返回码和所有模块源码SHA256。
- build/arduino_compile.log：包括第三方库警告的完整编译日志。
- build/Module1Compile/：自动生成的编译专用sketch，回调始终拒绝发送，没有FluxRail USB后端。**不要上传其UF2。**

烟测使用Generic RP2040、4MB no FS、W25Q64JV /4、125MHz、Adafruit TinyUSB。它证明GPIO/PIO/DMA和模块逻辑能在本机Arduino依赖中链接；不证明NKRO描述符、普通/IO4枚举、双核通信、Flash、主机或游戏可用。

九组测试覆盖默认绑定/US ASCII、十二usage全按、任意同usage交叠/快击/忙重试、目标UP时组合修饰不提前加入、独立修饰重触发、鼠标OR持有、64事件背压、序号/revision/快照/热改键、micros回绕、消抖/显式溢出、四持续灯效+PRESS的全亮度范围、最终通道限幅/灯序/RGB及GRB。

实际RP2040外设时序不由Unicorn模拟。需要实板测逐灯、灯珠色序/电平、按键延迟、DMA资源和USB并发；游戏还需测短UP识别。

阶段4/5公共集成另见tests/integration/README.md；当前源哈希以其validation_2026-10-03.json为准。独立烟测现在同时复制模块1使用的common/board头，不包含App/NormalHid。


阶段10新增全部169个支持usage的十二来源重触发及共用亮度/指示策略回归，模块1十一组68,415检查；整体74组144,358检查见tests/integration/validation_module1_2026-10-03.json。最终[代码审查](REVIEW_2026-10-03.md)记录实际修复、边界与未测项；历史快照保留，不用旧哈希说明新源码。
