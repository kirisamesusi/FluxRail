# FluxRail

RP2040音游控制器：十键、三霍尔绝对滑轨、双MT6701旋钮/PUSH、灯光与Web配置。模块1～4已接入一个正式Arduino工程，复用统一USB、schema2配置双槽和CDC v1。普通身份支持NKRO、相对X/Y及标准绝对XY；绝对模式Y从中点开始并保留本设备旋钮调整。IO4支持Hall绝对ADC与独立十二路功能表。当前0.10.2已在用户2MiB裸板通过IO4协议及100ms/400ms/25.5s超时窗口复测；最终4MiB硬件、真实外设及游戏未验，完整分层状态与恢复测试见测试/修复记录。

## 运行与交接

- [整合运行说明](RUN_INTEGRATED.md)：一键测试/编译、网页、首次校准、保存、冷切换和UF2流程。
- [模块整合进度](../progress/模块整合.md)：当前完成、真实验证、限制与接手位置。
- [固件版本与UF2识别](../spec/15_固件版本与UF2识别.md)：0.10.0起的识别头/版本比较、后续修改和发布步骤。
- [实测问题修复](../progress/修复.md)：0.10.1采集调度与标准绝对XY修复候选；Windows重新枚举、实板1kHz及游戏待验证。
- [公共运行契约及接口影响](../spec/14_模块整合运行契约.md)。
- [固件结构](../source-readmes/firmware/FluxRail/README.md)、[网页说明](../source-readmes/web/README.md)。
- [Web 开发与调整交接](../handoffs/FluxRail_Web_开发与调整交接.md)：当前布局、模板规则、构建/修改/验证及下一窗口接手。
- [旧 Web 备份说明](../../../../backups/README.md)：原版与后续快照的范围、校验和恢复。

从项目根目录运行：

~~~powershell
python tests/integration/run_all.py
node web/server.mjs
~~~

网页为 http://localhost:4174；正式UF2为tests/integration/build/arduino/FluxRail.ino.uf2。测试/编译脚本不上传，独立模块烟测UF2不是整机版本。

每次编译新固件前，必须保存旧固件、完整对应源码及改动记录，便于回滚。正式编译入口自动创建 `backups/firmware-rollback/<旧版本>/<UTC时间>/`，内含旧UF2/ELF、经原构建SHA核对的源码、下一版源码快照、`changes.patch`、依赖和恢复说明；备份不完整则停止编译。不要绕过该入口直接覆盖旧产物。当前0.10.2及回滚包位置、实际测试状态见 [测试进度](../progress/测试.md)。

## 需求与开发入口

[主需求](../handoffs/FluxRail_需求总览_用户版.md)、[窗口必读](../handoffs/FluxRail_开发交接_窗口必读.md)、[公共契约](../spec/01_公共接口与集成约定.md)、[硬件](../handoffs/rp2040_kb_firmware_notes.md)、[文档索引](../spec/00_项目需求与决策.md)、[验收](../spec/06_验收与实测计划.md)。模块1～4独立进度是历史模块事实，整合当前状态以模块整合进度和对应验证快照为准。

## 本地参考

- [旧项目保留内容](../../../../references/legacy-rp2040-zero/README.md)：原版/优化版/诊断、算法测试、网页和Git历史。
- [IO4源码选集](../../../../references/io4/README.md)：四个GitHub来源、固定commit/SHA256及失败样本。
- [本机Arduino USB快照](../../../../references/arduino-usb-audit/README.md)：原样取证，未修改安装库。
- [清理记录](../../../../references/CLEANUP.json)：已授权清理的809个文件与保留/移动记录。

