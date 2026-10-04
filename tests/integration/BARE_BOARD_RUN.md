# 只有开发板的分阶段实测

2026-10-04。当前实测目标为0.10.1。真实证据入口：`hardware_0_10_1_2026-10-04/`；首次只读运行因Luna发现工具门槛缺口而安全停止，正式RAM/稳定性回归使用其`regression/`子目录。保留`hardware_2026-10-04/`中的旧0.10.0结果。

只连接开发板时可以检查CDC身份/状态、Windows PnP/HID集合、RAM应用/读回/恢复、错误协议、CDC会话恢复、ADC驱动实际计数和软件负载、生产网页协议客户端。无外设时不能证明真实键、灯、磁轨、旋钮/PUSH、Y保持或按钮交互；主机适配器不是浏览器Web Serial/UI。Flash物理容量及恢复入口未核实前，主流程不保存Flash、不重启、不切IO4、不进ROM。

## 新会话准备

先确认当前端口/序号/版本，在新目录建立只读备份；已有非空目录会拒绝。例：

```powershell
cd C:/Users/admin/Documents/FluxRail
& tests/integration/prepare_board_baseline.ps1 -Port COM29 -Serial 5054316560DD729C -Version 0.10.1 -OutputDirectory tests/integration/hardware_new_run
```

确认baseline身份和配置、原始二进制CRC后再运行。版本、输出目录及审核门槛都是必填；已存在report.json时拒绝重用该次运行：

```powershell
$env:FLUXRAIL_TEST_OUT='tests/integration/hardware_new_run'
$env:FLUXRAIL_TEST_VERSION='0.10.1'
$env:FLUXRAIL_REVIEW_GATE='1'
node tests/integration/bare_board_test.mjs COM29
```

每阶段保存原始CDC日志及report.json，再暂停等待Luna子进程读取证据、提出评价。父进程收到真实评价后保存`N.review.json`，字段为`stage`、`reviewer="gpt-6-luna"`、`run_started`（必须等于本次report.started）、`stage_name`（等于对应阶段名）、`continue`及`evaluation`。不要提前生成审核文件。`continue:false`会中止主流程并核对恢复，最终收尾阶段仍须审核；串口实际关闭以进程退出和report.port_close_verified为准，工具session_id不是OS PID。

流程：基线复核→30组参数/20次模板切换/IO4映射RAM读回并恢复→12类拒绝/CRC损坏导入/分片与多行→20次端口重开/20次DTR含半行→10秒空闲计数、Hall+双SSI驱动启用/动态灯25%下300秒目标10Hz及60秒无间隔CDC压力→原始模板/meta/二进制恢复。所有阶段最终评价之前标pending。

轮询检查实际9.5～10.5Hz是该工具10Hz目标的±5%测试容差，不是产品采样指标。Hall率由同revision/epoch的adc_attempts和设备timestamp无符号增量计算，原始起止快照与skipped_periods等见hall-rate-report.json；通信通过不自动判定Hall1kHz通过。最大诊断是累计峰值，配置维护停顿和正常时段须分别解释。SSI全零帧合法CRC不证明传感器在线；只开启驱动也不代表有真实运动输出或USB运动带宽压力。

## 补充检查

```powershell
python tests/integration/windows_hid_probe.py --serial 5054316560DD729C --output tests/integration/hardware_new_run/windows-hid-probe.json
python tests/integration/windows_hid_probe.py --serial 5054316560DD729C --get-input-report --output tests/integration/hardware_new_run/windows-hid-current-report.json
node tests/integration/web_client_board_test.mjs COM29
```

HID探针默认只查询当前接口及本地解析构造的zero报告，要求目标序号的键盘及两个鼠标集合存在且鼠标X/Y解析成功，未向板发送输入报告。网页测试沿用上述环境变量，必须等主测试进程关闭COM端口后执行；生产SerialClient使用真实CDC桥接及7字节接收分片、50个排队请求和预期错误后的恢复。每轮补充测试也交Luna审核，不能将成功解释成浏览器UI/物理USB拔插或游戏验收。

可选`--get-input-report`使用[Microsoft HidD_GetInputReport](https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/hidsdi/nf-hidsdi-hidd_getinputreport)逐集合读取一次当前设备状态，记录原始字节和主机解析；长度取实际HID caps并使用相应Report ID。它读取控制路径状态，未捕获interrupt输入流，不证明真实运动/Y保持/按钮边沿或HID实时发送节奏。运行结果必须单独审核；读取失败也保留证据。
