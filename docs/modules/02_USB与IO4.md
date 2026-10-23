# 模块02：USB、IO4 与 CDC

更新：2026-10-05。本文面向接手后的新开发窗口；正式源码以当前 `firmware/FluxRail` 为准，硬件结论按证据日期和范围阅读。整机仍有未完成实测，IO4 软件完成和一次实板往返不等于游戏验收。

## 职责与边界

本模块负责单一 TinyUSB 栈下的普通 USB 人格和 IO4 人格、HID 报告编码/回调、CDC v1 JSON 配置通道、掉线后的状态恢复、显式保存/冷切换/进入 ROM 的 USB 释放时序。普通键盘映射与鼠标动作由公共输入层产生；本模块拥有唯一 USB 后端，但不拥有配置语义、Flash 序列化、霍尔/旋钮算法或网页状态。

当前固件元数据由 [FirmwareInfo.h](../../firmware/FluxRail/src/common/FirmwareInfo.h) 定义：版本 `0.10.2`、固件 ID `FLUXRAIL-FW-V1`、板型 `fluxrail-rp2040-4mib`、格式 1。构建身份是 4MiB/noFS；10 月 5 日实际 IO4 试验板被用户确认为 2MiB，目标板容量验收仍未完成。`docs/spec/00`、`CDC_PROTOCOL.md`、`firmware/README.md` 中的 0.10.0/0.10.1是历史阶段说明，不能覆盖当前头文件和较新的实板记录。

## 硬件、身份与默认行为

| 项目 | 当前固件行为 / 约束 |
| --- | --- |
| 普通人格 | `NORMAL_NKRO`，CDC 配置端点、NKRO 键盘、相对鼠标与绝对 XY 鼠标；普通 HID 描述符沿用 FluxRail 身份。 |
| IO4 人格 | 默认纯 HID、无 CDC；VID/PID `0CA3:0021`、制造商 `SEGA`、芯片唯一序列号。单 HID，IN/OUT端点 64 字节、1ms。`FLUXRAIL_IO4_CONFIG_CDC=1` 只是候选构建选项，未验收。 |
| IO4 字符串 | USB Product 为 92 字符；HID接口板名为 93 字符（含末尾分号），必须保持为两个字段。正式 ELF 回调模拟验证字符串未截断；实际完整主机描述符读取/游戏识别仍未验收。 |
| USB冷切换 | 模式属于保存的启动设置。先设置模式，再显式 `save_config`，最后 `reboot_to_saved_mode`；启动时、USB 首次连接前只注册一个人格。切换不是热换描述符。 |
| 无 CDC 恢复 | 工程恢复键 BT10/source9/GP9：按住接地并复位/上电，强制本次启动为普通模式，不覆盖保存的 IO4 模式；在 IO4 人格中必须靠此入口返回 CDC。首次失败记录保留；第二次实测成功。BOOTSEL 是 ROM 最终恢复入口。 |
| Flash /模式保存 | schema2 双槽位于 4MiB 分区末尾；迁移 schema1 只读、不自动写入。写入必须走公共维护流程，配置与校准只由 ConfigStore 持有。 |

十二来源的 IO4 默认映射为 `[8,6,5,4,7,3,2,1,9,10,0,0]`；它是工程起点，尚非用户确认的实际音击布局。Side 在协议输出层低有效。滑轨只可作为 ADC0 绝对值，不写 spinner；PUSH 两路可映射功能。IO4 INIT/灯位映射的参数单位、位意义与真实游戏行为仍需抓包确认。

## 关键源码与数据流

- [UsbBackend](../../firmware/FluxRail/src/usb/UsbBackend.h) 和 `src/usb/UsbBackend.cpp` 是核心0唯一后端：启动身份选定后注册普通/IO4接口，统一拥有回调分派、端点 busy/completion、物理按钮状态和模式隔离。
- [Io4Codec](../../firmware/FluxRail/src/usb/Io4Codec.h) 与 `.cpp` 编码 IN、严格归一化控制 `SET_REPORT` 与 interrupt OUT 两种形态、解析 INIT/灯命令、映射十二路功能、转 ADC。IN 为 Report ID 1 加 63 字节 payload；OUT 为 Report ID `0x10` 加 63 字节 payload。接受的初始化命令为 `0x01..0x04`，未知、错误长度/ID/type/instance拒绝且不改变状态；超时回到初始化状态。`0x05/PWM` 和 `0x85` 更新命令不支持。
- [NormalHid](../../firmware/FluxRail/src/usb/NormalHid.h) / `NormalHidCodec` 为普通人格持有 HID 报告；`UsbBackend` 把已校验的 Hall 绝对 output 映射到 IO4 ADC0 或普通绝对 XY，并按 revision/epoch/valid 检查。Windows 绝对 XY 枚举与真实按键报告读取的未决问题见下文。
- [CdcTransport](../../firmware/FluxRail/src/usb/CdcTransport.h) 只在核心0访问 CDC FIFO/USB mutex；读写非阻塞，`txComplete()`要求 FIFO 空且真实端点完成。
- [ConfigProtocol](../../firmware/FluxRail/src/protocol/ConfigProtocol.h) / `.cpp` 与 `ConfigProtocolDevice.cpp` 实现 CDC v1、设备级配置和 motion/calibration/启动模式命令；[CDC_PROTOCOL.md](../../firmware/FluxRail/src/protocol/CDC_PROTOCOL.md) 描述协议格式。每行4096字节、384 tokens、嵌套12层；每轮接收最多128字节、发送最多64字节，超长行丢弃至 LF，不解析截断前缀。IO4模式默认不可通过此 CDC 通信。
- [App](../../firmware/FluxRail/src/app/App.cpp) 和 `ConfigCoordinator` 编排暂停/ACK、释放报告、真实 USB 完成、Flash 写入、恢复与冷重启。正常构建 `FLUXRAIL_IO4_CONFIG_CDC=0`。

配置数据流：网页/主机 CDC 一行 JSON → `CdcTransport` → `ConfigProtocol` 校验 revision 和完整 payload → `ConfigCoordinator`/`App` 等双核应用 ACK → 返回设备读回；保存额外经过维护事务与双槽验证。模式切换等候配置已保存、target 与 saved 模式一致、输入已释放、CDC 响应总线发送完成，才 detach 并重启。USB FIFO 已复制不代表主机已收到响应。IO4 数据流：唯一 TinyUSB 回调 → `UsbBackend` → `Io4Codec` 严格解析/序列化 → 主机64字节报告；普通 HID input/运动从相同后端提交并以真实端点完成回执推进缓存。

## 配置协议接手要点

协议文档中的 CDC v1 和完整命令列表仍是接口来源。普通模式先 `hello`，读取 `get_usb_mode`、`get_device`、`get_status` 和完整配置，再以每个成功应答的 `revision` 填写下一次写请求的 `expected_revision`。改模式使用 `set_startup_usb_mode`；保存单独调用 `save_config`；重启需 `reboot_to_saved_mode`。断线后重新枚举并读回身份，不能盲重发已接受的 reboot/save。CDC设备工作能力由 `hello` 声明；`io4_game_verified`/主机能力字段为 false 时不能对外宣称已验证。

IO4指令的工程行为来自本地固定参考及协议交叉核对：启动 `system=0x02`，CMD1/CMD2到 `0x30`，CMD3清回 `0`；CMD1 timeout 单位100ms，timeout=0关闭；CMD2采样计数会保存但报告周期保持1ms；过期回 `0x02`。Side switch 空闲基值 `sw0=0x4000, sw1=0x8000`。ADC0取 Hall 已处理的 `[0,1]` output 转 `0..65535`。这些规则经过软件模拟和本次板上协议验证，但原机精确单位/状态位解释及游戏接受仍待真实抓包/游戏确认。

## 当前测试入口与证据

- `tests/integration/run_io4.py`：IO4编码器、状态机、App/配置/存储及冷切换软件路径；`run_protocol.py`、`run_storage.py`、`run_app_storage.py`覆盖 CDC、存储和维护路径。`run_usb_descriptors.py`运行正式 ELF 的两个人格 descriptor/回调；`compile_firmware.py`执行正式工程构建和源码/链接预算审计。均在集成测试目录的 README 说明中列明。这里只检查既有结果，没有重新运行。
- 正式软件证据：10 月 4 日正式 0.10.2 候选编译通过，38个项目翻译单元严格编译、两人格正式 ELF 描述符执行通过、版本/板型 UF2 元数据识别通过；这是构建和模拟证据，不是主机枚举/实板证明。细节见 [候选摘要](../../tests/integration/validation_pc_board_candidate_2026-10-05.json)。
- 最新板测入口：[10月5日 IO4实际验证](../../tests/integration/validation_io4_0_10_2_actual_2026-10-05.json)，其原始窗口在 `tests/integration/pc_board_0_10_2_2026-10-05/`。用户报告刷入0.10.2，板上 hello 读到0.10.2；刷写过程与UF2哈希未直接观察。用与旧失败相同 SHA 的协议工具复测通过：有效 IO4 枚举；8次真实64字节 interrupt IN；控制/中断 OUT初始化形态；命令状态切换；100/400/25,500ms非零超时窗口均未提前过期，实际在截止后回到 idle；GP9第二轮按住接地启动强制普通成功，随后保存 NORMAL 并重启，配置原样恢复。最终NORMAL身份下active4/revision=applied=saved11、dirty=false；完整schema相对原基线只差revision字段，4个profile完全恢复。该流程包含保存IO4及NORMAL启动模式，不是Flash掉电或4MiB容量验收。
- 局限：测试板被用户确认为2MiB，跳过了实板容量读取；正式目标4MiB的Flash分区/掉电保存仍未在目标板验收。只完成一次 IO4 进出流程，未完成规范建议的20轮；未实测完整IO4物理描述符/游戏、外部开关灯/传感器、游戏识别、电气健康、长时间高 IRQ 负载。对普通 HID 集合尝试 Windows `HidD_GetInputReport` 读取当前状态均失败（Win32 error 1）；没有捕获设备返回的报告 payload，根因未明。这不等于PnP枚举或普通 CDC 失败。IO4游戏兼容仍未验证。

## 已知冲突与接手建议

当前事实优先顺序：`FirmwareInfo.h`给当前源码版本；10月5日 `validation_io4_0_10_2_actual_2026-10-05.json`及原始记录给最新受限实板结果；10月5日 `validation_pc_board_candidate_2026-10-05.json`只描述此前未刷写的候选阶段；旧 progress/spec/README 的0.10.0/0.10.1句子是当时状态。最新IO4证据覆盖了此前“候选未刷”“IO4 timeout实板缺陷尚未关闭”的判断；旧0.10.1 timeout失败仍须保留为历史，不得当作当前结果。

接手先检查当前源/UF2和回滚材料身份，再分别复核IO4实板模式识别、control/interrupt OUT、超时边界和无 CDC 返回；确定测试板/目标板容量和存储布局后再做最终Flash/掉电验收。然后用实际目标游戏关闭软件 hook，验证设备被真实打开、功能映射/Side极性/ADC轴有效范围、模式往返与冷启动。修改IO4协议或USB初始化后，重跑相应的 C++ 测试、正式构建和 ELF 描述符审计，并新增日期独立证据；不要覆盖 10月3日软件快照或10月5日实板记录。

## 规范和来源

- 原规范（历史归档）：[需求索引与当前结论](../archive/2026-10-05/spec/00_项目需求与决策.md)、[公共接口](../archive/2026-10-05/spec/01_公共接口与集成约定.md)、[IO4兼容/冷切换](../archive/2026-10-05/spec/09_IO4兼容与USB冷切换.md)、[IO4外部项目对比](../archive/2026-10-05/spec/10_IO4外部项目对比.md)、[Flash序列化](../archive/2026-10-05/spec/12_完整配置与Flash序列化.md)、[IO4软件契约](../archive/2026-10-05/spec/13_IO4软件契约.md)、[整合运行契约](../archive/2026-10-05/spec/14_模块整合运行契约.md)、[固件版本与UF2识别](../archive/2026-10-05/spec/15_固件版本与UF2识别.md)、[Arduino USB审核](../archive/2026-10-05/spec/07_旧项目审查与来源.md)。旧 IO4 专项交接：[FluxRail_IO4专项_开发交接.md](../archive/2026-10-05/handoffs/FluxRail_IO4专项_开发交接.md)。
- 当前实现说明：[固件README](../../firmware/FluxRail/README.md)、[CDC协议](../../firmware/FluxRail/src/protocol/CDC_PROTOCOL.md)、[IO4编码接口](../../firmware/FluxRail/src/usb/Io4Codec.h)、[USB后端接口](../../firmware/FluxRail/src/usb/UsbBackend.h)。
- 固定第三方对照快照：[references/io4/README](../../references/io4/README.md)、[本机 Arduino USB 审核快照](../../references/arduino-usb-audit/README.md)。参考快照不是FluxRail依赖或成功实板基线；ESP32-S2项目/主机库/segatools仅用于协议交叉核对，旧 `failed-lkick` 是用户已验证失败的历史样本。



## 2026-10-11 协议与 IO4 网页口径
- **模板名 UTF-8（固件已改，未 ARM 编译/实板）**：CDC v1 仍为纯 ASCII 单行；`create_profile`/`rename_profile` 的 `name` 以 JSON `\uXXXX`（含代理对）传输，固件 `Json::utf8Value` 解码为 UTF-8，`ProfileManager::validName` 校验（1～3 字节序列、≤31 字节，拒绝控制/DEL/C1、零宽与双向控制、引号、反斜杠），回复时 `JsonWriter::name` 再转义。名称槽仍 32 字节，schema/Flash 布局和 `set_profile` 不变；旧 ASCII 名全部合法。源码：`protocol/Json.*`、`protocol/ConfigProtocolDevice.cpp`、`config/ProfileManager.cpp`。证据：[主机 96 项](../../tests/integration/utf8_name_2026-10-11/README.md)。固件版本已升到0.10.3（见[升级日志](../固件升级日志.md)），正式编译与刷机由用户执行。
- **IO4 网页**：界面只显示 BT1～BT10；P1/P2（source id 10/11）的映射不显示，保存时原值带回（用户2026-10-11确认：这两个PUSH按键硬件暂时取消）。固件 IO4 功能表仍是 12 项，未改。
