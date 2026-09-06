#!/bin/sh
set -eu

script_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
port=${1:-${PORT:-8080}}
bind_address=${BIND_ADDRESS:-127.0.0.1}
web_root=${WEB_ROOT:-$script_dir/public}

if [ ! -d "$web_root" ]; then
    echo "Web root does not exist: $web_root" >&2
    exit 1
fi

printf 'Serving %s at http://%s:%s\n' "$web_root" "$bind_address" "$port"
cd "$web_root"
exec python3 -m http.server "$port" --bind "$bind_address"
