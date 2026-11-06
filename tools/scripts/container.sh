#!/usr/bin/env bash
# Build/test with host compilers replaced by an unprivileged Docker process.
set -euo pipefail
root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
action="${1:-tests}"
if [[ $# -gt 0 ]]; then shift; fi
case "$action" in all|configure|rasta|cef|mupdf|tests|unit|integration|uat|sdk|clean|audit|standards|exec) ;; *) echo 'Unknown build action' >&2; exit 2;; esac
source "$root/tools/scripts/container-common.sh"
build_dir="${BUILD_DIR:-${GEM_CONTAINER_BUILD_DIR:-$root/build/container-sdl}}"
output_root="${GEM_OUTPUT_ROOT:-${GEM_CONTAINER_OUTPUT_ROOT:-$root/bin}}"
command=(make -f tools/scripts/build.mk "$action"
    "BUILD_DIR=$build_dir" "GEM_OUTPUT_ROOT=$output_root"
    "JOBS=${JOBS:-4}" "TEST_JOBS=${TEST_JOBS:-4}"
    "BUILD_TYPE=${BUILD_TYPE:-RelWithDebInfo}" "CMAKE_FLAGS=${CMAKE_FLAGS:-}")
if [[ "$action" == exec ]]; then
    [[ $# -gt 0 ]] || { echo 'exec requires a command' >&2; exit 2; }
    command=("$@")
else
    command+=("$@")
fi
docker run --rm --init --user "$(id -u):$(id -g)" --cap-drop ALL \
    --security-opt no-new-privileges \
    "${container_env[@]}" \
    --mount "type=bind,source=$root,target=$root" \
    --workdir "${GEM_EXEC_WORKDIR:-$root}" \
    "$image" bash "$root/tools/scripts/container-entry.sh" "${command[@]}"
