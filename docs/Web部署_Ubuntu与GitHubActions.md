# Web 部署：Ubuntu 24.04、Nginx、HTTPS、GitHub Actions

日期：2026-10-07（北京时间）。目标域名：`fluxrail.baka.moe`，仓库开发分支：`test`。这是朋友预览/测试站，不表示固件或最终硬件正式验收。

## 2026-10-08：从第3步之后使用交互部署包

用户已安装基础环境并添加Nginx Full规则，希望余下步骤交互完成。当前交付 [r3部署包](../outputs/FluxRail-Ubuntu-Setup-2026-10-08-r3.tar.gz) 含最新Web运行文件（包括BT10映射提示）、[交互脚本](../web/deploy/setup-fluxrail.sh)、工作流副本及网页SHA校验；不含密钥或登录信息。[包内说明](../web/deploy/README.setup.md)，脚本本地验证见[准备记录](../web/tests/deployment-script-2026-10-08/README.md)，r3包与本轮提交检查见[Git交付记录](../web/tests/git-delivery-2026-10-08/README.md)。原r2及其证据保留。

本地仓库根目录 PowerShell（将登录名/IP换成自己的；SSH非22端口时scp增加 `-P 实际端口`）：

```powershell
scp .\outputs\FluxRail-Ubuntu-Setup-2026-10-08-r3.tar.gz ubuntu@SERVER_IP:~/
```

然后在服务器自己的SSH交互终端执行：

```bash
tar -xzf ~/FluxRail-Ubuntu-Setup-2026-10-08-r3.tar.gz -C ~/
cd ~/fluxrail-setup
sudo bash setup-fluxrail.sh
```

脚本询问域名、公网SSH主机/端口、仓库与证书邮箱，完成独立Nginx站点、初次网页、部署账号/密钥与HTTPS。DNS和云安全组就绪后在脚本选择申请证书，Certbot交互接受条款。GitHub可选手动设置，或浏览器授权gh后自动写入web-test环境四项Secrets及域名Variable，可选择退出服务器gh登录；自动设置会替换该环境的同名部署参数，保留已有审核策略。状态/私钥保存于 `/var/lib/fluxrail-setup`，仅root可读，不默认打印私钥。

脚本不启用、重启或修改UFW，不重启服务器，不安装Node，也不修改Nginx主文件或其他站点。UFW未启用时添加规则暂不生效；云商安全组独立生效，请在云控制台放行80/443和实际SSH端口。无需为了添加规则重启机器。

初次网页来自部署包生成时的本地快照（包括当前未提交Web运行修改）；未来Actions仅发布GitHub test分支已提交内容。工作流仍需从本地提交并推送，脚本不会自动提交/推送。中途跳过/失败可重跑，已有网页、密钥、TLS配置保留；域名/目录冲突则停止。单独复制脚本而没有site目录时只安装准备页面。

重新打包当前网页时，从仓库根目录运行 `python web/deploy/package-setup.py outputs/新的文件名.tar.gz`；已有输出拒绝覆盖，同日多次交付用不同文件名。本次交互脚本仅本地语法/隔离模拟验证，Ubuntu安装、GitHub登录和证书真实执行仍待用户服务器操作。

## 方案与已准备的文件

当前 Web 是原生 ES 模块，无服务端 API、数据库或 npm 依赖。服务器由 Nginx 直接提供静态文件；Node.js 24 用于 GitHub Actions 测试，服务器可选安装。不需要 PM2、Node 常驻进程或开放4174端口。开发仍用根目录 `node web/server.mjs`。

```text
push test（web 更新）
  → GitHub Actions：Node 24 / npm test
  → 导出浏览器所需文件
  → SSH / rsync 上传独立版本目录
  → 切换 /srv/fluxrail/current
  → HTTPS 核对版本，失败恢复旧链接

朋友浏览器 → https://fluxrail.baka.moe → Nginx → 网页
朋友浏览器 → Web Serial → 朋友电脑上的 USB 设备
```

设备配置、导入文件和 UF2 检查在朋友的浏览器中进行；服务器不代理 USB、不保存设备配置。真实连接需要支持 Web Serial 的桌面 Chrome/Edge 与 HTTPS；没有设备也能使用模拟演示。手机可以看页面，但不要把手机展示等同于真实串口支持。

| 文件 | 用途 |
| --- | --- |
| [nginx.conf](../web/deploy/nginx.conf) | `fluxrail.baka.moe` 的 HTTP 初始配置；Certbot 后续加入 TLS |
| [prepare-site.mjs](../web/deploy/prepare-site.mjs) | 导出 HTML/CSS、src JS、运行图片、许可证，以及模拟必需的 `tests/golden.js` |
| [activate-release.sh](../web/deploy/activate-release.sh) | 切换版本并检查本机 Nginx HTTPS；失败回滚 |
| [deploy-web.yml](../.github/workflows/deploy-web.yml) | 跟随 `test` 分支 Web 更新；测试通过才部署 |

没有上传仓库、固件、测试报告、设计原图、凭据和备份；唯一公开的测试目录文件是模拟器依赖的 `tests/golden.js`。输出目录必须不存在，脚本不会清空旧目录。部署目录保留历史版本，不自动删除。

## 1. DNS 与云服务器端口

在 `baka.moe` 的 DNS 控制台新增：

| 类型 | 主机记录 | 值 |
| --- | --- | --- |
| A | fluxrail | 服务器公网 IPv4 |

只有服务器确实配置了公网 IPv6 并可访问时才设置 AAAA；错误 AAAA 会导致部分访问/签证失败。若使用 Cloudflare，首次配置可先用“仅 DNS”，排除代理干扰；以后开启代理时选择 Full (strict)。

云安全组放行 TCP 80、443 和实际 SSH 端口（默认22）。80用于 HTTP 跳转和证书验证/续期；不用开放4174。GitHub 托管 runner 必须能连到 SSH 端口；如果 SSH 只允许你家 IP，Actions 会超时，不能简单假设 runner 的 IP 固定。

本地 PowerShell 可检查：

```powershell
Resolve-DnsName fluxrail.baka.moe -Type A
```

## 2. 服务器安装基础软件

下面 Bash 命令在 Ubuntu 的管理员账号执行（账号需有 sudo；已是 root 也可以使用这些命令）。将 `SERVER_IP` 替换为真实公网 IP 后，从本地登录：

```powershell
ssh ubuntu@SERVER_IP
```

若云商登录用户是 root 或其他名字，替换 `ubuntu`。在服务器上：

```bash
sudo apt update
sudo apt upgrade -y
sudo apt install -y nginx rsync curl ca-certificates ufw snapd openssh-server
sudo systemctl enable --now nginx
sudo systemctl enable --now ssh
sudo nginx -t
```

确认自己的 SSH 实际端口已放行，再启用防火墙；保留当前连接并用第二个终端验证新连接：

```bash
sudo ufw allow 22/tcp
sudo ufw allow 'Nginx Full'
sudo ufw enable
sudo ufw status
```

如果 SSH 使用2222等其他端口，将第一条的22换成真实端口。云安全组与 UFW 都需要放行。系统升级如提示需重启，可先重启、重新连接后再继续。

## 3. Node.js（服务器可选）

静态站服务不依赖 Node，GitHub Actions 已自动安装 Node 24。若还想在服务器手动跑测试，可用普通管理员账号安装 nvm 与 Node 24，不要用 `sudo npm`：

```bash
curl -fsSL https://raw.githubusercontent.com/nvm-sh/nvm/v0.40.8/install.sh -o /tmp/fluxrail-nvm-install.sh
bash /tmp/fluxrail-nvm-install.sh
source ~/.bashrc
nvm install 24
nvm alias default 24
node --version
npm --version
```

nvm 是当前用户的环境，不会给其他用户自动安装 Node。Web 没有依赖，不需要 `npm install` 或 `npm ci`；需要测试时进入仓库 `web/` 执行 `npm test`。Node 24 在本次查阅时为 LTS；不要用已 EOL 的 Node 20 来配置新服务器。

## 4. 专用部署用户与初始网页

在服务器上建立无 sudo 权限的部署账号，仅负责站点文件：

```bash
sudo adduser --disabled-password --gecos '' fluxrail-deploy
sudo install -d -m 755 -o fluxrail-deploy -g fluxrail-deploy /srv/fluxrail /srv/fluxrail/releases
sudo -u fluxrail-deploy mkdir -m 755 /srv/fluxrail/releases/bootstrap
printf '%s\n' '<!doctype html><html lang="zh-CN"><meta charset="utf-8"><title>FluxRail</title><h1>FluxRail 部署准备中</h1></html>' | sudo -u fluxrail-deploy tee /srv/fluxrail/releases/bootstrap/index.html >/dev/null
sudo -u fluxrail-deploy ln -s /srv/fluxrail/releases/bootstrap /srv/fluxrail/current
```

这些是新服务器的首次初始化命令；已存在目录/用户时先检查现状，不重复执行初始化链接。Nginx 的 www-data 用户通过755目录/644文件读取站点，不能写入。

## 5. Nginx 配置

按用户要求，每个网站独立一个配置文件，不把 FluxRail 的域名或 server 块写进 `/etc/nginx/nginx.conf`。仓库里的 `web/deploy/nginx.conf` 是单站点模板，不是 Nginx 主配置；上传后命名为 `sites-available/fluxrail`。目录结构如下：

```text
/etc/nginx/nginx.conf                   # Ubuntu通用主配置，保持现有include
/etc/nginx/sites-available/fluxrail     # 只负责fluxrail.baka.moe
/etc/nginx/sites-enabled/fluxrail       # 链接到上面的站点配置
/srv/fluxrail/current                   # 只存FluxRail网页

# 以后新增其他站点：
/etc/nginx/sites-available/another-site
/etc/nginx/sites-enabled/another-site
/srv/another-site/current
```

以后添加网站，新建对应 `sites-available/<站点名>`，填写它自己的 server_name/root，链接到 sites-enabled，检查并reload，再为新域名单独运行 Certbot。各站共用80/443端口，由域名和TLS SNI区分。不要复制 FluxRail 的 root 或部署目录去给其他站点共用。

在本地仓库根目录 PowerShell，将配置传到服务器（管理员用户名同前）：

```powershell
scp .\web\deploy\nginx.conf ubuntu@SERVER_IP:/tmp/fluxrail-nginx.conf
```

在服务器上安装配置、启用站点：

```bash
sudo install -m 644 /tmp/fluxrail-nginx.conf /etc/nginx/sites-available/fluxrail
sudo ln -s /etc/nginx/sites-available/fluxrail /etc/nginx/sites-enabled/fluxrail
sudo nginx -t
sudo systemctl reload nginx
curl -I -H 'Host: fluxrail.baka.moe' http://127.0.0.1/
```

模板域名已经是 `fluxrail.baka.moe`。Ubuntu 默认站点可暂时保留；正确 Host 会匹配 FluxRail 站点。确认外部浏览器能打开 `http://fluxrail.baka.moe` 并看到准备页面，再申请证书。默认 nginx.conf 应包含 `/etc/nginx/mime.types`；否则 JS 的 MIME 错误会阻止模块加载。

## 6. Certbot 与 HTTPS

新服务器使用 Certbot snap 和 Nginx 插件：

```bash
sudo snap install --classic certbot
sudo /snap/bin/certbot --nginx -d fluxrail.baka.moe --redirect
sudo nginx -t
sudo /snap/bin/certbot renew --dry-run
systemctl list-timers --all 'snap.certbot.*'
```

按提示提供你的联系邮箱、接受服务条款。Certbot 会配置443、证书路径及 HTTP→HTTPS 跳转，snap 提供自动续期任务。不再额外安装 apt 版 Certbot，不手工复制证书到仓库。若提示 snapd 服务未准备好，检查 `sudo systemctl status snapd.socket` 后再安装。

在本地验证：

```powershell
curl.exe -I https://fluxrail.baka.moe
curl.exe -I http://fluxrail.baka.moe
```

HTTPS 应有有效证书，HTTP 应跳转。首次签发后不要用仓库中的 HTTP 模板直接覆盖现有配置，否则会抹掉 Certbot 加入的 TLS；修改时保留已生成的 TLS 配置，再 `nginx -t` / reload。

## 7. GitHub Actions 的 SSH 密钥

这是一把专门用于 GitHub→服务器的密钥，不是服务器访问 GitHub 的 deploy key；不要复用个人登录密钥。服务器无需克隆仓库，私有仓库也由 Actions 负责 checkout。

本地 PowerShell 生成密钥，放在仓库之外：

```powershell
ssh-keygen -t ed25519 -f "$env:USERPROFILE\.ssh\fluxrail-actions" -C "FluxRail GitHub Actions"
scp "$env:USERPROFILE\.ssh\fluxrail-actions.pub" ubuntu@SERVER_IP:/tmp/fluxrail-actions.pub
```

ssh-keygen 提示 passphrase 时按两次 Enter，使用空口令供无人值守部署；这把密钥仅授权专用部署账号。在服务器安装公钥：

```bash
sudo install -d -m 700 -o fluxrail-deploy -g fluxrail-deploy /home/fluxrail-deploy/.ssh
{ printf 'restrict '; cat /tmp/fluxrail-actions.pub; } | sudo tee /home/fluxrail-deploy/.ssh/authorized_keys >/dev/null
sudo chown fluxrail-deploy:fluxrail-deploy /home/fluxrail-deploy/.ssh/authorized_keys
sudo chmod 600 /home/fluxrail-deploy/.ssh/authorized_keys
```

`restrict` 禁用端口转发、PTY等功能，仍允许 rsync 与部署命令。新账号这一步建立唯一公钥；若账号已有授权，不要覆盖原文件，应检查后追加。该账号可管理整个站点文件，不能管理系统服务。

从本地验证登录：

```powershell
ssh -i "$env:USERPROFILE\.ssh\fluxrail-actions" fluxrail-deploy@SERVER_IP "id"
```

第一次登录要校验服务器主机密钥。服务器控制台执行：

```bash
sudo ssh-keygen -lf /etc/ssh/ssh_host_ed25519_key.pub
```

本地扫描并比对指纹：

```powershell
ssh-keyscan -t ed25519 SERVER_IP | Set-Content -Encoding ascii "$env:USERPROFILE\.ssh\fluxrail-known-hosts"
ssh-keygen -lf "$env:USERPROFILE\.ssh\fluxrail-known-hosts"
```

两边 SHA256 指纹必须一致；扫描本身不证明主机身份，不能跳过比对。非22端口时扫描用 `ssh-keyscan -p 2222 -t ed25519 SERVER_IP`，ssh用 `-p 2222`，scp用 `-P 2222`。工作流启用严格主机校验，不使用 `StrictHostKeyChecking=no`。

## 8. GitHub 设置与第一次自动发布

仓库 `kirisamesusi/FluxRail` → Settings → Environments → 建立 **web-test**。如果当前仓库方案/权限不能使用 Environment，则使用仓库级 Secrets/Variables，并从工作流删除 `environment: web-test`；仍须保持只允许 test 分支部署。

在该 Environment 的 Secrets 加入：

| 名称 | 内容 |
| --- | --- |
| DEPLOY_HOST | 服务器公网 IPv4或 SSH 主机域名；不要加 `https://`（当前工作流未支持裸 IPv6） |
| DEPLOY_PORT | 22或你的实际 SSH 端口；留空默认22 |
| DEPLOY_SSH_KEY | 本地 `fluxrail-actions` 私钥文件完整内容，含 BEGIN/END 行；不含 `.pub` |
| DEPLOY_KNOWN_HOSTS | 本地 `fluxrail-known-hosts` 文件完整内容，已经比对过指纹 |

在该 Environment 的 Variables 加入：

| 名称 | 内容 |
| --- | --- |
| DEPLOY_DOMAIN | fluxrail.baka.moe |

私钥直接粘贴到 GitHub Secrets 页面，不放到代码、Issues、日志或聊天里。若设置 Environment 的 Deployment branches，选择仅 `test`。如果希望完全自动部署，不启用每次部署人工审核。

本次新增工作流、部署脚本和文档尚未自动提交或推送。将部署文件随你确认要公开测试的 Web 改动一起提交到 `test` 并 push，GitHub 的 Actions 页面就会运行 **Deploy FluxRail Web**。工作流监听 `web/**`、LICENSE和工作流文件变化，纯固件更新不重新部署页面。

Actions 流程：Node 24 → 九套 `npm test` → 导出运行文件 → 上传 `/srv/fluxrail/releases/<SHA>-<run>-<attempt>/` → 原子切换 `current` → 通过本机443验证部署版本。全程不需要 root、sudo、Nginx重启或服务器 Node。没有依赖锁文件，所以没有 `npm ci` 步骤。

10/07部署准备检查曾有两项UF2提示断言失败（73项中71项通过），见[当日本地验证](../web/tests/deployment-2026-10-07/README.md)。2026-10-08用户要求测试适配当前Web显示，保留功能检查并调整提示节点后，专项8/8、完整73/73通过，见[新记录](../web/tests/uf2-current-web-2026-10-08/README.md)。`npm test`部署门槛继续保留；这次没有修改页面、执行服务器/Actions或公网发布，软件通过不证明旧固定说明目标或设备验收。

`workflow_dispatch` 已保留，只允许选择 `test`。GitHub 的手动 Run workflow 通常要求工作流也存在于默认分支；本仓库 main 保留历史，不必为了出现该按钮修改历史分支。首次使用依靠向 `test` 推送触发即可；后续失败可在 Actions 页面 Re-run jobs。

只有推到 GitHub 的已提交 Web 内容会部署；本地未提交的改动不会随工作流上传。

## 9. 检查、回滚与维护

发布后检查：

```powershell
curl.exe https://fluxrail.baka.moe/deploy-version.txt
curl.exe -I https://fluxrail.baka.moe/src/app.js
curl.exe -I https://fluxrail.baka.moe/tests/golden.js
curl.exe -I https://fluxrail.baka.moe/server.mjs
```

版本应匹配 Actions 的 SHA/run/attempt；app.js 应有 JS MIME，golden应200，server.mjs应404。用朋友电脑的 Chrome/Edge 打开，检查导航、图片、模拟演示和控制台；有真实设备时再人工授权 Web Serial。主页能打开或软件测试通过都不能当作设备、最终硬件或游戏验收。

如果故障，服务器上检查：

```bash
sudo nginx -t
sudo systemctl status nginx
sudo tail -n 80 /var/log/nginx/error.log
readlink /srv/fluxrail/current
ls -lt /srv/fluxrail/releases
```

403通常是目录/文件读取权限；502通常来自误用了反向代理配置，本方案不需要 proxy_pass；JS MIME 错误检查 mime.types；连接按钮检查 HTTPS、浏览器支持、用户授权及USB身份。IO4没有CDC时的BT10恢复流程仍按现有 Web 模块执行。

HTTPS 版本检查失败会自动恢复之前的链接并让 Actions 失败；如果是发布后的人工功能检查失败，可回滚。以 `fluxrail-deploy` 身份登录，确认 `OLD_RELEASE_ID` 是列表里已有的良好版本后执行（同样保持部署锁）：

```bash
old_release=OLD_RELEASE_ID
[[ "$old_release" =~ ^[0-9a-f]{40}-[0-9]+-[0-9]+$ ]] || exit 1
test -f "/srv/fluxrail/releases/$old_release/index.html" || exit 1
exec 9>/srv/fluxrail/.deploy.lock
flock -x 9
ln -s "/srv/fluxrail/releases/$old_release" /srv/fluxrail/.manual-current
mv -Tf /srv/fluxrail/.manual-current /srv/fluxrail/current
flock -u 9
```

网页资源采用 no-store，朋友刷新后加载新版本；已打开的页面不会在操作设备中途自动刷新。URL没有资源哈希，因此切换的瞬间若有人正在加载多个文件，仍可能跨版本；朋友测试站可刷新解决，持续高并发发布应另做版本化资源路径。

历史版本占用磁盘，定期检查 `du -sh /srv/fluxrail/releases`，保留当前与最近几版，仅删除已经核对不在使用的旧版本。部署程序不自动清理。证书保持80/443可达，定期观察续期任务；系统软件和可选 Node LTS按正常维护更新。

## 官方参考（本次已查阅）

- [Node.js 发布状态](https://nodejs.org/en/about/previous-releases)：选择支持期内 LTS。
- [nvm 安装文档](https://github.com/nvm-sh/nvm)：普通用户安装Node。
- [Ubuntu TLS/Certbot](https://ubuntu.com/server/docs/how-to/security/obtain-tls-certificates/)：Nginx插件、snap与续期。
- [Let's Encrypt HTTP-01](https://letsencrypt.org/docs/challenge-types/)：域名验证需要80端口。
- [Chrome Web Serial](https://developer.chrome.com/docs/capabilities/serial)：安全上下文和浏览器设备授权。
- [GitHub Actions 安全](https://docs.github.com/en/actions/reference/security/secure-use)：密钥、最小权限与固定action提交。
- [GitHub Environments](https://docs.github.com/en/actions/how-tos/deploy/configure-and-manage-deployments/manage-environments)：部署Secrets/Variables与分支约束。

本次仅本地准备配置和软件检查，未登录用户服务器、修改DNS/安全组、申请证书、设置GitHub Secrets或实际公网部署。
