# UF2固定说明与动态结果实际修复

日期：2026-10-08（北京时间）。用户再次发现错误文件时上方说明与下方结果同步变化。前序仅调整测试没有修运行代码；本轮落实上方说明固定、下方动态结果，并同步控制器测试和现行文档。

`update/controller.js` 的show始终固定version文字并清错误样式，所有动态文字/错误样式只写入result。fileSummary保存当前有效文件的文件名、版本、板型和块数/字节数，render按最新连接状态重建详情与比较消息，反复重连不重复追加。pendingText分别表示读取中/初始状态，选新文件及取消清除候选、摘要、错误和预期序号；非法文件仅显示一次原因。

两项控制器测试保留原功能检查，并检查每个状态上方固定、下方详情/模拟/比较/错误正确、读取中固定说明、错误只显示一次和反复render不重复详情。六项底层UF2/版本算法测试未改。

| 验证 | 结果 / 证据 |
| --- | --- |
| 完整 `npm --prefix web test` | 73/73通过，含8项UF2，[日志](node-tests.txt) |
| Edge浏览器检查 | 151.0.4129.107，1274/390/320px初始、非法保留区文件、有效文件与清空均通过，上方文字/非错误样式不变，错误仅在结果框一次，无页面错误/横向溢出；[脚本](check.mjs)、[报告](browser-check.json)、[窄屏错误截图](invalid-390.png) |
| 语法 / diff | controller与uf2测试语法检查，以及本轮文本diff检查 |
| 文档 / 归档 | [独立报告](DOCUMENTATION_CHECK.json) |
| r4部署包 | [新包](../../../outputs/FluxRail-Ubuntu-Setup-2026-10-08-r4.tar.gz)、[SHA](../../../outputs/FluxRail-Ubuntu-Setup-2026-10-08-r4.tar.gz.sha256)、[输入清单](../../../outputs/FluxRail-Ubuntu-Setup-2026-10-08-r4.tar.gz.manifest.json)，不覆盖r3；[包校验](PACKAGE_CHECK.json) |

修改前的controller与测试副本留在本目录；[前序测试适配记录](../uf2-current-web-2026-10-08/README.md)及旧失败/冻结SHA保持原样。浏览器使用内存UF2解析夹具，不是整机交付固件，未读写串口/设备。固件、协议、schema、IO4弹窗和BT10文案未改。本轮不涉及ARM、编译、刷机、最终硬件或游戏验收；Git推送及公网部署状态分别确认。
