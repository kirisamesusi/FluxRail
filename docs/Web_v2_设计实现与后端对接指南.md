# Web v2 设计实现与后端对接指南

更新：2026-10-11（北京时间）。对应设计包 [HANDOFF](../design/web-v2-2026-10-11/HANDOFF.md) / [DESIGN](../design/web-v2-2026-10-11/DESIGN.md) / [实现记录](../design/web-v2-2026-10-11/IMPLEMENTATION.md)。本文面向“以后改固件/协议，要同步改网页”的人：先读第 1～3 节弄清网页怎么用设备，再按第 6 节清单改。现行页面与测试状态的总入口仍是 [05 Web配置工具](modules/05_Web配置工具.md)。

**验证口径（务必分开）**：Web Node 80/80 与模拟演示截图是软件/DOM 层；固件 UTF-8 名称只在 x86 主机编译验证（96 项），**未做 ARM 编译、未刷机、未实板**。见[证据目录](../tests/integration/utf8_name_2026-10-11/README.md)和[测试状态表](测试状态与待验收.md)。

## 0. 后端现在要做什么（一页速览）

| 项 | 后端是否要动 | 状态 |
| --- | --- | --- |
| 模板名支持中文（UTF-8） | **要**：固件已改（0.10.3），需**用户正式ARM编译→刷机→实测** | 源码完成；主机96项通过；ARM/实板未做。步骤见[窗口必读“待办”](FluxRail_开发交接_窗口必读.md)，记录见[固件升级日志](固件升级日志.md) |
| 删除 PUSH（P1/P2） | 不动：固件/协议/schema保留，网页只是不显示，保存时原值带回 | 用户确认：PUSH按键硬件暂时取消 |
| 逐键亮度 | 不动：设备没有该字段，网页只提供逐键颜色，未新增协议字段 | 已核对 |
| 改动即时生效 | 不做：缺“丢弃RAM回到Flash”命令；若将来固件加此命令才可重评 | 保持“先改草稿、再应用并保存” |
| 色球固定 | 不涉及后端（浏览器本地） | 已做 |

规则：固件每次有改动并重新编译＝一次升级，先升 `FirmwareInfo.h` 版本并写升级日志；同源码重编译不升版本。

## 1. 页面与源码地图

三页签（`layout.js`）：设备状态 `#device-status` / 固件 `#update` / 实验性功能 `#experimental`；旧片段 `#keyboard #lighting #hall #knobs #profiles #diagnostics #input-status` 都回设备状态。三页共用同一 DOM 与同一份草稿，切页不重建。

设备状态页 = 左栏（模板列表 `#profile-list`、设备信息、历史操作 `#history`）+ 中间设备面板（10 键 `#keysL/#keysR/#keysB`、旋钮 `#knobL/#knobR`、滑轨 `#slider`）+ 右侧检查器 `#inspector`（随选中对象重绘：按键 / 旋钮 / 滑轨 / 模板）+ 顶部气泡 `#bubble` 与芯片 `#chips`。

| 文件 | 职责（v2 新增/重写已标注） |
| --- | --- |
| `index.html`、`style.css` | v2 重写；控件 ID 被 app/integration/io4-editor 引用，改 ID 须全局核对 |
| `src/app.js` | v2 重写。单页编排：`h()` DOM 助手、气泡/历史、设备面板、检查器、灯光、模板、实验页字段、连接/应用/读回流程、rAF 信号循环 |
| `src/colors.js` | 新。色球：三个槽（可固定）+ 调色盘，全局色与按键色各一套，存浏览器 localStorage（try/catch，不进设备/模板/快照） |
| `src/history.js` | 新。`changeList(base,draft)` 把草稿与设备读回的差异转成中文改动清单（颜色、按键动作按整体算一处） |
| `src/mascot.js` | 新。读 `assets/mascot-lines.txt`（一行一句，`#` 注释），缺失用内置台词，不连续重复 |
| `src/keyboard.js` | 加 `actionShort/actionText`（按键面板与历史用） |
| `src/knob-preview.js` | 保留纯函数 `projectPreview`；新增 `attachManualKnob`（仅模拟演示“手动测试”） |
| `src/template-actions.js`、`confirmation.js` | 默认新建名 `Custom N`（ASCII，兼容旧固件）、`nameHint`；删除按钮 `danger` 样式；名称规则说明 |
| `src/integration.js` | 去掉旧遥测圆盘；导出 `getMotion()` 供设备面板读 `get_motion` 最近快照；UF2 入口复用静态 `#integration-bootloader` |
| `src/io4-editor.js` | 只显示 BT1～BT10；id 10/11（P1/P2）的选择器保留在 DOM 外，保存时原值带回 |
| `src/config.js` | 名称规则改 UTF-8：`validName`（新建/改名，1～10 字符）、`validStoredName`（读回，旧 ASCII 最长 31 仍合法）、`utf8Length`、`NAME_RULE_TEXT` |
| `src/connection/client.js` | 请求行经 `asciiJson`，非 ASCII 转 `\uXXXX`，线路保持纯 ASCII |
| `src/snapshot.js`、`src/demo.js` | 名称按 UTF-8 解码/编码 |
| `src/device.js`、`workflows.js`、`gain-control.js`、`sensitivity.js` | **未改**。草稿 → `set_profile` → 读回 → `save_config` 的唯一通道 |
| `server.mjs`、`deploy/prepare-site.mjs` | 白名单加 v2 图片与 `assets/mascot-lines.txt`，MIME 加 `.txt` |

## 2. 状态模型（改后端时最容易踩的点）

网页草稿（`device.draft`）→ 设备 RAM（`apply`＝`set_profile` + `expected_revision`，随后完整读回）→ Flash（`save_config`）。三者独立；顶栏“应用并保存”＝先 apply，再必要时 `set_startup_usb_mode`，最后 `save_config`。**设计包的“改动即时生效”未实现**：设备无“回退到已保存 Flash”命令，且有未保存修订时 `reboot_to_saved_mode` 被 MODE_NOT_SAVED 拒绝，即时写 RAM 会让用户无法放弃。若将来固件增加“丢弃 RAM 改动回到 Flash”命令，才可重评。

气泡常驻状态由 `baseBubble()` 推导（未连接 / 已断开 / 需重新读取 / N 处修改未应用 / 已应用未保存 / 一致）；短提示 `say(kind,text)` 约 5 秒淡出，错误不自动消失。`pendingCount()`＝普通草稿改动数 + 启动模式草稿 + IO4 草稿。

## 3. 控件 ↔ 字段 ↔ 协议

| 界面控件 | 草稿字段（`device.draft`） | 写入/读取协议 |
| --- | --- | --- |
| 按键动作下拉 / 监听 / 实验页字符 | `config.keyboard.bindings[id].action`（`keyboard{usage,modifiers}` / `mouse_button{button}` / `disabled`） | 随 `set_profile`（无 profiles 能力则 `set_config`） |
| 灯效选择 | `config.lighting.mode` + `per_key` | 同上。**“自定义”＝`per_key=true,mode='static'`**，与动画互斥；其他灯效 `per_key=false`。旧配置“动画+逐键色”原值保留并提示 |
| 亮度 | `lighting.brightness_level` | 同上（无逐键亮度字段） |
| 全局色 / 按键色球 | `lighting.global_color` / `key_colors[id]`（RGB 三元组） | 同上；色球槽内容只在浏览器本地 |
| 旋钮 启用/反转/轴/灵敏度 | `motion.knobs[i].{enabled,invert,axis,gain}` | 同上。灵敏度显示 1.0～10.0，写入值＝显示值/5（`gain-control.js`/`sensitivity.js`） |
| 滑轨 启用/反转/中心灵敏度 | `motion.hall.{enabled,invert,center_gain}` | 同上 |
| 模板列表/设为活动/保存/改名/删除/恢复预置 | 经 `templateAction()` | `list_profiles` `get_profile` `create_profile` `restore_preset` `set_profile` 等（设备端已有命令，网页不新增命令） |
| 实验页（扫描/重触发、灯周期与反向、滑轨高级、旋钮高级、来源策略） | `config.scan/keyboard.retrigger_gap_us/lighting.period_ms|reverse`、`motion.hall/knobs/routing` | 同上；PUSH 参数不在界面显示，底层数据原样随保存带回 |
| IO4 映射 | 独立 IO4 草稿（12 项，界面只改 BT1～BT10） | `set_io4_bindings` → 共享 `save_config` |
| 按键“按下”高亮 | 无（只读） | `get_status.physical_mask`（位 i＝来源 id i），沿现有轮询：约 500 ms；有 `motion_telemetry` 能力时约 100 ms |
| 旋钮/滑轨指示 | 无（只读） | `get_motion`：`knobs[i].raw_turns/scaled_turns/valid`、`hall.p`（原生位置 0～1）、`hall.output`（灵敏度/反转后 0～1），指数缓动显示；模拟演示另有“演示动画/手动测试” |

按键键盘监听只听键盘（`keydown`，Esc 取消，blur/切页/换选中退出）。

## 4. 面板与检查器的更新规则
- 按键按钮原地更新（`updateKeys()`），不重建，避免拖动颜色时丢焦点；检查器重绘用 `renderInspector({keepFocus:true})` 保持焦点。
- 编辑一律走 `mark()`：重算 `localDirty`、刷新面板/灯光/芯片/气泡/按钮锁，并去抖约 0.6 秒写入历史。
- 模板预设（id 1～4）不可改名/覆盖/删除；使用中的模板不可删除；有未应用草稿时不能切换/改名/删除/设活动。

## 5. 模板名 UTF-8 链路（已改固件，未实板）
网页 `validName`：1～10 个字符、UTF-8 ≤ 31 字节、无首尾空格；拒绝控制/DEL/C1、零宽与双向控制、引号、反斜杠。→ `asciiJson` 把非 ASCII 转成 `\uXXXX`（含代理对）发出 → 固件 `Json::utf8Value` 解码 → `ProfileManager::validName` 校验（UTF-8 1～3 字节序列、≤31 字节）→ 存入原 32 字节名称槽 → 回复时 `JsonWriter::name` 再转义为 `\uXXXX` → 网页 `snapshot.js`/响应解码。schema 与 Flash 布局不变。**旧固件**会拒绝中文名，因此默认新建名保持 ASCII；用户手动输入中文时才依赖新固件。**兼容**：既有 ASCII 名（最长 31 字符）仍合法，网页读回用 `validStoredName`。

## 6. 以后更新后端：改动清单
1. **新增/修改配置字段**：固件 `DeviceConfig`/`ProfileCodec`/校验 → 网页 `config.js`（`validateConfig/validateMotion`、`hallFields/knobFields`）→ `snapshot.js`（schema 解码）→ `demo.js` 模拟器 → `history.js`（让改动清单有可读文案，否则会走兜底文案）→ 控件放哪：常用项进 `keyInspector/knobInspector/sliderInspector`，进阶项进 `renderEditors()` 的 `field()/numbers()` → 测试（`web.test.mjs` 校验与往返、`v2.test.mjs` 清单）→ 同步[公共契约](公共架构与配置契约.md)与受影响模块文档。schema 变化要写兼容/迁移。
2. **新增 CDC 命令**：固件 `ConfigProtocolDevice.cpp` → 能力位写进 `hello.capabilities` → 网页 `client.request` 仅在能力存在时使用（学 `capabilities.profiles` 的写法）→ 失败、超时、revision 冲突路径必须走“完整读回再放行”（`device.ready=false`）→ `demo.js` 如需演示再加，但**模拟器不得声明真实 Hall/旋钮/IO4/校准/bootloader 能力**。
3. **改名称规则**：网页 `config.js`（`NAME_RULE_TEXT` 同步给用户的说明文字）与固件 `ProfileManager::validName` 必须一致；固件改动用 `tests/integration/utf8_name_2026-10-11/utf8_name_tests.cpp` 为模板重跑主机测试并在新日期目录留证据。
4. **遥测**：`get_motion` 字段改名要同步 `integration.js`（`pollIntegration`、`getMotion`）与 `app.js` 的 `computeTargets()`；原生值取 `hall.p` / `raw_turns`，灵敏度后取 `hall.output` / `scaled_turns`。
5. **按键数量/编号变化**：`BT=[7,6,5,4,3,2,1,0,8,9]`（面板从左到右）与 `keyName`（`id<10 → BT(id+1)`，否则 P1/P2）在 `app.js` / `history.js`；`source_id`、绑定数组、逐键颜色索引不随显示顺序变化。
6. **新增版本/UF2**：固件版本只来自 `FirmwareInfo.h`；UF2 文件名带版本；网页仍读 UF2 实际头，不靠文件名（`update/uf2.js`）。
7. **提示文案**：以仓库原文为准（HANDOFF §4/§5 对照表），设计包原型文字冲突时让位；弹窗正文在 `workflows.js`/`template-actions.js`/`integration.js`。
8. **看板娘台词**：只改 `web/assets/mascot-lines.txt`，无需改代码。
9. **部署**：新增静态资源要同时加进 `server.mjs` 的 `publicAssets` 和 `deploy/prepare-site.mjs` 的 `assets`。

## 7. 不变量（不要破坏）
- 线路保持纯 ASCII、单行 ≤ 4096 字节；设备字段一律 `textContent`，不注入 HTML。
- 只通过 `Device.apply/mutate` 写设备；任何写失败或结果未知 → 先完整读回，不盲重试。
- 草稿、RAM、Flash 三态不混；`localStorage` 只存色球槽等 per-viewer 便利项，不存设备配置。
- 冻结证据不改：`tests/**` 既有证据目录、`docs/archive/**`、旧 SHA。注意 `python docs/tools/verify_docs.py` 不带参数会改写 `docs/archive/2026-10-05/DOCUMENTATION_CHECK.json`，请始终加 `--output <新日期证据目录>/xxx.json`。

## 8. 命令
- Web：`cd web && npm test`（当前 81 项）；本地 `node web/server.mjs` → `http://localhost:4174`，用“实验性功能 → 模拟演示”预览。
- 固件主机测试：在 `firmware/FluxRail/src` 下按[证据 README](../tests/integration/utf8_name_2026-10-11/README.md) 编译运行。
- 正式固件：`python tests/integration/compile_firmware.py`（未在本轮执行）。

## 9. 未决与未验证
- HANDOFF §8 第 4 项（色球）：**用户 2026-10-11 已决定：做“固定”，不做“改名”**。每个常用色球下有“固定/已固定”小按钮，被固定的球不被调色盘新颜色顶掉（从下一个球起跳过已固定的球；三球全固定时新颜色只用于当前选择、不进球）；固定状态随色球槽存浏览器 localStorage（`colors.js` 的 `pinned`，旧存档缺该字段按未固定处理）。
- IO4 的 P1/P2（source id 10/11）在界面隐藏：**用户 2026-10-11 已确认**，原因是这两个 PUSH 按键硬件暂时取消。固件映射仍保留，网页保存时原值带回；将来硬件恢复，只需在 `io4-editor.js` 把这两项重新显示。
- 固件 UTF-8 名称：缺 ARM 编译、UF2、实板创建/改名/保存/断电重读。
- 按键“按下”显示是 2～10 Hz 状态位图，不是事件流；短按可能漏显示，不能用来判断吞键。
