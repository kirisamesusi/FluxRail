# 公共集成与IO4软件验证

2026-10-04补充：当前0.10.1只有开发板的分阶段实测工具、显式版本/序号/证据目录与Luna审核门槛见[BARE_BOARD_RUN.md](BARE_BOARD_RUN.md)。新实板证据使用`hardware_0_10_1_2026-10-04/`，旧0.10.0证据保留；编译/模拟与实板/游戏结论各自记录。

验证模块1/完整配置/IO4公共适配，不修改模块2/3/4算法。build与stubs/stubs_app仅测试，不进入正式固件；当前长期证据[validation_module1_2026-10-03.json](validation_module1_2026-10-03.json)，历史validation/validation_cdc/validation_storage/validation_io4保留。

~~~powershell
python tests/module1/run_tests.py
python tests/integration/run_tests.py
python tests/integration/run_transport.py
python tests/integration/run_protocol.py
python tests/integration/run_storage.py
python tests/integration/run_app_storage.py
python tests/integration/run_io4.py
python tests/integration/compile_firmware.py
python tests/integration/run_usb_descriptors.py
python tests/integration/check_module1_regressions.py
python tests/integration/finalize_module1_report.py
~~~

模拟链接原始C++，ARM GCC16.1.0/Cortex-M0+，Unicorn2.1.3实际执行ARM Thumb指令；不是JS/Python重写算法。每套记录passed/completed和编译前后源哈希，超过指令预算为未完成。配置截断与IO4穷举按组提高预算，不能以预算退出当断言失败。七套74组144,358检查；io4含五组App fixture重复执行，所以合计是执行次数。

| 套 | 组/检查 | 主要真实C++路径与边界 |
| --- | --- | --- |
| module1 | 11/68,415 | 扫描/Mapper/灯；不同usage NKRO、同usage重触发、Shift顺序、鼠标OR、满队列/回绕/颜色/限幅/已确认链序 |
| integration | 8/2,021 | InputWorker/Coordinator/NormalHidCodec；SPSC/邮箱/暂停ACK/净状态恢复/epoch/描述符字段 |
| transport | 2/47 | NormalHid/UsbBackend；报告复制/ready/mutex/完成失败/GET与按钮；端点仅替身 |
| protocol | 9/1,320 | Json/RuntimeCodec/ConfigProtocol/CdcTransport；前缀截断/范围/多行/背压/限制/真实两核逻辑与DTR/FIFO替身 |
| storage | 18/5,012 | ProfileManager/DeviceCodec/ConfigStore；12模板/17节点/稳定ID/金样/全部4371字节前缀/独立扇区与每页commit中断/CRC语义/导入隔离/维护 |
| app_storage | 5/44 | 原始App及维护，开机读回/预览不擦写/等待实际释放/失败恢复/断连超时；Flash/GPIO/LED/时钟/watchdog边界替身 |
| io4 | 21/67,499 | Io4Codec/UsbBackend/原始App/协议/存储；IN金样/OUT全部长度和ID等非法形态、12路低有效、65536 ADC值、cmd状态机/超时/GET/主机灯、单端点维护、早期读取/启动恢复、全schema1迁移/混合槽、模式save中断、双向冷重启与CDC总线完成门槛 |

run_usb_descriptors.py另执行正式ARM ELF实际Device.begin、HID注册及descriptor回调，不依赖简化Adafruit替身；两人格通过：普通CDC＋两HID、IO4单HID/64字节INOUT/1ms、SEGA identity、product92/HID字符串93字符、unique serial转换、首次Port连接前注册。只有保存模式选择（另有实际App测试）、Port连接、ROM memcpy/memset、Flash unique-ID硬件边界替身；不模拟USB总线/主机枚举。完整device/config/report/string记录在build/usb_descriptor_results.json并纳入固定快照，ELF哈希变化拒绝旧记录。

compile_firmware.py使用固定CLI0.35.3/Pico6.1.0/TinyUSB3.7.7项目私有生成适配；固定4MiB/noFS/125MHz和0x3FB000分区界限，FLUXRAIL_IO4_CONFIG_CDC=0。从真实compile_commands重编所有36项目单元，保留头文件顺序并-Werror；第三方已知缺省字段降为warning，项目warning拒绝。不能用-isystem误混另一份TinyUSB。11 USB相关强符号/核心1入口/源复制/构建前后哈希/存储分区/Flash例程SRAM地址/静态帧≤2048均审计。当前Flash126,684/RAM81,836、最大静态帧1192字节，不代表真实栈峰值。

finalize_io4_report.py核对各套源哈希、完成状态、正式源码/分区/栈、描述符ELF哈希，并冻结UF2与脚本/样例/边界哈希；任何C++/头/入口改动必须重跑相应全部证据。旧finalize_storage_report.py委托该脚本，不覆盖历史存储快照。结果和日志位于build，不单独以旧日志证明新源。

schema1_defaults.bin保留阶段8独立金样1339字节/CRC3954875198；schema2_defaults.bin当前为1387字节/CRC1053904252，schema2_defaults_phase9.bin保留原配色/CRC894450391，每模板旧尾新增12 u8。cdc_examples/storage_examples继续演示既有命令，新io4_examples示范独立绑定→模式RAM预览→显式save→reboot。cdc_client.py仅使用调用者提供连接，不发现/打开端口、不上传、不自动save；真实revision使用每步应答值。

实板/Flash掉电/双核锁定/USB主机/Web Serial/PIO/灯色序/机械/游戏均未验证，IO4游戏兼容性未验证。Hall/Knob实际调度和普通绝对X仍未接入；绝对位置边界测试不当成真实采集。实板暂缓、不上传、不改游戏配置；后续见[HARDWARE_CHECKLIST](HARDWARE_CHECKLIST.md)、[IO4进度](../../docs/progress/IO4.md)。


阶段10新增：全部169支持usage的十二来源重触发；本地/IO4共用亮度（0..100、关闭、非法/背压/功能物理灯）；已确认模板和实际USB身份指示；faulted+busy LED禁止保存且超时恢复输入；新默认/复制/保存/迁移色保持。新增用例和最终审查见tests/module1/REVIEW_2026-10-03.md。

check_module1_regressions.py仅在build副本恢复旧灯亮度/DMA门槛，严格编译后确认预期回归失败；结果纳入新快照，不影响正式产物。finalize_module1_report.py调用既有汇总实现，写validation_module1_2026-10-03.json；旧finalize_io4_report.py入口也写本阶段新文件，绝不覆盖validation_io4历史快照。金样脚本核对仅四个indicator RGB位置可改变，schema1与阶段9原默认哈希保持。
