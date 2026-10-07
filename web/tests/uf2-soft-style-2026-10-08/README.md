# UF2提示柔和样式与文案

日期：2026-10-08。用户反馈整段粗体与整体风格不搭，并要求优化“固件版本不对”提示。

动态结果及顶部反馈改为13px、400字重、1.85行高；UF2结果继承页面正文字体。提示改为“固件文件检查未通过，请选择适用于本设备的 UF2 文件。”，适用于拒绝文件/设备身份不匹配。保留固定说明、顶部反馈更新、型号及设备说明；底层校验不变。

- Node73/73通过：[日志](node-tests.txt)。保留两项控制器测试及简短错误断言。
- Edge151.0.4129.107在1274/390/320px验证错误→正确→清空、固定说明、提示复位、13px/400及与页面正文一致的字体、下载说明、无页面错误/溢出：[脚本](check.mjs)、[报告](browser-check.json)。
- 已人工查看[手机错误](invalid-390.png)与[桌面有效文件](valid-1274.png)，其余截图一并保留。
- [文档校验](DOCUMENTATION_CHECK.json)。
- 新[r6包](../../../outputs/FluxRail-Ubuntu-Setup-2026-10-08-r6.tar.gz)、[SHA](../../../outputs/FluxRail-Ubuntu-Setup-2026-10-08-r6.tar.gz.sha256)、[清单](../../../outputs/FluxRail-Ubuntu-Setup-2026-10-08-r6.tar.gz.manifest.json)、[包校验](PACKAGE_CHECK.json)，旧包/证据保留。

内存UF2夹具与下载确认展示后取消；没有真实串口/设备指令、固件编译、ARM、刷机、硬件或游戏验收。Git推送与公网版本另行核对。
