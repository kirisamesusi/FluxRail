# 恢复 IO4 选择与重启提示原文

日期：2026-10-06（北京时间）。分支：test。

按用户最终要求，IO4 选择确认、重启确认及重启请求成功提示恢复本轮修改前原文。integration.js 和 workflows.js 与当前 HEAD 一致；没有改动原有 HTML、CSS 或确认组件。滑轨/两个旋钮移除常驻灵敏度说明及 IO4 映射帮助的明确 USB 拔插步骤保留。

验证：npm test 73 项通过；git diff --check 通过；上述两个 JS 文件及 HTML、CSS、确认组件相对 HEAD 的 git diff 为空。

前两轮记录与 Luna 对中间版本的审核保持原样，不代表当前恢复文案再次经 Luna 审核。本次未执行浏览器视觉、真实设备、ARM 模拟或固件编译。文档链接检查见 documentation-check.json。
