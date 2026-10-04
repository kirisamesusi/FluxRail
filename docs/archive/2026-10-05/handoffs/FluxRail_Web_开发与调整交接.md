# FluxRail Web 开发与调整交接

版本：v1.1 · 更新：2026-10-04。对应 W01～W05，以及十二路映射、灯光、滑轨、旋钮、模板和 IO4 的页面交互。

本文记录当前 Web 的启动、修改、验证和交接方法，供后续窗口直接接手。Web 当前开发目录为项目根目录的 web/；backups/ 下都是历史快照。固件版本/识别格式需要同步 firmware/ 与 tests/integration/，按第2.3节及共同契约处理。本文不授权刷写设备或修改游戏配置，具体工作以用户交给该窗口的任务为准。

## 1. 阅读顺序与事实来源

1. [窗口必读](FluxRail_开发交接_窗口必读.md)、[用户主需求](FluxRail_需求总览_用户版.md)：确认产品行为、分工及用户最新要求。
2. 本文件与 [web/README](../source-readmes/web/README.md)：当前页面、代码入口、启动和测试。
3. [Web 需求规格](../spec/05_模块4_Web上位机.md)、[公共接口](../spec/01_公共接口与集成约定.md)、[整合运行契约](../spec/14_模块整合运行契约.md)。
4. 涉及设备读写时阅读 [CDC_PROTOCOL](../../../../firmware/FluxRail/src/protocol/CDC_PROTOCOL.md)、[完整配置/Flash](../spec/12_完整配置与Flash序列化.md)、[IO4/schema2](../spec/13_IO4软件契约.md)。
5. 涉及固件更新时必读 [固件版本与UF2识别](../spec/15_固件版本与UF2识别.md)：0.10.0识别头、自动版本比较、修改文件、构建/维护和验收步骤。
6. [模块4进度](../progress/模块4.md)、[整合进度](../progress/模块整合.md)、[整合运行说明](../root/RUN_INTEGRATED.md)和对应验证记录。

用户最新明确要求优先。历史文档里的“未接入”“修改模板只跳转按键页”“草稿时所有模板操作禁用”“设备旋钮状态在旋钮设置”不再代表当前页面。“UF2只检查结构/要求核对SHA-256/手填预期版本”也是历史行为。保留历史证据，当前行为按本文、实际源码及最新验证记录核对；涉及公共协议变化，先同步公共契约和受影响模块，不另造字段或命令。

## 2. 当前界面与已确认要求

桌面采用左侧模块导航、右侧内容布局。左侧标题为“设置”，不显示 01～08 模块编号；设备读回、RAM 应用、设备保存和状态显示在右侧公共区域。蓝、白、粉水色配色和偏可爱的图形保持，标题、按钮、提示使用正常说明书式文字。

| 模块 | URL 片段 | 当前职责 |
| --- | --- | --- |
| 模板管理 | #profiles | 四预设、自定义、新建/保存/改名/删除/恢复/激活 |
| 按键映射 | #keyboard | 十二路动作菜单、监听键盘及物理状态 |
| 灯光设置 | #lighting | 模式、亮度、全局颜色 |
| 滑轨设置 | #hall | 启用、反向、中心倍率 |
| 旋钮设置 | #knobs | 两路参数及独立本地模拟圆盘 |
| 输入测试 | #diagnostics | 相同物理键布局、设备旋钮状态和实际遥测 |
| 固件更新 | #update | 设备当前版本、UF2身份/板型识别和自动版本比较、下载模式/恢复说明、USB身份操作 |
| 实验性功能 | #experimental | 11 个进阶分组，包括配置文件、IO4 映射、共享校准 |

模块切换只隐藏/显示同一 DOM，不重建草稿，不发送配置命令；URL 片段、前进/后退和实验室内部深链接可用。导航结束键盘监听。窄屏使用横向导航及单列内容；物理键布局在容器内部横向滚动，保持左右分组。

### 2.1 物理按键布局

用户提供的 PCB 图片只作摆位参考，不用于更改 GPIO 或固件编号。

~~~text
P1                                                P2
BT8   BT7   BT6   BT5        BT4   BT3   BT2   BT1
            BT9                  BT10
~~~

按键映射和输入测试使用同一摆位：P1 左上、P2 右上；中排左四键、间隔、右四键；下排 BT9/BT10。CSS 使用九列，中央第五列留空，BT9/BT10 在第三行第3/7列。

source_id 保持 0～9＝BT1～BT10，10＝P1，11＝P2；展示顺序为 [10,11,7,6,5,4,3,2,1,0,8,9]。改变摆位不能按 DOM 顺序重排绑定数组或逐键颜色。默认 BT8/7/6/5＝ASDF，BT4/3/2/1＝JKL;，BT9/10＝VN；同键重复映射和监听功能保持。

### 2.2 旋钮和页头图片

设备旋钮状态已从旋钮设置移到输入测试。真实圆盘使用设备累计 raw/scaled 圈数，不从低频相邻角度差推算圈数；本地旋钮模拟与参数仍在旋钮设置，本地模拟不发送 HID/CDC 输入。缺能力或断连时清除/标识旧遥测，不能冒充硬件已通过。

右上角使用项目图片 [fluxrail-lace-bunny.png](../../../../fluxrail-lace-bunny.png)。网页资源副本位于 [web/assets/fluxrail-lace-bunny.png](../../../../web/assets/fluxrail-lace-bunny.png)，两份已逐字节核对一致，尺寸 1582×994，透明背景；img.hero-art 通过 object-fit: contain 保持原图比例。

以后替换图片时同步修改 index.html 的资源路径和尺寸；新文件名或类型还需在 server.mjs 中加入明确允许的资源路径及 MIME。当前仅允许该指定 PNG，不开放整个项目或整个 assets/。修改服务器代码后重启本项目预览服务；只改 HTML/CSS/JS 时刷新页面即可。

### 2.3 固件版本与 UF2 文件检查（2026-10-04）

正式固件从0.10.0起带识别记录，版本唯一来源是 [FirmwareInfo.h](../../../../firmware/FluxRail/src/common/FirmwareInfo.h) 的 FLUXRAIL_FIRMWARE_VERSION；hello 与 UF2 使用同一记录。网页连接读回后展示当前版本，选文件自动读取 FluxRail 识别头/板型/版本，提示同版、可升级、所选版本过低或固件不对。手填 target-version、beforeUpdate 的旧逻辑和文件哈希核对已移除。

旧UF2无识别头不能通过；旧设备hello仍兼容并可比较合规SemVer。断线清除当前版本，模拟不能比较硬件版本，换设备序号需重新选文件。低版本目前只提示，不阻止手动复制；选文件只读本地内容，不发送设备命令。读取同版只确认版本，不证明实板升级成功。详细状态、格式、修改顺序和发布命令以 [15号共同契约](../spec/15_固件版本与UF2识别.md)为准。

这次同时修改固件和Web，需要刷写新正式固件才能获得设备的新身份字段；不能套用旧“网页改版无需刷固件”的结论。后续只改提示一般无需重新编译，但改版本宏或识别格式必须构建并核对实际UF2。

## 3. 模板、草稿和保存规则

编辑模板与活动模板是不同状态：选择供编辑不会自动激活。四个预设为 SDVX、音击、In Falsus、普通多K；ID1～4保留，自定义最多8个，总12个。预设不能在页面模板保存流程中覆盖、重命名或删除；音击两旋钮默认关闭。

| 操作 | 当前行为 | 二次确认与持久化 |
| --- | --- | --- |
| 点击预设，包括当前预设 | restore_preset 恢复固件内置默认值，再读回 | 有网页草稿时先确认舍弃；无草稿直接恢复，仅 RAM |
| 选择自定义模板 | 读取该模板 | 有网页草稿时先确认舍弃；不自动激活或保存 |
| 应用编辑到RAM | 应用普通网页草稿，再完整读回 | 不自动写 Flash；允许修改预设的运行配置 |
| 保存到设备 | 保存设备已应用的完整配置 | 二次确认；普通/IO4未应用草稿会阻止保存 |
| 保存到当前模板：自定义 | 必要时应用网页草稿，再 save_config | 二次确认；直接保存，无需先手动应用 |
| 保存到当前模板：预设 | 要求新名称，复制来源，草稿只写入新自定义槽，再保存 | 二次确认“新建并保存”；不覆盖来源预设 |
| 新建模板 | 包含当前普通网页草稿，新槽读回后选中 | 二次确认并保存；不自动激活 |
| 自定义改名/删除 | 更新设备模板并保存 | 二次确认；未应用普通草稿时禁用 |
| 恢复预设/设为活动模板 | 恢复或激活设备模板 | 独立二次确认，仅 RAM；掉电保留需再保存到设备 |

模板名称为1～31个可打印 ASCII 字符，不含双引号和反斜杠。取消或 Escape 不提交写入，保留原草稿；取消切换时恢复下拉选择，确认窗口关闭后返回触发控件焦点。确认期间会话、选择或 revision 变化时拒绝写入。

重要边界：固件目前没有独立“运行配置副本”和“模板库”两个存储。修改预设的运行配置可以应用并保存到设备，但预设按钮始终以固件内置默认值为恢复来源。不要把它描述成协议层拒绝 set_profile 写预设，或新建了独立网页模板库。

所有持久保存沿用 save_config，写入整个设备快照，包括其他已应用模板、启动身份和全局校准；不能说只保存单个模板。恢复预设后若尚未再次保存，页面显示设备有未保存变化是正常状态。机械校准全局共享，不能随模板切换删除。

普通 draft、设备 RAM、已保存 Flash 必须区分。保留完整配置、motion、schema2 独立 IO4 表及未编辑字段，不能从可见控件拼一个不完整模板。IO4 未应用草稿仍阻止共享写入和模板操作；先在 IO4 编辑器应用或读回舍弃。

## 4. 文件分工与修改入口

| 文件 | 负责内容 |
| --- | --- |
| [index.html](../../../../web/index.html) / [style.css](../../../../web/style.css) | 页面模块、控件 ID、物理布局、响应式、图片和确认窗口 |
| [src/layout.js](../../../../web/src/layout.js) | hash 导航、焦点、同 DOM 展示、深链接 |
| [src/app.js](../../../../web/src/app.js) | 渲染、字段编辑、共享动作锁、轮询、监听、物理布局 |
| [src/device.js](../../../../web/src/device.js) | 一致读回、revision、draft、应用、新槽复制、预设恢复和保存 |
| [src/template-actions.js](../../../../web/src/template-actions.js) | 模板操作确认、预设保护、取消和确认后的会话复核 |
| [src/confirmation.js](../../../../web/src/confirmation.js) | 原生 dialog、名称校验、取消/Escape |
| [src/connection/client.js](../../../../web/src/connection/client.js) | Web Serial 生命周期、串行请求、分行解析、超时/id/版本检查 |
| [src/config.js](../../../../web/src/config.js) / [src/keyboard.js](../../../../web/src/keyboard.js) | 公共字段校验、键位/字符/动作映射 |
| [src/integration.js](../../../../web/src/integration.js) / [src/io4-editor.js](../../../../web/src/io4-editor.js) | schema2 遥测、共享校准、USB/UF2及独立 IO4 表 |
| [src/knob-preview.js](../../../../web/src/knob-preview.js) | 有界本地旋转预览，不向设备写入 |
| [src/snapshot.js](../../../../web/src/snapshot.js) | 全包 schema1/2 导入导出 |
| [src/update/uf2.js](../../../../web/src/update/uf2.js) / [src/update/controller.js](../../../../web/src/update/controller.js) | UF2结构/身份/板型解析、SemVer比较、候选文件与连接/序号状态 |
| [src/demo.js](../../../../web/src/demo.js) | 隔离 schema1 模拟端口，不替代真实固件或硬件 |
| [server.mjs](../../../../web/server.mjs) / [package.json](../../../../web/package.json) | 本机静态资源白名单、CSP、启动和七套测试入口 |

HTML 控件 ID 同时被 app.js、integration.js 等引用，调整时同步调用方，检查重复/缺失 ID。设备字段用 textContent 展示，不把名称或导入内容注入 HTML。模块折叠、可爱样式或本地预览不能解除能力、busy、revision、维护或未应用 IO4 草稿的锁。

## 5. 构建、启动与预览操作

当前使用原生 ES 模块，无第三方运行依赖。Node >=20；已有软件验证使用 Node 24.14.1。没有 npm run build 或 dist/ 打包步骤，不需要 npm install。HTML/CSS/JS 和 assets/ 就是运行源码；固件 UF2 构建是另一条流程，不能把 Node 检查写成 RP2040 编译。

在项目根目录 PowerShell 启动：

~~~powershell
cd C:/Users/admin/Documents/FluxRail
node web/server.mjs
~~~

也可在 web/ 执行 npm start。默认页面 http://localhost:4174/#profiles，只监听127.0.0.1；不要直接通过 file:// 打开 ES 模块。

另起预览且保留已有服务时，用独立终端设置另一个端口：

~~~powershell
cd C:/Users/admin/Documents/FluxRail
$env:FLUXRAIL_WEB_PORT = '4175'
node web/server.mjs
~~~

不要盲目重复启动4174或结束所有 Node 进程。确认现有端口是本项目服务后复用；改 server.mjs 时，在拥有该服务的终端 Ctrl+C，再按原端口启动。当前服务 no-store，不生成构建缓存。新的静态资源还要同时满足路径白名单、MIME 与 CSP。

浏览器验证不依赖实板：进入“实验性功能 → 连接 · 模拟与设备信息 → 模拟演示”。“连接 FluxRail”才会让用户选实际串口，实际 Web Serial 使用桌面 Chrome/Edge 和 localhost/HTTPS。模拟保存只改变模拟状态，不写真实 Flash。

## 6. 后续修改流程与交付规则

1. 先核对当前任务、本文、最新代码与相关协议；Web在 web/ 开发，不在 backups/ 或历史 references/ 修改。若任务涉及固件版本/识别格式，按15号共同契约同步 firmware/ 与 tests/integration/。
2. 修改前按 [备份说明](../../../../backups/README.md)新建当前 Web 的快照，记录目的、范围、时间、文件大小和 SHA-256。不要覆盖已有历史快照。
3. 固件文件检查/提示改 update/uf2.js 与 update/controller.js，版本宏、hello与构建校验按15号契约同步。视觉修改集中在 HTML/CSS；导航改 layout.js；模板交互改 template-actions.js/confirmation.js/device.js；设备能力与任务改 integration.js。涉及协议/存储则先更新共同契约和影响记录。
4. 保持已确认物理编号、完整 draft、IO4 表、校准共享、音击旋钮关闭及普通/IO4职责。网页不承担实时采样、运动算法、USB栈或Flash所有权。
5. 运行与改动相关的检查；JS/设备逻辑改动执行 npm test。纯文案/替换图片至少检查浏览器显示、资源加载、比例、断连状态和控制台，不为了低影响修改重写测试。
6. 浏览器通过真实点击/键盘提交字段，再用设备模拟读回核对；fill 仅改 DOM 未必触发 change，不能把 DOM 数值变化当应用成功。导航、取消、草稿及写锁都应覆盖。
7. 更新本文的当前状态、模块4进度及 web/README；公共影响同步对应契约。验证 JSON 和截图放 web/tests/，名称带日期和用途；历史记录不改成新一轮结果。
8. 交付列出修改文件、原因、接口影响、实际命令/结果、未测项、备份和后续入口。分开记录语法、Node软件模拟、浏览器模拟、固件编译、实板和游戏，不编造通过。

错误/断线/超时/部分写入失败后保留写锁，完整读回或重连确认，不盲重试 create/save/import/boot。模板创建后应用失败可能已经存在新槽；不要自动再创建或删除它。Flash维护会暂停采集，不能承诺保存中实时输入不受影响。

## 7. 检查命令与当前验证基线

~~~powershell
cd C:/Users/admin/Documents/FluxRail/web
npm test
node --check src/app.js
node --check src/device.js
node --check src/template-actions.js
node --check src/confirmation.js
node --check src/update/uf2.js
node --check src/update/controller.js
node --check server.mjs
~~~

npm test 以实际 package.json 为准，目前包含 web、knob-preview、integration、integration-ui、layout、template-actions、uf2 七套，版本功能一轮为59项通过、0失败。新增文件也应单独 node --check，检查以上命令不能代替浏览器或硬件验证。

| 验证范围 | 证据与实际层次 |
| --- | --- |
| 左右工作区、hash导航、监听与跨模块草稿 | [布局记录](../../../../web/tests/validation-layout-2026-10-04.json)，当时40项；浏览器模拟与窄屏检查 |
| 物理十二路摆位、输入测试、遥测容器迁移 | [物理布局记录](../../../../web/tests/validation-physical-layout-2026-10-04.json)，当时40项；浏览器模拟 |
| 预设恢复、草稿另存、自定义保存、取消、冲突/容量/部分失败、schema2 IO4保留 | [模板记录](../../../../web/tests/validation-template-management-2026-10-04.json)，51项；Node及浏览器模拟 |
| 页头图片 | [页面截图](../../../../web/tests/header-lace-bunny-2026-10-04.png)；PNG HTTP200/image/png，与原文件一致；浏览器原图尺寸和contain已核对，控制台无warn/error |
| 备份 | 三份原完整快照共101个清单文件已逐项核对大小与SHA-256；换图前3个文件另有清单，见备份文档 |
| 固件版本与UF2识别 | [版本记录](../../../../tests/integration/validation_firmware_version_2026-10-04.json)，七套59项、ARM协议九组、正式Arduino38单元严格编译及两人格ELF描述符检查；[实际选文件截图](../../../../web/tests/firmware-version-2026-10-04.png)读出0.10.0 |
| 实板/游戏/刷写 | 版本功能已正式编译并生成UF2；未打开真实串口、未上传/刷写，未验证实际Flash掉电、Windows Web Serial/HID、运动机械或游戏 |

最新截图还包括 [按键映射](../../../../web/tests/physical-keys-2026-10-04.png)、[输入状态](../../../../web/tests/input-status-2026-10-04.png)、[模板管理](../../../../web/tests/template-management-2026-10-04.png)、[新建并保存确认](../../../../web/tests/template-new-save-2026-10-04.png)。旧证据中“修改模板→按键映射”或旧遥测位置仅属于对应历史版本，不作为当前验收条件。

上一轮布局/备份文档阶段只整理说明，未改运行逻辑；之后版本功能已修改固件/Web并编译，见第2.3节与版本记录。此次补齐交接只改文档，不新增代码测试结果。文档相关本地链接已检查，四份快照共104个清单文件分别通过Node与文档中的PowerShell只读校验；PowerShell明确使用UTF-8并在错误时停止，避免读取中文清单失败后误报成功。

## 8. 备份和接手说明

[备份总说明](../../../../backups/README.md)列出四份快照、完整/局部范围、校验和恢复步骤。最初旧 Web 是 web-before-layout-2026-10-04；之后的物理布局、模板管理、换图快照是不同阶段的恢复点。不要把备份中的 README、旧测试数或旧交互要求当当前开发规格。

交给下一窗口可直接使用：

> 项目为 FluxRail，目录 C:/Users/admin/Documents/FluxRail。请先读 docs/FluxRail_开发交接_窗口必读.md 和 docs/FluxRail_Web_开发与调整交接.md，再根据我的当前任务修改 web/。保持蓝白粉水色与正常说明文案、左右模块布局、P1/P2左上右上、左右四键加下方两键、模板确认/预设恢复和完整草稿/设备写锁。涉及UF2更新再读 docs/spec/15_固件版本与UF2识别.md，按其同步固件版本/识别与构建校验，不恢复手填版本/哈希核对。先备份当前版，运行相关检查，区分模拟与实板验证，更新交接/进度/证据；backups/ 都是历史快照，不直接开发或误作当前版本。
