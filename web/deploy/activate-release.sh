#!/usr/bin/env bash
# Run as fluxrail-deploy; no sudo or nginx reload needed for a static release.
set -euo pipefail
release_id=${1:?release id required}
domain=${2:?domain required}
[[ "$release_id" =~ ^[0-9a-f]{40}-[0-9]+-[0-9]+$ ]] || { echo 'Invalid release id' >&2; exit 1; }
[[ "$domain" =~ ^[a-zA-Z0-9][a-zA-Z0-9.-]*[a-zA-Z0-9]$ ]] || { echo 'Invalid domain' >&2; exit 1; }
base=/srv/fluxrail
release="$base/releases/$release_id"
[[ -f "$release/index.html" && -f "$release/src/app.js" && -f "$release/tests/golden.js" ]]
[[ "$(cat "$release/deploy-version.txt")" == "$release_id" ]]
exec 9>"$base/.deploy.lock"
flock -x 9
previous=$(readlink "$base/current" || true)
temporary="$base/.current-$$"
trap 'rm -f -- "$temporary"' EXIT
ln -s "$release" "$temporary"
mv -Tf -- "$temporary" "$base/current"

# Check the local nginx TLS endpoint with the correct Host/SNI and certificate.
# Query parameter avoids a proxy cache; release contents must match, not just HTTP 200.
if served=$(curl --fail --silent --show-error --noproxy '*' --max-time 20 \
    --resolve "$domain:443:127.0.0.1" "https://$domain/deploy-version.txt?release=$release_id") \
    && [[ "$served" == "$release_id" ]]; then
    echo "Activated $release_id"
else
    if [[ -n "$previous" ]]; then
        ln -s "$previous" "$temporary"
        mv -Tf -- "$temporary" "$base/current"
        echo "HTTPS check failed; restored $previous" >&2
    else
        rm -f -- "$base/current"
        echo 'HTTPS check failed; removed first release link' >&2
    fi
    exit 1
fi
