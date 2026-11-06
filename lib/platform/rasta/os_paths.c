/*
 * Resolves Linux XDG cache/state/runtime paths and installed GEM resources.
 * Runtime directories are private and ownership checked; resource discovery
 * works without a checkout or a particular current working directory.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (C) 2026 tomaz stih
 */
#define _POSIX_C_SOURCE 200809L
#include <platform/os.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#ifndef GEM_INSTALL_DATA_DIR
#define GEM_INSTALL_DATA_DIR "/usr/local/share/gem"
#endif

static int user_directory(const char *variable, const char *suffix, char *out,
                          size_t capacity)
{
    const char *base = getenv(variable);
    int length;
    if (!out || !capacity)
        return 0;
    if (base && base[0] == '/')
        length = snprintf(out, capacity, "%s", base);
    else {
        base = getenv("HOME");
        if (!base || base[0] != '/')
            return 0;
        length = snprintf(out, capacity, "%s/%s", base, suffix);
    }
    return length > 0 && (size_t)length < capacity;
}

int gem_os_user_cache_dir(char *out, size_t capacity)
{
    return user_directory("XDG_CACHE_HOME", ".cache", out, capacity);
}

int gem_os_user_state_dir(char *out, size_t capacity)
{
    return user_directory("XDG_STATE_HOME", ".local/state", out, capacity);
}

static int private_directory(const char *path, int create)
{
    struct stat info;
    if (create && mkdir(path, 0700) != 0 && lstat(path, &info) != 0)
        return 0;
    return lstat(path, &info) == 0 && S_ISDIR(info.st_mode) &&
           info.st_uid == geteuid() && (info.st_mode & 0777) == 0700;
}

int gem_os_runtime_dir(char *out, size_t capacity)
{
    const char *base = getenv("XDG_RUNTIME_DIR");
    char fallback[128];
    int length;
    if (!out || !capacity)
        return 0;
    if (!base || base[0] != '/' || !private_directory(base, 0)) {
        snprintf(fallback, sizeof(fallback), "/run/user/%lu",
                 (unsigned long)geteuid());
        if (!private_directory(fallback, 0)) {
            snprintf(fallback, sizeof(fallback), "/tmp/gem-runtime-%lu",
                     (unsigned long)geteuid());
            if (!private_directory(fallback, 1))
                return 0;
        }
        base = fallback;
    }
    length = snprintf(out, capacity, "%s/gem", base);
    return length > 0 && (size_t)length < capacity && private_directory(out, 1);
}

int gem_os_executable_dir(char *out, size_t capacity)
{
    if (!out || capacity < 2)
        return 0;
    ssize_t length = readlink("/proc/self/exe", out, capacity - 1);
    if (length <= 0 || (size_t)length >= capacity - 1)
        return 0;
    out[length] = '\0';
    char *slash = strrchr(out, '/');
    if (!slash)
        return 0;
    *slash = '\0';
    return 1;
}

static int resource_at(const char *base, const char *name, char *out,
                       size_t capacity)
{
    int length = snprintf(out, capacity, "%s/%s", base, name);
    return length > 0 && (size_t)length < capacity &&
           gem_os_access(out, GEM_OS_ACCESS_READ);
}

int gem_os_find_resource(const char *name, char *out, size_t capacity)
{
    char base[GEM_OS_PATH_MAX], path[GEM_OS_PATH_MAX];
    const char *override = getenv("GEM_RESOURCE_DIR");
    const char *directories = getenv("XDG_DATA_DIRS");
    if (!name || !out || !capacity)
        return 0;
    if (name[0] == '/')
        return resource_at("", name, out, capacity);
    if (override && override[0] && resource_at(override, name, out, capacity))
        return 1;
    if (gem_os_user_data_dir(base, sizeof(base))) {
        int length = snprintf(path, sizeof(path), "%s/gem", base);
        if (length > 0 && (size_t)length < sizeof(path) &&
            resource_at(path, name, out, capacity))
            return 1;
    }
    if (!directories || !directories[0])
        directories = "/usr/local/share:/usr/share";
    while (*directories) {
        const char *end = strchr(directories, ':');
        size_t length = end ? (size_t)(end - directories) : strlen(directories);
        if (*directories == '/' && length + 5 < sizeof(path)) {
            memcpy(path, directories, length);
            strcpy(path + length, "/gem");
            if (resource_at(path, name, out, capacity))
                return 1;
        }
        if (!end)
            break;
        directories = end + 1;
    }
    if (gem_os_executable_dir(base, sizeof(base))) {
        int length = snprintf(path, sizeof(path), "%s/../share/gem", base);
        if (length > 0 && (size_t)length < sizeof(path) &&
            resource_at(path, name, out, capacity))
            return 1;
    }
    if (resource_at(GEM_INSTALL_DATA_DIR, name, out, capacity))
        return 1;
    out[0] = '\0';
    return 0;
}
