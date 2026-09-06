#!/usr/bin/env bash
# Run this checkout's server without deleting sockets or killing other users.
set -euo pipefail
root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
exec "$root/bin/core/gemd" "$@"
