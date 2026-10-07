# IO4确认弹窗BT10映射提示

日期：2026-10-08（北京时间）。用户要求仅修改截图弹窗的BT10片段，其他内容不动。

`integration.js` 仅在IO4确认正文中生成“本设备的 BT10（当前映射：N）”；读取当前页面模板的普通BT10/source9动作并复用 `actionLabel`，不读取IO4功能编号冒充键盘映射。有未应用草稿/编辑非活动模板时分别标明草稿/编辑模板映射，禁用或未读回时明确标注。恢复仍须按本设备的物理BT10。

标题、警示、note、按钮文案、弹窗渲染器、HTML、CSS与其他BT10提示未改。确认/取消、revision复核和模式草稿逻辑保持原样。修改前源码在[integration.before.js](integration.before.js)，保护基线在[unchanged-before.json](unchanged-before.json)。

验证：`npm test` 73/73通过（[日志](node-tests.txt)），`node --check src/integration.js`通过。复用现有integration-ui测试夹具的真实按钮回调，六种情形检查键盘N、鼠标右键、禁用、草稿、非活动模板、未读回；核对其余弹窗字段及取消不发命令，见[COPY_CHECK](COPY_CHECK.json)；该初次检查记录保留了“按住”后的空格，最终仅移除该空格，复核见[FINAL_COPY_CHECK](FINAL_COPY_CHECK.json)。文档检查见[报告](DOCUMENTATION_CHECK.json)。

未新增或改动原测试，未修改固件/协议，没有真实串口、设备恢复、浏览器截图/视觉复测、正式编译、ARM、最终硬件或游戏测试。
