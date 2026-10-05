# 2026-10-05 生产 Web 重构软件证据

范围：test 分支沿现有生产实现落实固定设计模板，保留已有未提交工作。没有正式编译或打开真实串口，不刷机、合并、提交或发布。

- [最终 Node 输出](node-final-output.txt)：67项通过，新增组合保存/失败/未知结果/确认后状态变化/刷新失败和冷命令拒绝锁写测试；原协议、schema、模板保护和UF2回归保留。
- [语法检查](syntax-final-check.json)：9个改动生产ES模块通过。
- [最终浏览器记录](final-delivery/browser-check.json)：Edge（实际版本151.0.4129.107，见JSON），1274/390/320px；未连接、生产原模拟器、隔离CDC夹具各8模块，共72视图，无页面横向溢出或pageerror。9组流程涵盖导航/历史/焦点、物理来源布局、概况同步、刷新确认、小数1.5/旧倍率0.5和12、整数刻度原生鼠标落点、IO4/重启/UF2取消/Escape/确认、模式保存顺序、互斥/维护/校准/revision、应用与保存失败恢复、UF2本地识别、旧runtime、换设备及握手失败清旧诊断、命令拒绝锁写。
- [GPT-6-Luna评审与处理](LUNA_REVIEW.md)：两轮4项已修复，最终复核确认风险关闭，未发现阻断问题。
- [文档检查](docs-final-check.json)：现行文档链接检查，不覆盖原归档报告。
- [源码与证据SHA清单](manifest.json)：本次交付源文件与最终软件证据，不是设备上传或固件验收证明。

[检查脚本](check.mjs)在隔离Edge进程内把 navigator.serial.requestPort 绑定到现有DemoHost传输。扩展能力与注入错误只存在测试进程，不进入生产demo.js、不接触串口选择器或设备。UF2测试由脚本构造仅含识别头的解析夹具，不是可交付/可刷固件。正式设备的实际ACK、Flash保存、模式启动、ROM进入及更新均未验证。浏览器截图中 TEST-FIXTURE-ONLY 序号明确是夹具。

生产模拟器仍只声明其原schema1能力；不会用夹具能力冒充生产模拟器支持真实校准/IO4/固件更新。完整已保存配置由固件ACK保证，页面读回当前模板/元数据/保存状态；本轮没有导出全部设备字节与Flash逐字节核对。

[基线保护检查](baseline-protection.json)核验78项修改前文件，其中61项保持原字节，17项仅位于本轮授权修改范围；所有角色原图、既有证据和生产服务器保持原字节。

倍率：尺为1～10，实际滑轨校验0.1～8，旋钮0.01～16，默认1不变。滑轨9～10拒绝并明确提示。旧值原样显示且不因读回标脏，刻度外隐藏边界拇指；无关字段应用保存保留旧值。本轮不扩展固件范围/schema。

修改前完整快照为 [78项校验备份](../../../backups/web-before-production-refactor-2026-10-05/BACKUP-README.md)。固定模板42文件完整性已核验。最早 browser-check.json、final/ 与 final-command-lock/ 保留前三轮软件检查；最终交付参考 final-delivery/。修正刻度测试脚本的可见区域定位和UF2夹具magic后才生成相应通过报告，未将失败执行计为通过。

最终截图：[未连接桌面](final-delivery/disconnected-1274.png)、[设备夹具390](final-delivery/status-390.png)、[旋钮320](final-delivery/knobs-320.png)、[UF2确认320](final-delivery/uf2-dialog-320.png)。已人工查看桌面、390/320和确认截图。

运行本轮浏览器检查：先从项目根运行 node web/server.mjs，再运行 node web/tests/refactor-2026-10-05/check.mjs <新的绝对输出目录>。脚本引用本机bundled Playwright路径；换电脑先调整检查依赖路径，不改生产运行依赖。已有browser-check.json时拒绝覆盖。
