/*
 * Implements a reusable Magic Shadow Archiver image library. The code
 * handles the classic MSA header, alternating-side track ordering,
 * simple E5-based RLE compression, and whole-image conversion to and
 * from raw ST-style disk image bytes.
 *
 * MIT License (see: LICENSE)
 * Copyright (C) 2026 tomaz stih
 */

#include "msa/msa.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum {
    msa_sector_size = 512,
    msa_boot_total_sectors_offset = 19,
    msa_boot_sectors_per_track_offset = 24,
    msa_boot_sides_offset = 26
};

static void msa_set_error(char *error_text, size_t error_size,
    const char *message);
static uint16_t msa_read_be16(FILE *stream, int *ok);
static int msa_write_be16(FILE *stream, uint16_t value);
static size_t msa_track_index(const msa_image_t *image,
    uint16_t track, uint16_t side);
static int msa_valid_geometry(const msa_geometry_t *geometry);
static int msa_alloc_track_metadata(msa_image_t *image);
static int msa_decode_track(const uint8_t *source, size_t source_size,
    uint8_t *destination, size_t destination_size);
static size_t msa_encode_track(const uint8_t *source, size_t source_size,
    uint8_t *destination);
static uint16_t msa_read_le16(const uint8_t *bytes);

void msa_image_init(msa_image_t *image)
{
    if (image == NULL) {
        return;
    }
    memset(image, 0, sizeof(*image));
}

void msa_image_free(msa_image_t *image)
{
    if (image == NULL) {
        return;
    }
    free(image->data);
    free(image->track_stored_sizes);
    free(image->track_compressed);
    memset(image, 0, sizeof(*image));
}

size_t msa_track_count(const msa_image_t *image)
{
    size_t track_count;

    if (image == NULL || image->geometry.sides == 0 ||
        image->geometry.end_track < image->geometry.start_track) {
        return 0;
    }

    track_count = (size_t) (image->geometry.end_track -
        image->geometry.start_track + 1u);
    return track_count * (size_t) image->geometry.sides;
}

size_t msa_track_size(const msa_image_t *image)
{
    if (image == NULL) {
        return 0;
    }
    return (size_t) image->geometry.sectors_per_track * msa_sector_size;
}

const uint8_t *msa_track_data(const msa_image_t *image,
    uint16_t track, uint16_t side, size_t *size_out)
{
    size_t index;
    size_t track_size;

    if (size_out != NULL) {
        *size_out = 0;
    }
    if (image == NULL || image->data == NULL) {
        return NULL;
    }
    if (track < image->geometry.start_track || track > image->geometry.end_track ||
        side >= image->geometry.sides) {
        return NULL;
    }

    index = msa_track_index(image, track, side);
    track_size = msa_track_size(image);
    if (size_out != NULL) {
        *size_out = track_size;
    }
    return image->data + index * track_size;
}

int msa_read_file(const char *path, msa_image_t *image,
    char *error_text, size_t error_size)
{
    FILE *stream;
    uint16_t magic;
    size_t track_count;
    size_t track_size;
    size_t index;
    int ok = 1;
    msa_image_t loaded;

    if (path == NULL || image == NULL) {
        msa_set_error(error_text, error_size, "invalid read arguments");
        return 0;
    }

    stream = fopen(path, "rb");
    if (stream == NULL) {
        msa_set_error(error_text, error_size, strerror(errno));
        return 0;
    }

    msa_image_init(&loaded);
    magic = msa_read_be16(stream, &ok);
    loaded.geometry.sectors_per_track = msa_read_be16(stream, &ok);
    loaded.geometry.sides = (uint16_t) (msa_read_be16(stream, &ok) + 1u);
    loaded.geometry.start_track = msa_read_be16(stream, &ok);
    loaded.geometry.end_track = msa_read_be16(stream, &ok);
    if (!ok || magic != MSA_MAGIC || !msa_valid_geometry(&loaded.geometry)) {
        msa_set_error(error_text, error_size, "invalid MSA header");
        fclose(stream);
        msa_image_free(&loaded);
        return 0;
    }

    track_count = msa_track_count(&loaded);
    track_size = msa_track_size(&loaded);
    loaded.data_size = track_count * track_size;
    loaded.data = malloc(loaded.data_size);
    if (loaded.data == NULL || !msa_alloc_track_metadata(&loaded)) {
        msa_set_error(error_text, error_size, "out of memory");
        fclose(stream);
        msa_image_free(&loaded);
        return 0;
    }

    for (index = 0; index < track_count; ++index) {
        uint16_t stored_size;
        uint8_t *compressed_data;

        stored_size = msa_read_be16(stream, &ok);
        if (!ok || stored_size == 0) {
            msa_set_error(error_text, error_size, "invalid MSA track block");
            fclose(stream);
            msa_image_free(&loaded);
            return 0;
        }

        compressed_data = malloc(stored_size);
        if (compressed_data == NULL) {
            msa_set_error(error_text, error_size, "out of memory");
            fclose(stream);
            msa_image_free(&loaded);
            return 0;
        }
        if (fread(compressed_data, 1u, stored_size, stream) != stored_size) {
            free(compressed_data);
            msa_set_error(error_text, error_size, "truncated MSA track data");
            fclose(stream);
            msa_image_free(&loaded);
            return 0;
        }

        loaded.track_stored_sizes[index] = stored_size;
        loaded.track_compressed[index] = (stored_size != track_size) ? 1u : 0u;
        if (stored_size == track_size) {
            memcpy(loaded.data + index * track_size, compressed_data, track_size);
            free(compressed_data);
            continue;
        }
        if (!msa_decode_track(compressed_data, stored_size,
                loaded.data + index * track_size, track_size)) {
            free(compressed_data);
            msa_set_error(error_text, error_size, "invalid MSA RLE track");
            fclose(stream);
            msa_image_free(&loaded);
            return 0;
        }
        free(compressed_data);
    }

    fclose(stream);
    msa_image_free(image);
    *image = loaded;
    return 1;
}

int msa_write_file(const char *path, const msa_image_t *image,
    char *error_text, size_t error_size)
{
    FILE *stream;
    size_t track_count;
    size_t track_size;
    uint8_t *encoded;
    size_t encoded_capacity;
    size_t index;

    if (path == NULL || image == NULL || image->data == NULL ||
        !msa_valid_geometry(&image->geometry)) {
        msa_set_error(error_text, error_size, "invalid write arguments");
        return 0;
    }

    track_count = msa_track_count(image);
    track_size = msa_track_size(image);
    if (image->data_size != track_count * track_size) {
        msa_set_error(error_text, error_size, "image geometry does not match data");
        return 0;
    }

    stream = fopen(path, "wb");
    if (stream == NULL) {
        msa_set_error(error_text, error_size, strerror(errno));
        return 0;
    }

    encoded_capacity = track_size * 2u + 4u;
    encoded = malloc(encoded_capacity);
    if (encoded == NULL) {
        fclose(stream);
        msa_set_error(error_text, error_size, "out of memory");
        return 0;
    }

    if (!msa_write_be16(stream, MSA_MAGIC) ||
        !msa_write_be16(stream, image->geometry.sectors_per_track) ||
        !msa_write_be16(stream, (uint16_t) (image->geometry.sides - 1u)) ||
        !msa_write_be16(stream, image->geometry.start_track) ||
        !msa_write_be16(stream, image->geometry.end_track)) {
        free(encoded);
        fclose(stream);
        msa_set_error(error_text, error_size, "failed to write MSA header");
        return 0;
    }

    for (index = 0; index < track_count; ++index) {
        const uint8_t *track_data = image->data + index * track_size;
        size_t encoded_size = msa_encode_track(track_data, track_size, encoded);

        if (encoded_size >= track_size) {
            if (!msa_write_be16(stream, (uint16_t) track_size) ||
                fwrite(track_data, 1u, track_size, stream) != track_size) {
                free(encoded);
                fclose(stream);
                msa_set_error(error_text, error_size, "failed to write raw track");
                return 0;
            }
        } else {
            if (!msa_write_be16(stream, (uint16_t) encoded_size) ||
                fwrite(encoded, 1u, encoded_size, stream) != encoded_size) {
                free(encoded);
                fclose(stream);
                msa_set_error(error_text, error_size,
                    "failed to write compressed track");
                return 0;
            }
        }
    }

    free(encoded);
    fclose(stream);
    return 1;
}

int msa_build_image_from_raw(msa_image_t *image, const uint8_t *raw_data,
    size_t raw_size, const msa_geometry_t *geometry,
    char *error_text, size_t error_size)
{
    msa_image_t built;
    size_t track_count;
    size_t expected_size;

    if (image == NULL || raw_data == NULL || geometry == NULL ||
        !msa_valid_geometry(geometry)) {
        msa_set_error(error_text, error_size, "invalid raw image arguments");
        return 0;
    }

    track_count = (size_t) (geometry->end_track - geometry->start_track + 1u) *
        (size_t) geometry->sides;
    expected_size = track_count * (size_t) geometry->sectors_per_track *
        msa_sector_size;
    if (raw_size != expected_size) {
        msa_set_error(error_text, error_size,
            "raw image size does not match supplied geometry");
        return 0;
    }

    msa_image_init(&built);
    built.geometry = *geometry;
    built.data = malloc(raw_size);
    built.data_size = raw_size;
    if (built.data == NULL || !msa_alloc_track_metadata(&built)) {
        msa_set_error(error_text, error_size, "out of memory");
        msa_image_free(&built);
        return 0;
    }
    memcpy(built.data, raw_data, raw_size);

    msa_image_free(image);
    *image = built;
    return 1;
}

int msa_guess_geometry(const uint8_t *raw_data, size_t raw_size,
    msa_geometry_t *geometry, char *error_text, size_t error_size)
{
    uint16_t bpb_total_sectors = 0;
    uint16_t bpb_sectors_per_track = 0;
    uint16_t bpb_sides = 0;
    unsigned int sectors_per_track;
    unsigned int sides;
    unsigned int start_track;
    unsigned int track_count;
    unsigned int matches = 0;
    msa_geometry_t candidate;

    if (geometry == NULL || raw_size == 0 || raw_size % msa_sector_size != 0u) {
        msa_set_error(error_text, error_size, "raw image size is not sector aligned");
        return 0;
    }

    if (raw_data != NULL && raw_size >= 32u) {
        bpb_total_sectors = msa_read_le16(raw_data +
            msa_boot_total_sectors_offset);
        bpb_sectors_per_track = msa_read_le16(raw_data +
            msa_boot_sectors_per_track_offset);
        bpb_sides = msa_read_le16(raw_data + msa_boot_sides_offset);
        if (bpb_total_sectors != 0 && bpb_sectors_per_track != 0 &&
            bpb_sides != 0 &&
            (size_t) bpb_total_sectors * msa_sector_size == raw_size &&
            bpb_total_sectors % (bpb_sectors_per_track * bpb_sides) == 0) {
            track_count = bpb_total_sectors / (bpb_sectors_per_track * bpb_sides);
            geometry->sectors_per_track = bpb_sectors_per_track;
            geometry->sides = bpb_sides;
            geometry->start_track = 0;
            geometry->end_track = (uint16_t) (track_count - 1u);
            return 1;
        }
    }

    for (sectors_per_track = 8; sectors_per_track <= 11; ++sectors_per_track) {
        for (sides = 1; sides <= 2; ++sides) {
            if ((raw_size / msa_sector_size) % (sectors_per_track * sides) != 0u) {
                continue;
            }
            track_count = (unsigned int) ((raw_size / msa_sector_size) /
                (sectors_per_track * sides));
            if (track_count == 0 || track_count > 85) {
                continue;
            }
            start_track = 0;
            candidate.sectors_per_track = (uint16_t) sectors_per_track;
            candidate.sides = (uint16_t) sides;
            candidate.start_track = (uint16_t) start_track;
            candidate.end_track = (uint16_t) (track_count - 1u);
            *geometry = candidate;
            ++matches;
        }
    }

    if (matches == 1) {
        return 1;
    }
    if (matches == 0) {
        msa_set_error(error_text, error_size,
            "could not infer floppy geometry from raw size");
    } else {
        msa_set_error(error_text, error_size,
            "ambiguous raw image geometry; pass explicit geometry");
    }
    return 0;
}

static void msa_set_error(char *error_text, size_t error_size,
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

static uint16_t msa_read_be16(FILE *stream, int *ok)
{
    uint8_t bytes[2];

    if (stream == NULL || ok == NULL || *ok == 0) {
        if (ok != NULL) {
            *ok = 0;
        }
        return 0;
    }
    if (fread(bytes, 1u, sizeof(bytes), stream) != sizeof(bytes)) {
        *ok = 0;
        return 0;
    }
    return (uint16_t) ((uint16_t) bytes[0] << 8 | (uint16_t) bytes[1]);
}

static int msa_write_be16(FILE *stream, uint16_t value)
{
    uint8_t bytes[2];

    if (stream == NULL) {
        return 0;
    }
    bytes[0] = (uint8_t) (value >> 8);
    bytes[1] = (uint8_t) value;
    return fwrite(bytes, 1u, sizeof(bytes), stream) == sizeof(bytes);
}

static size_t msa_track_index(const msa_image_t *image,
    uint16_t track, uint16_t side)
{
    return ((size_t) (track - image->geometry.start_track) *
        (size_t) image->geometry.sides) + (size_t) side;
}

static int msa_valid_geometry(const msa_geometry_t *geometry)
{
    if (geometry == NULL || geometry->sectors_per_track == 0 ||
        geometry->sides == 0 || geometry->sides > 2 ||
        geometry->start_track > geometry->end_track) {
        return 0;
    }
    return 1;
}

static int msa_alloc_track_metadata(msa_image_t *image)
{
    size_t count;

    if (image == NULL) {
        return 0;
    }
    count = msa_track_count(image);
    image->track_stored_sizes = calloc(count, sizeof(*image->track_stored_sizes));
    image->track_compressed = calloc(count, sizeof(*image->track_compressed));
    return image->track_stored_sizes != NULL && image->track_compressed != NULL;
}

static int msa_decode_track(const uint8_t *source, size_t source_size,
    uint8_t *destination, size_t destination_size)
{
    size_t src_index = 0;
    size_t dst_index = 0;

    if (source == NULL || destination == NULL) {
        return 0;
    }

    while (src_index < source_size && dst_index < destination_size) {
        uint8_t byte = source[src_index++];

        if (byte != 0xe5u) {
            destination[dst_index++] = byte;
            continue;
        }
        if (src_index + 2u >= source_size) {
            return 0;
        }

        {
            uint8_t value = source[src_index++];
            uint16_t run_length = (uint16_t) ((uint16_t) source[src_index] << 8 |
                (uint16_t) source[src_index + 1u]);
            size_t run_index;

            src_index += 2u;
            if (run_length == 0 || dst_index + run_length > destination_size) {
                return 0;
            }
            for (run_index = 0; run_index < run_length; ++run_index) {
                destination[dst_index++] = value;
            }
        }
    }

    return src_index == source_size && dst_index == destination_size;
}

static size_t msa_encode_track(const uint8_t *source, size_t source_size,
    uint8_t *destination)
{
    size_t src_index = 0;
    size_t dst_index = 0;

    while (src_index < source_size) {
        size_t run_length = 1u;
        uint8_t value = source[src_index];
        int use_run = 0;

        while (src_index + run_length < source_size &&
               source[src_index + run_length] == value &&
               run_length < 65535u) {
            ++run_length;
        }

        if (value == 0xe5u) {
            use_run = (run_length >= 1u);
        } else if (run_length >= 4u) {
            use_run = 1;
        }

        if (use_run) {
            destination[dst_index++] = 0xe5u;
            destination[dst_index++] = value;
            destination[dst_index++] = (uint8_t) (run_length >> 8);
            destination[dst_index++] = (uint8_t) run_length;
            src_index += run_length;
            continue;
        }

        destination[dst_index++] = value;
        ++src_index;
    }

    return dst_index;
}

static uint16_t msa_read_le16(const uint8_t *bytes)
{
    return (uint16_t) ((uint16_t) bytes[0] | ((uint16_t) bytes[1] << 8));
}
