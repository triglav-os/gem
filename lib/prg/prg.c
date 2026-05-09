/*
 * Implements a small Atari ST executable loader for `.PRG` and `.APP`
 * files. The loader understands the standard TOS header, DRI/GST-style
 * 14-byte symbol records, and the compact relocation byte stream used by
 * contiguous text+data program images.
 *
 * MIT License (see: LICENSE)
 * Copyright (C) 2026 tomaz stih
 */

#include "prg/prg.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct prg_file_data {
    uint8_t *data;
    size_t size;
} prg_file_data_t;

static void prg_set_error(char *error_text, size_t error_size,
    const char *message)
{
    if (error_text == NULL || error_size == 0u) {
        return;
    }

    snprintf(error_text, error_size, "%s", message);
}

static uint16_t prg_read_be16(const uint8_t *data)
{
    return (uint16_t) (((uint16_t) data[0] << 8) | (uint16_t) data[1]);
}

static uint32_t prg_read_be32(const uint8_t *data)
{
    return ((uint32_t) data[0] << 24)
        | ((uint32_t) data[1] << 16)
        | ((uint32_t) data[2] << 8)
        | (uint32_t) data[3];
}

static void prg_write_be32(uint8_t *data, uint32_t value)
{
    data[0] = (uint8_t) ((value >> 24) & 0xffu);
    data[1] = (uint8_t) ((value >> 16) & 0xffu);
    data[2] = (uint8_t) ((value >> 8) & 0xffu);
    data[3] = (uint8_t) (value & 0xffu);
}

static int prg_read_whole_file(const char *path, prg_file_data_t *file_data,
    char *error_text, size_t error_size)
{
    FILE *fp;
    long file_size;
    uint8_t *buffer;

    fp = fopen(path, "rb");
    if (fp == NULL) {
        prg_set_error(error_text, error_size, "failed to open executable");
        return 0;
    }

    if (fseek(fp, 0L, SEEK_END) != 0) {
        fclose(fp);
        prg_set_error(error_text, error_size, "failed to seek executable");
        return 0;
    }

    file_size = ftell(fp);
    if (file_size < 0L) {
        fclose(fp);
        prg_set_error(error_text, error_size, "failed to size executable");
        return 0;
    }

    if (fseek(fp, 0L, SEEK_SET) != 0) {
        fclose(fp);
        prg_set_error(error_text, error_size, "failed to rewind executable");
        return 0;
    }

    buffer = NULL;
    if (file_size > 0L) {
        buffer = (uint8_t *) malloc((size_t) file_size);
        if (buffer == NULL) {
            fclose(fp);
            prg_set_error(error_text, error_size, "out of memory");
            return 0;
        }

        if (fread(buffer, 1u, (size_t) file_size, fp) != (size_t) file_size) {
            free(buffer);
            fclose(fp);
            prg_set_error(error_text, error_size, "failed to read executable");
            return 0;
        }
    }

    fclose(fp);
    file_data->data = buffer;
    file_data->size = (size_t) file_size;
    return 1;
}

static int prg_parse_header(prg_image_t *image, const uint8_t *data,
    size_t size, char *error_text, size_t error_size)
{
    const size_t header_size = 28u;

    if (size < header_size) {
        prg_set_error(error_text, error_size, "file too small for PRG header");
        return 0;
    }

    image->header.branch = prg_read_be16(data + 0u);
    image->header.text_size = prg_read_be32(data + 2u);
    image->header.data_size = prg_read_be32(data + 6u);
    image->header.bss_size = prg_read_be32(data + 10u);
    image->header.symbol_size = prg_read_be32(data + 14u);
    image->header.reserved = prg_read_be32(data + 18u);
    image->header.flags = prg_read_be32(data + 22u);
    image->header.absflag = prg_read_be16(data + 26u);

    return 1;
}

static int prg_parse_symbols(prg_image_t *image, const uint8_t *data,
    size_t size, char *error_text, size_t error_size)
{
    size_t index;
    size_t offset;
    size_t count;

    if ((size % 14u) != 0u) {
        prg_set_error(error_text, error_size,
            "unsupported symbol table length");
        return 0;
    }

    count = size / 14u;
    if (count == 0u) {
        return 1;
    }

    image->symbols = (prg_symbol_t *) calloc(count, sizeof(prg_symbol_t));
    if (image->symbols == NULL) {
        prg_set_error(error_text, error_size, "out of memory");
        return 0;
    }

    image->symbol_count = count;
    for (index = 0u, offset = 0u; index < count; ++index, offset += 14u) {
        memcpy(image->symbols[index].name, data + offset, 8u);
        image->symbols[index].name[8] = '\0';
        while (strlen(image->symbols[index].name) > 0u
            && image->symbols[index].name[
                strlen(image->symbols[index].name) - 1u] == ' ') {
            image->symbols[index].name[
                strlen(image->symbols[index].name) - 1u] = '\0';
        }
        image->symbols[index].type = prg_read_be16(data + offset + 8u);
        image->symbols[index].value = prg_read_be32(data + offset + 10u);
    }

    return 1;
}

static int prg_apply_relocation(prg_image_t *image, uint32_t offset,
    uint32_t load_address, char *error_text, size_t error_size)
{
    uint32_t value;

    if (offset > image->image_size || image->image_size - offset < 4u) {
        prg_set_error(error_text, error_size, "relocation offset out of range");
        return 0;
    }

    value = prg_read_be32(image->image + offset);
    value += load_address;
    prg_write_be32(image->image + offset, value);
    return 1;
}

static int prg_apply_relocations(prg_image_t *image, const uint8_t *data,
    size_t size, uint32_t load_address, char *error_text, size_t error_size)
{
    size_t index;
    uint32_t offset;

    if (image->header.absflag != 0u) {
        return 1;
    }
    if (size < 4u) {
        prg_set_error(error_text, error_size, "missing relocation stream");
        return 0;
    }

    offset = prg_read_be32(data);
    if (offset == 0u) {
        return 1;
    }

    if (!prg_apply_relocation(image, offset, load_address,
            error_text, error_size)) {
        return 0;
    }

    for (index = 4u; index < size; ++index) {
        uint8_t delta = data[index];

        if (delta == 0u) {
            return 1;
        }
        if (delta == 1u) {
            offset += 254u;
            continue;
        }

        offset += (uint32_t) delta;
        if (!prg_apply_relocation(image, offset, load_address,
                error_text, error_size)) {
            return 0;
        }
    }

    prg_set_error(error_text, error_size, "unterminated relocation stream");
    return 0;
}

void prg_image_init(prg_image_t *image)
{
    if (image == NULL) {
        return;
    }

    memset(image, 0, sizeof(*image));
}

void prg_image_free(prg_image_t *image)
{
    if (image == NULL) {
        return;
    }

    free(image->image);
    free(image->symbols);
    prg_image_init(image);
}

int prg_read_file(const char *path, uint32_t load_address,
    prg_image_t *image, char *error_text, size_t error_size)
{
    const size_t header_size = 28u;
    prg_file_data_t file_data;
    size_t image_size;
    size_t cursor;

    if (path == NULL || image == NULL) {
        prg_set_error(error_text, error_size, "invalid arguments");
        return 0;
    }

    prg_image_free(image);
    memset(&file_data, 0, sizeof(file_data));

    if (!prg_read_whole_file(path, &file_data, error_text, error_size)) {
        return 0;
    }

    if (!prg_parse_header(image, file_data.data, file_data.size,
            error_text, error_size)) {
        free(file_data.data);
        return 0;
    }

    image_size = (size_t) image->header.text_size
        + (size_t) image->header.data_size;
    cursor = header_size;

    if (file_data.size < cursor || file_data.size - cursor < image_size) {
        free(file_data.data);
        prg_set_error(error_text, error_size,
            "file truncated before text/data image");
        return 0;
    }

    image->image = (uint8_t *) malloc(image_size);
    if (image->image == NULL) {
        free(file_data.data);
        prg_set_error(error_text, error_size, "out of memory");
        return 0;
    }

    memcpy(image->image, file_data.data + cursor, image_size);
    image->image_size = image_size;
    cursor += image_size;

    if (file_data.size < cursor
        || file_data.size - cursor < image->header.symbol_size) {
        free(file_data.data);
        prg_image_free(image);
        prg_set_error(error_text, error_size,
            "file truncated before symbol table");
        return 0;
    }

    if (!prg_parse_symbols(image, file_data.data + cursor,
            (size_t) image->header.symbol_size, error_text, error_size)) {
        free(file_data.data);
        prg_image_free(image);
        return 0;
    }
    cursor += (size_t) image->header.symbol_size;

    if (!prg_apply_relocations(image, file_data.data + cursor,
            file_data.size - cursor, load_address, error_text, error_size)) {
        free(file_data.data);
        prg_image_free(image);
        return 0;
    }

    free(file_data.data);
    return 1;
}

const prg_symbol_t *prg_find_symbol(const prg_image_t *image,
    const char *name)
{
    size_t index;

    if (image == NULL || name == NULL) {
        return NULL;
    }

    for (index = 0u; index < image->symbol_count; ++index) {
        if (strcmp(image->symbols[index].name, name) == 0) {
            return &image->symbols[index];
        }
    }

    return NULL;
}
