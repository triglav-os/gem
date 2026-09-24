/*
 * Declares Atari ST/MSA disk-image access shared by Desktop and Stout.
 *
 * MIT License (see: LICENSE)
 * Copyright (C) 2026 tomaz stih
 */
#ifndef GEM_STOUT_DISK_H
#define GEM_STOUT_DISK_H

#include <fat12/fat12.h>

#include <stddef.h>
#include <stdint.h>

typedef struct stout_disk {
    uint8_t *data;
    size_t size;
    uint32_t sector_count;
    fat12_memory_disk_t memory;
    fat12_image_t fat12;
    int fat12_open;
    char path[1024];
    int writable;
} stout_disk_t;

/* Initializes an empty disk-image object. */
void stout_disk_init(stout_disk_t *disk);
/* Releases a disk image and all parsed FAT12 state. */
void stout_disk_free(stout_disk_t *disk);
/* Opens and validates a raw ST or MSA image, returning nonzero on success. */
int stout_disk_open(stout_disk_t *disk, const char *path,
                    char *error_text, size_t error_size);
/* Reads one 512-byte logical sector into a caller-owned buffer. */
int stout_disk_read_sector(const stout_disk_t *disk, uint32_t sector,
                           uint8_t *buffer, size_t buffer_size);
/* Writes and persists one sector of a writable raw ST image. */
int stout_disk_write_sector(stout_disk_t *disk, uint32_t sector,
                            const uint8_t *buffer, size_t buffer_size);
/* Returns the parsed FAT12 image, or NULL when no filesystem is available. */
const fat12_image_t *stout_disk_fat12(const stout_disk_t *disk);
/* Allocates and returns one file from the image by guest path. */
int stout_disk_read_path(const stout_disk_t *disk, const char *path,
                         uint8_t **data_out, size_t *size_out);
/* Returns nonzero when path names a directory in the mounted image. */
int stout_disk_directory_exists(const stout_disk_t *disk, const char *path);
/* Creates or replaces a file in a writable raw ST image. */
int stout_disk_write_path(stout_disk_t *disk, const char *path,
                          const uint8_t *data, size_t size, uint8_t attr);
/* Deletes a file from a writable raw ST image. */
int stout_disk_delete_path(stout_disk_t *disk, const char *path);
/* Renames or moves a file in a writable raw ST image. */
int stout_disk_rename_path(stout_disk_t *disk, const char *old_path,
                           const char *new_path);
/* Creates a directory in a writable raw ST image. */
int stout_disk_create_directory(stout_disk_t *disk, const char *path);
/* Reads a file's packed GEMDOS time and date words. */
int stout_disk_get_datetime(stout_disk_t *disk, const char *path,
                            uint16_t *time_out, uint16_t *date_out);
/* Persists a file's packed GEMDOS time and date words. */
int stout_disk_set_datetime(stout_disk_t *disk, const char *path,
                            uint16_t time, uint16_t date);
/* Counts currently unallocated FAT12 data clusters. */
uint32_t stout_disk_free_clusters(const stout_disk_t *disk);

#endif
