# 0.10.1 裸板实测 Luna 审核

2026-10-04，北京时间。模型gpt-6-luna，子进程`/root/luna_review`。Luna通读项目自有需求/交接/规格/进度、固件模块与CDC、测试与硬件清单、Web评审及来源索引，重复快照和第三方生成文档不重复计。Luna只读审核文档、代码与父窗口保存的实际证据，没有操作开发板；本文由父窗口按实际收到的审核记录整理。

结论：**分层交付审核通过，有明确未通过/未执行项；不代表整机或游戏验收通过。** 总报告为[validation_bare_board_0_10_1_2026-10-04.json](validation_bare_board_0_10_1_2026-10-04.json)，逐阶段原始评价在[regression](hardware_0_10_1_2026-10-04/regression/)的1～6.review.json及web-client.review.json。

| 阶段 | Luna核对的实证与结论 |
| --- | --- |
| 身份/备份 | 0.10.1、COM29、同序号5054316560DD729C，4模板/schema2 1387字节/CRC1053904252/revision1；不是Flash镜像比较 |
| Windows | 本机COL02 OK/ProblemCode0，两Mouse caps及本地zero X/Y解析成功；关闭本次实例Code10启动失败，不代表真实运动/按钮 |
| RAM | 30组参数、20次模板切换、IO4绑定读回/恢复；原final仅revision1→54，未写Flash |
| 协议 | 12类错误按预期拒绝，损坏CRC不提交，3字节分片及连续三请求成功；完整快照不变 |
| 会话 | 20重开+20DTR含残留半行，握手/配置恢复；不是物理USB拔插或冷枚举 |
| 采样/通信 | idle10058/10.057957s=1000.004Hz；paced300033/300.032571s=1000.001Hz；stress60063/60.063307s=999.995Hz；三个窗口新增skipped/ADC错误/队满/遥测丢弃0，历史最大间隔1822us未增长。3000轮/300.007s平均9.99975Hz，60.074s/6263状态压力；1个重复快照，不承诺每轮新鲜/最坏时序 |
| 恢复/关闭 | 四模板及meta核对恢复，1387字节仅revision1→56、最终CRC3256476313；主进程exit0/port_close_verified=true |
| 网页协议 | 53请求/53响应含50排队只读、预热hello及INVALID_PROFILE/之后恢复hello；7字节接收分片，faults空；生产客户端实板适配通过，浏览器Web Serial/UI未测 |
| 额外HID读取 | 三集合HidD_GetInputReport FALSE/Win32错误1，未通过。失败hex是预置缓冲，不能当设备数据；根因未区分主机句柄/API/驱动或设备控制路径 |
| 失败后健康 | post-hid-cdc四读请求ok/revision/applied56、同序号0.10.1、ready/idle/meta不变，finally关闭；只证明CDC及配置继续正常 |

工具首次预审发现主机轮询门槛和审核/版本默认可绕过问题，父窗口在只读阶段停止，20条全只读、原final二进制相同；修正后使用独立regression目录重跑。新版工具强制版本/目录/review、审核前pending、拒绝旧report、idle超时、目标HID序号/存在/解析检查。Luna增量复核后放行。工具session_id不是OS PID，端口关闭以实际进程退出及标志确认。

最终总审独立重算7个工具摘要、36个证据摘要均匹配。主机间隙统计限定paced完整起止motion精确匹配的CDC行437～6498及其主机时间；3002响应，gap min16/p50 95/p95 122/max128ms，明确不是传感器或USB输入延迟。HID失败标签在实际读后改为hex=null/prepared_buffer_hex并标无效，没有重复读取，tool_hash_scope已注明；原失败证据保留。

本轮没有重新编译/ARM模拟或刷写：固件源/本地UF2只关联已通过的历史修复证据。未验证Flash镜像字节、真实霍尔定位/MT6701/PUSH、Y保持/按钮、实际灯、浏览器UI、Flash保存/掉电、IO4与游戏。下轮从HID控制读取失败定位及相应硬件清单继续，不以通过的裸板项目替代这些验收。
