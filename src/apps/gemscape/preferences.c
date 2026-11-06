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

static int gemscape_storage_path(char *path, size_t capacity, int kind)
{
    const char *override = gem_os_getenv_ref("GEMSCAPE_PROFILE");
    char base[GEM_OS_PATH_MAX], directory[GEM_OS_PATH_MAX];
    int length;
    if (override && override[0] == '/') {
        length = snprintf(directory, sizeof(directory), "%s%s", override,
                          kind == 1 ? "/cache" : "");
    } else {
        int okay = kind == 0   ? gem_os_user_data_dir(base, sizeof(base))
                   : kind == 1 ? gem_os_user_cache_dir(base, sizeof(base))
                               : gem_os_user_state_dir(base, sizeof(base));
        if (!okay)
            return 0;
        length =
            snprintf(directory, sizeof(directory), "%s/gem/gemscape", base);
        if (kind == 0 && length > 0 && (size_t)length < sizeof(directory) &&
            !gem_os_path_exists(directory)) {
            char old[GEM_OS_PATH_MAX], parent[GEM_OS_PATH_MAX];
            int old_length = snprintf(old, sizeof(old), "%s/gemscape", base);
            int parent_length =
                snprintf(parent, sizeof(parent), "%s/gem", base);
            if (old_length > 0 && (size_t)old_length < sizeof(old) &&
                parent_length > 0 && (size_t)parent_length < sizeof(parent) &&
                gem_os_path_exists(old) && gem_os_mkdir_p(parent) &&
                !gem_os_copy_path(old, directory))
                return 0;
        }
    }
    if (length <= 0 || (size_t)length >= sizeof(directory) ||
        !gem_os_mkdir_p(directory))
        return 0;
    length = snprintf(path, capacity, "%s%s", directory,
                      kind == 2 ? "/cef.log" : "");
    return length > 0 && (size_t)length < capacity;
}

int gemscape_profile_path(char *path, size_t capacity)
{
    return gemscape_storage_path(path, capacity, 0);
}

int gemscape_cache_path(char *path, size_t capacity)
{
    return gemscape_storage_path(path, capacity, 1);
}

int gemscape_log_path(char *path, size_t capacity)
{
    return gemscape_storage_path(path, capacity, 2);
}
