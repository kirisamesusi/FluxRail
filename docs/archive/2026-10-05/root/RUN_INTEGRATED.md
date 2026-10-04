# FluxRail 整合工程运行与验收

2026-10-03。唯一正式工程为 firmware/FluxRail/FluxRail.ino；源码现已接入模块1～4，测试与编译不执行上传。最新事实以 docs/progress/模块整合.md 和 tests/integration/validation_integration_2026-10-03.json 为准。

## 一键软件验证

2026-10-04当前源码候选为0.10.1，包含Hall固定相位调度和标准绝对XY描述符修复；Y初始中点，随后保留本设备旋钮位置，无额外主机程序。Windows启动与实板1kHz仍需新版验证。后文0.10.0与先前验证记录属于历史基线。最新变更与分层证据以 [修复记录](../progress/修复.md) 为准。

2026-10-04后续裸板实测已完成本机COL02启动（ProblemCode0）及Hall空闲/轮询/通信压力平均约1kHz；RAM/协议/CDC会话与网页生产客户端适配通过，逐阶段Luna审核。补充HID当前状态控制读取未通过（错误1），真实外设XY/Y保持/按钮、Flash/IO4、浏览器Web Serial/UI和游戏仍待验；详见[测试进度](../progress/测试.md)及[裸板测试入口](../../../../tests/integration/BARE_BOARD_RUN.md)。上段“仍需新版验证”保留前阶段状态，以该分层新证据为准。

在项目根目录 PowerShell 执行：

~~~powershell
cd C:/Users/admin/Documents/FluxRail
python tests/integration/run_all.py
~~~

依次执行模块1/2/3、跨核/USB传输/协议/存储/App/IO4/完整运动校准的真实C++ ARM测试、网页测试、正式Arduino构建和正式ELF描述符测试。失败立即停止，build/full_results.json及时记录实际步骤，完整日志在tests/integration/build/full。只跑软件模拟与Web可加 --skip-build；该结果不包含正式构建。缺依赖/指令预算不足不是通过。

已安装工具：Python3.12、Unicorn2.1.3、Node>=20（本机24.14.1）、Arduino CLI0.35.3、Arduino-Pico6.1.0 / ARM GCC16.1.0。测试复用tests/module1/build/python-deps；新环境依赖准备见tests/module1/README.md。本机可将python替换为C:/Users/admin/AppData/Local/Programs/Python/Python312/python.exe。不需要npm install。构建配置固定4MiB noFS/W25Q64JV /4/125MHz/TinyUSB，保留0x3FB000之后配置区，不用IDE默认分区直接上传。

仅重新构建与核对描述符：

~~~powershell
python tests/integration/compile_firmware.py
python tests/integration/run_usb_descriptors.py
~~~

正式产物为 tests/integration/build/arduino/FluxRail.ino.uf2 与 .elf。module1/2/3独立烟测UF2没有完整设备功能，不能作为整机固件。构建使用项目私有TinyUSB启动适配，安装库不改动；USB/存储/协议均只有既有一个所有者。

2026-10-05新增强制回滚规则：每次正式编译前，入口会先保存上次成功固件、完整对应源码、下一源码快照、`changes.patch`、依赖及SHA清单至 `backups/firmware-rollback/<旧版本>/<UTC时间>/`；无法核对旧源码/依赖或完成备份则不执行编译。旧源码以实际构建manifest核对，不只备份改动的几个文件。恢复说明在每包README，设备配置备份和固件回滚分开处理。已补齐0.10.1与0.10.2回滚包；0.10.2实板通信复测最新状态见 [测试进度](../progress/测试.md)。

## 网页运行

2026-10-04当前页面、模板保存确认、六套Web测试及后续修改流程见 [Web开发交接](../handoffs/FluxRail_Web_开发与调整交接.md)；旧版与后续快照见 [备份说明](../../../../backups/README.md)。下方历史界面说明按该交接核对。

~~~powershell
node web/server.mjs
~~~

打开 http://localhost:4174，桌面Chrome/Edge由用户点“连接 FluxRail”选择串口。若端口已占用且当前FluxRail服务器已运行，直接使用现有页面；需要另起时设置FLUXRAIL_WEB_PORT。服务器只监听本机，页面不替代设备实时执行。Codex内嵌浏览器仅验证界面/隔离模拟，不证明Web Serial硬件连接。模拟演示入口为“实验性功能→连接·模拟与设备信息→模拟演示”，其schema1与虚拟Flash不能证明实板通过。

操作顺序：连接并读回→编辑所选模板→应用到RAM→需要掉电保留时显式保存。激活模板独立于选择编辑模板。真实旋钮圆盘使用设备累计raw/scaled圈数；本地模拟圆盘明确独立标识。全包导入导出支持schema1/2，固件负责最终校验。

## 首次硬件接入

1. 用户安排实板验证后，先核对新板引脚/Flash/UF2 family，按现有硬件清单刷写正式UF2；本轮没有上传。先普通身份枚举、十键/灯，再检查ADC/SSI启动和错误计数。
2. 无有效校准时滑轨不输出，原始ADC仍显示。移开磁铁确认空场，填写实际端点mm；依次三路参考、7～17个实际毫米标签（端点与交接），检查后全轨扫描，再结束采用。失败/取消保留旧记录；成功仅RAM，显式保存后才能掉电复用。测试曲线mm值不是设备默认行程。
3. 开启需要的来源/轴/倍率并应用。默认音击双旋钮关闭；勿把合成测试当实板CRC/磁场/安装方向证明。验证初次绝对报告Y居中、旋钮移动Y后Hall保持该Y、双旋钮方向/PUSH、来源接管、USB忙/断连/保存恢复；普通相对模式另测。
4. IO4功能表独立于普通键盘，工程默认布局需用户核对。冷切换必须应用启动身份→保存→重启到已保存身份。IO4默认无CDC，返回普通按住BT10/GP9复位或上电，之后重新连接网页；BOOTSEL是ROM最终恢复入口。游戏兼容尚未验证。
5. UF2命令仅进入ROM下载模式，设备应答总线完成后才detach；仍需用户手动复制正确固件并重连核对版本。页面不会自动上传文件。

详见tests/integration/HARDWARE_CHECKLIST.md、tests/module2/HARDWARE_CHECKLIST.md、tests/module3/HARDWARE_CHECKLIST.md。真实双核竞争、ADC/SSI/CRC、Flash掉电、PIO灯色/时序、Windows HID/Web Serial及游戏仍未验收；网页诊断约10Hz不能证明毫秒采样无丢失。来源余量/策略账本当前在MotionRouter及ARM测试中可查，公共benchmark和完整路由原因遥测尚未增加。

## 实验室 IO4 映射编辑（2026-10-03）

普通 USB 身份连接并读回设备，在模板区选择待编辑模板，展开“实验性功能 → IO4 · 按键功能映射”。十二路卡片按物理顺序显示，选择左/右 A、B、C、菜单、侧键、Test、Service或禁用。下方显示设备读回值及未应用草稿；编辑的是该模板的 IO4 功能表，普通键盘字母/鼠标表独立保留。

操作顺序：修改 → 应用 IO4 映射到 RAM → 如尚未激活则“将本模板设为活动” → 更新区选择 IO4 并“应用启动身份到 RAM” → 保存到设备 → 重启到已保存身份。启动身份应用后必须再保存，即便此前保存过映射。IO4默认无CDC，按住BT10/GP9复位或上电返回普通再连接。重复功能按 OR 持有，BT10恢复入口不受自定义功能影响。

未应用的IO4草稿会阻止共享写操作、保存、模板切换、USB身份、校准和UF2；可继续修改或点“读回映射（舍弃草稿）”。普通配置有草稿时先应用/读回，再编辑IO4。外部revision冲突禁止写入，不盲重试。所有写操作复用set_io4_bindings/set_active_profile/save_config与既有USB冷切换，未另建存储或协议。

本轮Web33项、ARM IO4 22组67513原始检查通过，包含普通编辑→保存→重建IO4按键报告验证。固件源码和阶段4正式UF2均未改变，无需因网页更新重新刷已部署整合固件。浏览器仅验证真实布局/断连锁定，无实板或游戏验证。证据tests/integration/validation_io4_editor_2026-10-03.json；完整原构建证据仍见validation_integration_2026-10-03.json，其Web30项是历史版本。


## 2026-10-03 Web 实验室布局与文案修正

实验室共 11 个分组，连接 · 模拟与设备信息排第一，运动 · 设备状态与共享校准排最后；IO4 · 按键功能映射位于同一内容容器内、运动分组之前，统一缩进/边框/间距。校准参数、操作按钮、状态说明分开排版，复选框靠近文字；滑轨与旋钮主区都可跳转并展开运动/校准面板。旋钮本地模拟、实际遥测及进阶输出/PUSH 防误触参数继续保留，未连接时显示进阶参数读取提示。

更新 schema1/2 导入、运动遥测、来源接管、USB 冷切换等历史提示；滑轨进阶状态显示现有 get_motion 读回的 ADC/位置/质量，断连或不支持时清除旧状态。UF2 按 bootloader_command 能力启用，校准编辑受维护/配置应用锁约束。未改固件、GPIO、保存格式或 CDC 命令；此网页修正无需重新刷写固件。

Web 四套测试共 35 项通过，含 IO4 容器位置、旋钮设备状态容器、升级能力及校准维护锁回归。浏览器验证实验室顺序/一致缩进、校准表单、模拟连接、两路旋钮及进阶参数、一圈累计读数和旋钮校准链接；控制台无 warn/error。模拟与 DOM 验证不代替实板/真实串口/Flash/游戏验收。


### 2026-10-03 滑轨基础设置显示修正

滑轨启用、反转输出方向、中心倍率在未连接或旧固件无 motion 时也显示，未读取参数保持禁用、倍率显示“待读取”；连接后复用同一设备草稿与实际读回值。按用户要求移除滑轨/旋钮主区“查看运动状态与共享校准”链接及提示条，共享校准仍在实验室末项。Web35项测试通过；浏览器验证三个控件可见、模拟连接后解锁、反向与倍率应用/读回一致、该链接文字已移除，控制台无warn/error。截图web/tests/hall-controls-2026-10-03.png。

## 当前版本与 UF2 检查（2026-10-04）

正式固件版本 0.10.0。Web 选中文件后自动识别 FluxRail/板型并与当前设备版本比较；无需手填版本或核对文件哈希。提示版本相同、当前较低可升级、所选版本过低或固件不对。旧版无识别头 UF2 无法识别；旧设备版本可以继续参与比较。发布只修改 firmware/FluxRail/src/common/FirmwareInfo.h 的 FLUXRAIL_FIRMWARE_VERSION，再运行 compile_firmware.py，脚本自动验证实际 UF2。正式产物位置不变，仍需手动复制并重连。详情见 [版本与UF2识别](../spec/15_固件版本与UF2识别.md)。
