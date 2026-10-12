/*
 * Maintains the hosted file selector's directory model: splitting the
 * caller's path and selection, wildcard matching, path joining, entry
 * sorting and cooperative directory scans that yield to other clients.
 *
 * MIT License (see: LICENSE)
 * Copyright (C) 2026 tomaz stih
 */

#include "fsel_private.h"

#include "platform/os.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

static int aes_fsel_has_wildcards(const char *text)
{
    if (text == NULL) {
        return 0;
    }

    while (*text != '\0') {
        if (*text == '*' || *text == '?') {
            return 1;
        }
        ++text;
    }
    return 0;
}

void aes_fsel_copy_text(char *target, size_t target_size, const char *source)
{
    size_t length;

    if (target == NULL || target_size == 0u) {
        return;
    }
    if (source == NULL) {
        target[0] = '\0';
        return;
    }
    length = strlen(source);
    if (length >= target_size) {
        length = target_size - 1u;
    }
    memmove(target, source, length);
    target[length] = '\0';
}

void aes_fsel_normalize_dir(char *path)
{
    size_t length;

    if (path == NULL || path[0] == '\0') {
        return;
    }

    length = strlen(path);
    while (length > 1 &&
           (path[length - 1u] == '/' || path[length - 1u] == '\\')) {
        path[length - 1u] = '\0';
        --length;
    }
}

void aes_fsel_parent_dir(char *path)
{
    char *slash;

    if (path == NULL || path[0] == '\0') {
        return;
    }
    aes_fsel_normalize_dir(path);
    slash = strrchr(path, '/');
    if (slash == NULL) {
        strcpy(path, ".");
        return;
    }
    if (slash == path) {
        slash[1] = '\0';
        return;
    }
    *slash = '\0';
}

void aes_fsel_split_input(const char *pipath, const char *pisel,
                          char *directory, size_t directory_size, char *pattern,
                          size_t pattern_size, char *selection,
                          size_t selection_size)
{
    char source[AES_PATH_LEN];
    char *slash;
    char cwd[AES_PATH_LEN];

    if (directory != NULL && directory_size > 0u) {
        directory[0] = '\0';
    }
    if (pattern != NULL && pattern_size > 0u) {
        pattern[0] = '\0';
    }
    if (selection != NULL && selection_size > 0u) {
        selection[0] = '\0';
    }

    if (pisel != NULL && selection != NULL && selection_size > 0u) {
        aes_fsel_copy_text(selection, selection_size, pisel);
    }

    if (pipath == NULL || pipath[0] == '\0') {
        if (!gem_os_getcwd(cwd, sizeof(cwd))) {
            strcpy(cwd, ".");
        }
        if (directory != NULL && directory_size > 0u) {
            aes_fsel_copy_text(directory, directory_size, cwd);
        }
        if (pattern != NULL && pattern_size > 0u) {
            strcpy(pattern, "*");
        }
        return;
    }

    aes_fsel_copy_text(source, sizeof(source), pipath);
    slash = strrchr(source, '/');
    if (slash == NULL) {
        slash = strrchr(source, '\\');
    }

    if (slash != NULL) {
        *slash = '\0';
        if (directory != NULL && directory_size > 0u) {
            if (source[0] != '\0') {
                aes_fsel_copy_text(directory, directory_size, source);
            } else {
                strcpy(directory, "/");
            }
        }
        if (pattern != NULL && pattern_size > 0u) {
            aes_fsel_copy_text(pattern, pattern_size, slash + 1);
        }
    } else {
        if (directory != NULL && directory_size > 0u) {
            if (!gem_os_getcwd(directory, directory_size)) {
                strcpy(directory, ".");
            }
        }
        if (pattern != NULL && pattern_size > 0u) {
            aes_fsel_copy_text(pattern, pattern_size, source);
        }
    }

    if (directory != NULL && directory[0] == '\0') {
        strcpy(directory, ".");
    }
    if (pattern != NULL && pattern[0] == '\0') {
        strcpy(pattern, "*");
    } else if (pattern != NULL && aes_fsel_has_wildcards(pattern) == 0) {
        if (selection != NULL && selection[0] == '\0') {
            aes_fsel_copy_text(selection, selection_size, pattern);
        }
        strcpy(pattern, "*");
    }
    if (directory != NULL) {
        aes_fsel_normalize_dir(directory);
    }
}

int aes_fsel_match_pattern(const char *pattern, const char *name)
{
    const char *star = NULL, *retry = NULL;
    if (!pattern || !name)
        return 0;
    /* Only the most recent star needs retrying: matching another star
     * subsumes earlier choices. O(pattern * name), with no recursion. */
    while (*name) {
        if (*pattern == '*') {
            star = ++pattern;
            retry = name;
        } else if (*pattern == '?' ||
                   (*pattern && tolower((unsigned char)*pattern) ==
                                    tolower((unsigned char)*name))) {
            ++pattern;
            ++name;
        } else if (star) {
            pattern = star;
            name = ++retry;
        } else {
            return 0;
        }
    }
    while (*pattern == '*')
        ++pattern;
    return *pattern == '\0';
}

int aes_fsel_join_path(const char *directory, const char *name, char *path,
                       size_t path_size)
{
    int rc;

    if (directory == NULL || name == NULL || path == NULL || path_size == 0u) {
        return 0;
    }
    if (strcmp(directory, "/") == 0) {
        rc = snprintf(path, path_size, "/%s", name);
    } else if (strcmp(directory, ".") == 0) {
        rc = snprintf(path, path_size, "./%s", name);
    } else {
        rc = snprintf(path, path_size, "%s/%s", directory, name);
    }
    return rc >= 0 && (size_t)rc < path_size;
}

static int aes_fsel_entry_compare(const void *left, const void *right)
{
    const aes_fsel_entry_t *a = (const aes_fsel_entry_t *)left;
    const aes_fsel_entry_t *b = (const aes_fsel_entry_t *)right;
    size_t i = 0u;

    if (a->is_directory != b->is_directory) {
        return (b->is_directory - a->is_directory);
    }

    while (a->name[i] != '\0' || b->name[i] != '\0') {
        int ca = tolower((unsigned char)a->name[i]);
        int cb = tolower((unsigned char)b->name[i]);

        if (ca != cb) {
            return ca - cb;
        }
        if (a->name[i] == '\0' || b->name[i] == '\0') {
            break;
        }
        ++i;
    }
    return 0;
}

void aes_fsel_set_selection_from_index(aes_fsel_state_t *state)
{
    if (state != NULL)
        state->editing_name = 0;
    if (state == NULL || state->selected < 0 ||
        state->selected >= state->entry_count ||
        state->entries[state->selected].is_directory != 0) {
        if (state != NULL) {
            state->selection[0] = '\0';
        }
        return;
    }

    aes_fsel_copy_text(state->selection, sizeof(state->selection),
                       state->entries[state->selected].name);
}

void aes_fsel_reload_entries(aes_fsel_state_t *state)
{
    gem_os_dir_t dir;
    gem_os_dirent_t dent;
    WORD count = 0;
    WORD selected = NIL;
    WORD sort_start = 0;
    unsigned scanned = 0;

    if (state == NULL) {
        return;
    }

    state->entry_count = 0;
    state->selected = NIL;

    if (strcmp(state->directory, "/") != 0) {
        strcpy(state->entries[count].name, "..");
        state->entries[count].is_directory = 1;
        state->entries[count].size_bytes = 0u;
        if (strcmp(state->selection, "..") == 0) {
            selected = count;
        }
        ++count;
        sort_start = 1;
    }

    if (gem_os_dir_open(state->directory, &dir) != 0) {
        while (count < AES_FSEL_MAX_ENTRIES &&
               gem_os_dir_read(&dir, &dent) != 0) {
            if (++scanned % 64u == 0 && aes_wait_hook && !aes_wait_hook())
                break;
            if (strcmp(dent.name, ".") == 0 || strcmp(dent.name, "..") == 0) {
                continue;
            }
            if (state->show_hidden == 0 &&
                (dent.info.is_hidden != 0 || dent.name[0] == '.')) {
                continue;
            }
            if (state->mode == AES_FSEL_DIRECTORY &&
                dent.info.is_directory == 0) {
                continue;
            }
            if (state->mode == AES_FSEL_LOCATION &&
                dent.info.is_directory == 0) {
                const char *dot = strrchr(dent.name, '.');
                if (dot == NULL ||
                    (strcasecmp(dot, ".st") != 0 &&
                     strcasecmp(dot, ".msa") != 0)) {
                    continue;
                }
            }
            if (dent.info.is_directory == 0 &&
                aes_fsel_match_pattern(state->pattern, dent.name) == 0) {
                continue;
            }

            aes_fsel_copy_text(state->entries[count].name,
                               sizeof(state->entries[count].name), dent.name);
            state->entries[count].is_directory = dent.info.is_directory ? 1 : 0;
            state->entries[count].size_bytes = dent.info.size_bytes;
            if (strcmp(state->selection, dent.name) == 0) {
                selected = count;
            }
            ++count;
        }
        gem_os_dir_close(&dir);
    }

    if (count - sort_start > 1) {
        qsort(&state->entries[sort_start], (size_t)(count - sort_start),
              sizeof(state->entries[0]), aes_fsel_entry_compare);
        selected = NIL;
        for (WORD i = 0; i < count; ++i) {
            if (strcmp(state->selection, state->entries[i].name) == 0) {
                selected = i;
                break;
            }
        }
    }

    state->entry_count = count;
    if (selected == NIL && count > 0 && state->selection[0] == '\0' &&
        state->mode != AES_FSEL_DIRECTORY) {
        selected = 0;
    }
    state->selected = selected;
    if (selected != NIL)
        aes_fsel_set_selection_from_index(state);
}
