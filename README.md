# FluxRail

RP2040音游控制器：十键、三霍尔绝对滑轨、双MT6701旋钮/PUSH、键灯与Web配置。主要软件已整合为一个Arduino工程，当前源码0.10.2；已有2MiB裸板通信/IO4分层复测，最终4MiB硬件、真实外设和游戏尚未完整验收。

当前开发与待验收使用`test`分支；正式验收完成后再建立/合并到`master`发布。`main`保留首次上传历史，后续开发从`test`接手。

FluxRail原创代码及配套文档采用[MIT许可证](LICENSE)，版权署名为`kirisamesusi`。第三方源码、依赖、参考资料及其归档副本保留各自许可，具体范围见[来源与第三方资料](THIRD_PARTY_NOTICES.md)。

## 开发与交接

- [文档中心](docs/README.md)：按模块分类的全部现行文档。
- [新窗口必读](docs/FluxRail_开发交接_窗口必读.md)：阅读顺序、当前事实与直接接手说明。
- [运行、构建与发布](RUN_INTEGRATED.md)：正式工程、工具链、Web、校准与固件回滚。
- [仓库结构与克隆开发](docs/仓库结构与克隆开发.md)：GitHub目录、依赖准备、历史构建恢复与上传规则。
- [测试状态与待验收](docs/测试状态与待验收.md)：各版本已验证范围、未测项及下一步。
- [基础素材库](design/asset-library/README.md)：用户选定的水色角色、原图、头像、键盘插画及可复用设计信息。

| 模块 | 开发文档 |
| --- | --- |
| 按键 + 灯光 | [01 按键与灯光](docs/modules/01_按键与灯光.md) |
| USB + IO4 + CDC | [02 USB与IO4](docs/modules/02_USB与IO4.md) |
| 滑轨 | [03 三霍尔滑轨](docs/modules/03_三霍尔滑轨.md) |
| 旋钮 + PUSH | [04 旋钮与PUSH](docs/modules/04_旋钮与PUSH.md) |
| Web | [05 Web配置工具](docs/modules/05_Web配置工具.md) |
| 双核、配置、模板与Flash | [公共架构与配置契约](docs/公共架构与配置契约.md) |

## 快速运行

从项目根目录分别执行：

```powershell
python scripts/prepare_checkout.py
python tests/integration/run_all.py
node web/server.mjs
```

网页`http://localhost:4174`；正式入口`firmware/FluxRail/FluxRail.ino`。每次正式编译的交付UF2/ELF都带版本和构建时间，存于`tests/integration/build/releases/<版本>/<UTC时间>/`，例如`FluxRail-0.10.2-<UTC时间>.uf2`。软件验证脚本不上传设备，独立模块烟测UF2不能代替整机固件。正式编译前自动保存旧UF2、完整对应源码、下一源码、diff和依赖，备份不完整则停止，详见运行说明。

## 历史与备份

[旧规格/进度/交接归档](docs/archive/2026-10-05/README.md)、[文档维护规则](docs/文档维护与归档.md)、[Web与固件备份](backups/README.md)、[旧板参考](references/legacy-rp2040-zero/README.md)、[IO4固定参考](references/io4/README.md)、[USB安装库取证](references/arduino-usb-audit/README.md)。2026-10-05整理保留旧记录与失败证据，当前完成度以现行测试状态表为准。
