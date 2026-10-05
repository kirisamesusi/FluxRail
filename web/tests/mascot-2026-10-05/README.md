# 水色吉祥物与 icon 验证

日期：2026-10-05（北京时间）。分支 `test`；固件源码仍为0.10.2。本轮仅Web视觉和静态资源修改。

## 修改与备份

用户选择图②的水色漫画少女，要求替换左上角水滴、映射到icon；随后要求右侧保留音游键盘并替换兔子，以及按网页主题优化小人配色。使用内置 imagegen 生成头像、右侧合成和主题配色版本。具体资源与完整提示词见[资源说明](../../assets/MASCOT.md)与[生成记录](../../assets/mascot-generation.json)。缩放导出保留alpha，原图不覆盖。

修改前[完整Web备份](../../../backups/web-before-mascot-2026-10-05/BACKUP-README.md)包含50个原文件，复制时逐项对比大小和SHA-256。既有44个业务源码/测试文件通过SHA再次核对，保持原字节；旧素材及旧冻结报告/截图未修改。

## 实际执行

- 在 `web/` 执行 `npm test`：59项通过、0失败，日志见[node-test-output.txt](node-test-output.txt)。属于Node软件回归，不是硬件验收。
- `node --check` 检查本轮服务器、资源导出脚本与验证脚本；Python检查文档工具语法。
- [check.mjs](check.mjs)使用本机Headless Edge，五种宽度1440/980/768/390/320px无页面横向溢出，标志文字不溢出，标志和右侧PNG成功解码，四个icon声明存在；未连接状态下切换按键映射正常，无浏览器脚本/控制台错误。报告见[browser-http-check.json](browser-http-check.json)。
- 27项HTTP检查：七个当前/兼容资源及旧兔子图片GET/HEAD成功、PNG/ICO MIME正确、CSP/nosniff保留、HEAD无body；原图、说明、导出工具及非白名单工程路径404。
- ICO目录及内嵌PNG签名核对，包含16/32/48px；人工查看[32px icon](../../assets/fluxrail-icon-32.png)、[桌面截图](layout-1440.png)、[手机截图](layout-390.png)和[左上角截图](brand.png)。透明边缘正常，字符/表情可见，键盘布局和装饰仍可辨认。
- 文档入口 `python docs/tools/verify_docs.py --output web/tests/mascot-2026-10-05/docs-check.json`，报告见[docs-check.json](docs-check.json)。新增输出参数用于保留历史归档报告。
- `git diff --check`；最终资源大小、SHA及原图对应关系见[assets-manifest.json](assets-manifest.json)。

重放视觉检查：从根目录启动 `node web/server.mjs`；执行 `node web/tests/mascot-2026-10-05/check.mjs <已安装playwright的index.mjs路径>`。脚本创建独立Headless Edge上下文，不复用用户浏览器/串口。报告使用排他写入；重放须另建证据目录，不能覆盖本轮记录。初次启动脚本时Windows绝对模块路径格式导致加载失败，改用 `pathToFileURL` 后完成上述检查；此前未启动浏览器或修改设备。

## 验证边界

未编译固件/生成UF2、未执行ARM模拟、未连接串口、未写RAM/Flash或刷写；裸板、最终4MiB硬件和游戏未测。Headless Edge布局/资源检查不证明原生串口chooser、真实设备功能或游戏兼容。未建立master发布分支，未改公共接口/schema/CDC协议。
