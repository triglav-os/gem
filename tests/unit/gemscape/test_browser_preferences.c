/*
 * Regresses selection editing boundaries and browser preference persistence
 * independently of Chromium and the display server.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (C) 2026 tomaz stih
 */
#define _POSIX_C_SOURCE 200809L
#include "browser_settings.h"
#include <gem/gemconf.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static void editing(void)
{
    char text[9] = "abcdefgh";
    gemctl_text_edit_t edit;
    gemctl_text_edit_init(&edit, text, sizeof(text));
    gemctl_text_edit_focus(&edit, 0);
    assert(gemctl_text_edit_key(&edit, 'x') == GEMCTL_TEXT_EDIT_IGNORED);
    gemctl_text_edit_position(&edit, 2, 0);
    gemctl_text_edit_position(&edit, 6, 1);
    assert(gemctl_text_edit_key(&edit, 'X') == GEMCTL_TEXT_EDIT_CHANGED);
    assert(strcmp(text, "abXgh") == 0 && edit.cursor == 3);
    gemctl_text_edit_key_mod(&edit, 80 << 8, K_LSHIFT);
    gemctl_text_edit_key(&edit, 76 << 8);
    assert(strcmp(text, "abgh") == 0 && edit.cursor == 2);
    gemctl_text_edit_key(&edit, 74 << 8);
    gemctl_text_edit_key(&edit, 42 << 8);
    assert(strcmp(text, "abgh") == 0);
    gemctl_text_edit_key(&edit, 77 << 8);
    gemctl_text_edit_key(&edit, 76 << 8);
    assert(strcmp(text, "abgh") == 0);
    gemctl_text_edit_key_mod(&edit, (4 << 8) | 1, K_CTRL);
    gemctl_text_edit_key(&edit, (28 << 8) | 'y');
    gemctl_text_edit_key(&edit, (14 << 8) | 'k');
    assert(strcmp(text, "yk") == 0);
    assert(gemctl_text_edit_key(&edit, 40 << 8) == GEMCTL_TEXT_EDIT_ACCEPT);
    gemctl_text_edit_focus(&edit, 1);
    assert(gemctl_text_edit_insert(&edit, "123456789") ==
           GEMCTL_TEXT_EDIT_IGNORED);
    assert(gemctl_text_edit_insert(&edit, "two\nlines") ==
           GEMCTL_TEXT_EDIT_IGNORED);
    assert(strcmp(text, "yk") == 0);
    assert(gemctl_text_edit_insert(&edit, "paste") == GEMCTL_TEXT_EDIT_CHANGED);
    assert(strcmp(text, "paste") == 0);
    gemctl_text_edit_blur(&edit);
    assert(gemctl_text_edit_key(&edit, 'z') == GEMCTL_TEXT_EDIT_IGNORED);
    assert(strcmp(text, "paste") == 0);
}

static void preferences(void)
{
    char root[] = "/tmp/gem-browser-test-XXXXXX", path[1024];
    gemscape_preferences_t expected, actual;
    gemconf_t config;
    struct stat info;
    assert(mkdtemp(root) != NULL);
    assert(setenv("XDG_CONFIG_HOME", root, 1) == 0);
    assert(setenv("XDG_CONFIG_DIRS", root, 1) == 0);
    assert(setenv("XDG_DATA_HOME", root, 1) == 0);
    unsetenv("GEMSCAPE_PROFILE");
    gemscape_preferences_load(&expected);
    assert(expected.zoom == 100 && expected.frame_rate == 30);
    assert(expected.persistent && expected.cookies &&
           !expected.session_cookies);
    snprintf(expected.homepage, sizeof(expected.homepage),
             "https://example.test/?x=1&y=2");
    expected.zoom = 150;
    expected.persistent = 0;
    expected.cookies = 0;
    expected.javascript = 0;
    expected.images = 0;
    expected.frame_rate = 60;
    assert(gemscape_preferences_save(&expected));
    gemscape_preferences_load(&actual);
    assert(memcmp(&expected, &actual, sizeof(expected)) == 0);
    gemconf_init(&config);
    assert(gemconf_load(&config, "gemscape"));
    assert(gemconf_set(&config, "browser", "zoom", "999999999999999999"));
    assert(gemconf_set(&config, "advanced", "frame_rate", "-1"));
    assert(gemconf_save(&config));
    gemconf_clear(&config);
    gemscape_preferences_load(&actual);
    assert(actual.zoom == 100 && actual.frame_rate == 30);
    assert(gemscape_profile_path(path, sizeof(path)));
    assert(stat(path, &info) == 0 && S_ISDIR(info.st_mode));
    assert((info.st_mode & 077) == 0);
    assert(!gemscape_profile_path(path, 2));
    snprintf(path, sizeof(path), "%s/gemscape", root);
    rmdir(path);
    snprintf(path, sizeof(path), "%s/gem/gemscape.cfg", root);
    unlink(path);
    snprintf(path, sizeof(path), "%s/gem", root);
    rmdir(path);
    rmdir(root);
}

int main(void)
{
    editing();
    preferences();
    puts("PASS browser editor boundaries and persisted preferences");
    return 0;
}
