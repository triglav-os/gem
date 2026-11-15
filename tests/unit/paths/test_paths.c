/*
 * Verifies XDG path resolution, data precedence and private runtime storage.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (C) 2026 tomaz stih
 */
#define _POSIX_C_SOURCE 200809L
#include <platform/os.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static void application_resources(const char *root)
{
    char path[1024], expected[1024], directories[2048];

    snprintf(expected, sizeof(expected), "%s/gemscape", root);
    assert(gem_os_mkdir(expected));
    snprintf(expected, sizeof(expected), "%s/gemscape/fixture.rsc", root);
    assert(gem_os_create_file(expected, "app", 3));
    /* The explicit resource override wins over the application directory. */
    assert(gem_os_find_app_resource("gemscape", "fixture.rsc", path,
                                    sizeof(path)));
    assert(strstr(path, "/override/fixture.rsc"));
    unsetenv("GEM_RESOURCE_DIR");
    assert(gem_os_find_app_resource("gemscape", "fixture.rsc", path,
                                    sizeof(path)));
    assert(strcmp(path, expected) == 0);
    assert(gem_os_find_resource("fixture.rsc", path, sizeof(path)));
    assert(strstr(path, "/gem/fixture.rsc"));
    assert(!gem_os_find_app_resource("../gemscape", "fixture.rsc", path,
                                     sizeof(path)) &&
           path[0] == '\0');
    assert(!gem_os_find_app_resource("gemscape", "fixture.rsc", path, 2));
    assert(!gem_os_find_app_resource("gemscape", "missing.rsc", path,
                                     sizeof(path)) &&
           path[0] == '\0');
    assert(unlink(expected) == 0);
    snprintf(expected, sizeof(expected), "%s/system/gemscape", root);
    assert(gem_os_mkdir_p(expected));
    snprintf(expected, sizeof(expected), "%s/system/gemscape/fixture.rsc",
             root);
    assert(gem_os_create_file(expected, "system", 6));
    snprintf(directories, sizeof(directories), "relative:/missing:%s/system",
             root);
    setenv("XDG_DATA_DIRS", directories, 1);
    assert(gem_os_find_app_resource("gemscape", "fixture.rsc", path,
                                    sizeof(path)));
    assert(strcmp(path, expected) == 0);
    assert(unlink(expected) == 0);
    assert(gem_os_find_app_resource("gemscape", "fixture.rsc", path,
                                    sizeof(path)));
    assert(strstr(path, "/gem/fixture.rsc"));
}

int main(int argc, char **argv)
{
    char root[] = "/tmp/gem-paths-XXXXXX", path[1024], expected[1024],
         data[1024];
    struct stat info;
    /* The install-layout test relocates this executable beside the apps. */
    if (argc == 4) {
        assert(gem_os_find_app_resource(argv[1], argv[2], path, sizeof(path)));
        assert(strcmp(path, argv[3]) == 0);
        return 0;
    }
    assert(argc == 1);
    assert(mkdtemp(root));
    assert(setenv("HOME", root, 1) == 0);
    setenv("XDG_CACHE_HOME", "relative", 1);
    setenv("XDG_STATE_HOME", "", 1);
    assert(gem_os_user_cache_dir(path, sizeof(path)));
    snprintf(expected, sizeof(expected), "%s/.cache", root);
    assert(strcmp(path, expected) == 0);
    assert(gem_os_user_state_dir(path, sizeof(path)));
    snprintf(expected, sizeof(expected), "%s/.local/state", root);
    assert(strcmp(path, expected) == 0);
    setenv("XDG_CACHE_HOME", root, 1);
    assert(gem_os_user_cache_dir(path, sizeof(path)) &&
           strcmp(path, root) == 0);
    assert(!gem_os_user_cache_dir(path, 2));
    setenv("XDG_RUNTIME_DIR", root, 1);
    assert(gem_os_runtime_dir(path, sizeof(path)));
    snprintf(expected, sizeof(expected), "%s/gem", root);
    assert(strcmp(path, expected) == 0);
    assert(lstat(path, &info) == 0 && info.st_uid == geteuid() &&
           (info.st_mode & 0777) == 0700);
    assert(rmdir(path) == 0);
    assert(symlink(root, path) == 0);
    assert(!gem_os_runtime_dir(path, sizeof(path)));
    assert(unlink(expected) == 0);
    assert(gem_os_runtime_dir(path, sizeof(path)));
    setenv("XDG_DATA_HOME", root, 1);
    setenv("XDG_DATA_DIRS", "relative:/missing", 1);
    unsetenv("GEM_RESOURCE_DIR");
    snprintf(data, sizeof(data), "%s/gem/fixture.rsc", root);
    assert(gem_os_create_file(data, "user", 4));
    assert(gem_os_find_resource("fixture.rsc", path, sizeof(path)));
    assert(strcmp(path, data) == 0);
    snprintf(expected, sizeof(expected), "%s/override", root);
    assert(gem_os_mkdir(expected));
    setenv("GEM_RESOURCE_DIR", expected, 1);
    snprintf(data, sizeof(data), "%s/override/fixture.rsc", root);
    assert(gem_os_create_file(data, "override", 8));
    assert(gem_os_find_resource("fixture.rsc", path, sizeof(path)) &&
           strcmp(path, data) == 0);
    assert(!gem_os_find_resource("missing.rsc", path, sizeof(path)) &&
           path[0] == '\0');
    application_resources(root);
    assert(gem_os_remove_tree_under(root, "/tmp"));
    return 0;
}
