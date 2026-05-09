/*
 * Declares a small reusable FAT12 floppy reader built around a minimal
 * disk-provider interface. Any backing format can be used as long as it
 * can supply logical sector reads, which lets callers layer FAT12 over
 * decoded `.MSA`, raw `.ST`, future `.STX`, or other disk sources.
 *
 * MIT License (see: LICENSE)
 * Copyright (C) 2026 tomaz stih
 */

#ifndef FAT12_FAT12_H
#define FAT12_FAT12_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define FAT12_ERROR_LENGTH 256u

#define FAT12_ATTR_READ_ONLY 0x01u
#define FAT12_ATTR_HIDDEN    0x02u
#define FAT12_ATTR_SYSTEM    0x04u
#define FAT12_ATTR_VOLUME    0x08u
#define FAT12_ATTR_DIRECTORY 0x10u
#define FAT12_ATTR_ARCHIVE   0x20u

typedef int (*fat12_read_sector_fn)(void *context, uint32_t sector_index,
    uint8_t *buffer, size_t buffer_size);

typedef struct fat12_disk {
    void *context;
    fat12_read_sector_fn read_sector;
} fat12_disk_t;

typedef struct fat12_memory_disk {
    const uint8_t *data;
    size_t size;
} fat12_memory_disk_t;

typedef struct fat12_bpb {
    uint16_t bytes_per_sector;
    uint8_t sectors_per_cluster;
    uint16_t reserved_sectors;
    uint8_t fat_count;
    uint16_t root_entries;
    uint16_t total_sectors;
    uint16_t sectors_per_fat;
    uint16_t sectors_per_track;
    uint16_t sides;
    uint32_t root_dir_sector;
    uint32_t root_dir_sector_count;
    uint32_t data_sector;
    uint32_t cluster_size;
} fat12_bpb_t;

typedef struct fat12_image {
    fat12_disk_t disk;
    fat12_bpb_t bpb;
} fat12_image_t;

typedef struct fat12_dirent {
    char name[13];
    uint8_t attr;
    uint16_t first_cluster;
    uint32_t size;
} fat12_dirent_t;

typedef int (*fat12_walk_fn)(const fat12_dirent_t *entry,
    const char *path, void *context);

/*
 * Initializes a convenience disk wrapper over a contiguous memory image.
 */
void fat12_memory_disk_init(fat12_memory_disk_t *memory_disk,
    const uint8_t *data, size_t size);

/*
 * Sector callback implementation for a memory-backed disk image.
 */
int fat12_memory_disk_read_sector(void *context, uint32_t sector_index,
    uint8_t *buffer, size_t buffer_size);

/*
 * Opens a FAT12 volume through a caller-supplied disk interface.
 */
int fat12_image_open(fat12_image_t *image, const fat12_disk_t *disk,
    char *error_text, size_t error_size);

/*
 * Walks the whole directory tree starting at the FAT12 root directory.
 * The callback receives a normalized slash-separated path for each
 * entry. Returning zero from the callback stops the walk successfully.
 */
int fat12_walk_root(const fat12_image_t *image, fat12_walk_fn callback,
    void *context, char *error_text, size_t error_size);

/*
 * Reads the contents of one regular file entry into a caller-supplied
 * buffer. The buffer size must be at least `entry->size`.
 */
int fat12_read_file(const fat12_image_t *image, const fat12_dirent_t *entry,
    uint8_t *buffer, size_t buffer_size,
    char *error_text, size_t error_size);

/*
 * Allocates and reads the contents of one regular file entry.
 * The caller owns the returned buffer and must free it.
 */
int fat12_read_file_alloc(const fat12_image_t *image,
    const fat12_dirent_t *entry, uint8_t **data_out, size_t *size_out,
    char *error_text, size_t error_size);

/*
 * Formats one FAT attribute byte into a short readable flag string such
 * as "D", "V", "RHA", or "-".
 */
void fat12_format_attr(uint8_t attr, char *attr_out, size_t attr_size);

#ifdef __cplusplus
}
#endif

#endif
