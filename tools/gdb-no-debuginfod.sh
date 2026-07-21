#!/usr/bin/env bash
set -euo pipefail

# VS Code's cppdbg adapter may re-enable debuginfod during launch. Clearing the
# URL list here prevents startup stalls while still letting the adapter use gdb.
export DEBUGINFOD_URLS=

exec /usr/bin/gdb "$@"
