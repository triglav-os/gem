/*
 * Rectangle arithmetic shared across the hosted AES: min/max words, GRECT
 * construction, point containment, intersection and the subtraction that
 * yields up to four uncovered fragments.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (C) 2026 tomaz stih
 */

#include "aes_internal.h"

WORD aes_min_word(WORD left, WORD right)
{
    return (left < right) ? left : right;
}

WORD aes_max_word(WORD left, WORD right)
{
    return (left > right) ? left : right;
}

void aes_set_rect(GRECT *rect, WORD x, WORD y, WORD w, WORD h)
{
    if (rect == NULL) {
        return;
    }

    rect->g_x = x;
    rect->g_y = y;
    rect->g_w = w;
    rect->g_h = h;
}

int aes_point_in_rect(WORD x, WORD y, const GRECT *rect)
{
    if (rect == NULL) {
        return 0;
    }

    return x >= rect->g_x && y >= rect->g_y && x < rect->g_x + rect->g_w &&
           y < rect->g_y + rect->g_h;
}

int aes_rects_intersect(const GRECT *left, const GRECT *right)
{
    WORD left_right;
    WORD left_bottom;
    WORD right_right;
    WORD right_bottom;

    if (left == NULL || right == NULL || left->g_w <= 0 || left->g_h <= 0 ||
        right->g_w <= 0 || right->g_h <= 0) {
        return 0;
    }

    left_right = (WORD)(left->g_x + left->g_w - 1);
    left_bottom = (WORD)(left->g_y + left->g_h - 1);
    right_right = (WORD)(right->g_x + right->g_w - 1);
    right_bottom = (WORD)(right->g_y + right->g_h - 1);

    if (left_right < right->g_x || right_right < left->g_x ||
        left_bottom < right->g_y || right_bottom < left->g_y) {
        return 0;
    }
    return 1;
}

int aes_intersect_rects(const GRECT *left, const GRECT *right, GRECT *out)
{
    WORD left_right;
    WORD left_bottom;
    WORD right_right;
    WORD right_bottom;
    WORD x0;
    WORD y0;
    WORD x1;
    WORD y1;

    if (out == NULL || aes_rects_intersect(left, right) == 0) {
        return 0;
    }

    left_right = (WORD)(left->g_x + left->g_w - 1);
    left_bottom = (WORD)(left->g_y + left->g_h - 1);
    right_right = (WORD)(right->g_x + right->g_w - 1);
    right_bottom = (WORD)(right->g_y + right->g_h - 1);

    x0 = aes_max_word(left->g_x, right->g_x);
    y0 = aes_max_word(left->g_y, right->g_y);
    x1 = aes_min_word(left_right, right_right);
    y1 = aes_min_word(left_bottom, right_bottom);
    aes_set_rect(out, x0, y0, (WORD)(x1 - x0 + 1), (WORD)(y1 - y0 + 1));
    return 1;
}

WORD aes_subtract_rect(const GRECT *source, const GRECT *cover, GRECT out[4])
{
    GRECT overlap;
    WORD count = 0;
    WORD source_right;
    WORD source_bottom;
    WORD overlap_right;
    WORD overlap_bottom;

    if (out == NULL || source == NULL || source->g_w <= 0 || source->g_h <= 0) {
        return 0;
    }

    if (cover == NULL || cover->g_w <= 0 || cover->g_h <= 0 ||
        aes_intersect_rects(source, cover, &overlap) == 0) {
        out[0] = *source;
        return 1;
    }

    source_right = (WORD)(source->g_x + source->g_w - 1);
    source_bottom = (WORD)(source->g_y + source->g_h - 1);
    overlap_right = (WORD)(overlap.g_x + overlap.g_w - 1);
    overlap_bottom = (WORD)(overlap.g_y + overlap.g_h - 1);

    if (source->g_y < overlap.g_y) {
        aes_set_rect(&out[count++], source->g_x, source->g_y, source->g_w,
                     (WORD)(overlap.g_y - source->g_y));
    }
    if (overlap_bottom < source_bottom) {
        aes_set_rect(&out[count++], source->g_x, (WORD)(overlap_bottom + 1),
                     source->g_w, (WORD)(source_bottom - overlap_bottom));
    }
    if (source->g_x < overlap.g_x) {
        aes_set_rect(&out[count++], source->g_x, overlap.g_y,
                     (WORD)(overlap.g_x - source->g_x), overlap.g_h);
    }
    if (overlap_right < source_right) {
        aes_set_rect(&out[count++], (WORD)(overlap_right + 1), overlap.g_y,
                     (WORD)(source_right - overlap_right), overlap.g_h);
    }

    return count;
}
