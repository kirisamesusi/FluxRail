#!/usr/bin/env bash
# Interactive Ubuntu 24.04 setup. No UFW changes, nginx main-config edits or reboot.
# The optional site/ directory comes from prepare-site.mjs, never a repository copy.
set -euo pipefail

DOMAIN=fluxrail.baka.moe
REPO=kirisamesusi/FluxRail
DEPLOY_USER=fluxrail-deploy
BASE=/srv/fluxrail
STATE=/var/lib/fluxrail-setup
SITE_CONFIG=/etc/nginx/sites-available/fluxrail
SITE_LINK=/etc/nginx/sites-enabled/fluxrail

say() { printf '\n%s\n' "$*"; }
die() { printf '\n错误：%s\n' "$*" >&2; exit 1; }
ask() {
    local target_name=$1 prompt_text=$2 default_value=${3:-} answer_value
    printf '%s%s：' "$prompt_text" "${default_value:+ [$default_value]}" >&3
    IFS= read -r answer_value <&3 || die '交互输入结束，请在SSH终端重新执行脚本。'
    printf -v "$target_name" '%s' "${answer_value:-$default_value}"
}
confirm() {
    local reply
    ask reply "$1 (y/N)" n
    [[ "$reply" == y || "$reply" == Y ]]
}
valid_host() { [[ "$1" =~ ^[a-zA-Z0-9][a-zA-Z0-9.-]*$ ]]; }
valid_domain() { [[ "$1" =~ ^[a-zA-Z0-9]([a-zA-Z0-9-]*[a-zA-Z0-9])?(\.[a-zA-Z0-9]([a-zA-Z0-9-]*[a-zA-Z0-9])?)+$ ]]; }
valid_port() { [[ "$1" =~ ^[0-9]{1,5}$ ]] && (( 10#$1 >= 1 && 10#$1 <= 65535 )); }
as_deploy() { runuser -u "$DEPLOY_USER" -- "$@"; }

write_nginx() {
    # Quote EOF to retain nginx's literal $uri variables.
    cat <<'NGINX'
# Managed by FluxRail interactive setup; keep Certbot's subsequent TLS changes.
server {
    listen 80;
    listen [::]:80;
NGINX
    printf '    server_name %s;\n    root %s/current;\n' "$DOMAIN" "$BASE"
    cat <<'NGINX'
    index index.html;
    autoindex off;
    charset utf-8;
    add_header Cache-Control "no-store" always;
    add_header X-Content-Type-Options "nosniff" always;
    add_header Content-Security-Policy "default-src 'self'; script-src 'self'; style-src 'self'; img-src 'self' blob:; connect-src 'self'; object-src 'none'; base-uri 'none'; frame-ancestors 'none'" always;
    add_header Permissions-Policy "serial=(self)" always;
    location = /favicon.ico { try_files /assets/fluxrail-favicon.ico =404; }
    location / {
        try_files $uri $uri/ =404;
        limit_except GET HEAD { deny all; }
    }
    location ~ /\. { deny all; }
}
NGINX
}

setup_site() {
    nginx -t || die '已有Nginx配置有错误，请先修复后重跑；没有改动站点配置。'
    [[ ! -L "$SITE_CONFIG" ]] || die "$SITE_CONFIG 是软链接，请先人工检查。"
    if [[ -e "$SITE_LINK" || -L "$SITE_LINK" ]]; then
        [[ -L "$SITE_LINK" && "$(readlink -f "$SITE_LINK")" == "$SITE_CONFIG" ]] || die "$SITE_LINK 已有不同配置，保留原文件并停止。"
    fi
    local created_config=0 created_link=0
    if [[ -e "$SITE_CONFIG" ]]; then
        # A rerun must not erase TLS, custom configuration or an unrelated site.
        grep -Fq "server_name $DOMAIN;" "$SITE_CONFIG" || die '已有FluxRail配置的域名不同；不会覆盖。'
        grep -Fq "root $BASE/current;" "$SITE_CONFIG" || die '已有FluxRail配置的网页目录不同；不会覆盖。'
        say '保留已有FluxRail站点配置（包括Certbot加入的HTTPS）。'
    else
        local pending="$STATE/nginx.pending"
        write_nginx > "$pending"
        install -m 644 "$pending" "$SITE_CONFIG"
        created_config=1
    fi
    if [[ ! -L "$SITE_LINK" ]]; then
        ln -s "$SITE_CONFIG" "$SITE_LINK"
        created_link=1
    fi
    if ! nginx -t; then
        if (( created_link )); then rm -f -- "$SITE_LINK"; fi
        if (( created_config )); then rm -f -- "$SITE_CONFIG"; fi
        die '新站点未通过nginx -t，已撤销本次新增配置，没有重载Nginx。'
    fi
    systemctl enable --now nginx
    systemctl reload nginx
}

install_initial_site() {
    if [[ -e "$BASE/current" || -L "$BASE/current" ]]; then
        [[ -L "$BASE/current" ]] || die "$BASE/current 已存在且不是软链接；不覆盖。"
        say "保留当前网页版本：$(readlink "$BASE/current")"
        return
    fi
    local release="$BASE/releases/setup-$(date -u +%Y%m%dT%H%M%SZ)-$$"
    as_deploy mkdir -m 755 "$release"
    if [[ -d "$SCRIPT_DIR/site" ]]; then
        [[ -f "$SCRIPT_DIR/site.sha256" ]] || die '部署包缺少site.sha256。'
        (cd "$SCRIPT_DIR" && sha256sum --quiet -c site.sha256) || die '网页部署包校验失败，停止安装。'
        [[ -f "$SCRIPT_DIR/site/index.html" && -f "$SCRIPT_DIR/site/src/app.js" && -f "$SCRIPT_DIR/site/tests/golden.js" ]] || die '网页运行文件不完整。'
        cp -R -- "$SCRIPT_DIR/site/." "$release/"
        chown -R "$DEPLOY_USER:$DEPLOY_USER" "$release"
        find "$release" -type d -exec chmod 755 {} +
        find "$release" -type f -exec chmod 644 {} +
        printf 'setup-%s\n' "$(date -u +%Y%m%dT%H%M%SZ)" > "$release/deploy-version.txt"
        chown "$DEPLOY_USER:$DEPLOY_USER" "$release/deploy-version.txt"
        chmod 644 "$release/deploy-version.txt"
        say '已安装部署包中的当前FluxRail网页。'
    else
        printf '%s\n' '<!doctype html><html lang="zh-CN"><meta charset="utf-8"><title>FluxRail</title><h1>FluxRail部署准备中</h1></html>' > "$release/index.html"
        chown "$DEPLOY_USER:$DEPLOY_USER" "$release/index.html"
        chmod 644 "$release/index.html"
        say '单独运行脚本未附site目录：先安装准备页面，后续由Actions发布网页。'
    fi
    as_deploy ln -s "$release" "$BASE/current"
}

setup_keys() {
    local key="$STATE/fluxrail-actions" ssh_dir="/home/$DEPLOY_USER/.ssh"
    [[ ! -L "$key" && ! -L "$key.pub" ]] || die '密钥文件不应为软链接。'
    if [[ ! -f "$key" ]]; then
        [[ ! -e "$key.pub" ]] || die '存在公钥但没有对应私钥，请先检查。'
        ssh-keygen -q -t ed25519 -N '' -C 'FluxRail GitHub Actions' -f "$key"
    fi
    # Derive the public key from the private key on every run; preserve existing auth.
    local public
    public=$(ssh-keygen -y -f "$key")
    printf '%s FluxRail GitHub Actions\n' "$public" > "$key.pub"
    chmod 600 "$key" "$key.pub"
    [[ ! -L "$ssh_dir" && ! -L "$ssh_dir/authorized_keys" ]] || die '部署账号SSH目录/授权文件是软链接，请先检查。'
    install -d -m 700 -o "$DEPLOY_USER" -g "$DEPLOY_USER" "$ssh_dir"
    touch "$ssh_dir/authorized_keys"
    if ! grep -Fq -- "$public" "$ssh_dir/authorized_keys"; then
        # Start a fresh line even if an existing key has no trailing newline.
        printf '\nrestrict %s FluxRail GitHub Actions\n' "$public" >> "$ssh_dir/authorized_keys"
    fi
    chown "$DEPLOY_USER:$DEPLOY_USER" "$ssh_dir/authorized_keys"
    chmod 600 "$ssh_dir/authorized_keys"
    [[ -f /etc/ssh/ssh_host_ed25519_key.pub ]] || die '缺少服务器Ed25519主机公钥；请检查SSH服务。'
    local known_host=$DEPLOY_HOST
    if [[ "$DEPLOY_PORT" != 22 ]]; then known_host="[$DEPLOY_HOST]:$DEPLOY_PORT"; fi
    local algorithm blob remainder
    read -r algorithm blob remainder < /etc/ssh/ssh_host_ed25519_key.pub
    printf '%s %s %s\n' "$known_host" "$algorithm" "$blob" > "$STATE/known_hosts"
    chmod 600 "$STATE/known_hosts"
    say '已生成/复用专用Actions密钥；服务器主机公钥直接从本机读取，没有关闭主机校验。'
    ssh-keygen -lf /etc/ssh/ssh_host_ed25519_key.pub
    # Single-line GitHub values must not acquire trailing newlines via stdin.
    printf '%s' "$DEPLOY_HOST" > "$STATE/DEPLOY_HOST"
    printf '%s' "$DEPLOY_PORT" > "$STATE/DEPLOY_PORT"
    printf '%s' "$DOMAIN" > "$STATE/DEPLOY_DOMAIN"
}

setup_https() {
    say "DNS查询结果（确认A记录指向公网IP，错误AAAA应先删除）："
    getent ahosts "$DOMAIN" || true
    say "请在云安全组确认TCP 80、443和SSH $DEPLOY_PORT 已放行；UFW状态如下："
    if command -v ufw >/dev/null; then ufw status; fi
    say '本脚本不启用、重启或修改UFW，也不修改云安全组。'
    if ! confirm 'DNS和云安全组已就绪，现在申请/检查HTTPS证书吗'; then
        say '已跳过HTTPS；补好DNS/端口后重跑本脚本。Actions需要HTTPS就绪才可完成发布。'
        return
    fi
    local certbot
    if [[ -x /snap/bin/certbot ]]; then
        certbot=/snap/bin/certbot
    elif command -v certbot >/dev/null; then
        certbot=$(command -v certbot)
        say "复用已有Certbot：$certbot（不会同时另装snap版本）。"
    else
        systemctl enable --now snapd.socket
        snap install --classic certbot
        certbot=/snap/bin/certbot
    fi
    ask CERT_EMAIL '证书到期/账户联系邮箱（Certbot随后询问服务条款）' ''
    [[ "$CERT_EMAIL" =~ ^[^[:space:]@]+@[^[:space:]@]+\.[^[:space:]@]+$ ]] || die '邮箱格式不正确。'
    # Preserve a pre-Certbot config for manual recovery without rolling back valid TLS.
    cp -p -- "$SITE_CONFIG" "$STATE/nginx-before-certbot-$(date -u +%Y%m%dT%H%M%SZ)-$$.conf"
    if ! "$certbot" --nginx -d "$DOMAIN" --cert-name "$DOMAIN" --redirect --email "$CERT_EMAIL" --keep-until-expiring; then
        say '证书申请未成功：先检查DNS/AAAA/安全组/80端口，修好后重跑；HTTP站点与密钥保留。'
        return
    fi
    nginx -t
    systemctl reload nginx
    if curl --fail --silent --show-error --noproxy '*' --max-time 20 --resolve "$DOMAIN:443:127.0.0.1" "https://$DOMAIN/" >/dev/null; then
        say "本机Nginx HTTPS检查通过：https://$DOMAIN"
    else
        say 'HTTPS本机检查失败，请查看Nginx/证书；尚不能确认公网可访问。'
    fi
    if confirm '现在运行证书自动续期演练吗'; then
        "$certbot" renew --dry-run || say '续期演练未通过，请检查以上日志。'
    fi
    systemctl list-timers --all '*certbot*' --no-pager
}

manual_github() {
    say "GitHub手动设置入口：https://github.com/$REPO/settings/environments"
    say '创建web-test环境，Secrets/Variables的值从以下服务器文件读取：'
    printf '  Secret DEPLOY_HOST        sudo cat %s/DEPLOY_HOST\n' "$STATE"
    printf '  Secret DEPLOY_PORT        sudo cat %s/DEPLOY_PORT\n' "$STATE"
    printf '  Secret DEPLOY_KNOWN_HOSTS sudo cat %s/known_hosts\n' "$STATE"
    printf '  Secret DEPLOY_SSH_KEY     sudo cat %s/fluxrail-actions\n' "$STATE"
    printf '  Variable DEPLOY_DOMAIN   sudo cat %s/DEPLOY_DOMAIN\n' "$STATE"
    say '私钥仅粘贴到GitHub Secrets，不要发到聊天/仓库/日志。已有同名Secret会被新值替换。'
}

setup_github() {
    manual_github
    if ! confirm '改用GitHub CLI浏览器授权，并自动写入上述部署Secrets/Variable吗'; then return; fi
    if ! command -v gh >/dev/null; then apt-get install -y gh; fi
    if ! gh auth status --hostname github.com >/dev/null 2>&1; then
        say '接下来按gh提示，在你自己的电脑浏览器打开GitHub网址，输入一次性验证码授权。登录信息保存在当前root账号的gh配置中；结束后可选择退出。'
        gh auth login --hostname github.com --git-protocol https --web || { say 'GitHub授权未完成；可以按上面的路径手动填写。'; return; }
    fi
    gh api "repos/$REPO" --jq '.full_name' || { say '没有仓库访问权限，改为手动设置。'; return; }
    local environment="repos/$REPO/environments/web-test"
    if ! gh api "$environment" > "$STATE/github-environment.json" 2>/dev/null; then
        printf '%s\n' '{"deployment_branch_policy":{"protected_branches":false,"custom_branch_policies":true}}' > "$STATE/github-environment-request.json"
        if ! gh api --method PUT "$environment" --input "$STATE/github-environment-request.json" > "$STATE/github-environment.json"; then
            say '无法创建web-test环境；检查仓库权限/方案。请按部署指南的手动兼容方案处理。'
            return
        fi
        # If interrupted here, a rerun still ensures a test-only branch policy below.
    fi
    local custom names
    custom=$(gh api "$environment" --jq '.deployment_branch_policy.custom_branch_policies // false')
    if [[ "$custom" == true ]]; then
        names=$(gh api "$environment/deployment-branch-policies" --jq '.branch_policies[] | select(.type == "branch" or .type == null) | .name')
        if ! grep -Fxq test <<< "$names"; then
            printf '%s\n' '{"name":"test","type":"branch"}' > "$STATE/github-branch-request.json"
            gh api --method POST "$environment/deployment-branch-policies" --input "$STATE/github-branch-request.json" >/dev/null || { say 'test部署分支设置失败，请手动检查Environment策略。'; return; }
        fi
    fi
    say '保留已有Environment审核/分支策略；工作流自身仍只允许test。已有审核规则可能需要每次人工审核。'
    gh secret set DEPLOY_HOST --repo "$REPO" --env web-test < "$STATE/DEPLOY_HOST"
    gh secret set DEPLOY_PORT --repo "$REPO" --env web-test < "$STATE/DEPLOY_PORT"
    gh secret set DEPLOY_SSH_KEY --repo "$REPO" --env web-test < "$STATE/fluxrail-actions"
    gh secret set DEPLOY_KNOWN_HOSTS --repo "$REPO" --env web-test < "$STATE/known_hosts"
    gh variable set DEPLOY_DOMAIN --repo "$REPO" --env web-test --body "$DOMAIN"
    say 'GitHub部署参数已写入web-test。'
    if ! gh api "repos/$REPO/contents/.github/workflows/deploy-web.yml?ref=test" >/dev/null 2>&1; then
        say '远端test尚未检测到部署工作流；请在本地提交并推送准备好的部署文件。'
    fi
    if confirm '现在退出本服务器的GitHub CLI登录（不会删除已写入Secrets）吗'; then
        gh auth logout --hostname github.com
    fi
}

main() {
    [[ "$EUID" == 0 ]] || die '请使用 sudo bash setup-fluxrail.sh 执行。'
    [[ -t 0 ]] || die '请先下载/解压脚本，再在SSH交互终端执行；不要curl管道运行。'
    exec 3<>/dev/tty
    local os_id os_version
    os_id=$(. /etc/os-release; printf '%s' "$ID")
    os_version=$(. /etc/os-release; printf '%s' "$VERSION_ID")
    [[ "$os_id" == ubuntu && "$os_version" == 24.04 ]] || die '本脚本按Ubuntu 24.04设计，请核对系统版本。'
    SCRIPT_DIR=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
    say 'FluxRail交互式部署：独立Nginx站点、HTTPS、专用SSH账号和GitHub Actions参数。不会重启服务器或改变UFW。'
    ask DOMAIN '网页域名' "$DOMAIN"
    valid_domain "$DOMAIN" || die '请输入完整域名，不含https://、路径或端口。'
    ask DEPLOY_HOST '供Actions连接的服务器公网IPv4或SSH主机域名（不要填内网IP）' ''
    valid_host "$DEPLOY_HOST" || die '主机格式不正确；暂不支持裸IPv6。'
    local port_default=22
    if [[ -n "${SSH_CONNECTION:-}" ]]; then port_default=${SSH_CONNECTION##* }; fi
    ask DEPLOY_PORT '公网SSH端口（默认参考当前连接；云端映射不同请手动修改）' "$port_default"
    valid_port "$DEPLOY_PORT" || die '端口必须是1～65535。'
    DEPLOY_PORT=$((10#$DEPLOY_PORT))
    ask REPO 'GitHub仓库（owner/repo）' "$REPO"
    [[ "$REPO" =~ ^[a-zA-Z0-9_.-]+/[a-zA-Z0-9_.-]+$ ]] || die '仓库必须填写owner/repo。'
    confirm '信息已核对，开始安装/配置上述站点吗' || exit 0
    umask 077
    apt-get update
    apt-get install -y nginx rsync curl ca-certificates snapd openssh-client util-linux
    if ! id "$DEPLOY_USER" >/dev/null 2>&1; then adduser --disabled-password --gecos '' "$DEPLOY_USER"; fi
    [[ "$(id -u "$DEPLOY_USER")" != 0 ]] || die '部署账号不得是root。'
    [[ "$(getent passwd "$DEPLOY_USER" | cut -d: -f6)" == "/home/$DEPLOY_USER" ]] || die '已有部署账号home路径不同，停止避免误改。'
    [[ ! -L "$STATE" && ! -L "$BASE" && ! -L "$BASE/releases" ]] || die '管理目录是软链接，请先检查。'
    install -d -m 700 -o root -g root "$STATE"
    install -d -m 755 -o "$DEPLOY_USER" -g "$DEPLOY_USER" "$BASE" "$BASE/releases"
    install_initial_site
    setup_site
    setup_keys
    setup_https
    setup_github
    say "配置流程结束。浏览器检查：https://$DOMAIN"
    say '首次网页来自部署包快照。后续自动更新需要：本地test分支提交并推送部署工作流/脚本及要展示的Web改动。脚本不会替你提交或推送代码。'
    say "部署密钥/参数保存于$STATE，仅root可读；重复运行会复用密钥、保留现有网页和TLS配置。"
    say '若跳过证书或GitHub授权，相应步骤仍未完成，可准备好后重跑。云安全组、公网访问和真实Web Serial需人工检查。'
}

if [[ "${BASH_SOURCE[0]}" == "$0" ]]; then
    trap 'printf "\n脚本在第%s行停止。已完成步骤保留，可修复后重跑；没有改动UFW。\n" "$LINENO" >&2' ERR
    main "$@"
fi
