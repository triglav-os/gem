/*
 * Loads whole files and resolves resource paths for the hosted AES:
 * GEM_RESOURCE_DIR first, then the working directory and the bundled
 * resource directory, trying the exact and lowercase spellings.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (C) 2026 tomaz stih
 */

#include "aes_internal.h"

#include "platform/os.h"

#include <stdio.h>
#include <string.h>

int aes_load_file(const char *filename, void **data_out, size_t *size_out)
{
    int fd;
    int32_t read_size;
    size_t capacity = 4096u;
    size_t used = 0;
    char *buffer;

    if (filename == NULL || data_out == NULL || size_out == NULL) {
        return 0;
    }

    fd = gem_os_open_read(filename);
    if (fd < 0) {
        return 0;
    }

    buffer = gem_os_alloc(capacity);
    if (buffer == NULL) {
        (void)gem_os_close(fd);
        return 0;
    }

    FOREVER
    {
        if (used == capacity) {
            size_t new_capacity = capacity * 2u;
            char *new_buffer = gem_os_alloc(new_capacity);

            if (new_buffer == NULL) {
                gem_os_free(buffer);
                (void)gem_os_close(fd);
                return 0;
            }
            memcpy(new_buffer, buffer, used);
            gem_os_free(buffer);
            buffer = new_buffer;
            capacity = new_capacity;
        }

        read_size = gem_os_read(fd, buffer + used, (uint32_t)(capacity - used));
        if (read_size < 0) {
            gem_os_free(buffer);
            (void)gem_os_close(fd);
            return 0;
        }
        if (read_size == 0) {
            break;
        }
        used += (size_t)read_size;
    }

    (void)gem_os_close(fd);
    *data_out = buffer;
    *size_out = used;
    return 1;
}

static void aes_ascii_lower(const char *source, char *target,
                            size_t target_size)
{
    size_t i;

    if (target == NULL || target_size == 0u) {
        return;
    }

    if (source == NULL) {
        target[0] = '\0';
        return;
    }

    for (i = 0; source[i] != '\0' && i + 1u < target_size; ++i) {
        char ch = source[i];

        if (ch >= 'A' && ch <= 'Z') {
            ch = (char)(ch - 'A' + 'a');
        }
        target[i] = ch;
    }
    target[i] = '\0';
}

int aes_try_resolve_path(const char *filename, char *resolved,
                         size_t resolved_size)
{
    static const char *search_dirs[] = {"", "bin/resources/"};
    const char *resource_dir;
    char lowercase[260];
    size_t i;

    if (filename == NULL || resolved == NULL || resolved_size == 0u) {
        return 0;
    }

    aes_ascii_lower(filename, lowercase, sizeof(lowercase));
    resource_dir = gem_os_getenv_ref("GEM_RESOURCE_DIR");
    if (resource_dir != NULL && resource_dir[0] != '\0') {
        int rc =
            snprintf(resolved, resolved_size, "%s/%s", resource_dir, filename);
        int fd;

        if (rc > 0 && (size_t)rc < resolved_size) {
            fd = gem_os_open_read(resolved);
            if (fd >= 0) {
                (void)gem_os_close(fd);
                return 1;
            }
        }
        rc =
            snprintf(resolved, resolved_size, "%s/%s", resource_dir, lowercase);
        if (rc > 0 && (size_t)rc < resolved_size) {
            fd = gem_os_open_read(resolved);
            if (fd >= 0) {
                (void)gem_os_close(fd);
                return 1;
            }
        }
    }

    for (i = 0; i < sizeof(search_dirs) / sizeof(search_dirs[0]); ++i) {
        const char *dir = search_dirs[i];
        int rc;
        int fd;

        rc = snprintf(resolved, resolved_size, "%s%s", dir, filename);
        if (rc > 0 && (size_t)rc < resolved_size) {
            fd = gem_os_open_read(resolved);
            if (fd >= 0) {
                (void)gem_os_close(fd);
                return 1;
            }
        }

        rc = snprintf(resolved, resolved_size, "%s%s", dir, lowercase);
        if (rc > 0 && (size_t)rc < resolved_size) {
            fd = gem_os_open_read(resolved);

            if (fd >= 0) {
                (void)gem_os_close(fd);
                return 1;
            }
        }
    }

    return 0;
}
