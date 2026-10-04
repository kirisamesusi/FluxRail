# 两项实测问题修复审查

2026-10-04。按用户要求由子代理 **gpt-6-luna** 独立只读审查，比较 `backups/firmware-before-bugfix-2026-10-04/` 与当前代码；未刷写、未连接串口、未执行游戏。审查范围与父窗口实际测试结果分开，最终测试入口见 `validation_bugfix_2026-10-04.json` 和 `docs/progress/修复.md`。

## 审查结论

本次Hall/核心1调度改动未发现需要立即修改的可复现代码缺陷。固定相位推进、跳过过期周期且仅采当前一次、uint32时间回绕、独立双SSI服务、维护恢复保留序号/统计，以及诊断复制/CDC有界传输符合既有契约。回归覆盖Hall迟到/停顿/回绕、Knob独立相位、App高频进入与有耗时ADC/SSI替身、维护/校准/IO4。

**发布阻断项仍为Windows绝对X Code10未解决。** 当前ID2保留X-only Mouse TLC，版本仅为0.10.1-hall候选，不能作为“两项均已修复”的正式发布。纯固件真正绝对X、独立系统Y与Windows普通鼠标兼容同时成立，审查员未找到官方支持证据；补固定Y、空Y或混合轴不能作为未经验证的结论。

官方依据：[MOUSE_INPUT_DATA](https://learn.microsoft.com/en-us/windows/win32/api/ntddmou/ns-ntddmou-mouse_input_data)使用同一个绝对/相对标志解释LastX和LastY；[HID鼠标驱动](https://learn.microsoft.com/en-us/windows-hardware/drivers/hid/keyboard-and-mouse-hid-client-drivers)将HID usages映射到系统鼠标。资料不能证明X-only描述符可启动，也不能证明补Y就保留Y。

## 最后增量复核

Hall到期判断在control之后取adc.nowUs；快照在采样/SSI完成后取snapshot_now并用于timestamp和50ms发布节拍。Luna复核认为消除了入口时刻过期问题、常规回绕行为正确，未发现新增阻断问题；Acquisition.h已明确时钟域契约。耗时场景拆为97us/1秒与37us间隔/200ms两组后保留验证目的，避免单场景耗尽模拟预算。

## 验证边界

审查是源码审阅；模拟外设耗时是设定值，不能证明实际ADC/SSI/USB/灯效并发的最坏负载。正式全量测试与UF2身份验证由父窗口实际运行并另存结果；本次审查没有新增实板或游戏证据。新版Hall达到真实1kHz、WindowsCode10消除、独立Y及游戏验收仍待后续。
