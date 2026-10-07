#!/usr/bin/env bash
# Isolated filesystem/command mocks: never runs main, apt, Certbot, gh or real services.
set -euo pipefail
fixture=${1:?Pass a new temporary test directory}
fixture=$(cd -- "$fixture" && pwd)
test_dir=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
source "$test_dir/../../deploy/setup-fluxrail.sh"
checks=0
pass() { checks=$((checks + 1)); printf 'PASS %s\n' "$1"; }

valid_domain fluxrail.baka.moe
! valid_domain 'https://fluxrail.baka.moe'
! valid_domain 'evil.example;touch /tmp/x'
! valid_domain '-fluxrail.baka.moe'
valid_host 203.0.113.5
! valid_host 'host;command'
valid_port 22
valid_port 00022
valid_port 65535
! valid_port 0
! valid_port 65536
! valid_port '22;command'
pass 'input validation accepts valid domains/ports and rejects URL/injection/range errors'

# Simulate terminal input without a Windows PTY, while exercising real ask/confirm.
exec 3>"$fixture/prompts.txt"
read() { printf -v answer_value '%s' y; }
confirm 'test confirmation'
unset -f read
pass 'ask writes into caller variable and confirm recognizes yes'

STATE="$fixture/state"
BASE="$fixture/site-root"
SITE_CONFIG="$fixture/sites-available/fluxrail"
SITE_LINK="$fixture/sites-enabled/fluxrail"
mkdir -p "$STATE" "$BASE/releases" "$fixture/sites-available" "$fixture/sites-enabled"
write_nginx > "$fixture/rendered.conf"
grep -Fq 'server_name fluxrail.baka.moe;' "$fixture/rendered.conf"
grep -Fq 'try_files $uri $uri/ =404;' "$fixture/rendered.conf"
grep -Fq 'serial=(self)' "$fixture/rendered.conf"
pass 'nginx template retains literal variables, domain and Web Serial policy'

NGINX_CALLS=0
FAIL_AT=0
nginx() { NGINX_CALLS=$((NGINX_CALLS + 1)); [[ "$NGINX_CALLS" != "$FAIL_AT" ]]; }
systemctl() { printf '%s\n' "$*" >> "$fixture/systemctl.txt"; }
setup_site
test -L "$SITE_LINK"
grep -Fxq 'reload nginx' "$fixture/systemctl.txt"
pass 'new independent site validates before service reload'
printf '\n# ssl_certificate marker must survive\n' >> "$SITE_CONFIG"
cp "$SITE_CONFIG" "$fixture/with-tls.conf"
setup_site
cmp "$SITE_CONFIG" "$fixture/with-tls.conf"
pass 'rerun preserves existing TLS/custom site configuration byte for byte'

printf 'unrelated site\n' > "$SITE_CONFIG"
if (setup_site) > "$fixture/conflict.txt" 2>&1; then exit 1; fi
grep -Fxq 'unrelated site' "$SITE_CONFIG"
pass 'conflicting existing site is refused without overwrite'
rm -f "$SITE_LINK" "$SITE_CONFIG" "$fixture/systemctl.txt"
NGINX_CALLS=0
FAIL_AT=2
if (setup_site) > "$fixture/invalid-config.txt" 2>&1; then exit 1; fi
test ! -e "$SITE_CONFIG"
test ! -L "$SITE_LINK"
test ! -e "$fixture/systemctl.txt"
pass 'nginx validation failure removes only newly created site and never reloads'

as_deploy() { "$@"; }
chown() { :; }
SCRIPT_DIR="$fixture/package"
mkdir -p "$SCRIPT_DIR/site/src" "$SCRIPT_DIR/site/tests"
printf '<!doctype html>test\n' > "$SCRIPT_DIR/site/index.html"
printf '// test JS\n' > "$SCRIPT_DIR/site/src/app.js"
printf '// fixture\n' > "$SCRIPT_DIR/site/tests/golden.js"
(cd "$SCRIPT_DIR" && find site -type f -print0 | sort -z | xargs -0 sha256sum > site.sha256)
install_initial_site
test -L "$BASE/current"
cmp "$SCRIPT_DIR/site/index.html" "$BASE/current/index.html"
previous=$(readlink "$BASE/current")
printf 'new payload\n' > "$SCRIPT_DIR/site/index.html"
install_initial_site
test "$(readlink "$BASE/current")" == "$previous"
test "$(cat "$BASE/current/index.html")" == '<!doctype html>test'
pass 'initial payload installs; rerun retains live version even if bundle changes'
rm -f "$BASE/current"
if (install_initial_site) > "$fixture/tamper.txt" 2>&1; then exit 1; fi
test ! -L "$BASE/current"
pass 'tampered payload fails checksum before publishing a current link'

printf 'TEST_PRIVATE_KEY_NEVER_PRINT\n' > "$STATE/fluxrail-actions"
printf '203.0.113.5' > "$STATE/DEPLOY_HOST"
printf '22' > "$STATE/DEPLOY_PORT"
printf 'fluxrail.baka.moe' > "$STATE/DEPLOY_DOMAIN"
printf 'test host key\n' > "$STATE/known_hosts"
confirm() { [[ "$1" == '改用GitHub CLI浏览器授权，并自动写入上述部署Secrets/Variable吗' ]]; }
gh() {
    printf '%s\n' "$*" >> "$fixture/gh-commands.txt"
    case "$1 $2" in
        'auth status') return 0 ;;
        'secret set') cat > "$fixture/secret-$3"; return 0 ;;
        'variable set') return 0 ;;
    esac
    if [[ "$1" == api ]]; then
        case "$2" in
            *deployment-branch-policies) printf 'test\n'; return 0 ;;
            *environments/web-test)
                if [[ "${3:-}" == --jq ]]; then printf 'true\n'; else printf '{}\n'; fi
                return 0 ;;
            *contents/*) return 0 ;;
            *) printf 'kirisamesusi/FluxRail\n'; return 0 ;;
        esac
    fi
    return 1
}
setup_github > "$fixture/github-output.txt"
test "$(find "$fixture" -maxdepth 1 -name 'secret-*' -type f | wc -l)" -eq 4
cmp "$STATE/fluxrail-actions" "$fixture/secret-DEPLOY_SSH_KEY"
! grep -Fq TEST_PRIVATE_KEY_NEVER_PRINT "$fixture/github-output.txt"
! grep -Fq TEST_PRIVATE_KEY_NEVER_PRINT "$fixture/gh-commands.txt"
! grep -Fq 'method PUT' "$fixture/gh-commands.txt"
grep -Fq 'variable set DEPLOY_DOMAIN --repo kirisamesusi/FluxRail --env web-test --body fluxrail.baka.moe' "$fixture/gh-commands.txt"
pass 'GitHub mocked secrets use stdin/environment; no key logging or existing environment replacement'

printf 'Completed %s isolated checks\n' "$checks"
