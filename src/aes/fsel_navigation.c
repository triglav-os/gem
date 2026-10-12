/*
 * Implements modern file-chooser locations and navigation history. The
 * selector discovers only existing directories through the host abstraction
 * and keeps navigation changes local until the caller accepts a result.
 *
 * MIT License (see: LICENSE)
 * Copyright (C) 2026 tomaz stih
 */

#include "fsel_private.h"

#include "platform/os.h"

#include <stdio.h>
#include <string.h>

static int aes_fsel_location_exists(const aes_fsel_state_t *state,
                                    const char *path)
{
    WORD i;

    for (i = 0; i < state->location_count; ++i) {
        if (strcmp(state->locations[i].path, path) == 0) {
            return 1;
        }
    }
    return 0;
}

static void aes_fsel_add_location(aes_fsel_state_t *state, const char *label,
                                  const char *path)
{
    aes_fsel_location_t *location;
    gem_os_file_info_t info;

    if (state == NULL || label == NULL || path == NULL || path[0] != '/' ||
        state->location_count >= AES_FSEL_MAX_LOCATIONS ||
        aes_fsel_location_exists(state, path) != 0 ||
        gem_os_stat_path(path, &info) == 0 || info.is_directory == 0) {
        return;
    }
    location = &state->locations[state->location_count++];
    aes_fsel_copy_text(location->label, sizeof(location->label), label);
    aes_fsel_copy_text(location->path, sizeof(location->path), path);
    aes_fsel_normalize_dir(location->path);
}

static const char *aes_fsel_volume_label(const gem_os_volume_t *volume)
{
    const char *slash;

    if (volume->label[0] != '\0') {
        return volume->label;
    }
    if (strcmp(volume->mount_path, "/") == 0) {
        return "Computer";
    }
    slash = strrchr(volume->mount_path, '/');
    return slash != NULL && slash[1] != '\0' ? slash + 1 : volume->mount_path;
}

static void aes_fsel_select_location(aes_fsel_state_t *state);

void aes_fsel_build_locations(aes_fsel_state_t *state)
{
    const char *home;
    char cwd[AES_PATH_LEN];
    gem_os_volume_iter_t volumes = {0};
    gem_os_volume_t volume;

    if (state == NULL) {
        return;
    }
    state->location_count = 0;
    state->selected_location = NIL;
    home = gem_os_getenv_ref("HOME");
    if (home != NULL) {
        aes_fsel_add_location(state, "Home", home);
    }
    if (gem_os_getcwd(cwd, sizeof(cwd)) != 0) {
        aes_fsel_add_location(state, "Workspace", cwd);
    }
    aes_fsel_add_location(state, "Computer", "/");
    if (gem_os_volume_iter_open(&volumes) != 0) {
        while (state->location_count < AES_FSEL_MAX_LOCATIONS &&
               gem_os_volume_iter_read(&volumes, &volume) != 0) {
            aes_fsel_add_location(state, aes_fsel_volume_label(&volume),
                                  volume.mount_path);
        }
        gem_os_volume_iter_close(&volumes);
    }
    aes_fsel_select_location(state);
}

static void aes_fsel_select_location(aes_fsel_state_t *state)
{
    WORD i;

    state->selected_location = NIL;
    for (i = 0; i < state->location_count; ++i) {
        if (strcmp(state->locations[i].path, state->directory) == 0) {
            state->selected_location = i;
            break;
        }
    }
}

void aes_fsel_navigate(aes_fsel_state_t *state, const char *path, int remember)
{
    gem_os_file_info_t info;
    char candidate[AES_PATH_LEN];

    if (state == NULL || path == NULL || path[0] == '\0' ||
        strlen(path) >= sizeof(state->directory) ||
        gem_os_stat_path(path, &info) == 0 || info.is_directory == 0) {
        return;
    }
    aes_fsel_copy_text(candidate, sizeof(candidate), path);
    aes_fsel_normalize_dir(candidate);
    strcpy(state->directory, candidate);
    if (remember != 0 &&
        (state->history_count == 0 ||
         strcmp(state->history[state->history_index], state->directory) != 0)) {
        if (state->history_index + 1 < state->history_count) {
            state->history_count = (WORD)(state->history_index + 1);
        }
        if (state->history_count == AES_FSEL_MAX_HISTORY) {
            memmove(state->history, state->history[1],
                    (AES_FSEL_MAX_HISTORY - 1u) * sizeof(state->history[0]));
            --state->history_count;
            if (state->history_index > 0) {
                --state->history_index;
            }
        }
        aes_fsel_copy_text(state->history[state->history_count],
                           sizeof(state->history[0]), state->directory);
        state->history_index = state->history_count++;
    }
    state->selection[0] = '\0';
    state->editing_name = 0;
    gemctl_text_edit_blur(&state->path_edit);
    state->selected = NIL;
    state->last_click_entry = NIL;
    state->last_click_ms = 0u;
    state->top_index = 0;
    aes_fsel_select_location(state);
    aes_fsel_reload_entries(state);
    aes_fsel_sync_work(state);
}

void aes_fsel_enter_directory(aes_fsel_state_t *state, const char *name)
{
    char path[AES_PATH_LEN];

    if (state == NULL || name == NULL) {
        return;
    }
    aes_fsel_copy_text(path, sizeof(path), state->directory);
    if (strcmp(name, "..") == 0) {
        aes_fsel_parent_dir(path);
    } else if (aes_fsel_join_path(state->directory, name, path, sizeof(path)) ==
               0) {
        return;
    }
    aes_fsel_navigate(state, path, 1);
}

void aes_fsel_move_history(aes_fsel_state_t *state, WORD delta)
{
    WORD target;

    if (state == NULL || state->history_count == 0) {
        return;
    }
    target = (WORD)(state->history_index + delta);
    if (target < 0 || target >= state->history_count) {
        return;
    }
    state->history_index = target;
    aes_fsel_navigate(state, state->history[target], 0);
}
