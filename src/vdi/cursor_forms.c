/*
 * Supplies the hosted VDI's mouse forms: built-in arrow and busy-bee
 * shapes, and the standard set loaded from cursors.rsc with bounds-checked
 * ICONBLK parsing. Cursor drawing and presentation live in cursor.c.
 *
 * MIT License (see: LICENSE)
 * Copyright (C) 2026 tomaz stih
 */

#define _POSIX_C_SOURCE 200809L

#include "vdi_internal.h"

#include "gem/aes.h"
#include "platform/os.h"

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define VDI_WORD_HEX(value) ((WORD)(UWORD)(value))

enum { vdi_cursor_width = 16, vdi_cursor_height = 16 };

enum {
    vdi_cursor_arrow_hot_x = 0,
    vdi_cursor_arrow_hot_y = 0,
    vdi_cursor_text_hot_x = 8,
    vdi_cursor_text_hot_y = 8,
    vdi_cursor_bee_hot_x = 7,
    vdi_cursor_bee_hot_y = 7,
    vdi_cursor_point_hand_hot_x = 4,
    vdi_cursor_point_hand_hot_y = 0,
    vdi_cursor_flat_hand_hot_x = 7,
    vdi_cursor_flat_hand_hot_y = 0,
    vdi_cursor_cross_hot_x = 8,
    vdi_cursor_cross_hot_y = 8
};

static const MFORM g_vdi_arrow_form = {
    0,
    0,
    1,
    BLACK,
    WHITE,
    {VDI_WORD_HEX(0xc000), VDI_WORD_HEX(0xe000), VDI_WORD_HEX(0xf000),
     VDI_WORD_HEX(0xf800), VDI_WORD_HEX(0xfc00), VDI_WORD_HEX(0xfe00),
     VDI_WORD_HEX(0xff00), VDI_WORD_HEX(0xff80), VDI_WORD_HEX(0xffc0),
     VDI_WORD_HEX(0xffe0), VDI_WORD_HEX(0xfe00), VDI_WORD_HEX(0xef00),
     VDI_WORD_HEX(0xcf00), VDI_WORD_HEX(0x8780), VDI_WORD_HEX(0x0780),
     VDI_WORD_HEX(0x0380)},
    {VDI_WORD_HEX(0x0000), VDI_WORD_HEX(0x4000), VDI_WORD_HEX(0x6000),
     VDI_WORD_HEX(0x7000), VDI_WORD_HEX(0x7800), VDI_WORD_HEX(0x7c00),
     VDI_WORD_HEX(0x7e00), VDI_WORD_HEX(0x7f00), VDI_WORD_HEX(0x7f80),
     VDI_WORD_HEX(0x7c00), VDI_WORD_HEX(0x6c00), VDI_WORD_HEX(0x4600),
     VDI_WORD_HEX(0x0600), VDI_WORD_HEX(0x0300), VDI_WORD_HEX(0x0300),
     VDI_WORD_HEX(0x0000)}};

static const MFORM g_vdi_bee_form = {
    7,
    7,
    1,
    BLACK,
    WHITE,
    {VDI_WORD_HEX(0x0e00), VDI_WORD_HEX(0x0e1f), VDI_WORD_HEX(0x0e3f),
     VDI_WORD_HEX(0x077f), VDI_WORD_HEX(0xe7ff), VDI_WORD_HEX(0xffff),
     VDI_WORD_HEX(0xffff), VDI_WORD_HEX(0x1fff), VDI_WORD_HEX(0x0fff),
     VDI_WORD_HEX(0x1ffe), VDI_WORD_HEX(0x3fff), VDI_WORD_HEX(0x7fff),
     VDI_WORD_HEX(0x7fff), VDI_WORD_HEX(0x7fff), VDI_WORD_HEX(0x7fff),
     VDI_WORD_HEX(0x7fbf)},
    {VDI_WORD_HEX(0x0000), VDI_WORD_HEX(0x0400), VDI_WORD_HEX(0x041e),
     VDI_WORD_HEX(0x0031), VDI_WORD_HEX(0x0361), VDI_WORD_HEX(0x6342),
     VDI_WORD_HEX(0x0cc5), VDI_WORD_HEX(0x0daa), VDI_WORD_HEX(0x0370),
     VDI_WORD_HEX(0x0eac), VDI_WORD_HEX(0x19fe), VDI_WORD_HEX(0x30b0),
     VDI_WORD_HEX(0x216f), VDI_WORD_HEX(0x226c), VDI_WORD_HEX(0x252b),
     VDI_WORD_HEX(0x1a0a)}};

static const char *cursor_resource_path(void)
{
    static char path[4096];
    const char *resource_dir = getenv("GEM_RESOURCE_DIR");

    if (resource_dir != NULL && resource_dir[0] != '\0') {
        if (snprintf(path, sizeof(path), "%s/cursors.rsc", resource_dir) <
            (int)sizeof(path)) {
            return path;
        }
    }
    if (gem_os_find_file("bin/resources/cursors.rsc", path, sizeof(path)) !=
        0) {
        return path;
    }
#ifdef GEM_BUILD_CURSOR_RSC
    return GEM_BUILD_CURSOR_RSC;
#else
    return "bin/resources/cursors.rsc";
#endif
}

static int vdi_load_binary_file(const char *path, uint8_t **data_out,
                                size_t *size_out)
{
    int fd;
    int64_t size;
    uint8_t *data;
    size_t offset;
    int32_t count;

    if (path == NULL || data_out == NULL || size_out == NULL) {
        return 0;
    }

    fd = gem_os_open_read(path);
    if (fd < 0) {
        return 0;
    }
    size = gem_os_seek(fd, 0, 2);
    if (size < 0 || gem_os_seek(fd, 0, 0) < 0 || (uint64_t)size > SIZE_MAX) {
        gem_os_close(fd);
        return 0;
    }

    data = malloc((size_t)size);
    if (data == NULL) {
        gem_os_close(fd);
        return 0;
    }
    offset = 0u;
    while (offset < (size_t)size) {
        count = gem_os_read(fd, data + offset,
                            (uint32_t)((size_t)size - offset));
        if (count <= 0) {
            free(data);
            gem_os_close(fd);
            return 0;
        }
        offset += (size_t)count;
    }
    gem_os_close(fd);

    *data_out = data;
    *size_out = (size_t)size;
    return 1;
}

static const void *vdi_resource_ptr(const uint8_t *base, size_t size,
                                    LONG offset, size_t bytes)
{
    size_t start;

    if (base == NULL || offset < 0) {
        return NULL;
    }

    start = (size_t)offset;
    if (start > size || bytes > size - start) {
        return NULL;
    }
    return base + start;
}

static WORD vdi_cursor_hot_x_for_selector(WORD selector)
{
    switch (selector) {
        case TEXT_CRSR:
            return vdi_cursor_text_hot_x;
        case HGLASS:
            return vdi_cursor_bee_hot_x;
        case POINT_HAND:
            return vdi_cursor_point_hand_hot_x;
        case FLAT_HAND:
            return vdi_cursor_flat_hand_hot_x;
        case THIN_CROSS:
        case THICK_CROSS:
        case OUTLN_CROSS:
            return vdi_cursor_cross_hot_x;
        default:
            return vdi_cursor_arrow_hot_x;
    }
}

static WORD vdi_cursor_hot_y_for_selector(WORD selector)
{
    switch (selector) {
        case TEXT_CRSR:
            return vdi_cursor_text_hot_y;
        case HGLASS:
            return vdi_cursor_bee_hot_y;
        case POINT_HAND:
            return vdi_cursor_point_hand_hot_y;
        case FLAT_HAND:
            return vdi_cursor_flat_hand_hot_y;
        case THIN_CROSS:
        case THICK_CROSS:
        case OUTLN_CROSS:
            return vdi_cursor_cross_hot_y;
        default:
            return vdi_cursor_arrow_hot_y;
    }
}

static void vdi_copy_builtin_cursor_forms(void)
{
    WORD selector;

    for (selector = 0; selector < vdi_standard_cursor_count; ++selector) {
        vdi_state.standard_mouse_forms[selector] = g_vdi_arrow_form;
    }
    vdi_state.standard_mouse_forms[ARROW] = g_vdi_arrow_form;
    vdi_state.standard_mouse_forms[HGLASS] = g_vdi_bee_form;
}

static int vdi_load_iconblk_cursor(const uint8_t *base, size_t size,
                                   const ICONBLK *icon, WORD selector,
                                   MFORM *out)
{
    const WORD *mask;
    const WORD *data;
    WORD row;

    if (base == NULL || icon == NULL || out == NULL ||
        icon->ib_wicon != vdi_cursor_width ||
        icon->ib_hicon != vdi_cursor_height) {
        return 0;
    }

    mask = vdi_resource_ptr(base, size, icon->ib_pmask, sizeof(out->mf_mask));
    data = vdi_resource_ptr(base, size, icon->ib_pdata, sizeof(out->mf_data));
    if (mask == NULL || data == NULL) {
        return 0;
    }

    memset(out, 0, sizeof(*out));
    if (icon->ib_xchar >= 0 && icon->ib_xchar < icon->ib_wicon &&
        icon->ib_ychar >= 0 && icon->ib_ychar < icon->ib_hicon) {
        out->mf_xhot = icon->ib_xchar;
        out->mf_yhot = icon->ib_ychar;
    } else {
        out->mf_xhot = vdi_cursor_hot_x_for_selector(selector);
        out->mf_yhot = vdi_cursor_hot_y_for_selector(selector);
    }
    out->mf_nplanes = 1;
    out->mf_fg = BLACK;
    out->mf_bg = WHITE;
    for (row = 0; row < vdi_cursor_height; ++row) {
        out->mf_mask[row] = mask[row];
        out->mf_data[row] = data[row];
    }
    return 1;
}

WORD vdi_load_standard_mouse_forms(void)
{
    const char *path = cursor_resource_path();
    uint8_t *resource = NULL;
    size_t resource_size = 0;
    const RSHDR *header;
    const ICONBLK *icons;
    WORD selector;

    vdi_copy_builtin_cursor_forms();

    if (!vdi_load_binary_file(path, &resource, &resource_size)) {
        vdi_state.mouse_form = vdi_state.standard_mouse_forms[ARROW];
        return 1;
    }
    if (resource_size < sizeof(RSHDR)) {
        free(resource);
        vdi_state.mouse_form = vdi_state.standard_mouse_forms[ARROW];
        return 1;
    }

    header = (const RSHDR *)resource;
    icons =
        vdi_resource_ptr(resource, resource_size, header->rsh_iconblk,
                         sizeof(ICONBLK) * (size_t)vdi_standard_cursor_count);
    if (header->rsh_nib >= vdi_standard_cursor_count && icons != NULL) {
        for (selector = 0; selector < vdi_standard_cursor_count; ++selector) {
            MFORM form;

            if (vdi_load_iconblk_cursor(resource, resource_size,
                                        &icons[selector], selector, &form)) {
                vdi_state.standard_mouse_forms[selector] = form;
            }
        }
    }

    free(resource);
    vdi_state.mouse_form = vdi_state.standard_mouse_forms[ARROW];
    return 1;
}
