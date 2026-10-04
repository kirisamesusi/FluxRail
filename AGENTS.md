# FluxRail 开发入口

新窗口先读`docs/FluxRail_开发交接_窗口必读.md`，再读`docs/公共架构与配置契约.md`、`docs/硬件与引脚.md`和任务对应的`docs/modules/`文档。导航在`docs/README.md`，最新分层验证在`docs/测试状态与待验收.md`。`docs/archive/`保留历史，`docs/spec/`和`docs/progress/`仅兼容入口。

## 工程约束

- 当前开发与待验收分支为`test`；新任务从`test`继续。正式验收后再按用户指令建立/合并到`master`发布，不能仅因软件测试通过就标记正式可用。`main`保留首次上传历史。

- 唯一正式工程`firmware/FluxRail/FluxRail.ino`。模块已接入App，不按旧“未实现/未接入”说明重新造实现。
- 核心1采集，核心0唯一USB/配置/灯/维护。复用现有DeviceConfig、CDC v1、schema2和ConfigCoordinator实际应用ACK；业务模块不另建USB、Flash保存或协议。
- 滑轨绝对，旋钮相对增量；普通绝对XY后端保留本设备Y；十二来源可映射、不同usage NKRO、同键盘usage每次DOWN重触发；音击双旋钮默认关闭。
- 引脚/BT编号/灯链与亮度限制以硬件及模块文档为准；最终目标4MiB与历史2MiB裸板实测须区分。
- 编译、ARM模拟、DOM、裸板、最终硬件和游戏分别报告；没有证据不能写通过。原始冻结测试证据不覆盖，不改旧SHA。

## 构建与交付

新克隆先读`docs/仓库结构与克隆开发.md`。`python scripts/prepare_checkout.py`校验并恢复历史回滚基线，已有构建不覆盖；恢复报告不代表本次编译或实测。

统一入口`python tests/integration/run_all.py`，正式编译`python tests/integration/compile_firmware.py`；正式编译前自动保留旧UF2/ELF、对应完整源码、下一源码、diff及依赖，备份不完整停止。不要绕过入口覆盖旧正式产物。工具链、FQBN和维护流程见`RUN_INTEGRATED.md`。

**每次正式编译交付的UF2/ELF文件名必须带固件版本号。** 入口在检查通过后输出`tests/integration/build/releases/<版本>/<UTC时间>/FluxRail-<版本>-<UTC时间>.uf2/.elf`及摘要清单；同版本重编译分开保留。版本来自FirmwareInfo.h与实际UF2校验，不能仅改文件名。Arduino内部固定产物供既有校验/回滚使用，交付优先带版本文件。独立模块烟测UF2不能当整机固件。

Web从根目录`node web/server.mjs`，localhost:4174；Web软件测试在web/执行`npm test`。根据改动范围选择相关检查，具体任务与设备操作范围按用户当前指令执行。

## 文档同步

模块改动更新相应模块交接；公共接口/schema/协议变动同步公共契约、受影响模块及测试；新实测建日期/版本独立证据并更新状态表。文档结构变化运行`python docs/tools/verify_docs.py`。详细规则见`docs/文档维护与归档.md`。
