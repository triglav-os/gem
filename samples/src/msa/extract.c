/*
 * Implements the msa tool's host-side file operations: whole-file reads
 * and writes, directory creation, and the descriptor-based extraction
 * that refuses symlinks, foreign owners and multiply linked targets.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (C) 2026 tomaz stih
 */

#define _POSIX_C_SOURCE 200809L

#include "msa_cli.h"

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

int msa_read_binary_file(const char *path, uint8_t **data_out, size_t *size_out,
                         char *error_text, size_t error_size)
{
    FILE *stream;
    long file_size;
    uint8_t *data;

    if (path == NULL || data_out == NULL || size_out == NULL) {
        snprintf(error_text, error_size, "invalid input path");
        return 0;
    }

    stream = fopen(path, "rb");
    if (stream == NULL) {
        snprintf(error_text, error_size, "%s", strerror(errno));
        return 0;
    }
    if (fseek(stream, 0L, SEEK_END) != 0) {
        fclose(stream);
        snprintf(error_text, error_size, "failed to seek input file");
        return 0;
    }
    file_size = ftell(stream);
    if (file_size < 0 || fseek(stream, 0L, SEEK_SET) != 0) {
        fclose(stream);
        snprintf(error_text, error_size, "failed to size input file");
        return 0;
    }

    data = malloc((size_t)file_size);
    if (data == NULL) {
        fclose(stream);
        snprintf(error_text, error_size, "out of memory");
        return 0;
    }
    if (file_size > 0 &&
        fread(data, 1u, (size_t)file_size, stream) != (size_t)file_size) {
        free(data);
        fclose(stream);
        snprintf(error_text, error_size, "failed to read input file");
        return 0;
    }
    fclose(stream);

    *data_out = data;
    *size_out = (size_t)file_size;
    return 1;
}

int msa_write_binary_file(const char *path, const uint8_t *data, size_t size,
                          char *error_text, size_t error_size)
{
    FILE *stream;

    if (path == NULL || (size > 0 && data == NULL)) {
        snprintf(error_text, error_size, "invalid output path");
        return 0;
    }

    stream = fopen(path, "wb");
    if (stream == NULL) {
        snprintf(error_text, error_size, "%s", strerror(errno));
        return 0;
    }
    if (size > 0 && fwrite(data, 1u, size, stream) != size) {
        fclose(stream);
        snprintf(error_text, error_size, "failed to write output file");
        return 0;
    }
    fclose(stream);
    return 1;
}

int msa_mkdir_p(const char *path)
{
    char *copy;
    char *scan;

    if (path == NULL || path[0] == '\0') {
        return 0;
    }

    copy = malloc(strlen(path) + 1u);
    if (copy == NULL) {
        return 0;
    }
    strcpy(copy, path);

    for (scan = copy + 1; *scan != '\0'; ++scan) {
        if (*scan != '/') {
            continue;
        }
        *scan = '\0';
        if (mkdir(copy, 0777) != 0 && errno != EEXIST) {
            free(copy);
            return 0;
        }
        *scan = '/';
    }

    if (mkdir(copy, 0777) != 0 && errno != EEXIST) {
        free(copy);
        return 0;
    }
    free(copy);
    return 1;
}

static void msa_set_extract_error(msa_extract_context_t *extract,
                                  const char *message, const char *path)
{
    if (extract == NULL) {
        return;
    }
    if (path == NULL) {
        snprintf(extract->error_text, sizeof(extract->error_text), "%s",
                 message);
        return;
    }
    snprintf(extract->error_text, sizeof(extract->error_text), "%s: %.180s",
             message, path);
}

/* Walk directory descriptors, never archive-supplied symlinks or parents. */
static int msa_extract_parent(int root_fd, char *path, char **name)
{
    int directory = dup(root_fd);
    char *component = path;
    if (directory < 0 || !path[0] || path[0] == '/') {
        if (directory >= 0)
            close(directory);
        return -1;
    }
    for (;;) {
        char *slash = strchr(component, '/');
        if (slash)
            *slash = '\0';
        if (!*component || !strcmp(component, ".") ||
            !strcmp(component, "..") || strchr(component, '\\')) {
            close(directory);
            return -1;
        }
        if (!slash) {
            *name = component;
            return directory;
        }
        if (mkdirat(directory, component, 0700) && errno != EEXIST) {
            close(directory);
            return -1;
        }
        int next = openat(directory, component,
                          O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC);
        close(directory);
        if (next < 0)
            return -1;
        directory = next;
        component = slash + 1;
    }
}

int msa_extract_entry(const fat12_dirent_t *entry, const char *path,
                      void *context)
{
    msa_extract_context_t *extract = context;
    char relative[256];
    char *name = NULL;
    uint8_t *data = NULL;
    size_t size = 0;
    int fd = -1;
    int ok = 0;
    if (!extract || !entry || !path)
        return 0;
    if (entry->attr & FAT12_ATTR_VOLUME)
        return 1;
    if (!strcmp(entry->name, ".") || !strcmp(entry->name, ".."))
        return 1;
    if (strlen(path) >= sizeof(relative)) {
        msa_set_extract_error(extract, "path exceeds extraction limit", path);
        return 0;
    }
    strcpy(relative, path);
    int parent = msa_extract_parent(extract->root_fd, relative, &name);
    if (parent < 0) {
        msa_set_extract_error(extract, "unsafe extraction directory", path);
        return 0;
    }
    if (entry->attr & FAT12_ATTR_DIRECTORY) {
        if (!mkdirat(parent, name, 0700) || errno == EEXIST) {
            fd = openat(parent, name,
                        O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC);
            ok = fd >= 0;
        }
    } else if (fat12_read_file_alloc(extract->image, entry, &data, &size,
                                     extract->error_text,
                                     sizeof(extract->error_text))) {
        fd = openat(parent, name,
                    O_WRONLY | O_CREAT | O_NOFOLLOW | O_NONBLOCK | O_CLOEXEC,
                    0600);
        struct stat st;
        if (fd >= 0 && !fstat(fd, &st) && S_ISREG(st.st_mode) &&
            st.st_nlink == 1 && st.st_uid == geteuid() && !ftruncate(fd, 0)) {
            size_t written = 0;
            while (written < size) {
                ssize_t count = write(fd, data + written, size - written);
                if (count < 0 && errno == EINTR)
                    continue;
                if (count <= 0)
                    break;
                written += (size_t)count;
            }
            ok = written == size;
        }
    }
    free(data);
    if (fd >= 0 && close(fd))
        ok = 0;
    close(parent);
    if (!ok && !extract->error_text[0])
        msa_set_extract_error(extract, "refused or failed extraction", path);
    return ok;
}
