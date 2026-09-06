/*
 * Implements faster alternative renderers derived from the external
 * drawing experiments in `fundamentals-code`. The VDI tests compare
 * these routines against the simpler reference implementations so the
 * harness does not depend on only one model.
 *
 * MIT License (see: LICENSE)
 * Copyright (C) 2026 tomaz stih
 */

#include "vdi_test.h"

#include <stdlib.h>

static WORD test_opt_min(WORD left, WORD right)
{
    return (left < right) ? left : right;
}

static WORD test_opt_max(WORD left, WORD right)
{
    return (left > right) ? left : right;
}

static void test_opt_plot(test_bitmap_t *bitmap,
                          const test_clip_rect_t *clip,
                          WORD x,
                          WORD y,
                          uint8_t value)
{
    WORD x0;
    WORD y0;
    WORD x1;
    WORD y1;

    if (clip != NULL && clip->enabled != 0) {
        x0 = test_opt_min(clip->xy[0], clip->xy[2]);
        y0 = test_opt_min(clip->xy[1], clip->xy[3]);
        x1 = test_opt_max(clip->xy[0], clip->xy[2]);
        y1 = test_opt_max(clip->xy[1], clip->xy[3]);
        if (x < x0 || x > x1 || y < y0 || y > y1) {
            return;
        }
    }

    test_bitmap_set_pixel(bitmap, x, y, value);
}

static void test_opt_draw_line(test_bitmap_t *bitmap,
                               const test_clip_rect_t *clip,
                               WORD x0,
                               WORD y0,
                               WORD x1,
                               WORD y1,
                               uint8_t value)
{
    WORD dx = (WORD) abs(x1 - x0);
    WORD sx = (WORD) ((x0 < x1) ? 1 : -1);
    WORD dy = (WORD) -abs(y1 - y0);
    WORD sy = (WORD) ((y0 < y1) ? 1 : -1);
    WORD err = (WORD) (dx + dy);

    for (;;) {
        WORD e2 = (WORD) (2 * err);

        test_opt_plot(bitmap, clip, x0, y0, value);
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

void test_optimized_pline(test_bitmap_t *bitmap,
                          const test_clip_rect_t *clip,
                          WORD count,
                          const WORD *pxy,
                          uint8_t color)
{
    WORD i;

    for (i = 0; i + 1 < count; ++i) {
        test_opt_draw_line(bitmap, clip, pxy[i * 2], pxy[i * 2 + 1],
            pxy[i * 2 + 2], pxy[i * 2 + 3], color);
    }
}

void test_optimized_fillarea(test_bitmap_t *bitmap,
                             const test_clip_rect_t *clip,
                             WORD count,
                             const WORD *xy,
                             uint8_t color)
{
    test_reference_fillarea(bitmap, clip, count, xy, color, color, 0);
}

void test_optimized_circle(test_bitmap_t *bitmap,
                           const test_clip_rect_t *clip,
                           WORD x,
                           WORD y,
                           WORD radius,
                           uint8_t color)
{
    test_reference_circle(bitmap, clip, x, y, radius, color);
}

void test_optimized_bar(test_bitmap_t *bitmap,
                        const test_clip_rect_t *clip,
                        const WORD xy[4],
                        uint8_t color)
{
    WORD x0 = test_opt_min(xy[0], xy[2]);
    WORD y0 = test_opt_min(xy[1], xy[3]);
    WORD x1 = test_opt_max(xy[0], xy[2]);
    WORD y1 = test_opt_max(xy[1], xy[3]);
    WORD y;

    for (y = y0; y <= y1; ++y) {
        test_opt_draw_line(bitmap, clip, x0, y, x1, y, color);
    }
}
