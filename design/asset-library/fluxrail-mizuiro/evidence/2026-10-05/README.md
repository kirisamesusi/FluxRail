# 基础素材库保存验证

日期：2026-10-05（北京时间）；分支 `test`。本轮用户要求保存小人及设计信息作为基础素材库。

保存18个素材：6张角色/头像/键盘原图、4张早期候选、5张现用PNG和1个ICO、2张参考输入。复制时比较源文件与目标完整字节，全部一致；同时保存原始提示词、角色特征、主题配色、用户已确认偏好和Web资源对应关系。原图不依赖本机Codex/临时目录。

执行只读工具 `python design/asset-library/fluxrail-mizuiro/tools/verify_library.py`，检查[SHA清单](../../manifest.json)、素材ID唯一、PNG尺寸/签名、ICO目录及图册/说明的本地链接；实际结果见[integrity-check.json](integrity-check.json)。

Headless Edge打开本地离线图册，检查18张图片成功解码、3个推荐基准及各分类切换；1440px和390px无横向溢出，无页面脚本错误/联网请求。查看桌面与手机截图，实际结果见[gallery-check.json](gallery-check.json)、[桌面截图](gallery-desktop.png)、[手机截图](gallery-mobile.png)。

文档检查：`python docs/tools/verify_docs.py --output design/asset-library/fluxrail-mizuiro/evidence/2026-10-05/docs-check.json`，结果见[docs-check.json](docs-check.json)。检查本轮修改的差异格式，保存验证不会覆盖此前Web测试或归档证据。

本轮未运行Web功能回归、固件编译、ARM模拟、串口或任何设备/游戏操作；页面运行素材、HTML/CSS/JS、原Web生成记录与旧视觉验证保持原字节。图册浏览仅验证本地素材保存与展示。
