# Windows HID主动状态读取：2026-10-04接续

本轮确认主动查询被拒绝，尚未唯一定位主机/设备拒绝层。不要据此把键盘或鼠标中断输入判为损坏，也不要把主机工具编译视为固件编译。

- 同一0.10.1开发板，序号5054316560DD729C、NORMAL_NKRO；当前Windows七个PnP节点OK/ProblemCode0。
- 键盘、相对/绝对鼠标三个集合和keyboard去kbd辅助路径：access0可读身份/caps；GENERIC_READ四次均拒绝访问5。系统键鼠集合exclusive是候选背景，不是原错误1的定因。
- 仅对observed COL02发两次主机控制查询：native IOCTL返回0xC0000010/Win32映射1；独立C# HidD_GetInputReport也FALSE/error1。长度6、ReportID2，失败hex为空，没有真实报告可验。
- Native IOSB仍初始化0不能作成功完成证据；调用耗时也不能证明USB请求没发出。
- 六份只读CDC备份每份15请求，revision/applied1、四模板和1387字节schema2完全相同，CRC1053904252。之前历史收尾revision56到本轮1的原因/时间未知。
- C#工具三次中止（JSON编码一次、打开两次）均发生HidD查询前；中止证据保留。组合修正后绑定可以打开并真正查询，未隔离此前打开失败的唯一因素。
- 没有改固件/驱动或写Flash，没有新增固件编译、ARM模拟、游戏或物理输入验证。主机Python/PS语法及C#Add-Type执行属于工具检查。
- 本进程没有管理员权限，USB ETW启动被拒绝；没有总线证据能区分请求未下USB与板端响应失败。

原始证据与逐轮Luna评价：`hid_diagnosis_2026-10-04/`；冻结结果：`validation_hid_diagnosis_2026-10-04.json`。旧裸板回归冻结文件未覆盖。

Microsoft参考：[GET_INPUT_REPORT](https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/hidclass/ni-hidclass-ioctl_hid_get_input_report)、[HidD_GetInputReport](https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/hidsdi/nf-hidsdi-hidd_getinputreport)、[系统exclusive集合](https://learn.microsoft.com/en-us/windows-hardware/drivers/hid/top-level-collections-opened-by-windows-for-system-use)、[NTSTATUS定义](https://learn.microsoft.com/en-us/openspecs/windows_protocols/ms-erref/596a1078-e883-4972-9bbc-49e60bebca55)、[USB ETW](https://learn.microsoft.com/en-us/windows-hardware/drivers/usbcon/how-to-capture-a-usb-event-trace)。源码检查见NormalHid::getMouse/getKeyboard、Adafruit回调及TinyUSB HID_REQ_CONTROL_GET_REPORT；源码存在处理逻辑不能替代实板控制捕获。

## 管理员环境继续入口（仅准备，未实测捕获）

`capture_hid_etw.ps1`不自动提权/不装驱动。要求管理员终端、新输出目录、明确端口/版本/序号和Python路径，先采CDC基线及当前HID路径，再开启16MB循环ETW；查询COL02一次并采查询后CDC；finally停止自己的随机命名ETS会话并检查退出码。UCX/HUB3用HeadersBusTrace0x40，USBPORT用Diagnostic1，实际事件是否有payload以provider schema为准。

在仓库根目录的管理员PowerShell中运行，输出目录必须尚不存在：

```powershell
powershell -NoProfile -File tests/integration/capture_hid_etw.ps1 -Port COM29 -Serial 5054316560DD729C -Version 0.10.1 -OutputDirectory tests/integration/hid_etw_next_run -Python C:/Users/admin/AppData/Local/Programs/Python/Python312/python.exe
```

采集后先交Luna审核。确认stop成功、目标身份、正常CDC正对照流量及事件丢失情况，再查目标mouse interface3的GET_REPORT（bmRequestType A1、bRequest01、wValue0102、wLength6）与完成状态。不能用空日志/缺事件或XML转换成功来推断没有USB请求。若中途抛错，ETL/命令记录会保留，tracerpt及配置hash后处理需另行完成；失败报告不能改写为通过。

若Windows提供程序无法给出足够的控制细节，再选被动USB分析器/USBPcap或独立主机；在获得决定性证据前保持固件不变。
