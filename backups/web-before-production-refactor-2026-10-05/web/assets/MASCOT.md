# FluxRail 水色吉祥物资源

日期：2026-10-05（北京时间）。用户选定第二轮候选的「图②：淡水色漫画少女」，并要求统一 Web 标志、icon、右侧插画；右侧保留原音游控制器，替换兔子。后续按页面蓝白粉主题柔化角色配色，保持表情、造型和构图。

| 文件 | 用途 |
| --- | --- |
| `fluxrail-mascot-aqua.png` | 用户选定的原图，原字节保留 |
| `fluxrail-mascot-icon-source.png` | 同角色头像，调整颜色前的原图 |
| `fluxrail-mascot-controller-source.png` | 保留音游控制器并替换兔子，调整颜色前的原图 |
| `fluxrail-mascot-theme-source.png` | 与页面配色统一后的半身原图 |
| `fluxrail-mascot-icon-theme-source.png` | 配色统一后的头像原图 |
| `fluxrail-mascot-controller-theme-source.png` | 配色统一后的右侧插画原图 |
| `fluxrail-mascot-brand.png` | 页面左上角，256×256 导出 |
| `fluxrail-mascot-controller.png` | 页面右侧，800×503 导出 |
| `fluxrail-icon-32.png` / `fluxrail-icon-192.png` | 浏览器 PNG icon |
| `fluxrail-icon-180.png` | Apple touch icon |
| `fluxrail-favicon.ico` | 内含 16/32/48px PNG 的 ICO；服务器兼容 `/favicon.ico` |

使用内置 imagegen 生成和编辑，不使用 CLI/API 密钥。完整提示词见[生成记录](mascot-generation.json)。生成参考是用户提供的漫画图与选定角色；参考漫画的画师未确认，不把角色推测当作者署名。原兔子资源及根目录旧素材保留。所有透明背景和角色内部白色填充保留。

角色配色以网页的灰蓝文字、蓝白背景和淡粉点缀为参考：浅蓝头发，灰蓝衣领/线条，粉色星星/腮红。色值写入生成提示词作为参考，并非逐像素强制色板。装饰画不表示实际设备状态或接线。

资源导出只进行等比例缩放和 PNG/ICO 编码，不重绘图像。`node web/tools/export-mascot.mjs <已安装的sharp模块路径>` 可重新导出运行资源；离线导出工具使用 Sharp，网页启动和 `npm test` 仍不需要第三方依赖或打包。

仅运行用的七个图片路径进入服务器白名单；原图、生成记录和工具不对 HTTP 暴露。新资源校验和验证见[本轮记录](../tests/mascot-2026-10-05/README.md)。
