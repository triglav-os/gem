#!/usr/bin/env bash
# Build/test with host compilers replaced by an unprivileged Docker process.
set -euo pipefail
root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
action="${1:-tests}"
case "$action" in all|tests|unit|integration|uat|sdk|audit) ;; *) echo 'Unknown build action' >&2; exit 2;; esac
image="${GEM_CONTAINER_IMAGE:-gem-build:local}"
build_dir="${GEM_CONTAINER_BUILD_DIR:-$root/build/container}"
output_root="${GEM_CONTAINER_OUTPUT_ROOT:-$build_dir/runtime}"
if [[ "$image" == gem-build:local ]]; then
    docker build -t "$image" -f "$root/tools/container/Dockerfile" "$root/tools/container"
elif [[ "$image" == wischner/gcc-x86_64-linux-sdl:* ]]; then
    docker build -t gem-build:sdl --build-arg "GEM_TOOLCHAIN_IMAGE=$image" \
        -f "$root/tools/container/Dockerfile.sdl" "$root/tools/container"
    image=gem-build:sdl
fi
docker run --rm --user "$(id -u):$(id -g)" --cap-drop ALL \
    --security-opt no-new-privileges \
    --mount "type=bind,source=$root,target=$root" \
    --workdir "$root" \
    "$image" make "$action" BUILD_DIR="$build_dir" \
    "CMAKE_FLAGS=-DGEM_OUTPUT_ROOT=$output_root"
