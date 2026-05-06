/*
 * Implements private helpers used by the minimal GEM VDI layer. These
 * routines keep drawing, font lookup, event pumping, and MFDB access
 * separate from the public workstation entry points in `vdi.c`.
 *
 * MIT License (see: LICENSE)
 * Copyright (C) 2026 tomaz stih
 */

#include "_vdi.h"

#include "gem/aes.h"

#include "platform/hid.h"
#include "platform/raster.h"

#include <dirent.h>
#include <stdio.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

vdi_state_t _vdi;

#define VDI_WORD_HEX(value) ((WORD) (UWORD) (value))

enum {
    _vdi_cursor_width = 16,
    _vdi_cursor_height = 16
};

enum {
    _vdi_cursor_arrow_hot_x = 0,
    _vdi_cursor_arrow_hot_y = 0,
    _vdi_cursor_bee_hot_x = 7,
    _vdi_cursor_bee_hot_y = 7
};

/*
 * Built-in GEM mouse forms derived from the supplied Atari ST cursor
 * artwork. The arrow uses a (0,0) hot spot. The bee is centered so the
 * user points with the middle of the sprite rather than a corner.
 */
static const MFORM g_vdi_arrow_form = {
    0, 0, 1, BLACK, WHITE,
    {
        VDI_WORD_HEX(0xc000), VDI_WORD_HEX(0xe000),
        VDI_WORD_HEX(0xf000), VDI_WORD_HEX(0xf800),
        VDI_WORD_HEX(0xfc00), VDI_WORD_HEX(0xfe00),
        VDI_WORD_HEX(0xff00), VDI_WORD_HEX(0xff80),
        VDI_WORD_HEX(0xffc0), VDI_WORD_HEX(0xffe0),
        VDI_WORD_HEX(0xfe00), VDI_WORD_HEX(0xef00),
        VDI_WORD_HEX(0xcf00), VDI_WORD_HEX(0x8780),
        VDI_WORD_HEX(0x0780), VDI_WORD_HEX(0x0380)
    },
    {
        VDI_WORD_HEX(0x0000), VDI_WORD_HEX(0x4000),
        VDI_WORD_HEX(0x6000), VDI_WORD_HEX(0x7000),
        VDI_WORD_HEX(0x7800), VDI_WORD_HEX(0x7c00),
        VDI_WORD_HEX(0x7e00), VDI_WORD_HEX(0x7f00),
        VDI_WORD_HEX(0x7f80), VDI_WORD_HEX(0x7c00),
        VDI_WORD_HEX(0x6c00), VDI_WORD_HEX(0x4600),
        VDI_WORD_HEX(0x0600), VDI_WORD_HEX(0x0300),
        VDI_WORD_HEX(0x0300), VDI_WORD_HEX(0x0000)
    }
};
static const MFORM g_vdi_bee_form = {
    7, 7, 1, BLACK, WHITE,
    {
        VDI_WORD_HEX(0x0e00), VDI_WORD_HEX(0x0e1f),
        VDI_WORD_HEX(0x0e3f), VDI_WORD_HEX(0x077f),
        VDI_WORD_HEX(0xe7ff), VDI_WORD_HEX(0xffff),
        VDI_WORD_HEX(0xffff), VDI_WORD_HEX(0x1fff),
        VDI_WORD_HEX(0x0fff), VDI_WORD_HEX(0x1ffe),
        VDI_WORD_HEX(0x3fff), VDI_WORD_HEX(0x7fff),
        VDI_WORD_HEX(0x7fff), VDI_WORD_HEX(0x7fff),
        VDI_WORD_HEX(0x7fff), VDI_WORD_HEX(0x7fbf)
    },
    {
        VDI_WORD_HEX(0x0000), VDI_WORD_HEX(0x0400),
        VDI_WORD_HEX(0x041e), VDI_WORD_HEX(0x0031),
        VDI_WORD_HEX(0x0361), VDI_WORD_HEX(0x6342),
        VDI_WORD_HEX(0x0cc5), VDI_WORD_HEX(0x0daa),
        VDI_WORD_HEX(0x0370), VDI_WORD_HEX(0x0eac),
        VDI_WORD_HEX(0x19fe), VDI_WORD_HEX(0x30b0),
        VDI_WORD_HEX(0x216f), VDI_WORD_HEX(0x226c),
        VDI_WORD_HEX(0x252b), VDI_WORD_HEX(0x1a0a)
    }
};
static uint16_t g_vdi_cursor_saved[_vdi_cursor_height];

static void _vdi_set_screen_pixel_raw(WORD x, WORD y, WORD color);
static WORD _vdi_get_screen_pixel_raw(WORD x, WORD y);
static void _vdi_cursor_restore(void);
static void _vdi_cursor_draw(void);
void _vdi_prepare_screen_write(void);

static const char *cursor_resource_path(void)
{
#ifdef GEM_CURSOR_RSC
    return GEM_CURSOR_RSC;
#else
    return "bin/cursors.rsc";
#endif
}

static int _vdi_load_binary_file(const char *path,
                                 uint8_t **data_out,
                                 size_t *size_out)
{
    FILE *stream;
    long size;
    uint8_t *data;

    if (path == NULL || data_out == NULL || size_out == NULL) {
        return 0;
    }

    stream = fopen(path, "rb");
    if (stream == NULL) {
        return 0;
    }
    if (fseek(stream, 0L, SEEK_END) != 0) {
        fclose(stream);
        return 0;
    }
    size = ftell(stream);
    if (size < 0 || fseek(stream, 0L, SEEK_SET) != 0) {
        fclose(stream);
        return 0;
    }

    data = malloc((size_t) size);
    if (data == NULL) {
        fclose(stream);
        return 0;
    }
    if (size > 0 && fread(data, (size_t) size, 1, stream) != 1) {
        free(data);
        fclose(stream);
        return 0;
    }
    fclose(stream);

    *data_out = data;
    *size_out = (size_t) size;
    return 1;
}

static const void *_vdi_resource_ptr(const uint8_t *base,
                                     size_t size,
                                     LONG offset,
                                     size_t bytes)
{
    size_t start;

    if (base == NULL || offset < 0) {
        return NULL;
    }

    start = (size_t) offset;
    if (start > size || bytes > size - start) {
        return NULL;
    }
    return base + start;
}

static WORD _vdi_cursor_hot_x_for_selector(WORD selector)
{
    return (selector == HGLASS) ? _vdi_cursor_bee_hot_x :
        _vdi_cursor_arrow_hot_x;
}

static WORD _vdi_cursor_hot_y_for_selector(WORD selector)
{
    return (selector == HGLASS) ? _vdi_cursor_bee_hot_y :
        _vdi_cursor_arrow_hot_y;
}

static void _vdi_copy_builtin_cursor_forms(void)
{
    WORD selector;

    for (selector = 0; selector < _vdi_standard_cursor_count; ++selector) {
        _vdi.standard_mouse_forms[selector] = g_vdi_arrow_form;
    }
    _vdi.standard_mouse_forms[ARROW] = g_vdi_arrow_form;
    _vdi.standard_mouse_forms[HGLASS] = g_vdi_bee_form;
}

static int _vdi_load_iconblk_cursor(const uint8_t *base,
                                    size_t size,
                                    const ICONBLK *icon,
                                    WORD selector,
                                    MFORM *out)
{
    const WORD *mask;
    const WORD *data;
    WORD row;

    if (base == NULL || icon == NULL || out == NULL ||
        icon->ib_wicon != _vdi_cursor_width ||
        icon->ib_hicon != _vdi_cursor_height) {
        return 0;
    }

    mask = _vdi_resource_ptr(base, size, icon->ib_pmask,
        sizeof(out->mf_mask));
    data = _vdi_resource_ptr(base, size, icon->ib_pdata,
        sizeof(out->mf_data));
    if (mask == NULL || data == NULL) {
        return 0;
    }

    memset(out, 0, sizeof(*out));
    if (icon->ib_xchar >= 0 && icon->ib_xchar < icon->ib_wicon &&
        icon->ib_ychar >= 0 && icon->ib_ychar < icon->ib_hicon) {
        out->mf_xhot = icon->ib_xchar;
        out->mf_yhot = icon->ib_ychar;
    } else {
        out->mf_xhot = _vdi_cursor_hot_x_for_selector(selector);
        out->mf_yhot = _vdi_cursor_hot_y_for_selector(selector);
    }
    out->mf_nplanes = 1;
    out->mf_fg = BLACK;
    out->mf_bg = WHITE;
    for (row = 0; row < _vdi_cursor_height; ++row) {
        out->mf_mask[row] = mask[row];
        out->mf_data[row] = data[row];
    }
    return 1;
}

WORD _vdi_load_standard_mouse_forms(void)
{
    const char *path = cursor_resource_path();
    uint8_t *resource = NULL;
    size_t resource_size = 0;
    const RSHDR *header;
    const ICONBLK *icons;
    WORD selector;

    _vdi_copy_builtin_cursor_forms();

    if (!_vdi_load_binary_file(path, &resource, &resource_size)) {
        _vdi.mouse_form = _vdi.standard_mouse_forms[ARROW];
        return 1;
    }
    if (resource_size < sizeof(RSHDR)) {
        free(resource);
        _vdi.mouse_form = _vdi.standard_mouse_forms[ARROW];
        return 1;
    }

    header = (const RSHDR *) resource;
    icons = _vdi_resource_ptr(resource, resource_size, header->rsh_iconblk,
        sizeof(ICONBLK) * (size_t) _vdi_standard_cursor_count);
    if (header->rsh_nib >= _vdi_standard_cursor_count && icons != NULL) {
        for (selector = 0; selector < _vdi_standard_cursor_count;
            ++selector) {
            MFORM form;

            if (_vdi_load_iconblk_cursor(resource, resource_size,
                &icons[selector], selector, &form)) {
                _vdi.standard_mouse_forms[selector] = form;
            }
        }
    }

    free(resource);
    _vdi.mouse_form = _vdi.standard_mouse_forms[ARROW];
    return 1;
}

WORD _vdi_set_mouse_form(const MFORM *form)
{
    if (form == NULL) {
        return 0;
    }

    _vdi_cursor_restore();
    _vdi.mouse_form = *form;
    _vdi.cursor_x = (WORD) (_vdi.mouse_x - _vdi.mouse_form.mf_xhot);
    _vdi.cursor_y = (WORD) (_vdi.mouse_y - _vdi.mouse_form.mf_yhot);
    _vdi_cursor_draw();
    if (_vdi.update_depth == 0) {
        gem_raster_present();
    } else {
        _vdi.present_pending = 1;
    }
    return 1;
}

WORD _vdi_select_system_mouse_form(WORD selector)
{
    if (selector < 0 || selector >= _vdi_standard_cursor_count) {
        selector = ARROW;
    }

    return _vdi_set_mouse_form(&_vdi.standard_mouse_forms[selector]);
}

enum {
    _vdi_max_fonts = 16,
    _vdi_font_id_system = 1,
    _vdi_font_id_small_internal = 2,
    _vdi_font_id_ibm = 3,
    _vdi_font_id_small = 5
};

typedef struct vdi_font {
    WORD font_id;
    char name[33];
    char file_name[64];
    WORD first_ade;
    WORD last_ade;
    WORD top;
    WORD ascent;
    WORD half;
    WORD descent;
    WORD bottom;
    WORD max_char_width;
    WORD max_cell_width;
    WORD form_width;
    WORD form_height;
    uint32_t data_offset;
    uint32_t off_offset;
    uint8_t *data;
    size_t data_size;
    WORD uniform_width;
    int present;
    int resident;
} vdi_font_t;

static vdi_font_t g_vdi_fonts[_vdi_max_fonts];
static WORD g_vdi_current_font = _vdi_font_id_system;
static int g_vdi_fonts_loaded;

static uint16_t read_le16(const uint8_t *bytes)
{
    return (uint16_t) bytes[0] | ((uint16_t) bytes[1] << 8);
}

static uint32_t read_le32(const uint8_t *bytes)
{
    return (uint32_t) read_le16(bytes) | ((uint32_t) read_le16(bytes + 2) << 16);
}

static const char *font_dir(void)
{
#ifdef GEM_FONT_DIR
    return GEM_FONT_DIR;
#else
    return "bin/fonts";
#endif
}

static int is_font_file_name(const char *file_name)
{
    size_t length;

    if (file_name == NULL) {
        return 0;
    }
    length = strlen(file_name);
    if (length < 5u) {
        return 0;
    }

    return strcmp(file_name + length - 4u, ".fnt") == 0 ||
        strcmp(file_name + length - 4u, ".FNT") == 0;
}

static int is_system_file_name(const char *file_name)
{
    return file_name != NULL &&
        (strcmp(file_name, "AtariSTHigh.fnt") == 0 ||
        strcmp(file_name, "ATARISTHIGH.FNT") == 0);
}

static int is_small_file_name(const char *file_name)
{
    return file_name != NULL &&
        (strcmp(file_name, "8x16_z1.fnt") == 0 ||
        strcmp(file_name, "8X16_Z1.FNT") == 0);
}

static WORD find_font_slot_by_id(WORD font_id)
{
    WORD slot;

    for (slot = 0; slot < _vdi_max_fonts; ++slot) {
        if (g_vdi_fonts[slot].present && g_vdi_fonts[slot].font_id == font_id) {
            return slot;
        }
    }
    return -1;
}

static WORD font_id_from_alias(WORD font_id)
{
    if (font_id == _vdi_font_id_ibm) {
        return _vdi_font_id_system;
    }
    if (font_id == _vdi_font_id_small) {
        return _vdi_font_id_small_internal;
    }
    return font_id;
}

static vdi_font_t *find_font_by_id(WORD font_id)
{
    WORD slot = find_font_slot_by_id(font_id_from_alias(font_id));

    if (slot < 0) {
        return NULL;
    }
    return &g_vdi_fonts[slot];
}

static vdi_font_t *current_font(void)
{
    return find_font_by_id(g_vdi_current_font);
}

static WORD glyph_index(const vdi_font_t *font, unsigned int ch)
{
    if (font == NULL || !font->resident) {
        return -1;
    }
    if (ch < (unsigned int) font->first_ade || ch > (unsigned int) font->last_ade) {
        return -1;
    }
    return (WORD) (ch - (unsigned int) font->first_ade);
}

static WORD glyph_start_bit(const vdi_font_t *font, WORD index)
{
    const uint8_t *table;

    if (font == NULL || !font->resident || index < 0) {
        return 0;
    }

    table = font->data + font->off_offset + (size_t) index * 2u;
    return (WORD) read_le16(table);
}

static WORD glyph_end_bit(const vdi_font_t *font, WORD index)
{
    const uint8_t *table;

    if (font == NULL || !font->resident || index < 0) {
        return 0;
    }

    table = font->data + font->off_offset + (size_t) (index + 1) * 2u;
    return (WORD) read_le16(table);
}

static WORD glyph_width(const vdi_font_t *font, unsigned int ch)
{
    WORD index;

    if (font == NULL || !font->resident) {
        return 0;
    }
    if (font->uniform_width > 0) {
        return font->uniform_width;
    }
    index = glyph_index(font, ch);
    if (index < 0) {
        return font->max_char_width;
    }
    return (WORD) (glyph_end_bit(font, index) - glyph_start_bit(font, index));
}

static void clear_font_runtime(vdi_font_t *font)
{
    if (font == NULL) {
        return;
    }

    free(font->data);
    font->first_ade = 0;
    font->last_ade = 0;
    font->top = 0;
    font->ascent = 0;
    font->half = 0;
    font->descent = 0;
    font->bottom = 0;
    font->max_char_width = 0;
    font->max_cell_width = 0;
    font->form_width = 0;
    font->form_height = 0;
    font->data_offset = 0u;
    font->off_offset = 0u;
    font->data = NULL;
    font->data_size = 0u;
    font->uniform_width = 0;
    font->resident = 0;
}

static void clear_font_slot(vdi_font_t *font)
{
    if (font == NULL) {
        return;
    }

    clear_font_runtime(font);
    memset(font, 0, sizeof(*font));
}

static int load_font_file(vdi_font_t *font, WORD font_id, const char *file_name)
{
    BYTE path[512];
    char stored_file_name[64];
    FILE *stream;
    long file_size;
    uint8_t *data;
    uint32_t fields[3];
    uint32_t data_offset = 0u;
    uint32_t off_offset = 0u;
    size_t bitmap_size;
    size_t off_size;
    int i;

    strncpy(stored_file_name, file_name, sizeof(stored_file_name) - 1u);
    stored_file_name[sizeof(stored_file_name) - 1u] = '\0';
    sprintf((char *) path, "%s/%s", font_dir(), stored_file_name);
    stream = fopen((const char *) path, "rb");
    if (stream == NULL) {
        return 0;
    }

    if (fseek(stream, 0L, SEEK_END) != 0) {
        fclose(stream);
        return 0;
    }
    file_size = ftell(stream);
    if (file_size < 88L || fseek(stream, 0L, SEEK_SET) != 0) {
        fclose(stream);
        return 0;
    }

    data = malloc((size_t) file_size);
    if (data == NULL) {
        fclose(stream);
        return 0;
    }
    if (fread(data, 1u, (size_t) file_size, stream) != (size_t) file_size) {
        fclose(stream);
        free(data);
        return 0;
    }
    fclose(stream);

    clear_font_runtime(font);
    font->font_id = font_id;
    strncpy(font->file_name, stored_file_name, sizeof(font->file_name) - 1u);
    font->file_name[sizeof(font->file_name) - 1u] = '\0';
    memcpy(font->name, data + 4, 32u);
    font->name[32] = '\0';
    font->first_ade = (WORD) read_le16(data + 36);
    font->last_ade = (WORD) read_le16(data + 38);
    font->top = (WORD) read_le16(data + 40);
    font->ascent = (WORD) read_le16(data + 42);
    font->half = (WORD) read_le16(data + 44);
    font->descent = (WORD) read_le16(data + 46);
    font->bottom = (WORD) read_le16(data + 48);
    font->max_char_width = (WORD) read_le16(data + 50);
    font->max_cell_width = (WORD) read_le16(data + 52);
    font->form_width = (WORD) read_le16(data + 80);
    font->form_height = (WORD) read_le16(data + 82);
    font->data = data;
    font->data_size = (size_t) file_size;

    bitmap_size = (size_t) font->form_width * (size_t) font->form_height;
    off_size = (size_t) (font->last_ade - font->first_ade + 2) * 2u;
    fields[0] = read_le32(data + 68);
    fields[1] = read_le32(data + 72);
    fields[2] = read_le32(data + 76);

    for (i = 0; i < 3; ++i) {
        if (fields[i] != 0u && fields[i] + bitmap_size <= (size_t) file_size) {
            data_offset = fields[i];
        }
        if (fields[i] != 0u && fields[i] + off_size <= (size_t) file_size) {
            if (data_offset == 0u || fields[i] > data_offset) {
                off_offset = fields[i];
            }
        }
    }
    if (data_offset == 0u || off_offset == 0u || off_offset <= data_offset) {
        free(data);
        clear_font_runtime(font);
        return 0;
    }

    font->data_offset = data_offset;
    font->off_offset = off_offset;

    /* Check if all glyphs have uniform width for O(1) width lookup */
    {
        WORD num_glyphs = (WORD) (font->last_ade - font->first_ade + 1);
        WORD w0 = 0;
        WORD uniform = 1;
        WORD g;

        if (num_glyphs > 0 && off_offset + (size_t)(num_glyphs + 1u) * 2u <= (size_t) file_size) {
            const uint8_t *ot = data + off_offset;

            w0 = (WORD) (read_le16(ot + 2u) - read_le16(ot));
            for (g = 1; g < num_glyphs; ++g) {
                WORD w = (WORD) (read_le16(ot + (size_t)(g + 1u) * 2u) -
                    read_le16(ot + (size_t) g * 2u));

                if (w != w0) {
                    uniform = 0;
                    break;
                }
            }
        } else {
            uniform = 0;
        }
        font->uniform_width = (uniform != 0) ? w0 : 0;
    }

    font->present = 1;
    font->resident = 1;
    return 1;
}

static WORD reserve_font_slot(WORD preferred_slot, WORD font_id,
                              const char *file_name)
{
    WORD slot;
    WORD free_slot = -1;

    if (file_name == NULL) {
        return -1;
    }

    for (slot = 0; slot < _vdi_max_fonts; ++slot) {
        if (g_vdi_fonts[slot].present &&
            strcmp(g_vdi_fonts[slot].file_name, file_name) == 0) {
            return slot;
        }
        if (!g_vdi_fonts[slot].present && free_slot < 0) {
            free_slot = slot;
        }
    }

    if (preferred_slot >= 0 && preferred_slot < _vdi_max_fonts &&
        !g_vdi_fonts[preferred_slot].present) {
        free_slot = preferred_slot;
    }
    if (free_slot < 0) {
        return -1;
    }

    clear_font_slot(&g_vdi_fonts[free_slot]);
    g_vdi_fonts[free_slot].font_id = font_id;
    strncpy(g_vdi_fonts[free_slot].file_name, file_name,
        sizeof(g_vdi_fonts[free_slot].file_name) - 1u);
    g_vdi_fonts[free_slot].file_name[sizeof(g_vdi_fonts[free_slot].file_name) -
        1u] = '\0';
    g_vdi_fonts[free_slot].present = 1;
    return free_slot;
}

static WORD next_dynamic_font_id(void)
{
    WORD font_id = 1;
    WORD slot;

    for (;;) {
        int in_use = 0;

        if (font_id == _vdi_font_id_ibm || font_id == _vdi_font_id_small) {
            ++font_id;
            continue;
        }
        for (slot = 0; slot < _vdi_max_fonts; ++slot) {
            if (g_vdi_fonts[slot].present && g_vdi_fonts[slot].font_id == font_id) {
                in_use = 1;
                break;
            }
        }
        if (!in_use) {
            return font_id;
        }
        ++font_id;
    }
}

static int load_font_slot(vdi_font_t *font)
{
    if (font == NULL || !font->present) {
        return 0;
    }
    if (font->resident) {
        return 1;
    }
    return load_font_file(font, font->font_id, font->file_name);
}

static void unload_external_font_slots(void)
{
    WORD slot;

    for (slot = 0; slot < _vdi_max_fonts; ++slot) {
        if (g_vdi_fonts[slot].present &&
            !is_system_file_name(g_vdi_fonts[slot].file_name) &&
            !is_small_file_name(g_vdi_fonts[slot].file_name)) {
            clear_font_slot(&g_vdi_fonts[slot]);
        }
    }
}

static WORD _vdi_clamp_word(WORD value, WORD low, WORD high)
{
    if (value < low) {
        return low;
    }
    if (value > high) {
        return high;
    }
    return value;
}

int _vdi_load_fonts(void)
{
    if (g_vdi_fonts_loaded != 0) {
        return 1;
    }

    memset(g_vdi_fonts, 0, sizeof(g_vdi_fonts));
    if (!load_font_file(&g_vdi_fonts[0], _vdi_font_id_system,
        "AtariSTHigh.fnt")) {
        return 0;
    }
    if (!load_font_file(&g_vdi_fonts[1], _vdi_font_id_small_internal,
        "8x16_z1.fnt")) {
        clear_font_slot(&g_vdi_fonts[0]);
        return 0;
    }

    g_vdi_fonts_loaded = 1;
    g_vdi_current_font = _vdi_font_id_system;
    return 1;
}

WORD _vdi_load_external_fonts(void)
{
    DIR *directory;
    struct dirent *entry;

    if (!_vdi_load_fonts()) {
        return 0;
    }

    directory = opendir(font_dir());
    if (directory == NULL) {
        return _vdi_font_count();
    }

    while ((entry = readdir(directory)) != NULL) {
        WORD slot;
        WORD font_id;

        if (!is_font_file_name(entry->d_name) ||
            is_system_file_name(entry->d_name) ||
            is_small_file_name(entry->d_name)) {
            continue;
        }

        font_id = next_dynamic_font_id();
        slot = reserve_font_slot(-1, font_id, entry->d_name);
        if (slot >= 0) {
            (void) load_font_slot(&g_vdi_fonts[slot]);
        }
    }

    closedir(directory);
    return _vdi_font_count();
}

WORD _vdi_unload_external_fonts(void)
{
    vdi_font_t *current = current_font();

    unload_external_font_slots();
    if (current == NULL || !current->present || !current->resident) {
        g_vdi_current_font = _vdi_font_id_system;
    }
    return 0;
}

void _vdi_unload_fonts(void)
{
    WORD slot;

    for (slot = 0; slot < _vdi_max_fonts; ++slot) {
        clear_font_slot(&g_vdi_fonts[slot]);
    }
    g_vdi_current_font = _vdi_font_id_system;
    g_vdi_fonts_loaded = 0;
}

WORD _vdi_select_font(WORD font_id)
{
    WORD resolved = font_id_from_alias(font_id);
    vdi_font_t *font;

    if (!_vdi_load_fonts()) {
        return 0;
    }
    font = find_font_by_id(resolved);
    if (font == NULL || !load_font_slot(font)) {
        g_vdi_current_font = _vdi_font_id_system;
        return g_vdi_current_font;
    }

    g_vdi_current_font = resolved;
    return g_vdi_current_font;
}

WORD _vdi_default_font_id(void)
{
    return _vdi_font_id_system;
}

WORD _vdi_font_count(void)
{
    WORD count = 0;
    WORD slot;

    for (slot = 0; slot < _vdi_max_fonts; ++slot) {
        if (g_vdi_fonts[slot].present && g_vdi_fonts[slot].resident) {
            ++count;
        }
    }
    return count;
}

WORD _vdi_font_id_for_element(WORD element_num)
{
    WORD count = 0;
    WORD slot;

    for (slot = 0; slot < _vdi_max_fonts; ++slot) {
        if (g_vdi_fonts[slot].present && g_vdi_fonts[slot].resident) {
            ++count;
            if (count == element_num) {
                return g_vdi_fonts[slot].font_id;
            }
        }
    }
    return 0;
}

const char *_vdi_font_name(WORD element_num)
{
    WORD count = 0;
    WORD slot;

    for (slot = 0; slot < _vdi_max_fonts; ++slot) {
        if (g_vdi_fonts[slot].present && g_vdi_fonts[slot].resident) {
            ++count;
            if (count == element_num) {
                return g_vdi_fonts[slot].name;
            }
        }
    }
    return NULL;
}

WORD _vdi_font_cell_width(void)
{
    vdi_font_t *font = current_font();

    return (font != NULL) ? font->max_char_width : 0;
}

WORD _vdi_font_text_height(void)
{
    vdi_font_t *font = current_font();

    return (font != NULL) ? font->form_height : 0;
}

WORD _vdi_font_ascent(void)
{
    vdi_font_t *font = current_font();

    return (font != NULL) ? font->ascent : 0;
}

WORD _vdi_font_first_ade(void)
{
    vdi_font_t *font = current_font();

    return (font != NULL) ? font->first_ade : 0;
}

WORD _vdi_font_last_ade(void)
{
    vdi_font_t *font = current_font();

    return (font != NULL) ? font->last_ade : 0;
}

WORD _vdi_char_cell_width(char ch)
{
    vdi_font_t *font = current_font();

    return glyph_width(font, (unsigned char) ch);
}

WORD _vdi_string_width(const char *string)
{
    vdi_font_t *font;
    WORD width;

    if (string == NULL) {
        return 0;
    }

    font = current_font();
    if (font != NULL && font->uniform_width > 0) {
        return (WORD) (strlen(string) * (size_t) font->uniform_width);
    }

    width = 0;
    while (*string != '\0') {
        width = (WORD) (width + _vdi_char_cell_width(*string));
        ++string;
    }
    return width;
}

static uint8_t *_vdi_screen_row_mutable(WORD y)
{
    if (_vdi.surface == NULL || _vdi.surface->pixels == NULL ||
        y < 0 || y >= _vdi.height) {
        return NULL;
    }

    return (uint8_t *) _vdi.surface->pixels +
        (size_t) y * _vdi.surface->pitch;
}

static const uint8_t *_vdi_screen_row_const(WORD y)
{
    if (_vdi.surface == NULL || _vdi.surface->pixels == NULL ||
        y < 0 || y >= _vdi.height) {
        return NULL;
    }

    return (const uint8_t *) _vdi.surface->pixels +
        (size_t) y * _vdi.surface->pitch;
}

static uint8_t _vdi_screen_mask_for_x(WORD x)
{
    return (uint8_t) (1u << (7u - ((unsigned int) x & 7u)));
}

static void _vdi_apply_mask(uint8_t *byte, uint8_t mask, WORD color)
{
    WORD mode;

    if (byte == NULL || mask == 0u) {
        return;
    }

    mode = _vdi_write_mode();
    switch (mode) {
    case 2:
        if (color != 0) {
            *byte |= mask;
        }
        break;
    case 3:
        if (color != 0) {
            *byte ^= mask;
        }
        break;
    case 4:
        *byte &= (uint8_t) ~mask;
        break;
    case 1:
    default:
        if (color != 0) {
            *byte |= mask;
        } else {
            *byte &= (uint8_t) ~mask;
        }
        break;
    }
}

static uint8_t _vdi_mfdb_sample_direct(const MFDB *mfdb, WORD x, WORD y)
{
    const UWORD *row;
    UWORD mask;

    if (_vdi_uses_screen(mfdb)) {
        const uint8_t *screen_row = _vdi_screen_row_const(y);

        if (screen_row == NULL || x < 0 || x >= _vdi.width) {
            return 0;
        }
        return (screen_row[(size_t) x / 8u] & _vdi_screen_mask_for_x(x)) != 0u ?
            1u : 0u;
    }
    if (mfdb == NULL || mfdb->fd_addr == NULL || mfdb->fd_nplanes != 1 ||
        x < 0 || y < 0 || x >= mfdb->fd_w || y >= mfdb->fd_h) {
        return 0;
    }

    row = (const UWORD *) mfdb->fd_addr +
        (size_t) y * (size_t) mfdb->fd_wdwidth;
    mask = (UWORD) (0x8000u >> ((unsigned int) x & 15u));
    return (row[(size_t) x / 16u] & mask) != 0u ? 1u : 0u;
}

WORD _vdi_parse_env_word(const char *name, WORD fallback)
{
    const char *value = getenv(name);
    char *end = NULL;
    long parsed;

    if (value == NULL || value[0] == '\0') {
        return fallback;
    }

    parsed = strtol(value, &end, 10);
    if (end == value || *end != '\0' || parsed <= 0 || parsed > 32767L) {
        return fallback;
    }

    return (WORD) parsed;
}

void _vdi_pump_events(void)
{
}

void _vdi_set_mouse_state(WORD x, WORD y, WORD status)
{
    WORD hot_x = _vdi.mouse_form.mf_xhot;
    WORD hot_y = _vdi.mouse_form.mf_yhot;

    _vdi_cursor_restore();
    _vdi.mouse_x = x;
    _vdi.mouse_y = y;
    _vdi.mouse_status = status;
    _vdi.cursor_x = (WORD) (x - hot_x);
    _vdi.cursor_y = (WORD) (y - hot_y);
    _vdi_cursor_draw();
    if (_vdi.update_depth == 0) {
        gem_raster_present();
    } else {
        _vdi.present_pending = 1;
    }
}

void _vdi_begin_update(void)
{
    ++_vdi.update_depth;
}

void _vdi_end_update(void)
{
    if (_vdi.update_depth <= 0) {
        return;
    }

    --_vdi.update_depth;
    if (_vdi.update_depth == 0 && _vdi.present_pending != 0) {
        _vdi.present_pending = 0;
        _vdi_cursor_draw();
        gem_raster_present();
    }
    _vdi_pump_events();
}

void _vdi_present_screen(void)
{
    if (_vdi.update_depth > 0) {
        _vdi.present_pending = 1;
        _vdi_pump_events();
        return;
    }

    _vdi_cursor_draw();
    gem_raster_present();
    _vdi_pump_events();
}

static void _vdi_set_screen_pixel_raw(WORD x, WORD y, WORD color)
{
    uint8_t *row = _vdi_screen_row_mutable(y);
    uint8_t mask;

    if (row == NULL || x < 0 || x >= _vdi.width) {
        return;
    }

    mask = _vdi_screen_mask_for_x(x);
    _vdi_apply_mask(&row[(size_t) x / 8u], mask, color);
}

static WORD _vdi_get_screen_pixel_raw(WORD x, WORD y)
{
    const uint8_t *row = _vdi_screen_row_const(y);

    if (row == NULL || x < 0 || x >= _vdi.width) {
        return 0;
    }

    return (WORD)
        ((row[(size_t) x / 8u] & _vdi_screen_mask_for_x(x)) != 0u);
}

static void _vdi_cursor_restore(void)
{
    WORD y;

    if (_vdi.cursor_drawn == 0) {
        return;
    }

    for (y = 0; y < _vdi_cursor_height; ++y) {
        WORD x;

        for (x = 0; x < _vdi_cursor_width; ++x) {
            uint16_t bit = (uint16_t) (0x8000u >> x);

            if ((_vdi.mouse_form.mf_mask[y] & bit) == 0u) {
                continue;
            }
            _vdi_set_screen_pixel_raw((WORD) (_vdi.cursor_x + x),
                (WORD) (_vdi.cursor_y + y),
                (WORD) ((g_vdi_cursor_saved[y] & bit) != 0u));
        }
    }

    _vdi.cursor_drawn = 0;
}

static void _vdi_cursor_draw(void)
{
    WORD y;
    WORD fg_color;
    WORD bg_color;

    if (!_vdi.open || _vdi.cursor_hidden != 0 || _vdi.cursor_drawn != 0) {
        return;
    }

    fg_color = (_vdi.mouse_form.mf_fg == WHITE) ? 1 : 0;
    bg_color = (_vdi.mouse_form.mf_bg == WHITE) ? 1 : 0;

    for (y = 0; y < _vdi_cursor_height; ++y) {
        WORD x;
        uint16_t saved = 0u;

        for (x = 0; x < _vdi_cursor_width; ++x) {
            WORD px = (WORD) (_vdi.cursor_x + x);
            WORD py = (WORD) (_vdi.cursor_y + y);
            uint16_t bit = (uint16_t) (0x8000u >> x);

            if ((_vdi.mouse_form.mf_mask[y] & bit) == 0u) {
                continue;
            }
            if (_vdi_get_screen_pixel_raw(px, py) != 0) {
                saved |= bit;
            }
            if ((_vdi.mouse_form.mf_data[y] & bit) != 0u) {
                _vdi_set_screen_pixel_raw(px, py, fg_color);
            } else {
                _vdi_set_screen_pixel_raw(px, py, bg_color);
            }
        }
        g_vdi_cursor_saved[y] = saved;
    }

    _vdi.cursor_drawn = 1;
}

void _vdi_prepare_screen_write(void)
{
    _vdi_cursor_restore();
}

void _vdi_set_screen_pixel(WORD x, WORD y, WORD color)
{
    _vdi_prepare_screen_write();
    _vdi_set_screen_pixel_raw(x, y, color);
}

WORD _vdi_get_screen_pixel(WORD x, WORD y)
{
    return _vdi_get_screen_pixel_raw(x, y);
}

void _vdi_clear_screen(WORD color)
{
    uint8_t value = (uint8_t) ((color != 0) ? 0xffu : 0x00u);

    _vdi_prepare_screen_write();
    memset(_vdi.surface->pixels, value,
        (size_t) _vdi.surface->pitch * (size_t) _vdi.height);
}

int _vdi_clip_line_segment(WORD *x0, WORD *y0, WORD *x1, WORD *y1)
{
    enum {
        _vdi_clip_left = 1,
        _vdi_clip_right = 2,
        _vdi_clip_bottom = 4,
        _vdi_clip_top = 8
    };

    vdi_rect_t clip;

    if (x0 == NULL || y0 == NULL || x1 == NULL || y1 == NULL) {
        return 0;
    }

    _vdi_get_active_clip_rect(&clip);
    for (;;) {
        int code0 = 0;
        int code1 = 0;
        long x = 0;
        long y = 0;
        int out_code;

        if (*x0 < clip.x0) {
            code0 |= _vdi_clip_left;
        } else if (*x0 > clip.x1) {
            code0 |= _vdi_clip_right;
        }
        if (*y0 < clip.y0) {
            code0 |= _vdi_clip_top;
        } else if (*y0 > clip.y1) {
            code0 |= _vdi_clip_bottom;
        }

        if (*x1 < clip.x0) {
            code1 |= _vdi_clip_left;
        } else if (*x1 > clip.x1) {
            code1 |= _vdi_clip_right;
        }
        if (*y1 < clip.y0) {
            code1 |= _vdi_clip_top;
        } else if (*y1 > clip.y1) {
            code1 |= _vdi_clip_bottom;
        }

        if ((code0 | code1) == 0) {
            return 1;
        }
        if ((code0 & code1) != 0) {
            return 0;
        }

        out_code = (code0 != 0) ? code0 : code1;
        if ((out_code & _vdi_clip_top) != 0) {
            if (*y1 == *y0) {
                return 0;
            }
            x = *x0 + ((long) (*x1 - *x0) * (clip.y0 - *y0)) /
                (*y1 - *y0);
            y = clip.y0;
        } else if ((out_code & _vdi_clip_bottom) != 0) {
            if (*y1 == *y0) {
                return 0;
            }
            x = *x0 + ((long) (*x1 - *x0) * (clip.y1 - *y0)) /
                (*y1 - *y0);
            y = clip.y1;
        } else if ((out_code & _vdi_clip_right) != 0) {
            if (*x1 == *x0) {
                return 0;
            }
            y = *y0 + ((long) (*y1 - *y0) * (clip.x1 - *x0)) /
                (*x1 - *x0);
            x = clip.x1;
        } else {
            if (*x1 == *x0) {
                return 0;
            }
            y = *y0 + ((long) (*y1 - *y0) * (clip.x0 - *x0)) /
                (*x1 - *x0);
            x = clip.x0;
        }

        if (out_code == code0) {
            *x0 = (WORD) x;
            *y0 = (WORD) y;
        } else {
            *x1 = (WORD) x;
            *y1 = (WORD) y;
        }
    }
}

static void _vdi_hline_to_row(uint8_t *row, WORD left, WORD right, WORD color)
{
    WORD mode = _vdi_write_mode();
    size_t start_byte = (size_t) left / 8u;
    size_t end_byte = (size_t) right / 8u;
    unsigned int start_bit = (unsigned int) left & 7u;
    unsigned int end_bit = (unsigned int) right & 7u;
    uint8_t left_mask = (uint8_t) (0xffu >> start_bit);
    uint8_t right_mask = (uint8_t) (0xffu << (7u - end_bit));

#define _APPLY(byte_ref, mask) \
    switch (mode) { \
    case 2: if (color != 0) (byte_ref) |= (mask); break; \
    case 3: if (color != 0) (byte_ref) ^= (mask); break; \
    case 4: (byte_ref) &= (uint8_t) ~(mask); break; \
    default: \
        if (color != 0) (byte_ref) |= (mask); \
        else (byte_ref) &= (uint8_t) ~(mask); \
        break; \
    }

    if (start_byte == end_byte) {
        uint8_t mask = left_mask & right_mask;

        _APPLY(row[start_byte], mask);
        return;
    }

    _APPLY(row[start_byte], left_mask);

    if (end_byte > start_byte + 1u) {
        size_t mid_start = start_byte + 1u;
        size_t mid_count = end_byte - mid_start;

        switch (mode) {
        case 2:
            if (color != 0) {
                memset(&row[mid_start], 0xffu, mid_count);
            }
            break;
        case 4:
            memset(&row[mid_start], 0x00u, mid_count);
            break;
        case 3:
            if (color != 0) {
                size_t i;

                for (i = mid_start; i < end_byte; ++i) {
                    row[i] ^= 0xffu;
                }
            }
            break;
        case 1:
        default:
            memset(&row[mid_start], (color != 0) ? 0xffu : 0x00u, mid_count);
            break;
        }
    }

    _APPLY(row[end_byte], right_mask);

#undef _APPLY
}

void _vdi_draw_screen_hline_direct(WORD y, WORD left, WORD right, WORD color)
{
    uint8_t *row;

    if (left > right) {
        WORD tmp = left;

        left = right;
        right = tmp;
    }
    row = _vdi_screen_row_mutable(y);
    if (row == NULL) {
        return;
    }
    _vdi_hline_to_row(row, left, right, color);
}

void _vdi_draw_screen_hline(WORD y, WORD x0, WORD x1, WORD color)
{
    vdi_rect_t clip;
    uint8_t *row;
    WORD left;
    WORD right;

    if (x0 > x1) {
        WORD tmp = x0;

        x0 = x1;
        x1 = tmp;
    }

    _vdi_get_active_clip_rect(&clip);
    if (y < clip.y0 || y > clip.y1) {
        return;
    }

    left = (x0 < clip.x0) ? clip.x0 : x0;
    right = (x1 > clip.x1) ? clip.x1 : x1;
    if (left > right) {
        return;
    }

    row = _vdi_screen_row_mutable(y);
    if (row == NULL) {
        return;
    }

    _vdi_hline_to_row(row, left, right, color);
}

void _vdi_draw_line_segment(WORD x0, WORD y0, WORD x1, WORD y1, WORD color)
{
    WORD dx;
    WORD sx;
    WORD dy;
    WORD sy;
    WORD err;

    if (!_vdi_clip_line_segment(&x0, &y0, &x1, &y1)) {
        return;
    }

    _vdi_prepare_screen_write();

    if (y0 == y1) {
        _vdi_draw_screen_hline_direct(y0, x0, x1, color);
        return;
    }

    if (x0 == x1) {
        /* Vertical line: compute mask and byte offset once, advance by pitch */
        uint8_t mask = _vdi_screen_mask_for_x(x0);
        size_t byte_off = (size_t) x0 / 8u;
        WORD mode = _vdi_write_mode();
        WORD min_y = (y0 < y1) ? y0 : y1;
        WORD max_y = (y0 < y1) ? y1 : y0;
        size_t pitch = _vdi.surface->pitch;
        uint8_t *row = (uint8_t *) _vdi.surface->pixels +
            (size_t) min_y * pitch;
        WORD scan_y;

        for (scan_y = min_y; scan_y <= max_y; ++scan_y, row += pitch) {
            switch (mode) {
            case 2:
                if (color != 0) {
                    row[byte_off] |= mask;
                }
                break;
            case 3:
                if (color != 0) {
                    row[byte_off] ^= mask;
                }
                break;
            case 4:
                row[byte_off] &= (uint8_t) ~mask;
                break;
            default:
                if (color != 0) {
                    row[byte_off] |= mask;
                } else {
                    row[byte_off] &= (uint8_t) ~mask;
                }
                break;
            }
        }
        return;
    }

    dx = (WORD) abs(x1 - x0);
    sx = (WORD) ((x0 < x1) ? 1 : -1);
    dy = (WORD) -abs(y1 - y0);
    sy = (WORD) ((y0 < y1) ? 1 : -1);
    err = (WORD) (dx + dy);

    FOREVER {
        WORD e2 = (WORD) (2 * err);

        _vdi_set_screen_pixel_raw(x0, y0, color);
        if (x0 == x1 && y0 == y1) {
            return;
        }

        if (e2 >= dy) {
            err = (WORD) (err + dy);
            x0 = (WORD) (x0 + sx);
        }
        if (e2 <= dx) {
            err = (WORD) (err + dx);
            y0 = (WORD) (y0 + sy);
        }
    }
}

void _vdi_fill_rect(WORD x0, WORD y0, WORD x1, WORD y1, WORD color)
{
    vdi_rect_t rect;
    vdi_rect_t clip;
    WORD mode;
    size_t pitch;
    WORD height;
    uint8_t *base;
    size_t start_byte;
    size_t end_byte;
    unsigned int start_bit;
    unsigned int end_bit;
    uint8_t fill_byte;
    uint8_t left_mask;
    uint8_t right_mask;
    WORD row;

    rect.x0 = (x0 < x1) ? x0 : x1;
    rect.x1 = (x0 < x1) ? x1 : x0;
    rect.y0 = (y0 < y1) ? y0 : y1;
    rect.y1 = (y0 < y1) ? y1 : y0;

    _vdi_get_active_clip_rect(&clip);
    if (!_vdi_intersect_rects(&rect, &clip, &rect)) {
        return;
    }

    _vdi_prepare_screen_write();

    mode = _vdi_write_mode();
    pitch = _vdi.surface->pitch;
    height = (WORD) (rect.y1 - rect.y0 + 1);
    base = (uint8_t *) _vdi.surface->pixels + (size_t) rect.y0 * pitch;
    start_byte = (size_t) rect.x0 / 8u;
    end_byte = (size_t) rect.x1 / 8u;
    start_bit = (unsigned int) rect.x0 & 7u;
    end_bit = (unsigned int) rect.x1 & 7u;

    if (mode == 1 || mode == 4) {
        fill_byte = (mode == 1 && color != 0) ? 0xffu : 0x00u;

        /* Full-width fill: single memset over all rows */
        if (start_byte == 0 && end_byte == pitch - 1 &&
            start_bit == 0 && end_bit == 7) {
            memset(base, fill_byte, pitch * (size_t) height);
            return;
        }

        left_mask = (uint8_t) (0xffu >> start_bit);
        right_mask = (uint8_t) (0xffu << (7u - end_bit));

        if (start_byte == end_byte) {
            uint8_t mask = left_mask & right_mask;

            for (row = 0; row < height; ++row, base += pitch) {
                if (fill_byte != 0u) {
                    base[start_byte] |= mask;
                } else {
                    base[start_byte] &= (uint8_t) ~mask;
                }
            }
            return;
        }

        for (row = 0; row < height; ++row, base += pitch) {
            if (fill_byte != 0u) {
                base[start_byte] |= left_mask;
            } else {
                base[start_byte] &= (uint8_t) ~left_mask;
            }
            if (end_byte > start_byte + 1u) {
                memset(&base[start_byte + 1u], fill_byte,
                    end_byte - start_byte - 1u);
            }
            if (fill_byte != 0u) {
                base[end_byte] |= right_mask;
            } else {
                base[end_byte] &= (uint8_t) ~right_mask;
            }
        }
        return;
    }

    /* General case for modes 2, 3, 4 */
    for (row = 0; row < height; ++row, base += pitch) {
        _vdi_hline_to_row(base, rect.x0, rect.x1, color);
    }
}

void _vdi_draw_glyph(WORD x, WORD y, char ch, WORD color)
{
    vdi_font_t *font = current_font();
    vdi_rect_t clip;
    WORD opaque_background = _vdi_text_background_mode();
    WORD index;
    WORD start_bit;
    WORD width;
    WORD clip_col0;
    WORD clip_col1;
    WORD row_index;

    if (font == NULL) {
        return;
    }

    index = glyph_index(font, (unsigned char) ch);
    if (index < 0) {
        index = glyph_index(font, (unsigned char) '?');
        if (index < 0) {
            return;
        }
    }

    if (font->uniform_width > 0) {
        start_bit = (WORD) ((LONG) index * font->uniform_width);
        width = font->uniform_width;
    } else {
        start_bit = glyph_start_bit(font, index);
        width = (WORD) (glyph_end_bit(font, index) - start_bit);
    }
    if (width <= 0) {
        return;
    }

    _vdi_get_active_clip_rect(&clip);

    /* Pre-clip the glyph column range to avoid per-hline clip lookups */
    clip_col0 = (clip.x0 > x) ? (WORD) (clip.x0 - x) : 0;
    clip_col1 = (clip.x1 < (WORD) (x + width - 1))
        ? (WORD) (clip.x1 - x) : (WORD) (width - 1);
    if (clip_col0 > clip_col1) {
        return;
    }

    _vdi_prepare_screen_write();

    for (row_index = 0; row_index < font->form_height; ++row_index) {
        WORD draw_y = (WORD) (y + row_index);
        const uint8_t *glyph_row;
        WORD col;

        if (draw_y < clip.y0 || draw_y > clip.y1) {
            continue;
        }

        if (opaque_background != 0) {
            _vdi_draw_screen_hline_direct(draw_y, (WORD) (x + clip_col0),
                (WORD) (x + clip_col1), 0);
        }

        /* Direct byte access into font bitmap — avoids per-bit glyph_bit() call */
        glyph_row = font->data + font->data_offset +
            (size_t) row_index * (size_t) font->form_width;

        col = clip_col0;
        while (col <= clip_col1) {
            WORD abs_bit = (WORD) (start_bit + col);
            size_t byte_off = (size_t) abs_bit / 8u;
            unsigned int bit_in_byte = 7u - ((unsigned int) abs_bit & 7u);

            if (byte_off < font->data_size &&
                !(glyph_row[byte_off] & (uint8_t) (1u << bit_in_byte))) {
                ++col;
                continue;
            }

            {
                WORD run_start = col;

                ++col;
                while (col <= clip_col1) {
                    abs_bit = (WORD) (start_bit + col);
                    byte_off = (size_t) abs_bit / 8u;
                    bit_in_byte = 7u - ((unsigned int) abs_bit & 7u);
                    if (byte_off >= font->data_size ||
                        !(glyph_row[byte_off] & (uint8_t) (1u << bit_in_byte))) {
                        break;
                    }
                    ++col;
                }
                _vdi_draw_screen_hline_direct(draw_y, (WORD) (x + run_start),
                    (WORD) (x + col - 1), color);
            }
        }
    }
}

char _vdi_scancode_to_ascii(uint16_t key)
{
    if (key >= 4u && key <= 29u) {
        return (char) ('a' + (char) (key - 4u));
    }
    if (key >= 30u && key <= 38u) {
        return (char) ('1' + (char) (key - 30u));
    }

    switch (key) {
    case 39u:
        return '0';
    case 40u:
        return '\n';
    case 42u:
        return '\b';
    case 44u:
        return ' ';
    case 45u:
        return '-';
    case 47u:
        return '[';
    case 48u:
        return ']';
    case 51u:
        return ';';
    case 52u:
        return '\'';
    case 54u:
        return ',';
    case 55u:
        return '.';
    case 56u:
        return '/';
    default:
        return '\0';
    }
}

int _vdi_uses_screen(const MFDB *mfdb)
{
    return mfdb == NULL || mfdb->fd_addr == NULL;
}

WORD _vdi_mfdb_get_pixel(const MFDB *mfdb, WORD x, WORD y)
{
    return (WORD) _vdi_mfdb_sample_direct(mfdb, x, y);
}

void _vdi_mfdb_set_pixel(MFDB *mfdb, WORD x, WORD y, WORD color)
{
    if (_vdi_uses_screen(mfdb)) {
        if (_vdi_point_visible(x, y)) {
            _vdi_set_screen_pixel(x, y, color);
        }
        return;
    }

    _vdi_mfdb_draw_hline(mfdb, y, x, x, color);
}

void _vdi_mfdb_draw_hline(MFDB *mfdb, WORD y, WORD x0, WORD x1, WORD color)
{
    UWORD *row_words;
    WORD left;
    WORD right;
    WORD x;

    if (x0 > x1) {
        WORD tmp = x0;

        x0 = x1;
        x1 = tmp;
    }
    if (_vdi_uses_screen(mfdb)) {
        _vdi_draw_screen_hline(y, x0, x1, color);
        return;
    }
    if (mfdb == NULL || mfdb->fd_addr == NULL || mfdb->fd_nplanes != 1 ||
        y < 0 || y >= mfdb->fd_h) {
        return;
    }

    left = _vdi_clamp_word(x0, 0, (WORD) (mfdb->fd_w - 1));
    right = _vdi_clamp_word(x1, 0, (WORD) (mfdb->fd_w - 1));
    if (left > right) {
        return;
    }
    row_words = (UWORD *) mfdb->fd_addr +
        (size_t) y * (size_t) mfdb->fd_wdwidth;
    for (x = left; x <= right; ++x) {
        UWORD mask = (UWORD) (0x8000u >> ((unsigned int) x & 15u));
        UWORD *word = &row_words[(size_t) x / 16u];

        if (color != 0) {
            *word |= mask;
        } else {
            *word &= (UWORD) ~mask;
        }
    }
}

void _vdi_fill_work_out(WORD work_out[57])
{
    WORD cell_width = _vdi_font_cell_width();
    WORD text_height = _vdi_font_text_height();

    memset(work_out, 0, sizeof(WORD) * 57u);
    work_out[0] = (WORD) (_vdi.width - 1);
    work_out[1] = (WORD) (_vdi.height - 1);
    work_out[2] = 1;
    work_out[3] = 1;
    work_out[4] = 1;
    work_out[5] = 1;
    work_out[6] = 1;
    work_out[7] = 1;
    work_out[8] = 1;
    work_out[9] = 1;
    work_out[10] = 1;
    work_out[11] = 1;
    work_out[12] = 1;
    work_out[13] = 2;
    work_out[39] = 1;
    work_out[40] = 1;
    work_out[41] = 1;
    work_out[42] = 1;
    work_out[43] = 1;
    work_out[44] = 1;
    work_out[35] = 1;
    work_out[45] = cell_width;
    work_out[46] = text_height;
    work_out[47] = cell_width;
    work_out[48] = text_height;
    work_out[49] = 1;
    work_out[51] = 1;
    work_out[54] = 1;
    work_out[56] = 1;
}
