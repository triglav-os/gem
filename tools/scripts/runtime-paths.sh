#!/usr/bin/env bash
# Resolve and migrate interactive XDG storage. Sourced by runtime.sh.
# SPDX-License-Identifier: GPL-2.0-or-later
xdg_base() {
    local value="${!1:-}"
    if [[ "$value" == /* ]]; then printf '%s' "$value"; else printf '%s/%s' "$HOME" "$2"; fi
}
gem_config_base="$(xdg_base XDG_CONFIG_HOME .config)"
gem_data_base="$(xdg_base XDG_DATA_HOME .local/share)"
gem_cache_base="$(xdg_base XDG_CACHE_HOME .cache)"
gem_state_base="$(xdg_base XDG_STATE_HOME .local/state)"
# Only GEM's own directories and the freedesktop home Trash are shared.
# Creating these with a private umask does not chmod existing user directories.
(
    umask 077
    mkdir -p "$gem_config_base/gem" "$gem_data_base/gem" "$gem_data_base/Trash" \
        "$gem_cache_base/gem" "$gem_state_base/gem"
    # Previous session code wrote to build/f5/config even when user-data was set.
    for legacy in "$root/build/f5/config/gem" "$root/build/user-data/config/gem"; do
        if [[ -d "$legacy" ]]; then
            for file in "$legacy"/*.cfg; do
                [[ -f "$file" && ! -e "$gem_config_base/gem/${file##*/}" ]] || continue
                cp -n -- "$file" "$gem_config_base/gem/"
            done
        fi
    done
    if [[ ! -e "$gem_data_base/gem/gemscape" ]]; then
        for legacy in "$root/build/user-data/gemscape" "$gem_data_base/gemscape"; do
            if [[ -d "$legacy" ]]; then
                cp -a -- "$legacy" "$gem_data_base/gem/gemscape"
                break
            fi
        done
    fi
)
container_env+=(--env "XDG_CONFIG_HOME=$gem_config_base" --env "XDG_DATA_HOME=$gem_data_base"
    --env "XDG_CACHE_HOME=$gem_cache_base" --env "XDG_STATE_HOME=$gem_state_base"
    --env "XDG_RUNTIME_DIR=/run/user/$(id -u)"
    --env "GEM_SESSION_DIRECTORY=${GEM_SESSION_DIRECTORY:-/run/user/$(id -u)/gem/session-${key:0:16}}"
    --env "GEM_SESSION_LOG_DIRECTORY=$gem_state_base/gem/sessions/${key:0:16}")
storage_args=(--tmpfs "/run/user/$(id -u):rw,uid=$(id -u),gid=$(id -g),mode=0700")
declare -A gem_storage_mounts=()
gem_storage_mount() {
    local directory="$1" mode="${2:-}"
    # XDG bases may intentionally share a directory. Docker rejects duplicates.
    [[ -z "${gem_storage_mounts[$directory]:-}" ]] || return 0
    gem_storage_mounts["$directory"]=1
    storage_args+=(--mount "type=bind,source=$directory,target=$directory$mode")
}
for directory in "$gem_config_base/gem" "$gem_data_base/gem" "$gem_data_base/Trash" \
    "$gem_cache_base/gem" "$gem_state_base/gem"; do
    gem_storage_mount "$directory"
done
# System configuration is read-only and follows XDG precedence inside Docker.
gem_system_config="${XDG_CONFIG_DIRS:-/etc/xdg}"
container_env+=(--env "XDG_CONFIG_DIRS=$gem_system_config")
IFS=: read -r -a gem_system_dirs <<< "$gem_system_config"
for directory in "${gem_system_dirs[@]}"; do
    if [[ "$directory" == /* && -d "$directory/gem" ]]; then
        gem_storage_mount "$directory/gem" ,readonly
    fi
done
if [[ -n "${GEMSCAPE_PROFILE:-}" ]]; then
    [[ "$GEMSCAPE_PROFILE" == /* ]] || { echo 'GEMSCAPE_PROFILE must be absolute' >&2; exit 2; }
    (umask 077; mkdir -p "$GEMSCAPE_PROFILE")
    container_env+=(--env GEMSCAPE_PROFILE)
    gem_storage_mount "$GEMSCAPE_PROFILE"
fi
