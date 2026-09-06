#!/usr/bin/env bash
# Build/test with host compilers replaced by an unprivileged Docker process.
set -euo pipefail
root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
action="${1:-tests}"
case "$action" in all|tests|unit|integration|uat|sdk|audit) ;; *) echo 'Unknown build action' >&2; exit 2;; esac
docker build -t gem-build:local -f "$root/tools/container/Dockerfile" "$root/tools/container"
docker run --rm --user "$(id -u):$(id -g)" --cap-drop ALL \
    --security-opt no-new-privileges \
    --mount "type=bind,source=$root,target=/workspace" \
    gem-build:local make "$action" BUILD_DIR=/workspace/build/container \
    CMAKE_FLAGS=-DGEM_OUTPUT_ROOT=/workspace/build/container/runtime
