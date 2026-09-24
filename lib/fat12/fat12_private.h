/*
 * Declares the FAT12 library's internal constants, traversal budget and
 * the sector, cluster and error helpers shared by its image and directory
 * modules. Not part of the public fat12.h interface.
 *
 * MIT License (see: LICENSE)
 * Copyright (C) 2026 tomaz stih
 */

#ifndef GEM_FAT12_PRIVATE_H
#define GEM_FAT12_PRIVATE_H

#include "fat12/fat12.h"

#include <stddef.h>
#include <stdint.h>

enum {
    fat12_attr_long_name = 0x0fu,
    fat12_cluster_end_min = 0x0ff8u,
    fat12_cluster_bad = 0x0ff7u,
    fat12_boot_sector_size = 512u
};

/* One traversal budget bounds hostile cycles and deeply nested images. */
typedef struct fat12_walk_state {
    unsigned char visited[4096];
    size_t depth;
    size_t entries;
    int stopped;
} fat12_walk_state_t;

/* Byte offset of a data cluster. */
uint32_t fat12_cluster_offset(const fat12_image_t *image, uint16_t cluster);
/* Next cluster in a chain, or an end/bad marker. */
uint16_t fat12_next_cluster(const fat12_image_t *image, uint16_t cluster);
/* Read a byte span from the image with bounds checks. */
int fat12_read_bytes(const fat12_image_t *image, uint32_t byte_offset,
                     uint8_t *buffer, size_t byte_count);
/* Read a little-endian 16-bit value. */
uint16_t fat12_read_le16(const uint8_t *data);
/* Read a little-endian 32-bit value. */
uint32_t fat12_read_le32(const uint8_t *data);
/* Record an error message in the image state. */
void fat12_set_error(char *error_text, size_t error_size, const char *message);

#endif
