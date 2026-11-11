/*
 * Verifies thin window edges, resize hit areas and client clipping around
 * the size grip with no scrollbars, either axis, both axes, and no SIZER.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (C) 2026 tomaz stih
 */

#include "window_internal.h"

#include <assert.h>
#include <stdio.h>

static long area(const GRECT *r)
{
    return (long)r->g_w * r->g_h;
}

static unsigned long corner_pixels(VDI_HANDLE vdi, const GRECT *r)
{
    unsigned long hash = 5381;
    WORD x;
    WORD y;
    for (y = r->g_y; y < r->g_y + r->g_h; ++y) {
        for (x = r->g_x; x < r->g_x + r->g_w; ++x) {
            WORD pixel = 0;
            WORD color = 0;
            assert(v_get_pixel(vdi, x, y, &pixel, &color));
            hash = hash * 33 + (unsigned)pixel;
        }
    }
    return hash;
}

static void check_window(UWORD kind, WORD width, WORD height)
{
    WORD handle = wind_create(kind, 0, 0, 640, 400);
    aes_window_t *window;
    GRECT grip = {0};
    GRECT visible;
    GRECT overlap;
    WORD x, y, w, h;
    WORD count = 0;
    long painted = 0;
    int has_grip;
    unsigned long before = 0;
    VDI_HANDLE vdi = graf_handle(NULL, NULL, NULL, NULL);

    assert(handle > 0);
    assert(wind_open(handle, 30, 40, width, height));
    window = aes_find_window(handle);
    assert(window != NULL);
    assert(wind_calc(WC_BORDER, kind, window->work.g_x, window->work.g_y,
                     window->work.g_w, window->work.g_h, &x, &y, &w, &h));
    assert(x == 30 && y == 40 && w == width && h == height);
    has_grip = aes_window_sizer_rect(window, &grip);
    assert(has_grip == ((kind & SIZER) != 0));
    if (has_grip) {
        assert(aes_window_hit_part(window, grip.g_x + grip.g_w / 2,
                                  grip.g_y + grip.g_h / 2) == AES_WINDOW_PART_SIZER);
        before = corner_pixels(vdi, &grip);
    }
    if (aes_window_has_compact_sizer(window)) {
        WORD pixel;
        WORD color;
        WORD offset;
        /* v_get_pixel returns the raw monochrome bit, not the logical index. */
        WORD dark = aes_dark_color() == WHITE ? 1 : 0;
        for (offset = 0; offset < grip.g_w - 1; ++offset) {
            assert(v_get_pixel(vdi, grip.g_x + offset, grip.g_y, &pixel, &color));
            assert(pixel == dark);
        }
        for (offset = 0; offset < grip.g_h - 1; ++offset) {
            assert(v_get_pixel(vdi, grip.g_x, grip.g_y + offset, &pixel, &color));
            assert(pixel == dark);
        }
        assert(window->work.g_x == window->outer.g_x + 1);
        assert(window->work.g_x + window->work.g_w ==
               window->outer.g_x + window->outer.g_w - 1);
        assert(window->work.g_y + window->work.g_h ==
               window->outer.g_y + window->outer.g_h - 1);
        assert(aes_window_hit_part(window, grip.g_x - 1,
                                  window->work.g_y + window->work.g_h - 1) ==
               AES_WINDOW_PART_WORK);
        assert(aes_window_hit_part(window,
                                  window->work.g_x + window->work.g_w - 1,
                                  grip.g_y - 1) == AES_WINDOW_PART_WORK);
    }

    /* An application filling every visible rectangle must never erase the
     * resize glyph, even though a compact corner lies within WF_WORKXYWH. */
    assert(wind_get(handle, WF_FIRSTXYWH, &visible.g_x, &visible.g_y,
                    &visible.g_w, &visible.g_h));
    while (visible.g_w > 0 && visible.g_h > 0) {
        WORD box[4] = {visible.g_x, visible.g_y,
                      visible.g_x + visible.g_w - 1,
                      visible.g_y + visible.g_h - 1};
        overlap = visible;
        if (has_grip)
            assert(!aes_intersect_rects(&grip, &overlap, &overlap));
        painted += area(&visible);
        ++count;
        vsf_interior(vdi, FIS_SOLID);
        vsf_color(vdi, aes_light_color());
        vsf_perimeter(vdi, 0);
        vr_recfl(vdi, box);
        assert(wind_get(handle, WF_NEXTXYWH, &visible.g_x, &visible.g_y,
                        &visible.g_w, &visible.g_h));
    }
    overlap = window->work;
    if (!has_grip || !aes_intersect_rects(&grip, &overlap, &overlap)) {
        overlap.g_w = 0;
        overlap.g_h = 0;
    }
    assert(painted == area(&window->work) - area(&overlap));
    assert(count == (aes_window_has_compact_sizer(window) ? 2 : 1));
    if (has_grip)
        assert(before == corner_pixels(vdi, &grip));
    assert(wind_close(handle));
    assert(wind_delete(handle));
}

int main(void)
{
    const UWORD kinds[] = {
        NAME | MOVER | SIZER,
        NAME | MOVER,
        NAME | MOVER | SIZER | VSLIDE,
        NAME | MOVER | SIZER | HSLIDE,
        NAME | MOVER | SIZER | VSLIDE | HSLIDE
    };
    size_t index;
    assert(appl_init() > 0);
    for (index = 0; index < sizeof(kinds) / sizeof(kinds[0]); ++index) {
        check_window(kinds[index], 320, 240);
        check_window(kinds[index], 180, 140);
    }
    assert(appl_exit());
    puts("window frames: edge geometry, corner hit testing and paint protection passed");
    return 0;
}
