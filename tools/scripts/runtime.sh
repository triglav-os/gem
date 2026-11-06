#!/usr/bin/env bash
# Keep viewer, applications and debugger in one checkout-specific container.
# SPDX-License-Identifier: GPL-2.0-or-later
set -euo pipefail
root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
key="$(printf '%s' "$root" | sha256sum)"
name="gem-runtime-${key:0:16}-$(id -u)"
action="${1:-run}"
shift || true
case "$action" in run|session|exec|stop) ;; *) echo 'Unknown runtime action' >&2; exit 2;; esac
mkdir -p "$root/build"
# Protect creation/removal, but release before long-lived docker exec commands.
exec 8>"$root/build/.runtime.lock"
flock --exclusive 8
if [[ "$action" == stop || ( "$action" == session && "${1:-}" == stop ) ]]; then
    if docker container inspect "$name" >/dev/null 2>&1; then
        if [[ "$action" == session ]]; then shift; fi
        docker exec "$name" python3 -B "$root/tools/scripts/sample_session.py" stop "$@" 2>/dev/null || true
        docker rm -f "$name" >/dev/null
    fi
    exit
fi
if [[ "$(docker inspect --format '{{.State.Running}}' "$name" 2>/dev/null || true)" != true ]]; then
    docker rm "$name" >/dev/null 2>&1 || true
    source "$root/tools/scripts/container-common.sh"
    display_args=()
    if [[ -n "${DISPLAY:-}" ]]; then
        display_args+=(--env DISPLAY)
        if [[ -d /tmp/.X11-unix ]]; then
            display_args+=(--mount type=bind,source=/tmp/.X11-unix,target=/tmp/.X11-unix,readonly)
        fi
        authority="${XAUTHORITY:-$HOME/.Xauthority}"
        if [[ -f "$authority" ]]; then
            display_args+=(--env XAUTHORITY=/tmp/gem.Xauthority
                --mount "type=bind,source=$authority,target=/tmp/gem.Xauthority,readonly")
        fi
    fi
    if [[ -n "${WAYLAND_DISPLAY:-}" && -n "${XDG_RUNTIME_DIR:-}" ]]; then
        socket="$XDG_RUNTIME_DIR/$WAYLAND_DISPLAY"
        if [[ -S "$socket" ]]; then
            display_args+=(--env XDG_RUNTIME_DIR=/tmp/gem-wayland --env WAYLAND_DISPLAY
                --mount "type=bind,source=$socket,target=/tmp/gem-wayland/$WAYLAND_DISPLAY")
        fi
    fi
    # Bash reads the kernel hostname without invoking host preload-sensitive tools.
    IFS= read -r runtime_hostname < /proc/sys/kernel/hostname
    docker run --detach --init --name "$name" --hostname "$runtime_hostname" \
        --user "$(id -u):$(id -g)" \
        --cap-drop ALL --cap-add SYS_PTRACE --security-opt no-new-privileges \
        "${container_env[@]}" "${display_args[@]}" \
        --mount "type=bind,source=$root,target=$root" --workdir "$root" \
        "$image" sleep infinity >/dev/null
fi
flock --unlock 8
case "$action" in
    session) exec docker exec -i "$name" python3 -B "$root/tools/scripts/sample_session.py" "$@" ;;
    exec) exec docker exec -i "$name" "$@" ;;
    run)
        trap 'docker exec "$name" python3 -B "$root/tools/scripts/sample_session.py" stop >/dev/null 2>&1 || true' EXIT
        docker exec -i "$name" python3 -B "$root/tools/scripts/sample_session.py" run "$@"
        ;;
esac
