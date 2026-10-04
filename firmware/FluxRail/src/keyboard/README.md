> 2026-10-05现行交接：[模块文档](../../../../docs/modules/01_按键与灯光.md)、[测试状态](../../../../docs/测试状态与待验收.md)。本模块已接入正式App；下文为原模块阶段技术说明，旧“未接入”与测试范围按历史日期阅读。原始字节已保存于整理前文档压缩包。

# 模块1：键盘接入说明

本目录负责K01～K05，业务接口不持有USB/Flash/CDC。模块类型已改为common唯一类型的别名，公共InputWorker/ConfigCoordinator/NormalHid与正式FluxRail.ino已接入普通NKRO原型；纯C++编译/模拟及正式链接已通过，主机NKRO/实板/游戏尚未验证。整体构建见firmware/FluxRail/README.md。

## 核心与调用顺序

核心1：一次beginKeyPinsRp2040；读取十键raw_down_mask并以KeyScanner::reset建立初始参考，之后每500us～1ms调用sample。板适配器只使用GP0..9内部上拉；不初始化GP19/20。模块3过滤PUSH后与BT一起交公共输入生产者。

EdgeSink::tryEdge参数：source_id、down、timestamp_us、revision。timestamp_us是首次观察到连续稳定候选电平的micros时间；默认1000us按下/松开消抖后才发事件。source_id=0..9对应BT1..10，10/11对应P1/P2。公共生产者给十二路流统一分配连续32位sequence，通过已有约定的可靠SPSC送核心0。模块不能用volatile或本地队列代替跨核同步。

核心0：完整校验公共配置，复制其bindings子集到KeyboardConfig，调用ActionMapper::reset；从公共队列读取边沿，交enqueue(source_id,down,sequence,timestamp_us,revision)。调用service(now_us,sink)推进。配置验证失败不会修改旧配置。只在此核心持有映射器，不需内部锁。

- Accepted：公共消费者可移除边沿。
- Backpressure：64事件本地积压满，保留该边沿并先service；重试同一sequence，不自行跳到下一事件。
- StaleRevision：统计并丢弃旧revision边沿；不能重放到新绑定。
- Invalid：未初始化或来源编号无效，公共层应诊断而非无限重试。
- NeedsResync：检测断序后停流，公共层应协调恢复。

service每次最多成功提交一份状态，不delay、不忙等、不动态分配。每次最多遍历十二绑定+比较28位图字节。内部Event保留原sequence/timestamp，方便后续公共诊断扩展；当前没有编造USB延迟实测值。

## 公共USB必须满足的ActionSink约束

tryKeyboard接收KeyboardState（modifiers字节+28字节usage位图）；位图bit n表示usage n，usage E0..E7在modifiers。这是语义数据，不是最终USB报告结构；公共NormalHid负责唯一描述符/Report ID/端点及编码。支持04..A4和E0..E7；错误usage 00..03及保留区拒绝。默认A是usage04、没有Shift。

tryMouseButtons接收1/2/4的全局三按钮状态；统一鼠标路由器同时用于绝对和相对运动。不能因静止跳过按钮发送，也不能在运动报告中清零它。D02未确认重复鼠标重触发，当前仅做来源OR持有。

两回调true必须表示已可靠接收并复制、且保证报告顺序。下一报告只能在前一份可保序发送时接收；可直接在USB ready时send，或进入可靠发送FIFO，**不可覆盖latest快照**。回调false保持相同待发状态，下次重试。实际传输故障由公共层执行释放/快照恢复，不能把“提交成功”宣称为游戏已收到。

重复同usage每次物理DOWN逐项处理；目标UP采用旧键/修饰状态仅清该目标，随后独立报告恢复目标并应用新来源组合修饰。gap默认1000us，从UP接收成功时算起，允许0..100000us；gap=0也不会在同一次service发送两份。USB公共端点1ms轮询和游戏识别仍待验，不能把这个建议数字当性能结论。某来源UP而其他来源仍持有时保持；最后一个释放结束。独立修饰usage重触发会短暂撤销该修饰，这是键本身的语义。

## 热改键与故障恢复

公共层应先提交不可变PauseApply配置，在核心1采样边界应用新revision并暂停旧流，拿到十二路真实mask/ACK之后才允许清旧队列；核心0以此reset。恢复依次：全键释放、鼠标释放、按新绑定重建键状态、重建鼠标状态。recovering为true时不能声称两核配置应用及输出恢复已完成；新revision边沿可排入，但会等恢复结束后处理。

主动Flash维护/身份重启须执行释放停机流程；意外断连时公共层请求暂停，连接恢复重新取得真实mask并全释放重建，不回放旧事件。reset会显式统计清掉的旧积压，拒绝新revision历史。正常运动来源接管不调用reset，避免释放仍持有的鼠标按钮。

KeyScanner队列提交失败只记一次overflow_fault并置needsResync，之后继续更新真实状态但停止发事件。必须由公共层安排同样的停流/队列清空/全释放/BT+PUSH快照恢复，再reset扫描器。它不会偷偷吞边沿后假装正常。

## 单位与默认起点

ScanConfig按下/松开分别0..20000us，默认1000us；时间为单调32位micros无符号差，允许计数器回绕，调用间隔应小于2^31us。默认usage按BT1..10：33/0F/0E/0D/09/07/16/04/19/11；P1/P2暂禁用。bindingFromUsAscii解释单个可打印US字符，大写及移位符号带LeftShift，非ASCII/控制字符拒绝，不处理输入法或主机布局。

扫描统计实际服务间隔/稳定延迟、有效边沿/溢出；映射统计边沿/拒绝/断序/报告/重触发/重置丢弃/最高积压。最大服务耗时、边沿到USB延迟及并发预算须集成实测。

测试及复现见[测试说明](../../../../tests/module1/README.md)，阶段记录见[模块1进度](../../../../docs/progress/模块1.md)。
