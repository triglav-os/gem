/*
 * Computes hosted AES window geometry: border sizes, work-area derivation,
 * the rectangle of every chrome part (closer, fuller, title, sizer, arrows,
 * slider slots and thumbs), title/info text storage and hit testing.
 * Painting and redraw scheduling live in separate modules.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (C) 2026 tomaz stih
 */

#include "window_internal.h"

#include <string.h>

WORD aes_window_left_border(const aes_window_t *window)
{
    return (window != NULL && window->kind != 0u) ? 1 : 0;
}

int aes_window_has_vscroll(const aes_window_t *window)
{
    return window != NULL &&
           (window->kind & (UPARROW | DNARROW | VSLIDE)) != 0u;
}

int aes_window_has_hscroll(const aes_window_t *window)
{
    return window != NULL &&
           (window->kind & (LFARROW | RTARROW | HSLIDE)) != 0u;
}

int aes_window_has_compact_sizer(const aes_window_t *window)
{
    return window != NULL && (window->kind & SIZER) != 0u &&
           aes_window_has_vscroll(window) == 0 &&
           aes_window_has_hscroll(window) == 0;
}

WORD aes_window_scroll_span(const aes_window_t *window)
{
    WORD span;

    if (window == NULL) {
        return 0;
    }

    span = (WORD)(aes_window_title_height(window) - 1);
    return aes_max_word(span, 12);
}

WORD aes_window_scroll_button_side(const aes_window_t *window)
{
    WORD side;

    if (window == NULL) {
        return 0;
    }

    side = (WORD)(aes_window_title_height(window) - 2);
    return aes_max_word(side, 10);
}

WORD aes_window_right_border(const aes_window_t *window)
{
    if (aes_window_has_compact_sizer(window) != 0) {
        return aes_window_left_border(window);
    }
    if (window != NULL &&
        ((window->kind & SIZER) != 0u || aes_window_has_vscroll(window) != 0)) {
        return aes_window_scroll_span(window);
    }
    return aes_window_left_border(window);
}

WORD aes_window_bottom_border(const aes_window_t *window)
{
    if (aes_window_has_compact_sizer(window) != 0) {
        return aes_window_left_border(window);
    }
    if (window != NULL &&
        ((window->kind & SIZER) != 0u || aes_window_has_hscroll(window) != 0)) {
        return aes_window_scroll_span(window);
    }
    return (window != NULL && window->kind != 0u) ? 2 : 0;
}

WORD aes_window_title_height(const aes_window_t *window)
{
    if (window == NULL || window->kind == 0u) {
        return 0;
    }
    if ((window->kind & (NAME | CLOSER | FULLER | MOVER)) == 0u) {
        return 2;
    }
    return aes_menu_chrome_height();
}

int aes_window_closer_rect(const aes_window_t *window, GRECT *rect)
{
    WORD title_height;
    WORD side;

    if (window == NULL || rect == NULL || (window->kind & CLOSER) == 0u) {
        return 0;
    }

    title_height = aes_window_title_height(window);
    side = (WORD)(title_height - 2);
    aes_set_rect(rect, (WORD)(window->outer.g_x + 1),
                 (WORD)(window->outer.g_y + 1), (WORD)(side + 1), side);
    return 1;
}

int aes_window_fuller_rect(const aes_window_t *window, GRECT *rect)
{
    WORD title_height;
    WORD side;

    if (window == NULL || rect == NULL || (window->kind & FULLER) == 0u) {
        return 0;
    }

    title_height = aes_window_title_height(window);
    side = (WORD)(title_height - 2);
    aes_set_rect(rect, (WORD)(window->outer.g_x + window->outer.g_w - side - 2),
                 (WORD)(window->outer.g_y + 1), (WORD)(side + 1), side);
    return 1;
}

int aes_window_title_rect(const aes_window_t *window, GRECT *rect)
{
    GRECT closer_rect = {0, 0, 0, 0};
    GRECT fuller_rect = {0, 0, 0, 0};
    WORD left;
    WORD right;
    WORD title_height;

    if (window == NULL || rect == NULL ||
        (window->kind & (NAME | MOVER | CLOSER | FULLER)) == 0u) {
        return 0;
    }

    title_height = aes_window_title_height(window);
    left = (WORD)(window->outer.g_x + 2);
    right = (WORD)(window->outer.g_x + window->outer.g_w - 3);
    if (aes_window_closer_rect(window, &closer_rect) != 0) {
        left = (WORD)(closer_rect.g_x + closer_rect.g_w + 2);
    }
    if (aes_window_fuller_rect(window, &fuller_rect) != 0) {
        right = (WORD)(fuller_rect.g_x - 2);
    }
    if (right < left) {
        return 0;
    }

    aes_set_rect(rect, left, (WORD)(window->outer.g_y + 2),
                 (WORD)(right - left + 1), (WORD)(title_height - 4));
    return 1;
}

int aes_window_sizer_rect(const aes_window_t *window, GRECT *rect)
{
    WORD size;
    WORD right_border;
    WORD bottom_border;

    if (window == NULL || rect == NULL || (window->kind & SIZER) == 0u) {
        return 0;
    }

    /* A compact window reserves only this corner for its size grip.
     * Client visible rectangles exclude it, including the part inside work. */
    if (aes_window_has_compact_sizer(window) != 0) {
        size = aes_min_word(aes_window_scroll_span(window),
                            aes_min_word(window->outer.g_w, window->outer.g_h));
        if (size < 3) {
            return 0;
        }
        aes_set_rect(rect,
                     (WORD)(window->outer.g_x + window->outer.g_w - size),
                     (WORD)(window->outer.g_y + window->outer.g_h - size),
                     size, size);
        return 1;
    }

    right_border = aes_window_right_border(window);
    bottom_border = aes_window_bottom_border(window);
    size = (WORD)(aes_min_word(right_border, bottom_border) - 2);
    aes_set_rect(
        rect, (WORD)(window->outer.g_x + window->outer.g_w - right_border + 2),
        (WORD)(window->outer.g_y + window->outer.g_h - bottom_border + 2), size,
        size);
    return 1;
}

int aes_window_vtrack_rect(const aes_window_t *window, GRECT *rect)
{
    WORD left;
    WORD right;
    WORD top;
    WORD bottom;

    if (window == NULL || rect == NULL || aes_window_has_vscroll(window) == 0) {
        return 0;
    }

    left = (WORD)(window->work.g_x + window->work.g_w);
    right = (WORD)(window->outer.g_x + window->outer.g_w - 2);
    top = window->work.g_y;
    bottom = (WORD)(window->work.g_y + window->work.g_h - 1);
    if (right < left || bottom < top) {
        return 0;
    }

    aes_set_rect(rect, left, top, (WORD)(right - left + 1),
                 (WORD)(bottom - top + 1));
    return 1;
}

int aes_window_htrack_rect(const aes_window_t *window, GRECT *rect)
{
    WORD left;
    WORD right;
    WORD top;
    WORD bottom;

    if (window == NULL || rect == NULL || aes_window_has_hscroll(window) == 0) {
        return 0;
    }

    left = (WORD)(window->outer.g_x + 1);
    right = (WORD)(window->work.g_x + window->work.g_w - 1);
    top = (WORD)(window->work.g_y + window->work.g_h);
    bottom = (WORD)(window->outer.g_y + window->outer.g_h - 2);
    if (right < left || bottom < top) {
        return 0;
    }

    aes_set_rect(rect, left, top, (WORD)(right - left + 1),
                 (WORD)(bottom - top + 1));
    return 1;
}

int aes_window_vslot_rect(const aes_window_t *window, GRECT *rect)
{
    GRECT track;
    GRECT up_rect;
    GRECT down_rect;
    WORD top;
    WORD bottom;

    if (window == NULL || rect == NULL ||
        aes_window_vtrack_rect(window, &track) == 0) {
        return 0;
    }

    top = track.g_y;
    bottom = (WORD)(track.g_y + track.g_h - 1);
    if (aes_window_vup_rect(window, &up_rect) != 0) {
        top = (WORD)(up_rect.g_y + up_rect.g_h);
    }
    if (aes_window_vdown_rect(window, &down_rect) != 0) {
        bottom = (WORD)(down_rect.g_y - 1);
    }
    if (bottom < top) {
        return 0;
    }

    aes_set_rect(rect, track.g_x, top, track.g_w, (WORD)(bottom - top + 1));
    return 1;
}

int aes_window_hslot_rect(const aes_window_t *window, GRECT *rect)
{
    GRECT track;
    GRECT left_rect;
    GRECT right_rect;
    WORD left;
    WORD right;

    if (window == NULL || rect == NULL ||
        aes_window_htrack_rect(window, &track) == 0) {
        return 0;
    }

    left = track.g_x;
    right = (WORD)(track.g_x + track.g_w - 1);
    if (aes_window_hleft_rect(window, &left_rect) != 0) {
        left = (WORD)(left_rect.g_x + left_rect.g_w);
    }
    if (aes_window_hright_rect(window, &right_rect) != 0) {
        right = (WORD)(right_rect.g_x - 1);
    }
    if (right < left) {
        return 0;
    }

    aes_set_rect(rect, left, track.g_y, (WORD)(right - left + 1), track.g_h);
    return 1;
}

int aes_window_vup_rect(const aes_window_t *window, GRECT *rect)
{
    GRECT track;
    WORD side;

    if (window == NULL || rect == NULL || (window->kind & UPARROW) == 0u ||
        aes_window_vtrack_rect(window, &track) == 0) {
        return 0;
    }

    side = aes_min_word(aes_window_scroll_button_side(window), track.g_h);
    aes_set_rect(rect, track.g_x, track.g_y, track.g_w, side);
    return 1;
}

int aes_window_vdown_rect(const aes_window_t *window, GRECT *rect)
{
    GRECT track;
    WORD side;

    if (window == NULL || rect == NULL || (window->kind & DNARROW) == 0u ||
        aes_window_vtrack_rect(window, &track) == 0) {
        return 0;
    }

    side = aes_min_word(aes_window_scroll_button_side(window), track.g_h);
    aes_set_rect(rect, track.g_x, (WORD)(track.g_y + track.g_h - side),
                 track.g_w, side);
    return 1;
}

int aes_window_hleft_rect(const aes_window_t *window, GRECT *rect)
{
    GRECT track;
    WORD side;

    if (window == NULL || rect == NULL || (window->kind & LFARROW) == 0u ||
        aes_window_htrack_rect(window, &track) == 0) {
        return 0;
    }

    side = aes_min_word(aes_window_scroll_button_side(window), track.g_w);
    aes_set_rect(rect, track.g_x, track.g_y, side, track.g_h);
    return 1;
}

int aes_window_hright_rect(const aes_window_t *window, GRECT *rect)
{
    GRECT track;
    WORD side;

    if (window == NULL || rect == NULL || (window->kind & RTARROW) == 0u ||
        aes_window_htrack_rect(window, &track) == 0) {
        return 0;
    }

    side = aes_min_word(aes_window_scroll_button_side(window), track.g_w);
    aes_set_rect(rect, (WORD)(track.g_x + track.g_w - side), track.g_y, side,
                 track.g_h);
    return 1;
}

int aes_window_vthumb_rect(const aes_window_t *window, GRECT *rect)
{
    GRECT slot;
    WORD span;
    WORD size;
    WORD pos;

    if (window == NULL || rect == NULL || (window->kind & VSLIDE) == 0u ||
        aes_window_vslot_rect(window, &slot) == 0) {
        return 0;
    }

    span = slot.g_h;
    size =
        (WORD)((span * aes_max_word(0, aes_min_word(window->vslsize, 1000))) /
               1000L);
    size = aes_max_word(size, aes_min_word(slot.g_w, span));
    size = aes_min_word(size, span);
    pos = slot.g_y;
    if (span > size) {
        pos = (WORD)(slot.g_y +
                     ((span - size) *
                      aes_max_word(0, aes_min_word(window->vslide, 1000))) /
                         1000L);
    }

    aes_set_rect(rect, slot.g_x, pos, slot.g_w, size);
    return 1;
}

int aes_window_hthumb_rect(const aes_window_t *window, GRECT *rect)
{
    GRECT slot;
    WORD span;
    WORD size;
    WORD pos;

    if (window == NULL || rect == NULL || (window->kind & HSLIDE) == 0u ||
        aes_window_hslot_rect(window, &slot) == 0) {
        return 0;
    }

    span = slot.g_w;
    size =
        (WORD)((span * aes_max_word(0, aes_min_word(window->hslsize, 1000))) /
               1000L);
    size = aes_max_word(size, aes_min_word(slot.g_h, span));
    size = aes_min_word(size, span);
    pos = slot.g_x;
    if (span > size) {
        pos = (WORD)(slot.g_x +
                     ((span - size) *
                      aes_max_word(0, aes_min_word(window->hslide, 1000))) /
                         1000L);
    }

    aes_set_rect(rect, pos, slot.g_y, size, slot.g_h);
    return 1;
}

void aes_compute_work(aes_window_t *window)
{
    WORD left_border;
    WORD right_border;
    WORD bottom_border;
    WORD title_height;

    if (window == NULL) {
        return;
    }

    left_border = aes_window_left_border(window);
    right_border = aes_window_right_border(window);
    bottom_border = aes_window_bottom_border(window);
    title_height = aes_window_title_height(window);
    aes_set_rect(&window->work, (WORD)(window->outer.g_x + left_border),
                 (WORD)(window->outer.g_y + title_height),
                 (WORD)(window->outer.g_w - left_border - right_border),
                 (WORD)(window->outer.g_h - title_height - bottom_border));
    if (window->work.g_w < 0) {
        window->work.g_w = 0;
    }
    if (window->work.g_h < 0) {
        window->work.g_h = 0;
    }
}

WORD aes_wind_set_text(WORD handle, WORD field, const char *text)
{
    aes_window_t *window = aes_find_window(handle);
    char *target;
    size_t size;

    if (window == NULL || text == NULL) {
        return 0;
    }

    if (field == WF_NAME) {
        target = window->name;
        size = sizeof(window->name);
    } else if (field == WF_INFO) {
        target = window->info;
        size = sizeof(window->info);
    } else {
        return 0;
    }

    if (strncmp(target, text, size) == 0) {
        return 1;
    }

    strncpy(target, text, size - 1u);
    target[size - 1u] = '\0';
    if (window->open != 0) {
        aes_redraw_window_text(window);
    }
    return 1;
}

WORD aes_window_hit_part(const aes_window_t *window, WORD x, WORD y)
{
    GRECT rect;
    GRECT thumb;

    if (window == NULL || window->open == 0 ||
        !aes_point_in_rect(x, y, &window->outer)) {
        return AES_WINDOW_PART_NONE;
    }

    if (aes_window_closer_rect(window, &rect) != 0 &&
        aes_point_in_rect(x, y, &rect)) {
        return AES_WINDOW_PART_CLOSER;
    }
    if (aes_window_fuller_rect(window, &rect) != 0 &&
        aes_point_in_rect(x, y, &rect)) {
        return AES_WINDOW_PART_FULLER;
    }
    if (aes_window_sizer_rect(window, &rect) != 0 &&
        aes_point_in_rect(x, y, &rect)) {
        return AES_WINDOW_PART_SIZER;
    }
    if (aes_window_vup_rect(window, &rect) != 0 &&
        aes_point_in_rect(x, y, &rect)) {
        return AES_WINDOW_PART_VUP;
    }
    if (aes_window_vdown_rect(window, &rect) != 0 &&
        aes_point_in_rect(x, y, &rect)) {
        return AES_WINDOW_PART_VDOWN;
    }
    if (aes_window_hleft_rect(window, &rect) != 0 &&
        aes_point_in_rect(x, y, &rect)) {
        return AES_WINDOW_PART_HLEFT;
    }
    if (aes_window_hright_rect(window, &rect) != 0 &&
        aes_point_in_rect(x, y, &rect)) {
        return AES_WINDOW_PART_HRIGHT;
    }
    if (aes_window_vthumb_rect(window, &thumb) != 0 &&
        aes_point_in_rect(x, y, &thumb)) {
        return AES_WINDOW_PART_VSLIDE;
    }
    if (aes_window_hthumb_rect(window, &thumb) != 0 &&
        aes_point_in_rect(x, y, &thumb)) {
        return AES_WINDOW_PART_HSLIDE;
    }
    if (aes_window_vtrack_rect(window, &rect) != 0 &&
        aes_point_in_rect(x, y, &rect)) {
        if (aes_window_vthumb_rect(window, &thumb) != 0 && y < thumb.g_y) {
            return AES_WINDOW_PART_VPAGE_UP;
        }
        return AES_WINDOW_PART_VPAGE_DOWN;
    }
    if (aes_window_htrack_rect(window, &rect) != 0 &&
        aes_point_in_rect(x, y, &rect)) {
        if (aes_window_hthumb_rect(window, &thumb) != 0 && x < thumb.g_x) {
            return AES_WINDOW_PART_HPAGE_LEFT;
        }
        return AES_WINDOW_PART_HPAGE_RIGHT;
    }
    if (aes_window_title_rect(window, &rect) != 0 &&
        aes_point_in_rect(x, y, &rect)) {
        return AES_WINDOW_PART_TITLE;
    }
    if (aes_point_in_rect(x, y, &window->work)) {
        return AES_WINDOW_PART_WORK;
    }
    return AES_WINDOW_PART_NONE;
}
