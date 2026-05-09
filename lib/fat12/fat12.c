/*
 * Implements a read-only FAT12 helper on top of a minimal sector-read
 * interface. The code parses the BPB, reads FAT12 cluster chains, and
 * walks short-name directory trees without assuming any particular disk
 * container format.
 *
 * MIT License (see: LICENSE)
 * Copyright (C) 2026 tomaz stih
 */

#include "fat12/fat12.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum {
    fat12_attr_long_name = 0x0fu,
    fat12_cluster_end_min = 0x0ff8u,
    fat12_cluster_bad = 0x0ff7u,
    fat12_boot_sector_size = 512u
};

static void fat12_set_error(char *error_text, size_t error_size,
    const char *message);
static uint16_t fat12_read_le16(const uint8_t *data);
static uint32_t fat12_read_le32(const uint8_t *data);
static int fat12_read_sector_data(const fat12_image_t *image,
    uint32_t sector_index, uint8_t *buffer, size_t buffer_size);
static int fat12_read_bytes(const fat12_image_t *image, uint32_t byte_offset,
    uint8_t *buffer, size_t byte_count);
static void fat12_trim_spaces(char *text);
static void fat12_format_name(const uint8_t entry[32], char *name_out,
    size_t name_size);
static int fat12_is_dot_entry(const fat12_dirent_t *entry);
static int fat12_read_entry(const uint8_t entry_bytes[32], fat12_dirent_t *entry);
static uint32_t fat12_cluster_offset(const fat12_image_t *image, uint16_t cluster);
static uint16_t fat12_next_cluster(const fat12_image_t *image, uint16_t cluster);
static int fat12_walk_directory_region(const fat12_image_t *image,
    const uint8_t *dir_data, size_t dir_size, const char *prefix,
    fat12_walk_fn callback, void *context,
    char *error_text, size_t error_size);
static int fat12_walk_cluster_directory(const fat12_image_t *image,
    uint16_t first_cluster, const char *prefix,
    fat12_walk_fn callback, void *context,
    char *error_text, size_t error_size);

void fat12_memory_disk_init(fat12_memory_disk_t *memory_disk,
    const uint8_t *data, size_t size)
{
    if (memory_disk == NULL) {
        return;
    }
    memory_disk->data = data;
    memory_disk->size = size;
}

int fat12_memory_disk_read_sector(void *context, uint32_t sector_index,
    uint8_t *buffer, size_t buffer_size)
{
    fat12_memory_disk_t *memory_disk = context;
    uint32_t byte_offset;

    if (memory_disk == NULL || buffer == NULL || buffer_size == 0u) {
        return 0;
    }
    byte_offset = sector_index * (uint32_t) buffer_size;
    if ((size_t) byte_offset + buffer_size > memory_disk->size) {
        return 0;
    }
    memcpy(buffer, memory_disk->data + byte_offset, buffer_size);
    return 1;
}

int fat12_image_open(fat12_image_t *image, const fat12_disk_t *disk,
    char *error_text, size_t error_size)
{
    uint8_t boot_sector[fat12_boot_sector_size];
    uint32_t root_dir_sectors;

    if (image == NULL || disk == NULL || disk->read_sector == NULL) {
        fat12_set_error(error_text, error_size, "invalid FAT12 disk interface");
        return 0;
    }
    if (!disk->read_sector(disk->context, 0u, boot_sector, sizeof(boot_sector))) {
        fat12_set_error(error_text, error_size, "failed to read boot sector");
        return 0;
    }

    memset(image, 0, sizeof(*image));
    image->disk = *disk;
    image->bpb.bytes_per_sector = fat12_read_le16(boot_sector + 11);
    image->bpb.sectors_per_cluster = boot_sector[13];
    image->bpb.reserved_sectors = fat12_read_le16(boot_sector + 14);
    image->bpb.fat_count = boot_sector[16];
    image->bpb.root_entries = fat12_read_le16(boot_sector + 17);
    image->bpb.total_sectors = fat12_read_le16(boot_sector + 19);
    image->bpb.sectors_per_fat = fat12_read_le16(boot_sector + 22);
    image->bpb.sectors_per_track = fat12_read_le16(boot_sector + 24);
    image->bpb.sides = fat12_read_le16(boot_sector + 26);

    if (image->bpb.bytes_per_sector != fat12_boot_sector_size ||
        image->bpb.sectors_per_cluster == 0 ||
        image->bpb.fat_count == 0 ||
        image->bpb.sectors_per_fat == 0) {
        fat12_set_error(error_text, error_size, "invalid or unsupported FAT12 BPB");
        return 0;
    }

    root_dir_sectors = ((uint32_t) image->bpb.root_entries * 32u +
        (uint32_t) image->bpb.bytes_per_sector - 1u) /
        (uint32_t) image->bpb.bytes_per_sector;
    image->bpb.root_dir_sector = (uint32_t) image->bpb.reserved_sectors +
        (uint32_t) image->bpb.fat_count * (uint32_t) image->bpb.sectors_per_fat;
    image->bpb.root_dir_sector_count = root_dir_sectors;
    image->bpb.data_sector = image->bpb.root_dir_sector + root_dir_sectors;
    image->bpb.cluster_size = (uint32_t) image->bpb.bytes_per_sector *
        (uint32_t) image->bpb.sectors_per_cluster;

    return 1;
}

int fat12_walk_root(const fat12_image_t *image, fat12_walk_fn callback,
    void *context, char *error_text, size_t error_size)
{
    uint8_t *root_dir;
    size_t root_size;
    int ok;

    if (image == NULL || callback == NULL) {
        fat12_set_error(error_text, error_size, "invalid FAT12 walk arguments");
        return 0;
    }

    root_size = (size_t) image->bpb.root_dir_sector_count *
        (size_t) image->bpb.bytes_per_sector;
    root_dir = malloc(root_size);
    if (root_dir == NULL) {
        fat12_set_error(error_text, error_size, "out of memory");
        return 0;
    }
    ok = fat12_read_bytes(image,
        image->bpb.root_dir_sector * (uint32_t) image->bpb.bytes_per_sector,
        root_dir,
        root_size);
    if (!ok) {
        free(root_dir);
        fat12_set_error(error_text, error_size, "failed to read FAT12 root directory");
        return 0;
    }

    ok = fat12_walk_directory_region(image, root_dir, root_size, "",
        callback, context, error_text, error_size);
    free(root_dir);
    return ok;
}

int fat12_read_file(const fat12_image_t *image, const fat12_dirent_t *entry,
    uint8_t *buffer, size_t buffer_size,
    char *error_text, size_t error_size)
{
    uint16_t cluster;
    size_t copied = 0;
    uint16_t guard = 0;

    if (image == NULL || entry == NULL || (entry->size > 0 && buffer == NULL)) {
        fat12_set_error(error_text, error_size, "invalid FAT12 read arguments");
        return 0;
    }
    if ((entry->attr & FAT12_ATTR_DIRECTORY) != 0u ||
        (entry->attr & FAT12_ATTR_VOLUME) != 0u) {
        fat12_set_error(error_text, error_size, "entry is not a regular file");
        return 0;
    }
    if (buffer_size < entry->size) {
        fat12_set_error(error_text, error_size, "buffer is too small for file");
        return 0;
    }
    if (entry->size == 0u) {
        return 1;
    }

    cluster = entry->first_cluster;
    while (copied < entry->size) {
        size_t chunk = entry->size - copied;

        if (cluster < 2u || cluster >= fat12_cluster_end_min) {
            fat12_set_error(error_text, error_size,
                "FAT12 cluster chain ended before file data");
            return 0;
        }
        if (chunk > image->bpb.cluster_size) {
            chunk = image->bpb.cluster_size;
        }
        if (!fat12_read_bytes(image, fat12_cluster_offset(image, cluster),
                buffer + copied, chunk)) {
            fat12_set_error(error_text, error_size,
                "failed to read FAT12 file cluster");
            return 0;
        }
        copied += chunk;
        if (copied >= entry->size) {
            break;
        }

        cluster = fat12_next_cluster(image, cluster);
        if (cluster == fat12_cluster_bad) {
            fat12_set_error(error_text, error_size, "invalid FAT12 cluster chain");
            return 0;
        }
        if (++guard > 4096u) {
            fat12_set_error(error_text, error_size, "FAT12 cluster loop detected");
            return 0;
        }
    }

    return 1;
}

int fat12_read_file_alloc(const fat12_image_t *image,
    const fat12_dirent_t *entry, uint8_t **data_out, size_t *size_out,
    char *error_text, size_t error_size)
{
    uint8_t *buffer;

    if (data_out == NULL || size_out == NULL) {
        fat12_set_error(error_text, error_size, "invalid FAT12 allocation arguments");
        return 0;
    }
    *data_out = NULL;
    *size_out = 0;

    if (entry == NULL) {
        fat12_set_error(error_text, error_size, "missing FAT12 entry");
        return 0;
    }

    if (entry->size == 0u) {
        buffer = NULL;
    } else {
        buffer = malloc(entry->size);
        if (buffer == NULL) {
            fat12_set_error(error_text, error_size, "out of memory");
            return 0;
        }
    }

    if (!fat12_read_file(image, entry, buffer, entry->size,
            error_text, error_size)) {
        free(buffer);
        return 0;
    }

    *data_out = buffer;
    *size_out = entry->size;
    return 1;
}

void fat12_format_attr(uint8_t attr, char *attr_out, size_t attr_size)
{
    char buffer[8];
    size_t index = 0;

    if (attr_out == NULL || attr_size == 0u) {
        return;
    }

    if ((attr & FAT12_ATTR_DIRECTORY) != 0u) {
        buffer[index++] = 'D';
    }
    if ((attr & FAT12_ATTR_VOLUME) != 0u) {
        buffer[index++] = 'V';
    }
    if ((attr & FAT12_ATTR_READ_ONLY) != 0u) {
        buffer[index++] = 'R';
    }
    if ((attr & FAT12_ATTR_HIDDEN) != 0u) {
        buffer[index++] = 'H';
    }
    if ((attr & FAT12_ATTR_SYSTEM) != 0u) {
        buffer[index++] = 'S';
    }
    if ((attr & FAT12_ATTR_ARCHIVE) != 0u) {
        buffer[index++] = 'A';
    }
    if (index == 0u) {
        buffer[index++] = '-';
    }
    buffer[index] = '\0';
    snprintf(attr_out, attr_size, "%s", buffer);
}

static void fat12_set_error(char *error_text, size_t error_size,
    const char *message)
{
    if (error_text == NULL || error_size == 0u) {
        return;
    }
    if (message == NULL) {
        error_text[0] = '\0';
        return;
    }
    snprintf(error_text, error_size, "%s", message);
}

static uint16_t fat12_read_le16(const uint8_t *data)
{
    return (uint16_t) ((uint16_t) data[0] | ((uint16_t) data[1] << 8));
}

static uint32_t fat12_read_le32(const uint8_t *data)
{
    return (uint32_t) data[0] |
        ((uint32_t) data[1] << 8) |
        ((uint32_t) data[2] << 16) |
        ((uint32_t) data[3] << 24);
}

static int fat12_read_sector_data(const fat12_image_t *image,
    uint32_t sector_index, uint8_t *buffer, size_t buffer_size)
{
    if (image == NULL || buffer == NULL ||
        buffer_size != (size_t) image->bpb.bytes_per_sector) {
        return 0;
    }
    return image->disk.read_sector(image->disk.context, sector_index,
        buffer, buffer_size);
}

static int fat12_read_bytes(const fat12_image_t *image, uint32_t byte_offset,
    uint8_t *buffer, size_t byte_count)
{
    uint8_t *sector_buffer;
    uint32_t sector_size;
    size_t copied = 0;

    if (image == NULL || buffer == NULL) {
        return 0;
    }

    sector_size = image->bpb.bytes_per_sector;
    sector_buffer = malloc(sector_size);
    if (sector_buffer == NULL) {
        return 0;
    }

    while (copied < byte_count) {
        uint32_t sector_index = (byte_offset + (uint32_t) copied) / sector_size;
        uint32_t sector_offset = (byte_offset + (uint32_t) copied) % sector_size;
        size_t chunk = byte_count - copied;

        if (chunk > sector_size - sector_offset) {
            chunk = sector_size - sector_offset;
        }
        if (!fat12_read_sector_data(image, sector_index, sector_buffer, sector_size)) {
            free(sector_buffer);
            return 0;
        }
        memcpy(buffer + copied, sector_buffer + sector_offset, chunk);
        copied += chunk;
    }

    free(sector_buffer);
    return 1;
}

static void fat12_trim_spaces(char *text)
{
    size_t length;

    if (text == NULL) {
        return;
    }
    length = strlen(text);
    while (length > 0u && text[length - 1u] == ' ') {
        text[length - 1u] = '\0';
        --length;
    }
}

static void fat12_format_name(const uint8_t entry[32], char *name_out,
    size_t name_size)
{
    char base[9];
    char ext[4];

    if (name_out == NULL || name_size == 0u) {
        return;
    }

    memcpy(base, entry, 8u);
    memcpy(ext, entry + 8, 3u);
    base[8] = '\0';
    ext[3] = '\0';
    fat12_trim_spaces(base);
    fat12_trim_spaces(ext);

    if (ext[0] != '\0') {
        snprintf(name_out, name_size, "%s.%s", base, ext);
    } else {
        snprintf(name_out, name_size, "%s", base);
    }
}

static int fat12_is_dot_entry(const fat12_dirent_t *entry)
{
    return entry != NULL &&
        (strcmp(entry->name, ".") == 0 || strcmp(entry->name, "..") == 0);
}

static int fat12_read_entry(const uint8_t entry_bytes[32], fat12_dirent_t *entry)
{
    if (entry_bytes == NULL || entry == NULL) {
        return 0;
    }
    if (entry_bytes[0] == 0x00u || entry_bytes[0] == 0xe5u ||
        entry_bytes[11] == fat12_attr_long_name) {
        return 0;
    }

    memset(entry, 0, sizeof(*entry));
    fat12_format_name(entry_bytes, entry->name, sizeof(entry->name));
    entry->attr = entry_bytes[11];
    entry->first_cluster = fat12_read_le16(entry_bytes + 26);
    entry->size = fat12_read_le32(entry_bytes + 28);
    return 1;
}

static uint32_t fat12_cluster_offset(const fat12_image_t *image, uint16_t cluster)
{
    return (image->bpb.data_sector +
        (uint32_t) (cluster - 2u) * (uint32_t) image->bpb.sectors_per_cluster) *
        (uint32_t) image->bpb.bytes_per_sector;
}

static uint16_t fat12_next_cluster(const fat12_image_t *image, uint16_t cluster)
{
    uint8_t bytes[2];
    uint32_t fat_offset = (uint32_t) cluster + ((uint32_t) cluster / 2u);
    uint16_t value;

    if (!fat12_read_bytes(image,
            (uint32_t) image->bpb.reserved_sectors *
                (uint32_t) image->bpb.bytes_per_sector + fat_offset,
            bytes,
            sizeof(bytes))) {
        return fat12_cluster_bad;
    }

    value = fat12_read_le16(bytes);
    if ((cluster & 1u) == 0u) {
        return (uint16_t) (value & 0x0fffu);
    }
    return (uint16_t) (value >> 4);
}

static int fat12_walk_directory_region(const fat12_image_t *image,
    const uint8_t *dir_data, size_t dir_size, const char *prefix,
    fat12_walk_fn callback, void *context,
    char *error_text, size_t error_size)
{
    size_t offset;

    for (offset = 0; offset + 32u <= dir_size; offset += 32u) {
        const uint8_t *entry_bytes = dir_data + offset;
        fat12_dirent_t entry;
        char path[256];

        if (entry_bytes[0] == 0x00u) {
            break;
        }
        if (!fat12_read_entry(entry_bytes, &entry)) {
            continue;
        }

        if (prefix != NULL && prefix[0] != '\0') {
            snprintf(path, sizeof(path), "%s/%s", prefix, entry.name);
        } else {
            snprintf(path, sizeof(path), "%s", entry.name);
        }

        if (!callback(&entry, path, context)) {
            return 1;
        }
        if ((entry.attr & FAT12_ATTR_DIRECTORY) != 0u &&
            !fat12_is_dot_entry(&entry) &&
            entry.first_cluster >= 2u) {
            if (!fat12_walk_cluster_directory(image, entry.first_cluster, path,
                    callback, context, error_text, error_size)) {
                return 0;
            }
        }
    }

    return 1;
}

static int fat12_walk_cluster_directory(const fat12_image_t *image,
    uint16_t first_cluster, const char *prefix,
    fat12_walk_fn callback, void *context,
    char *error_text, size_t error_size)
{
    uint16_t cluster = first_cluster;
    uint16_t guard = 0;
    uint8_t *dir_data;

    dir_data = malloc(image->bpb.cluster_size);
    if (dir_data == NULL) {
        fat12_set_error(error_text, error_size, "out of memory");
        return 0;
    }

    while (cluster >= 2u && cluster < fat12_cluster_end_min) {
        if (!fat12_read_bytes(image, fat12_cluster_offset(image, cluster),
                dir_data, image->bpb.cluster_size)) {
            free(dir_data);
            fat12_set_error(error_text, error_size,
                "failed to read FAT12 directory cluster");
            return 0;
        }
        if (!fat12_walk_directory_region(image, dir_data,
                image->bpb.cluster_size, prefix,
                callback, context, error_text, error_size)) {
            free(dir_data);
            return 0;
        }

        cluster = fat12_next_cluster(image, cluster);
        if (cluster == fat12_cluster_bad) {
            free(dir_data);
            fat12_set_error(error_text, error_size,
                "invalid FAT12 cluster chain");
            return 0;
        }
        if (++guard > 4096u) {
            free(dir_data);
            fat12_set_error(error_text, error_size,
                "FAT12 cluster loop detected");
            return 0;
        }
    }

    free(dir_data);
    return 1;
}
