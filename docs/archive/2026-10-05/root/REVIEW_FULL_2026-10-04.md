# FluxRail 全项目代码审查报告

- 审查对象：`C:\Users\admin\Documents\FluxRail`（RP2040 / Arduino-Pico 6.1.0 + Adafruit TinyUSB 3.7.7 私有适配）
- 审查日期：2026-10-04
- 审查范围：固件全量源码（11 个模块，约 7,964 行 C++）、Web 上位机（约 1,292 行 JS/MJS）、测试与文档、依赖注入脚本
- 审查方式：**逐文件通读 + 实际运行验证**（非纯静态阅读）
- 上一轮记录：`tests/integration/REVIEW_INTEGRATION_2026-10-03.md`（只读审查，未发现可复现阻断缺陷）

---

## 一、结论摘要

> **未发现阻断级（Blocker）或严重级（Critical）缺陷。** 代码可以直接进入实板联调阶段。

本项目在工程质量上明显高于同类个人项目：

- **不变量被系统性维护**：`config::validDevice()` / `hall::validateCalibrationStructure()` / `lighting::validLedOrder()` / `keyboard::validConfig()` 构成一道"拒绝非法状态进入运行期"的闸门，配置变更、Flash 导入、IO4 映射全部复用它。
- **并发模型自洽**：单生产者-单消费者队列的 release/acquire 配对正确；跨核唯一所有权（核心0 = USB/路由/配置/灯，核心1 = 采集）没有第二生产者。
- **Flash 可靠性做到"提交后置 + 写后读回"**：commit 块最后写入，掉电不会产生"半个有效槽"；两槽交替 + 半范围 generation 比较，无 ABA 问题。
- **看似可疑但实测正确的实现**（已逐一核实，见第五节），不存在"改一处崩一片"的隐雷。

发现的问题全部为**一般级 / 建议级**，共 4 条，均不阻塞联调。

软件验证状态：**Web 单元测试 35/35 通过；集成测试 11 组全部通过**（`passed: true, status: complete`）。
实板 / Windows HID / 游戏 / Flash 掉电：**未验证**（`hardware_tested: false, game_tested: false, uploaded: false`）。

---

## 二、审查方法与本次实际执行的验证

| 验证项 | 命令 | 结果 |
|---|---|---|
| Web 单元测试 | `node --test tests/web.test.mjs tests/knob-preview.test.mjs tests/integration.test.mjs tests/integration-ui.test.mjs` | **35 passed / 0 failed** |
| 集成测试套件（11 组） | `python tests/integration/run_all.py --skip-build` | **11/11 exit_code = 0**，`PASS: complete software validation` |

集成套件 11 组明细（全部 exit 0）：
`module1` / `module2` / `module3` / `integration` / `transport` / `protocol` / `storage` / `app_storage` / `io4` / `motion` / `web_all`

静态核对手段：全量通读 + 关键算法逐行推演 + 与 linker 产物（`memmap_default.ld`）、SDK 源码（`hardware/spi.c`）、TinyUSB 适配补丁（`dependencies/prepare_tinyusb.py`）、Arduino 核心入口（`main.cpp`）交叉比对。

代码规模：

| 区域 | 行数 |
|---|---|
| firmware `src/`（11 模块 87 个文件） | 7,964 |
| app | 1,263 |
| protocol | 1,835 |
| hall | 1,088 |
| usb | 954 |
| knobs | 694 |
| config | 669 |
| keyboard | 549 |
| lighting | 375 |
| realtime | 252 |
| common | 249 |
| board | 27 |
| web `src/` | 1,055 |
| web 全部 JS/MJS（含测试） | 1,292 |

---

## 三、问题清单

### 一般级（建议在实板联调前顺手处理）

#### G-1　`App::applyRuntimeConfig()` 空指针依赖隐式不变量

- **位置**：`firmware/FluxRail/src/app/App.cpp:70`
- **代码**：
  ```cpp
  candidate_ = device_;
  config::find(candidate_, candidate_.active_id)->runtime = c;   // ← 未判空
  ```
- **根因**：正确性完全依赖 `validDevice()` 保证 `active_id` 一定可解析。当前所有进入路径都满足该不变量（`defaults()` 造出 1..4 号模板；`store_.load()` 仅在 `validDevice` 通过后赋值），因此**当前不可达**。
- **影响**：仅防御性缺口。本项目其余同类位置（`App::applyDevice`、`App::serviceMotion`、`ProfileManager`）都显式判空，此处风格不一致，未来重构时容易踩坑。
- **修复**：
  ```cpp
  auto *p = config::find(candidate_, candidate_.active_id);
  if (!p) return false;
  p->runtime = c;
  ```

#### G-2　`FlashRp2040::program()` 的 SRAM 上界硬编码

- **位置**：`firmware/FluxRail/src/config/FlashRp2040.cpp:46`
- **代码**：
  ```cpp
  if (uintptr_t(data) < 0x20000000 || uintptr_t(data) > 0x20042000 - len) return false;
  ```
- **根因**：`0x20042000` 是 RP2040 全部 SRAM（256 KiB 主 RAM + 8 KiB scratch）的末端。该检查意图正确——拒绝把 XIP Flash 或非法地址当作 Flash 写源——但数值写死。同一个类的 `available()` 却全部通过 linker 符号校验（`__flash_binary_end` / `_EEPROM_start` / `_FS_start`）。
- **影响**：不构成当前缺陷（RP2040 固定）。若将来改 RAM 布局或移植到 RP2350，会静默失效或误拒合法写入。
- **修复**：改用 linker 符号表达上界（如 `&__StackLimit` / `__bss_end__` 派生），或补一条 `static_assert` 把该常量与 `memmap_default.ld` 的 `RAM LENGTH` 绑在一起。

#### G-3　CDC 应答发送无超时兜底（依赖主机的重连行为）

- **位置**：`firmware/FluxRail/src/protocol/ConfigProtocol.cpp:455-464`
- **现象**：应答按每轮 ≤64 字节分片续传，`port.write()` 返回 0 时保留 `tx_sent_` 并在下一轮重试；此分支 `return` 在 `service()` 最前部，因此**只要 `tx_size_` 常驻非零，其后所有请求（包括 `get_status`）都不会被处理**。
- **已核实的安全性**：断连/重连会走 `resetSession()`（`ConfigProtocol.cpp:443-452`）清空缓冲；Web 客户端 `web/src/connection/client.js` 对每个请求设 2,000 ms 超时，超时即 `abort()` → `close()` 关闭串口，从而必然触发 `resetSession()`。**因此对官方 Web 上位机不会卡死。**
- **残留风险**：若使用者用自写脚本（Python/pyserial 等）打开端口后停止读取又不关闭连接，设备侧会一直等待，期间不再响应任何新请求。
- **建议**：① 在 `CDC_PROTOCOL.md` 中把"主机必须设置应答超时并在超时后关闭端口"写成显式契约；② 或给 TX 加一个"自最后一次成功写入起 N 秒未推进则丢弃本行并计入 `stats_`"的兜底计数器。

### 建议级

#### S-1　Web：`document.getElementById` 未判空会导致整块面板静默消失

- **位置**：`web/src/io4-editor.js:47`（`document.getElementById('experimental-content').insertBefore(...)`）、`web/src/app.js` 同类调用
- **影响**：`index.html` 结构一旦调整，脚本抛 `TypeError` 会中断 `mountIo4Editor` / `mount...` 的剩余逻辑，表现为"IO4 面板凭空没了"，没有任何错误提示。
- **修复**：判空并给出可见提示，例如
  ```js
  const host = document.getElementById('experimental-content');
  if (!host) { console.warn('IO4 面板挂载点缺失'); return { sync(){}, hasDraft:()=>false }; }
  ```

#### S-2　`valid` 与 `enabled` 的语义分层需要显式注释

- **位置**：`firmware/FluxRail/src/knobs/KnobMotion.cpp:88`（`out.valid = true` 在 `settings_.enabled == false` 时也成立，`delta_counts` 为 0），实际过滤发生在 `firmware/FluxRail/src/app/MotionRouter.cpp:94`（`!m.enabled` 直接丢弃）。
- **结论**：功能正确，已核实。"禁用来源不会接管轴"这一关键性质由路由层单独保证，不依赖 `valid`。
- **建议**：在 `KnobMotion.h` 的 `Motion` 结构旁注明"`valid` 仅表示数据可用，是否可路由由 `enabled` + 路由层判定"，避免后续有人在路由层去掉 `!m.enabled` 判断。

#### S-3　正式固件构建当前不可从零复现

- **位置**：`tests/integration/compile_firmware.py:19`
- **现象**：`arduino-cli.exe` 路径硬编码为 `C:/Program Files/Arduino IDE/resources/app/lib/backend/resources/arduino-cli.exe`，且 `arduino-cli` 不在 `PATH`；本机复核时该命令不存在，仅能依赖 `tests/integration/build/` 中已存在的构建产物。
- **影响**：源码改动后无法一键重建 UF2 并复核"源码哈希 ↔ UF2"的绑定关系，削弱了 `source_link_audit` 的可信度。
- **建议**：把固定版本的 `arduino-cli` 纳入仓库工具目录（或写清安装步骤与校验值），并在 README 里给出单条可复制命令。

#### S-4　`tests/integration/build/`（21 MB）构建产物混在源码树内

- **现象**：目录内同时存放 `.uf2` / `.elf` / `.o` / `.su` / 日志 / 截图（如 `FluxRail.ino.uf2`、`integration_ui.png`）。仓库当前**未初始化 git**（无 `.git` / `.gitignore`）。
- **风险**：`HARDWARE_CHECKLIST.md:46` 自己就提示过"历史阶段产物不当成本次固件"——这正是产物入库带来的误用风险。
- **建议**：初始化版本控制并忽略 `build/` 下除结果 JSON 与审查记录以外的内容；或在目录顶层放 `README` 明确"产物非权威，权威是 `firmware_results.json` 中的源哈希"。

---

## 四、逐模块审查记录

| 模块 | 关键核对点 | 结论 |
|---|---|---|
| `board/BoardConfig.h` | 引脚表、灯链序、扫描/灯周期、超时常量 | 与 `docs/spec/02` 表格一致；灯链 `{8,7,6,5,4,3,2,1,0,9}` 与用户 2026-10-03 确认一致 |
| `app/App.cpp` | 双核启动顺序、看门狗、恢复状态机、维护门槛 | 正确（详见第五节） |
| `app/Acquisition.cpp` | Pause→discard→Resume 事务、校准结果发布条件 | 正确；`resume()` 失败时回退 `pause()`，不留半应用状态 |
| `app/MotionRouter.cpp` | 单轴单所有者、同批平局、小数分段、背压 | 正确；`next()`/`subtract()` 的头指针语义经逐段推演验证 |
| `app/ConfigCoordinator.cpp` | epoch/revision 校验、物理恢复两阶段 | 正确；`epoch_` 保留 0 为"未启动"哨兵 |
| `realtime/SpscQueue.h` | 满判定、release/acquire 配对、回绕 | 正确；`head - tail == Capacity` 在 32 位回绕下成立 |
| `realtime/InputWorker.cpp` | Pause/Resume 净变化补发、序列号分配 | 正确；`last_sequence = UINT32_MAX` 是**已在头文件注明**的有意哨兵 |
| `keyboard/KeyScanner.cpp` | 候选/稳定双掩码消抖、时间回绕 | 正确 |
| `keyboard/ActionMapper` | 重触发间隔、NKRO 位图 | 正确 |
| `hall/HallTypes.cpp` | 折线投影、歧义判定、结构校验 | 正确；`positions`/`validateCalibration` 的 5 点采样检查逻辑自洽 |
| `hall/HallCalibration.cpp` | Welford 在线统计、分阶段采集、MapCheck 预算分摊 | 正确；`node_count < kMinNodes(7)` 时提前 `fail`，MapCheck 不会越界访问 |
| `hall/HallPosition.cpp` | 饱和/弱场/离曲线/活动判定、滤波 | 正确；分母 `right_mm - left_mm ≥ 1` 有校验保证 |
| `knobs/Mt6701SsiRp2040.cpp` | SSI 手动初始化、CPOL/CPHA、GPIO 复用 | 正确（详见第五节） |
| `knobs/KnobMotion.cpp` | 角度展开、歧义/间隙拒绝、int64 溢出防护、滤波 | 正确；`INT64_MAX/MIN` 边界检查无有符号溢出 |
| `lighting/KeyLighting.cpp` | 亮度换算、限幅、位序打包 | 正确；`positions[]` 与物理布局 **完全一致**（详见第五节） |
| `usb/UsbBackend.cpp` | 弱符号钩子注册、互斥、IO4/normal 分派 | 正确 |
| `usb/Io4Codec.cpp` | 描述符 ↔ 载荷布局、OUT 归一化 | 正确；`static_assert` 与描述符 63 字节一一对应 |
| `usb/NormalHidCodec.cpp` | NKRO 位图保留区屏蔽 | 正确；`0x00-0x03` 与 `0xA5-0xDF` 屏蔽位运算推导无误 |
| `config/ConfigStore.cpp` | 双槽 CRC、commit 后置、写后读回、generation 半范围比较 | 正确（详见第五节） |
| `config/ProfileManager.cpp` | 模板增删改、预置保护、ID 单调性 | 正确；`create/delete` 边界（12 容量、`UINT32_MAX`）都做了检查 |
| `protocol/ConfigProtocol.cpp` | 行长上限、丢弃态、pending 状态机、TX 续传 | 正确；唯一观察见 G-3 |
| `protocol/Json.cpp` | 令牌/深度/长度上限 | 正确；`parse(rx_, rx_size_)` 显式传长度，不依赖 `NUL` 终止 |
| `web/src/*` | XSS、草稿/读回语义、revision 一致性 | **无 `innerHTML` / `insertAdjacentHTML` / `eval` / `new Function` / `document.write`；无 `localStorage` 伪造持久化**；全程 `textContent` + `createElement` |
| `web/server.mjs` | 静态服务、路径穿越、CSP | 正确；白名单 + `startsWith(root)` + 扩展名白名单 + CSP 头，穿越尝试返回 404 |

---

## 五、已核实"看似可疑但实际正确"的实现（避免重复排查）

以下 7 处是代码审查中最容易被误报为缺陷的地方，本次已逐一核实为**正确**，记录在此以免下一轮重复投入：

1. **`board/KnobPins` 与实际 SPI 外设的对应**
   `knob[0] = {rx:12, cs:13, sck:14, push:19}`、`knob[1] = {rx:16, cs:17, sck:18, push:20}`，与 `Mt6701SsiRp2040.cpp` 中 `spi1`/`spi0` 的使用一致（GP12/13/14 属 `spi1`，GP16/17/18 属 `spi0`）。
   另：`spi_init()` 只设 CPOL_0/CPHA_0，随后立即被 `spi_set_format(spi, 8, SPI_CPOL_1, SPI_CPHA_0, SPI_MSB_FIRST)` 覆盖，且 pico-sdk 的 `spi_init` 不触碰 GPIO 功能选择，因此手动 `gpio_set_function(p.rx, GPIO_FUNC_SPI)` 是 SSI 手工初始化的正确做法。

2. **USB 后端在 `setup()` 之前注册**
   Arduino-Pico `main.cpp` 中 `TinyUSB_Device_Init(0)`（约第 148 行）先于 `setup()`（约第 181 行）执行，而 `fluxrail_usb_wants_cdc()` / `fluxrail_usb_configure_before_connect()` 是私有适配注入的弱符号钩子；`dependencies/prepare_tinyusb.py` 注入的 `__usb_mutex` / `fluxrail_cdc_tx_idle` / `FLUXRAIL_TINYUSB_STARTUP_ADAPTER=2` 与 `Adafruit_TinyUSB_rp2040.cpp` 的用法一致，并在 `compile_firmware.py` 中用 SHA256 固定，不会被静默替换。

3. **`FlashRp2040::available()` 的三条断言与 linker 产物一致**
   生成的 `memmap_default.ld` 中 `_EEPROM_start = _FS_start = _FS_end = 272625664 = 0x103FF000`，即 `XIP_BASE + 0x3FF000` ✓；`FLASH LENGTH = 4173824 (0x3FB000)` 把代码区限制在 `0x3FB000` 以下 ✓。两个配置槽 `0x3FB000` / `0x3FD000` 恰好落在代码区末端与 EEPROM 之间的 16 KiB 空隙里，彼此独立且不与 EEPROM 重叠。设计正确且运行时自校验。

4. **Flash 擦写期间的并发与看门狗**
   `save_and_disable_interrupts → rp2040.idleOtherCore → flash_range_erase/program → resumeOtherCore → restore_interrupts` 是 pico-sdk 规定的多核安全顺序；每个扇区/页前都 `watchdog_update()`，2 s 看门狗在 4 KiB 擦除（约 30–50 ms）耗时下余量充足。

5. **跨核队列的内存序**
   `head_`/`tail_` 分别在生产者/消费者侧用 release 发布、acquire 读取，`alignas(4)`；RP2040 为 Cortex-M0+，`dmb` 指令可用，无 D-cache 故不存在伪共享正确性问题；`clearConsumer()` 由消费者独占写 `tail_`，符合注释承诺的前置条件（先拿到生产者暂停 ACK）。

6. **NKRO 位图保留区屏蔽**
   `out[1] &= 0xf0` 清除 usage `0x00-0x03`；`out[1+20] &= 0x1f` 清除 `0xA5-0xA7`；其后的字节清零覆盖 `0xA8-0xDF`。位索引推导（字节 = usage>>3，位 = usage&7，`+1` 为修饰字节偏移）全部正确。

7. **`KeyLighting::renderLights()` 的 `positions[]` 与物理布局完全吻合**
   `docs/spec/02` 确认 BT1..BT10 默认键为 `; L K J F D S A V N`，即 QWERTY 左半 A/S/D/F/V = BT8/BT7/BT6/BT5/BT9，右半 J/K/L/;/N = BT4/BT3/BT2/BT1/BT10。据此推算各 BT 的物理左→右名次恰为 `{8,7,6,5,3,2,1,0,4,9}`，**与代码中的 `positions[]` 逐位一致**。
   它与 `BoardConfig::kLedChainToBt`（`{8,7,6,5,4,3,2,1,0,9}`）不同是**有意为之**：前者是物理空间排布（决定彩虹波扫过方向），后者是 WS2812 串联接线次序（用户 2026-10-03 另行确认）。两份排列都正确，不应合并。

8. **Web 层 XSS 与配置伪造**
   全量检索 `innerHTML | insertAdjacentHTML | outerHTML | dangerously | eval( | new Function | document.write | localStorage | sessionStorage`：唯一命中是 `web/src/app.js:57` 的注释（"不通过 localStorage 伪造设备掉电保存"）。所有 DOM 写入走 `textContent` 或 `svgNode()` 的 `setAttribute`。配置草稿与实际持久化严格分离，`save_config` 是唯一落盘入口。

---

## 六、验证覆盖矩阵（关键：已跑 vs 未跑）

| 层次 | 内容 | 状态 |
|---|---|---|
| 单元 | Web 逻辑（35 项：配置校验、快照编解码、CRC、UF2 校验、旋钮预览、IO4 编辑器） | ✅ 已通过 |
| 单元 | 模块1/2/3 C++ 测试（经 Unicorn2 模拟 ARM 执行） | ✅ 已通过 |
| 集成 | 传输层、协议层、存储层、app+存储、IO4、运动、web_all | ✅ 已通过 |
| 链接 | 严格编译（`-Werror`，仅第三方 `missing-field-initializers` 降级）+ 唯一强符号 USB 回调 + 核心1 入口指向项目函数 + 栈帧 ≤2 KiB + 存储分区保留 | ✅ 已通过（`source_link_audit.json`） |
| 实板 | 枚举、键位、灯序/GRB、断连恢复、看门狗 | ❌ **未执行** |
| 实板 | IO4 首次上电即 0CA3:0021、长字符串、64 B IN/OUT、1 ms | ❌ **未执行** |
| 实板 | 普通 ↔ IO4 冷切换（≥20 次）、维护/保存并发（≥50 次）、两槽掉电 | ❌ **未执行** |
| 主机 | Windows HID 解析、游戏兼容性 | ❌ **未执行** |

---

## 七、未验证清单（联调必须覆盖，摘自 `tests/integration/HARDWARE_CHECKLIST.md`）

以下为**项目自身已明确声明未做**的验收项，审查无法替代，请按顺序推进：

1. **上传前**：连接目标板、确认 4 MiB Flash 与接线、明确授权后再刷 `FluxRail.ino.uf2`；记录 UF2 SHA256、板卡序号、Windows 版本。
2. **首次普通枚举**：核对接口/端点/VID-PID/真实 serial/报告长度；验证无六键 boot 截断；十键全按无丢失；断连重连的全释放与当前状态恢复（不得重放离线期间往返动作）。
3. **灯与并发**：逐 BT 验证红/绿/蓝与 GRB/RGB；实测键灯上限 127、GP11 上限 63 与实际 25%；捕获 GP10/11 时序、核心0 最大服务耗时、队列溢出与恢复。
4. **看门狗**：受控注入核心1 停转，确认 2 s 内重启；不得用静态分析代替。
5. **IO4 冷切换**：先 `set_startup_usb_mode=IO4` 仅 RAM 预览 → 显式 `save` → `reboot_to_saved_mode`；验证上电首帧即 IO4 枚举、无短暂普通枚举；抓 cmd1/2/3 与 cmd4 GPIO 真实字节；拒绝未知命令不得触发 UF2。
6. **掉电**：两槽分别做擦除中/写页中/commit 前掉电，确认始终能回到上一个有效槽。
7. **`FLUXRAIL_IO4_CONFIG_CDC=1`**：本轮为 `0`；带配置 CDC 的 IO4 是**独立候选**，需单独构建/枚举/游戏验收，不得套用本轮结论。

> 特别注意：`HARDWARE_CHECKLIST.md:46` 提示当前软件报告版本号为 `0.8.1-module1-software`，而 `ConfigProtocol.cpp:118` 的 `hello` 应答返回 `0.9.0-integrated-software`。以实际构建产物的 `firmware_results.json` 源哈希为准，不要把历史阶段产物当成本次固件。

---

## 八、建议的下一步

| 优先级 | 事项 |
|---|---|
| 高 | 按第七节顺序开始实板验收；此前不要宣称任何"实板通过" |
| 中 | 顺手修掉 G-1（加判空）；G-2/G-3 与 S-1 视重构计划排期 |
| 中 | 恢复可复现的固件构建链（S-3），并记录 UF2 SHA256 与源哈希的绑定 |
| 低 | 初始化版本控制、隔离 `tests/integration/build/` 产物（S-4） |
| 低 | 在 `Hub`/README 中固化"主机必须设应答超时并在超时后关闭端口"的契约（G-3） |

---

## 九、总体评价

这是一个**架构清晰、纪律性极强**的项目。所有权边界（谁写什么、谁能读什么）在头文件注释层面就被明确约束，配置变更走统一的"暂停 → 清队列 → 恢复 → ACK"事务，不存在半应用状态；可靠性关键路径（Flash、USB 故障、队列溢出、看门狗）都定义了显式的失败语义与恢复动作，而不是"忽略错误继续跑"。

代码里几乎没有遗留 `TODO`/`FIXME`；上一轮审查（2026-10-03）与本轮审查在高危区域（并发、Flash、协议、位运算）均未发现可复现缺陷，两份结论互相印证。

**当前唯一实质风险不在代码，而在验证缺口**：全部结论目前都建立在主机模拟之上，实板电气特性（灯序/GRB、霍尔实际磁场、SSI 安装方向、PUSH 机械抖动）与主机行为（Windows HID 解析、游戏识别）尚无任何数据。建议把资源集中投到第七节的实板验收上。
