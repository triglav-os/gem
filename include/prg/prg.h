/*
 * Declares a reusable Atari ST executable loader for `.PRG` and `.APP`
 * files. The loader parses the standard TOS program header, optional
 * DRI/GST symbol table, and relocation stream, then exposes a relocated
 * text+data image that tools such as emulators, analyzers, and archive
 * utilities can inspect or execute.
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

/*
 * Recommended fixed-size buffer length for short human-readable error
 * strings returned by this library.
 */
#define PRG_ERROR_LENGTH 256u

/*
 * One parsed symbol-table entry from an Atari executable.
 *
 * Members:
 *      name   - Symbol name, trimmed and NUL-terminated. Atari DRI/GST
 *               names are eight characters wide on disk, so this buffer
 *               stores up to eight visible characters plus the
 *               terminator.
 *      type   - Raw symbol type word from the executable.
 *      value  - Relocated symbol value relative to the program load
 *               address used when the image was loaded.
 *
 * Notes:
 *      The loader preserves the raw type value so later tools can
 *      interpret DRI/GST classes without losing information.
 */
typedef struct prg_symbol {
    char name[9];
    uint16_t type;
    uint32_t value;
} prg_symbol_t;

/*
 * Parsed Atari ST executable header.
 *
 * Members:
 *      branch      - Initial branch word at the start of the program.
 *      text_size   - Size of the text segment in bytes.
 *      data_size   - Size of the initialized data segment in bytes.
 *      bss_size    - Size of the zero-initialized BSS segment in bytes.
 *      symbol_size - Byte size of the optional symbol table.
 *      reserved    - Reserved longword from the on-disk header.
 *      flags       - TOS program flags field.
 *      absflag     - Non-zero when the image is absolute and does not
 *                    carry a relocation stream.
 *
 * Notes:
 *      The loader exposes the raw header verbatim because higher-level
 *      tools often need to inspect original executable metadata rather
 *      than only the relocated image bytes.
 */
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

/*
 * Loaded Atari executable image.
 *
 * Members:
 *      header       - Parsed on-disk executable header.
 *      image        - Relocated text+data bytes owned by this object.
 *      image_size   - Byte size of `image`.
 *      symbols      - Optional parsed symbol table owned by this object.
 *      symbol_count - Number of entries in `symbols`.
 *
 * Notes:
 *      The BSS segment is described by `header.bss_size` but is not
 *      appended to `image`. Tools that need a full runtime address space
 *      should allocate separate zero-filled BSS storage themselves.
 */
typedef struct prg_image {
    prg_header_t header;
    uint8_t *image;
    size_t image_size;
    prg_symbol_t *symbols;
    size_t symbol_count;
} prg_image_t;

/*
 * Initializes an empty PRG image object.
 *
 * Parameters:
 *      image        - Object to initialize.
 *
 * Returns:
 *      Nothing.
 *
 * Notes:
 *      Call this before first use or after manual stack allocation of a
 *      `prg_image_t`.
 */
void prg_image_init(prg_image_t *image);

/*
 * Releases all storage owned by a PRG image object.
 *
 * Parameters:
 *      image        - Object to clear.
 *
 * Returns:
 *      Nothing.
 *
 * Notes:
 *      This frees the relocated text+data image and parsed symbol table,
 *      then resets the structure back to the same empty state produced
 *      by `prg_image_init()`.
 */
void prg_image_free(prg_image_t *image);

/*
 * Reads and relocates an Atari ST `.PRG` or `.APP` executable.
 *
 * Parameters:
 *      path         - Filesystem path to the executable.
 *      load_address - Base address to apply during relocation.
 *      image        - Destination image object to populate.
 *      error_text   - Optional error buffer for a short message.
 *      error_size   - Size of `error_text` in bytes.
 *
 * Returns:
 *      Non-zero on success, otherwise zero.
 *
 * Notes:
 *      On success, `image->image` contains the relocated text+data
 *      bytes. The load address influences both the relocation fixups and
 *      the symbol values reported through `prg_find_symbol()`.
 */
int prg_read_file(const char *path, uint32_t load_address,
    prg_image_t *image, char *error_text, size_t error_size);

/*
 * Finds one loaded symbol by exact name.
 *
 * Parameters:
 *      image        - Loaded executable image.
 *      name         - Exact symbol name to look up.
 *
 * Returns:
 *      Pointer to the matching symbol, or NULL when absent.
 *
 * Notes:
 *      Names are compared after the loader has trimmed the fixed-width
 *      on-disk form. Matching is case-sensitive.
 */
const prg_symbol_t *prg_find_symbol(const prg_image_t *image,
    const char *name);

#ifdef __cplusplus
}
#endif

#endif
