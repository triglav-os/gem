#!/usr/bin/env bash
set -euo pipefail
root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
if [[ "${GEM_IN_CONTAINER:-}" != 1 ]]; then
    exec bash "$root/tools/scripts/runtime.sh" exec bash "$root/tools/scripts/gdb-no-debuginfod.sh" "$@"
fi

# VS Code's cppdbg adapter may re-enable debuginfod during launch. Clearing the
# URL list here prevents startup stalls while still letting the adapter use gdb.
export DEBUGINFOD_URLS=

exec /usr/bin/gdb "$@"
