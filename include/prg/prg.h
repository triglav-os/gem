/*
 * Declares a reusable Atari ST executable loader for `.PRG` and `.APP`
 * files. The loader parses the standard program header, optional DRI
 * symbol table, and relocation stream, and exposes a relocated text+data
 * image that other tools can execute or inspect.
 *
 * MIT License (see: LICENSE)
 * Copyright (C) 2026 tomaz stih
 */

#ifndef PRG_PRG_H
#define PRG_PRG_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define PRG_ERROR_LENGTH 256u

typedef struct prg_symbol {
    char name[9];
    uint16_t type;
    uint32_t value;
} prg_symbol_t;

typedef struct prg_header {
    uint16_t branch;
    uint32_t text_size;
    uint32_t data_size;
    uint32_t bss_size;
    uint32_t symbol_size;
    uint32_t reserved;
    uint32_t flags;
    uint16_t absflag;
} prg_header_t;

typedef struct prg_image {
    prg_header_t header;
    uint8_t *image;
    size_t image_size;
    prg_symbol_t *symbols;
    size_t symbol_count;
} prg_image_t;

/*
 * Initializes an empty PRG image object.
 */
void prg_image_init(prg_image_t *image);

/*
 * Releases all storage owned by a PRG image object.
 */
void prg_image_free(prg_image_t *image);

/*
 * Reads and relocates an Atari ST `.PRG` or `.APP` executable.
 */
int prg_read_file(const char *path, uint32_t load_address,
    prg_image_t *image, char *error_text, size_t error_size);

/*
 * Finds one symbol by exact name and returns it, or NULL when missing.
 */
const prg_symbol_t *prg_find_symbol(const prg_image_t *image,
    const char *name);

#ifdef __cplusplus
}
#endif

#endif
