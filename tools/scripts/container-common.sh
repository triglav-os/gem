#!/usr/bin/env bash
# Shared toolchain and environment for build, test and interactive containers.
# SPDX-License-Identifier: GPL-2.0-or-later
image="${GEM_CONTAINER_IMAGE:-wischner/gcc-x86_64-linux-sdl:1.3.0}"
if [[ "$image" == gem-build:local ]]; then
    docker build -t "$image" -f "$root/tools/container/Dockerfile" "$root/tools/container" >&2
elif [[ "$image" == wischner/gcc-x86_64-linux-sdl:* ]]; then
    docker build -t gem-build:sdl --build-arg "GEM_TOOLCHAIN_IMAGE=$image" \
        -f "$root/tools/container/Dockerfile.sdl" "$root/tools/container" >&2
    image=gem-build:sdl
fi
# Cache account metadata in an image layer; all launched processes remain unprivileged.
image="$(docker build --quiet -t "gem-build:user-$(id -u)-$(id -g)" \
    --build-arg "GEM_BASE_IMAGE=$image" \
    --build-arg "GEM_UID=$(id -u)" --build-arg "GEM_GID=$(id -g)" \
    --build-arg "GEM_USER=$(id -un)" --build-arg "GEM_GROUP=$(id -gn)" \
    -f "$root/tools/container/Dockerfile.user" "$root/tools/container")"
container_env=(--env GEM_IN_CONTAINER=1 --env HOME=/tmp/gem-home)
for variable in SDL_VIDEODRIVER SDL_AUDIODRIVER RASTA_BIN GEM_RUNTIME_ROOT \
    GEM_SESSION_DIRECTORY GEM_RESOURCE_DIR GEMD_SOCKET GEM_RASTA_HOST \
    GEM_RASTA_PORT GEM_RASTA_FRAMEBUFFER GEM_RASTA_CURSOR GEM_RASTA_INVERSE \
    GEM_RASTA_SCALE GEM_VDI_WIDTH GEM_VDI_HEIGHT GEM_VDI_BPP GEMREAD_DOCUMENT \
    GEM_TRACE_AES GEM_TRACE_DRAW GEM_TRACE_HID GEMSCAPE_DEBUG; do
    if [[ -v "$variable" ]]; then container_env+=(--env "$variable"); fi
done
