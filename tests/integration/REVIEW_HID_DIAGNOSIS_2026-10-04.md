# 独立审核：HID 当前状态读取诊断（2026-10-04）

## 审核结论

冻结报告与现有原始证据相符，可以交付为一次**根因尚未定位的分层诊断**。两种 Windows 主机控制查询均失败，但目前没有 USB 总线捕获，无法判断 GET_REPORT 是否到达设备或由哪一层拒绝。此结果不证明 HID 中断输入、真实键鼠动作或游戏功能损坏，也不足以支持修改固件。

报告和阶段记录将实板 CDC 读取、Windows PnP、主机 HID 当前报告查询、固件编译、ARM 模拟与游戏测试分层，未把脚本 exit 0 当作查询通过；也正确记录了本轮没有新固件编译、模拟、刷写、Flash 写入或游戏测试。

## 独立复核

- 冻结 JSON 中 42 项原始证据 SHA-256 与 7 项工具 SHA-256 均逐项匹配；六个 CDC 时点各有 15 条请求，允许操作全部为只读，响应成功且 revision 为 1。六份导出均为 1387 字节、CRC32 `1053904252`；schema2 二进制与四模板 profiles 的 SHA-256 在六个目录中完全一致。
- 设备身份均为序号 `5054316560DD729C`、固件版本 `0.10.1`、模式 `NORMAL_NKRO`。健康快照为 ready、idle、未 apply、无 apply 失败、valid slots 为 0。历史 regression 的 revision 56 与本轮起始 revision 1 之间的重置原因和时间未知，报告未推断原因。
- 当前诊断范围内真实发送的控制查询只有两次：一次 access=0 的 COL02 `NtDeviceIoControlFile`，NTSTATUS `0xC0000010`（映射 Win32 error 1）；一次独立 C# `HidD_GetInputReport`，返回 false/error 1。两次均为长度 6、Report ID 2，均没有有效报告字节，相关句柄已关闭。Native IOSB 的零值明确标为非完成证据。C# 的三个前置中止/打开尝试没有发送控制查询；metadata-only OpenOnly 成功也未发查询。C# 工具修改为组合变化，唯一导致此前 CreateFile 失败的因子未隔离。
- PnP 证据中的七个节点均为 `OK` 且 ProblemCode 0。这关闭的是该次本机枚举状态检查；节点 Upper/LowerFilters 属性为空不能证明整个 Windows 类栈无过滤器。
- ETW 之前因非管理员权限被拒，确实没有 `usb.etl`。后续脚本只做过静态审核，冻结报告没有把它写成已运行或已捕获。继续追因需要管理员 ETW 或其他被动总线证据；确认 trace stop、目标设备身份、正向 CDC 对照和事件丢失情况后，才分析目标 interface 3 的 GET_REPORT 与完成状态。

## 交付与后续入口

建议保留 `validation_hid_diagnosis_2026-10-04.json`、`hid_diagnosis_2026-10-04/` 中的原始文件与逐阶段审查，以及 `HID_DIAGNOSIS_2026-10-04.md`。冻结哈希验证通过；本审核放在 `tests/integration/`，没有改动冻结 JSON 或证据目录。

下一步可由有管理员权限的环境按 `HID_DIAGNOSIS_2026-10-04.md` 中的命令使用全新输出目录运行 `capture_hid_etw.ps1`。该脚本尚未实测，运行失败时应保留原 ETL 和命令日志，不把空日志或 XML 转换成功解释成没有 USB 请求。若系统提供程序无法给出有效控制传输证据，应转向被动 USB 分析器或独立主机。继续取得决定性证据前，保持固件不变。
