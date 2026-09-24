/*
 * Exercises layered XDG configuration, typed reads, enumeration, validation
 * and atomic user-file persistence in libgemconf.
 *
 * MIT License (see: LICENSE)
 * Copyright (C) 2026 tomaz stih
 */

#define _POSIX_C_SOURCE 200809L

#include "gem/gemconf.h"
#include "platform/os.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void make_path(char *out, size_t out_size, const char *left,
                      const char *right)
{
    int length = snprintf(out, out_size, "%s/%s", left, right);

    assert(length > 0 && (size_t)length < out_size);
}

int main(void)
{
    char root[] = "/tmp/gemconf-test-XXXXXX";
    char system_dir[GEM_OS_PATH_MAX];
    char user_dir[GEM_OS_PATH_MAX];
    char system_gem[GEM_OS_PATH_MAX];
    char user_gem[GEM_OS_PATH_MAX];
    char system_file[GEM_OS_PATH_MAX];
    char user_file[GEM_OS_PATH_MAX];
    char resolved[GEM_OS_PATH_MAX];
    char too_long[1025];
    const char system_text[] = "# system defaults\n"
                               "[desktop]\n"
                               "show_workspace = false\n"
                               "columns = 12\n"
                               "\n"
                               "[locations]\n"
                               "/tmp = Temporary\n";
    const char user_text[] = "; user override\n"
                             "[desktop]\n"
                             "show_workspace = yes\n";
    gemconf_t config;
    size_t before;
    size_t i;
    int found_location = 0;

    assert(mkdtemp(root) != NULL);
    make_path(system_dir, sizeof(system_dir), root, "system");
    make_path(user_dir, sizeof(user_dir), root, "user");
    make_path(system_gem, sizeof(system_gem), system_dir, "gem");
    make_path(user_gem, sizeof(user_gem), user_dir, "gem");
    make_path(system_file, sizeof(system_file), system_gem, "desktop.cfg");
    make_path(user_file, sizeof(user_file), user_gem, "desktop.cfg");
    assert(gem_os_mkdir_p(system_gem));
    assert(gem_os_mkdir_p(user_gem));
    assert(gem_os_write_file_atomic(system_file, system_text,
                                    sizeof(system_text) - 1u));
    assert(
        gem_os_write_file_atomic(user_file, user_text, sizeof(user_text) - 1u));
    assert(setenv("XDG_CONFIG_DIRS", system_dir, 1) == 0);
    assert(setenv("XDG_CONFIG_HOME", user_dir, 1) == 0);

    gemconf_init(&config);
    assert(gemconf_load(&config, "desktop"));
    assert(gemconf_get_bool(&config, "desktop", "show_workspace", 0));
    assert(gemconf_get_long(&config, "desktop", "columns", 0) == 12);
    assert(strcmp(gemconf_get(&config, "missing", "value", "fallback"),
                  "fallback") == 0);
    for (i = 0u; i < gemconf_count(&config); ++i) {
        if (strcmp(gemconf_entry_section(&config, i), "locations") == 0 &&
            strcmp(gemconf_entry_key(&config, i), "/tmp") == 0 &&
            strcmp(gemconf_entry_value(&config, i), "Temporary") == 0) {
            found_location = 1;
        }
    }
    assert(found_location);
    assert(gemconf_user_path(&config, resolved, sizeof(resolved)));
    assert(strcmp(resolved, user_file) == 0);

    before = gemconf_count(&config);
    memset(too_long, 'x', sizeof(too_long) - 1u);
    too_long[sizeof(too_long) - 1u] = '\0';
    assert(!gemconf_set(&config, "desktop", "too_long", too_long));
    assert(gemconf_count(&config) == before);
    assert(!gemconf_load(&config, "../desktop"));
    assert(gemconf_set(&config, "desktop", "show_trash", "off"));
    assert(gemconf_save(&config));
    gemconf_clear(&config);

    gemconf_init(&config);
    assert(gemconf_load(&config, "desktop"));
    assert(!gemconf_get_bool(&config, "desktop", "show_trash", 1));
    assert(gemconf_remove(&config, "locations", "/tmp"));
    assert(!gemconf_remove(&config, "locations", "/tmp"));
    gemconf_clear(&config);

    assert(gem_os_unlink(system_file));
    assert(gem_os_unlink(user_file));
    assert(gem_os_rmdir(system_gem));
    assert(gem_os_rmdir(user_gem));
    assert(gem_os_rmdir(system_dir));
    assert(gem_os_rmdir(user_dir));
    assert(gem_os_rmdir(root));
    return 0;
}
