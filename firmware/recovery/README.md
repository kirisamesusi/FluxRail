# RP2040 Flash 初始化 / FluxRail 恢复

RP2040_Flash_Reset.uf2 是 Raspberry Pi 官方 nuke_universal.uf2 的原样副本，仅改文件名。官方文档：https://www.raspberrypi.com/documentation/microcontrollers/pico-series.html#reset-flash-memory

**它会清除外部 Flash 内的应用和所有保存设置，包括 IO4 启动模式、模板和校准。清空后必须重新刷正式固件。**

1. 拔掉 USB，按住开发板 BOOT/BOOTSEL 按钮再连接 USB，出现 RPI-RP2 后松开按钮。
2. 将 RP2040_Flash_Reset.uf2 复制到 RPI-RP2。等待清空程序执行完成并重新出现 RPI-RP2；不要立即拔电。
3. 将 ../../tests/integration/build/arduino/FluxRail.ino.uf2（正式固件）复制到 RPI-RP2。若盘符没有重新出现，重新按住 BOOTSEL 插入 USB。
4. 重启后以默认普通身份运行，再从 Chrome/Edge 的 FluxRail 网页重新选择串口连接。

此恢复文件只提供清空功能；它本身不会提供 FluxRail 网页串口。无需 GP9 跳线。

已经检查 UF2 文件头、尾标记、块序号和 RP2040 family ID 0xE48BFF56；尚未在实板运行。来源、SHA256 和校验信息见 source.json。
