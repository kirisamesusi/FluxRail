# IO4专项与单固件双向冷切换进度

2026-10-03，北京时间。按用户任务完成阶段9软件范围；实板暂缓、不上传、不改游戏配置。接手按IO4交接→公共入口/契约/阶段8→固件README→专题09/10/12→固定本地参考顺序阅读。references/io4的30份固定来源哈希已核对；ESP32/旧失败样本只作取证，不作为RP2040成功证据。模块2/3仍在开发，未改其算法/网页/已确认灯链。

## 交付状态

| 项目 | 软件完成与审查 | 尚待验证 |
| --- | --- | --- |
| IO4 IN/OUT/状态机 | Io4Codec显式LE、ID/长度/type/instance严格归一化，IN/descriptor金样，0x01/02/03/04与超时；拒绝PWM/原机更新 | 原机初始化抓包、参数单位/位意义、主机控制/中断OUT |
| 身份和长字符串 | 0CA3:0021、SEGA、真实serial；product92、接口93字符分别存储；UTF-16上限126；真实正式ELF回调未截断 | Windows/游戏识别、目标端点资源与枚举 |
| 首次连接前选择 | 固定Device.begin钩子在默认CDC前完整只读ConfigStore；setup复用同一快照，首次Port连接前注册单一人格 | GPIO启动稳定性、真实上电/复位时序 |
| 唯一USB后端 | UsbBackend分派普通或IO4；库descriptor/get/set回调保留唯一所有者；实际1或2HID端点维护完成检查 | USB并发/IRQ/物理完成捕获 |
| 功能/绝对轴边界 | 每模板独立十二功能表、Side低有效，ADC0绝对0..1转换；普通Mapper语义保留 | 音击实体布局、模块2真实位置/共享校准加载，旋钮用途 |
| 配置迁移/Flash | schema1逐字段完整迁移→schema2，所有旧字段/模板ID/校准保留，不自动写；现有两槽/CRC/commit/维护复用 | 实板Flash/掉电/真实双核锁定 |
| 显式双向冷切换 | set mode→save→reboot；未保存/目标不符/保存失败/断连/超时拒绝；应答FIFO复制与总线完成分开；detach后250ms应用重启 | 真实断开间隔、重复双向切换、主机重连 |
| 无CDC恢复 | 默认纯IO4；BT10/source9/GP9启动时强制普通且不改保存配置，再普通save/reboot | 工程键号待产品确认、按键/上电电气实测 |

普通NKRO、十二路绑定与同usage每物理DOWN重触发继续原Mapper；全局校准、音击两旋钮默认关闭、最终键灯127/指示63限幅及BT9→8→7→6→5→4→3→2→1→10链序不变。普通滑轨绝对/旋钮相对是保留需求；正式App仍未调度Hall/Knob，普通绝对X仍未实现，不能宣称该部分集成完成。IO4只提供有revision/epoch/valid/finite校验的绝对值边界；测试替身没有进入正式固件。

## 默认、契约与模块影响

- 启动模式0=NORMAL_NKRO/1=IO4，本次启动固定；当前/requested/saved分别读回，模板与身份独立。FLUXRAIL_IO4_CONFIG_CDC=0为已编译候选；=1仅独立配置通道工程候选，本轮未构建/实测其描述符，也未游戏验证。
- 默认IO4表[8,6,5,4,7,3,2,1,9,10,0,0]，BT8/7/6左ABC、BT4/3/2右ABC、BT5/1左右Menu、BT9/10左右Side；P1/P2禁用。此表为可编辑工程起点，不是用户确认的音击物理摆位。Side空闲sw0=0x4000/sw1=0x8000，Test/Service独立功能。
- 初始化采用固定本地参考工程行为：初始0x02，cmd1/2→0x30，cmd3→0；cmd1参数100ms/单位，0关闭超时；cmd2保存计数但固定1ms报告，不伪装多样本平均。cmd4主机灯只覆盖六ABC功能BT，断连/超时/配置/维护清理。位序/主机优先级仍待实测。
- schema2每旧329字节Profile尾新增12 u8→341，默认1387字节/CRC894450391，最大12模板＋17节点4651字节。schema1金样和旧格式保留，槽header/payload schema必须一致；未知版本拒绝。保存唯一完整DeviceConfig，不新造EEPROM模式字段。
- [公共v0.8](../spec/01_公共接口与集成约定.md)、[IO4软件契约](../spec/13_IO4软件契约.md)、[CDC协议](../../../../firmware/FluxRail/src/protocol/CDC_PROTOCOL.md)与[样例](../../../../tests/integration/io4_examples.jsonl)同步；模块4需适配schema2/io4_bindings和模式状态，网页本轮不改。模块2/3交值对象边界不改算法，正式采集/完整运动ACK另阶段。

## 逐项代码审查与修正

1. 报告按字段编码，63 payload只由库前置ID，避免65字节；所有错误尺寸/ID/type/instance无状态修改。未用字段明确初始化，Side空闲和多来源OR持有正确；GET_REPORT一致截断，非法命令统计拒绝。
2. 普通/IO4 complete/failed集中分派，强符号审计唯一；忙时不消费边沿或推进序号。断序/传输故障触发公共恢复，断连只重建当前物理状态。mutex忙时清轴/灯/断连登记待办，profile确认返回成功后握手才继续，detach同mutex确认端点空闲。
3. 启动加载仅一次完整ConfigStore读取到成员工作区，后续App复用，不等待CDC/核心1、不擦写、不在小栈放完整DeviceConfig。恢复仅覆盖实际本次人格，保留保存模式。无有效槽普通默认。
4. 迁移逐字段保持ID/next_id/活动/所有motion/校准/revision，功能表独立；旧set_profile与runtime修改保留IO4表。header/payload版本一致验证，失败退旧完整槽；commit最后写与旧槽保留不改。
5. 保存/重启使用既有两核维护；IO4只等一个实际注册端点，普通两端点完成。重启必须完整revision已保存且目标匹配，保存/恢复失败禁止切换；CDC应答必须FIFO空且总线端点idle，不能用write成功冒充送达。断连/超时恢复公共输入状态，250ms间隔非阻塞。
6. 对照固定SEGA835Lib product解析与HID接口原串，区分92/93字符尾分号；正式ELF真实回调验证长串、纯HID无默认CDC泄漏、普通完整接口与serial转换。私有适配生成器固定三份上游SHA并记录生成哈希，系统安装库不改。

## 验证证据（分别报告）

| 层级 | 当前结果 |
| --- | --- |
| 正式编译/链接 | 通过：CLI0.35.3/Pico6.1.0/TinyUSB3.7.7，固定4MiB/noFS/125MHz及双槽分区；36项目翻译单元严格编译，项目warning=0；11 USB强符号唯一，Flash126,428/RAM81,836字节；静态单函数最大1192字节，预算2048 |
| 真实C++ ARM模拟 | 通过：7套共65组120,703检查，全部passed/completed，前后源哈希一致；USB/Flash/GPIO/时钟/外设仅边界替身，不模拟总线/真实双核 |
| 正式ELF描述符执行 | 另2人格通过：真实构造与回调，普通CDC＋两HID、IO4纯单HID/两64字节1ms端点、完整长串、平台serial转换及首次Port前顺序；mode/Port/ROM/unique-ID硬件边界替身 |
| 实板/主机 | 未上传、未实测；Flash掉电、USB枚举/Web Serial、GPIO灯位序/RGB或GRB、双核/PIO/DMA/延迟/机械均未验证 |
| 游戏 | 未验证，IO4游戏兼容性未验证；未改游戏配置 |

各套组数/检查：module1 9/45,343；integration 8/2,021；transport 2/47；protocol 9/1,320；storage 17/5,001；app_storage 3/23；io4 17/66,948。IO4套包含三组App fixture回归重复执行，合计是实际执行次数，不代表65个互不重复场景。新IO4覆盖OUT全尺寸/ID/type、65,536 ADC边界、12路极性/映射、初始化/超时回绕、busy/GET/灯限幅、真实App维护、保存模式启动/强制普通、schema1全12模板/有效校准迁移/混合槽、保存中断身份保留、CDC应答门槛、失败与超时不切换。

固定快照：[validation_io4_2026-10-03.json](../../../../tests/integration/validation_io4_2026-10-03.json)，含当前源码/依赖/脚本/测试/样例/UF2哈希及描述符完整字节；UF2 SHA256=eb5cf416b5d4ae504ec1abe93f72bc5418da8437c32af02a3ab00137f2a5d687，未上传。阶段8validation_storage及更早快照保留；新源不引用旧产物证明。复现命令见[正式README](../source-readmes/firmware/FluxRail/README.md)，旧finalize_storage_report.py现委托IO4快照生成且不覆盖历史存储JSON。

## 后续边界

模块2/3稳定后由公共层接真实采集/配置ACK/校准任务/诊断/运动路由，复用共享校准。实板恢复后先按[验收入口](../../../../tests/integration/HARDWARE_CHECKLIST.md)分别验证纯IO4/普通首次枚举与长串、INIT/OUT捕获、≥20次双向冷切换、≥50次维护并发和故障/掉电路径；这些次数目前未执行。最后按用户安排做游戏兼容/轴范围与位序验收；任何模拟结果均不替代实板或游戏证据。
