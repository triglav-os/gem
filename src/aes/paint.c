/*
 * Low-level painting primitives shared by window chrome and object
 * rendering: solid, pattern, checker and inverted rectangles, edge lines
 * and text output including the stippled disabled style.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (C) 2026 tomaz stih
 */

#include "window_internal.h"

#include "../vdi/vdi_internal.h"

#include "platform/os.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

void aes_fill_rect(WORD x0, WORD y0, WORD x1, WORD y1, WORD color)
{
    WORD rect[4];

    if (x0 > x1 || y0 > y1) {
        return;
    }

    rect[0] = x0;
    rect[1] = y0;
    rect[2] = x1;
    rect[3] = y1;
    vsf_color(aes_state.vdi_handle, color);
    v_bar(aes_state.vdi_handle, rect);
}

void aes_draw_rect_edges(WORD x0, WORD y0, WORD x1, WORD y1, int draw_top,
                         int draw_right, int draw_bottom, int draw_left)
{
    WORD line[4];

    if (x0 > x1 || y0 > y1) {
        return;
    }

    if (draw_top != 0) {
        line[0] = x0;
        line[1] = y0;
        line[2] = x1;
        line[3] = y0;
        v_pline(aes_state.vdi_handle, 2, line);
    }
    if (draw_right != 0) {
        line[0] = x1;
        line[1] = y0;
        line[2] = x1;
        line[3] = y1;
        v_pline(aes_state.vdi_handle, 2, line);
    }
    if (draw_bottom != 0) {
        line[0] = x0;
        line[1] = y1;
        line[2] = x1;
        line[3] = y1;
        v_pline(aes_state.vdi_handle, 2, line);
    }
    if (draw_left != 0) {
        line[0] = x0;
        line[1] = y0;
        line[2] = x0;
        line[3] = y1;
        v_pline(aes_state.vdi_handle, 2, line);
    }
}

void aes_invert_rect(WORD x0, WORD y0, WORD x1, WORD y1)
{
    vdi_rect_t clip;
    gem_raster_surface_t *surface;
    size_t pitch;
    size_t start_byte;
    size_t end_byte;
    uint8_t left_mask;
    uint8_t right_mask;
    WORD y;

    vdi_get_active_clip_rect(&clip);
    x0 = aes_max_word(x0, clip.x0);
    y0 = aes_max_word(y0, clip.y0);
    x1 = aes_min_word(x1, clip.x1);
    y1 = aes_min_word(y1, clip.y1);
    if (x0 > x1 || y0 > y1) {
        return;
    }

    surface = gem_raster_surface();
    if (surface == NULL || surface->pixels == NULL ||
        surface->format != GEM_RASTER_MONO1) {
        return;
    }

    vdi_prepare_screen_write();
    pitch = surface->pitch;
    start_byte = (size_t)x0 / 8u;
    end_byte = (size_t)x1 / 8u;
    left_mask = (uint8_t)(0xffu >> ((unsigned int)x0 & 7u));
    right_mask = (uint8_t)(0xffu << (7u - ((unsigned int)x1 & 7u)));

    for (y = y0; y <= y1; ++y) {
        uint8_t *row = (uint8_t *)surface->pixels + (size_t)y * pitch;
        size_t b;

        if (start_byte == end_byte) {
            row[start_byte] ^= (uint8_t)(left_mask & right_mask);
            continue;
        }
        row[start_byte] ^= left_mask;
        for (b = start_byte + 1u; b < end_byte; ++b) {
            row[b] ^= 0xffu;
        }
        row[end_byte] ^= right_mask;
    }
}

void aes_fill_pattern_rect(WORD x0, WORD y0, WORD x1, WORD y1,
                           const uint8_t *rows, size_t row_count)
{
    vdi_rect_t clip;
    WORD y;
    WORD dark_pixel = (aes_dark_color() == WHITE) ? 1 : 0;
    gem_raster_surface_t *surface;
    size_t pitch;
    size_t start_byte;
    size_t end_byte;
    uint8_t left_mask;
    uint8_t right_mask;

    vdi_get_active_clip_rect(&clip);
    x0 = aes_max_word(x0, clip.x0);
    y0 = aes_max_word(y0, clip.y0);
    x1 = aes_min_word(x1, clip.x1);
    y1 = aes_min_word(y1, clip.y1);
    if (x0 > x1 || y0 > y1 || rows == NULL || row_count == 0u) {
        return;
    }

    /* Paper fill (bulk). */
    aes_fill_rect(x0, y0, x1, y1, aes_light_color());

    /*
     * Pattern dots: write mono shadow bytes, not one plot_pixel per
     * coordinate. Desktop checker (0xAA/0x55) and scrollbar stipples
     * repeat every 8 columns, so each framebuffer byte uses the same
     * pattern byte (aligned to absolute x).
     */
    surface = gem_raster_surface();
    if (surface == NULL || surface->pixels == NULL ||
        surface->format != GEM_RASTER_MONO1) {
        return;
    }

    vdi_prepare_screen_write();
    pitch = surface->pitch;
    start_byte = (size_t)x0 / 8u;
    end_byte = (size_t)x1 / 8u;
    left_mask = (uint8_t)(0xffu >> ((unsigned int)x0 & 7u));
    right_mask = (uint8_t)(0xffu << (7u - ((unsigned int)x1 & 7u)));

    for (y = y0; y <= y1; ++y) {
        uint8_t *row = (uint8_t *)surface->pixels + (size_t)y * pitch;
        uint8_t pat = rows[(size_t)(y % (WORD)row_count)];
        size_t b;

        if (start_byte == end_byte) {
            uint8_t mask = (uint8_t)(left_mask & right_mask & pat);

            if (dark_pixel != 0) {
                row[start_byte] |= mask;
            } else {
                row[start_byte] &= (uint8_t)~mask;
            }
            continue;
        }

        if (dark_pixel != 0) {
            row[start_byte] |= (uint8_t)(left_mask & pat);
            for (b = start_byte + 1u; b < end_byte; ++b) {
                row[b] |= pat;
            }
            row[end_byte] |= (uint8_t)(right_mask & pat);
        } else {
            row[start_byte] &= (uint8_t) ~(left_mask & pat);
            for (b = start_byte + 1u; b < end_byte; ++b) {
                row[b] &= (uint8_t)~pat;
            }
            row[end_byte] &= (uint8_t) ~(right_mask & pat);
        }
    }
}

void aes_fill_checker_rect(WORD x0, WORD y0, WORD x1, WORD y1)
{
    static const uint8_t desktop_rows[] = {0xaa, 0x55};

    if (x0 > x1 || y0 > y1) {
        return;
    }

    aes_fill_pattern_rect(x0, y0, x1, y1, desktop_rows, sizeof(desktop_rows));
}

void aes_draw_text(WORD x, WORD y, WORD color, const char *text)
{
    if (text == NULL || *text == '\0') {
        aes_draw_trace("text skip x=%d y=%d color=%d text=%p", x, y, color,
                       (const void *)text);
        return;
    }

    aes_draw_trace("text x=%d y=%d color=%d \"%s\"", x, y, color, text);
    vst_color(aes_state.vdi_handle, color);
    v_gtext(aes_state.vdi_handle, x, y, (CONST BYTE *)text);
}

void aes_stipple_text_pixels(WORD x, WORD y, WORD foreground, WORD background,
                             const char *text)
{
    WORD width;
    WORD height;
    WORD top;
    WORD left;
    WORD x1;
    WORD y1;
    gem_raster_surface_t *surface;
    size_t pitch;
    size_t start_byte;
    size_t end_byte;
    uint8_t left_mask;
    uint8_t right_mask;
    WORD py;
    /* Shadow packing matches rasta: WHITE→1 (set), BLACK→0 (clear). */
    WORD fg_pixel = (foreground == WHITE) ? 1 : 0;
    WORD bg_pixel = (background == WHITE) ? 1 : 0;

    if (text == NULL || *text == '\0') {
        return;
    }

    width = (WORD)vdi_string_width(text);
    height = vdi_font_text_height();
    left = x;
    top = (WORD)(y - vdi_font_ascent());
    if (width <= 0 || height <= 0) {
        return;
    }

    surface = gem_raster_surface();
    if (surface == NULL || surface->pixels == NULL ||
        surface->format != GEM_RASTER_MONO1) {
        return;
    }

    x1 = (WORD)(left + width - 1);
    y1 = (WORD)(top + height - 1);
    vdi_prepare_screen_write();
    pitch = surface->pitch;
    start_byte = (size_t)left / 8u;
    end_byte = (size_t)x1 / 8u;
    left_mask = (uint8_t)(0xffu >> ((unsigned int)left & 7u));
    right_mask = (uint8_t)(0xffu << (7u - ((unsigned int)x1 & 7u)));

    /*
     * Checker stipple: where (x+y) is odd and the pixel is foreground,
     * force background. Byte masks only — no per-pixel get/set.
     */
    for (py = top; py <= y1; ++py) {
        uint8_t *row = (uint8_t *)surface->pixels + (size_t)py * pitch;
        /* (x+y) odd: y even → odd x → 0x55; y odd → even x → 0xAA. */
        uint8_t checker = ((py & 1) != 0) ? 0xaau : 0x55u;
        size_t b;

        for (b = start_byte; b <= end_byte; ++b) {
            uint8_t span;
            uint8_t mask;
            uint8_t cur;
            uint8_t is_fg;

            if (start_byte == end_byte) {
                span = (uint8_t)(left_mask & right_mask);
            } else if (b == start_byte) {
                span = left_mask;
            } else if (b == end_byte) {
                span = right_mask;
            } else {
                span = 0xffu;
            }

            mask = (uint8_t)(span & checker);
            cur = row[b];
            is_fg = (fg_pixel != 0) ? (uint8_t)(cur & mask)
                                    : (uint8_t)((uint8_t)(~cur) & mask);
            if (bg_pixel != 0) {
                row[b] = (uint8_t)(cur | is_fg);
            } else {
                row[b] = (uint8_t)(cur & (uint8_t)~is_fg);
            }
        }
    }
}

void aes_draw_trace(const char *fmt, ...)
{
    static int enabled = -1;
    va_list ap;

    if (enabled < 0) {
        const char *trace = gem_os_getenv_ref("GEM_TRACE_DRAW");

        enabled = trace != NULL && trace[0] != '\0';
    }
    if (!enabled) {
        return;
    }

    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    fputc('\n', stderr);
}

void aes_draw_hline(WORD x0, WORD x1, WORD y, WORD color)
{
    WORD pts[4];

    if (x0 > x1) {
        return;
    }

    pts[0] = x0;
    pts[1] = y;
    pts[2] = x1;
    pts[3] = y;
    vsl_color(aes_state.vdi_handle, color);
    v_pline(aes_state.vdi_handle, 2, pts);
}

void aes_draw_vline(WORD x, WORD y0, WORD y1, WORD color)
{
    WORD pts[4];

    if (y0 > y1) {
        return;
    }

    pts[0] = x;
    pts[1] = y0;
    pts[2] = x;
    pts[3] = y1;
    vsl_color(aes_state.vdi_handle, color);
    v_pline(aes_state.vdi_handle, 2, pts);
}
