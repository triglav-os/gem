#!/usr/bin/env bash
# Run this checkout's server without deleting sockets or killing other users.
set -euo pipefail
root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
if [[ "${GEM_IN_CONTAINER:-}" != 1 ]]; then
    exec bash "$root/tools/scripts/runtime.sh" exec bash "$root/tools/scripts/start-gemd.sh" "$@"
fi
exec "$root/bin/core/gemd" "$@"
