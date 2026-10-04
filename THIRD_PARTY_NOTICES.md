# 来源与第三方资料

本仓库包含FluxRail自己的当前源码，以及为审核/回滚保留的旧项目和第三方选集。自2026-10-05起，FluxRail原创代码及配套文档采用根目录的[MIT许可证](LICENSE)，版权署名为`kirisamesusi`。

## MIT适用范围

MIT适用于本项目原创的固件、Web配置工具、测试/维护脚本、依赖生成脚本及配套文档，也适用于归档/备份中由同一作者拥有版权的原创部分。历史文件与清单原字节保留，历史“尚未选择许可证”的记录按当时状态解读。

MIT不替代第三方文件的许可证，也不授予本项目作者不拥有的版权、第三方商标或其他权利。`references/`、`firmware/recovery/`、安装库取证和回滚依赖等第三方资料，及其备份副本，按原文件的许可和来源声明处理；不得因为根目录有MIT就视为全部可按MIT再发布。未发现明确许可证的第三方源码不获得额外再发布授权。

## 第三方来源

- `references/io4/`：来源、固定commit、URL与SHA见`SOURCES.json`，角色见其README。发现的根许可及文件版权头原样保留；README已指出部分来源没有独立根LICENSE，不能将选集当完整可构建工程。
- `references/arduino-usb-audit/`：本机Arduino/TinyUSB原样取证，来源说明与SHA保留；不是当前安装依赖。
- `references/legacy-rp2040-zero/`：旧RP2040-Zero源码、网页和算法参考，保留历史名称与版权信息。旧Git元数据本地另存，不作为嵌套仓库上传。
- `dependencies/prepare_tinyusb.py`：从已固定的Arduino-Pico6.1.0附带TinyUSB3.7.7生成项目私有适配；生成目录不入Git，回滚包内的既有依赖保留其LICENSE和manifest。
- `firmware/recovery/`：官方Flash清空UF2的原样副本及`source.json`记录，作用/来源/SHA见其README。
- `backups/`：历史源码/依赖/固件快照及文档原稿，使用对应原文件的来源说明，不能将这些目录称为新实现。

已保留的许可包括：`references/io4/SEGA835Lib/LICENSE.txt`为SSPL v1，`references/io4/segatools/LICENSE`为Unlicense，回滚包内Adafruit TinyUSB根LICENSE为MIT；具体文件如有不同版权头或许可，以对应文件声明为准。IO4参考选集中的GEEKiDoS/Lee来源未发现独立根LICENSE，仍需按原来源授权情况处理。

后续新增或更新第三方资料时，继续保留其版权头、许可证和来源SHA；不要用FluxRail的MIT覆盖这些声明。
