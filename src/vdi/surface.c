/*
 * Implements low-level pixel access for the hosted GEM VDI layer: packed
 * monochrome row and mask helpers, write-mode application, screen and
 * MFDB pixel reads and writes, and the MFDB horizontal run writer used by
 * raster copies. Lines and fills live in lines.c and fill.c.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (C) 2026 tomaz stih
 */

#include "vdi_internal.h"

#include "vdi_state.h"

#include <stdlib.h>
#include <string.h>

static WORD vdi_clamp_word(WORD value, WORD low, WORD high)
{
    if (value < low) {
        return low;
    }
    if (value > high) {
        return high;
    }
    return value;
}

uint8_t *vdi_screen_row_mutable(WORD y)
{
    if (vdi_state.surface == NULL || vdi_state.surface->pixels == NULL ||
        y < 0 || y >= vdi_state.height) {
        return NULL;
    }

    return (uint8_t *)vdi_state.surface->pixels +
           (size_t)y * vdi_state.surface->pitch;
}

static const uint8_t *vdi_screen_row_const(WORD y)
{
    if (vdi_state.surface == NULL || vdi_state.surface->pixels == NULL ||
        y < 0 || y >= vdi_state.height) {
        return NULL;
    }

    return (const uint8_t *)vdi_state.surface->pixels +
           (size_t)y * vdi_state.surface->pitch;
}

uint8_t vdi_screen_mask_for_x(WORD x)
{
    return (uint8_t)(1u << (7u - ((unsigned int)x & 7u)));
}

static void vdi_apply_mask(uint8_t *byte, uint8_t mask, WORD color)
{
    WORD mode;

    if (byte == NULL || mask == 0u) {
        return;
    }

    mode = vdi_write_mode();
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
            *byte &= (uint8_t)~mask;
            break;
        case 1:
        default:
            if (color != 0) {
                *byte |= mask;
            } else {
                *byte &= (uint8_t)~mask;
            }
            break;
    }
}

static uint8_t vdi_mfdb_sample_direct(const MFDB *mfdb, WORD x, WORD y)
{
    const UWORD *row;
    UWORD mask;

    if (vdi_uses_screen(mfdb)) {
        const uint8_t *screen_row = vdi_screen_row_const(y);

        if (screen_row == NULL || x < 0 || x >= vdi_state.width) {
            return 0;
        }
        return (screen_row[(size_t)x / 8u] & vdi_screen_mask_for_x(x)) != 0u
                   ? 1u
                   : 0u;
    }
    if (mfdb == NULL || mfdb->fd_addr == NULL || mfdb->fd_nplanes != 1 ||
        x < 0 || y < 0 || x >= mfdb->fd_w || y >= mfdb->fd_h) {
        return 0;
    }

    row = (const UWORD *)mfdb->fd_addr + (size_t)y * (size_t)mfdb->fd_wdwidth;
    mask = (UWORD)(0x8000u >> ((unsigned int)x & 15u));
    return (row[(size_t)x / 16u] & mask) != 0u ? 1u : 0u;
}

void vdi_set_screen_pixel_raw(WORD x, WORD y, WORD color)
{
    uint8_t *row = vdi_screen_row_mutable(y);
    uint8_t mask;

    if (row == NULL || x < 0 || x >= vdi_state.width) {
        return;
    }

    mask = vdi_screen_mask_for_x(x);
    vdi_apply_mask(&row[(size_t)x / 8u], mask, color);
}

static WORD vdi_get_screen_pixel_raw(WORD x, WORD y)
{
    const uint8_t *row = vdi_screen_row_const(y);

    if (row == NULL || x < 0 || x >= vdi_state.width) {
        return 0;
    }

    return (WORD)((row[(size_t)x / 8u] & vdi_screen_mask_for_x(x)) != 0u);
}

WORD vdi_get_screen_pixel(WORD x, WORD y)
{
    return vdi_get_screen_pixel_raw(x, y);
}

void vdi_set_screen_pixel(WORD x, WORD y, WORD color)
{
    vdi_prepare_screen_write();
    vdi_set_screen_pixel_raw(x, y, color);
}

void vdi_clear_screen(WORD color)
{
    uint8_t value = (uint8_t)((color != 0) ? 0xffu : 0x00u);

    vdi_prepare_screen_write();
    memset(vdi_state.surface->pixels, value,
           (size_t)vdi_state.surface->pitch * (size_t)vdi_state.height);
}

int vdi_uses_screen(const MFDB *mfdb)
{
    return mfdb == NULL || mfdb->fd_addr == NULL;
}

WORD vdi_mfdb_get_pixel(const MFDB *mfdb, WORD x, WORD y)
{
    return (WORD)vdi_mfdb_sample_direct(mfdb, x, y);
}

void vdi_mfdb_set_pixel(MFDB *mfdb, WORD x, WORD y, WORD color)
{
    if (vdi_uses_screen(mfdb)) {
        if (vdi_point_visible(x, y)) {
            vdi_set_screen_pixel(x, y, color);
        }
        return;
    }

    vdi_mfdb_draw_hline(mfdb, y, x, x, color);
}

void vdi_mfdb_draw_hline(MFDB *mfdb, WORD y, WORD x0, WORD x1, WORD color)
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
    if (vdi_uses_screen(mfdb)) {
        vdi_draw_screen_hline(y, x0, x1, color);
        return;
    }
    if (mfdb == NULL || mfdb->fd_addr == NULL || mfdb->fd_nplanes != 1 ||
        y < 0 || y >= mfdb->fd_h || x1 < 0 || x0 >= mfdb->fd_w) {
        return;
    }

    left = vdi_clamp_word(x0, 0, (WORD)(mfdb->fd_w - 1));
    right = vdi_clamp_word(x1, 0, (WORD)(mfdb->fd_w - 1));
    if (left > right) {
        return;
    }
    row_words = (UWORD *)mfdb->fd_addr + (size_t)y * (size_t)mfdb->fd_wdwidth;
    for (x = left; x <= right; ++x) {
        UWORD mask = (UWORD)(0x8000u >> ((unsigned int)x & 15u));
        UWORD *word = &row_words[(size_t)x / 16u];

        if (color != 0) {
            *word |= mask;
        } else {
            *word &= (UWORD)~mask;
        }
    }
}
