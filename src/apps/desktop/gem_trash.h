/*
 * Declares the GEMix desktop Trash backend. It implements the freedesktop
 * Trash layout behind virtual trash items while leaving all host filesystem
 * access to the GEM operating-system abstraction.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (C) 2026 tomaz stih
 */

#ifndef GEM_SAMPLE_TRASH_H
#define GEM_SAMPLE_TRASH_H

#include <gem/os.h>

#include <stddef.h>
#include <stdint.h>

enum {
    GEM_TRASH_OK = 0,
    GEM_TRASH_INVALID = -1,
    GEM_TRASH_IO_ERROR = -2,
    GEM_TRASH_EXISTS = -3,
    GEM_TRASH_NOT_FOUND = -4,
    GEM_TRASH_FULL = -5
};

typedef struct gem_trash_item {
    char name[GEM_OS_PATH_MAX];
    char payload_path[GEM_OS_PATH_MAX];
    char info_path[GEM_OS_PATH_MAX];
    char original_path[GEM_OS_PATH_MAX];
    char store_root[GEM_OS_PATH_MAX];
    char deletion_date[32];
    uint64_t size_bytes;
    int64_t mtime;
    int is_directory;
    int is_executable;
} gem_trash_item_t;

typedef int (*gem_trash_list_callback_t)(const gem_trash_item_t *item,
                                         void *context);

/*
 * Percent-encodes path into encoded. Returns GEM_TRASH_OK when it fits.
 */
int gem_trash_encode_path(const char *path, char *encoded,
                          size_t encoded_size);

/*
 * Decodes a percent-encoded absolute path. Invalid escapes and relative paths
 * are rejected.
 */
int gem_trash_decode_path(const char *encoded, char *path, size_t path_size);

/*
 * Moves absolute source_path into the correct same-filesystem Trash store and
 * optionally returns its virtual item metadata in moved_item.
 */
int gem_trash_put(const char *source_path, gem_trash_item_t *moved_item);

/*
 * Visits every valid top-level item in the home and mounted-volume Trash
 * stores. item_count may be NULL. A callback returning zero stops iteration.
 */
int gem_trash_list(gem_trash_list_callback_t callback, void *context,
                   size_t *item_count);

/*
 * Resolves an existing payload directory for browsing and enforces that it
 * remains under item's Trash files store.
 */
int gem_trash_resolve_directory(const gem_trash_item_t *item,
                                const char *path, char *resolved,
                                size_t resolved_size);

/* Restores item to its recorded original path, recreating parent folders. */
int gem_trash_restore(const gem_trash_item_t *item);

/* Copy or move a validated top-level Trash item to a new path, never replacing
 * an existing entry. A successful move also removes its Trash metadata. */
int gem_trash_export(const gem_trash_item_t *item, const char *destination,
                     int move);

/* Restores item beside its original path under a generated collision name. */
int gem_trash_restore_renamed(const gem_trash_item_t *item);

/* Permanently removes item and its .trashinfo record. */
int gem_trash_purge(const gem_trash_item_t *item);

/* Permanently removes all valid items from every known Trash store. */
int gem_trash_empty(size_t *removed_count);

#endif
