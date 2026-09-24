/*
 * Resolves Desktop file associations and launches their program through the
 * AES shell. Atari executable families have built-in Stout associations;
 * ordered desktop.cfg association sections override those defaults.
 *
 * MIT License (see: LICENSE)
 * Copyright (C) 2026 tomaz stih
 */

#include "desktop.h"
#include "prg/prg.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>

enum { DESKTOP_ASSOCIATION_MAX = 1000 };

static int desktop_pattern_match(const char *pattern, size_t pattern_length,
                                 const char *name)
{
    size_t pattern_index = 0u;
    size_t name_index = 0u;
    size_t star = (size_t)-1;
    size_t retry = 0u;

    while (name[name_index] != '\0') {
        if (pattern_index < pattern_length &&
            pattern[pattern_index] == '*') {
            star = pattern_index++;
            retry = name_index;
        } else if (pattern_index < pattern_length &&
                   (pattern[pattern_index] == '?' ||
                    tolower((unsigned char)pattern[pattern_index]) ==
                        tolower((unsigned char)name[name_index]))) {
            ++pattern_index;
            ++name_index;
        } else if (star != (size_t)-1) {
            pattern_index = star + 1u;
            name_index = ++retry;
        } else {
            return 0;
        }
    }
    while (pattern_index < pattern_length &&
           pattern[pattern_index] == '*')
        ++pattern_index;
    return pattern_index == pattern_length;
}

static int desktop_patterns_match(const char *patterns, const char *name)
{
    const char *cursor = patterns;

    if (patterns == NULL || name == NULL) return 0;
    while (*cursor != '\0') {
        const char *start;
        const char *end;

        while (*cursor == ' ' || *cursor == '\t' || *cursor == ',' ||
               *cursor == ';')
            ++cursor;
        start = cursor;
        while (*cursor != '\0' && *cursor != ',' && *cursor != ';') ++cursor;
        end = cursor;
        while (end > start && isspace((unsigned char)end[-1])) --end;
        if (end > start &&
            desktop_pattern_match(start, (size_t)(end - start), name))
            return 1;
    }
    return 0;
}

static int desktop_builtin_stout_pattern(const char *name)
{
    prg_file_kind_t program_kind;

    return prg_file_kind(name, &program_kind);
}

static int desktop_program_is_stout(const char *program)
{
    const char *leaf;

    if (program == NULL) return 0;
    leaf = strrchr(program, '/');
    leaf = leaf != NULL ? leaf + 1u : program;
    return strcasecmp(leaf, "stout") == 0;
}

static const char *desktop_associated_program(const char *name)
{
    static char section[32];
    unsigned int index;

    for (index = 0u; index < DESKTOP_ASSOCIATION_MAX; ++index) {
        const char *kind;
        const char *patterns;
        const char *program;

        (void)snprintf(section, sizeof(section), "association.%03u", index);
        kind = gemconf_get(&g_desktop.config, section, "kind", NULL);
        patterns = gemconf_get(&g_desktop.config, section, "patterns", NULL);
        program = gemconf_get(&g_desktop.config, section, "executable", NULL);
        if (kind == NULL && patterns == NULL && program == NULL) continue;
        if (kind == NULL || strcasecmp(kind, "directory") == 0 ||
            program == NULL || program[0] == '\0' ||
            !desktop_patterns_match(patterns, name))
            continue;
        return program;
    }
    return desktop_builtin_stout_pattern(name) ? "stout" : NULL;
}

static int desktop_resolve_program(const char *program, char *resolved,
                                   size_t resolved_size)
{
    if (desktop_program_is_stout(program)) {
        if (gem_os_find_executable("stout", resolved, resolved_size) != 0 ||
            gem_os_find_file("bin/apps/stout", resolved, resolved_size) != 0)
            return 1;
    }
    if (strlen(program) >= resolved_size) return 0;
    memcpy(resolved, program, strlen(program) + 1u);
    return 1;
}

static int desktop_launch_tail(const desktop_browser_window_t *browser,
                               const desktop_browser_entry_t *entry,
                               const char *program, int pass_entry,
                               char tail[128])
{
    char text[127];
    int length;

    if (!pass_entry) {
        tail[0] = 0;
        tail[1] = '\0';
        return 1;
    }
    if (browser->backend == DESKTOP_BROWSER_DISK &&
        desktop_program_is_stout(program)) {
        size_t root_length = strlen(browser->disk_root);
        const char *relative = entry->path;

        if (strncmp(relative, browser->disk_root, root_length) == 0) {
            relative += root_length;
            while (*relative == '/') ++relative;
        }
        length = snprintf(text, sizeof(text), "\"A:/%s\" \"%s\"", relative,
                          browser->disk_root);
    } else {
        length = snprintf(text, sizeof(text), "\"%s\"", entry->path);
    }
    if (length <= 0 || (size_t)length >= sizeof(text)) return 0;
    tail[0] = (char)length;
    memcpy(tail + 1u, text, (size_t)length + 1u);
    return 1;
}

int desktop_browser_entry_is_launchable(const desktop_browser_window_t *browser,
                                        const desktop_browser_entry_t *entry)
{
    if (browser == NULL || entry == NULL || entry->is_dir != 0 ||
        entry->is_parent != 0 ||
        (browser->backend != DESKTOP_BROWSER_FILES &&
         browser->backend != DESKTOP_BROWSER_DISK))
        return 0;
    return desktop_associated_program(entry->name) != NULL ||
           entry->is_app != 0;
}

int desktop_browser_launch_entry(const desktop_browser_window_t *browser,
                                 const desktop_browser_entry_t *entry,
                                 char *status, size_t status_size)
{
    const char *associated = desktop_associated_program(entry->name);
    const char *program = associated;
    char resolved[GEM_OS_PATH_MAX];
    char tail[128];
    int okay;

    if (status == NULL || status_size == 0u) return 0;
    if (program == NULL && entry->is_executable != 0 &&
        browser->backend == DESKTOP_BROWSER_FILES)
        program = entry->path;
    if (program == NULL ||
        !desktop_resolve_program(program, resolved, sizeof(resolved)) ||
        !desktop_launch_tail(browser, entry, program, associated != NULL,
                             tail)) {
        (void)snprintf(status, status_size, "Cannot launch %.*s", 470,
                       entry->name);
        return 0;
    }
    okay = shel_write(1, 1, 0, resolved, tail) != 0;
    (void)snprintf(status, status_size,
                   okay ? "Launched %.*s" : "Launch failed: %.*s", 470,
                   entry->name);
    return okay;
}
