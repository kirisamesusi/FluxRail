# MT6701旋钮与PUSH（原业务模块3）

更新：2026-10-06。本文按当前源码整理。旋钮算法和 PUSH 已接入正式 App，软件模拟/编译与整机软件集成已有证据；SSI 实帧、磁环机械表现、PUSH 误触及游戏输入均未实测。当前源码固件版本宏为 **0.10.2**（[`FirmwareInfo.h`](../../firmware/FluxRail/src/common/FirmwareInfo.h)）；10 月 5 日测试板的设备 `hello` 也报告 **0.10.2**。用户手动刷写已确认，但上传过程和所刷 UF2 哈希未直接观测。完整 0.10.2 回滚归档 UF2 的 SHA-256 为 `92991fa7e70e591fa16857ac09cd513eb337e6b6f0ca9cbe2824292379d9b479`；测试板为 2 MiB，不能据此宣称最终 4 MiB Flash 验收完成。10 月 4 日 1 kHz 采样率实测仅属 0.10.1，不转作 0.10.2 或旋钮 SSI 实测。

## 职责与硬件

两路 MT6701 以 SSI 输出角度，由模块读取完整帧、验证 CRC/场状态、展开连续角度、计算相对增量与累计圈数；两路 PUSH 从芯片 SSI 状态位及 GPIO 交叉确认，经候选滤波后作为普通物理输入来源。模块算法不直接访问 USB、Flash、CDC、模板或网页；正式 App、全局运动路由和发送由公共层所有。

| 旋钮 | SSI | PUSH | 默认建议 |
| --- | --- | --- | --- |
| U4/FPC1（ID 0） | SPI1 RX GP12、CS GP13、SCK GP14 | GP19 | 轴 X，默认关闭 |
| U5/FPC2（ID 1） | SPI0 RX GP16、CS GP17、SCK GP18 | GP20 | 轴 Y，默认关闭 |

无 MOSI；尤其不可初始化 SPI0 默认 TX GP19，以免占用 U4 PUSH。`BoardConfig.h` 是唯一引脚表。当前代码候选参数为 2 MHz、CPOL 高/下降沿采样、24 时钟、CS 覆盖全帧；CRC 多项式为 `x^6+x+1`，零初值/无反射/无最终异或是尚待真实帧验证的实现约定。不要把合成 CRC 向量测试写成芯片协议已实测。

旋钮默认 `enabled=false`、U4=X/U5=Y、gain=1、每圈 1024 HID count、无默认低通滤波；音击两路模板默认关闭。PUSH 默认禁用，两个 PUSH 的动作绑定存于公共十二路 `bindings[10/11]`，而不是旋钮 Settings。PUSH 高有效，候选过滤要求健康 SSI/场、GPIO 与 SSI PUSH 状态一致、稳定确认和低电平重新武装；旋转期间延长确认时间。由于真按压和某些偏心/晃动可能产生不可分辨磁变化，这只是待机械验证的保护策略。

## 当前实现与数据流

- `Mt6701SsiRp2040` 通过 RP2040 SDK 有界 SPI FIFO 访问两路传感器；无 TX/MOSI 引脚初始化，单次传输故障有界返回，单路失败不会在读循环里阻塞另一轴。
- 每路 `KnobSampler` 保持独立 500 µs 固定相位；迟到时跳过已过期周期，只取当前帧，不追赶补采。采样序号包含失败尝试，时间戳记帧中点。
- `KnobMotion` 验证 24 位帧、14 位角度/状态/CRC，展开跨零增量并拒绝半圈歧义、速度边界外运动及过长间隔。失效/重建期间不补猜历史。小数 delta 和设备累计圈数保留；配置允许的低通作用于展开后的增量。
- `PushFilter` 生成过滤 PUSH 状态及快照；`KnobModule` 管理两路算法、独立有界运动队列、背压与暂停恢复丢弃账本。队满停该轴并触发公共恢复，不覆盖旧记录；另一轴继续运行。
- 正式 `app/Acquisition.cpp::service` 每轮轮询 ADC 和两路 SSI，再把样本送 `KnobModule`；PUSH 通过 `AppPushSink<app::App>` 转到唯一 `InputWorker::submitPush`，获得公共十二路边沿流。运动由核心 1 `Acquisition` 队列复制到核心 0 `App::serviceMotion`，经 `MotionRouter` 路由到 `UsbBackend`。路由器统一处理滑轨与 X 旋钮来源接管、Y 轴、分时增量、背压和报告提交。

关键文件：[`KnobTypes.h`](../../firmware/FluxRail/src/knobs/KnobTypes.h)、`Mt6701SsiRp2040.{h,cpp}`、`KnobSampler.{h,cpp}`、`KnobMotion.{h,cpp}`、`PushFilter.{h,cpp}`、`KnobModule.{h,cpp}`、[`AppPushSink.h`](../../firmware/FluxRail/src/knobs/AppPushSink.h)、[`Acquisition.cpp`](../../firmware/FluxRail/src/app/Acquisition.cpp)、[`MotionRouter.h`](../../firmware/FluxRail/src/app/MotionRouter.h)、[`App.cpp`](../../firmware/FluxRail/src/app/App.cpp)。核心 1 是模块对象与硬件外设的唯一所有者；核心 0 不直接读取模块可变诊断引用，只通过公共队列和按值快照访问。

## 接口、配置与协议依赖

模块类型在 `knobs/KnobTypes.h`。公共 `common::KnobSettings` 是 `knobs::Settings` 的别名；`Profile::knobs[2]` 直接保存完整参数，避免第二份配置结构。默认范围及单位以 [`knobs/README.md`](../../firmware/FluxRail/src/knobs/README.md) 和 Settings 校验代码为准。旋钮运动局部类型 `knobs::Motion` 仍不是单独 wire 包；正式 App 通过既有公共运动队列与 `common::MotionSnapshot`，统一处理 revision/epoch。

ConfigCoordinator 通过真实暂停 ACK 后清旧队列、完成参数投影和 Resume ACK；维护、断连或恢复期间结束旧 PUSH 按下状态，重建角度参考并丢弃维护期间运动，不能重放离线圈数。配置更改只在实际采样边界采用并由整机配置事务确认。PUSH 动作仍只来自公共 bindings。CDC/Web 的 JSON 能力由公共协议实现；不要在模块中自建协议、Profile、持久化或第二个动作映射器。

输出语义要按当前正式整合 README 和源码理解：算法产生相对增量；普通模式最终由整机 HID 选择相对或 Hall 参与时的绝对 XY 路径，IO4 不自动将旋钮变成普通鼠标或 spinner。当前正式整合策略中，Hall 启用时旋钮 Y 增量进入本设备绝对 Y 状态，旋钮每 count 使用 32 个逻辑单位；Hall 关闭时走相对鼠标增量。IO4 的旋钮用途仍未定义，PUSH 仍是普通按键输入来源。改变这些输出语义必须同步公共路由、USB 协议、规格和游戏验收，不能仅改 KnobMotion。

## Web旋钮灵敏度标尺（2026-10-06）

两旋钮与滑轨统一1.0～10.0、步进/显示0.1，5.0等效旧gain1；Web明确编辑时换算 `gain=灵敏度/5`，1.0/4.9/5.0/5.1/10.0对应实际0.2/0.98/1/1.02/2。原KnobMotion继续使用实际gain/invert作用于增量和scaled_turns，不把新的标尺数直接乘角度，历史累计圈数也不重新缩放。raw_turns、物理活动、可信转速、PUSH、输出轴和count单位不变。counts_per_turn_at_gain1仍是旧gain1的每圈count，Web标为“灵敏度5.0每圈 HID count”。

固件默认gain1和范围0.01～16、CDC/schema2及序列化保持原语义；默认新标尺自然显示5.0，音击两旋钮默认关不变。旧倍率12或无法对应0.1步进的1.003等仍保留，控件明确提示旧倍率而不显示假边界值；无关编辑保存不量化，明确选择新刻度才替换。本地预览继续用真实gain计算，可见设置文案改用新灵敏度，遥测累计量不换单位。详见[公共契约](../公共架构与配置契约.md)、[实现](../../web/src/sensitivity.js)与[本轮验证](../../web/tests/sensitivity-2026-10-06/README.md)。

新增ARM组 `test_web_sensitivity_scale` 对91刻度、两个ID、正反向执行真实KnobMotion，检查原始/缩放圈数、增量count、活动与轴；本轮模块全部15组/6701检查通过，6个单元严格编译。此为合成SSI软件证据，没有正式整机构建、实板SSI/PUSH或游戏复测。

## 构建、测试与已验证证据

本次只查阅记录和源码，未运行测试/构建。模块测试说明及入口在 [`tests/module3/README.md`](../../tests/module3/README.md)：

~~~powershell
python tests/module3/run_tests.py
python tests/module3/compile_smoke.py
python tests/module3/compile_app_adapter.py
python tests/module3/finalize_report.py
~~~

模块级独立编译/模拟用于算法回归；当前正式 App 已接入，应同时参考 [`tests/integration/run_all.py`](../../tests/integration/run_all.py) 和正式整机固件 README，不能用隔离 Module3 烟测 UF2 代替整机 UF2。

已留存的证据：

- 2026-10-03 模块 3 严格 ARM 编译、13 组原始算法 C++ ARM 指令模拟（4,147 项检查）、隔离 Arduino 链接和真实 `AppPushSink` 类型适配编译通过。见 [`tests/module3/validation_2026-10-03.json`](../../tests/module3/validation_2026-10-03.json)。该快照明确标记当时未接 App，且未测硬件/游戏。
- 2026-10-04 后续固件 README 与当前 `Acquisition.cpp`/`App.cpp` 已显示两路 SSI 和 PUSH 正式调度、运动路由接入；之前 README/progress 的“未挂正式 App”是较早阶段状态。整机 0.10.1 正式构建及模拟证据见 [`tests/integration/validation_bugfix_2026-10-04.json`](../../tests/integration/validation_bugfix_2026-10-04.json) 与 [`firmware/FluxRail/README.md`](../../firmware/FluxRail/README.md)。该证据证明所记源码的集成编译/软件验证，不证明真实 SSI、机械结构或游戏工作。
- 2026-10-05 IO4 实板复测的 `hello` 读回版本为 0.10.2，测试板设备切换/恢复及同一 SHA 工具协议、超时边界复测按固定报告通过。上传是用户完成，上传行为及具体文件哈希没有被直接观察；测试板为 2 MiB，报告未测试外部传感器/输入/灯、电气状态或游戏。详见 [`validation_io4_0_10_2_actual_2026-10-05.json`](../../tests/integration/validation_io4_0_10_2_actual_2026-10-05.json)、[0.10.2 设备证据目录](../../tests/integration/pc_board_0_10_2_2026-10-05/) 和[回滚归档说明](../../backups/firmware-rollback/0.10.2/20261004T171126137829Z/README.md)。不能把 0.10.1 的 1 kHz 采样实测移作 0.10.2 SSI 实板结果。
- 硬件验收列表 [`tests/module3/HARDWARE_CHECKLIST.md`](../../tests/module3/HARDWARE_CHECKLIST.md) 的 SSI 时序/CRC 实帧、磁场与一圈关系、双路并发、PUSH 误触/漏触和整机预算项仍待执行。CRC 实际初始化约定、最大服务耗时、机械阈值及游戏输入均不可从模拟推断。

## 未实测与接手建议

尚未见真实 24 位 SSI 帧或逻辑分析仪时序，CRC 初始化/首位/时钟边沿仍待确认；也没有实际磁环一圈映射、强弱磁性能、两路最坏服务时长、PUSH 按压/旋转误触率、USB 主机与游戏验收。软件模拟的 2 kHz 周期、合成曲线和隔离链接不等于实板达标。模块级 `validation_2026-10-03.json` 是阶段快照，源码与报告哈希若不匹配，先重跑对应脚本并更新证据，不能沿用快照声称验证了后续源码。

接手先核对 `FirmwareInfo.h`、已构建 ELF/UF2 的元数据和设备 `hello` 读回，再读 [`knobs/README.md`](../../firmware/FluxRail/src/knobs/README.md)、`Acquisition`、`App::serviceCore1/serviceMotion`、`ConfigCoordinator` 与 `MotionRouter`。实际接入代码已经存在；如要调整旋钮行为，沿统一 App 配置/暂停 ACK/运动队列修改，避免另建采样调度或 HID 发送所有者。上板前按硬件检查表留存板型、引脚、传感器模块、磁环/间隙、原始 SSI 帧、固件版本及 UF2 哈希；分别记录角度、场质量、PUSH GPIO/SSI/过滤结果、样本间隔、缺采和队列账本。机械上不能稳定区分真按压和误触时应先调整结构，再评估阈值。

## 原规范与证据入口

- 原模块规范：[模块 3 MT6701 旋钮](../archive/2026-10-05/spec/04_模块3_MT6701旋钮.md)、[公共接口约定](../archive/2026-10-05/spec/01_公共接口与集成约定.md)、[完整配置与 Flash 序列化](../archive/2026-10-05/spec/12_完整配置与Flash序列化.md)、[模块整合运行契约](../archive/2026-10-05/spec/14_模块整合运行契约.md)、[验收与实测计划](../archive/2026-10-05/spec/06_验收与实测计划.md)。
- 验收/历史进度：[模块 3 进度](../archive/2026-10-05/progress/模块3.md)、[整合进度](../archive/2026-10-05/progress/模块整合.md)、[修复进度](../archive/2026-10-05/progress/修复.md)。模块 3 阶段 3 和 10 月 3 日 validation 都早于整机 App 接入状态，作为历史验证边界保留。
- 当前源码与测试入口见上文链接；厂家 SSI/PUSH 电气定义见原规范中引用的 MT6701 Rev1.8，以及 [`tests/module3/HARDWARE_CHECKLIST.md`](../../tests/module3/HARDWARE_CHECKLIST.md)。
