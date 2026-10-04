# FluxRail 正式整合固件

2026-10-04。唯一入口FluxRail.ino，模块1～4已接入统一App/USB/配置/协议。当前0.10.1为Hall与绝对XY修复软件候选，尚未刷写；历史0.10.0实板有844Hz/Code10问题。整机复现步骤见[RUN_INTEGRATED](../../../root/RUN_INTEGRATED.md)，当前修复事实见[修复进度](../../../progress/修复.md)，历史整合记录见[模块整合进度](../../../progress/模块整合.md)。

## 构建

~~~powershell
python tests/integration/run_all.py
~~~

固定Arduino CLI0.35.3、Arduino-Pico6.1.0、ARM GCC16.1.0、TinyUSB3.7.7项目启动适配；Generic RP2040 / 4MiB noFS / W25Q64JV /4 / 125MHz。build.flash_length和upload.maximum_size均4173824，FLUXRAIL_STORAGE_PARTITION=1、FLUXRAIL_IO4_CONFIG_CDC=0。产物tests/integration/build/arduino/FluxRail.ino.uf2/.elf，当前0.10.1正式构建Flash152724字节/RAM107220字节，38项目单元严格编译warning0，11 USB强符号唯一，最大单函数静态帧1200/预算2048字节，不代表真实栈峰值。核心1保留8KiB栈。

prepare_tinyusb.py先验证三份上游SHA，再生成项目私有库。Device.begin在clearConfiguration后、默认CDC注册前选择已保存身份；唯一configure钩子注册对应HID，最后首次Port_InitDevice。适配宏2、防阻塞CDC FIFO/实际端点完成查询、126字符UTF16上限保持，安装库不修改。普通注册CDC，IO4默认不注册。

## 运行所有权

| 路径 | 职责 |
| --- | --- |
| app/App | 唯一双核入口，核心0 USB/配置/灯/维护，核心1真实输入与采样 |
| app/Acquisition + common/MotionTypes | 活动Profile和全局校准的唯一采集投影，模块队列/低频值复制/校准命令 |
| hall / knobs | 原模块定位、校准、相对运动、PUSH过滤；业务算法不修改 |
| realtime/InputWorker/ConfigMailbox | 500us十BT扫描，唯一可靠十二路边沿；PauseApply/Resume实际采用ACK |
| app/ConfigCoordinator | 暂停ACK→清旧流→释放重建→USB真正完成→采集应用/Resume ACK |
| app/MotionRouter | revision/epoch过滤、活动/静止来源接管、轴独立、分段及正反小数余量、背压 |
| usb/UsbBackend/NormalHid/Io4Codec | 同一后端/栈/回调，普通和IO4分派，运动端点完成与按键所有权 |
| config/ProfileManager/DeviceCodec/ConfigStore/FlashRp2040 | schema2完整模板/全局校准、显式双槽保存及唯一Flash所有者 |
| protocol + usb/CdcTransport | CDC v1 bounded JSON，RX128/TX64每轮，唯一4096行/384token/12层协议 |
| lighting | 既有双PIO/双DMA异步灯，约60Hz，统一亮度/关闭/127与63限幅 |

核心1十BT500us、Hall三ADC1000us、双SSI各500us，不补采过期周期。GP26/27/28 ADC；U4 SPI1 RX12/CS13/SCK14/PUSH19；U5 SPI0 RX16/CS17/SCK18/PUSH20，没有MOSI。PUSH过滤后source10/11进入同一InputWorker，原始GP19不被SPI发送占用。采集快照20Hz，网页串行轮询约10Hz，核心0不直接读核心1可变对象。高频记录复用模块已有SPSC队列。

## 运动、校准与恢复

普通NKRO实例仍29字节无ID。第二HID实例：ID1三按钮+signed16相对X/Y（5字节payload）；ID2标准Mouse/Pointer集合，按钮字节恒0、unsigned16绝对X/Y各0..32767（5字节payload），均加1字节Report ID。真实按钮只由ID1持有，ID2提供中性兼容usage，实际Windows按钮交互仍待验证。Hall不积分、不二次反转。Hall启用时NormalHid保留逻辑XY，初始均16384；Hall只替换X，Y旋钮增量加入保留Y，Hall未校准也不伪造Hall输出。旋钮每count=32逻辑单位，约1/1024屏幕跨度，默认1024counts/圈约一屏；范围夹紧且到边界后反向立即生效。Hall关闭时旋钮继续ID1相对count。缓存仅发送成功后更新；配置/维护/断连不重置Y，新启动重新初始化。外部鼠标改的系统位置不回传，下一绝对报告恢复本设备逻辑Y，无主机助手。

IO4仍0CA3:0021、SEGA、唯一芯片serial、product92字符/HID板名93字符含尾分号；IN ID1+63、OUT ID0x10+63和0x01/02/03/04保持。Hall output直接映射ADC，不生成spinner，旋钮PUSH作为十二路来源功能；两旋钮音击默认关闭。游戏兼容与工程功能布局未实测。详见spec/13。

无有效全局校准时Hall不输出但ADC诊断可用。共享校准通过统一命令：start先暂停/释放/恢复握手，之后仅Hall抑制输出，其他来源继续；空场、三个参考、7～17实际毫米标签、MapCheck、全轨扫、finish。候选在Hall实际采样边界采用后复制唯一DeviceConfig、递增revision/generation，再等待完整ACK；失败/取消保留旧记录。完成仅RAM，显式save才持久化。保存/重启加载已有有效校准，不虚构默认行程。

保存复用已有维护：全采集暂停ACK→清旧流/释放→实际USB完成和DMA结束→双槽读回commit→状态重建→Resume ACK。两槽offset0x3FB000/0x3FD000，各8192字节，逐字段schema2，不使用EEPROM；schema1只读迁移，不自动擦写。Flash暂停另一核/关中断属于维护阻塞，不承诺维护期间实时采集。

CDC新增运动/校准/UF2命令沿既有v1请求应答，无主动push事件。冷切换要求当前revision全部保存且目标等于保存身份，成功应答总线完成后detach/250ms/应用重启；UF2同一门槛后进ROM。不自动上传。IO4无CDC时按住BT10/GP9复位/上电强制普通且不改保存配置；BOOTSEL保留最终恢复入口。

断连/模块队列故障按新epoch重建，不重放离线历史。既有永久异步配置拒绝恢复可能持续重试，未证实合法配置可触发，接手须检查真实故障恢复。ADC/SSI/CRC、双核竞争/最大延迟、PIO色序、Flash掉电、Windows HID/Web Serial、游戏全部仍待实板验证。公开benchmark/完整来源原因遥测未增加；内部路由账本和ARM测试已接入。

## 固件版本与 UF2 识别（2026-10-04）

当前源码候选版本0.10.1，含采集调度及普通绝对XY软件修复；新版Windows/实板/游戏尚未验收，最新已实板运行版本仍为0.10.0。进度见 [修复记录](../../../progress/修复.md)。版本唯一入口为 src/common/FirmwareInfo.h 的 FLUXRAIL_FIRMWARE_VERSION。正式构建在 UF2 payload 保留固定 FluxRail 识别记录，hello 返回同一记录的版本与身份。编译脚本用网页解析器检查最终产物，缺失/重复/错误板型或版本不一致则失败。完整格式及兼容行为见 [固件版本与UF2识别](../../../spec/15_固件版本与UF2识别.md)。

共享1ms运动端点通过MotionSink::absoluteWithY合并待发送Hall X和Y；连续同方向段合并，反向段保序不抵消，正反小数分开保留，成功提交才扣量。联合报告不适用时保持相对/绝对轮换。队列满有显式丢弃账本并触发epoch恢复，不能保证无限反转负载无积压。来源接管遵循spec/14，背景增量不补历史，仅清对应轴余量。描述符结构/模拟通过不等于Windows Code10消除或游戏接收。

异步传输失败/断连后，普通后端在恢复完成且流可用时重发已提交的缓存XY；重发幂等，不重复增加Y，无已提交坐标不伪造初始运动。上述“模式切换保持Y”指运行时Hall开关/模板；NORMAL/IO4身份冷切换会重启，按新启动初始化Y，不跨重启持久化。
