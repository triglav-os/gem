#!/usr/bin/env bash
set -euo pipefail
rpc_dir=$(mktemp -d /tmp/gem-rpc-test.XXXXXXXX)
export GEMD_SOCKET="$rpc_dir/socket"
export GEM_RASTA_FRAMEBUFFER="$rpc_dir/framebuffer"
export GEM_RASTA_PORT=5003 GEM_VDI_WIDTH=904 GEM_VDI_HEIGHT=900
server_pid=
cleanup() {
    local status=$?
    if [[ -n "$server_pid" ]]; then
        kill "$server_pid" 2>/dev/null || true
        wait "$server_pid" 2>/dev/null || true
    fi
    cat "$rpc_dir/log" >&2
    rm -f "$rpc_dir/socket" "$rpc_dir/framebuffer" "$rpc_dir/log"
    rmdir "$rpc_dir"
    exit "$status"
}
trap cleanup EXIT
"$1" >"$rpc_dir/log" 2>&1 &
server_pid=$!
for (( i=0; i<100; ++i )); do
    [[ -S "$GEMD_SOCKET" ]] && break
    kill -0 "$server_pid"
    sleep .05
done
"$2"
kill -0 "$server_pid"
