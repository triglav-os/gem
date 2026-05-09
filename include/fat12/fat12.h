/*
 * Declares a reusable FAT12 floppy reader built around a minimal
 * sector-provider interface. Callers supply one logical-sector read
 * callback, and this module handles boot-parameter parsing, directory
 * traversal, and file extraction over Atari ST style floppy images such
 * as decoded `.MSA`, raw `.ST`, or future formats with the same logical
 * sector layout.
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

/*
 * Recommended fixed-size buffer length for short human-readable error
 * strings returned by this library.
 */
#define FAT12_ERROR_LENGTH 256u

/*
 * Standard FAT12 directory attribute bits.
 */
#define FAT12_ATTR_READ_ONLY 0x01u  /* Entry is marked read-only. */
#define FAT12_ATTR_HIDDEN    0x02u  /* Entry is hidden from normal views. */
#define FAT12_ATTR_SYSTEM    0x04u  /* Entry is a system file. */
#define FAT12_ATTR_VOLUME    0x08u  /* Entry is a volume label. */
#define FAT12_ATTR_DIRECTORY 0x10u  /* Entry names a directory. */
#define FAT12_ATTR_ARCHIVE   0x20u  /* Entry requests archive backup. */

/*
 * Logical sector-read callback used by the FAT12 reader.
 *
 * Parameters:
 *      context      - Caller-owned disk object passed through unchanged.
 *      sector_index - Zero-based logical sector number to read.
 *      buffer       - Destination buffer for one sector.
 *      buffer_size  - Size of `buffer` in bytes.
 *
 * Returns:
 *      Non-zero on success, otherwise zero.
 *
 * Notes:
 *      The callback is expected to read exactly one logical sector.
 *      `buffer_size` is normally the sector size declared by the volume
 *      boot parameter block, most often 512 bytes on Atari ST floppies.
 */
typedef int (*fat12_read_sector_fn)(void *context, uint32_t sector_index,
    uint8_t *buffer, size_t buffer_size);

/*
 * Abstract FAT12 disk provider.
 *
 * Members:
 *      context      - Caller-owned backing object.
 *      read_sector  - Callback that reads one logical sector.
 *
 * Notes:
 *      This is the main portability boundary for the library. Any image
 *      format can be used as long as it can expose logical sectors
 *      through this structure.
 */
typedef struct fat12_disk {
    void *context;
    fat12_read_sector_fn read_sector;
} fat12_disk_t;

/*
 * Convenience wrapper for a contiguous in-memory disk image.
 *
 * Members:
 *      data         - Pointer to the start of the image bytes.
 *      size         - Total byte size of the image.
 *
 * Notes:
 *      Pair this structure with `fat12_memory_disk_read_sector()` when
 *      the floppy image has already been decoded into one flat memory
 *      buffer.
 */
typedef struct fat12_memory_disk {
    const uint8_t *data;
    size_t size;
} fat12_memory_disk_t;

/*
 * Parsed FAT12 BIOS parameter block and derived geometry.
 *
 * Members:
 *      bytes_per_sector      - Logical sector size in bytes.
 *      sectors_per_cluster   - Allocation unit size in sectors.
 *      reserved_sectors      - Count of reserved sectors before the FAT.
 *      fat_count             - Number of FAT copies on the volume.
 *      root_entries          - Number of fixed root directory entries.
 *      total_sectors         - Total logical sectors in the volume.
 *      sectors_per_fat       - Sector count of one FAT copy.
 *      sectors_per_track     - Track geometry hint from the boot sector.
 *      sides                 - Side count hint from the boot sector.
 *      root_dir_sector       - First sector of the root directory.
 *      root_dir_sector_count - Number of sectors occupied by the root.
 *      data_sector           - First data-cluster sector.
 *      cluster_size          - Cluster size in bytes.
 *
 * Notes:
 *      The last four fields are derived values computed by the library
 *      after the boot sector is parsed successfully.
 */
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

/*
 * Open FAT12 volume descriptor.
 *
 * Members:
 *      disk         - Sector provider used for all future reads.
 *      bpb          - Parsed boot/geometry data for the open volume.
 *
 * Notes:
 *      After `fat12_image_open()` succeeds, callers can reuse this
 *      object for directory traversal and file extraction without
 *      reparsing the boot sector.
 */
typedef struct fat12_image {
    fat12_disk_t disk;
    fat12_bpb_t bpb;
} fat12_image_t;

/*
 * Normalized FAT12 directory entry.
 *
 * Members:
 *      name          - Uppercase `8.3` name formatted as `NAME.EXT` or
 *                      `NAME`, always NUL-terminated.
 *      attr          - FAT attribute byte.
 *      first_cluster - First data cluster, or zero for empty files and
 *                      fixed-root special cases.
 *      size          - File size in bytes.
 *
 * Notes:
 *      Directory entries are reported in a simplified portable form so
 *      callers do not have to decode raw on-disk directory records.
 */
typedef struct fat12_dirent {
    char name[13];
    uint8_t attr;
    uint16_t first_cluster;
    uint32_t size;
} fat12_dirent_t;

/*
 * Directory-walk callback.
 *
 * Parameters:
 *      entry        - Normalized directory entry for the current item.
 *      path         - Slash-separated path from the volume root.
 *      context      - Caller-owned walk context.
 *
 * Returns:
 *      Non-zero to continue the walk, zero to stop successfully.
 *
 * Notes:
 *      Returning zero is treated as an intentional early stop rather
 *      than an error. This makes it easy to search for one file or
 *      collect only the first few entries.
 */
typedef int (*fat12_walk_fn)(const fat12_dirent_t *entry,
    const char *path, void *context);

/*
 * Initializes a memory-backed disk wrapper.
 *
 * Parameters:
 *      memory_disk   - Wrapper to initialize.
 *      data          - Flat image bytes to expose as sectors.
 *      size          - Total byte size of `data`.
 *
 * Returns:
 *      Nothing.
 *
 * Notes:
 *      This is a convenience helper only. Callers still need to fill a
 *      `fat12_disk_t` with `context = memory_disk` and
 *      `read_sector = fat12_memory_disk_read_sector`.
 */
void fat12_memory_disk_init(fat12_memory_disk_t *memory_disk,
    const uint8_t *data, size_t size);

/*
 * Reads one logical sector from a memory-backed disk image.
 *
 * Parameters:
 *      context      - `fat12_memory_disk_t *` previously initialized by
 *                     `fat12_memory_disk_init()`.
 *      sector_index - Zero-based logical sector number to read.
 *      buffer       - Destination buffer for the sector bytes.
 *      buffer_size  - Requested sector size in bytes.
 *
 * Returns:
 *      Non-zero on success, otherwise zero.
 *
 * Notes:
 *      This function is meant to be installed directly into
 *      `fat12_disk_t.read_sector`.
 */
int fat12_memory_disk_read_sector(void *context, uint32_t sector_index,
    uint8_t *buffer, size_t buffer_size);

/*
 * Opens a FAT12 volume through a caller-supplied disk interface.
 *
 * Parameters:
 *      image        - Volume object to initialize.
 *      disk         - Sector provider that exposes the underlying image.
 *      error_text   - Optional error buffer for a short message.
 *      error_size   - Size of `error_text` in bytes.
 *
 * Returns:
 *      Non-zero on success, otherwise zero.
 *
 * Notes:
 *      This call parses the boot sector, validates basic FAT12 layout
 *      assumptions, and computes derived geometry such as the start of
 *      the root directory and data area.
 */
int fat12_image_open(fat12_image_t *image, const fat12_disk_t *disk,
    char *error_text, size_t error_size);

/*
 * Walks the FAT12 directory tree starting at the root directory.
 *
 * Parameters:
 *      image        - Open FAT12 volume.
 *      callback     - Called once for each entry encountered.
 *      context      - Caller-owned callback state.
 *      error_text   - Optional error buffer for a short message.
 *      error_size   - Size of `error_text` in bytes.
 *
 * Returns:
 *      Non-zero on success, otherwise zero.
 *
 * Notes:
 *      Paths are reported with slash separators and no leading slash.
 *      Volume-label entries may still be reported, so callers that want
 *      only ordinary files should filter with `FAT12_ATTR_VOLUME` and
 *      `FAT12_ATTR_DIRECTORY`.
 */
int fat12_walk_root(const fat12_image_t *image, fat12_walk_fn callback,
    void *context, char *error_text, size_t error_size);

/*
 * Reads one regular file into a caller-supplied buffer.
 *
 * Parameters:
 *      image        - Open FAT12 volume.
 *      entry        - File entry previously returned by a directory walk.
 *      buffer       - Destination buffer for file bytes.
 *      buffer_size  - Size of `buffer` in bytes.
 *      error_text   - Optional error buffer for a short message.
 *      error_size   - Size of `error_text` in bytes.
 *
 * Returns:
 *      Non-zero on success, otherwise zero.
 *
 * Notes:
 *      `buffer_size` must be at least `entry->size`. This routine is
 *      intended for regular files; directory entries should be handled
 *      through `fat12_walk_root()` instead.
 */
int fat12_read_file(const fat12_image_t *image, const fat12_dirent_t *entry,
    uint8_t *buffer, size_t buffer_size,
    char *error_text, size_t error_size);

/*
 * Allocates and reads one regular file.
 *
 * Parameters:
 *      image        - Open FAT12 volume.
 *      entry        - File entry previously returned by a directory walk.
 *      data_out     - Receives a newly allocated file buffer.
 *      size_out     - Receives the byte size of the returned file.
 *      error_text   - Optional error buffer for a short message.
 *      error_size   - Size of `error_text` in bytes.
 *
 * Returns:
 *      Non-zero on success, otherwise zero.
 *
 * Notes:
 *      The caller owns `*data_out` and must free it. This is the
 *      easiest file-read path when the exact file size is not known by
 *      the caller ahead of time.
 */
int fat12_read_file_alloc(const fat12_image_t *image,
    const fat12_dirent_t *entry, uint8_t **data_out, size_t *size_out,
    char *error_text, size_t error_size);

/*
 * Formats a FAT attribute byte into a compact readable flag string.
 *
 * Parameters:
 *      attr         - FAT attribute byte.
 *      attr_out     - Destination string buffer.
 *      attr_size    - Size of `attr_out` in bytes.
 *
 * Returns:
 *      Nothing.
 *
 * Notes:
 *      The output uses short letters such as `D`, `V`, `RHA`, or `-`.
 *      This is intended mainly for command-line tools and diagnostics.
 */
void fat12_format_attr(uint8_t attr, char *attr_out, size_t attr_size);

#ifdef __cplusplus
}
#endif

#endif
