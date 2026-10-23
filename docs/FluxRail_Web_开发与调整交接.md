# 兼容入口：FluxRail_Web_开发与调整交接

2026-10-05已整理归档。请阅读[现行交接文档](modules/05_Web配置工具.md)。

[完整原稿（历史状态）](archive/2026-10-05/handoffs/FluxRail_Web_开发与调整交接.md)保留日期、阶段和测试边界；不作为当前完成状态。原始字节已保存在整理前文档压缩包。

## 2026-10-11 Web v2 新设计
完整说明见[Web v2 设计实现与后端对接指南](Web_v2_设计实现与后端对接指南.md)与[05 Web配置工具](modules/05_Web配置工具.md)，下面只是摘要。
按[设计包](../design/web-v2-2026-10-11/HANDOFF.md)改为三个页签（设备状态 / 固件 / 实验性功能）：首页含模板列表、设备面板（10键、两旋钮、滑轨，按键按下沿用 `get_status.physical_mask` 轮询）、右侧检查器、气泡与历史操作。要点：
- **仍是“先改草稿、再应用并保存”**。“改动即时生效”已核实不可行：设备没有“回退到 Flash”命令，且有未保存修订时 `reboot_to_saved_mode` 被拒（MODE_NOT_SAVED），即时写 RAM 会让用户无法放弃。
- 灯效“自定义”＝`per_key`，与动画互斥；没有逐键亮度字段。色球调色板存浏览器 localStorage，不进设备。
- 旋钮/滑轨指示：原生值=`get_motion` 的 `raw_turns`/`hall.p`，灵敏度后=`scaled_turns`/`hall.output`；仅模拟演示可手动测试。
- 看板娘台词：直接编辑 `web/assets/mascot-lines.txt`（一行一句，`#` 注释），刷新生效，缺失时用内置台词。
- 模板名可用中文，依赖固件 UTF-8 名称（见[证据](../tests/integration/utf8_name_2026-10-11/README.md)）；未刷新固件时用 ASCII 名。
- IO4 映射只显示 BT1～BT10，P1/P2 的选择器保留在DOM外并随保存原值带回；PUSH 防误触不再显示，底层数据原样保留。
- 验证：Web Node 80/80；浏览器截图为模拟演示，不是实板。
