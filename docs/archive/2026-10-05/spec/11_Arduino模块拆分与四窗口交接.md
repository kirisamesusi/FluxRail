# FluxRail Arduino工程组织与集成

版本：v0.4 · 更新：2026-10-03。对应G01/Q02。各窗口首先阅读[开发交接入口](../handoffs/FluxRail_开发交接_窗口必读.md)；本文件专门说明工程组织、资源和合并，不重复主需求。用户确认Arduino模块化汇总；工程已按下述目录实现模块1及公共层，真实入口为firmware/FluxRail/FluxRail.ino；软件证据和未接入项见[模块1进度](../progress/模块1.md)。目录中的模块2/3源码已有独立实现，实际App调度/运动集成仍未接入。

## 结论与Arduino规则

可以用一个Arduino sketch、多个.h/.cpp独立实现；每个.cpp单独编译，最后链接为一个RP2040固件。主文件与目录同名，建议firmware/FluxRail/FluxRail.ino。

Arduino自动把多个.ino拼接，容易产生全局变量/函数顺序依赖，所以只保留一个薄入口.ino，其余业务用明确.h/.cpp。不include其他.cpp，不把每个模块都做成独立setup/loop。[官方构建规则](https://docs.arduino.cc/arduino-cli/sketch-build-process/)

src下C/C++文件递归编译，不放.ino；Arduino IDE不会把src深层文件都显示为主窗口标签，可从文件浏览器/编辑器修改。若用户希望所有模块直接在IDE标签中显示，也可平铺.h/.cpp于sketch根；默认建议分目录，接口相同。[官方sketch目录规范](https://docs.arduino.cc/arduino-cli/sketch-specification/)

## 工程目录与职责（以当前源码为准）

~~~
FluxRail/
  README.md
  docs/spec/
  firmware/FluxRail/
    FluxRail.ino
    src/
      app/          App, Scheduler, RuntimeHealth
      board/        BoardConfig
      common/       InputTypes, ConfigTypes, ModuleStatus
      realtime/     InputWorker, EventQueue, MotionQueue, ConfigMailbox
      keyboard/     KeyScanner, ActionMapper, Retrigger
      lighting/     KeyLighting, ModeIndicator, Ws2812Output
      hall/         HallSampler, HallPosition, HallCalibration
      knobs/        Mt6701Ssi, KnobMotion, PushFilter
      usb/          UsbPersona, NormalHid, Io4Backend, Io4Codec
      config/       ConfigStore, ProfileManager, ConfigValidation
      protocol/     ConfigProtocol, CommandRouter, Telemetry
  web/
    index.html
    src/
      connection/
      keyboard/
      lighting/
      hall/
      knobs/
      profiles/
      diagnostics/
      update/
  dependencies/     实现阶段的固定USB适配依赖/补丁与构建说明
  references/       当前阶段下载和保留的参考，不参与编译
~~~

这里文件名是职责建议，不要求每个小类型都拆成一个文件。主入口只启动App并调用核心0/1服务；算法不堆入.ino。web仍一个页面，内部JS可分文件，不影响用户单页体验。Web当前采用原生ES模块本地单页，无打包构建步骤；实际文件分工、启动和调整流程见 [Web开发交接](../handoffs/FluxRail_Web_开发与调整交接.md)，以上Web子目录是历史职责建议，不要求将现有JS迁入这些子目录。

references在sketch之外，避免ESP32/主机C#源、旧.ino和重复TinyUSB回调被Arduino误编译。dependencies中的适配层以明确Arduino依赖/构建规则接入，不将整套第三方仓库直接塞进src。

## 四业务模块与公共层

| 交接窗口 | 主要文件 | 必须产出 | 不能自行负责 |
| --- | --- | --- | --- |
| 1 键盘/灯，建议兼任集成 | keyboard、lighting；集成时app/usb/config/protocol/realtime | 十二路映射、NKRO/重触发、灯模式；集成入口/身份/协议实现 | 不改霍尔/SSI算法来适配自己的USB |
| 2 三霍尔 | hall | 三路采样、绝对位置、校准任务、状态/统计/参数契约 | 不调用Mouse/Serial/Flash，不定义USB描述符 |
| 3 两MT6701 | knobs | 两SSI/CRC、展开与相对增量、PUSH防误触、圈数遥测 | 不自行建主循环/USB/存储；机械暂缓项明确 |
| 4 Web | web；共同冻结protocol字段 | 单页各区域、连接/CRUD/校准/诊断/更新/冷切换交互 | 不把实时运动计算放网页，不私自更改固件保存格式 |

四个人都能分开修改自己的目录。公共层由**一个集成负责人**持有；建议窗口1兼任，但不是把公共层都塞到键盘文件。USB普通/IO4后端各有独立文件，共用唯一UsbPersona生命周期和全局回调入口。

## 数据方向

核心1：
KeyScanner/PushFilter → 带source_id的边沿；
HallSampler/HallPosition → 有效绝对位置；
Mt6701Ssi/KnobMotion → 两路相对增量/展开圈数；
全部经公共队列/快照传给核心0。

核心0：
ActionMapper/Retrigger → 普通键鼠动作或IO4功能；
来源路由 → NormalHid/Io4Backend；
ProfileManager/ConfigProtocol → 校验快照 → 核心1ACK；
KeyLighting/ModeIndicator → PIO/DMA灯输出；
维护保存 → ConfigStore，进入UF2/身份重启 → UsbPersona。

模块状态可用于诊断，不让USB回调直接调用校准或LED阻塞更新。USB OUT只快速解码/入队，后续任务处理。

## 建议接口职责，不是代码

| 模块接口 | 含义 |
| --- | --- |
| 初始化 | 硬件资源仅一次、所在核心明确、返回成功/失败 |
| 周期服务 | 输入单调时间戳和固定配置快照，预算有界 |
| 输出 | 统一类型/单位/有效性/序号/revision，返回队列状态 |
| 配置应用 | 完整校验后采样边界生效，ACK revision |
| 校准任务 | start/status/cancel，异步，无delay长等待 |
| 诊断快照 | 不直接重新读对方外设，不影响采样节拍 |
| 维护停机/恢复 | 结束输出，保存后重建参考，无旧事件补发 |

公共ConfigTypes和InputTypes先冻结，每窗口引用同一头文件；不能各自定义一个略有差别的Profile或MotionRecord。数据结构不含网页DOM/JSON对象；协议层负责序列化。

普通/IO4后端共享物理输入，却有不同语义：滑轨绝对鼠标X/IO4 ADC，旋钮普通相对，按键普通usage/IO4功能bit。不是换标识后直接复用同一报告结构。

## Arduino USB适配归属

当前本机启动和字符串限制需要专项适配，详见[审核](09_IO4兼容与USB冷切换.md)。该代码归usb/dependencies，所有全局回调只定义一次；其他模块不复制第三方tud_*回调。

固定Arduino-Pico6.1.0及TinyUSB3.7.7，Generic RP2040/4MB/125MHz。构建说明必须记录实际选中的库路径、编译宏和受控修改；不能靠某窗口修改自己电脑安装库后让其他窗口无法复现。

references/arduino-usb-audit是原样取证，不是已实现适配层。未来先验证最小普通与IO4身份，再接业务模块。不能绕过身份验证直接合并大量代码后才排查“插上不识别”。

## 合并顺序和交付约束

1. 集成窗口先冻结公共类型/字段/目录、板配置、队列、唯一调度和USB原型验证方式。
2. 各窗口按接口产出自己的模块，可独立以传感器记录/模拟数据检查；旋钮机械未定部分保留。
3. 先集成按键+普通USB、灯，再滑轨绝对、旋钮相对与PUSH；IO4身份独立通过初始化和冷切换门槛。
4. Web按协议样例开发，真实能力握手后显示；模拟页面成功不是固件成功。
5. 最后按[验收](06_验收与实测计划.md)全模块并发和冷切换，不仅分别编译。
6. 每次合并交变更文件、接口变动、固定构建说明、验证结果、待实测项；接口变动先修改共同契约并列出影响，再同步四窗口。

交接给其他窗口时明确：不要移动references、不要改硬件GPIO、不要恢复旧相对滑轨、不要把相同键盘键重复映射改为只合并状态、不要默认音击旋钮开启。模块职责清楚，后续修改某一个算法不需要重写整份固件。
