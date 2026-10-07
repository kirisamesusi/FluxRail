# Web、测试与部署工具Git交付检查

日期：2026-10-08（北京时间）。用户要求整理当前Web及刚修改的测试等并推送到Git；分支 `test`，远端 `origin`（kirisamesusi/FluxRail）。提交前fetch确认本地HEAD与origin/test一致，无需合并。

本轮范围：

- 当前Web确认弹窗/说明、灵敏度说明移除、BT10映射提示；UF2两项控制器测试以当前Web显示分工为准，保留原功能检查。
- Web模块/README/导航/状态表及部署文档，10/06～10/08的独立Web验证记录，保留历史失败与冻结SHA。
- `.github/workflows/deploy-web.yml`、`web/deploy/`及交互脚本验证；只在test分支测试通过后发布静态站点。
- 旧部署包与SHA/输入清单，另生成包含当前BT10文案的r3包；不覆盖r2。

工作区机械/PCB/BOM成果及旋钮模块机械文档不在本次提交范围，保留本地。

| 本地检查 | 结果 / 证据 |
| --- | --- |
| `npm --prefix web test` | 73/73通过，[日志](node-tests.txt) |
| 部署Shell语法 | setup-fluxrail.sh及activate-release.sh的 `bash -n`通过 |
| Ubuntu初始化隔离检查 | 10组通过，[日志](setup-tests.txt)；Windows Git Bash模拟命令/链接，不是用户服务器实测 |
| r3部署包 | 33个普通文件，包含29个运行/许可证文件；[包](../../../outputs/FluxRail-Ubuntu-Setup-2026-10-08-r3.tar.gz)、[SHA](../../../outputs/FluxRail-Ubuntu-Setup-2026-10-08-r3.tar.gz.sha256)、[清单](../../../outputs/FluxRail-Ubuntu-Setup-2026-10-08-r3.tar.gz.manifest.json)，内容校验见[包报告](PACKAGE_CHECK.json) |
| 文档与归档 | [本轮报告](DOCUMENTATION_CHECK.json)；脚本固定旧日期不表示本轮日期 |
| 暂存范围、链接与密钥检查 | [暂存报告](STAGED_CHECK.json)；只提交指定Web范围 |

Git提交和推送完成情况通过提交对象及远端test引用确认；GitHub Actions触发后的执行/公网验证是另一层状态，不根据本地检查宣称部署成功。本轮没有固件编译、ARM、真实串口、Flash/刷机、最终硬件或游戏验收。
