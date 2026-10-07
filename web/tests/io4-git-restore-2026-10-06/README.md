# 从远端 test 恢复 IO4 与重启代码

日期：2026-10-06（北京时间）。用户要求从 Git 拉取原版，不再手工修改文案。

执行 git fetch origin test，远端 origin/test 为 d260fbf，与当前 test 的 HEAD 相同。随后使用 git restore --source=origin/test --worktree，恢复 web/src/integration.js、web/src/workflows.js、web/src/io4-editor.js。

验证三份文件以及 web/src/confirmation.js、web/index.html、web/style.css 相对 origin/test 的 diff 为空；三份 JS 的 node --check 通过。保留 gain-control.js 移除三处常驻灵敏度说明的修改，未改动无关设计文件。文档链接检查见 documentation-check.json。

前几轮记录保持不变：[初次修改](../io4-copy-2026-10-06/README.md)、[中间版本 Luna 审核](../io4-copy-review-2026-10-06/README.md)、[上次手工恢复](../io4-copy-restore-2026-10-06/README.md)。这些阶段的测试与审核不能当作本次新增验证。本次未复跑完整 npm test 或 Luna，未执行浏览器视觉、设备操作、ARM 或正式固件编译。
