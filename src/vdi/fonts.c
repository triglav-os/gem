/*
 * Loads and manages GEM bitmap fonts for the hosted VDI: locating the
 * font directory, validating file layout and glyph offset tables, slot
 * allocation for bundled and dynamically loaded fonts, selection and the
 * name/id queries. Glyph metrics and rendering live in glyphs.c.
 *
 * MIT License (see: LICENSE)
 * Copyright (C) 2026 tomaz stih
 */

#define _POSIX_C_SOURCE 200809L

#include "fonts_private.h"

#include "platform/os.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static vdi_font_t g_vdi_fonts[vdi_max_fonts];
static WORD g_vdi_current_font = vdi_font_id_system;
static int g_vdi_fonts_loaded;
static int g_vdi_small_font_metrics;

static int copy_font_file_name(char *target, size_t target_size,
                               const char *source)
{
    size_t length;

    if (target == NULL || target_size == 0u || source == NULL) {
        return 0;
    }
    length = strlen(source);
    if (length >= target_size) {
        target[0] = '\0';
        return 0;
    }
    memcpy(target, source, length + 1u);
    return 1;
}

uint16_t vdi_font_read_le16(const uint8_t *bytes)
{
    return (uint16_t)bytes[0] | ((uint16_t)bytes[1] << 8);
}

static uint32_t read_le32(const uint8_t *bytes)
{
    return (uint32_t)vdi_font_read_le16(bytes) |
           ((uint32_t)vdi_font_read_le16(bytes + 2) << 16);
}

static const char *font_dir(void)
{
    static char path[4096];
    const char *resource_dir = getenv("GEM_RESOURCE_DIR");

    if (resource_dir != NULL && resource_dir[0] != '\0') {
        if (snprintf(path, sizeof(path), "%s/fonts", resource_dir) <
            (int)sizeof(path)) {
            return path;
        }
    }
    if (gem_os_find_file("bin/resources/fonts/AtariSTHigh.fnt", path,
                         sizeof(path)) != 0) {
        char *slash = strrchr(path, '/');

        if (slash != NULL) {
            *slash = '\0';
            return path;
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
    return file_name != NULL && (strcmp(file_name, "AtariSTHigh.fnt") == 0 ||
                                 strcmp(file_name, "ATARISTHIGH.FNT") == 0);
}

static int is_small_file_name(const char *file_name)
{
    return file_name != NULL && (strcmp(file_name, "8x16_z1.fnt") == 0 ||
                                 strcmp(file_name, "8X16_Z1.FNT") == 0);
}

static WORD find_font_slot_by_id(WORD font_id)
{
    WORD slot;

    for (slot = 0; slot < vdi_max_fonts; ++slot) {
        if (g_vdi_fonts[slot].present && g_vdi_fonts[slot].font_id == font_id) {
            return slot;
        }
    }
    return -1;
}

static WORD font_id_from_alias(WORD font_id)
{
    if (font_id == vdi_font_id_ibm) {
        return vdi_font_id_system;
    }
    if (font_id == vdi_font_id_small) {
        return vdi_font_id_small_internal;
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

vdi_font_t *vdi_current_font(void)
{
    return find_font_by_id(g_vdi_current_font);
}

int vdi_small_font_metrics_active(void)
{
    return g_vdi_small_font_metrics;
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

int vdi_font_bitmap_valid(const uint8_t *data, size_t size, WORD first,
                          WORD last, WORD width, WORD height,
                          uint32_t bitmap_offset, uint32_t table_offset)
{
    size_t bitmap_size, entries, i;
    uint16_t previous = 0;
    if (!data || first < 0 || last < first || last > 255 || width <= 0 ||
        width > 4095 || height <= 0 || height > 1024)
        return 0;
    bitmap_size = (size_t)width * (size_t)height;
    entries = (size_t)(last - first + 2);
    if (bitmap_offset < 88 || bitmap_offset > size ||
        bitmap_size > size - bitmap_offset ||
        table_offset < bitmap_offset + bitmap_size || table_offset > size ||
        entries * 2 > size - table_offset)
        return 0;
    for (i = 0; i < entries; ++i) {
        uint16_t bit = vdi_font_read_le16(data + table_offset + i * 2);
        if (bit < previous || bit > (unsigned)width * 8u)
            return 0;
        previous = bit;
    }
    return 1;
}

static int load_font_file(vdi_font_t *font, WORD font_id, const char *file_name)
{
    BYTE path[512];
    char stored_file_name[64];
    int fd;
    int64_t file_size;
    uint8_t *data;
    size_t offset;
    int32_t count;
    uint32_t fields[3];
    uint32_t data_offset = 0u;
    uint32_t off_offset = 0u;
    size_t bitmap_size;
    size_t off_size;
    int i;
    int path_length;

    if (!copy_font_file_name(stored_file_name, sizeof(stored_file_name),
                             file_name)) {
        return 0;
    }
    path_length = snprintf((char *)path, sizeof(path), "%s/%s", font_dir(),
                           stored_file_name);
    if (path_length < 0 || (size_t)path_length >= sizeof(path))
        return 0;
    fd = gem_os_open_read((const char *)path);
    if (fd < 0) {
        return 0;
    }

    file_size = gem_os_seek(fd, 0, 2);
    if (file_size < 88L || file_size > 16L * 1024L * 1024L ||
        gem_os_seek(fd, 0, 0) < 0) {
        gem_os_close(fd);
        return 0;
    }

    data = malloc((size_t)file_size);
    if (data == NULL) {
        gem_os_close(fd);
        return 0;
    }
    offset = 0u;
    while (offset < (size_t)file_size) {
        count = gem_os_read(fd, data + offset,
                            (uint32_t)((size_t)file_size - offset));
        if (count <= 0) {
            gem_os_close(fd);
            free(data);
            return 0;
        }
        offset += (size_t)count;
    }
    gem_os_close(fd);

    clear_font_runtime(font);
    font->font_id = font_id;
    (void)copy_font_file_name(font->file_name, sizeof(font->file_name),
                              stored_file_name);
    memcpy(font->name, data + 4, 32u);
    font->name[32] = '\0';
    font->first_ade = (WORD)vdi_font_read_le16(data + 36);
    font->last_ade = (WORD)vdi_font_read_le16(data + 38);
    font->top = (WORD)vdi_font_read_le16(data + 40);
    font->ascent = (WORD)vdi_font_read_le16(data + 42);
    font->half = (WORD)vdi_font_read_le16(data + 44);
    font->descent = (WORD)vdi_font_read_le16(data + 46);
    font->bottom = (WORD)vdi_font_read_le16(data + 48);
    font->max_char_width = (WORD)vdi_font_read_le16(data + 50);
    font->max_cell_width = (WORD)vdi_font_read_le16(data + 52);
    font->form_width = (WORD)vdi_font_read_le16(data + 80);
    font->form_height = (WORD)vdi_font_read_le16(data + 82);
    font->data = data;
    font->data_size = (size_t)file_size;

    if (font->form_width <= 0 || font->form_height <= 0 ||
        font->first_ade < 0 || font->last_ade < font->first_ade ||
        font->last_ade > 255) {
        clear_font_slot(font);
        return 0;
    }
    bitmap_size = (size_t)font->form_width * (size_t)font->form_height;
    off_size = (size_t)(font->last_ade - font->first_ade + 2) * 2u;
    fields[0] = read_le32(data + 68);
    fields[1] = read_le32(data + 72);
    fields[2] = read_le32(data + 76);

    for (i = 0; i < 3; ++i) {
        if (fields[i] != 0u && fields[i] + bitmap_size <= (size_t)file_size) {
            data_offset = fields[i];
        }
        if (fields[i] != 0u && fields[i] + off_size <= (size_t)file_size) {
            if (data_offset == 0u || fields[i] > data_offset) {
                off_offset = fields[i];
            }
        }
    }
    if (!vdi_font_bitmap_valid(data, (size_t)file_size, font->first_ade,
                               font->last_ade, font->form_width,
                               font->form_height, data_offset, off_offset)) {
        clear_font_runtime(font);
        return 0;
    }

    font->data_offset = data_offset;
    font->off_offset = off_offset;

    {
        WORD num_glyphs = (WORD)(font->last_ade - font->first_ade + 1);
        WORD w0 = 0;
        WORD uniform = 1;
        WORD g;

        if (num_glyphs > 0 &&
            off_offset + (size_t)(num_glyphs + 1u) * 2u <= (size_t)file_size) {
            const uint8_t *ot = data + off_offset;

            w0 = (WORD)(vdi_font_read_le16(ot + 2u) - vdi_font_read_le16(ot));
            for (g = 1; g < num_glyphs; ++g) {
                WORD w = (WORD)(vdi_font_read_le16(ot + (size_t)(g + 1u) * 2u) -
                                vdi_font_read_le16(ot + (size_t)g * 2u));

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

    for (slot = 0; slot < vdi_max_fonts; ++slot) {
        if (g_vdi_fonts[slot].present &&
            strcmp(g_vdi_fonts[slot].file_name, file_name) == 0) {
            return slot;
        }
        if (!g_vdi_fonts[slot].present && free_slot < 0) {
            free_slot = slot;
        }
    }

    if (preferred_slot >= 0 && preferred_slot < vdi_max_fonts &&
        !g_vdi_fonts[preferred_slot].present) {
        free_slot = preferred_slot;
    }
    if (free_slot < 0) {
        return -1;
    }

    if (strlen(file_name) >= sizeof(g_vdi_fonts[free_slot].file_name)) {
        return -1;
    }
    clear_font_slot(&g_vdi_fonts[free_slot]);
    g_vdi_fonts[free_slot].font_id = font_id;
    (void)copy_font_file_name(g_vdi_fonts[free_slot].file_name,
                              sizeof(g_vdi_fonts[free_slot].file_name),
                              file_name);
    g_vdi_fonts[free_slot].present = 1;
    return free_slot;
}

static WORD next_dynamic_font_id(void)
{
    WORD font_id = 1;
    WORD slot;

    for (;;) {
        int in_use = 0;

        if (font_id == vdi_font_id_ibm || font_id == vdi_font_id_small) {
            ++font_id;
            continue;
        }
        for (slot = 0; slot < vdi_max_fonts; ++slot) {
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

    for (slot = 0; slot < vdi_max_fonts; ++slot) {
        if (g_vdi_fonts[slot].present &&
            !is_system_file_name(g_vdi_fonts[slot].file_name) &&
            !is_small_file_name(g_vdi_fonts[slot].file_name)) {
            clear_font_slot(&g_vdi_fonts[slot]);
        }
    }
}

int vdi_load_fonts(void)
{
    if (g_vdi_fonts_loaded != 0) {
        return 1;
    }

    memset(g_vdi_fonts, 0, sizeof(g_vdi_fonts));
    if (!load_font_file(&g_vdi_fonts[0], vdi_font_id_system,
                        "AtariSTHigh.fnt")) {
        return 0;
    }
    if (!load_font_file(&g_vdi_fonts[1], vdi_font_id_small_internal,
                        "8x16_z1.fnt")) {
        clear_font_slot(&g_vdi_fonts[0]);
        return 0;
    }

    g_vdi_fonts_loaded = 1;
    g_vdi_current_font = vdi_font_id_system;
    g_vdi_small_font_metrics = 0;
    return 1;
}

WORD vdi_load_external_fonts(void)
{
    gem_os_dir_t directory;
    gem_os_dirent_t entry;

    if (!vdi_load_fonts()) {
        return 0;
    }

    if (!gem_os_dir_open(font_dir(), &directory)) {
        return vdi_font_count();
    }

    while (gem_os_dir_read(&directory, &entry)) {
        WORD slot;
        WORD font_id;

        if (!is_font_file_name(entry.name) ||
            is_system_file_name(entry.name) || is_small_file_name(entry.name)) {
            continue;
        }

        font_id = next_dynamic_font_id();
        slot = reserve_font_slot(-1, font_id, entry.name);
        if (slot >= 0) {
            (void)load_font_slot(&g_vdi_fonts[slot]);
        }
    }

    gem_os_dir_close(&directory);
    return vdi_font_count();
}

WORD vdi_unload_external_fonts(void)
{
    vdi_font_t *current = vdi_current_font();

    unload_external_font_slots();
    if (current == NULL || !current->present || !current->resident) {
        g_vdi_current_font = vdi_font_id_system;
    }
    return 0;
}

void vdi_unload_fonts(void)
{
    WORD slot;

    for (slot = 0; slot < vdi_max_fonts; ++slot) {
        clear_font_slot(&g_vdi_fonts[slot]);
    }
    g_vdi_current_font = vdi_font_id_system;
    g_vdi_small_font_metrics = 0;
    g_vdi_fonts_loaded = 0;
}

WORD vdi_select_font(WORD font_id)
{
    WORD resolved;
    vdi_font_t *font;

    if (font_id == vdi_font_id_ibm_compat) {
        resolved = vdi_font_id_system;
    } else if (font_id == vdi_font_id_small_compat) {
        resolved = vdi_font_id_small_internal;
    } else {
        resolved = font_id_from_alias(font_id);
    }

    if (!vdi_load_fonts()) {
        return 0;
    }
    font = find_font_by_id(resolved);
    if (font == NULL || !load_font_slot(font)) {
        g_vdi_current_font = vdi_font_id_system;
        g_vdi_small_font_metrics = 0;
        return g_vdi_current_font;
    }

    g_vdi_current_font = resolved;
    g_vdi_small_font_metrics = font_id == vdi_font_id_small_compat;
    return g_vdi_current_font;
}

WORD vdi_default_font_id(void)
{
    return vdi_font_id_system;
}

WORD vdi_font_count(void)
{
    WORD count = 0;
    WORD slot;

    for (slot = 0; slot < vdi_max_fonts; ++slot) {
        if (g_vdi_fonts[slot].present && g_vdi_fonts[slot].resident) {
            ++count;
        }
    }
    return count;
}

WORD vdi_font_id_for_element(WORD element_num)
{
    WORD count = 0;
    WORD slot;

    for (slot = 0; slot < vdi_max_fonts; ++slot) {
        if (g_vdi_fonts[slot].present && g_vdi_fonts[slot].resident) {
            ++count;
            if (count == element_num) {
                return g_vdi_fonts[slot].font_id;
            }
        }
    }
    return 0;
}

const char *vdi_font_name(WORD element_num)
{
    WORD count = 0;
    WORD slot;

    for (slot = 0; slot < vdi_max_fonts; ++slot) {
        if (g_vdi_fonts[slot].present && g_vdi_fonts[slot].resident) {
            ++count;
            if (count == element_num) {
                return g_vdi_fonts[slot].name;
            }
        }
    }
    return NULL;
}
