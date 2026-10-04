# FluxRail IO4兼容与Arduino USB冷切换审核

版本：v0.4 · 源码审核日期：2026-10-03。对应U01/U02，主需求见[用户版](../handoffs/FluxRail_需求总览_用户版.md)，后续决策D21/D22见[清单](08_功能缺口与使用场景清单.md)。审核本机Arduino-Pico6.1.0 / Adafruit TinyUSB3.7.7、项目旧失败实现、外部GitHub源码。此审核保留接手前的源码取证；其中“未编写/未编译”属于历史状态。阶段9软件已实现及编译/模拟通过，见[IO4进度](../progress/IO4.md)、[软件契约](13_IO4软件契约.md)和[验证快照](../../../../tests/integration/validation_io4_2026-10-03.json)；当前仍未连接实板，游戏兼容性未验证。

## 审核结论

**Arduino环境内存在实现单固件双身份的基础，但当前普通封装API不能直接满足完整IO4身份与无竞态启动。需要项目内可复现的USB初始化/描述符适配。** 不能承诺“加if(mode)就完成”，也没有证据证明Arduino本身做不到。

两身份建议NORMAL_NKRO与IO4，底层均用TinyUSB。Arduino菜单选USB Stack是构建选择，不是运行时IO4开关。运行时在第一次USB枚举前固定一套完整身份；切换保存启动身份后重启/重新枚举，不在已有连接中仅更换Report ID。

项目旧reference-lkick用户已测不可用，不再作为正常工作基线。其核心样本仅留失败审查。外部ESP32固件可参考协议，不等于在FluxRail的RP2040/Arduino上已可用。详见[项目对比](10_IO4外部项目对比.md)。

## 本机源码证据

快照位于 ../../references/arduino-usb-audit，保留原版权头与SHA256，不修改已安装核心。

| 检查项 | 6.1.0/3.7.7实际源码 | 含义 |
| --- | --- | --- |
| 启动顺序 | cores/rp2040/main.cpp先initVariant，再TinyUSB_Device_Init(0)，然后启动核心1、setup | setup内读身份晚于USB启动，存在首次枚举竞态 |
| 早期入口 | initVariant是weak，但后续仍自动调用TinyUSB初始化 | 仅在initVariant里setID/注册接口不足，下面begin会重置 |
| 初始化复位 | API.cpp调用TinyUSBDevice.begin；Device.cpp begin先clearConfiguration再默认SerialTinyUSB.begin，最后硬件初始化 | 不能把早期设置当作始终保留；不调用Serial.begin也不等于没有CDC描述符 |
| 身份/连接API | setID、manufacturer/product/serial、detach/attach | 支持能力存在，但不能用Pico-SDK的USB.setVIDPID混入Adafruit路径 |
| 字符串限制 | Device.h _desc_str[33]；Device.cpp转换最多32字符 | IO4长板名超限；只调用字符串setter会截断，需完整描述符适配 |
| 设备类/接口 | begin默认CDC复合类，addInterface分配接口和端点 | IO4纯HID与普通CDC复合人格要分别构造，不能依赖同一默认集合 |
| 多HID实例 | HID.cpp实例数组[CFG_TUD_HID]、begin计数、按instance回调；默认上限2 | 本机源码有多实例基础，但普通绝对/相对/键盘最终组合须枚举验证；超过2需固定构建配置 |
| 64字节端点 | RP2040 config HID buffer64，report_id由sendReport加前缀 | IO4发ID1+63字节，不发ID1+64字节变65 |
| 回调所有权 | Adafruit Device/HID.cpp已定义全局TinyUSB回调，非weak | sketch复制外部tud_descriptor_*或HID回调可能链接重定义，不能混用两套所有者 |
| OUT回调 | 本机hid_device.c控制SET_REPORT会按report_id去首字节；中断OUT回调report_id=0且buffer含整包 | 接收层必须归一化两路径并验证长度/类型/实例 |
| 软重启 | RP2040Support.h rp2040.reboot使用watchdog_reboot；bootloader另有入口 | 重启应用可换人格，不等同进入UF2，也不必每次重刷 |

官方[USB文档](https://arduino-pico.readthedocs.io/en/latest/usb.html)区分Pico-SDK和Adafruit栈；网上文档当前6.2.0，最终以已保存6.1.0源码审核为准。其多HID限制提示不能代替本机版本检查，亦不能仅因本机有API就宣称实测可用。

## 建议实现路线，仍需原型验证

优先保持Arduino工程和同一TinyUSB栈，用**项目内固定版本适配层**：

1. 在USB硬件启动前读取小型、有校验的启动身份；无效身份回普通模式。启动选择不能等待Web、串口、磁铁校准或核心1。
2. 在描述符初始化完成、控制器连接主机之前，配置完整设备/配置/HID报告/字符串；一次只注册所选身份的接口。
3. 受控调整Adafruit初始化顺序：在clearConfiguration后、硬件初始化前提供项目钩子，按身份添加CDC/HID和完整长字符串支持。这需要项目内固定库适配，**当前下载的审核快照不是已经完成的补丁**。
4. 普通模式可用Adafruit HID对象和setReportCallback；IO4若完整描述符需求超出封装，由适配层统一接管。全局回调只有一个所有者。
5. 冷切换靠MCU重启重置对象/接口计数，不在同次运行clearConfiguration后反复begin旧对象；HID/CDC静态计数和instance必须一致。
6. 保证USB任务/IRQ在核心0；模式启动与核心1启动顺序一致，无并发修改描述符。

备选是构建级禁止自动USB初始化（本机main.cpp受NO_USB保护），由项目明确启动原生TinyUSB和自己的完整回调；必须确认宏作用于所有编译单元、CDC/自动上传和IRQ任务支持。在.ino里局部define不是整个核心构建选项。不能一边拉入Adafruit设备类，一边重复定义其强回调。

不需要换成ESP-IDF或改为独立Pico-SDK项目。但Arduino库解析优先级、固定依赖/构建标志、预编译缓存与接口最大数必须纳入交接。若原型验证前封装适配未完成，分开普通/IO4 UF2只能作为开发排查方案，不能冒充最终单固件冷切换交付。

## 普通与IO4身份契约

| 项目 | NORMAL_NKRO | IO4 |
| --- | --- | --- |
| 功能 | NKRO、相对旋钮、绝对滑轨、鼠标三按钮、配置CDC | IO4按键/绝对滑轨ADC/初始化命令；普通键鼠不自动并发 |
| 标识 | FluxRail产品名、稳定真实序列号；普通VID/PID另定 | 参考协议标识0CA3:0021及所需完整板名，按目标识别实测 |
| 按键 | 十二路keyboard/mouse_button动作 | 十二路到IO4功能/bit，不能直接用A/Space字符 |
| 模板 | 四预置+自定义 | USB身份与模板分开，可用音击配置的IO4映射 |
| 旋钮 | 每路相对X或Y | 音击默认关；spinner用途另立规则 |
| Web | 配置CDC | CDC是否保留待实测，不能把AIME READER端口当FluxRail配置口 |
| 指示灯 | 当前生效模板色（建议） | IO4身份优先色（建议），实际25% |
| 更新 | UF2明确维护 | 若无配置通道，回普通或物理ROM恢复 |

IO4必须核对设备字符串和HID接口字符串各自位置，不能只在任意字符串里放长板名。额外CDC/MSC在别人的复合固件出现，不说明目标环境接受任意接口排列；先验证最小IO4身份，再决定保留配置CDC。新板无PN532/额外LED控制板，不复制Aime/MSC驱动盘依赖。

## IO4协议与音击映射

输入到主机ID1，payload63字节：ADC8×2、spinner4×2、coin2×2、switch2×2、system/usb各1、保留29；含ID64。主机输出ID0x10，cmd1字节+payload62＝63，含ID64；明确字节序/尺寸，static_assert，非对齐字段安全解码。

归一化接收：

- 控制SET_REPORT：report_id0x10，通常buffer为cmd开头的63字节；
- 中断OUT：report_id0，buffer首字节0x10，完整64字节；
- 不匹配长度/type/instance/id则拒绝并计数；不能靠盲reinterpret_cast猜形态。

初始化至少核对0x01通信超时、0x02采样计数、0x03清状态的请求/响应状态演变；不把某参考写0x30/0直接当完整硬件行为。GPIO0x04灯输出解析需核对位序和映射；0x05/PWM与0x85原机更新按目标需求明确支持/拒绝，不让0x85自动进入FluxRail UF2。GET_REPORT是否请求由抓包确认，不能一律返回0又承诺兼容。

主机侧segatools对音击的映射提供交叉参考：左A/B/C＝switch[0]bit0/5/4，右A/B/C＝[0]bit1/[1]bit0/[0]bit15；左/右Menu＝[1]bit14/[0]bit13；**左右Side为低有效位**，与主板低有效轴体电气输入不是同一层。BT到功能独立可配，暂不猜十键的音击摆位。

滑轨用于ADC[0]绝对当前位置。segatools音击层使用ADC0=0x7FFF-lever，右到左增加；DLL lever为有符号域，原始ADC为uint16域，不能直接把signed中心0当原始轴中心。实际有效范围由目标检测/游戏校准测定，不把0～65535满量程自动当音击最佳范围。[主机映射源码](https://github.com/djhackersdev/segatools/blob/ca9c72db968c81fdf88ba01f9b4a474bf818e401/mu3hook/io4.c)

同一校准在普通绝对鼠标与IO4 ADC间共用，后端范围/方向转换各自明确。不再把滑轨同时写入ADC和spinner而不解释用途。用户只要求IO4模拟，不自动加入读卡、投币、整机外围模拟。Test/Service功能预留独立映射。

## 冷切换流程与恢复入口

普通Web显示“当前USB身份”和“重启后身份”，二者分开：

1. 设置目标启动身份并完整校验；显示将断线。
2. 显式维护保存成功后才继续；失败保持原身份，不重启。
3. 释放旧键/鼠标按钮、结束IO4开关状态、停运动/诊断，确保应答可送达。
4. 软件断开USB，按实测保证主机观察到移除的间隔，再重启应用。
5. 开机在首次枚举前选描述符；不先枚举默认普通再切IO4。
6. 主机重新发现目标身份，Web若CDC可用重新选端口/握手；灯色仅表示实际生效。
7. Windows缓存/端口号变化记录；普通与IO4身份不能共用造成歧义的描述符缓存键。

不要求每次物理拔电/烧UF2，但软断开间隔和主机重识别必须实测；反复软件重启不等于主机必定看见断开。

若IO4不带配置CDC，必须有**无需网页的应用启动返回普通模式**入口。可建议RUN复位时按一个指定BT强制普通身份，只在启动判定，键号待定；实体BOOTSEL通常进入ROM下载，不是自动返回普通应用。不可默认使用游戏中BT1+BT8长按切换。ROM BOOTSEL+正确UF2作为最后恢复路径，更新时保留Flash配置区域约定。

## “插上不识别”的审查与验收

先分层，不能没日志就认定是VID/PID：

- 完全不枚举：USB供电/线/时钟/描述符长度/初始竞态/硬件IRQ。
- 枚举但不是IO4：首次描述符/身份保存/回调冲突/字符串截断/Windows缓存。
- 枚举为IO4但初始化失败：IN/OUT长度、Report ID错位、控制/中断OUT、状态机、GET_REPORT。
- 初始化成功但按键/滑轨错误：BT映射、Side极性、轴数值域/方向、主机hook接管。
- 单模式可用、切换失败：主机未观察断开、接口实例计数、模式未持久、旧端点状态、CDC重连。

集成开工首个验证里程碑＝两种最小身份分别编译/枚举/初始化，再单固件冷切换。保存以下证据：

1. 普通冷上电、IO4冷上电各≥20次，首次枚举没有短暂错误身份。
2. 同一线/主机普通→IO4→普通≥50轮，不重刷/不靠拔插补救；计数失败和耗时。
3. 复位、断电、睡眠恢复/换口、保存中异常，以及无CDC物理返回。
4. 导出的完整device/config/report/string描述符、Report ID/尺寸及Windows设备状态。
5. USBPcap/主机日志中实际初始化命令及响应，比较控制SET_REPORT和中断OUT。
6. BT逐项、双键并发、Side按下/松开、滑轨左中右和失磁恢复；主机程序不截获替代物理IO4时也验证。

本阶段只审核和取证；以上任何实板测试均未完成。Arduino可行性属于有条件工程结论，最终“可靠识别与双向切换”以这些证据放行为准。
