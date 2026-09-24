/*
 * Implements raw ST and MSA disk-image loading and FAT12 read access.
 *
 * MIT License (see: LICENSE)
 * Copyright (C) 2026 tomaz stih
 */

#include "stout/disk.h"

#include "msa/msa.h"
#include "platform/os.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

static void stout_disk_error(char *text, size_t size, const char *message)
{
    if (text != NULL && size != 0u) {
        snprintf(text, size, "%s", message);
    }
}

static int stout_disk_read_file(const char *path, uint8_t **data, size_t *size,
                                char *error_text, size_t error_size)
{
    int fd;
    int64_t length;
    uint8_t *buffer;
    size_t offset;
    int32_t count;

    fd = gem_os_open_read(path);
    if (fd < 0) {
        stout_disk_error(error_text, error_size, "unable to open disk image");
        return 0;
    }
    length = gem_os_seek(fd, 0, 2);
    if (length <= 0 || gem_os_seek(fd, 0, 0) < 0 ||
        (uint64_t)length > SIZE_MAX) {
        gem_os_close(fd);
        stout_disk_error(error_text, error_size, "invalid disk image size");
        return 0;
    }
    buffer = malloc((size_t)length);
    if (buffer == NULL) {
        gem_os_close(fd);
        stout_disk_error(error_text, error_size, "unable to read disk image");
        return 0;
    }
    offset = 0u;
    while (offset < (size_t)length) {
        count = gem_os_read(fd, buffer + offset,
                            (uint32_t)((size_t)length - offset));
        if (count <= 0) {
            free(buffer);
            gem_os_close(fd);
            stout_disk_error(error_text, error_size, "unable to read disk image");
            return 0;
        }
        offset += (size_t)count;
    }
    if (gem_os_close(fd) < 0) {
        free(buffer);
        stout_disk_error(error_text, error_size, "unable to read disk image");
        return 0;
    }
    *data = buffer;
    *size = (size_t)length;
    return 1;
}

void stout_disk_init(stout_disk_t *disk)
{
    if (disk != NULL) memset(disk, 0, sizeof(*disk));
}

void stout_disk_free(stout_disk_t *disk)
{
    if (disk == NULL) return;
    free(disk->data);
    stout_disk_init(disk);
}

int stout_disk_open(stout_disk_t *disk, const char *path,
                    char *error_text, size_t error_size)
{
    const char *dot;
    uint8_t *raw = NULL;
    size_t raw_size = 0u;
    msa_image_t msa;
    int ok = 0;

    if (disk == NULL || path == NULL) return 0;
    stout_disk_free(disk);
    dot = strrchr(path, '.');
    if (dot != NULL && strcasecmp(dot, ".msa") == 0) {
        msa_image_init(&msa);
        if (!msa_read_file(path, &msa, error_text, error_size)) return 0;
        raw_size = msa.data_size;
        raw = malloc(raw_size);
        if (raw != NULL) {
            size_t track;
            size_t track_size = msa_track_size(&msa);
            for (track = 0u; track < msa_track_count(&msa); ++track) {
                uint16_t tracks = (uint16_t)(track / msa.geometry.sides);
                uint16_t side = (uint16_t)(track % msa.geometry.sides);
                const uint8_t *source = msa_track_data(
                    &msa, (uint16_t)(tracks + msa.geometry.start_track), side,
                    NULL);
                if (source == NULL) break;
                memcpy(raw + track * track_size, source, track_size);
            }
            ok = track == msa_track_count(&msa);
        }
        msa_image_free(&msa);
        if (!ok) {
            free(raw);
            stout_disk_error(error_text, error_size, "invalid MSA image");
            return 0;
        }
    } else if (dot != NULL && strcasecmp(dot, ".st") == 0) {
        if (!stout_disk_read_file(path, &raw, &raw_size, error_text,
                                  error_size)) return 0;
        disk->writable = 1;
    } else {
        stout_disk_error(error_text, error_size, "not an ST or MSA image");
        return 0;
    }
    if (raw_size == 0u || (raw_size % 512u) != 0u) {
        free(raw);
        stout_disk_error(error_text, error_size, "disk image is not sector aligned");
        return 0;
    }
    disk->data = raw;
    disk->size = raw_size;
    disk->sector_count = (uint32_t)(raw_size / 512u);
    if (snprintf(disk->path, sizeof(disk->path), "%s", path) >=
        (int)sizeof(disk->path)) {
        stout_disk_free(disk);
        stout_disk_error(error_text, error_size, "disk image path is too long");
        return 0;
    }
    fat12_memory_disk_init(&disk->memory, disk->data, disk->size);
    {
        fat12_disk_t provider = {&disk->memory, fat12_memory_disk_read_sector};
        char fat_error[FAT12_ERROR_LENGTH];
        disk->fat12_open = fat12_image_open(&disk->fat12, &provider,
                                             fat_error, sizeof(fat_error));
    }
    return 1;
}

int stout_disk_read_sector(const stout_disk_t *disk, uint32_t sector,
                           uint8_t *buffer, size_t buffer_size)
{
    if (disk == NULL || buffer == NULL || buffer_size < 512u ||
        sector >= disk->sector_count) return 0;
    memcpy(buffer, disk->data + (size_t)sector * 512u, 512u);
    return 1;
}

const fat12_image_t *stout_disk_fat12(const stout_disk_t *disk)
{
    return disk != NULL && disk->fat12_open ? &disk->fat12 : NULL;
}

typedef struct stout_disk_find_context {
    const char *wanted;
    fat12_dirent_t entry;
    int found;
} stout_disk_find_context_t;

typedef struct stout_disk_directory_context {
    const char *wanted;
    int found;
} stout_disk_directory_context_t;

static int stout_disk_find_callback(const fat12_dirent_t *entry,
                                    const char *path, void *context)
{
    stout_disk_find_context_t *find = context;

    if ((entry->attr & (FAT12_ATTR_DIRECTORY | FAT12_ATTR_VOLUME)) == 0u &&
        strcasecmp(path, find->wanted) == 0) {
        find->entry = *entry;
        find->found = 1;
        return 0;
    }
    return 1;
}

static int stout_disk_directory_callback(const fat12_dirent_t *entry,
                                         const char *path, void *context)
{
    stout_disk_directory_context_t *find = context;

    if ((entry->attr & FAT12_ATTR_DIRECTORY) != 0u &&
        strcasecmp(path, find->wanted) == 0) {
        find->found = 1;
        return 0;
    }
    return 1;
}

int stout_disk_directory_exists(const stout_disk_t *disk, const char *path)
{
    stout_disk_directory_context_t find;
    char normalized[256];
    char error_text[FAT12_ERROR_LENGTH];
    size_t index;

    if (disk == NULL || !disk->fat12_open || path == NULL)
        return 0;
    while (*path == '/' || *path == '\\')
        ++path;
    if (*path == '\0')
        return 1;
    if (strlen(path) >= sizeof(normalized))
        return 0;
    for (index = 0u; path[index] != '\0'; ++index)
        normalized[index] = path[index] == '\\' ? '/' : path[index];
    normalized[index] = '\0';
    find.wanted = normalized;
    find.found = 0;
    return fat12_walk_root(&disk->fat12, stout_disk_directory_callback, &find,
                           error_text, sizeof(error_text)) && find.found;
}

int stout_disk_read_path(const stout_disk_t *disk, const char *path,
                         uint8_t **data_out, size_t *size_out)
{
    stout_disk_find_context_t find;
    char normalized[256];
    size_t index;
    fat12_dirent_t entry;
    char error_text[FAT12_ERROR_LENGTH];

    if (disk == NULL || !disk->fat12_open || path == NULL ||
        data_out == NULL || size_out == NULL) {
        return 0;
    }
    while (*path == '/' || *path == '\\') ++path;
    if (strlen(path) >= sizeof(normalized)) return 0;
    for (index = 0u; path[index] != '\0'; ++index)
        normalized[index] = path[index] == '\\' ? '/' : path[index];
    normalized[index] = '\0';
    find.wanted = normalized;
    memset(&find.entry, 0, sizeof(find.entry));
    find.found = 0;
    if (!fat12_walk_root(&disk->fat12, stout_disk_find_callback, &find,
                         error_text, sizeof(error_text)) ||
        find.found == 0) {
        return 0;
    }
    entry = find.entry;
    return fat12_read_file_alloc(&disk->fat12, &entry, data_out, size_out,
                                 error_text, sizeof(error_text));
}
