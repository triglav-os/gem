#!/usr/bin/env bash
# Serialize this checkout's shared build products, test sessions and reports.
# SPDX-License-Identifier: GPL-2.0-or-later
set -euo pipefail
root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
mkdir -p "$root/build"
exec 9>"$root/build/.container.lock"
if ! flock --exclusive --nonblock 9; then
    echo 'Waiting for another GEM build/test command in this checkout...' >&2
    flock --exclusive 9
fi
exec "$@"
