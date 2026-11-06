/*
 * Loads and saves validated Gemscape preferences and resolves its private
 * persistent CEF profile independently of the disposable Docker home.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (C) 2026 tomaz stih
 */
#include "browser_settings.h"
#include <gem/gemconf.h>
#include <gem/os.h>
#include <stdio.h>
#include <string.h>

void gemscape_preferences_load(gemscape_preferences_t *preferences)
{
    gemconf_t config;
    long value;
    gemconf_init(&config);
    gemconf_load(&config, "gemscape");
    memset(preferences, 0, sizeof(*preferences));
    snprintf(preferences->homepage, sizeof(preferences->homepage), "%s",
             gemconf_get(&config, "browser", "homepage",
                         "https://www.wikipedia.org/"));
    value = gemconf_get_long(&config, "browser", "zoom", 100);
    preferences->zoom = value >= 50 && value <= 200 ? (int)value : 100;
    preferences->persistent =
        gemconf_get_bool(&config, "privacy", "persistent", 1);
    preferences->session_cookies =
        gemconf_get_bool(&config, "privacy", "session_cookies", 0);
    preferences->cookies = gemconf_get_bool(&config, "privacy", "cookies", 1);
    preferences->javascript =
        gemconf_get_bool(&config, "advanced", "javascript", 1);
    preferences->images = gemconf_get_bool(&config, "advanced", "images", 1);
    value = gemconf_get_long(&config, "advanced", "frame_rate", 30);
    preferences->frame_rate =
        value == 15 || value == 30 || value == 60 ? (int)value : 30;
    gemconf_clear(&config);
}

int gemscape_preferences_save(const gemscape_preferences_t *preferences)
{
    gemconf_t config;
    char zoom[16], rate[16];
    int result;
    gemconf_init(&config);
    gemconf_load(&config, "gemscape");
    snprintf(zoom, sizeof(zoom), "%d", preferences->zoom);
    snprintf(rate, sizeof(rate), "%d", preferences->frame_rate);
    result =
        gemconf_set(&config, "browser", "homepage", preferences->homepage) &&
        gemconf_set(&config, "browser", "zoom", zoom) &&
        gemconf_set(&config, "privacy", "persistent",
                    preferences->persistent ? "true" : "false") &&
        gemconf_set(&config, "privacy", "session_cookies",
                    preferences->session_cookies ? "true" : "false") &&
        gemconf_set(&config, "privacy", "cookies",
                    preferences->cookies ? "true" : "false") &&
        gemconf_set(&config, "advanced", "javascript",
                    preferences->javascript ? "true" : "false") &&
        gemconf_set(&config, "advanced", "images",
                    preferences->images ? "true" : "false") &&
        gemconf_set(&config, "advanced", "frame_rate", rate) &&
        gemconf_save(&config);
    gemconf_clear(&config);
    return result;
}

int gemscape_profile_path(char *path, size_t capacity)
{
    const char *base = gem_os_getenv_ref("GEMSCAPE_PROFILE");
    const char *home;
    int size;
    if (base != NULL && base[0] == '/') {
        size = snprintf(path, capacity, "%s", base);
    } else {
        base = gem_os_getenv_ref("XDG_DATA_HOME");
        if (base != NULL && base[0] == '/')
            size = snprintf(path, capacity, "%s/gemscape", base);
        else {
            home = gem_os_getenv_ref("HOME");
            if (home == NULL || home[0] != '/')
                return 0;
            size = snprintf(path, capacity, "%s/.local/share/gemscape", home);
        }
    }
    return size > 0 && (size_t)size < capacity && gem_os_mkdir_p(path);
}
