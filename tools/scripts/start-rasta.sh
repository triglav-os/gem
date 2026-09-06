#!/usr/bin/env bash
# Run the downloaded viewer in the foreground; never kill unrelated sessions.
set -euo pipefail
root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
viewer="${RASTA_BIN:-$root/bin/tools/rasta}"
if [[ ! -x "$viewer" ]]; then
    echo 'Run make to download and build Rasta.' >&2
    exit 1
fi
exec "$viewer" --inverse --port "${GEM_RASTA_PORT:-5000}" "$@"
