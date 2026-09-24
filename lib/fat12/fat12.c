/*
 * Implements a read-only FAT12 helper on top of a minimal sector-read
 * interface: BPB parsing and geometry checks, bounded sector and byte
 * reads, cluster chain following and file extraction. Directory walking
 * lives in fat12_dir.c.
 *
 * MIT License (see: LICENSE)
 * Copyright (C) 2026 tomaz stih
 */

#include "fat12_private.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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
    size_t byte_offset;

    if (memory_disk == NULL || memory_disk->data == NULL || buffer == NULL ||
        buffer_size == 0u) {
        return 0;
    }
    if ((size_t)sector_index > SIZE_MAX / buffer_size)
        return 0;
    byte_offset = (size_t)sector_index * buffer_size;
    if (byte_offset > memory_disk->size ||
        buffer_size > memory_disk->size - byte_offset) {
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
    if (!disk->read_sector(disk->context, 0u, boot_sector,
                           sizeof(boot_sector))) {
        fat12_set_error(error_text, error_size, "failed to read boot sector");
        return 0;
    }

    memset(image, 0, sizeof(*image));
    image->disk = *disk;
    image->bpb.bytes_per_sector = fat12_read_le16(boot_sector + 11);
    image->bpb.sectors_per_cluster = boot_sector[13];
    image->bpb.reserved_sectors = fat12_read_le16(boot_sector + 14);
    /* Atari ST boot sectors omit the reserved-sector count and report zero;
     * sector zero is nevertheless occupied by the boot sector before the
     * first FAT. Normalize that native ST convention to the FAT layout. */
    if (image->bpb.reserved_sectors == 0u) {
        image->bpb.reserved_sectors = 1u;
    }
    image->bpb.fat_count = boot_sector[16];
    image->bpb.root_entries = fat12_read_le16(boot_sector + 17);
    image->bpb.total_sectors = fat12_read_le16(boot_sector + 19);
    image->bpb.sectors_per_fat = fat12_read_le16(boot_sector + 22);
    image->bpb.sectors_per_track = fat12_read_le16(boot_sector + 24);
    image->bpb.sides = fat12_read_le16(boot_sector + 26);

    if (image->bpb.bytes_per_sector != fat12_boot_sector_size ||
        image->bpb.sectors_per_cluster == 0 ||
        (image->bpb.sectors_per_cluster &
         (image->bpb.sectors_per_cluster - 1)) ||
        image->bpb.sectors_per_cluster > 128 || !image->bpb.reserved_sectors ||
        !image->bpb.root_entries || image->bpb.fat_count == 0 ||
        image->bpb.sectors_per_fat == 0) {
        fat12_set_error(error_text, error_size,
                        "invalid or unsupported FAT12 BPB");
        return 0;
    }

    root_dir_sectors = ((uint32_t)image->bpb.root_entries * 32u +
                        (uint32_t)image->bpb.bytes_per_sector - 1u) /
                       (uint32_t)image->bpb.bytes_per_sector;
    image->bpb.root_dir_sector =
        (uint32_t)image->bpb.reserved_sectors +
        (uint32_t)image->bpb.fat_count * (uint32_t)image->bpb.sectors_per_fat;
    image->bpb.root_dir_sector_count = root_dir_sectors;
    image->bpb.data_sector = image->bpb.root_dir_sector + root_dir_sectors;
    image->bpb.cluster_size = (uint32_t)image->bpb.bytes_per_sector *
                              (uint32_t)image->bpb.sectors_per_cluster;

    if (image->bpb.data_sector >= image->bpb.total_sectors) {
        fat12_set_error(error_text, error_size,
                        "FAT12 metadata exceeds volume");
        return 0;
    }
    uint32_t clusters = (image->bpb.total_sectors - image->bpb.data_sector) /
                        image->bpb.sectors_per_cluster;
    if (!clusters || clusters >= 4085u ||
        ((clusters + 2u) * 3u + 1u) / 2u >
            (uint32_t)image->bpb.sectors_per_fat * 512u) {
        fat12_set_error(error_text, error_size,
                        "invalid FAT12 cluster geometry");
        return 0;
    }
    return 1;
}

int fat12_read_file(const fat12_image_t *image, const fat12_dirent_t *entry,
                    uint8_t *buffer, size_t buffer_size, char *error_text,
                    size_t error_size)
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
            fat12_set_error(error_text, error_size,
                            "invalid FAT12 cluster chain");
            return 0;
        }
        if (++guard > 4096u) {
            fat12_set_error(error_text, error_size,
                            "FAT12 cluster loop detected");
            return 0;
        }
    }

    return 1;
}

int fat12_read_file_alloc(const fat12_image_t *image,
                          const fat12_dirent_t *entry, uint8_t **data_out,
                          size_t *size_out, char *error_text, size_t error_size)
{
    uint8_t *buffer;

    if (data_out == NULL || size_out == NULL) {
        fat12_set_error(error_text, error_size,
                        "invalid FAT12 allocation arguments");
        return 0;
    }
    *data_out = NULL;
    *size_out = 0;

    if (entry == NULL || image == NULL ||
        entry->size > (uint32_t)image->bpb.total_sectors * 512u) {
        fat12_set_error(error_text, error_size,
                        "file size exceeds FAT12 volume");
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

    if (!fat12_read_file(image, entry, buffer, entry->size, error_text,
                         error_size)) {
        free(buffer);
        return 0;
    }

    *data_out = buffer;
    *size_out = entry->size;
    return 1;
}

void fat12_set_error(char *error_text, size_t error_size, const char *message)
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

uint16_t fat12_read_le16(const uint8_t *data)
{
    return (uint16_t)((uint16_t)data[0] | ((uint16_t)data[1] << 8));
}

uint32_t fat12_read_le32(const uint8_t *data)
{
    return (uint32_t)data[0] | ((uint32_t)data[1] << 8) |
           ((uint32_t)data[2] << 16) | ((uint32_t)data[3] << 24);
}

static int fat12_read_sector_data(const fat12_image_t *image,
                                  uint32_t sector_index, uint8_t *buffer,
                                  size_t buffer_size)
{
    if (image == NULL || buffer == NULL ||
        buffer_size != (size_t)image->bpb.bytes_per_sector ||
        sector_index >= image->bpb.total_sectors) {
        return 0;
    }
    return image->disk.read_sector(image->disk.context, sector_index, buffer,
                                   buffer_size);
}

int fat12_read_bytes(const fat12_image_t *image, uint32_t byte_offset,
                     uint8_t *buffer, size_t byte_count)
{
    uint8_t *sector_buffer;
    uint32_t sector_size;
    size_t copied = 0;

    if (image == NULL || buffer == NULL) {
        return 0;
    }

    sector_size = image->bpb.bytes_per_sector;
    size_t volume_size = (size_t)image->bpb.total_sectors * sector_size;
    if (!sector_size || byte_offset > volume_size ||
        byte_count > volume_size - byte_offset)
        return 0;
    sector_buffer = malloc(sector_size);
    if (sector_buffer == NULL) {
        return 0;
    }

    while (copied < byte_count) {
        uint32_t sector_index = (byte_offset + (uint32_t)copied) / sector_size;
        uint32_t sector_offset = (byte_offset + (uint32_t)copied) % sector_size;
        size_t chunk = byte_count - copied;

        if (chunk > sector_size - sector_offset) {
            chunk = sector_size - sector_offset;
        }
        if (!fat12_read_sector_data(image, sector_index, sector_buffer,
                                    sector_size)) {
            free(sector_buffer);
            return 0;
        }
        memcpy(buffer + copied, sector_buffer + sector_offset, chunk);
        copied += chunk;
    }

    free(sector_buffer);
    return 1;
}

uint32_t fat12_cluster_offset(const fat12_image_t *image, uint16_t cluster)
{
    return (image->bpb.data_sector +
            (uint32_t)(cluster - 2u) *
                (uint32_t)image->bpb.sectors_per_cluster) *
           (uint32_t)image->bpb.bytes_per_sector;
}

uint16_t fat12_next_cluster(const fat12_image_t *image, uint16_t cluster)
{
    uint8_t bytes[2];
    uint32_t fat_offset = (uint32_t)cluster + ((uint32_t)cluster / 2u);
    uint16_t value;

    if (!fat12_read_bytes(image,
                          (uint32_t)image->bpb.reserved_sectors *
                                  (uint32_t)image->bpb.bytes_per_sector +
                              fat_offset,
                          bytes, sizeof(bytes))) {
        return fat12_cluster_bad;
    }

    value = fat12_read_le16(bytes);
    if ((cluster & 1u) == 0u) {
        return (uint16_t)(value & 0x0fffu);
    }
    return (uint16_t)(value >> 4);
}
