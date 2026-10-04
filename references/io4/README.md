# FluxRail IO4参考源码

2026-10-03从GitHub下载30个相关文件，原样保存；固定commit、URL、尺寸与SHA256见[SOURCES.json](SOURCES.json)。这是源码选集用于协议核对，不是完整独立可编译工程。未执行/编译/上传第三方代码。

| 目录 | 角色 |
| --- | --- |
| ongeeki-firmware | ESP32-S2 TinyUSB IO4固件的描述符/报告/回调/硬件映射 |
| SDDT_io4_esp32s2 | 同源旧分支的OUT端点/配置/回调对照，非独立成功证明 |
| SEGA835Lib | 电脑IO4读写、结构与能力核对；不是设备固件 |
| segatools | 主机IO4协议与音击switch/ADC转换核对 |
| failed-lkick | 项目旧IO4核心失败样本；用户实测不可用，不作为成功实现基线 |

详细结论见[GitHub审查](../../docs/spec/10_IO4外部项目对比.md)和[Arduino冷切换审核](../../docs/spec/09_IO4兼容与USB冷切换.md)。

保留原版权头与可发现的根许可。GEEKiDoS/Lee此次树中未发现独立根LICENSE，不将其源码复制再发布权限作默认假设。新实现可以按核实后的协议行为独立编写。没有下载ESP-IDF、FreeRTOS、Pico-SDK、PN532或驱动盘依赖。
