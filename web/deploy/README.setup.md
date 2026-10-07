# FluxRail Ubuntu交互部署包

适用 Ubuntu 24.04。请先将 `fluxrail.baka.moe` 的DNS指向服务器，并在云安全组放行80/443与实际SSH端口。

在自己的电脑上传tar.gz，服务器解压后进入包目录：

```bash
sudo bash setup-fluxrail.sh
```

脚本询问网页域名、公网SSH主机、SSH端口、GitHub仓库和证书邮箱。之后：

1. 创建独立部署账号，安装包内当前Web网页（已有网页版本保留）。
2. 建立 `/etc/nginx/sites-available/fluxrail` 并启用，保留主配置/其他站点/已有TLS。
3. 询问是否申请HTTPS，Certbot交互接受服务条款，可选择续期演练。
4. 生成/复用专用SSH密钥，保留账号原授权；从本机主机公钥生成known_hosts。
5. 选择GitHub手动配置，或者浏览器授权gh后自动写入 `web-test` 环境Secrets/Variable；可选择退出gh登录。

不启用或修改UFW，不重启机器，不安装Node。UFW和云安全组分别生效；仅添加Nginx Full规则不意味着UFW已启用。云安全组需你在云商控制台操作。

GitHub自动配置会写入/替换本仓库web-test环境中的同名部署Secrets，新环境只允许test分支；已有环境策略保留。私钥放在 `/var/lib/fluxrail-setup/fluxrail-actions`，仅root可读，不默认打印。如果选手动，按脚本给出的sudo cat命令自行读取并粘贴到GitHub Secrets，别发到聊天或代码里。

首次网站内容来自这份本地部署包快照，含生成时未提交的Web运行修改；后续自动更新来自GitHub的test分支。仓库仍需提交并推送 `.github/workflows/deploy-web.yml`、`web/deploy/` 和你要公开测试的Web改动；工作流副本在包内 `workflow/deploy-web.yml` 供核对，脚本不会自动提交代码。

中途失败/跳过可以重复执行；已有网站链接、密钥和TLS配置不会重建。已有FluxRail站点如果域名/目录不匹配会停止，不覆盖。网页包SHA不匹配会拒绝安装。

本地脚本检查和隔离模拟不替代Ubuntu服务器、签证/续期、GitHub登录/部署或公网验证。完成后请在自己的浏览器检查HTTPS/图片/导航/模拟演示，再按需测试真实Web Serial。
