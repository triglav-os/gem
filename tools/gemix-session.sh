#!/bin/sh

# Starts one native Gemix desktop session from a relocatable package.

set -eu

script_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
gemix_root=$(CDPATH= cd -- "$script_dir/.." && pwd)

export GEM_RESOURCE_DIR="${GEM_RESOURCE_DIR:-$gemix_root/share/gem}"
export PATH="$script_dir${PATH:+:$PATH}"

server_pid=
cleanup()
{
    if [ -n "$server_pid" ]; then
        kill "$server_pid" 2>/dev/null || true
        wait "$server_pid" 2>/dev/null || true
    fi
}
trap cleanup EXIT HUP INT TERM

"$script_dir/gemd" &
server_pid=$!

attempt=0
while [ ! -S /tmp/gemd.sock ]; do
    if ! kill -0 "$server_pid" 2>/dev/null; then
        wait "$server_pid"
        exit 1
    fi
    attempt=$((attempt + 1))
    if [ "$attempt" -ge 100 ]; then
        echo "gemix-session: gemd did not create /tmp/gemd.sock" >&2
        exit 1
    fi
    sleep 0.02
done

cd "$gemix_root"
"$script_dir/desktop"
