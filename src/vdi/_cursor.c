/*
 * Implements private cursor handling for the hosted GEM VDI layer.
 * This includes built-in mouse forms, resource-backed cursor loading,
 * cursor save/restore, and screen presentation that overlays the
 * pointer onto the monochrome framebuffer.
 *
 * MIT License (see: LICENSE)
 * Copyright (C) 2026 tomaz stih
 */

#include "_internal.h"

#include "gem/aes.h"

#include "platform/raster.h"

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define VDI_WORD_HEX(value) ((WORD) (UWORD) (value))

enum {
    _vdi_cursor_width = 16,
    _vdi_cursor_height = 16
};

enum {
    _vdi_cursor_arrow_hot_x = 0,
    _vdi_cursor_arrow_hot_y = 0,
    _vdi_cursor_text_hot_x = 8,
    _vdi_cursor_text_hot_y = 8,
    _vdi_cursor_bee_hot_x = 7,
    _vdi_cursor_bee_hot_y = 7,
    _vdi_cursor_point_hand_hot_x = 4,
    _vdi_cursor_point_hand_hot_y = 0,
    _vdi_cursor_flat_hand_hot_x = 7,
    _vdi_cursor_flat_hand_hot_y = 0,
    _vdi_cursor_cross_hot_x = 8,
    _vdi_cursor_cross_hot_y = 8
};

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

static const char *cursor_resource_path(void)
{
#ifdef GEM_CURSOR_RSC
    return GEM_CURSOR_RSC;
#else
    return "bin/cursors.rsc";
#endif
}

static int _vdi_load_binary_file(const char *path, uint8_t **data_out,
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

static const void *_vdi_resource_ptr(const uint8_t *base, size_t size,
    LONG offset, size_t bytes)
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
    switch (selector) {
    case TEXT_CRSR:
        return _vdi_cursor_text_hot_x;
    case HGLASS:
        return _vdi_cursor_bee_hot_x;
    case POINT_HAND:
        return _vdi_cursor_point_hand_hot_x;
    case FLAT_HAND:
        return _vdi_cursor_flat_hand_hot_x;
    case THIN_CROSS:
    case THICK_CROSS:
    case OUTLN_CROSS:
        return _vdi_cursor_cross_hot_x;
    default:
        return _vdi_cursor_arrow_hot_x;
    }
}

static WORD _vdi_cursor_hot_y_for_selector(WORD selector)
{
    switch (selector) {
    case TEXT_CRSR:
        return _vdi_cursor_text_hot_y;
    case HGLASS:
        return _vdi_cursor_bee_hot_y;
    case POINT_HAND:
        return _vdi_cursor_point_hand_hot_y;
    case FLAT_HAND:
        return _vdi_cursor_flat_hand_hot_y;
    case THIN_CROSS:
    case THICK_CROSS:
    case OUTLN_CROSS:
        return _vdi_cursor_cross_hot_y;
    default:
        return _vdi_cursor_arrow_hot_y;
    }
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

static int _vdi_load_iconblk_cursor(const uint8_t *base, size_t size,
    const ICONBLK *icon, WORD selector, MFORM *out)
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

void _vdi_prepare_screen_write(void)
{
    _vdi_cursor_restore();
}

static void _vdi_set_screen_pixel_raw(WORD x, WORD y, WORD color)
{
    uint8_t *row;
    uint8_t mask;

    if (_vdi.surface == NULL || _vdi.surface->pixels == NULL ||
        x < 0 || y < 0 || x >= _vdi.width || y >= _vdi.height) {
        return;
    }

    row = (uint8_t *) _vdi.surface->pixels +
        (size_t) y * _vdi.surface->pitch;
    mask = (uint8_t) (1u << (7u - ((unsigned int) x & 7u)));
    if (color != 0) {
        row[(size_t) x / 8u] |= mask;
    } else {
        row[(size_t) x / 8u] &= (uint8_t) ~mask;
    }
}

static WORD _vdi_get_screen_pixel_raw(WORD x, WORD y)
{
    const uint8_t *row;

    if (_vdi.surface == NULL || _vdi.surface->pixels == NULL ||
        x < 0 || y < 0 || x >= _vdi.width || y >= _vdi.height) {
        return 0;
    }

    row = (const uint8_t *) _vdi.surface->pixels +
        (size_t) y * _vdi.surface->pitch;
    return (WORD) ((row[(size_t) x / 8u] &
        (uint8_t) (1u << (7u - ((unsigned int) x & 7u)))) != 0u);
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
