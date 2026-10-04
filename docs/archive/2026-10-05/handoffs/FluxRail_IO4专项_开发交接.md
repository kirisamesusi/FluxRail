# FluxRail IO4专项开发交接

整理日期：2026-10-03。工作目录：C:/Users/admin/Documents/FluxRail。

此文保留阶段8接手任务与历史基线。阶段9已完成IO4软件实现及单固件双向冷切换，当前状态以[IO4进度](../progress/IO4.md)、[软件契约](../spec/13_IO4软件契约.md)及[验证快照](../../../../tests/integration/validation_io4_2026-10-03.json)为准；下文“当前没有/尚未”属于接手时历史口径，实板/游戏仍未验证。以接手时源码和最新进度为准，保留其他窗口修改。实板、上传、游戏配置及游戏验证按用户安排暂缓，不阻塞可独立完成的软件工作。

## 1. 阅读顺序

1. [统一开发入口](FluxRail_开发交接_窗口必读.md)，按入口读主需求、公共契约、硬件及工程组织。
2. [模块1进度](../progress/模块1.md)：阶段8完成项、末尾下一步和用户灯链确认；阶段7的RAM-only限制已由阶段8更新。
3. [正式固件README](../source-readmes/firmware/FluxRail/README.md)。
4. [IO4兼容与冷切换审核](../spec/09_IO4兼容与USB冷切换.md)、[外部来源对比](../spec/10_IO4外部项目对比.md)。它们是历史取证和实现要求，不是当前IO4已实现证明；普通启动适配已在后续阶段完成。
5. [完整配置/Flash schema1](../spec/12_完整配置与Flash序列化.md)、[当前CDC协议](../../../../firmware/FluxRail/src/protocol/CDC_PROTOCOL.md)。
6. 本地references/io4/README.md、SOURCES.json和references/arduino-usb-audit；需要时再看[旧失败实现审查](../spec/07_旧项目审查与来源.md)。
7. 模块2/3最新README及进度只用于公共接口适配。用户说明其代码仍在写，可先完成IO4编码、状态机、身份和模拟，不要求等待完整采集接入，也不改其业务算法。

## 2. 当前可复用基线

| 已完成的软件 | 接手边界 |
| --- | --- |
| 十BT扫描、十二路绑定、普通NKRO、每个物理DOWN的同usage重触发、键灯/模式灯 | 正式App目前只采十BT；P1/P2过滤输入入口已有，实际模块3尚未调度 |
| 双核PauseApply/ACK、旧流清理、全释放/状态重建、Resume/ACK | 目前运行应用的是模块1投影，不能声称Hall/knobs已参与完整运行ACK |
| 四预置+八自定义、稳定ID、完整DeviceConfig和全局机械校准 | ID1=SDVX/2=ONGEKI/3=IN FALSUS/4=MULTI K；USB身份与活动模板分开 |
| 显式save、独立双槽、启动完整读取、CRC/语义/读回验证 | startup_usb_mode字段已存在，但当前仅允许0=NORMAL_NKRO |
| CDC配置/模板CRUD、二进制快照分块导入导出 | IO4、bootloader、校准任务和运动能力仍false |
| 固定项目TinyUSB启动适配、唯一USB回调和严格构建审计 | 只支持普通CDC+两个HID；不要再建第二套USB栈/回调 |

当前没有普通滑轨绝对X输出、Hall/knobs正式App调度、运动来源路由或IO4后端。保存的校准能开机读回，但尚未调用HallModule实际加载/采样。

阶段8基线：48组、53,591项ARM真实C++检查通过；正式编译/链接通过，Flash119,516字节、全局RAM80,248字节、最大单函数静态帧1192字节。快照：[validation_storage_2026-10-03.json](../../../../tests/integration/validation_storage_2026-10-03.json)。UF2 SHA256=d80e6f222356e58cb8cd43b9e5e184ca133216f648e7669285358e9db7949857，未上传。接手后有源码变化必须重新构建和生成新快照，不能继续引用旧产物证明新实现。

## 3. 最关键的源码入口

以下路径均相对项目根目录。

| 文件/目录 | IO4窗口需要处理的关系 |
| --- | --- |
| dependencies/prepare_tinyusb.py | 固定上游SHA，生成dependencies/generated/Adafruit_TinyUSB_Arduino；受控适配必须写进生成脚本及清单，不只编辑生成结果 |
| src/usb/NormalHid.cpp（位于firmware/FluxRail下，下同） | 当前拥有fluxrail_usb_configure_before_connect和HID complete/failed项目回调；库持有descriptor/get/set强回调。新身份须统一分派，不能重定义冲突 |
| src/usb/NormalHid.h、NormalHidCodec.* | 普通29字节NKRO、5字节相对鼠标；默认CFG_TUD_HID=2。不得破坏普通模式的现有可靠性 |
| src/app/App.* | 当前硬编码normalHid：启动检查、连接、握手sink、释放完成检查、错误恢复及输出；IO4不能只新增类却不接实际App |
| src/app/ConfigCoordinator.*、src/realtime/ConfigMailbox.h、InputWorker.* | 公共暂停/恢复与唯一输入生产者；适配IO4释放/重建语义，不另造一套跨核配置所有者 |
| src/common/DeviceConfig.h、src/config/ProfileManager.*、DeviceCodec.* | 唯一完整配置/校验/逐字段格式；当前没有IO4功能绑定表，keyboard usage不等于IO4 bit |
| src/config/ConfigStore.*、FlashRp2040.* | 唯一双槽存储及维护写入，复用完整验证/最新generation选择，不新建EEPROM模式字节 |
| src/protocol/ConfigProtocol.*、ConfigProtocolDevice.cpp、CDC_PROTOCOL.md | 新增实际支持的模式命令/能力/当前与启动身份状态；保留expected_revision、异步确认及分块协议 |
| src/lighting/*、src/board/BoardConfig.h | GPIO灯由公共层驱动，IO4 OUT只提交有界灯状态/命令；模式色仍待用户选择 |
| tests/integration/compile_firmware.py、finalize_storage_report.py | 固定分区、依赖、唯一回调、严格编译、栈帧和源哈希审计；新报告不得沿用写死io4_implemented=false或虚报true |

### 必须修正的启动时序

本机Arduino-Pico6.1.0在setup前调用TinyUSB初始化。现有私有钩子位于clearConfiguration之后、**默认SerialTinyUSB.begin之后**、第一次TinyUSB_Port_InitDevice连接之前。

App::beginCore0目前在setup阶段才store_.load(device_)，对首次USB身份选择已经太晚。必须在首次连接前以有界只读流程取得经过完整槽/CRC/schema/语义校验的保存模式，并保证后续App读取与之使用同一有效快照。无有效配置或不可识别模式回普通。启动读取不能调用擦写、核心1暂停、等待CDC或机械校准，也不能在小栈上临时放完整DeviceConfig/ConfigStore大对象。

若选择纯HID IO4，需要受控改变默认CDC注册位置/条件；当前钩子本身不提供无CDC人格。32字符字符串缓冲也必须处理，尤其长HID接口板名。保持一个USB所有者，并把上游源码校验、生成补丁、构建选项及manifest一起更新。不能手改系统安装库，不能在setup先枚举普通再换IO4。

## 4. IO4协议与身份最低实现范围

先用本地固定commit快照对照，再独立实现；参考文件不是可直接刷入的RP2040工程。旧failed-lkick已被用户实测不可用，只作失败取证。主机SEGA835Lib/segatools可交叉核对，ESP32参考不构成RP2040成功证据。

- 候选兼容VID/PID为0CA3:0021；实现完整device/configuration/report/string，不仅替换标识。HID接口长板名与USB product字符串各自位置必须正确。
- 本地ongeeki-firmware/main/src/usb/desc.hpp的HID接口板名参考为：
  `I/O CONTROL BD;15257;01;90;1831;6679A;00;GOUT=14_ADIN=8,E_ROTIN=4_COININ=2_SWIN=2,E_UQ1=41,6;`
  这是参考识别串，不证明当前设备具备全部外围功能；不要照抄其固定serial、AIME READER、MSC驱动盘、PN532或额外灯板。
- IN：Report ID=1，payload63字节；ADC8×u16、spinner4×u16、coin2×u16、switch2×u16、system/usb各u8、reserved29字节；总传输64字节。
- OUT：Report ID=0x10，cmd u8+payload62字节；总传输64字节。TinyUSB sendReport带ID时只传63字节payload，避免65字节。
- 所有字段显式little endian、安全逐字段编码/解码；尺寸static_assert，未使用字段取明确值。
- 控制SET_REPORT归一化：id=0x10，通常63字节从cmd开始；中断OUT归一化：id=0，64字节首字节0x10。按实际固定TinyUSB路径核对；严格检查instance/type/id/长度，非法数据拒绝并统计，不能盲reinterpret_cast。
- 实现并模拟0x01通信超时、0x02采样计数、0x03清状态的状态演变；0x04 GPIO灯输出核对位序与物理灯映射。0x05/PWM及0x85原机更新明确支持或拒绝；0x85不能自动进入FluxRail UF2。
- GET_REPORT须按已实现报告/请求长度返回一致当前状态，不能一律0却声明兼容。初始化细节有分歧时记录来源、采用的工程行为和待抓包项，不把参考0x30/0当所有原机状态保证。
- USB忙则有界保留，提交/完成/失败/断连计数可诊断；断连不补历史按键或旋钮动作。

### 功能映射与绝对轴

十二物理来源BT1..10/P1/P2具有固定source_id0..11。另定义IO4功能绑定，不能复用A/Space等键盘usage直接写switch。BT到音击功能的物理摆位尚未确认，允许可配置和明确工程起点；不要宣称用户已确认默认布局。

segatools参考：左A/B/C=sw[0]bit0/5/4，右A/B/C=sw[0]bit1/sw[1]bit0/sw[0]bit15；左/右Menu=sw[1]bit14/sw[0]bit13；左右Side为低有效。电气低有效与协议极性分层处理，释放重建必须保留正确的低有效空闲位。Test/Service留独立映射。

滑轨只向ADC[0]提供绝对当前位置，复用全局机械校准，不把它变相对量或同时写spinner。主机参考ADC0=0x7FFF-lever、右到左增加；原始uint16 ADC域与有符号lever域分开，范围/反向/中心及失磁策略明确，最佳游戏范围仍待实测。模块2未稳定时可用明确标记的输入替身模拟边界，不在正式固件中把替身当实际采集。

音击两旋钮默认关闭；IO4 spinner用途未冻结，不能自动把普通相对鼠标语义搬过去。普通模式继续保持滑轨绝对、旋钮相对以及未来绝对X不得反复重置Y的要求。

## 5. 配置扩展、保存和双向冷切换

当前schema1只接受startup_usb_mode=0，Profile中也没有IO4功能绑定。下个窗口先冻结唯一扩展类型和迁移契约，同步公共接口、二进制/CDC契约和接入样例，然后实现。新增绑定字段须新schema和明确schema1→新schema迁移；旧配置的四预置/自定义ID、next_id、活动模板、参数与全局校准不得丢失。未知未来schema仍安全拒绝。不要悄悄改变schema1字节顺序或校验语义。

继续复用slot A=0x3FB000/B=0x3FD000，各8192字节、独立4KiB擦除扇区，最后EEPROM0x3FF000不使用；payload上限7680，no FS。Flash保存必须是全部模板/活动ID/启动身份/全局校准的完整已确认一致快照，只显式save写入。

建议沿既有协议加入get_usb_mode、set_startup_usb_mode、reboot_to_saved_mode，名称/字段最终同步CDC契约。区分：
- 当前实际USB身份：本次启动选择，运行中不热换。
- RAM中请求的启动身份：临时预览，可能dirty。
- 已保存的启动身份：经完整读回确认，下次启动采用。

先设置RAM启动身份→用户显式save且完整维护成功→受控请求重启到保存身份。保存失败、恢复失败、响应未完成、请求目标与保存值不符均不得直接切换。重启流程释放旧键鼠/IO4开关、暂停输入与运动、等待实际传输完成及CDC应答送达、detach、有界间隔、应用重启。协议TX数据复制完不等于总线传输完成，两个阶段须区分。超时/断连的结果未知时给出读回/恢复路径；真正断开间隔仍待主机实测。

IO4是否带配置CDC尚待目标兼容性验证；软件可先实现最小纯HID及明确独立的CDC候选方案，能力如实声明，不能把AIME端口当配置口。如果无CDC，必须提供无需网页的应用启动强制普通恢复入口。启动键号待用户选；可做参数化策略并记录工程起点，只在启动判定，不能沿用游戏中BT1+BT8长按。ROM BOOTSEL进入UF2是最后恢复路径，不等于返回普通应用。恢复强制普通不应隐式覆盖已保存模板/校准。

Flash/配置/协议/USB所有者仍唯一。维护暂停/确认/恢复、双核健康和断连恢复要覆盖两身份；不要让IO4模式仍等待未注册的普通keyboard/mouse端点而无法ready或save。IO4主机OUT与本地灯设置优先关系须明确记录；未定模式灯颜色沿现有工程起点，不替用户做产品确认。

## 6. 推荐的软件推进顺序

1. 补齐冻结的IO4值类型、功能映射、报告codec、OUT归一化和初始化状态机，先用真实C++离线边界测试。
2. 扩展项目私有TinyUSB生成适配、早期只读模式选择及长字符串，两个身份均在首次连接前一次配置完成。
3. 让App/ConfigCoordinator使用统一USB后端，接十二输入/灯命令、维护与恢复；模块2/3未完成时保留清楚的实际能力边界。
4. 实现新配置迁移/CDC模式管理、显式保存与双向冷重启/启动恢复入口。预览不写Flash，失败不误切。
5. 审查、模拟、正式构建、唯一回调/分区/栈与源哈希审计，更新公共契约、协议、集成样例、模块1进度及IO4专项进度/验证快照。
6. 实板恢复后再执行枚举、初始化、冷切换和游戏验收；软件阶段交付不等待本轮禁止的设备操作。

## 7. 软件验证和后续实板门槛

必要模拟包括：IN/OUT精确字节金样；控制/中断两路径一致；全部错误长度/id/type/instance；GET_REPORT请求长度；初始化次序/超时/清状态；Side空闲/按下/释放极性；十二路映射与并发；ADC左中右/反向/范围/失效；USB背压/完成/错误/断连；两身份维护释放/恢复；模式保存中断/坏槽回退；schema1迁移及未知schema；启动选择在首次连接前、无默认CDC泄漏；无CDC返回普通；重启失败和应答未完成禁止切换。

沿用基线复现命令，从项目根目录运行：

~~~powershell
python tests/module1/run_tests.py
python tests/integration/run_tests.py
python tests/integration/run_transport.py
python tests/integration/run_protocol.py
python tests/integration/run_storage.py
python tests/integration/run_app_storage.py
python tests/integration/compile_firmware.py
python tests/integration/finalize_storage_report.py
~~~

另增加IO4真实C++测试和相应快照；finalizer随新schema/能力/源码同步，不能为通过验证删掉检查。固定CLI0.35.3/Pico6.1.0/TinyUSB3.7.7/ARM GCC16.1.0/Generic RP2040/4MiB noFS/W25Q64JV /4/125MHz。build.flash_length及upload.maximum_size=4173824，FLUXRAIL_STORAGE_PARTITION=1。所有回调单一强定义、核心1入口/两核健康、每函数静态栈帧≤2048字节继续检查；该检查不证明实板运行栈峰值。

若shell仍出现“Failed to create unified exec process: pin C:\”，之前通过可用Node文件/子进程运行时调用固定Python与ArduinoCLI完成构建；这是工具启动问题，不应绕过项目依赖或验证。

编译/模拟只能标为软件完成。后续实板应保留完整描述符导出、Windows设备状态、USBPcap/实际主机初始化日志；每身份冷上电≥20次，普通→IO4→普通≥50轮，不重刷/不靠拔插补救；复位/断电/睡眠/换口/保存异常/无CDC物理返回，以及所有BT、Side、滑轨、灯输出均逐项记录。主机hook自身模拟IO4时，游戏有输入不证明物理IO4被打开。本轮不上传、不刷第三方固件、不修改游戏配置。

## 8. 不得回退及仍待决定

- 十二路绑定、普通NKRO、每次物理DOWN同usage重触发继续保留。
- 滑轨绝对、旋钮相对，普通绝对X不反复重置Y；校准全局共享、保存后开机复用。
- LED链已确认：BT9→BT8→BT7→BT6→BT5→BT4→BT3→BT2→BT1→BT10；现有数组一致，RGB/GRB仍待实板。
- 音击两旋钮默认关闭；不加读卡/投币/额外灯板等用户未要求外围。
- 不改模块2/3业务算法和模块4网页；通过公共层适配，保留其他窗口修改。
- 重复鼠标按钮策略、默认灯效、模式灯颜色尚待用户选择，沿记录中的工程起点。
- IO4默认BT功能布局、配置CDC兼容性、无CDC启动恢复键、OUT灯优先策略、轴最佳范围仍需明确；可独立软件实现的部分先推进，不能把待实测伪装成已验证。

## 9. 可复制给下个窗口的任务

> 负责FluxRail IO4专项及单固件普通/IO4双向冷切换，工作目录C:/Users/admin/Documents/FluxRail。先读docs/FluxRail_IO4专项_开发交接.md并按其顺序阅读公共入口/契约、阶段8进度、固件README、IO4专题及本地固定参考。复用现有完整DeviceConfig、双槽Flash、两核维护握手、CDC和固定项目TinyUSB适配，不重新实现已有功能。优先完成IO4报告/OUT归一化/初始化状态机、完整身份与长接口字符串、首次USB连接前的已保存模式读取、唯一后端分派、功能映射和配置迁移、显式save后双向冷重启及无CDC恢复入口。模块2/3仍在写，可先使用明确边界适配和离线测试，不改其算法或网页。普通NKRO、十二路绑定、同usage每物理DOWN重触发、滑轨绝对/旋钮相对、共享校准、音击旋钮默认关闭和已确认LED链序不变。不上传、不改游戏配置，实板暂缓；每项做代码审查、必要真实C++模拟和正式构建，同步公共契约/CDC协议/样例/模块1进度及IO4验证快照，分别报告编译、模拟、实板和游戏状态，不把IO4游戏兼容性声明为已验证。
