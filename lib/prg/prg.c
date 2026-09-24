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

#include "platform/os.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

typedef struct prg_file_data {
    uint8_t *data;
    size_t size;
} prg_file_data_t;

int prg_file_kind(const char *path, prg_file_kind_t *kind_out)
{
    const char *dot;
    prg_file_kind_t kind;
    char extension[5];
    size_t index;

    if (path == NULL || kind_out == NULL) {
        return 0;
    }
    dot = strrchr(path, '.');
    if (dot == NULL) {
        return 0;
    }
    if (strlen(dot) != 4u) return 0;
    for (index = 0u; index < 4u; ++index)
        extension[index] = (char)tolower((unsigned char)dot[index]);
    extension[4] = '\0';
    if (strcmp(extension, ".prg") == 0) kind = PRG_FILE_PRG;
    else if (strcmp(extension, ".tos") == 0) kind = PRG_FILE_TOS;
    else if (strcmp(extension, ".ttp") == 0) kind = PRG_FILE_TTP;
    else if (strcmp(extension, ".app") == 0) kind = PRG_FILE_APP;
    else if (strcmp(extension, ".gtp") == 0) kind = PRG_FILE_GTP;
    else if (strcmp(extension, ".acc") == 0) kind = PRG_FILE_ACC;
    else return 0;
    *kind_out = kind;
    return 1;
}

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
    return (uint16_t)(((uint16_t)data[0] << 8) | (uint16_t)data[1]);
}

static uint32_t prg_read_be32(const uint8_t *data)
{
    return ((uint32_t)data[0] << 24) | ((uint32_t)data[1] << 16) |
           ((uint32_t)data[2] << 8) | (uint32_t)data[3];
}

static void prg_write_be32(uint8_t *data, uint32_t value)
{
    data[0] = (uint8_t)((value >> 24) & 0xffu);
    data[1] = (uint8_t)((value >> 16) & 0xffu);
    data[2] = (uint8_t)((value >> 8) & 0xffu);
    data[3] = (uint8_t)(value & 0xffu);
}

static int prg_read_whole_file(const char *path, prg_file_data_t *file_data,
                               char *error_text, size_t error_size)
{
    int fd;
    int64_t file_size;
    uint8_t *buffer;
    size_t offset;
    int32_t count;

    fd = gem_os_open_read(path);
    if (fd < 0) {
        prg_set_error(error_text, error_size, "failed to open executable");
        return 0;
    }

    file_size = gem_os_seek(fd, 0, 2);
    if (file_size < 0) {
        gem_os_close(fd);
        prg_set_error(error_text, error_size, "failed to seek executable");
        return 0;
    }

    if (gem_os_seek(fd, 0, 0) < 0 || (uint64_t)file_size > SIZE_MAX) {
        gem_os_close(fd);
        prg_set_error(error_text, error_size, "failed to rewind executable");
        return 0;
    }

    buffer = NULL;
    if (file_size > 0L) {
        buffer = (uint8_t *)malloc((size_t)file_size);
        if (buffer == NULL) {
            gem_os_close(fd);
            prg_set_error(error_text, error_size, "out of memory");
            return 0;
        }

        offset = 0u;
        while (offset < (size_t)file_size) {
            count = gem_os_read(fd, buffer + offset,
                                (uint32_t)((size_t)file_size - offset));
            if (count <= 0) {
                free(buffer);
                gem_os_close(fd);
                prg_set_error(error_text, error_size,
                              "failed to read executable");
                return 0;
            }
            offset += (size_t)count;
        }
    }

    gem_os_close(fd);
    file_data->data = buffer;
    file_data->size = (size_t)file_size;
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
        /*
         * Symbols are not required to execute a TOS image.  Linkers used
         * several private/debug symbol layouts, while the header still
         * provides the exact byte count needed to find the relocation
         * stream.  Preserve execution compatibility by skipping an unknown
         * layout instead of mistaking it for malformed executable data.
         */
        (void)data;
        (void)error_text;
        (void)error_size;
        return 1;
    }

    count = size / 14u;
    if (count == 0u) {
        return 1;
    }

    image->symbols = (prg_symbol_t *)calloc(count, sizeof(prg_symbol_t));
    if (image->symbols == NULL) {
        prg_set_error(error_text, error_size, "out of memory");
        return 0;
    }

    image->symbol_count = count;
    for (index = 0u, offset = 0u; index < count; ++index, offset += 14u) {
        memcpy(image->symbols[index].name, data + offset, 8u);
        image->symbols[index].name[8] = '\0';
        while (strlen(image->symbols[index].name) > 0u &&
               image->symbols[index]
                       .name[strlen(image->symbols[index].name) - 1u] == ' ') {
            image->symbols[index]
                .name[strlen(image->symbols[index].name) - 1u] = '\0';
        }
        image->symbols[index].type = prg_read_be16(data + offset + 8u);
        image->symbols[index].value = prg_read_be32(data + offset + 10u);
    }

    return 1;
}

static int prg_apply_relocation(prg_image_t *image, uint32_t offset,
                                uint32_t load_address, char *error_text,
                                size_t error_size)
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
                                 size_t size, uint32_t load_address,
                                 char *error_text, size_t error_size)
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

    if (!prg_apply_relocation(image, offset, load_address, error_text,
                              error_size)) {
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

        offset += (uint32_t)delta;
        if (!prg_apply_relocation(image, offset, load_address, error_text,
                                  error_size)) {
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

int prg_read_memory(const void *input, size_t input_size, uint32_t load_address,
                    prg_image_t *image, char *error_text, size_t error_size)
{
    const size_t header_size = 28u;
    const uint8_t *data = (const uint8_t *)input;
    size_t image_size;
    size_t cursor;

    if (data == NULL || image == NULL) {
        prg_set_error(error_text, error_size, "invalid arguments");
        return 0;
    }

    prg_image_free(image);
    if (!prg_parse_header(image, data, input_size, error_text, error_size)) {
        return 0;
    }

    image_size =
        (size_t)image->header.text_size + (size_t)image->header.data_size;
    cursor = header_size;

    if (input_size < cursor || input_size - cursor < image_size) {
        prg_set_error(error_text, error_size,
                      "file truncated before text/data image");
        return 0;
    }

    image->image = (uint8_t *)malloc(image_size);
    if (image->image == NULL) {
        prg_set_error(error_text, error_size, "out of memory");
        return 0;
    }

    memcpy(image->image, data + cursor, image_size);
    image->image_size = image_size;
    cursor += image_size;

    if (input_size < cursor || input_size - cursor < image->header.symbol_size) {
        prg_image_free(image);
        prg_set_error(error_text, error_size,
                      "file truncated before symbol table");
        return 0;
    }

    if (!prg_parse_symbols(image, data + cursor,
                           (size_t)image->header.symbol_size, error_text,
                           error_size)) {
        prg_image_free(image);
        return 0;
    }
    cursor += (size_t)image->header.symbol_size;

    if (!prg_apply_relocations(image, data + cursor, input_size - cursor,
                               load_address,
                               error_text, error_size)) {
        prg_image_free(image);
        return 0;
    }

    return 1;
}

int prg_read_file(const char *path, uint32_t load_address, prg_image_t *image,
                  char *error_text, size_t error_size)
{
    prg_file_data_t file_data;
    int result;

    if (path == NULL || image == NULL) {
        prg_set_error(error_text, error_size, "invalid arguments");
        return 0;
    }
    memset(&file_data, 0, sizeof(file_data));
    if (!prg_read_whole_file(path, &file_data, error_text, error_size)) {
        return 0;
    }
    result = prg_read_memory(file_data.data, file_data.size, load_address,
                             image, error_text, error_size);
    free(file_data.data);
    return result;
}

const prg_symbol_t *prg_find_symbol(const prg_image_t *image, const char *name)
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
