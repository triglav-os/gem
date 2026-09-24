/*
 * Implements cursor handling for the hosted GEM VDI layer: mouse form
 * selection, pointer movement with partial presents, saving and restoring
 * the pixels under the pointer, and the screen presentation that overlays
 * it on the monochrome framebuffer. Forms are loaded in cursor_forms.c.
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

static void vdi_cursor_plot_raw(WORD x, WORD y, WORD color);
static WORD vdi_cursor_pixel_raw(WORD x, WORD y);
static void vdi_cursor_restore(void);
static void vdi_cursor_draw(void);

enum { vdi_cursor_width = 16, vdi_cursor_height = 16 };

static uint16_t g_vdi_cursor_saved[vdi_cursor_height];

static void vdi_cursor_plot_raw(WORD x, WORD y, WORD color);
static WORD vdi_cursor_pixel_raw(WORD x, WORD y);
static void vdi_cursor_restore(void);
static void vdi_cursor_draw(void);

void vdi_presenter_cursor_sync(void)
{
    gem_raster_cursor_update(
        (int)vdi_state.cursor_x, (int)vdi_state.cursor_y,
        (const uint16_t *)(const void *)vdi_state.mouse_form.mf_mask,
        vdi_state.open && vdi_state.cursor_hidden == 0 &&
            vdi_state.cursor_drawn != 0);
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
    vdi_presenter_cursor_sync();
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
    vdi_presenter_cursor_sync();
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
    vdi_presenter_cursor_sync();
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
    vdi_presenter_cursor_sync();
}

static void vdi_cursor_plot_raw(WORD x, WORD y, WORD color)
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

static WORD vdi_cursor_pixel_raw(WORD x, WORD y)
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
            vdi_cursor_plot_raw((WORD)(vdi_state.cursor_x + x),
                                (WORD)(vdi_state.cursor_y + y),
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
            if (vdi_cursor_pixel_raw(px, py) != 0) {
                saved |= bit;
            }
            if ((vdi_state.mouse_form.mf_data[y] & bit) != 0u) {
                vdi_cursor_plot_raw(px, py, fg_color);
            } else {
                vdi_cursor_plot_raw(px, py, bg_color);
            }
        }
        g_vdi_cursor_saved[y] = saved;
    }

    vdi_state.cursor_drawn = 1;
}
