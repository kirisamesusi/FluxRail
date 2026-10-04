# FluxRail 正式整合固件

更新：2026-10-05。唯一入口FluxRail.ino，当前固件版本0.10.2，按键/灯、Hall、双MT6701/PUSH、USB/IO4、schema2及CDC已接入App。已有2MiB裸板分层验证，最终4MiB硬件与游戏待验；具体结论见[测试状态](../../docs/测试状态与待验收.md)。

- [公共架构与配置契约](../../docs/公共架构与配置契约.md)：双核、队列、实际ACK、来源路由、Flash与维护。
- [运行、构建与发布](../../RUN_INTEGRATED.md)：固定FQBN、依赖、正式产物、自动完整回滚、版本识别和校准。
- [按模块源码入口](../../docs/README.md)：keyboard/lighting、usb/protocol、hall、knobs和Web。
- [当前CDC命令表](src/protocol/CDC_PROTOCOL.md)：字段与请求应答边界。

正式构建从项目根执行`python tests/integration/run_all.py`或`python tests/integration/compile_firmware.py`；不上传设备。Flash分区和私有TinyUSB适配由正式入口固定，独立烟测UF2不能替代整机固件。版本唯一入口src/common/FirmwareInfo.h。

每次成功构建导出带版本号和UTC构建时间的UF2/ELF及SHA清单到`tests/integration/build/releases/<版本>/<UTC时间>/`；交付优先使用`FluxRail-<版本>-<UTC时间>.uf2`，同版本多次编译分别保留。内部固定Arduino文件用于既有校验和回滚，具体规则见运行发布。

核心1拥有InputWorker/Acquisition和模块采集，核心0拥有App服务、唯一USB、ConfigCoordinator、MotionRouter、配置存储、CDC和灯；其他模块不另建主循环/USB/Flash。详细说明已整合到公共架构与各模块文档。

[旧固件README完整归档](../../docs/archive/2026-10-05/source-readmes/firmware/FluxRail/README.md)保留历史资源数字与0.10.1候选状态；原数字不可当作0.10.2的新构建证明。
