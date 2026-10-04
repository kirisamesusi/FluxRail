# HID ETW 2026-10-04 总审（Luna）

**判定：可交付，结论范围和遗留项记录完整。** 我独立复算 `validation_hid_etw_2026-10-04.json` 中全部 80 项 SHA-256，80/80 匹配且无缺文件；再验证旧 `validation_hid_diagnosis_2026-10-04.json` 的42项证据和7项工具哈希，49/49 匹配。原retry1预检内容已丢失，但明确的缺失标记存在，误写内容另存，正确的中性预检在新launch目录；不应把该新预检当作旧审查恢复。

设备操作计数与状态相符：新ETW轮次包括75条baseline只读CDC请求（中止轮15、retry1前后各15、中性轮前后各15），中性动作记录14条CDC请求，其中12条只读、2条`set_active_profile` RAM写入；合计89条请求。另有1次本轮C# `HidD_GetInputReport` 查询，返回失败。只读日志与动作日志没有 `save`/Flash或重启请求。

中性轮开始于版本0.10.1、序号5054316560DD729C、revision 1、active profile 4、`physical_mask=0`。唯一两次写请求按预期切换到profile 1再回profile 4，最终revision/applied revision为3，健康状态通过，串口已关闭。Hall配置为profile1/2/3/4分别false/true/false/false，实际切换的1和4均禁用Hall。前后schema2导出均为1387字节，实际差异只有offset 4的revision值；profile JSON逐字节相同。对导出重新计算的CRC32分别为revision1时1053904252、revision3时3601827843，与报告一致。没有声称全模板Hall禁用。

两次管理员ETW捕获均有成功停止证据：retry1与中性轮的stop命令exit 0；解码分别为5006和5183事件，报告的event/buffer loss均为0。retry1控制窗口中未看到本次C#查询对应的目标GET_REPORT请求；可交付的严格结论是：这次HidD调用失败，且没有对应GET_REPORT进入所观察的Windows UCX/xHCI软件栈。ETW不是物理总线分析仪，不能据此指出具体Windows拒绝组件或验证固件GET_REPORT回调。

中性轮的目标HID中断IN endpoint 0x83有4个成功、29字节完成，endpoint 0x84有4个成功、6字节完成。因此可称为**传输层通过**。离线字段库存没有捕获报告payload或Report ID，故**报告内容未验证**；不能宣称全零内容已读回。这一轮也没有验证真实按键、鼠标运动、Hall/旋钮、绝对XY/Y保持或游戏表现。

本轮无新增固件编译、ARM模拟或刷写；此处提到的工具编译/运行仅属于主机诊断。物理输入及游戏仍需独立后续验收。汇总JSON、进度文档和阶段review之间的结论一致；没有发现会阻断交付的证据矛盾。
