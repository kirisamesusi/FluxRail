# 来源与第三方资料

本仓库包含FluxRail自己的当前源码，以及为审核/回滚保留的旧项目和第三方选集。没有为项目整体新增许可证。

- `references/io4/`：来源、固定commit、URL与SHA见`SOURCES.json`，角色见其README。发现的根许可及文件版权头原样保留；README已指出部分来源没有独立根LICENSE，不能将选集当完整可构建工程。
- `references/arduino-usb-audit/`：本机Arduino/TinyUSB原样取证，来源说明与SHA保留；不是当前安装依赖。
- `references/legacy-rp2040-zero/`：旧RP2040-Zero源码、网页和算法参考，保留历史名称与版权信息。旧Git元数据本地另存，不作为嵌套仓库上传。
- `dependencies/prepare_tinyusb.py`：从已固定的Arduino-Pico6.1.0附带TinyUSB3.7.7生成项目私有适配；生成目录不入Git，回滚包内的既有依赖保留其LICENSE和manifest。
- `firmware/recovery/`：官方Flash清空UF2的原样副本及`source.json`记录，作用/来源/SHA见其README。
- `backups/`：历史源码/依赖/固件快照及文档原稿，使用对应原文件的来源说明，不能将这些目录称为新实现。

仓库整理保留了已有许可与来源记录；后续如选择整体开源许可，应单独明确FluxRail原创部分的范围，并继续保留第三方自身的许可。
