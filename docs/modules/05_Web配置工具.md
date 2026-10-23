# 模块4：Web 配置工具

更新：2026-10-11（北京时间）。**2026-10-11 页面已按 Web v2 设计包重做为三页签（设备状态 / 固件 / 实验性功能），本文凡描述“左侧设置模块导航、八个页面、按键/灯光/滑轨/旋钮/模板各自一页、旋钮本地圆盘预览、首页可展开输入状态”的段落均为 2026-10-08 之前的历史描述，现行结构见下方“Web v2”小节与[Web v2 设计实现与后端对接指南](../Web_v2_设计实现与后端对接指南.md)。**下文本文按当前 `web/` 源码与 `package.json` 整理；Web 是原生 ES 模块，运行不需打包或第三方依赖。历史规格、阶段记录及旧 Web 交接已移入文末归档链接，日期较早的页面描述与测试数字不能当作当前状态。用户再次反馈UF2上下重复错误后，已修实际提示渲染并同步测试：上方固定说明、下方动态结果；73/73完整Node与三宽度浏览器检查通过，见[本轮修复](../../web/tests/uf2-fixed-result-2026-10-08/README.md)。

## 当前结构与入口

### Web v2（2026-10-11，现行）

设计包与实现记录：[HANDOFF](../../design/web-v2-2026-10-11/HANDOFF.md)、[DESIGN](../../design/web-v2-2026-10-11/DESIGN.md)、[IMPLEMENTATION](../../design/web-v2-2026-10-11/IMPLEMENTATION.md)；详细映射、更新清单见[对接指南](../Web_v2_设计实现与后端对接指南.md)。

| 页面 / 片段 | 内容 |
| --- | --- |
| 设备状态 `#device-status` | 左栏模板列表/设备信息/历史操作；中间设备面板（BT1～BT10、左右旋钮、滑轨，实时显示按下与旋钮/滑轨位置）；右侧检查器（按键动作与灯光、旋钮、滑轨、模板管理）；顶部气泡显示草稿/应用/保存状态；灯效、亮度、全局色 |
| 固件 `#update` | UF2 检查、版本比较、进入下载模式（逻辑与原固件页一致） |
| 实验性功能 `#experimental` | 模拟演示、完整快照导入导出、键位/扫描、灯光高级、滑轨与旋钮进阶、来源策略、校准、状态记录/CSV、IO4 |

关键变化：
- 仍是“先改草稿、再应用并保存”。“改动即时生效”经核对不可行（无回退 Flash 命令；未保存时重启回已保存模式被拒），未实现。
- 灯效“自定义”＝`per_key`，与动画互斥；色球槽及“固定”状态存浏览器 localStorage（用户决定：做固定、不做改名；只是便利，不进设备/模板/快照）。
- 模板名可用中文：网页限 1～10 字符、UTF-8 ≤ 31 字节，线路以 `\uXXXX` 保持 ASCII，固件已改 UTF-8 校验与编解码（schema/Flash 布局不变）；**仅主机 96 项验证，未 ARM 编译/实板**，见[证据](../../tests/integration/utf8_name_2026-10-11/README.md)。旧固件拒绝中文名，默认新建名保持 ASCII `Custom N`。
- 旋钮/滑轨指示读 `get_motion`（原生 `raw_turns`/`hall.p`，灵敏度后 `scaled_turns`/`hall.output`）；按键按下读 `get_status.physical_mask`（轮询，不是事件流）。
- IO4 界面只显示 BT1～BT10，P1/P2 选择器保留在 DOM 外原值带回（用户已确认，PUSH 按键硬件暂时取消）；PUSH 防误触不再显示，数据原样保留。
- 看板娘台词：编辑 `web/assets/mascot-lines.txt`，刷新生效。
- 验证：Node 81/81（原 73，已把受 UI 重构影响的 layout/integration-ui/sensitivity/template-actions/web 测试按新规则更新，新增 `tests/v2.test.mjs`）；Chromium 模拟演示无控制台报错，三页×三尺寸截图在设计目录 `shots/`。以上是软件/DOM 层，不是实板。

### 以下为 2026-10-08 及更早的历史结构描述

2026-10-08用户要求整理并推送当前Web、测试与文档到Git：本轮沿 `test` 分支提交Web运行修改、UF2测试适配、独立验证记录、Ubuntu部署工具及Actions工作流。部署包另建r3以纳入BT10最新文案，旧包/失败证据保留；软件73/73与部署脚本10组隔离检查通过，见[交付记录](../../web/tests/git-delivery-2026-10-08/README.md)。Git推送、Actions与公网部署状态需分别确认，软件通过不替代真实设备/最终硬件验收。

2026-10-08新增 [交互服务器初始化脚本](../../web/deploy/setup-fluxrail.sh) 与 [部署包构建入口](../../web/deploy/package-setup.py)：从用户已完成的基础环境之后，交互配置独立站点、初次网页、证书和GitHub部署参数；UFW/主配置/其他站点不改，已有TLS、网页版本和密钥保留。包内网页只含原静态导出白名单，私钥在运行脚本时服务器生成。本地10组隔离检查及73项Web回归通过，见[本轮证据](../../web/tests/deployment-script-2026-10-08/README.md)；未实际操作服务器、签证或GitHub登录/发布。

2026-10-07新增 [Ubuntu / Nginx独立站点 / HTTPS / GitHub Actions部署](../Web部署_Ubuntu与GitHubActions.md)。目标 `fluxrail.baka.moe`，从 `test` 分支更新朋友测试站；服务器静态托管，导出仅浏览器运行文件及模拟依赖 `tests/golden.js`，不上传整个仓库。Node 24仅用于Actions测试；本地4174入口不变。部署版本上传完成后切换链接并核对HTTPS版本，失败恢复旧链接。10/07本地两项UF2提示失败是当时测试口径的记录，10/08已按当前Web适配断言并通过；npm测试门槛保留，新配置尚未在用户服务器/Actions实际执行。旧证据见[部署准备检查](../../web/tests/deployment-2026-10-07/README.md)，新结果见[10/08记录](../../web/tests/uf2-current-web-2026-10-08/README.md)。

本轮已按 [2026-10-05重构交接](../Web重构交接_2026-10-05.md) 与 [固定模板](../../design/web-template-2026-10-05/README.md) 沿现有生产实现完成结构与动作迁移。生产端口仍为4174；固定模板4176保持冻结，不使用静态表单或 shell.js 覆盖生产。验证见 [生产重构记录](../../web/tests/refactor-2026-10-05/README.md)。

桌面使用左侧“设置”模块导航和右侧单模块工作区，顶部将标题、插画、“重新读取设置 / 应用并保存 / 重启设备”和反馈合为无外框区域。模块切换隐藏/显示同一 DOM，保留编辑器和草稿；支持 URL 片段、前进/后退及实验室深链接，切换时结束键盘监听。窄屏为横向模块导航与单列内容；普通映射、首页输入状态和 IO4 共用十二来源物理摆位：上排左侧 BT8、BT7、BT6、BT5，留中缝后右侧 BT4、BT3、BT2、BT1，下排 BT9、BT10，P1/P2 旋钮按压位于左右上角。显示顺序不改变设备 `source_id`、绑定数组或逐键颜色索引。

页面模块按设备状态、按键映射、灯光设置、滑轨设置、旋钮设置、模板管理、固件更新和实验性功能排列，默认设备状态。旧 `#diagnostics` 跳转并展开首页输入状态；品牌 Logo 返回首页。设备信息、真实活动模板和当前编辑模板分别显示，断连保留读数时标为“上次读回”，新握手清除上一设备诊断、记录与导入候选。首页的滑轨/两旋钮 enabled 和设置页引用同一草稿，修改只更新网页，不自动应用或激活。真实旋钮累计遥测放入首页可展开输入状态，本地圆盘继续留在旋钮设置。共享校准在“滑轨 · 校准、输出范围与响应”组内同宽直显，11项参数带单位/作用帮助。

### 当前页面与信息归属

用户2026-10-07再次确认截图中的组织方式；设备信息属于首页，模拟入口属于独立实验性模块，不恢复旧“设备 · 演示与信息”混合分组。

| 页面 / 地址片段 | 内容与职责 |
| --- | --- |
| 设备状态 `#device-status` | 当前会话设备型号、固件/CDC协议、唯一序号、当前USB模式、启动模式（设备RAM）、活动模板、修订号；活动与编辑模板分别显示；按键/滑轨/两旋钮概况及跳转；可展开输入状态 |
| 按键映射 `#keyboard` | 十二来源普通键鼠映射与监听录入 |
| 灯光设置 `#lighting` | 常用灯效/亮度；逐键配色在实验性功能 |
| 滑轨设置 `#hall` | 常用启用/方向/中心灵敏度；共享校准与高级参数在实验性功能 |
| 旋钮设置 `#knobs` | 常用配置与本地旋转预览；真实累计遥测在首页，高级输出/PUSH参数在实验性功能 |
| 模板管理 `#profiles` | 预置保护、自定义模板和独立激活；完整配置文件操作在实验性功能 |
| 固件更新 `#update` | 当前版本、本地UF2识别和比较、下载模式入口 |
| 实验性功能 `#experimental` | 临时模拟演示、完整快照导入导出、键位/扫描、逐键灯、滑轨校准/输出、旋钮/PUSH、来源策略、原始状态/CSV及IO4编辑 |

模拟入口位于“实验性功能 → 临时测试 · 模拟演示”（`#experiment-device` / `#demo`）；深链接可展开该组。顶部标题旁 `#demo-label` 和侧栏 `#connection-state` 显示模拟身份，首页显示该模拟会话的信息。设备信息搬到首页不代表模拟身份提示要删除，也不改变UF2对模拟版本的处理。模式选择、连接状态仍在侧栏，重新读取/应用并保存/重启与反馈仍在顶部。

2026-10-05用户选定水色漫画少女作为吉祥物：左上角替换原水滴，浏览器 PNG/ICO 与 Apple touch icon 使用同角色头像，右侧保留原音游控制器的双旋钮、滑轨、键位及蕾丝装饰，将兔子替换为该角色。角色进一步按页面蓝白粉主题柔化为浅蓝头发、灰蓝线条/衣领和淡粉点缀。资源与提示词见[资源说明](../../web/assets/MASCOT.md)；选定原图、调整前衍生图和主题配色原图全部保留，运行资源独立缩放导出。页面图片仅为装饰。

运行图片为 `assets/fluxrail-mascot-brand.png` 和 `assets/fluxrail-mascot-controller.png`；icon 包含32/192px PNG、180px touch icon和16/32/48px ICO，兼容 `/favicon.ico`。`server.mjs`只增加这些明确路径及ICO MIME，继续限制本地服务、CSP与工程资料访问；原图、生成记录与离线导出工具不进入HTTP白名单。离线导出工具 `web/tools/export-mascot.mjs` 需要已安装Sharp模块路径，Web运行/测试仍不依赖Sharp。

用户随后要求长期保存基础素材，已建立[水色少女素材库](../../design/asset-library/fluxrail-mizuiro/README.md)与[视觉规范](../视觉素材与角色规范.md)，包含6张角色原图、4张候选、6个现用导出文件、2张参考输入和完整生成信息；默认沿用主题配色版。素材库只作为设计复用入口，不改Web运行路径或服务范围；原文件按字节复制，旧Web证据及素材SHA保留。

| 修改目标 | 主要入口 |
| --- | --- |
| 页面结构、可访问控件、静态布局 | [index.html](../../web/index.html)、[style.css](../../web/style.css) |
| hash 导航、同 DOM 切换、焦点与深链接 | [layout.js](../../web/src/layout.js) |
| 组合应用保存与设备确认；倍率尺与原值兼容 | [workflows.js](../../web/src/workflows.js)、[gain-control.js](../../web/src/gain-control.js)、[sensitivity.js](../../web/src/sensitivity.js) |
| 主页面编排、字段控件、状态和动作锁（v2 重写） | [app.js](../../web/src/app.js) |
| v2 色球、改动清单、看板娘台词 | [colors.js](../../web/src/colors.js)、[history.js](../../web/src/history.js)、[mascot.js](../../web/src/mascot.js)、[mascot-lines.txt](../../web/assets/mascot-lines.txt) |
| 设备一致读回、revision、普通草稿、应用与模板写入 | [device.js](../../web/src/device.js) |
| 模板确认、预设保护及确认后状态复核 | [template-actions.js](../../web/src/template-actions.js)、[confirmation.js](../../web/src/confirmation.js) |
| 串口会话、请求队列、分行协议、超时和错误处理 | [connection/client.js](../../web/src/connection/client.js) |
| 字段/范围及按键录入 | [config.js](../../web/src/config.js)、[keyboard.js](../../web/src/keyboard.js) |
| 真实运动遥测、共享校准、USB 冷切换、IO4 接入 | [integration.js](../../web/src/integration.js)、[io4-editor.js](../../web/src/io4-editor.js) |
| schema1/2 完整快照适配 | [snapshot.js](../../web/src/snapshot.js) |
| 隔离模拟端口 | [demo.js](../../web/src/demo.js) |
| UF2 文件校验、版本比较、连接状态提示 | [update/uf2.js](../../web/src/update/uf2.js)、[update/controller.js](../../web/src/update/controller.js) |
| 本地服务器白名单、CSP、MIME 与端口 | [server.mjs](../../web/server.mjs) |
| 当前测试入口和 Node 版本 | [package.json](../../web/package.json) |

HTML 中的控件 ID 被 app、integration 等多个模块引用；调整 ID 要同时核对调用方。设备返回的名称和导入数据以文本方式显示。设备字段和范围以归档的 CDC 契约及完整配置规格为准，Web 校验是前置检查，固件仍做最终校验。

## 生产客户端与模拟端口

“连接 FluxRail”进入浏览器 Web Serial，用户选择串口后客户端完成 CDC 握手和能力检查，再以一致 revision 读回状态；支持 profiles 的固件继续读设备元数据、模板列表和选中模板，旧 runtime 固件走活动配置子集。它通过设备声明的能力选择可用功能；读回失败、revision 冲突、协议错误、超时或写入结果不确定时，会阻止继续写入，要求完整读回或重连确认。运行需要桌面 Chrome/Edge 支持 Web Serial，并从 localhost 或 HTTPS 访问；网页不自动发现设备、保存配置或上传固件。

“模拟演示”动态加载隔离的 `DemoHost`/模拟端口，带有明确模拟序号和版本；当前模拟器仅声明 schema1 与基本键盘、灯光、配置读写等能力，不声明真实 Hall/旋钮、IO4、校准、bootloader 或固件更新能力。它可用于页面、协议适配和保存流程演示，但保存只改模拟对象，不能证明 CDC、真实 Flash、设备双核、USB 切换、传感器、ROM 或游戏行为。不要把模拟接口扩展成新的设备协议。模拟连接与旋钮页本地圆盘预览是不同功能：前者建立隔离会话，后者不建立会话、不发设备命令。

普通草稿、设备 RAM 和已保存 Flash 是不同状态。“应用并保存”先校验草稿，沿 Device.apply() 等实际应用 ACK 和一致读回；如有全局模式草稿，再沿 set_startup_usb_mode ACK/读回；最后才调用 save_config 并读回当前模板/保存状态。没有网页修改时仍能保存 RAM 变化。应用失败不开始保存；保存未确认时明确说明设置已应用、要求读回核对，不盲重试。每次 refresh 开始即失效 ready，只有完整一致读回成功才重新放行写入。当前保存确认是设备保存 ACK、revision/dirty 及当前模板读回，不声称重新逐字节核对全部模板与校准；完整字节核对仍使用 snapshot 导出路径。

“重新读取设置”有普通、IO4 或模式草稿时明确列出舍弃范围并确认，取消/Escape 保留草稿。模式选择只是全局待应用值，不放入模板、不立即发命令；选择 IO4 先提示下次上电/重启、网页无法连接和 BT10 恢复，取消保留原模式。重启/UF2 先确认并复核会话、所选模板、revision、能力、维护、校准和草稿；重启保留原保护，网页草稿或 RAM dirty 时仍须先保存/读回，不能用弹窗绕过固件的已保存模式限制。IO4 未应用草稿锁共享动作，普通和模式草稿锁 IO4；模板操作不丢弃模式草稿。页面关闭不自动保存，不自动激活编辑对象。旧 runtime 无保存能力时按钮明确为“应用设置（不支持保存）”，只调用 get_config/set_config 子集。

## 模板、确认与持久化约束

预设 ID 1–4 由固件内置默认值恢复，不允许通过页面模板保存流程覆盖、改名或删除；自定义模板最多 8 个，总数最多 12。名称（2026-10-11 起）为 UTF-8：网页新建/改名限 1～10 个字符且 ≤ 31 字节，不含引号、反斜杠、控制/零宽/双向控制字符；读回的旧 ASCII 名（至多 31）仍合法。每次选择预设（包括当前预设）都会走 `restore_preset` 并读回默认值；有普通网页草稿时切换前要确认舍弃。选择自定义模板只读入编辑器，不自动激活。

“保存到当前模板”对自定义模板确认后应用普通草稿（如有）并保存；当前选中预设时要求输入新名称，创建自定义副本，将草稿写入新槽后再保存，不先覆盖预设来源，也不自动激活。新建、改名、删除等需要确认；取消/Escape 不发送相应写命令。确认期间重新检查连接、模板选择和 revision，变化时拒绝执行。操作出错或结果未知时不盲目重试；创建后应用失败可能已有新槽，需先读回确认。

所有持久保存共用 `save_config`，写的是设备完整配置快照，包含其他已应用模板、启动 USB 身份及全局校准等内容，不是网页自己的单模板存储。活动模板切换、预设恢复和导入只修改 RAM；需掉电保留时另行保存。网页不使用 `localStorage` 保存设备配置（v2 仅用它保存色球调色板这类个人便利项）。Flash 保存期间固件可能暂停输入采集。

## 配置导入导出与 IO4

配置文件以设备 `export_config` 返回的完整字节为真值，按分页验证 schema、revision、长度、CRC 和偏移；不会用当前表单重建真实快照。文件 JSON 只是身份字段和 `hex-le-schema1/2` 二进制数据的封套。导入先在本地核对产品、板型、schema、长度、CRC、模板和字段范围并显示拟变更，再通过 `begin_import`、512 字节顺序块和 `commit_import` 提交；固件还会做校准曲线和语义完整性检查。全量导入替换 RAM 配置，不自动保存。跨设备/机械导入校准前需要人工确认机械适用性。当前传输是全包，页面不声称支持 current/all/calibration 分范围导入。

IO4 是 schema2 每模板独立的十二路功能表，与普通键盘 usage/modifier 映射分离。编辑时先读回当前模板；网页选择变化只是 IO4 草稿，必须通过 `set_io4_bindings` ACK 并读回后才算应用。IO4 专用“应用并保存 IO4 设置”先 set_io4_bindings ACK/读回，再共享 save_config；取消独立激活/保存/下一步按钮。活动模板由模板管理单独设置，组合操作不自动激活编辑对象；全局启动模式在侧栏选择，再经顶部应用并保存，冷重启才启用。IO4 未应用草稿会锁住共享保存、模板操作、USB 切换、校准及 UF2 入口；普通配置有未应用草稿时需先应用或读回，再编辑 IO4。外部 revision 变化时旧草稿不能写入，使用明确读回入口重新加载。IO4 默认不提供 CDC；USB 供电时，拔掉 USB，按住 BT10 后重新插上，待设备启动后松开，可让本次启动进入普通模式。恢复不会修改已保存的 IO4 启动设置；如需以后都以普通模式启动，须连接网页选择普通模式并应用保存。

2026-10-06用户随后提供截图明确恢复目标：截图对应 design/web-template-2026-10-05 的设计弹窗，远端 test 最新版的通用长文不是该目标。生产 IO4 选择现恢复模板标题、粉色感叹号、加粗警示/正文和“了解，选择 IO4”按钮；重启确认恢复模板简短说明。confirmation.js 增加可选 warning/note，复用既有取消、焦点、名称和 UF2 流程，每次打开重置显示状态。模板的演示提示替换为生产事实：IO4 仅选择草稿，不写设备；重启确认后会实际重启并断连。没有复制模板 shell.js，也不改冻结模板或设备保护锁。滑轨和两个旋钮的常驻灵敏度说明仍移除，范围、步进、换算与旧值/错误提示保持。来源、浏览器截图与 Luna 审核见[截图模板恢复记录](../../web/tests/dialog-template-restore-2026-10-06/README.md)，此前从远端恢复仅是[中间阶段](../../web/tests/io4-git-restore-2026-10-06/README.md)。

## 运动、校准和旋钮边界

2026-10-08仅调整IO4启动确认正文的BT10片段为“按住本设备的 BT10（当前映射：N）”，映射读取页面模板的BT10/source9普通动作并复用键位标签；网页有草稿时标为“网页草稿映射”，编辑非活动模板时标为“编辑模板映射”，禁用或无读回也明确标注。其他标题、警示、说明、按钮和样式保持原样，其他恢复提示未改。恢复入口仍是物理BT10，括号只是识别线索，不能改为按电脑键盘的同名键。软件检查见[本轮记录](../../web/tests/io4-bt10-label-2026-10-08/README.md)，不改变固件/协议或构成实板恢复验收。

设备遥测经现有 `get_motion` 路径读取，真实圆盘使用设备累计 raw/scaled 圈数，不从低频角度采样差积分圈数；断连、换设备或能力不支持时清除旧数据。当前轮询诊断适合状态显示，不是毫秒级无丢帧性能证明。配置中的 Hall、旋钮、PUSH 和路由参数可被 schema2 读取/写入，但页面设置这些值不证明固件运行算法已按参数工作。

共享校准只对声明 `hall_calibration` 的真实整合固件开放。界面要求实际左/右毫米端点和移除磁铁确认，按设备任务状态依次提交空场、三路参考、实测节点、曲线检查/扫描，再等待固件采用及 ACK；候选校准仍是 RAM 状态，需另行保存。未应用普通草稿、IO4 草稿、维护/应用锁或 revision 冲突都会禁用写入口。模拟器禁用真实校准。不能把引导界面、全局校准有效位或软件回归误说成实板校准完成。

旋钮设置的拖动、滚轮、键盘和按钮圆盘只演示当前网页配置下的本地几何投影，输入圈数有界，不调用 CDC/HID，也不修改草稿。真实安装方向、实际机械左右位置以硬件资料为准，网页示意不证明真实旋钮输出。

2026-10-06按用户后续要求：三个灵敏度控件统一1.0～10.0、步进0.1，整数也显示一位小数；5.0保持旧实际倍率1。新值除以5写入现有center_gain/gain（1.0→0.2、10.0→2），不改变固件算法/默认值、CDC或schema2。数值越大输出响应越高。后续按用户要求，输入框在键入/粘贴时即时截去第二位及后续小数（5.19→5.1、5.678→5.6），不等完成编辑才舍入；保留删除/替换、小数点和原生箭头0.1步进，禁止科学计数法与正负号。完成编辑仍校验1.0～10.0（10.1拒绝），新值除以5及旧配置保护不变。读回/导入不量化或标脏，Hall f32只接受表示误差。无法准确用新刻度表示的旧值（例如原gain12或1.003）保留，在帮助文字明确显示原倍率，数字框留空并隐藏尺上拇指；无关修改保存不阻塞，明确编辑新刻度才替换。数字框、刻度尺和本地预览文案使用新单位，预览计算及遥测仍用实际gain/累计量。公共契约与滑轨/旋钮模块已同步，见[本轮记录](../../web/tests/sensitivity-2026-10-06/README.md)。设备状态新SVG控制器与状态勾图标复用water-fill/blush-fill、圆润线条，导航和页标题共用art-device-status；原缺失符号引用已补齐。灯光移除百分比换算说明和 GP11/Test 指示灯编辑入口，完整草稿/快照中的 indicator 字段继续保留，固件限幅不变。

## UF2 检查与更新边界

连接后显示 hello 当前版本；选文件只在本地验证 UF2 结构及 RP2040 family、块编号和地址范围，限制在 4MiB 新板程序区内并排除公共配置保留区，再从连续 payload 中解析 FluxRail 识别头、板型和 SemVer 并自动比较。旧 UF2 无识别头不能识别；同版/可升级/版本偏低是版本比较提示，不是签名验证或升级成功证明。模拟版本不与真实设备比较；断连和设备序号变化会清除或要求重新选文件。低版本会提示，不代表阻止手动降级。

UF2当前版本放大显示，文件检查结果在更新模块。2026-10-08先前仅适配测试没有修复上下同步变化，用户再次提供错误文件截图后，本轮改 `update/controller.js` 落实上方固定、下方动态；以下为当前显示与测试预期。

| 节点 / 控制器参数 | 当前显示与测试检查 |
| --- | --- |
| `#current-firmware` / `current` | 单独显示未连接、真实固件版本或标明仅模拟的版本 |
| `#update-cap` | 当前设备进入下载模式的能力说明，由app按握手能力更新 |
| `#version-result` / `version` | 始终为“选择UF2后自动识别固件并比较版本。”，不显示动态消息或错误样式 |
| `#uf2-result` / `result` | 下方动态结果框：初始选择说明、读取进度、文件详情加连接/比较/模拟/换设备结果，或单一错误；仅此节点挂错误样式 |

| 状态 | 两区域分工 |
| --- | --- |
| 未选文件 / 清空选择 | result提示选择正式UF2；候选、文件摘要与错误清空，result移除错误样式 |
| 有效文件、未连接 / 断连 | result保留文件名、版本、板型、块数/字节数并提示“请连接设备”；current显示未连接 |
| 连接 / 同设备重连 | result按当前版本重算可升级/同版/过低/无法比较，保留文件详情；反复render不重复追加 |
| 模拟会话、有效文件 | result保留文件详情，提示“模拟会话不能用于比较实际设备版本，请连接真实设备。” |
| 更换设备序号 | result保留详情并提示设备序号改变，挂错误样式；重新选文件后按当前设备比较 |
| 无效 / 过大文件 | 清除旧候选/摘要，result只显示一次文件错误；清空选择后恢复正常状态 |

模拟提示只在UF2有有效候选、需要判断比较结果时出现；它与实验性功能中的模拟入口、首页设备信息、侧栏模拟连接状态各有用途。不能因入口迁移而删除这一断言，也不能让错误文件退回显示上一份有效文件详情。

`update/controller.js` 的 `show()` 固定version文字并清除其错误样式，只把动态文字与错误样式写入result。`fileSummary`保存本次有效文件详情，连接变化时与最新比较消息组合重建；`pendingText`保留读取中/未选文件状态，避免读取过程中出现上一文件结果。每次选文件先清候选、摘要、序号和错误。本轮保留两项控制器测试，检查所有状态中上方固定、文件详情保留且不重复、错误只出现一次、取消清错误以及模拟不判断真实升级；底层UF2/版本算法未改。

10/08此前仅适配测试的通过没有实现固定说明；本轮同时修实际代码并验证页面。10/06、10/07及此前10/08记录保留各自快照，不覆盖旧失败和SHA。

[实际修复记录](../../web/tests/uf2-fixed-result-2026-10-08/README.md)：完整Node73/73与Edge三宽度初始/错误/有效/清空检查通过；与[此前仅适配测试](../../web/tests/uf2-current-web-2026-10-08/README.md)区分。

2026-10-08后续反馈：`app.js`选正确UF2或清空文件时同步更新顶部操作提示并清错误样式，避免上一次文件失败提示残留。`#uf2-result`与顶部`#notice`采用14px、600字重；识别详情标签改为“型号”。文件拒绝及身份不匹配的页面提示统一为“固件版本不对：请重新检查固件文件”，原UF2结构/分区/身份校验和详细抛出错误保留。下载确认步骤使用“匹配此设备”；上方固定说明及IO4确认其他内容不变。Node73/73、Edge1274/390/320px错误→正确→清空、字号/字重、下载说明及无横向溢出检查通过，见[独立记录](../../web/tests/uf2-copy-readability-2026-10-08/README.md)。

步骤与备用 BOOT/RUN 方法集中在进入下载模式确认内；取消/Escape 不发送命令。Web Serial 不直接传送 UF2。操作流程仍是先保存所需配置、进入 ROM/BOOTSEL、用户手动复制匹配的新板 UF2、重连后检查身份/版本并做实板功能验证。复制到盘、读到版本或文件校验通过都不代表完整刷写/Flash 掉电/游戏验收。固件版本宏、hello 身份或识别格式发生变化时须同步固件和共同契约，按 UF2 契约构建；仅页面检查不能替代固件构建。

## 启动与完整现有测试命令

2026-10-08最新用户反馈调整：上一轮14px/600整段粗体过重，现行动态提示改为13px/400；UF2结果继承页面正文字体，背景及布局保持原有风格。拒绝文件/身份不匹配提示改为“固件文件检查未通过，请选择适用于本设备的 UF2 文件。”，避免把全部校验失败描述成版本号问题。固定说明、顶部成功/清空复位、型号与设备说明不变。Node73/73与Edge三宽度复核见[独立证据](../../web/tests/uf2-soft-style-2026-10-08/README.md)，前序记录保留。

Node 版本要求 `>=20`。没有 npm install、打包构建或 `dist/`；直接从 `web/` 运行：

```powershell
cd C:/Users/admin/Documents/FluxRail/web
node server.mjs
```

浏览器打开 `http://localhost:4174`。服务器只监听 `127.0.0.1`；自定义端口可设 `FLUXRAIL_WEB_PORT`。也可使用 package script `npm start`。修改服务器白名单/MIME 后要重启本项目服务器；HTML/CSS/JS 可刷新页面。

`package.json` 当前定义的完整 Web 软件测试命令及九个测试文件如下：

```powershell
cd C:/Users/admin/Documents/FluxRail/web
npm test
```

该命令等价于：

```powershell
node --test tests/web.test.mjs tests/knob-preview.test.mjs tests/integration.test.mjs tests/integration-ui.test.mjs tests/layout.test.mjs tests/template-actions.test.mjs tests/uf2.test.mjs tests/workflows.test.mjs tests/sensitivity.test.mjs
```

UF2专项使用 `node --test tests/uf2.test.mjs`。2026-10-08实际固定说明修复后完整Web为73/73通过（包含8项UF2），浏览器三宽度检查也通过，见[本轮记录](../../web/tests/uf2-fixed-result-2026-10-08/README.md)。保留npm测试发布门槛；文档/软件/浏览器检查不代表真实设备验收。

单文件语法检查入口（新增/改动的模块也应单独补查）：

```powershell
node --check src/app.js
node --check src/device.js
node --check src/workflows.js
node --check src/gain-control.js
node --check src/template-actions.js
node --check src/confirmation.js
node --check src/update/uf2.js
node --check src/update/controller.js
node --check server.mjs
```

2026-10-05生产重构已运行67项Node回归、改动模块语法检查、72种浏览器视图及隔离协议夹具流程，详情见[本轮证据](../../web/tests/refactor-2026-10-05/README.md)。GPT-6-Luna两轮4项已修正，第三次最终复核确认风险关闭，无阻断项。未执行固件编译、ARM模拟、设备串口、Flash、刷机、最终硬件或游戏；旧报告中的通过数仅表示当时状态。

后续吉祥物任务的独立验证见[2026-10-05视觉记录](../../web/tests/mascot-2026-10-05/README.md)：本轮 `npm test` 59项通过；Headless Edge检查1440/980/768/390/320px布局、图片解码、图标声明、未连接导航和27项HTTP检查，已人工查看桌面/手机截图与32px icon。既有44个 `src/`及测试文件与修改前备份SHA一致。未执行固件编译、ARM模拟、串口、裸板、最终硬件或游戏验证。

## 已验证与未验证

可引用的历史 Web/浏览器证据包括：布局记录中的 40 项 Node 回归和模拟/窄屏检查、物理布局记录中的相同 40 项及十二路布局、模板记录中的 51 项回归及隔离模拟确认流程、固件版本记录中的七套 Web 软件回归及浏览器选择正式 UF2 后解析出 0.10.0。它们分别见 [布局验证](../../web/tests/validation-layout-2026-10-04.json)、[物理布局验证](../../web/tests/validation-physical-layout-2026-10-04.json)、[模板验证](../../web/tests/validation-template-management-2026-10-04.json)、[固件版本验证](../../tests/integration/validation_firmware_version_2026-10-04.json)。这些是历史证据，不是本次复跑结果；固件版本验证文件也记录了没有上传、没有硬件测试。

上述Web软件快照较早，未涵盖后来真实串口试验。2026-10-05的[跨午夜裸板证据](../../tests/integration/validation_pc_board_candidate_2026-10-05.json)及[V4实际审核](../../tests/integration/pc_board_2026-10-04/webserial-offscreen-v4.actual.review.json)包含0.10.1隐藏Electron生产页通过真实Web Serial读写、保存/重连和配置恢复，chooser/权限为自动化适配器，原生Chrome/Edge交互选择器/UI未验收；[后续0.10.2证据](../../tests/integration/validation_io4_0_10_2_actual_2026-10-05.json)记录IO4超时复测及GP9第二次恢复普通通过，第一次失败保留。实测板2MiB，不能代表最终4MiB分区或掉电中断验收；普通HID payload读取error1、真实机械校准/运动、全功能UI/游戏仍未完成。当前分层结论统一见[测试状态表](../测试状态与待验收.md)。文档整理任务不执行设备操作，接手按用户任务范围分别记录软件、裸板、最终硬件与游戏。

用户补充的交付规则：每次正式编译UF2/ELF文件名带实际版本，正式入口导出到`tests/integration/build/releases/<版本>/<UTC时间>/`。Web仍从用户选择的文件读取实际识别头，不能只凭文件名推断版本；发布步骤见[运行发布](../../RUN_INTEGRATED.md)。

## Web 历史备份与恢复选择

当前唯一开发目录是项目根 `web/`。备份目录都标为历史快照，不包含设备 RAM/Flash、模板数据或整个项目：

| 目录 | 适用恢复点 | 范围 |
| --- | --- | --- |
| [web-before-layout-2026-10-04](../../backups/web-before-layout-2026-10-04/BACKUP-README.md) | 最初 Web；左右模块布局改版前 | 完整 `web/` 快照，28 个原文件 |
| [web-before-physical-layout-2026-10-04](../../backups/web-before-physical-layout-2026-10-04/BACKUP-README.md) | 已有左右布局，物理键位/遥测位置改动前 | 完整快照，35 个原文件 |
| [web-before-template-management-2026-10-04](../../backups/web-before-template-management-2026-10-04/BACKUP-README.md) | 物理摆位完成，模板确认/预设恢复流程前 | 完整快照，38 个原文件 |
| [web-before-header-image-2026-10-04](../../backups/web-before-header-image-2026-10-04/BACKUP-README.md) | 模板流程完成，页头 PNG 图片替换前 | 仅 `index.html`、`style.css`、`server.mjs` 三文件，不能独立启动 |
| [web-before-production-refactor-2026-10-05](../../backups/web-before-production-refactor-2026-10-05/BACKUP-README.md) | 当前水色视觉、生产结构与动作重构前；含既有未提交成果 | 完整 web/ 74文件及4份相关文档，78项SHA核验；清单为manifest.json |
| [web-before-mascot-2026-10-05](../../backups/web-before-mascot-2026-10-05/BACKUP-README.md) | 水滴标志、兔子控制器插画及新增icon之前 | 完整 `web/` 快照，50个原文件，复制时逐项SHA/大小校验 |

选择最符合目标状态的恢复点；要回到最初旧界面选 before-layout。恢复前按[备份总说明](../../backups/README.md)先备份并校验当前版，逐项校验所选清单 SHA-256，在空暂存目录按清单准确恢复、检查对应版本，再替换当前 `web/`。完整恢复不可直接覆盖复制，否则会遗留新版本文件形成混搭；局部换图快照只恢复列出的三个文件并重新检查与当前 `src/` 的兼容性。不要复制 `BACKUP-README.md`/清单到运行目录。恢复页面不会回滚设备配置或刷写固件。

## 参考与归档来源

- [模块4规格（归档）](../archive/2026-10-05/spec/05_模块4_Web上位机.md)
- [模块4进度（归档）](../archive/2026-10-05/progress/模块4.md)
- [旧 Web 开发与调整交接（归档）](../archive/2026-10-05/handoffs/FluxRail_Web_开发与调整交接.md)
- [Web README](../../web/README.md)：页面概览、功能边界及启动说明。
- [备份总说明](../../backups/README.md)：快照清单、哈希校验和安全恢复步骤。
- [布局评审记录](../../web/tests/layout-review-2026-10-04.md)、[模板测试输出记录](../../web/tests/layout-test-output-2026-10-04.txt)、[水色视觉评审](../../web/tests/style_review_2026-10-03.md)：解释 UI 决策和当时的验证范围。
- 协议、完整快照、IO4/schema2、整合运行和 UF2 格式的权威字段/命令以归档的 `spec/01`、`spec/12`、`spec/13`、`spec/14`、`spec/15` 及当前固件 CDC 文档为准；页面不自行发明协议或存储结构。

2026-10-06补充检查：九套Node共72项通过；Hall13组/7716检查、旋钮15组/6701检查及各6单元严格ARM编译通过；Edge1274/390/320px的72视图/10检查组通过，覆盖新刻度单次换算/保存再读回、旧值与旧精度、额外小数/箭头0.1、SVG绘制、原确认和锁。详见[灵敏度与图标验证](../../web/tests/sensitivity-2026-10-06/README.md)。没有正式固件编译、真实串口、板测/游戏操作。
