#!/usr/bin/env bash
# VS Code debugger entry point; Python and GDB both run inside Docker.
# SPDX-License-Identifier: GPL-2.0-or-later
set -euo pipefail
root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
exec bash "$root/tools/scripts/runtime.sh" exec python3 -B \
    "$root/tools/scripts/sample_debugger.py" "$@"
