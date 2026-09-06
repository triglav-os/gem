/*
 * Resolves sample data beside installed executables, with an explicit
 * override and a build-tree fallback. Only the public GEM API is used.
 *
 * MIT License (see: LICENSE)
 * Copyright (C) 2026 tomaz stih
 */
#define _POSIX_C_SOURCE 200809L
#include "sample_resources.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static WORD load_from(const char *directory, const char *name)
{
    char path[260];
    if (!directory || !*directory)
        return 0;
    int size = snprintf(path, sizeof(path), "%s/%s", directory, name);
    return size > 0 && (size_t)size < sizeof(path) ? rsrc_load(path) : 0;
}

WORD sample_resource_load(const char *name)
{
    char executable[260];
    if (!name || strchr(name, '/') || strchr(name, '\\'))
        return 0;
    if (load_from(getenv("GEM_SAMPLE_DATA"), name))
        return 1;
    ssize_t length =
        readlink("/proc/self/exe", executable, sizeof(executable) - 1);
    if (length > 0 && (size_t)length < sizeof(executable) - 1) {
        executable[length] = '\0';
        char *slash = strrchr(executable, '/');
        if (slash && (size_t)(slash - executable) + 6 < sizeof(executable)) {
            strcpy(slash, "/data");
            if (load_from(executable, name))
                return 1;
        }
    }
    return load_from(SAMPLE_BUILD_DATA, name);
}
