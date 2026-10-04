# FluxRail 本机Arduino USB审核快照

2026-10-03原样复制Arduino-Pico6.1.0及核心自带Adafruit TinyUSB3.7.7的11个相关文件。原路径、哈希见[SOURCES.json](SOURCES.json)，保留原版权头。未修改电脑安装目录。

检查启动时机、begin清描述符/默认CDC、32字符限制、HID实例与强回调、控制/中断OUT差异、普通软重启。结论见[IO4与冷切换审核](../../docs/spec/09_IO4兼容与USB冷切换.md)。

这是审核证据，不是已完成的USB补丁或完整构建依赖。不要把它直接include进新sketch，避免和核心已有回调重定义。
