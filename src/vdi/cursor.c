/*
 * Implements private cursor handling for the hosted GEM VDI layer.
 * This includes built-in mouse forms, resource-backed cursor loading,
 * cursor save/restore, and screen presentation that overlays the
 * pointer onto the monochrome framebuffer.
 *
 * MIT License (see: LICENSE)
 * Copyright (C) 2026 tomaz stih
 */

#define _POSIX_C_SOURCE 200809L

#include "vdi_internal.h"

#include "gem/aes.h"

#include "platform/raster.h"

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

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

static uint16_t g_vdi_cursor_saved[vdi_cursor_height];

static void vdi_set_screen_pixel_raw(WORD x, WORD y, WORD color);
static WORD vdi_get_screen_pixel_raw(WORD x, WORD y);
static void vdi_cursor_restore(void);
static void vdi_cursor_draw(void);

static const char *cursor_resource_path(void)
{
    static char path[4096];
    const char *resource_dir = getenv("GEM_RESOURCE_DIR");
    ssize_t length;
    char *slash;

    if (resource_dir != NULL && resource_dir[0] != '\0') {
        if (snprintf(path, sizeof(path), "%s/cursors.rsc", resource_dir) <
            (int)sizeof(path)) {
            return path;
        }
    }
    length = readlink("/proc/self/exe", path, sizeof(path) - 1u);
    if (length > 0 && (size_t)length < sizeof(path)) {
        path[length] = '\0';
        slash = strrchr(path, '/');
        if (slash != NULL) {
            *slash = '\0';
            if (strlen(path) + strlen("/../share/gem/cursors.rsc") <
                sizeof(path)) {
                strcat(path, "/../share/gem/cursors.rsc");
                if (access(path, R_OK) == 0) {
                    return path;
                }
            }
        }
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

    data = malloc((size_t)size);
    if (data == NULL) {
        fclose(stream);
        return 0;
    }
    if (size > 0 && fread(data, (size_t)size, 1, stream) != 1) {
        free(data);
        fclose(stream);
        return 0;
    }
    fclose(stream);

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

WORD vdi_set_mouse_form(const MFORM *form)
{
    if (form == NULL) {
        return 0;
    }

    vdi_cursor_restore();
    vdi_state.mouse_form = *form;
    vdi_state.cursor_x =
        (WORD)(vdi_state.mouse_x - vdi_state.mouse_form.mf_xhot);
    vdi_state.cursor_y =
        (WORD)(vdi_state.mouse_y - vdi_state.mouse_form.mf_yhot);
    vdi_cursor_draw();
    if (vdi_state.update_depth == 0) {
        gem_raster_present();
    } else {
        vdi_state.present_pending = 1;
    }
    return 1;
}

WORD vdi_select_system_mouse_form(WORD selector)
{
    if (selector < 0 || selector >= vdi_standard_cursor_count) {
        selector = ARROW;
    }

    return vdi_set_mouse_form(&vdi_state.standard_mouse_forms[selector]);
}

void vdi_set_mouse_state(WORD x, WORD y, WORD status)
{
    WORD hot_x = vdi_state.mouse_form.mf_xhot;
    WORD hot_y = vdi_state.mouse_form.mf_yhot;
    WORD old_cursor_x = vdi_state.cursor_x;
    WORD old_cursor_y = vdi_state.cursor_y;

    vdi_cursor_restore();
    vdi_state.mouse_x = x;
    vdi_state.mouse_y = y;
    vdi_state.mouse_status = status;
    vdi_state.cursor_x = (WORD)(x - hot_x);
    vdi_state.cursor_y = (WORD)(y - hot_y);
    vdi_cursor_draw();
    /*
     * Always push only the old/new 16x16 cursor boxes to the FB — even
     * while begin_update is nested (window drag). A full-screen present
     * on every motion event is what made the pointer feel glacial.
     */
    if (vdi_state.cursor_hidden == 0) {
        gem_raster_present_rect((int)old_cursor_x, (int)old_cursor_y,
                                vdi_cursor_width + 1, vdi_cursor_height + 1);
    }
    if (vdi_state.cursor_hidden == 0 && vdi_state.cursor_drawn != 0) {
        gem_raster_present_rect((int)vdi_state.cursor_x,
                                (int)vdi_state.cursor_y, vdi_cursor_width + 1,
                                vdi_cursor_height + 1);
    }
}

void vdi_present_screen(void)
{
    if (vdi_state.update_depth > 0) {
        vdi_state.present_pending = 1;
        vdi_pump_events();
        return;
    }

    vdi_cursor_draw();
    if (vdi_state.cursor_hidden == 0 && vdi_state.cursor_drawn != 0) {
        vdi_mark_dirty(vdi_state.cursor_x, vdi_state.cursor_y,
                       (WORD)(vdi_state.cursor_x + 16),
                       (WORD)(vdi_state.cursor_y + 16));
    }
    if (vdi_state.dirty_valid != 0) {
        gem_raster_present_rect(
            (int)vdi_state.dirty_x0, (int)vdi_state.dirty_y0,
            (int)(vdi_state.dirty_x1 - vdi_state.dirty_x0 + 1),
            (int)(vdi_state.dirty_y1 - vdi_state.dirty_y0 + 1));
        vdi_state.dirty_valid = 0;
    } else {
        gem_raster_present();
    }
    vdi_pump_events();
}

void vdi_prepare_screen_write(void)
{
    vdi_cursor_restore();
}

static void vdi_set_screen_pixel_raw(WORD x, WORD y, WORD color)
{
    uint8_t *row;
    uint8_t mask;

    if (vdi_state.surface == NULL || vdi_state.surface->pixels == NULL ||
        x < 0 || y < 0 || x >= vdi_state.width || y >= vdi_state.height) {
        return;
    }

    row = (uint8_t *)vdi_state.surface->pixels +
          (size_t)y * vdi_state.surface->pitch;
    mask = (uint8_t)(1u << (7u - ((unsigned int)x & 7u)));
    if (color != 0) {
        row[(size_t)x / 8u] |= mask;
    } else {
        row[(size_t)x / 8u] &= (uint8_t)~mask;
    }
}

static WORD vdi_get_screen_pixel_raw(WORD x, WORD y)
{
    const uint8_t *row;

    if (vdi_state.surface == NULL || vdi_state.surface->pixels == NULL ||
        x < 0 || y < 0 || x >= vdi_state.width || y >= vdi_state.height) {
        return 0;
    }

    row = (const uint8_t *)vdi_state.surface->pixels +
          (size_t)y * vdi_state.surface->pitch;
    return (WORD)((row[(size_t)x / 8u] &
                   (uint8_t)(1u << (7u - ((unsigned int)x & 7u)))) != 0u);
}

static void vdi_cursor_restore(void)
{
    WORD y;

    if (vdi_state.cursor_drawn == 0) {
        return;
    }

    for (y = 0; y < vdi_cursor_height; ++y) {
        WORD x;

        for (x = 0; x < vdi_cursor_width; ++x) {
            uint16_t bit = (uint16_t)(0x8000u >> x);

            if ((vdi_state.mouse_form.mf_mask[y] & bit) == 0u) {
                continue;
            }
            vdi_set_screen_pixel_raw(
                (WORD)(vdi_state.cursor_x + x), (WORD)(vdi_state.cursor_y + y),
                (WORD)((g_vdi_cursor_saved[y] & bit) != 0u));
        }
    }

    /* Restoring the backing surface must also erase the presented cursor. */
    vdi_mark_dirty(vdi_state.cursor_x, vdi_state.cursor_y,
                   (WORD)(vdi_state.cursor_x + 15),
                   (WORD)(vdi_state.cursor_y + 15));
    vdi_state.cursor_drawn = 0;
}

static void vdi_cursor_draw(void)
{
    WORD y;
    WORD fg_color;
    WORD bg_color;

    if (!vdi_state.open || vdi_state.cursor_hidden != 0 ||
        vdi_state.cursor_drawn != 0) {
        return;
    }

    /* Same as rasta path: WHITE→1, BLACK→0 in the mono shadow. */
    fg_color = (vdi_state.mouse_form.mf_fg == WHITE) ? 1 : 0;
    bg_color = (vdi_state.mouse_form.mf_bg == WHITE) ? 1 : 0;

    for (y = 0; y < vdi_cursor_height; ++y) {
        WORD x;
        uint16_t saved = 0u;

        for (x = 0; x < vdi_cursor_width; ++x) {
            WORD px = (WORD)(vdi_state.cursor_x + x);
            WORD py = (WORD)(vdi_state.cursor_y + y);
            uint16_t bit = (uint16_t)(0x8000u >> x);

            if ((vdi_state.mouse_form.mf_mask[y] & bit) == 0u) {
                continue;
            }
            if (vdi_get_screen_pixel_raw(px, py) != 0) {
                saved |= bit;
            }
            if ((vdi_state.mouse_form.mf_data[y] & bit) != 0u) {
                vdi_set_screen_pixel_raw(px, py, fg_color);
            } else {
                vdi_set_screen_pixel_raw(px, py, bg_color);
            }
        }
        g_vdi_cursor_saved[y] = saved;
    }

    vdi_state.cursor_drawn = 1;
}
