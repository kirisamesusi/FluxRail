# Luna 文档与首轮实板审核

审查模型：gpt-6-luna。审查方式：子进程只读检查文档、源码和父窗口保存的实际证据；子进程未操作开发板。以下是父窗口按收到的实际评审整理的摘要。

## 文档范围

项目入口、需求与交接、docs/spec 00–15、progress全部模块、硬件笔记、固件模块与CDC文档、tests说明/清单/评审、Web交接及评审、IO4/USB/旧项目参考索引。未重复通读vendor/generated/build/backups副本。没有发现适用AGENTS.md。历史“未接入”、0.10.0及旧测试数量按当前修复记录核对。

## 初始读回审核

initial-readback.json支持真实CDC报告0.10.1、固定识别头/板型、序号5054316560DD729C、普通身份、配置ready/revision1、维护idle、4模板、无校准及无Flash槽。Hall/SSI started属于驱动状态；Hall无校准valid=false合理。SSI全零合法CRC不能证明MT6701在线。一个快照不能证明持续1kHz；须设备adc_attempts/时间差及诊断。saved_revision=null/dirty=true不代表保存成功。游戏未验。

## PnP/HID与产物审核

windows-pnp.json的本机当前COL02为OK/ProblemCode0，其他相关节点同样OK；可以关闭这台Windows上当前设备/0.10.1实例的Code10启动问题。windows-hid-probe.json显示两个鼠标集合可查询preparsed caps，绝对集合6字节输入、两个轴value cap，X/Y本地zero报告解析SUCCESS。zero报告没有送到设备，未读取实际运动报告，不能当真实XY/按钮/Y保持或游戏验收。

software-artifact-check.json确认84个固件源摘要无漂移，341504字节UF2摘要与历史修复报告一致；本轮没有重新编译或模拟。补充证据边界：设备只读回版本/身份，没有读出整片Flash，所以不能证明实板镜像逐字节等于本地UF2。

## 后续重点

Hall率按同revision/epoch两份新鲜设备快照计算，区分维护停顿和正常/压力时段，不以均值遮盖max_interval或skipped_periods。物理按键/按钮、Hall校准/全轨、旋钮/PUSH、Flash/掉电、IO4恢复/冷切换、Web Serial/UI、游戏分别待验；无外设时只关闭实际可证的驱动与通信层。
