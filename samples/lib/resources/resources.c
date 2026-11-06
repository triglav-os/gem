/*
 * Resolves sample data beside installed executables, with an explicit
 * override and a build-tree fallback, and copies resource bit blocks into
 * application-owned memory so they outlive rsrc_free(). Only the public
 * GEM API is used.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (C) 2026 tomaz stih
 */
#define _POSIX_C_SOURCE 200809L
#include "sample_resources.h"

#include <stdint.h>
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
    if (rsrc_load((char *)name))
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

void sample_free_bitblk(BITBLK *bitblk)
{
    if (bitblk == NULL) {
        return;
    }

    free((void *)(intptr_t)bitblk->bi_pdata);
    bitblk->bi_pdata = 0;
}

int sample_clone_bitblk(BITBLK *dst, const BITBLK *src)
{
    size_t plane_bytes;
    void *data_copy;

    if (dst == NULL || src == NULL || src->bi_pdata == 0 || src->bi_wb <= 0 ||
        src->bi_hl <= 0) {
        return 0;
    }

    plane_bytes = (size_t)src->bi_wb * (size_t)src->bi_hl;
    data_copy = malloc(plane_bytes);
    if (data_copy == NULL) {
        return 0;
    }

    memcpy(data_copy, (const void *)(intptr_t)src->bi_pdata, plane_bytes);
    *dst = *src;
    dst->bi_pdata = (LONG)(intptr_t)data_copy;
    return 1;
}

int sample_load_bitblks(const char *primary_path, const char *fallback_path,
                        BITBLK *bitblks, WORD count)
{
    BITBLK *bitblk;
    WORD ii;

    if (bitblks == NULL || count <= 0) {
        return 0;
    }
    if (sample_resource_load(primary_path) == 0 &&
        sample_resource_load(fallback_path) == 0) {
        return 0;
    }

    for (ii = 0; ii < count; ++ii) {
        if (rsrc_gaddr(R_BITBLK, ii, (void **)&bitblk) == 0 || bitblk == NULL ||
            !sample_clone_bitblk(&bitblks[ii], bitblk)) {
            /* Every earlier clone owns a plane; release them all. */
            for (WORD loaded = 0; loaded < ii; ++loaded) {
                sample_free_bitblk(&bitblks[loaded]);
            }
            rsrc_free();
            return 0;
        }
    }
    rsrc_free();
    return 1;
}
