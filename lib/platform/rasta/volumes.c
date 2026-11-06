/*
 * Enumerates mounted volumes for the GEM desktop from the mount table,
 * exposing only real filesystems with capitalized labels.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (C) 2026 tomaz stih
 */

#define _GNU_SOURCE
#include "platform/os.h"

#include <ctype.h>
#include <errno.h>
#include <mntent.h>
#include <stdio.h>
#include <string.h>

static int gem_os_should_expose_mount(const struct mntent *mnt)
{
    static const char *const ignored_fs[] = {
        "proc",   "sysfs",   "tmpfs",    "devtmpfs",    "devpts",
        "cgroup", "cgroup2", "overlay",  "squashfs",    "nsfs",
        "mqueue", "debugfs", "tracefs",  "securityfs",  "pstore",
        "autofs", "fusectl", "configfs", "binfmt_misc", "ramfs"};
    size_t i;

    if (mnt == NULL || mnt->mnt_dir == NULL || mnt->mnt_type == NULL) {
        return 0;
    }
    if (strcmp(mnt->mnt_dir, "/") == 0) {
        return 1;
    }
    if (strncmp(mnt->mnt_dir, "/snap/", 6) == 0 ||
        strncmp(mnt->mnt_dir, "/proc", 5) == 0 ||
        strncmp(mnt->mnt_dir, "/sys", 4) == 0 ||
        strncmp(mnt->mnt_dir, "/dev", 4) == 0 ||
        (strncmp(mnt->mnt_dir, "/run", 4) == 0 &&
         strncmp(mnt->mnt_dir, "/run/media/", 11) != 0)) {
        return 0;
    }
    for (i = 0; i < sizeof(ignored_fs) / sizeof(ignored_fs[0]); ++i) {
        if (strcmp(mnt->mnt_type, ignored_fs[i]) == 0) {
            return 0;
        }
    }
    return 1;
}

static void gem_os_capitalize_label(char *label, size_t size)
{
    size_t i;
    int new_word = 1;

    if (label == NULL || size == 0u) {
        return;
    }

    for (i = 0; i < size && label[i] != '\0'; ++i) {
        if (label[i] == ' ' || label[i] == '_' || label[i] == '-') {
            new_word = 1;
            continue;
        }
        if (new_word != 0 && label[i] >= 'a' && label[i] <= 'z') {
            label[i] = (char)(label[i] - ('a' - 'A'));
        } else if (new_word == 0 && label[i] >= 'A' && label[i] <= 'Z') {
            label[i] = (char)(label[i] + ('a' - 'A'));
        }
        new_word = 0;
    }
}

int gem_os_volume_iter_open(gem_os_volume_iter_t *iter)
{
    FILE *fp;

    if (iter == NULL) {
        errno = EINVAL;
        return 0;
    }

    fp = setmntent("/proc/mounts", "r");
    if (fp == NULL) {
        return 0;
    }

    iter->handle = fp;
    return 1;
}

int gem_os_volume_iter_read(gem_os_volume_iter_t *iter, gem_os_volume_t *volume)
{
    FILE *fp;
    struct mntent *mnt;
    const char *base;

    if (iter == NULL || volume == NULL || iter->handle == NULL) {
        errno = EINVAL;
        return 0;
    }

    fp = (FILE *)iter->handle;
    for (;;) {
        mnt = getmntent(fp);
        if (mnt == NULL) {
            return 0;
        }
        if (!gem_os_should_expose_mount(mnt)) {
            continue;
        }

        strncpy(volume->mount_path, mnt->mnt_dir,
                sizeof(volume->mount_path) - 1u);
        volume->mount_path[sizeof(volume->mount_path) - 1u] = '\0';

        if (strcmp(mnt->mnt_dir, "/") == 0) {
            strncpy(volume->label, "Root", sizeof(volume->label) - 1u);
            volume->label[sizeof(volume->label) - 1u] = '\0';
        } else {
            base = strrchr(mnt->mnt_dir, '/');
            if (base != NULL && base[1] != '\0') {
                ++base;
            } else {
                base = mnt->mnt_dir;
            }
            strncpy(volume->label, base, sizeof(volume->label) - 1u);
            volume->label[sizeof(volume->label) - 1u] = '\0';
            gem_os_capitalize_label(volume->label, sizeof(volume->label));
        }
        return 1;
    }
}

void gem_os_volume_iter_close(gem_os_volume_iter_t *iter)
{
    if (iter == NULL || iter->handle == NULL) {
        return;
    }

    (void)endmntent((FILE *)iter->handle);
    iter->handle = NULL;
}
