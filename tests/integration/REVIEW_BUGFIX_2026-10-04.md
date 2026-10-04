# 两项实测问题修复审查

2026-10-04。按用户要求由 **gpt-6-luna** 子代理独立只读评审。Hall改动对比首次修复前快照；绝对XY/队列与恢复改动对比 `backups/firmware-before-absolute-xy-2026-10-04/`。子代理未编辑、未运行测试、未执行串口/刷写/游戏操作；父窗口实际执行结果见 `validation_bugfix_2026-10-04.json` 与 `docs/progress/修复.md`。

## 当前绝对XY与恢复审查结论

用户选择无主机助手、Y启动中点后保留本设备Y旋钮位置，覆盖先前未定路线。ID2改为完整Mouse/Pointer绝对XY及恒释放兼容按钮；真实按钮由ID1持有。Luna审阅按钮所有权、固定范围、坐标提交、USB mutex、运行时Hall模式、有限队列绕回/容量、同向合并、反向保序及联合报告，未发现新增缺陷。Windows驱动启动和实际按钮交互须实板验证，不能从字段检查推出已消除Code10。

初次复审发现 **P2：USB提交成功后异步传输失败，没有新Hall/Y运动时，主机可能保留旧坐标**。NormalHid原先只置故障标志，MotionRouter已消费提交量；父窗口据此增加 `absolute_submitted_` 与原子恢复标志，异步失败/断连请求幂等重发缓存XY。重发在已完成配置恢复、流可用、绝对模式、mounted/未suspend、端点ready/无fault且已有绝对提交时进行；维护中不发送，同步拒绝保留标志，新正常绝对提交可替代恢复包。缓存是设备逻辑状态，恢复不再次积分Y。

Luna对该修正与新回归进行了第二次只读复核，结论：**复核通过，未发现新增缺陷**。标志原子发布、坐标/模式受USB guard保护，回调与提交不发生坐标撕裂或重复积分；成功提交才清标志。transport组覆盖异步回调、无新运动、同步拒绝重试、维护抑制、mutex与幂等重发；真实App组覆盖无有效Hall、Y停止后的故障/断连重连恢复。Luna未执行这些测试，执行证据由父窗口保存。

USB身份切换范围已明确：运行时Hall开关/模板、保存和重连保留易失坐标；NORMAL/IO4身份冷切换需要重启，按新启动重新初始化Y，不承诺跨重启持久化。其他鼠标位置不回传，绝对报告恢复本设备拥有的Y。

## 当前验证与验收边界

正式源码版本0.10.1，两个问题的软件改动已实现。Hall固定相位及绝对XY软件回归通过不等于实板1kHz或Windows驱动启动通过；真实按钮跨两个Mouse集合的交互、物理Hall/旋钮/PUSH、IO4和游戏仍需对应环境。当前未发现待修的软件评审项；新版硬件验收尚未执行。最终全量结果、资源、UF2摘要以固定报告为准，不引用旧候选产物证明新代码。

## 以下为Hall阶段历史评审（USB路线选择前）

2026-10-04。按用户要求由子代理 **gpt-6-luna** 独立只读审查，比较 `backups/firmware-before-bugfix-2026-10-04/` 与当前代码；未刷写、未连接串口、未执行游戏。审查范围与父窗口实际测试结果分开，最终测试入口见 `validation_bugfix_2026-10-04.json` 和 `docs/progress/修复.md`。

## 审查结论

本次Hall/核心1调度改动未发现需要立即修改的可复现代码缺陷。固定相位推进、跳过过期周期且仅采当前一次、uint32时间回绕、独立双SSI服务、维护恢复保留序号/统计，以及诊断复制/CDC有界传输符合既有契约。回归覆盖Hall迟到/停顿/回绕、Knob独立相位、App高频进入与有耗时ADC/SSI替身、维护/校准/IO4。

**发布阻断项仍为Windows绝对X Code10未解决。** 当前ID2保留X-only Mouse TLC，版本仅为0.10.1-hall候选，不能作为“两项均已修复”的正式发布。纯固件真正绝对X、独立系统Y与Windows普通鼠标兼容同时成立，审查员未找到官方支持证据；补固定Y、空Y或混合轴不能作为未经验证的结论。

官方依据：[MOUSE_INPUT_DATA](https://learn.microsoft.com/en-us/windows/win32/api/ntddmou/ns-ntddmou-mouse_input_data)使用同一个绝对/相对标志解释LastX和LastY；[HID鼠标驱动](https://learn.microsoft.com/en-us/windows-hardware/drivers/hid/keyboard-and-mouse-hid-client-drivers)将HID usages映射到系统鼠标。资料不能证明X-only描述符可启动，也不能证明补Y就保留Y。

## 最后增量复核

Hall到期判断在control之后取adc.nowUs；快照在采样/SSI完成后取snapshot_now并用于timestamp和50ms发布节拍。Luna复核认为消除了入口时刻过期问题、常规回绕行为正确，未发现新增阻断问题；Acquisition.h已明确时钟域契约。耗时场景拆为97us/1秒与37us间隔/200ms两组后保留验证目的，避免单场景耗尽模拟预算。

## 验证边界

审查是源码审阅；模拟外设耗时是设定值，不能证明实际ADC/SSI/USB/灯效并发的最坏负载。正式全量测试与UF2身份验证由父窗口实际运行并另存结果；本次审查没有新增实板或游戏证据。新版Hall达到真实1kHz、WindowsCode10消除、独立Y及游戏验收仍待后续。
