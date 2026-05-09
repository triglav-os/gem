/*
 * Declares a small reusable Magic Shadow Archiver image library for
 * reading, writing, and inspecting Atari ST `.MSA` floppy archives.
 * The interface works at whole-image granularity so command-line tools
 * and future applications can share one decoder/encoder without
 * embedding MSA format knowledge repeatedly.
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

#define MSA_MAGIC 0x0e0fU
#define MSA_ERROR_LENGTH 256u

typedef struct msa_geometry {
    uint16_t sectors_per_track;
    uint16_t sides;
    uint16_t start_track;
    uint16_t end_track;
} msa_geometry_t;

typedef struct msa_image {
    msa_geometry_t geometry;
    uint8_t *data;
    size_t data_size;
    uint16_t *track_stored_sizes;
    uint8_t *track_compressed;
} msa_image_t;

/*
 * Initializes an empty MSA image object.
 */
void msa_image_init(msa_image_t *image);

/*
 * Releases all storage owned by an MSA image object.
 */
void msa_image_free(msa_image_t *image);

/*
 * Returns the number of track-side blocks stored by the image.
 */
size_t msa_track_count(const msa_image_t *image);

/*
 * Returns the decoded byte size of one track-side block.
 */
size_t msa_track_size(const msa_image_t *image);

/*
 * Returns a pointer to one decoded track-side block.
 * The pointer remains owned by the image.
 */
const uint8_t *msa_track_data(const msa_image_t *image,
    uint16_t track, uint16_t side, size_t *size_out);

/*
 * Reads and decompresses an MSA archive from disk.
 * On failure, `error_text` receives a short message when provided.
 */
int msa_read_file(const char *path, msa_image_t *image,
    char *error_text, size_t error_size);

/*
 * Writes an MSA archive to disk from an in-memory decoded image.
 * The encoder stores tracks uncompressed when that is smaller or equal.
 */
int msa_write_file(const char *path, const msa_image_t *image,
    char *error_text, size_t error_size);

/*
 * Builds an in-memory decoded image from a raw ST-style disk image.
 */
int msa_build_image_from_raw(msa_image_t *image, const uint8_t *raw_data,
    size_t raw_size, const msa_geometry_t *geometry,
    char *error_text, size_t error_size);

/*
 * Guesses MSA geometry from a raw image size and, when available,
 * FAT-style boot sector fields in `raw_data`.
 */
int msa_guess_geometry(const uint8_t *raw_data, size_t raw_size,
    msa_geometry_t *geometry, char *error_text, size_t error_size);

#ifdef __cplusplus
}
#endif

#endif
