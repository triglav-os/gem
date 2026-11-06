/*
 * Implements pointer-local AES shell lookups for libgem clients while routing
 * every environment and filesystem operation through the portable OS layer.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (C) 2026 tomaz stih
 */

#include "gem/gem.h"
#include "platform/os.h"

#include <string.h>

WORD shel_envrn(char **env, char *var)
{
    const char *value;

    if (env == NULL || var == NULL) {
        return 0;
    }
    value = gem_os_getenv_ref(var);
    if (value == NULL) {
        *env = NULL;
        return 0;
    }
    *env = (char *)value;
    return 1;
}

WORD shel_find(char *path)
{
    char resolved[260];

    if (path == NULL ||
        gem_os_find_file(path, resolved, sizeof(resolved)) == 0) {
        return 0;
    }
    strcpy(path, resolved);
    return 1;
}
