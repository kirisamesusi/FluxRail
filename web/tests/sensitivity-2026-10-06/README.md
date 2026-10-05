# 2026-10-06 · 设备状态图标与灵敏度检查

分支test；保留此前未提交修改，按10/05固定设计参考沿生产代码修改。固定模板未变。用户要求同风格设备状态icon，滑轨/双旋钮统一1～10、5等效旧1、数值单调提升响应且一位小数，相关文档和GPT-6-Luna评审。

## 实现及兼容

- `art-device-status` 新SVG复用水蓝/浅粉渐变及圆润线条，控制器加状态勾。导航原引用缺失的符号现已定义，页标题共用。
- `sensitivity.js` 统一产品刻度1.0～10.0/步进0.1，实际gain=刻度/5；按整数十分位除以50避免冗长浮点。1/4.9/5/5.1/10对应实际0.2/0.98/1/1.02/2。
- 明确编辑才换算；CDC/schema2、完整草稿和快照继续存实际gain，固件算法及默认gain1未改。不需要刷机以支持新标尺。新刻度10传实际2，落在旧Hall/旋钮校验范围中。
- 新输入完成编辑后收敛到一位小数，例如5.14→5.1；先拒绝原始越界输入。只显示产品标尺一位，不截断实际倍率/累计圈数。Hall按f32实际表示判断；Knob按f64机器精度判断，避免把旧1.0000005显示成5.0。
- 旧值不能准确用标尺表示时明确显示原倍率，保留原值、控件留空、尺拇指隐藏。无关修改保存不改变旧倍率，明确新选择才替换。物理活动、原始圈数、全局校准gain、运动轴/单位不变。每圈count的基础值标为“灵敏度5.0每圈 HID count”。

## 实际执行

| 检查 | 结果及证据 |
| --- | --- |
| 完整Web软件 | `npm test --prefix web`，九套72项通过：[最终日志](node-final-tests.txt)。新增91点单调/校验、本地投影、旧精度保留、非法/空值、实际Device应用/保存/快照/读回只换算一次及所有SVG引用完整。 |
| Hall ARM | `python tests/module2/run_tests.py --build-dir tests/integration/build/sensitivity-2026-10-06/hall`，6单元严格编译，13组7716检查通过；新增91刻度/正反向/五位置。见[日志](hall-tests.txt)、[冻结报告](hall-arm.json)。 |
| 旋钮 ARM | 对应module3独立build-dir，6单元严格编译，15组6701检查通过；新增91刻度/双ID/正反向及raw/scaled/count/activity/axis。见[日志](knob-tests.txt)、[冻结报告](knob-arm.json)。 |
| 浏览器 | Headless Edge151.0.4129.107，1274/390/320px，八模块×未连接/原模拟器/隔离CDC夹具=72视图/10检查组；无横向溢出、无page error。见[最终报告](final-delivery/browser-check.json)、[执行日志](final-browser-output.txt)及本目录check.mjs。 |
| 人工视觉 | 设备状态桌面/手机、新图标、默认5.0的滑轨/双旋钮和旧值保留界面截图检查；本机内嵌浏览器也已刷新查看新图标。 |
| 文档/源码 | 本目录docs-final-check.json记录91文档/736本地链接通过；syntax-check.json记录7个JS语法及统一入口九套测试与package一致；protection-check.json记录87备份文件、93固件文件和22旧冻结证据SHA通过；manifest.json记录最终SHA。 |
| Luna | 只读GPT-6-Luna评审见[LUNA_REVIEW.md](LUNA_REVIEW.md)。 |

浏览器夹具只将测试端口绑定已有DemoHost，序号TEST-FIXTURE-ONLY，无真实串口。包括新值1/4.9/5/5.1/10的实际CDC payload/保存读回、额外小数、箭头0.1、整数拇指落点、旧倍率/精度无关保存、原确认取消/Escape、草稿/维护/校准/revision锁、应用失败停止保存、保存未确认锁写、旧runtime及本地UF2识别。生成的UF2只是测试解析器夹具，不可刷机。

早期浏览器尝试目录browser/、browser-final/保留诊断截图：测试最初以图标原坐标宽度40判断缩放后的图标而误报，随后对旧值提示收起导致的布局变化使用了旧滑块坐标而误报；均修正测试定位，产品图标/交互未因此反复改动。browser-final-2/为精度复核前的成功报告，最终交付以final-delivery/为准。Node首次严格比较4.9/5的浮点0.9800000000000001与0.98失败，换算实现改按十分位/50，最终72项通过。Luna提出f64旧精度不应共用宽容差，已按实际存储表示收窄并补查。

## 分层边界及追溯

正式固件源码93文件与修改前SHA一致，版本仍0.10.2；没有正式Arduino整机编译或新UF2交付，没有USB/Flash/ROM实测、刷机、最终硬件/游戏验收，没有自动提交/合并/发布。ARM测试ELF是模拟可执行文件，不是整机固件。

修改前[局部源码快照](../../../backups/web-before-sensitivity-2026-10-06/BACKUP-README.md)已验证，旧冻结证据/SHA未覆盖。新规则见[公共契约](../../../docs/公共架构与配置契约.md)、[Hall模块](../../../docs/modules/03_三霍尔滑轨.md)、[旋钮模块](../../../docs/modules/04_旋钮与PUSH.md)、[Web模块](../../../docs/modules/05_Web配置工具.md)、[web修改.md](../../../web修改.md)。统一run_all补齐workflows/sensitivity两套Web测试；本轮选择相关测试单独执行，没有声称全套run_all或正式构建通过。
