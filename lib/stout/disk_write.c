/*
 * Implements bounded FAT12 file and directory updates for writable raw ST
 * images. MSA images remain read-only because persisting them requires
 * re-encoding.
 *
 * MIT License (see: LICENSE)
 * Copyright (C) 2026 tomaz stih
 */

#include "stout/disk.h"

#include "platform/os.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

static uint16_t stout_disk_le16(const uint8_t *value)
{
    return (uint16_t)((uint16_t)value[0] | ((uint16_t)value[1] << 8));
}

static void stout_disk_put_le16(uint8_t *target, uint16_t value)
{
    target[0] = (uint8_t)(value & 0xffu);
    target[1] = (uint8_t)(value >> 8);
}

static void stout_disk_put_le32(uint8_t *target, uint32_t value)
{
    target[0] = (uint8_t)(value & 0xffu);
    target[1] = (uint8_t)((value >> 8) & 0xffu);
    target[2] = (uint8_t)((value >> 16) & 0xffu);
    target[3] = (uint8_t)(value >> 24);
}

static int stout_disk_name(const char *path, size_t length, uint8_t name[11])
{
    const char *dot;
    size_t base_length;
    size_t extension_length = 0u;
    size_t index;

    if (path == NULL || length == 0u) return 0;
    dot = memchr(path, '.', length);
    base_length = dot != NULL ? (size_t)(dot - path) : length;
    if (dot != NULL) extension_length = length - base_length - 1u;
    if (base_length == 0u || base_length > 8u || extension_length > 3u)
        return 0;
    memset(name, ' ', 11u);
    for (index = 0u; index < base_length; ++index) {
        unsigned char value = (unsigned char)path[index];

        if (value <= 32u || strchr("\"*+,/:;<=>?[\\]|", value) != NULL)
            return 0;
        name[index] = (uint8_t)toupper(value);
    }
    for (index = 0u; index < extension_length; ++index) {
        unsigned char value = (unsigned char)dot[index + 1u];

        if (value <= 32u || strchr("\"*+,/:;<=>?[\\]|", value) != NULL)
            return 0;
        name[8u + index] = (uint8_t)toupper(value);
    }
    return 1;
}

static uint8_t *stout_disk_root(stout_disk_t *disk)
{
    size_t offset;
    size_t size;

    if (disk == NULL || !disk->fat12_open) return NULL;
    offset = (size_t)disk->fat12.bpb.root_dir_sector *
             disk->fat12.bpb.bytes_per_sector;
    size = (size_t)disk->fat12.bpb.root_entries * 32u;
    if (offset > disk->size || size > disk->size - offset) return NULL;
    return disk->data + offset;
}

static uint16_t stout_disk_fat_get(const stout_disk_t *disk, uint16_t cluster)
{
    const fat12_bpb_t *bpb = &disk->fat12.bpb;
    size_t offset = (size_t)bpb->reserved_sectors * bpb->bytes_per_sector +
                    cluster + cluster / 2u;
    uint16_t value;

    if (offset + 2u > disk->size) return 0xff7u;
    value = stout_disk_le16(disk->data + offset);
    return (cluster & 1u) != 0u ? (uint16_t)(value >> 4)
                                : (uint16_t)(value & 0x0fffu);
}

static int stout_disk_fat_set(stout_disk_t *disk, uint16_t cluster,
                              uint16_t value)
{
    const fat12_bpb_t *bpb = &disk->fat12.bpb;
    uint8_t copy;

    value &= 0x0fffu;
    for (copy = 0u; copy < bpb->fat_count; ++copy) {
        size_t offset = ((size_t)bpb->reserved_sectors +
                         (size_t)copy * bpb->sectors_per_fat) *
                            bpb->bytes_per_sector +
                        cluster + cluster / 2u;
        uint16_t pair;

        if (offset + 2u > disk->size) return 0;
        pair = stout_disk_le16(disk->data + offset);
        if ((cluster & 1u) != 0u)
            pair = (uint16_t)((pair & 0x000fu) | (value << 4));
        else
            pair = (uint16_t)((pair & 0xf000u) | value);
        stout_disk_put_le16(disk->data + offset, pair);
    }
    return 1;
}

static uint16_t stout_disk_cluster_limit(const stout_disk_t *disk)
{
    const fat12_bpb_t *bpb = &disk->fat12.bpb;
    uint32_t count = (bpb->total_sectors - bpb->data_sector) /
                     bpb->sectors_per_cluster;

    return (uint16_t)(count + 2u);
}

static int stout_disk_free_chain(stout_disk_t *disk, uint16_t cluster)
{
    uint16_t limit = stout_disk_cluster_limit(disk);
    uint16_t guard = 0u;

    while (cluster >= 2u && cluster < 0x0ff8u && cluster < limit) {
        uint16_t next = stout_disk_fat_get(disk, cluster);

        if (!stout_disk_fat_set(disk, cluster, 0u)) return 0;
        cluster = next;
        if (++guard > limit) return 0;
    }
    return 1;
}

static uint16_t stout_disk_alloc_cluster(stout_disk_t *disk)
{
    uint16_t cluster;
    uint16_t limit = stout_disk_cluster_limit(disk);

    for (cluster = 2u; cluster < limit; ++cluster)
        if (stout_disk_fat_get(disk, cluster) == 0u) return cluster;
    return 0u;
}

static uint8_t *stout_disk_cluster(stout_disk_t *disk, uint16_t cluster)
{
    size_t offset;

    if (cluster < 2u || cluster >= stout_disk_cluster_limit(disk)) return NULL;
    offset = ((size_t)disk->fat12.bpb.data_sector +
              (size_t)(cluster - 2u) *
                  disk->fat12.bpb.sectors_per_cluster) *
             disk->fat12.bpb.bytes_per_sector;
    if (offset > disk->size ||
        disk->fat12.bpb.cluster_size > disk->size - offset) return NULL;
    return disk->data + offset;
}

static uint8_t *stout_disk_find_region(uint8_t *region, size_t size,
                                       const uint8_t name[11],
                                       uint8_t **free_entry, int *at_end)
{
    size_t index;

    if (at_end != NULL) *at_end = 0;
    for (index = 0u; index + 32u <= size; index += 32u) {
        uint8_t *entry = region + index;

        if (entry[0] == 0x00u || entry[0] == 0xe5u) {
            if (free_entry != NULL && *free_entry == NULL) *free_entry = entry;
            if (entry[0] == 0x00u) {
                if (at_end != NULL) *at_end = 1;
                return NULL;
            }
        } else if (entry[11] != 0x0fu && memcmp(entry, name, 11u) == 0) {
            return entry;
        }
    }
    return NULL;
}

/* A zero directory cluster denotes the fixed root directory. */
static uint8_t *stout_disk_find_entry(stout_disk_t *disk,
                                      uint16_t directory_cluster,
                                      const uint8_t name[11],
                                      uint8_t **free_entry,
                                      int extend_directory)
{
    uint16_t cluster;
    uint16_t previous = 0u;
    uint16_t guard = 0u;
    uint16_t limit = stout_disk_cluster_limit(disk);
    uint8_t *found;

    if (free_entry != NULL) *free_entry = NULL;
    if (directory_cluster == 0u) {
        uint8_t *root = stout_disk_root(disk);

        return root != NULL
                   ? stout_disk_find_region(
                         root, (size_t)disk->fat12.bpb.root_entries * 32u,
                         name, free_entry, NULL)
                   : NULL;
    }
    cluster = directory_cluster;
    while (cluster >= 2u && cluster < 0x0ff8u && cluster < limit) {
        uint8_t *region = stout_disk_cluster(disk, cluster);
        uint16_t next;
        int at_end;

        if (region == NULL) return NULL;
        found = stout_disk_find_region(region, disk->fat12.bpb.cluster_size,
                                       name, free_entry, &at_end);
        if (found != NULL || at_end != 0) return found;
        previous = cluster;
        next = stout_disk_fat_get(disk, cluster);
        if (next >= 0x0ff8u) break;
        cluster = next;
        if (++guard > limit) return NULL;
    }
    if (free_entry != NULL && *free_entry != NULL) return NULL;
    if (!extend_directory || free_entry == NULL || previous == 0u) return NULL;
    cluster = stout_disk_alloc_cluster(disk);
    if (cluster == 0u || !stout_disk_fat_set(disk, previous, cluster) ||
        !stout_disk_fat_set(disk, cluster, 0x0fffu)) return NULL;
    *free_entry = stout_disk_cluster(disk, cluster);
    if (*free_entry != NULL)
        memset(*free_entry, 0, disk->fat12.bpb.cluster_size);
    return NULL;
}

static int stout_disk_make_directory(stout_disk_t *disk, uint16_t parent,
                                     const uint8_t name[11],
                                     uint16_t *cluster_out)
{
    uint8_t *entry;
    uint8_t *free_entry;
    uint8_t *region;
    uint16_t cluster;

    entry = stout_disk_find_entry(disk, parent, name, &free_entry, 1);
    if (entry != NULL) {
        if ((entry[11] & FAT12_ATTR_DIRECTORY) == 0u) return 0;
        *cluster_out = stout_disk_le16(entry + 26u);
        return *cluster_out >= 2u;
    }
    if (free_entry == NULL) return 0;
    cluster = stout_disk_alloc_cluster(disk);
    if (cluster == 0u || !stout_disk_fat_set(disk, cluster, 0x0fffu)) return 0;
    region = stout_disk_cluster(disk, cluster);
    if (region == NULL) return 0;
    memset(region, 0, disk->fat12.bpb.cluster_size);
    memset(region, ' ', 11u);
    region[0] = '.';
    region[11] = FAT12_ATTR_DIRECTORY;
    stout_disk_put_le16(region + 26u, cluster);
    memset(region + 32u, ' ', 11u);
    region[32u] = '.';
    region[33u] = '.';
    region[43u] = FAT12_ATTR_DIRECTORY;
    stout_disk_put_le16(region + 58u, parent);
    memset(free_entry, 0, 32u);
    memcpy(free_entry, name, 11u);
    free_entry[11] = FAT12_ATTR_DIRECTORY;
    stout_disk_put_le16(free_entry + 26u, cluster);
    *cluster_out = cluster;
    return 1;
}

static int stout_disk_parent(stout_disk_t *disk, const char *path,
                             uint16_t *parent_out, uint8_t leaf[11],
                             int create_missing)
{
    uint16_t parent = 0u;
    const char *cursor = path;

    if (path == NULL || parent_out == NULL) return 0;
    while (*cursor == '/' || *cursor == '\\') ++cursor;
    while (*cursor != '\0') {
        const char *component = cursor;
        size_t length;
        uint8_t name[11];
        uint8_t *entry;

        while (*cursor != '\0' && *cursor != '/' && *cursor != '\\')
            ++cursor;
        length = (size_t)(cursor - component);
        while (*cursor == '/' || *cursor == '\\') ++cursor;
        if (!stout_disk_name(component, length, name)) return 0;
        if (*cursor == '\0') {
            memcpy(leaf, name, 11u);
            *parent_out = parent;
            return 1;
        }
        entry = stout_disk_find_entry(disk, parent, name, NULL, 0);
        if (entry == NULL) {
            if (!create_missing ||
                !stout_disk_make_directory(disk, parent, name, &parent))
                return 0;
        } else {
            if ((entry[11] & FAT12_ATTR_DIRECTORY) == 0u) return 0;
            parent = stout_disk_le16(entry + 26u);
        }
        if (parent < 2u) return 0;
    }
    return 0;
}

static int stout_disk_flush(stout_disk_t *disk)
{
    return disk != NULL && disk->writable != 0 && disk->path[0] != '\0' &&
           gem_os_write_file_atomic(disk->path, disk->data, disk->size) != 0;
}

uint32_t stout_disk_free_clusters(const stout_disk_t *disk)
{
    uint16_t cluster;
    uint16_t limit;
    uint32_t free_clusters = 0u;

    if (disk == NULL || !disk->fat12_open) return 0u;
    limit = stout_disk_cluster_limit(disk);
    for (cluster = 2u; cluster < limit; ++cluster)
        if (stout_disk_fat_get(disk, cluster) == 0u) ++free_clusters;
    return free_clusters;
}

int stout_disk_write_sector(stout_disk_t *disk, uint32_t sector,
                            const uint8_t *buffer, size_t buffer_size)
{
    uint8_t previous[512];
    uint8_t *target;

    if (disk == NULL || disk->writable == 0 || buffer == NULL ||
        buffer_size < 512u || sector >= disk->sector_count) return 0;
    target = disk->data + (size_t)sector * 512u;
    memcpy(previous, target, sizeof(previous));
    memcpy(target, buffer, sizeof(previous));
    if (stout_disk_flush(disk)) return 1;
    memcpy(target, previous, sizeof(previous));
    return 0;
}

int stout_disk_write_path(stout_disk_t *disk, const char *path,
                          const uint8_t *data, size_t size, uint8_t attr)
{
    uint8_t name[11];
    uint8_t *entry;
    uint8_t *free_entry;
    uint8_t *backup;
    uint16_t parent;
    uint16_t first = 0u;
    uint16_t previous = 0u;
    size_t copied = 0u;
    int existed;

    if (disk == NULL || disk->writable == 0 || size > UINT32_MAX ||
        (size != 0u && data == NULL)) return 0;
    backup = malloc(disk->size);
    if (backup == NULL) return 0;
    memcpy(backup, disk->data, disk->size);
    if (!stout_disk_parent(disk, path, &parent, name, 1)) goto fail;
    entry = stout_disk_find_entry(disk, parent, name, &free_entry, 1);
    existed = entry != NULL;
    if (entry != NULL && (entry[11] & FAT12_ATTR_DIRECTORY) != 0u) goto fail;
    if (entry == NULL) entry = free_entry;
    if (entry == NULL) goto fail;
    if (existed &&
        !stout_disk_free_chain(disk, stout_disk_le16(entry + 26u))) goto fail;
    while (copied < size) {
        uint16_t cluster = stout_disk_alloc_cluster(disk);
        size_t chunk = size - copied;
        size_t offset;

        if (cluster == 0u || !stout_disk_fat_set(disk, cluster, 0x0fffu))
            goto fail;
        if (previous != 0u && !stout_disk_fat_set(disk, previous, cluster))
            goto fail;
        if (first == 0u) first = cluster;
        if (chunk > disk->fat12.bpb.cluster_size)
            chunk = disk->fat12.bpb.cluster_size;
        offset = ((size_t)disk->fat12.bpb.data_sector +
                  (size_t)(cluster - 2u) *
                      disk->fat12.bpb.sectors_per_cluster) *
                 disk->fat12.bpb.bytes_per_sector;
        if (offset > disk->size || disk->fat12.bpb.cluster_size >
                                      disk->size - offset) goto fail;
        memset(disk->data + offset, 0, disk->fat12.bpb.cluster_size);
        memcpy(disk->data + offset, data + copied, chunk);
        copied += chunk;
        previous = cluster;
    }
    memset(entry, 0, 32u);
    memcpy(entry, name, 11u);
    entry[11] = (uint8_t)((attr & 0x07u) | FAT12_ATTR_ARCHIVE);
    stout_disk_put_le16(entry + 26u, first);
    stout_disk_put_le32(entry + 28u, (uint32_t)size);
    if (!stout_disk_flush(disk)) goto fail;
    free(backup);
    return 1;

fail:
    memcpy(disk->data, backup, disk->size);
    free(backup);
    return 0;
}

int stout_disk_delete_path(stout_disk_t *disk, const char *path)
{
    uint8_t name[11];
    uint8_t *entry;
    uint8_t *backup;
    uint16_t parent;

    if (disk == NULL || disk->writable == 0 ||
        !stout_disk_parent(disk, path, &parent, name, 0))
        return 0;
    entry = stout_disk_find_entry(disk, parent, name, NULL, 0);
    if (entry == NULL || (entry[11] & FAT12_ATTR_DIRECTORY) != 0u) return 0;
    backup = malloc(disk->size);
    if (backup == NULL) return 0;
    memcpy(backup, disk->data, disk->size);
    if (!stout_disk_free_chain(disk, stout_disk_le16(entry + 26u))) goto fail;
    entry[0] = 0xe5u;
    if (!stout_disk_flush(disk)) goto fail;
    free(backup);
    return 1;

fail:
    memcpy(disk->data, backup, disk->size);
    free(backup);
    return 0;
}

int stout_disk_rename_path(stout_disk_t *disk, const char *old_path,
                           const char *new_path)
{
    uint8_t old_name[11];
    uint8_t new_name[11];
    uint8_t *entry;
    uint8_t *destination;
    uint8_t *free_entry;
    uint8_t *backup;
    uint16_t old_parent;
    uint16_t new_parent;

    if (disk == NULL || disk->writable == 0 ||
        !stout_disk_parent(disk, old_path, &old_parent, old_name, 0) ||
        !stout_disk_parent(disk, new_path, &new_parent, new_name, 0) ||
        stout_disk_find_entry(disk, new_parent, new_name, NULL, 0) != NULL)
        return 0;
    backup = malloc(disk->size);
    if (backup == NULL) return 0;
    memcpy(backup, disk->data, disk->size);
    entry = stout_disk_find_entry(disk, old_parent, old_name, NULL, 0);
    if (entry == NULL ||
        (old_parent != new_parent &&
         (entry[11] & FAT12_ATTR_DIRECTORY) != 0u)) goto fail_rename;
    if (old_parent == new_parent) {
        memcpy(entry, new_name, 11u);
    } else {
        destination = stout_disk_find_entry(disk, new_parent, new_name,
                                            &free_entry, 1);
        if (destination != NULL || free_entry == NULL) goto fail_rename;
        memcpy(free_entry, entry, 32u);
        memcpy(free_entry, new_name, 11u);
        entry[0] = 0xe5u;
    }
    if (!stout_disk_flush(disk)) goto fail_rename;
    free(backup);
    return 1;

fail_rename:
    memcpy(disk->data, backup, disk->size);
    free(backup);
    return 0;
}

int stout_disk_create_directory(stout_disk_t *disk, const char *path)
{
    uint8_t name[11];
    uint8_t *backup;
    uint16_t parent;
    uint16_t cluster;

    if (disk == NULL || disk->writable == 0 ||
        !stout_disk_parent(disk, path, &parent, name, 0) ||
        stout_disk_find_entry(disk, parent, name, NULL, 0) != NULL)
        return 0;
    backup = malloc(disk->size);
    if (backup == NULL) return 0;
    memcpy(backup, disk->data, disk->size);
    if (!stout_disk_make_directory(disk, parent, name, &cluster))
        goto fail_directory;
    if (!stout_disk_flush(disk)) goto fail_directory;
    free(backup);
    return 1;

fail_directory:
    memcpy(disk->data, backup, disk->size);
    free(backup);
    return 0;
}

int stout_disk_get_datetime(stout_disk_t *disk, const char *path,
                            uint16_t *time_out, uint16_t *date_out)
{
    uint8_t name[11];
    uint8_t *entry;
    uint16_t parent;

    if (disk == NULL || time_out == NULL || date_out == NULL ||
        !stout_disk_parent(disk, path, &parent, name, 0)) return 0;
    entry = stout_disk_find_entry(disk, parent, name, NULL, 0);
    if (entry == NULL) return 0;
    *time_out = stout_disk_le16(entry + 22u);
    *date_out = stout_disk_le16(entry + 24u);
    return 1;
}

int stout_disk_set_datetime(stout_disk_t *disk, const char *path,
                            uint16_t time, uint16_t date)
{
    uint8_t name[11];
    uint8_t *entry;
    uint8_t *backup;
    uint16_t parent;

    if (disk == NULL || disk->writable == 0 ||
        !stout_disk_parent(disk, path, &parent, name, 0)) return 0;
    entry = stout_disk_find_entry(disk, parent, name, NULL, 0);
    if (entry == NULL) return 0;
    backup = malloc(disk->size);
    if (backup == NULL) return 0;
    memcpy(backup, disk->data, disk->size);
    stout_disk_put_le16(entry + 22u, time);
    stout_disk_put_le16(entry + 24u, date);
    if (!stout_disk_flush(disk)) {
        memcpy(disk->data, backup, disk->size);
        free(backup);
        return 0;
    }
    free(backup);
    return 1;
}
