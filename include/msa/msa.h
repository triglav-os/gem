/*
 * Declares a reusable Magic Shadow Archiver image library for reading,
 * writing, and inspecting Atari ST `.MSA` floppy archives. The library
 * works at whole-image granularity so tools can decode or encode floppy
 * images once and then layer FAT12, emulation, or other analysis on top
 * without reimplementing the MSA container format.
 *
 * MIT License (see: LICENSE)
 * Copyright (C) 2026 tomaz stih
 */

#ifndef MSA_MSA_H
#define MSA_MSA_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * On-disk big-endian magic word used by standard MSA archives.
 */
#define MSA_MAGIC 0x0e0fU

/*
 * Recommended fixed-size buffer length for short human-readable error
 * strings returned by this library.
 */
#define MSA_ERROR_LENGTH 256u

/*
 * Basic floppy geometry carried by an MSA archive.
 *
 * Members:
 *      sectors_per_track - Logical sectors stored per track-side block.
 *      sides             - Number of sides minus zero-based indexing
 *                          expectation. A standard double-sided image
 *                          reports 2 here.
 *      start_track       - First encoded track number.
 *      end_track         - Last encoded track number, inclusive.
 *
 * Notes:
 *      MSA stores track data as a sequence of track-side blocks, so the
 *      total track count is `(end_track - start_track + 1) * sides`.
 */
typedef struct msa_geometry {
    uint16_t sectors_per_track;
    uint16_t sides;
    uint16_t start_track;
    uint16_t end_track;
} msa_geometry_t;

/*
 * Decoded MSA image.
 *
 * Members:
 *      geometry           - Floppy geometry parsed from the archive.
 *      data               - Fully decoded track data owned by the image.
 *      data_size          - Total byte size of `data`.
 *      track_stored_sizes - Per-track-side stored lengths from the
 *                           archive, one entry per encoded block.
 *      track_compressed   - Per-track-side flag array indicating whether
 *                           each stored block was compressed in the
 *                           archive.
 *
 * Notes:
 *      The `data` buffer stores blocks in MSA order, which alternates by
 *      track and side. Helper accessors such as `msa_track_data()` hide
 *      that layout from callers.
 */
typedef struct msa_image {
    msa_geometry_t geometry;
    uint8_t *data;
    size_t data_size;
    uint16_t *track_stored_sizes;
    uint8_t *track_compressed;
} msa_image_t;

/*
 * Initializes an empty MSA image object.
 *
 * Parameters:
 *      image        - Object to initialize.
 *
 * Returns:
 *      Nothing.
 *
 * Notes:
 *      Call this before first use or after manual stack allocation of an
 *      `msa_image_t`.
 */
void msa_image_init(msa_image_t *image);

/*
 * Releases all storage owned by an MSA image object.
 *
 * Parameters:
 *      image        - Object to clear.
 *
 * Returns:
 *      Nothing.
 *
 * Notes:
 *      This frees decoded track data and all per-track metadata, then
 *      resets the object to the empty state produced by
 *      `msa_image_init()`.
 */
void msa_image_free(msa_image_t *image);

/*
 * Returns the number of encoded track-side blocks in an image.
 *
 * Parameters:
 *      image        - Decoded MSA image.
 *
 * Returns:
 *      Number of track-side blocks represented by the archive.
 *
 * Notes:
 *      This is the natural length of the `track_stored_sizes` and
 *      `track_compressed` arrays.
 */
size_t msa_track_count(const msa_image_t *image);

/*
 * Returns the decoded byte size of one track-side block.
 *
 * Parameters:
 *      image        - Decoded MSA image.
 *
 * Returns:
 *      Byte size of one decoded block.
 *
 * Notes:
 *      This is normally `sectors_per_track * 512`, since MSA archives
 *      standard Atari floppy sectors.
 */
size_t msa_track_size(const msa_image_t *image);

/*
 * Returns a pointer to one decoded track-side block.
 *
 * Parameters:
 *      image        - Decoded MSA image.
 *      track        - Logical track number to access.
 *      side         - Side number to access.
 *      size_out     - Optional receiver for the decoded block size.
 *
 * Returns:
 *      Pointer to the decoded block, or NULL when the request is out of
 *      range.
 *
 * Notes:
 *      The returned pointer remains owned by `image` and becomes invalid
 *      after `msa_image_free()`.
 */
const uint8_t *msa_track_data(const msa_image_t *image,
    uint16_t track, uint16_t side, size_t *size_out);

/*
 * Reads and decompresses an MSA archive from disk.
 *
 * Parameters:
 *      path         - Filesystem path to the `.MSA` archive.
 *      image        - Destination image object to populate.
 *      error_text   - Optional error buffer for a short message.
 *      error_size   - Size of `error_text` in bytes.
 *
 * Returns:
 *      Non-zero on success, otherwise zero.
 *
 * Notes:
 *      On success, `image->data` contains the fully decoded floppy
 *      contents and the per-track metadata records which blocks were
 *      compressed in the original archive.
 */
int msa_read_file(const char *path, msa_image_t *image,
    char *error_text, size_t error_size);

/*
 * Writes an in-memory decoded image back to an MSA archive.
 *
 * Parameters:
 *      path         - Destination filesystem path.
 *      image        - Decoded image to encode.
 *      error_text   - Optional error buffer for a short message.
 *      error_size   - Size of `error_text` in bytes.
 *
 * Returns:
 *      Non-zero on success, otherwise zero.
 *
 * Notes:
 *      The encoder emits compressed track blocks only when the MSA RLE
 *      representation is smaller than or equal to the raw block.
 */
int msa_write_file(const char *path, const msa_image_t *image,
    char *error_text, size_t error_size);

/*
 * Builds a decoded MSA image from a raw ST-style floppy image.
 *
 * Parameters:
 *      image        - Destination image object to populate.
 *      raw_data     - Flat raw floppy bytes in track order.
 *      raw_size     - Byte size of `raw_data`.
 *      geometry     - Geometry to associate with the raw image.
 *      error_text   - Optional error buffer for a short message.
 *      error_size   - Size of `error_text` in bytes.
 *
 * Returns:
 *      Non-zero on success, otherwise zero.
 *
 * Notes:
 *      This helper is useful when converting raw `.ST` style images into
 *      MSA format or when constructing synthetic test images entirely in
 *      memory.
 */
int msa_build_image_from_raw(msa_image_t *image, const uint8_t *raw_data,
    size_t raw_size, const msa_geometry_t *geometry,
    char *error_text, size_t error_size);

/*
 * Guesses floppy geometry for a raw disk image.
 *
 * Parameters:
 *      raw_data     - Raw floppy bytes, optionally including a FAT boot
 *                     sector.
 *      raw_size     - Byte size of `raw_data`.
 *      geometry     - Receives the guessed geometry on success.
 *      error_text   - Optional error buffer for a short message.
 *      error_size   - Size of `error_text` in bytes.
 *
 * Returns:
 *      Non-zero on success, otherwise zero.
 *
 * Notes:
 *      The guesser first tries FAT-style boot-sector fields when they
 *      are available and plausible, then falls back to common Atari ST
 *      floppy sizes.
 */
int msa_guess_geometry(const uint8_t *raw_data, size_t raw_size,
    msa_geometry_t *geometry, char *error_text, size_t error_size);

#ifdef __cplusplus
}
#endif

#endif
