# Ubuntu交互部署脚本准备

日期：2026-10-08（北京时间）；分支test。用户已配置基础软件/Nginx Full规则，要求剩余配置做成交互脚本。新增服务器初始化脚本、部署包构建入口和当前网页部署包；保持UFW现状，使用独立站点，不修改网页/固件业务代码。

交互项目：网页域名、公网SSH主机/端口、仓库、开始配置、DNS/安全组就绪后签证、证书邮箱/条款、续期演练、手动GitHub设置或gh浏览器授权/写入Secrets、是否退出gh。新账号无sudo，专用密钥只在服务器生成，默认不打印私钥；root受限状态目录 `/var/lib/fluxrail-setup`。重跑保留已有网页/TLS/授权，遇到冲突停止。新Nginx配置失败撤销本次新增站点文件与链接。

## 本地验证

- `bash -n`：脚本与隔离检查脚本语法通过。
- [setup.test.sh](setup.test.sh) 的10组隔离检查通过，见[日志](setup-tests-r2.txt)：输入/注入与端口校验、真实ask/confirm变量传递、Nginx变量原样生成、先校验再reload、重跑保留TLS、冲突不覆盖、错误配置撤销新增文件/链接、初次网页/重跑保留、包内容篡改拒绝、Secrets经stdin传递且不打印私钥/不覆盖已有Environment策略。
- 检查在Windows Git Bash临时目录，使用MSYS模拟链接以及nginx/systemctl/chown/as_deploy/gh命令桩；没有运行真实apt、Certbot、服务重载、GitHub API或Linux root配置。不把这些结果当作Ubuntu真实部署证据。
- 当前完整 `web/npm test`：73项全部通过、退出码0；前序UF2测试适配见[独立证据](../uf2-current-web-2026-10-08/README.md)。
- 部署包33个普通文件（29个Web/许可证文件及脚本/说明/工作流/网页SHA清单），无symlink/密钥/测试证据/固件。包字节/输入SHA和原包不覆盖检查见[包检查](PACKAGE_CHECK.json)。
- 文档检查见[新报告](DOCUMENTATION_CHECK.json)，历史证据未覆盖。该文档校验器的固定旧日期字段不表示本轮日期。

交付：[tar.gz包](../../../outputs/FluxRail-Ubuntu-Setup-2026-10-08-r2.tar.gz)、[SHA256](../../../outputs/FluxRail-Ubuntu-Setup-2026-10-08-r2.tar.gz.sha256)、[输入清单](../../../outputs/FluxRail-Ubuntu-Setup-2026-10-08-r2.tar.gz.manifest.json)。打包网页是本地当前快照，含生成时未提交的Web运行修改；未来Actions上传的是GitHub test已提交内容。

尚未验证：用户Ubuntu真实执行、已有SSH/sudo策略/云安全组、DNS/AAAA、Certbot签发/续期、gh设备授权/Environment权限及远端Actions、真实Nginx解析/公网访问、浏览器DOM/Web Serial、ARM/固件/硬件/游戏。脚本不自动提交或推送仓库，工作流仍需在test上传后触发。

CLI调用依据：[gh secret set](https://cli.github.com/manual/gh_secret_set)、[gh variable set](https://cli.github.com/manual/gh_variable_set)、[gh auth login](https://cli.github.com/manual/gh_auth_login)、[Environment API](https://docs.github.com/en/rest/deployments/environments)、[部署分支策略](https://docs.github.com/en/rest/deployments/branch-policies)。
