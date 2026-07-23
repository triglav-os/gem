/*
 * Implements private font loading, font selection, and glyph rendering
 * for the hosted GEM VDI layer. Separating font runtime state from the
 * surface and cursor helpers keeps text-related work localized.
 *
 * MIT License (see: LICENSE)
 * Copyright (C) 2026 tomaz stih
 */

#define _POSIX_C_SOURCE 200809L

#include "_internal.h"

#include "_state.h"

#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

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
    return (uint32_t) read_le16(bytes) |
        ((uint32_t) read_le16(bytes + 2) << 16);
}

static const char *font_dir(void)
{
    static char path[4096];
    const char *resource_dir = getenv("GEM_RESOURCE_DIR");
    ssize_t length;
    char *slash;

    if (resource_dir != NULL && resource_dir[0] != '\0') {
        if (snprintf(path, sizeof(path), "%s/fonts", resource_dir) <
            (int) sizeof(path)) {
            return path;
        }
    }
    length = readlink("/proc/self/exe", path, sizeof(path) - 1u);
    if (length > 0 && (size_t) length < sizeof(path)) {
        path[length] = '\0';
        slash = strrchr(path, '/');
        if (slash != NULL) {
            *slash = '\0';
            if (strlen(path) + strlen("/../share/gem/fonts") <
                sizeof(path)) {
                strcat(path, "/../share/gem/fonts");
                if (access(path, R_OK) == 0) {
                    return path;
                }
            }
        }
    }
#ifdef GEM_BUILD_FONT_DIR
    return GEM_BUILD_FONT_DIR;
#else
    return "bin/resources/fonts";
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
    if (ch < (unsigned int) font->first_ade ||
        ch > (unsigned int) font->last_ade) {
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

    {
        WORD num_glyphs = (WORD) (font->last_ade - font->first_ade + 1);
        WORD w0 = 0;
        WORD uniform = 1;
        WORD g;

        if (num_glyphs > 0 &&
            off_offset + (size_t) (num_glyphs + 1u) * 2u <=
            (size_t) file_size) {
            const uint8_t *ot = data + off_offset;

            w0 = (WORD) (read_le16(ot + 2u) - read_le16(ot));
            for (g = 1; g < num_glyphs; ++g) {
                WORD w = (WORD) (read_le16(ot + (size_t) (g + 1u) * 2u) -
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
    g_vdi_fonts[free_slot].file_name[
        sizeof(g_vdi_fonts[free_slot].file_name) - 1u] = '\0';
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
            if (g_vdi_fonts[slot].present &&
                g_vdi_fonts[slot].font_id == font_id) {
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

    clip_col0 = (clip.x0 > x) ? (WORD) (clip.x0 - x) : 0;
    clip_col1 = (clip.x1 < (WORD) (x + width - 1)) ?
        (WORD) (clip.x1 - x) : (WORD) (width - 1);
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
                        !(glyph_row[byte_off] &
                        (uint8_t) (1u << bit_in_byte))) {
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
