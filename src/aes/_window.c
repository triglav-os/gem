/*
 * Implements the private hosted AES window geometry, redraw,
 * frame drawing, and object-tree rendering helpers.
 *
 * MIT License (see: LICENSE)
 * Copyright (C) 2026 tomaz stih
 */

#include "_aes.h"

#include "../vdi/_internal.h"

#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static WORD _aes_window_left_border(const aes_window_t *window);
static int _aes_window_has_vscroll(const aes_window_t *window);
static int _aes_window_has_hscroll(const aes_window_t *window);
static WORD _aes_window_scroll_span(const aes_window_t *window);
static WORD _aes_window_scroll_button_side(const aes_window_t *window);
static WORD _aes_window_right_border(const aes_window_t *window);
static WORD _aes_window_bottom_border(const aes_window_t *window);
static WORD _aes_window_title_height(const aes_window_t *window);
static int _aes_is_window_work_root(const OBJECT *tree);
static int _aes_is_dialog_root_tree(const OBJECT *tree);
static int _aes_is_menu_bar_object(const OBJECT *tree, WORD object);
static int _aes_is_dialog_frame_object(const OBJECT *tree, WORD object,
                                       WORD parent);
static int _aes_window_closer_rect(const aes_window_t *window, GRECT *rect);
static int _aes_window_fuller_rect(const aes_window_t *window, GRECT *rect);
static int _aes_window_title_rect(const aes_window_t *window, GRECT *rect);
static int _aes_window_sizer_rect(const aes_window_t *window, GRECT *rect);
int _aes_window_vtrack_rect(const aes_window_t *window, GRECT *rect);
int _aes_window_htrack_rect(const aes_window_t *window, GRECT *rect);
int _aes_window_vslot_rect(const aes_window_t *window, GRECT *rect);
int _aes_window_hslot_rect(const aes_window_t *window, GRECT *rect);
static int _aes_window_vup_rect(const aes_window_t *window, GRECT *rect);
static int _aes_window_vdown_rect(const aes_window_t *window, GRECT *rect);
static int _aes_window_hleft_rect(const aes_window_t *window, GRECT *rect);
static int _aes_window_hright_rect(const aes_window_t *window, GRECT *rect);
int _aes_window_vthumb_rect(const aes_window_t *window, GRECT *rect);
int _aes_window_hthumb_rect(const aes_window_t *window, GRECT *rect);
void _aes_compute_work(aes_window_t *window);
WORD _aes_wind_set_text(WORD handle, WORD field, const char *text);
static void _aes_present_after_window_draw(void);
static void _aes_desktop_rect_local(GRECT *rect);
static void _aes_expand_window_damage_rect(const GRECT *src, GRECT *out);
static void _aes_window_draw_cover_rect(const aes_window_t *window, GRECT *out);
void _aes_redraw_region(const GRECT *dirty);
static void _aes_queue_window_redraw(const aes_window_t *window,
                                     const GRECT *dirty);
static void _aes_queue_desktop_redraw(const GRECT *dirty);
void _aes_redraw_window_change(const GRECT *before, const GRECT *after);
void _aes_redraw_window_title_states(const aes_window_t *previous_top,
                                     const aes_window_t *new_top);
static void _aes_fill_rect(WORD x0, WORD y0, WORD x1, WORD y1, WORD color);
static void _aes_draw_rect_edges(WORD x0, WORD y0, WORD x1, WORD y1,
                                 int draw_top,
                                 int draw_right,
                                 int draw_bottom,
                                 int draw_left);
static void _aes_invert_rect(WORD x0, WORD y0, WORD x1, WORD y1);
static void _aes_fill_pattern_rect(WORD x0, WORD y0, WORD x1, WORD y1,
                                   const uint8_t *rows, size_t row_count);
static void _aes_fill_checker_rect(WORD x0, WORD y0, WORD x1, WORD y1);
int _aes_window_is_top(const aes_window_t *window);
static void _aes_draw_window_icon(WORD x0, WORD y0, WORD x1, WORD y1,
                                  char glyph);
static void _aes_draw_window_icon_offset(WORD x0, WORD y0, WORD x1, WORD y1,
                                         char glyph, WORD dx, WORD dy);
static void _aes_draw_sizer_glyph(const aes_window_t *window);
static void _aes_draw_window_title(const aes_window_t *window,
                                   const WORD outer_box[4],
                                   WORD title_height);
static void _aes_draw_trace(const char *fmt, ...);
static void _aes_stipple_text_pixels(WORD x, WORD y, WORD foreground,
                                     WORD background, const char *text);
void _aes_draw_window_frame(const aes_window_t *window);
void _aes_clear_window_frame(const aes_window_t *window);
void _aes_redraw_open_windows(void);
void _aes_raise_window(aes_window_t *window);
WORD _aes_window_hit_part(const aes_window_t *window, WORD x, WORD y);
void _aes_object_extent(OBJECT *tree, WORD object, WORD *x, WORD *y);
static void _aes_draw_text(WORD x, WORD y, WORD color, const char *text);
static void _aes_draw_ted_object(const OBJECT *tree,
                                 WORD object,
                                 const OBJECT *obj,
                                 const TEDINFO *ted,
                                 const WORD rect[4]);
static void _aes_draw_hline(WORD x0, WORD x1, WORD y, WORD color);
static void _aes_draw_vline(WORD x, WORD y0, WORD y1, WORD color);
static void _aes_draw_dialog_frame(const WORD rect[4]);
static void _aes_draw_button_frame(const WORD rect[4], WORD dark_color,
                                   WORD light_color, int default_button);
static void _aes_draw_icon_object(const OBJECT *obj,
                                  const ICONBLK *icon,
                                  WORD abs_x,
                                  WORD abs_y,
                                  const WORD rect[4]);
static void _aes_draw_user_object(const OBJECT *tree,
                                  WORD object,
                                  const OBJECT *obj,
                                  WORD abs_x,
                                  WORD abs_y,
                                  WORD clip[4]);
static void _aes_draw_object(const OBJECT *tree,
                             WORD object,
                             WORD abs_x,
                             WORD abs_y,
                             WORD clip[4]);
void _aes_draw_tree_recursive(const OBJECT *tree,
                              WORD object,
                              WORD parent_x,
                              WORD parent_y,
                              WORD depth,
                              WORD clip[4]);
WORD _aes_find_in_subtree(OBJECT *tree,
                          WORD object,
                          WORD parent_x,
                          WORD parent_y,
                          WORD depth,
                          WORD mx,
                          WORD my);

static WORD _aes_window_left_border(const aes_window_t *window)
{
    return (window != NULL && window->kind != 0u) ? 1 : 0;
}

static int _aes_window_has_vscroll(const aes_window_t *window)
{
    return window != NULL &&
        (window->kind & (UPARROW | DNARROW | VSLIDE)) != 0u;
}

static int _aes_window_has_hscroll(const aes_window_t *window)
{
    return window != NULL &&
        (window->kind & (LFARROW | RTARROW | HSLIDE)) != 0u;
}

static WORD _aes_window_scroll_span(const aes_window_t *window)
{
    WORD span;

    if (window == NULL) {
        return 0;
    }

    span = (WORD) (_aes_window_title_height(window) - 1);
    return _aes_max_word(span, 12);
}

static WORD _aes_window_scroll_button_side(const aes_window_t *window)
{
    WORD side;

    if (window == NULL) {
        return 0;
    }

    side = (WORD) (_aes_window_title_height(window) - 2);
    return _aes_max_word(side, 10);
}

static WORD _aes_window_right_border(const aes_window_t *window)
{
    if (window != NULL &&
        ((window->kind & SIZER) != 0u || _aes_window_has_vscroll(window) != 0)) {
        return _aes_window_scroll_span(window);
    }
    return _aes_window_left_border(window);
}

static WORD _aes_window_bottom_border(const aes_window_t *window)
{
    if (window != NULL &&
        ((window->kind & SIZER) != 0u || _aes_window_has_hscroll(window) != 0)) {
        return _aes_window_scroll_span(window);
    }
    return (window != NULL && window->kind != 0u) ? 2 : 0;
}

static WORD _aes_window_title_height(const aes_window_t *window)
{
    if (window == NULL || window->kind == 0u) {
        return 0;
    }
    if ((window->kind & (NAME | CLOSER | FULLER | MOVER)) == 0u) {
        return 2;
    }
    return _aes_menu_chrome_height();
}

static int _aes_is_window_work_root(const OBJECT *tree)
{
    size_t i;

    if (tree == NULL) {
        return 0;
    }

    for (i = 0; i < AES_MAX_WINDOWS; ++i) {
        const aes_window_t *window = &_aes.windows[i];

        if (window->used == 0 || window->open == 0) {
            continue;
        }
        if (tree[ROOT].ob_x == window->work.g_x &&
            tree[ROOT].ob_y == window->work.g_y &&
            tree[ROOT].ob_width == window->work.g_w &&
            tree[ROOT].ob_height == window->work.g_h) {
            return 1;
        }
    }

    return 0;
}

static int _aes_is_dialog_root_tree(const OBJECT *tree)
{
    if (tree == NULL || tree == _aes.menu_tree) {
        return 0;
    }

    if (_aes_is_window_work_root(tree) != 0) {
        return 0;
    }

    if (tree[ROOT].ob_head == NIL) {
        return 0;
    }

    return (tree[ROOT].ob_type == G_IBOX || tree[ROOT].ob_type == G_BOX) ?
        1 : 0;
}

static int _aes_is_menu_bar_object(const OBJECT *tree, WORD object)
{
    WORD bar;

    if (tree == NULL || tree != _aes.menu_tree || object < 0) {
        return 0;
    }

    bar = tree[ROOT].ob_head;
    return (bar != NIL && object == bar) ? 1 : 0;
}

static int _aes_is_dialog_frame_object(const OBJECT *tree, WORD object,
                                       WORD parent)
{
    if (tree == NULL || object <= ROOT || parent != ROOT) {
        return 0;
    }

    if (_aes_is_dialog_root_tree(tree) == 0) {
        return 0;
    }

    return (tree[object].ob_type == G_BOX || tree[object].ob_type == G_IBOX) ?
        1 : 0;
}

static int _aes_window_closer_rect(const aes_window_t *window, GRECT *rect)
{
    WORD title_height;
    WORD side;

    if (window == NULL || rect == NULL || (window->kind & CLOSER) == 0u) {
        return 0;
    }

    title_height = _aes_window_title_height(window);
    side = (WORD) (title_height - 2);
    _aes_set_rect(rect,
        (WORD) (window->outer.g_x + 1),
        (WORD) (window->outer.g_y + 1),
        (WORD) (side + 1),
        side);
    return 1;
}

static int _aes_window_fuller_rect(const aes_window_t *window, GRECT *rect)
{
    WORD title_height;
    WORD side;

    if (window == NULL || rect == NULL || (window->kind & FULLER) == 0u) {
        return 0;
    }

    title_height = _aes_window_title_height(window);
    side = (WORD) (title_height - 2);
    _aes_set_rect(rect,
        (WORD) (window->outer.g_x + window->outer.g_w - side - 2),
        (WORD) (window->outer.g_y + 1),
        (WORD) (side + 1),
        side);
    return 1;
}

static int _aes_window_title_rect(const aes_window_t *window, GRECT *rect)
{
    GRECT closer_rect = { 0, 0, 0, 0 };
    GRECT fuller_rect = { 0, 0, 0, 0 };
    WORD left;
    WORD right;
    WORD title_height;

    if (window == NULL || rect == NULL ||
        (window->kind & (NAME | MOVER | CLOSER | FULLER)) == 0u) {
        return 0;
    }

    title_height = _aes_window_title_height(window);
    left = (WORD) (window->outer.g_x + 2);
    right = (WORD) (window->outer.g_x + window->outer.g_w - 3);
    if (_aes_window_closer_rect(window, &closer_rect) != 0) {
        left = (WORD) (closer_rect.g_x + closer_rect.g_w + 2);
    }
    if (_aes_window_fuller_rect(window, &fuller_rect) != 0) {
        right = (WORD) (fuller_rect.g_x - 2);
    }
    if (right < left) {
        return 0;
    }

    _aes_set_rect(rect, left, (WORD) (window->outer.g_y + 2),
        (WORD) (right - left + 1), (WORD) (title_height - 4));
    return 1;
}

static int _aes_window_sizer_rect(const aes_window_t *window, GRECT *rect)
{
    WORD size;
    WORD right_border;
    WORD bottom_border;

    if (window == NULL || rect == NULL || (window->kind & SIZER) == 0u) {
        return 0;
    }

    right_border = _aes_window_right_border(window);
    bottom_border = _aes_window_bottom_border(window);
    size = (WORD) (_aes_min_word(right_border, bottom_border) - 2);
    _aes_set_rect(rect,
        (WORD) (window->outer.g_x + window->outer.g_w - right_border + 2),
        (WORD) (window->outer.g_y + window->outer.g_h - bottom_border + 2),
        size,
        size);
    return 1;
}

int _aes_window_vtrack_rect(const aes_window_t *window, GRECT *rect)
{
    WORD left;
    WORD right;
    WORD top;
    WORD bottom;

    if (window == NULL || rect == NULL || _aes_window_has_vscroll(window) == 0) {
        return 0;
    }

    left = (WORD) (window->work.g_x + window->work.g_w);
    right = (WORD) (window->outer.g_x + window->outer.g_w - 2);
    top = window->work.g_y;
    bottom = (WORD) (window->work.g_y + window->work.g_h - 1);
    if (right < left || bottom < top) {
        return 0;
    }

    _aes_set_rect(rect, left, top, (WORD) (right - left + 1),
        (WORD) (bottom - top + 1));
    return 1;
}

int _aes_window_htrack_rect(const aes_window_t *window, GRECT *rect)
{
    WORD left;
    WORD right;
    WORD top;
    WORD bottom;

    if (window == NULL || rect == NULL || _aes_window_has_hscroll(window) == 0) {
        return 0;
    }

    left = (WORD) (window->outer.g_x + 1);
    right = (WORD) (window->work.g_x + window->work.g_w - 1);
    top = (WORD) (window->work.g_y + window->work.g_h);
    bottom = (WORD) (window->outer.g_y + window->outer.g_h - 2);
    if (right < left || bottom < top) {
        return 0;
    }

    _aes_set_rect(rect, left, top, (WORD) (right - left + 1),
        (WORD) (bottom - top + 1));
    return 1;
}

int _aes_window_vslot_rect(const aes_window_t *window, GRECT *rect)
{
    GRECT track;
    GRECT up_rect;
    GRECT down_rect;
    WORD top;
    WORD bottom;

    if (window == NULL || rect == NULL ||
        _aes_window_vtrack_rect(window, &track) == 0) {
        return 0;
    }

    top = track.g_y;
    bottom = (WORD) (track.g_y + track.g_h - 1);
    if (_aes_window_vup_rect(window, &up_rect) != 0) {
        top = (WORD) (up_rect.g_y + up_rect.g_h);
    }
    if (_aes_window_vdown_rect(window, &down_rect) != 0) {
        bottom = (WORD) (down_rect.g_y - 1);
    }
    if (bottom < top) {
        return 0;
    }

    _aes_set_rect(rect, track.g_x, top, track.g_w,
        (WORD) (bottom - top + 1));
    return 1;
}

int _aes_window_hslot_rect(const aes_window_t *window, GRECT *rect)
{
    GRECT track;
    GRECT left_rect;
    GRECT right_rect;
    WORD left;
    WORD right;

    if (window == NULL || rect == NULL ||
        _aes_window_htrack_rect(window, &track) == 0) {
        return 0;
    }

    left = track.g_x;
    right = (WORD) (track.g_x + track.g_w - 1);
    if (_aes_window_hleft_rect(window, &left_rect) != 0) {
        left = (WORD) (left_rect.g_x + left_rect.g_w);
    }
    if (_aes_window_hright_rect(window, &right_rect) != 0) {
        right = (WORD) (right_rect.g_x - 1);
    }
    if (right < left) {
        return 0;
    }

    _aes_set_rect(rect, left, track.g_y, (WORD) (right - left + 1), track.g_h);
    return 1;
}

static int _aes_window_vup_rect(const aes_window_t *window, GRECT *rect)
{
    GRECT track;
    WORD side;

    if (window == NULL || rect == NULL || (window->kind & UPARROW) == 0u ||
        _aes_window_vtrack_rect(window, &track) == 0) {
        return 0;
    }

    side = _aes_min_word(_aes_window_scroll_button_side(window), track.g_h);
    _aes_set_rect(rect, track.g_x, track.g_y, track.g_w, side);
    return 1;
}

static int _aes_window_vdown_rect(const aes_window_t *window, GRECT *rect)
{
    GRECT track;
    WORD side;

    if (window == NULL || rect == NULL || (window->kind & DNARROW) == 0u ||
        _aes_window_vtrack_rect(window, &track) == 0) {
        return 0;
    }

    side = _aes_min_word(_aes_window_scroll_button_side(window), track.g_h);
    _aes_set_rect(rect, track.g_x, (WORD) (track.g_y + track.g_h - side),
        track.g_w, side);
    return 1;
}

static int _aes_window_hleft_rect(const aes_window_t *window, GRECT *rect)
{
    GRECT track;
    WORD side;

    if (window == NULL || rect == NULL || (window->kind & LFARROW) == 0u ||
        _aes_window_htrack_rect(window, &track) == 0) {
        return 0;
    }

    side = _aes_min_word(_aes_window_scroll_button_side(window), track.g_w);
    _aes_set_rect(rect, track.g_x, track.g_y, side, track.g_h);
    return 1;
}

static int _aes_window_hright_rect(const aes_window_t *window, GRECT *rect)
{
    GRECT track;
    WORD side;

    if (window == NULL || rect == NULL || (window->kind & RTARROW) == 0u ||
        _aes_window_htrack_rect(window, &track) == 0) {
        return 0;
    }

    side = _aes_min_word(_aes_window_scroll_button_side(window), track.g_w);
    _aes_set_rect(rect, (WORD) (track.g_x + track.g_w - side), track.g_y,
        side, track.g_h);
    return 1;
}

int _aes_window_vthumb_rect(const aes_window_t *window, GRECT *rect)
{
    GRECT slot;
    WORD span;
    WORD size;
    WORD pos;

    if (window == NULL || rect == NULL || (window->kind & VSLIDE) == 0u ||
        _aes_window_vslot_rect(window, &slot) == 0) {
        return 0;
    }

    span = slot.g_h;
    size = (WORD) ((span * _aes_max_word(0, _aes_min_word(window->vslsize, 1000)))
        / 1000L);
    size = _aes_max_word(size, _aes_min_word(slot.g_w, span));
    size = _aes_min_word(size, span);
    pos = slot.g_y;
    if (span > size) {
        pos = (WORD) (slot.g_y + ((span - size) *
            _aes_max_word(0, _aes_min_word(window->vslide, 1000))) / 1000L);
    }

    _aes_set_rect(rect, slot.g_x, pos, slot.g_w, size);
    return 1;
}

int _aes_window_hthumb_rect(const aes_window_t *window, GRECT *rect)
{
    GRECT slot;
    WORD span;
    WORD size;
    WORD pos;

    if (window == NULL || rect == NULL || (window->kind & HSLIDE) == 0u ||
        _aes_window_hslot_rect(window, &slot) == 0) {
        return 0;
    }

    span = slot.g_w;
    size = (WORD) ((span * _aes_max_word(0, _aes_min_word(window->hslsize, 1000)))
        / 1000L);
    size = _aes_max_word(size, _aes_min_word(slot.g_h, span));
    size = _aes_min_word(size, span);
    pos = slot.g_x;
    if (span > size) {
        pos = (WORD) (slot.g_x + ((span - size) *
            _aes_max_word(0, _aes_min_word(window->hslide, 1000))) / 1000L);
    }

    _aes_set_rect(rect, pos, slot.g_y, size, slot.g_h);
    return 1;
}

void _aes_compute_work(aes_window_t *window)
{
    WORD left_border;
    WORD right_border;
    WORD bottom_border;
    WORD title_height;

    if (window == NULL) {
        return;
    }

    left_border = _aes_window_left_border(window);
    right_border = _aes_window_right_border(window);
    bottom_border = _aes_window_bottom_border(window);
    title_height = _aes_window_title_height(window);
    _aes_set_rect(&window->work,
        (WORD) (window->outer.g_x + left_border),
        (WORD) (window->outer.g_y + title_height),
        (WORD) (window->outer.g_w - left_border - right_border),
        (WORD) (window->outer.g_h - title_height - bottom_border));
    if (window->work.g_w < 0) {
        window->work.g_w = 0;
    }
    if (window->work.g_h < 0) {
        window->work.g_h = 0;
    }
}

WORD _aes_wind_set_text(WORD handle, WORD field, const char *text)
{
    aes_window_t *window = _aes_find_window(handle);
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
        _aes_draw_window_frame(window);
    }
    return 1;
}

static void _aes_present_after_window_draw(void)
{
    if (_aes.update_depth > 0) {
        return;
    }

    _vdi_begin_update();
    _vdi_present_screen();
    _vdi_end_update();
}

static void _aes_desktop_rect_local(GRECT *rect)
{
    WORD menu_height;

    if (rect == NULL) {
        return;
    }

    if (_aes.vdi_ready == 0) {
        _aes_set_rect(rect, 0, 0, 0, 0);
        return;
    }

    menu_height = _aes_menu_bar_height();
    _aes_set_rect(rect, 0, menu_height, (WORD) (_aes.work_out[0] + 1),
        (WORD) (_aes_max_word((WORD) 0,
            (WORD) (_aes.work_out[1] + 1 - menu_height))));
}

static void _aes_expand_window_damage_rect(const GRECT *src, GRECT *out)
{
    if (out == NULL) {
        return;
    }
    if (src == NULL || src->g_w <= 0 || src->g_h <= 0) {
        _aes_set_rect(out, 0, 0, 0, 0);
        return;
    }

    /*
     * Hosted AES draws a one-pixel frame shadow on the right and bottom
     * edges, so damage tracking must include that extra extent.
     */
    out->g_x = src->g_x;
    out->g_y = src->g_y;
    out->g_w = (WORD) (src->g_w + 1);
    out->g_h = (WORD) (src->g_h + 1);
}

static void _aes_window_draw_cover_rect(const aes_window_t *window, GRECT *out)
{
    if (out == NULL) {
        return;
    }

    if (window == NULL || window->open == 0 || window->used == 0) {
        _aes_set_rect(out, 0, 0, 0, 0);
        return;
    }

    _aes_expand_window_damage_rect(&window->outer, out);
}

void _aes_redraw_region(const GRECT *dirty)
{
    WORD clip[4];
    GRECT desktop;
    GRECT menu_rect;
    WORD bar;
    int redraw_menu = 0;

    if (dirty == NULL || dirty->g_w <= 0 || dirty->g_h <= 0 ||
        _aes.vdi_ready == 0) {
        return;
    }

    _aes_trace("redraw_region dirty=%d,%d %dx%d",
        dirty->g_x, dirty->g_y, dirty->g_w, dirty->g_h);

    if (_aes.menu_visible != 0 && _aes.menu_tree != NULL) {
        bar = _aes.menu_tree[ROOT].ob_head;
        if (bar != NIL &&
            _aes_menu_subtree_rect(_aes.menu_tree, bar, &menu_rect) != 0 &&
            _aes_rects_intersect(&menu_rect, dirty) != 0) {
            redraw_menu = 1;
        }
    }

    _aes_desktop_rect_local(&desktop);
    if (_aes_rects_intersect(&desktop, dirty) == 0) {
        if (redraw_menu != 0) {
            _aes_menu_redraw_tree(_aes.menu_tree);
        }
        return;
    }

    clip[0] = _aes_max_word(desktop.g_x, dirty->g_x);
    clip[1] = _aes_max_word(desktop.g_y, dirty->g_y);
    clip[2] = _aes_min_word((WORD) (desktop.g_x + desktop.g_w - 1),
        (WORD) (dirty->g_x + dirty->g_w - 1));
    clip[3] = _aes_min_word((WORD) (desktop.g_y + desktop.g_h - 1),
        (WORD) (dirty->g_y + dirty->g_h - 1));

    ++_aes.update_depth;
    _vdi_begin_update();
    vs_clip(_aes.vdi_handle, 1, clip);
    _aes_fill_checker_rect(clip[0], clip[1], clip[2], clip[3]);
    /*
     * z_order only ever increases -- every window open and every
     * window topped hands out a fresh value that's never reused or
     * compacted, so next_window_z keeps growing for the life of the
     * process. Scanning every z-value up to it (as opposed to just
     * the handful of windows that actually exist) made each redraw
     * cost grow with the total number of window-topping operations
     * ever performed in the session, not with how many windows are
     * actually open -- redraws would get slower and slower the
     * longer a session ran. Collect the (at most AES_MAX_WINDOWS)
     * windows that actually need drawing and sort just those instead.
     */
    {
        aes_window_t *painted[AES_MAX_WINDOWS];
        WORD painted_count = 0;
        WORD a;
        WORD b;

        for (a = 0; a < (WORD) AES_MAX_WINDOWS; ++a) {
            GRECT cover_rect;

            _aes_window_draw_cover_rect(&_aes.windows[a], &cover_rect);
            if (_aes.windows[a].used != 0 && _aes.windows[a].open != 0 &&
                _aes_rects_intersect(&cover_rect, dirty) != 0) {
                painted[painted_count++] = &_aes.windows[a];
            }
        }
        for (a = 1; a < painted_count; ++a) {
            aes_window_t *key = painted[a];
            uint32_t key_z = key->z_order;

            b = (WORD) (a - 1);
            while (b >= 0 && painted[b]->z_order > key_z) {
                painted[(size_t) b + 1] = painted[b];
                --b;
            }
            painted[(size_t) b + 1] = key;
        }
        for (a = 0; a < painted_count; ++a) {
            _aes_trace("redraw_region draw handle=%d z=%lu outer=%d,%d %dx%d",
                painted[a]->handle, (unsigned long) painted[a]->z_order,
                painted[a]->outer.g_x, painted[a]->outer.g_y,
                painted[a]->outer.g_w, painted[a]->outer.g_h);
            _aes_draw_window_frame(painted[a]);
            _aes_queue_window_redraw(painted[a], dirty);
        }
    }
    vs_clip(_aes.vdi_handle, 0, clip);
    --_aes.update_depth;
    _vdi_end_update();

    _aes_queue_desktop_redraw(dirty);

    if (redraw_menu != 0) {
        _aes_menu_redraw_tree(_aes.menu_tree);
    }
}

static void _aes_queue_desktop_redraw(const GRECT *dirty)
{
    GRECT desktop;
    GRECT redraw;
    WORD msg[8];

    if (dirty == NULL || _aes.desktop_owner_app_id == 0) {
        return;
    }

    _aes_desktop_rect_local(&desktop);
    if (_aes_intersect_rects(&desktop, dirty, &redraw) == 0) {
        return;
    }

    msg[0] = WM_REDRAW;
    msg[1] = _aes.current_app_id;
    msg[2] = 0;
    msg[3] = 0;
    msg[4] = redraw.g_x;
    msg[5] = redraw.g_y;
    msg[6] = redraw.g_w;
    msg[7] = redraw.g_h;
    (void) appl_write(_aes.desktop_owner_app_id, 8, msg);
}

static void _aes_queue_window_redraw(const aes_window_t *window,
                                     const GRECT *dirty)
{
    GRECT redraw;
    WORD msg[8];

    if (window == NULL || dirty == NULL || window->owner == 0 ||
        window->work.g_w <= 0 || window->work.g_h <= 0) {
        return;
    }

    if (_aes_intersect_rects(&window->work, dirty, &redraw) == 0) {
        _aes_trace("queue_redraw skip handle=%d dirty=%d,%d %dx%d work=%d,%d %dx%d",
            window->handle, dirty->g_x, dirty->g_y, dirty->g_w, dirty->g_h,
            window->work.g_x, window->work.g_y, window->work.g_w,
            window->work.g_h);
        return;
    }

    /*
     * This used to subtract every higher window's outer rect from
     * `redraw` and post one WM_REDRAW per resulting fragment (up to
     * 64 messages for one damaged window). That precision is never
     * actually needed: every client here re-derives the true
     * occlusion-aware visible-rect list itself via
     * wind_get(WF_FIRSTXYWH/WF_NEXTXYWH) before drawing, so the rect
     * in this message is only a "something changed here" hint, safe
     * to be coarser than exact. Posting one message per fragment
     * instead just multiplied traffic through the shared, fixed-size
     * (AES_MAX_MESSAGES) message queue -- easy to exhaust with only a
     * few overlapping windows, silently dropping later messages
     * (appl_write returns failure but every caller here discards it)
     * and leaving whichever window's redraw got dropped stuck showing
     * stale pixels until something else happens to repaint it.
     */
    msg[0] = WM_REDRAW;
    msg[1] = _aes.current_app_id;
    msg[2] = 0;
    msg[3] = window->handle;
    msg[4] = redraw.g_x;
    msg[5] = redraw.g_y;
    msg[6] = redraw.g_w;
    msg[7] = redraw.g_h;
    _aes_trace("queue_redraw handle=%d owner=%d dirty=%d,%d %dx%d redraw=%d,%d %dx%d",
        window->handle, window->owner, dirty->g_x, dirty->g_y, dirty->g_w,
        dirty->g_h, redraw.g_x, redraw.g_y, redraw.g_w, redraw.g_h);
    (void) appl_write(window->owner, 8, msg);
}

void _aes_redraw_window_change(const GRECT *before, const GRECT *after)
{
    GRECT expanded_before;
    GRECT expanded_after;
    GRECT exposed[4];
    WORD exposed_count;
    WORD i;

    _aes_expand_window_damage_rect(before, &expanded_before);
    _aes_expand_window_damage_rect(after, &expanded_after);
    exposed_count = _aes_subtract_rect(&expanded_before, &expanded_after,
        exposed);
    _aes_trace("window_change before=%d,%d %dx%d after=%d,%d %dx%d exposed=%d",
        expanded_before.g_x, expanded_before.g_y, expanded_before.g_w,
        expanded_before.g_h, expanded_after.g_x, expanded_after.g_y,
        expanded_after.g_w, expanded_after.g_h, exposed_count);

    for (i = 0; i < exposed_count; ++i) {
        _aes_trace("window_change exposed[%d]=%d,%d %dx%d", i,
            exposed[i].g_x, exposed[i].g_y, exposed[i].g_w, exposed[i].g_h);
        _aes_redraw_region(&exposed[i]);
    }
    _aes_trace("window_change redraw_after=%d,%d %dx%d",
        expanded_after.g_x, expanded_after.g_y, expanded_after.g_w,
        expanded_after.g_h);
    _aes_redraw_region(&expanded_after);
}

void _aes_redraw_window_title_states(const aes_window_t *previous_top,
                                     const aes_window_t *new_top)
{
    GRECT dirty;

    if (previous_top != NULL && previous_top->open != 0 &&
        previous_top->used != 0) {
        dirty.g_x = previous_top->outer.g_x;
        dirty.g_y = previous_top->outer.g_y;
        dirty.g_w = (WORD) (previous_top->outer.g_w + 1);
        dirty.g_h = (WORD) (previous_top->work.g_y -
            previous_top->outer.g_y + 1);
        if (dirty.g_w > 0 && dirty.g_h > 0) {
            _aes_redraw_region(&dirty);
        }
    }

    if (new_top != NULL && new_top->open != 0 && new_top->used != 0 &&
        new_top != previous_top) {
        _aes_menu_switch_to_app(new_top->owner);
        dirty.g_x = new_top->outer.g_x;
        dirty.g_y = new_top->outer.g_y;
        dirty.g_w = (WORD) (new_top->outer.g_w + 1);
        dirty.g_h = (WORD) (new_top->work.g_y - new_top->outer.g_y + 1);
        if (dirty.g_w > 0 && dirty.g_h > 0) {
            _aes_redraw_region(&dirty);
        }
    }
}

static void _aes_fill_rect(WORD x0, WORD y0, WORD x1, WORD y1, WORD color)
{
    WORD rect[4];

    if (x0 > x1 || y0 > y1) {
        return;
    }

    rect[0] = x0;
    rect[1] = y0;
    rect[2] = x1;
    rect[3] = y1;
    vsf_color(_aes.vdi_handle, color);
    v_bar(_aes.vdi_handle, rect);
}

static void _aes_draw_rect_edges(WORD x0, WORD y0, WORD x1, WORD y1,
                                 int draw_top,
                                 int draw_right,
                                 int draw_bottom,
                                 int draw_left)
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
        v_pline(_aes.vdi_handle, 2, line);
    }
    if (draw_right != 0) {
        line[0] = x1;
        line[1] = y0;
        line[2] = x1;
        line[3] = y1;
        v_pline(_aes.vdi_handle, 2, line);
    }
    if (draw_bottom != 0) {
        line[0] = x0;
        line[1] = y1;
        line[2] = x1;
        line[3] = y1;
        v_pline(_aes.vdi_handle, 2, line);
    }
    if (draw_left != 0) {
        line[0] = x0;
        line[1] = y0;
        line[2] = x0;
        line[3] = y1;
        v_pline(_aes.vdi_handle, 2, line);
    }
}

static void _aes_invert_rect(WORD x0, WORD y0, WORD x1, WORD y1)
{
    WORD x;
    WORD y;

    if (x0 > x1 || y0 > y1) {
        return;
    }

    _vdi_prepare_screen_write();
    for (y = y0; y <= y1; ++y) {
        for (x = x0; x <= x1; ++x) {
            _vdi_set_screen_pixel(x, y,
                (WORD) (_vdi_get_screen_pixel(x, y) == 0 ? 1 : 0));
        }
    }
}

static void _aes_fill_pattern_rect(WORD x0, WORD y0, WORD x1, WORD y1,
                                   const uint8_t *rows, size_t row_count)
{
    WORD x;
    WORD y;
    WORD dark_pixel = (_aes_dark_color() == WHITE) ? 1 : 0;
    uint8_t row_bits;

    if (x0 > x1 || y0 > y1 || rows == NULL || row_count == 0u) {
        return;
    }

    _aes_fill_rect(x0, y0, x1, y1, _aes_light_color());
    for (y = y0; y <= y1; ++y) {
        row_bits = rows[(size_t) (y % (WORD) row_count)];
        for (x = x0; x <= x1; ++x) {
            if ((row_bits & (uint8_t) (0x80u >> (x & 7))) != 0u) {
                _vdi_plot_pixel(x, y, dark_pixel);
            }
        }
    }
}

static void _aes_fill_checker_rect(WORD x0, WORD y0, WORD x1, WORD y1)
{
    static const uint8_t desktop_rows[] = {
        0xaa, 0x55
    };

    if (x0 > x1 || y0 > y1) {
        return;
    }

    _aes_fill_pattern_rect(x0, y0, x1, y1, desktop_rows, sizeof(desktop_rows));
}

int _aes_window_is_top(const aes_window_t *window)
{
    size_t i;
    uint32_t top_z = 0u;

    if (window == NULL || window->used == 0 || window->open == 0) {
        return 0;
    }

    for (i = 0; i < AES_MAX_WINDOWS; ++i) {
        if (_aes.windows[i].used != 0 && _aes.windows[i].open != 0 &&
            _aes.windows[i].z_order > top_z) {
            top_z = _aes.windows[i].z_order;
        }
    }

    return window->z_order == top_z;
}

static void _aes_draw_window_icon(WORD x0, WORD y0, WORD x1, WORD y1,
                                  char glyph)
{
    _aes_draw_window_icon_offset(x0, y0, x1, y1, glyph, 0, 0);
}

static void _aes_draw_window_icon_offset(WORD x0, WORD y0, WORD x1, WORD y1,
                                         char glyph, WORD dx, WORD dy)
{
    char text[2];
    WORD text_attrib[6];
    WORD previous_font = 0;
    WORD system_font = 1;
    WORD text_width;
    WORD text_height;
    WORD text_x;
    WORD text_y;

    if (x0 > x1 || y0 > y1) {
        return;
    }

    if (vqt_attributes(_aes.vdi_handle, text_attrib) != 0) {
        previous_font = text_attrib[0];
    }
    if (previous_font != 0 && previous_font != system_font) {
        (void) vst_font(_aes.vdi_handle, system_font);
    }

    text[0] = glyph;
    text[1] = '\0';
    text_width = (WORD) _vdi_string_width(text);
    text_height = _vdi_font_text_height();
    text_x = (WORD) (x0 + ((x1 - x0 + 1) - text_width) / 2 + dx);
    text_y = (WORD) (y0 + ((y1 - y0 + 1) - text_height) / 2 +
        _vdi_font_ascent() + dy);
    _aes_draw_text(text_x, text_y, _aes_dark_color(), text);

    if (previous_font != 0 && previous_font != system_font) {
        (void) vst_font(_aes.vdi_handle, previous_font);
    }
}

static void _aes_draw_sizer_glyph(const aes_window_t *window)
{
    GRECT rect;
    WORD x1;
    WORD y1;

    if (window == NULL || _aes_window_sizer_rect(window, &rect) == 0) {
        return;
    }

    x1 = (WORD) (rect.g_x + rect.g_w - 2);
    y1 = (WORD) (rect.g_y + rect.g_h - 2);
    _aes_fill_rect(rect.g_x, rect.g_y, x1, y1, _aes_light_color());
    _aes_draw_window_icon(rect.g_x, rect.g_y, x1, y1, 6);
}

static void _aes_draw_window_title(const aes_window_t *window,
                                   const WORD outer_box[4],
                                   WORD title_height)
{
    static const uint8_t title_rows[] = {
        0x00, 0x55, 0x00, 0x55
    };
    GRECT closer_rect;
    GRECT fuller_rect;
    WORD text_attrib[6];
    WORD previous_font = 0;
    WORD system_font = 1;
    WORD title_sep[4];
    WORD title_left;
    WORD title_right;
    WORD title_box_left = 0;
    WORD title_box_right = -1;
    WORD title_text_left;
    WORD title_text_right;
    WORD title_top;
    WORD title_bottom;
    WORD text_height;
    WORD title_y;
    WORD title_width;
    WORD available_width;
    WORD title_x;
    int has_closer;
    int has_fuller;
    int active_title;

    if (window == NULL || title_height <= 0) {
        return;
    }

    if (vqt_attributes(_aes.vdi_handle, text_attrib) != 0) {
        previous_font = text_attrib[0];
    }
    if (previous_font != 0 && previous_font != system_font) {
        (void) vst_font(_aes.vdi_handle, system_font);
    }

    text_height = _vdi_font_text_height();
    active_title = _aes_window_is_top(window);
    has_closer = _aes_window_closer_rect(window, &closer_rect);
    has_fuller = _aes_window_fuller_rect(window, &fuller_rect);
    title_top = (WORD) (outer_box[1] + 1);
    title_bottom = (WORD) (window->work.g_y - 2);
    title_left = (WORD) (outer_box[0] + 1);
    title_right = (WORD) (outer_box[2] - 1);

    title_sep[0] = outer_box[0];
    title_sep[1] = (WORD) (window->work.g_y - 1);
    title_sep[2] = outer_box[2];
    title_sep[3] = title_sep[1];
    vsl_color(_aes.vdi_handle, _aes_dark_color());
    v_pline(_aes.vdi_handle, 2, title_sep);

    if (has_closer != 0) {
        WORD close_sep[4];
        WORD close_right = (WORD) (closer_rect.g_x + closer_rect.g_w - 2);

        _aes_fill_rect(closer_rect.g_x, closer_rect.g_y,
            close_right,
            (WORD) (closer_rect.g_y + closer_rect.g_h - 1),
            _aes_light_color());
        close_sep[0] = close_right;
        close_sep[1] = closer_rect.g_y;
        close_sep[2] = close_sep[0];
        close_sep[3] = (WORD) (closer_rect.g_y + closer_rect.g_h - 1);
        vsl_color(_aes.vdi_handle, _aes_dark_color());
        v_pline(_aes.vdi_handle, 2, close_sep);
        _aes_draw_window_icon_offset(closer_rect.g_x, closer_rect.g_y,
            close_right,
            (WORD) (closer_rect.g_y + closer_rect.g_h - 1), 5, 0, 0);
        title_left = (WORD) (close_sep[0] + 1);
    }

    if (has_fuller != 0) {
        WORD fuller_sep[4];
        WORD fuller_left = (WORD) (fuller_rect.g_x + 1);
        WORD fuller_right = (WORD) (fuller_rect.g_x + fuller_rect.g_w - 1);

        _aes_fill_rect(fuller_left, fuller_rect.g_y,
            fuller_right,
            (WORD) (fuller_rect.g_y + fuller_rect.g_h - 1),
            _aes_light_color());
        fuller_sep[0] = fuller_left;
        fuller_sep[1] = fuller_rect.g_y;
        fuller_sep[2] = fuller_sep[0];
        fuller_sep[3] = (WORD) (fuller_rect.g_y + fuller_rect.g_h - 1);
        vsl_color(_aes.vdi_handle, _aes_dark_color());
        v_pline(_aes.vdi_handle, 2, fuller_sep);
        _aes_draw_window_icon_offset(fuller_left, fuller_rect.g_y,
            fuller_right,
            (WORD) (fuller_rect.g_y + fuller_rect.g_h - 1), 7, 2, 0);
        title_right = (WORD) (fuller_sep[0] - 1);
    }

    if ((window->kind & NAME) != 0u && window->name[0] != '\0') {
        title_width = (WORD) _vdi_string_width(window->name);
        available_width = (WORD) (title_right - title_left + 1);
        if (available_width <= title_width) {
            title_x = title_left;
        } else {
            title_x = (WORD) (title_left +
                (available_width - title_width) / 2);
        }

        title_text_left = (WORD) (title_x - 2);
        title_text_right = (WORD) (title_x + title_width + 1);
        if (title_text_left < title_left) {
            title_text_left = title_left;
        }
        if (title_text_right > title_right) {
            title_text_right = title_right;
        }
        title_box_left = title_text_left;
        title_box_right = title_text_right;
    }

    if (title_left <= title_right && title_top <= title_bottom) {
        if (active_title != 0) {
            if (title_box_left <= title_box_right) {
                WORD left_fill_right = (WORD) (title_box_left - 1);
                WORD right_fill_left = (WORD) (title_box_right + 1);

                if (title_left <= left_fill_right) {
                    _aes_fill_pattern_rect(title_left, title_top,
                        left_fill_right, title_bottom, title_rows,
                        sizeof(title_rows));
                }
                if (right_fill_left <= title_right) {
                    _aes_fill_pattern_rect(right_fill_left, title_top,
                        title_right, title_bottom, title_rows,
                        sizeof(title_rows));
                }
            } else {
                _aes_fill_pattern_rect(title_left, title_top, title_right,
                    title_bottom, title_rows,
                    sizeof(title_rows));
            }
        } else {
            _aes_fill_rect(title_left, title_top, title_right, title_bottom,
                _aes_light_color());
        }
    }

    if (title_box_left <= title_box_right) {
        _aes_fill_rect(title_box_left, title_top, title_box_right,
            title_bottom, _aes_light_color());
        title_y = (WORD) (title_top +
            (title_bottom - title_top - text_height) / 2 +
            _vdi_font_ascent() + 1);
        _aes_draw_text(title_x, title_y, _aes_dark_color(), window->name);
    }

    if (previous_font != 0 && previous_font != system_font) {
        (void) vst_font(_aes.vdi_handle, previous_font);
    }
}

void _aes_draw_window_frame(const aes_window_t *window)
{
    static const uint8_t scrollbar_rows[] = {
        0x88, 0x22
    };
    WORD outer_box[10];
    WORD left_fill[4];
    WORD right_fill[4];
    WORD bottom_fill[4];
    WORD gutter_x = 0;
    WORD gutter_y = 0;
    WORD gutter_right = 0;
    WORD gutter_bottom = 0;
    WORD gutter_fill[4];
    WORD vertical_sep[4];
    WORD horizontal_sep[4];
    WORD shadow_bottom[4];
    WORD shadow_right[4];
    WORD work_box[10];

    if (_aes.vdi_ready == 0 || window == NULL || window->open == 0) {
        return;
    }

    _aes_trace("draw_window_frame handle=%d outer=%d,%d %dx%d work=%d,%d %dx%d",
        window->handle, window->outer.g_x, window->outer.g_y,
        window->outer.g_w, window->outer.g_h, window->work.g_x,
        window->work.g_y, window->work.g_w, window->work.g_h);

    outer_box[0] = window->outer.g_x;
    outer_box[1] = window->outer.g_y;
    outer_box[2] = (WORD) (window->outer.g_x + window->outer.g_w - 1);
    outer_box[3] = outer_box[1];
    outer_box[4] = outer_box[2];
    outer_box[5] = (WORD) (window->outer.g_y + window->outer.g_h - 1);
    outer_box[6] = outer_box[0];
    outer_box[7] = outer_box[5];
    outer_box[8] = outer_box[0];
    outer_box[9] = outer_box[1];

    work_box[0] = window->work.g_x;
    work_box[1] = window->work.g_y;
    work_box[2] = (WORD) (window->work.g_x + window->work.g_w - 1);
    work_box[3] = work_box[1];
    work_box[4] = work_box[2];
    work_box[5] = (WORD) (window->work.g_y + window->work.g_h - 1);
    work_box[6] = work_box[0];
    work_box[7] = work_box[5];
    work_box[8] = work_box[0];
    work_box[9] = work_box[1];

    vsl_color(_aes.vdi_handle, WHITE);
    v_pline(_aes.vdi_handle, 5, outer_box);
    shadow_bottom[0] = (WORD) (window->outer.g_x + 1);
    shadow_bottom[1] = (WORD) (window->outer.g_y + window->outer.g_h);
    shadow_bottom[2] = (WORD) (window->outer.g_x + window->outer.g_w);
    shadow_bottom[3] = shadow_bottom[1];
    shadow_right[0] = (WORD) (window->outer.g_x + window->outer.g_w);
    shadow_right[1] = (WORD) (window->outer.g_y + 1);
    shadow_right[2] = shadow_right[0];
    shadow_right[3] = (WORD) (window->outer.g_y + window->outer.g_h);
    v_pline(_aes.vdi_handle, 2, shadow_bottom);
    v_pline(_aes.vdi_handle, 2, shadow_right);

    left_fill[0] = (WORD) (window->outer.g_x + 1);
    left_fill[1] = window->work.g_y;
    left_fill[2] = (WORD) (window->work.g_x - 1);
    left_fill[3] = (WORD) (window->outer.g_y + window->outer.g_h - 2);
    if (left_fill[0] <= left_fill[2] && left_fill[1] <= left_fill[3]) {
        _aes_fill_rect(left_fill[0], left_fill[1], left_fill[2], left_fill[3],
            _aes_light_color());
    }

    right_fill[0] = (WORD) (window->work.g_x + window->work.g_w);
    right_fill[1] = window->work.g_y;
    right_fill[2] = (WORD) (window->outer.g_x + window->outer.g_w - 2);
    right_fill[3] = (WORD) (window->outer.g_y + window->outer.g_h - 2);
    if (right_fill[0] <= right_fill[2] && right_fill[1] <= right_fill[3]) {
        _aes_fill_rect(right_fill[0], right_fill[1], right_fill[2],
            right_fill[3], _aes_light_color());
    }

    bottom_fill[0] = (WORD) (window->outer.g_x + 1);
    bottom_fill[1] = (WORD) (window->work.g_y + window->work.g_h);
    bottom_fill[2] = (WORD) (window->outer.g_x + window->outer.g_w - 2);
    bottom_fill[3] = (WORD) (window->outer.g_y + window->outer.g_h - 2);
    if (bottom_fill[0] <= bottom_fill[2] && bottom_fill[1] <= bottom_fill[3]) {
        _aes_fill_rect(bottom_fill[0], bottom_fill[1], bottom_fill[2],
            bottom_fill[3], _aes_light_color());
    }

    if ((window->kind & SIZER) != 0u) {
        gutter_x = (WORD) (window->outer.g_x + window->outer.g_w -
            _aes_window_right_border(window));
        gutter_y = (WORD) (window->outer.g_y + window->outer.g_h -
            _aes_window_bottom_border(window));
        gutter_right = (WORD) (window->outer.g_x + window->outer.g_w - 2);
        gutter_bottom = (WORD) (window->outer.g_y + window->outer.g_h - 2);

        gutter_fill[0] = gutter_x;
        gutter_fill[1] = window->work.g_y;
        gutter_fill[2] = gutter_right;
        gutter_fill[3] = gutter_bottom;
        _aes_fill_rect(gutter_fill[0], gutter_fill[1], gutter_fill[2],
            gutter_fill[3], _aes_light_color());

        gutter_fill[0] = window->outer.g_x + 1;
        gutter_fill[1] = gutter_y;
        gutter_fill[2] = gutter_right;
        gutter_fill[3] = gutter_bottom;
        _aes_fill_rect(gutter_fill[0], gutter_fill[1], gutter_fill[2],
            gutter_fill[3], _aes_light_color());

        vertical_sep[0] = gutter_x;
        vertical_sep[1] = window->outer.g_y + 1;
        vertical_sep[2] = vertical_sep[0];
        vertical_sep[3] = (WORD) (window->outer.g_y + window->outer.g_h - 1);
        horizontal_sep[0] = window->outer.g_x + 1;
        horizontal_sep[1] = gutter_y;
        horizontal_sep[2] = (WORD) (window->outer.g_x + window->outer.g_w - 1);
        horizontal_sep[3] = horizontal_sep[1];
        vsl_color(_aes.vdi_handle, WHITE);
        v_pline(_aes.vdi_handle, 2, vertical_sep);
        v_pline(_aes.vdi_handle, 2, horizontal_sep);
    }

    if (window->work.g_w > 0 && window->work.g_h > 0 &&
        (window->kind & SIZER) == 0u) {
        v_pline(_aes.vdi_handle, 5, work_box);
    }
    _aes_draw_window_title(window, outer_box,
        _aes_window_title_height(window));
    if (_aes_window_has_vscroll(window) != 0) {
        GRECT track;
        GRECT slot;
        GRECT thumb;
        GRECT button;
        WORD right;
        WORD bottom;

        if (_aes_window_vtrack_rect(window, &track) != 0) {
            right = (WORD) (track.g_x + track.g_w - 1);
            bottom = (WORD) (track.g_y + track.g_h - 1);
            _aes_fill_pattern_rect(track.g_x, track.g_y, right, bottom,
                scrollbar_rows, sizeof(scrollbar_rows));
            vsl_color(_aes.vdi_handle, _aes_dark_color());
            _aes_draw_rect_edges(track.g_x, track.g_y, right, bottom,
                1, 0, 1, 1);
        }
        if (_aes_window_vup_rect(window, &button) != 0) {
            right = (WORD) (button.g_x + button.g_w - 1);
            bottom = (WORD) (button.g_y + button.g_h - 1);
            _aes_fill_rect(button.g_x, button.g_y, right, bottom,
                _aes_light_color());
            vsl_color(_aes.vdi_handle, _aes_dark_color());
            _aes_draw_rect_edges(button.g_x, button.g_y, right, bottom,
                0, 0, 1, 1);
            _aes_draw_window_icon_offset(button.g_x, button.g_y, right,
                bottom, 1, 0, -1);
        }
        if (_aes_window_vdown_rect(window, &button) != 0) {
            right = (WORD) (button.g_x + button.g_w - 1);
            bottom = (WORD) (button.g_y + button.g_h - 1);
            _aes_fill_rect(button.g_x, button.g_y, right, bottom,
                _aes_light_color());
            vsl_color(_aes.vdi_handle, _aes_dark_color());
            _aes_draw_rect_edges(button.g_x, button.g_y, right, bottom,
                1, 0, 0, 1);
            _aes_draw_window_icon(button.g_x, button.g_y, right, bottom, 2);
        }
        if (_aes_window_vthumb_rect(window, &thumb) != 0) {
            int draw_top = 1;
            int draw_bottom = 1;

            right = (WORD) (thumb.g_x + thumb.g_w - 1);
            bottom = (WORD) (thumb.g_y + thumb.g_h - 1);
            if (_aes_window_vslot_rect(window, &slot) != 0) {
                if (thumb.g_y <= slot.g_y) {
                    draw_top = 0;
                }
                if (bottom >= slot.g_y + slot.g_h - 1) {
                    draw_bottom = 0;
                }
            }
            _aes_fill_rect(thumb.g_x, thumb.g_y, right, bottom,
                _aes_light_color());
            vsl_color(_aes.vdi_handle, _aes_dark_color());
            _aes_draw_rect_edges(thumb.g_x, thumb.g_y, right, bottom,
                draw_top, 0, draw_bottom, 1);
        }
    }
    if (_aes_window_has_hscroll(window) != 0) {
        GRECT track;
        GRECT slot;
        GRECT thumb;
        GRECT button;
        WORD right;
        WORD bottom;

        if (_aes_window_htrack_rect(window, &track) != 0) {
            right = (WORD) (track.g_x + track.g_w - 1);
            bottom = (WORD) (track.g_y + track.g_h - 1);
            _aes_fill_pattern_rect(track.g_x, track.g_y, right, bottom,
                scrollbar_rows, sizeof(scrollbar_rows));
            vsl_color(_aes.vdi_handle, _aes_dark_color());
            _aes_draw_rect_edges(track.g_x, track.g_y, right, bottom,
                1, 1, 0, 1);
        }
        if (_aes_window_hleft_rect(window, &button) != 0) {
            right = (WORD) (button.g_x + button.g_w - 1);
            bottom = (WORD) (button.g_y + button.g_h - 1);
            _aes_fill_rect(button.g_x, button.g_y, right, bottom,
                _aes_light_color());
            vsl_color(_aes.vdi_handle, _aes_dark_color());
            _aes_draw_rect_edges(button.g_x, button.g_y, right, bottom,
                1, 1, 0, 0);
            _aes_draw_window_icon_offset(button.g_x, button.g_y, right,
                bottom, 4, 0, 2);
        }
        if (_aes_window_hright_rect(window, &button) != 0) {
            right = (WORD) (button.g_x + button.g_w - 1);
            bottom = (WORD) (button.g_y + button.g_h - 1);
            _aes_fill_rect(button.g_x, button.g_y, right, bottom,
                _aes_light_color());
            vsl_color(_aes.vdi_handle, _aes_dark_color());
            _aes_draw_rect_edges(button.g_x, button.g_y, right, bottom,
                1, 0, 0, 1);
            _aes_draw_window_icon_offset(button.g_x, button.g_y, right,
                bottom, 3, 0, 2);
        }
        if (_aes_window_hthumb_rect(window, &thumb) != 0) {
            int draw_left = 1;
            int draw_right = 1;

            right = (WORD) (thumb.g_x + thumb.g_w - 1);
            bottom = (WORD) (thumb.g_y + thumb.g_h - 1);
            if (_aes_window_hslot_rect(window, &slot) != 0) {
                if (thumb.g_x <= slot.g_x) {
                    draw_left = 0;
                }
                if (right >= slot.g_x + slot.g_w - 1) {
                    draw_right = 0;
                }
            }
            _aes_fill_rect(thumb.g_x, thumb.g_y, right, bottom,
                _aes_light_color());
            vsl_color(_aes.vdi_handle, _aes_dark_color());
            _aes_draw_rect_edges(thumb.g_x, thumb.g_y, right, bottom,
                1, draw_right, 0, draw_left);
        }
    }
    _aes_draw_sizer_glyph(window);
    _aes_present_after_window_draw();
}

void _aes_clear_window_frame(const aes_window_t *window)
{
    WORD rect[4];

    if (_aes.vdi_ready == 0 || window == NULL) {
        return;
    }

    rect[0] = window->outer.g_x;
    rect[1] = window->outer.g_y;
    rect[2] = (WORD) (window->outer.g_x + window->outer.g_w - 1);
    rect[3] = (WORD) (window->outer.g_y + window->outer.g_h - 1);
    vsf_color(_aes.vdi_handle, BLACK);
    v_bar(_aes.vdi_handle, rect);
    _aes_present_after_window_draw();
}

void _aes_redraw_open_windows(void)
{
    GRECT desktop;

    _aes_desktop_rect_local(&desktop);
    _aes_redraw_region(&desktop);
}

void _aes_raise_window(aes_window_t *window)
{
    if (window == NULL) {
        return;
    }

    window->z_order = _aes.next_window_z++;
}

WORD _aes_window_hit_part(const aes_window_t *window, WORD x, WORD y)
{
    GRECT rect;
    GRECT thumb;

    if (window == NULL || window->open == 0 ||
        !_aes_point_in_rect(x, y, &window->outer)) {
        return AES_WINDOW_PART_NONE;
    }

    if (_aes_window_closer_rect(window, &rect) != 0 &&
        _aes_point_in_rect(x, y, &rect)) {
        return AES_WINDOW_PART_CLOSER;
    }
    if (_aes_window_fuller_rect(window, &rect) != 0 &&
        _aes_point_in_rect(x, y, &rect)) {
        return AES_WINDOW_PART_FULLER;
    }
    if (_aes_window_sizer_rect(window, &rect) != 0 &&
        _aes_point_in_rect(x, y, &rect)) {
        return AES_WINDOW_PART_SIZER;
    }
    if (_aes_window_vup_rect(window, &rect) != 0 &&
        _aes_point_in_rect(x, y, &rect)) {
        return AES_WINDOW_PART_VUP;
    }
    if (_aes_window_vdown_rect(window, &rect) != 0 &&
        _aes_point_in_rect(x, y, &rect)) {
        return AES_WINDOW_PART_VDOWN;
    }
    if (_aes_window_hleft_rect(window, &rect) != 0 &&
        _aes_point_in_rect(x, y, &rect)) {
        return AES_WINDOW_PART_HLEFT;
    }
    if (_aes_window_hright_rect(window, &rect) != 0 &&
        _aes_point_in_rect(x, y, &rect)) {
        return AES_WINDOW_PART_HRIGHT;
    }
    if (_aes_window_vthumb_rect(window, &thumb) != 0 &&
        _aes_point_in_rect(x, y, &thumb)) {
        return AES_WINDOW_PART_VSLIDE;
    }
    if (_aes_window_hthumb_rect(window, &thumb) != 0 &&
        _aes_point_in_rect(x, y, &thumb)) {
        return AES_WINDOW_PART_HSLIDE;
    }
    if (_aes_window_vtrack_rect(window, &rect) != 0 &&
        _aes_point_in_rect(x, y, &rect)) {
        if (_aes_window_vthumb_rect(window, &thumb) != 0 && y < thumb.g_y) {
            return AES_WINDOW_PART_VPAGE_UP;
        }
        return AES_WINDOW_PART_VPAGE_DOWN;
    }
    if (_aes_window_htrack_rect(window, &rect) != 0 &&
        _aes_point_in_rect(x, y, &rect)) {
        if (_aes_window_hthumb_rect(window, &thumb) != 0 && x < thumb.g_x) {
            return AES_WINDOW_PART_HPAGE_LEFT;
        }
        return AES_WINDOW_PART_HPAGE_RIGHT;
    }
    if (_aes_window_title_rect(window, &rect) != 0 &&
        _aes_point_in_rect(x, y, &rect)) {
        return AES_WINDOW_PART_TITLE;
    }
    if (_aes_point_in_rect(x, y, &window->work)) {
        return AES_WINDOW_PART_WORK;
    }
    return AES_WINDOW_PART_NONE;
}

void _aes_object_extent(OBJECT *tree, WORD object, WORD *x, WORD *y)
{
    WORD abs_x = 0;
    WORD abs_y = 0;
    WORD current = object;

    if (tree == NULL || object < 0) {
        if (x != NULL) {
            *x = 0;
        }
        if (y != NULL) {
            *y = 0;
        }
        return;
    }

    while (current != NIL) {
        abs_x = (WORD) (abs_x + tree[current].ob_x);
        abs_y = (WORD) (abs_y + tree[current].ob_y);
        current = _aes_find_parent(tree, current);
    }

    if (x != NULL) {
        *x = abs_x;
    }
    if (y != NULL) {
        *y = abs_y;
    }
}

static void _aes_draw_text(WORD x, WORD y, WORD color, const char *text)
{
    if (text == NULL || *text == '\0') {
        _aes_draw_trace("text skip x=%d y=%d color=%d text=%p",
            x, y, color, (const void *) text);
        return;
    }

    _aes_draw_trace("text x=%d y=%d color=%d \"%s\"",
        x, y, color, text);
    vst_color(_aes.vdi_handle, color);
    v_gtext(_aes.vdi_handle, x, y, (CONST BYTE *) text);
}

static void _aes_stipple_text_pixels(WORD x, WORD y, WORD foreground,
                                     WORD background, const char *text)
{
    WORD width;
    WORD height;
    WORD top;
    WORD left;
    WORD px;
    WORD py;

    if (text == NULL || *text == '\0') {
        return;
    }

    width = (WORD) _vdi_string_width(text);
    height = _vdi_font_text_height();
    left = x;
    top = (WORD) (y - _vdi_font_ascent());
    if (width <= 0 || height <= 0) {
        return;
    }

    _vdi_prepare_screen_write();
    for (py = top; py < top + height; ++py) {
        for (px = left; px < left + width; ++px) {
            if (((px + py) & 1) != 0 &&
                _vdi_get_screen_pixel(px, py) == foreground) {
                _vdi_set_screen_pixel(px, py, background);
            }
        }
    }
}

static void _aes_draw_trace(const char *fmt, ...)
{
    const char *trace;
    FILE *fp;
    va_list ap;

    trace = getenv("GEM_TRACE_DRAW");
    if (trace == NULL || trace[0] == '\0') {
        return;
    }

    fp = fopen("/tmp/gem_draw_trace.log", "a");
    if (fp == NULL) {
        return;
    }

    va_start(ap, fmt);
    vfprintf(fp, fmt, ap);
    va_end(ap);
    fputc('\n', fp);
    fclose(fp);
}

static void _aes_draw_ted_object(const OBJECT *tree,
                                 WORD object,
                                 const OBJECT *obj,
                                 const TEDINFO *ted,
                                 const WORD rect[4])
{
    const char *text;
    WORD text_x;
    WORD text_y;
    WORD text_width;
    WORD caret_width;
    WORD caret_top;
    WORD caret_bottom;
    WORD caret_x;
    WORD text_color;

    if (obj == NULL || ted == NULL) {
        return;
    }

    text = (const char *) (intptr_t) ted->te_ptext;
    text_x = (WORD) (rect[0] + 2);
    text_color = _aes_dark_color();
    text_width = (WORD) _vdi_string_width(text != NULL ? text : "");
    caret_width = text_width;

    if (obj->ob_type == G_FTEXT || obj->ob_type == G_FBOXTEXT) {
        text_y = (WORD) (rect[1] +
            ((rect[3] - rect[1] + 1 - _vdi_font_text_height()) > 0 ?
            (rect[3] - rect[1] + 1 - _vdi_font_text_height()) / 2 : 0) +
            _vdi_font_ascent());
        caret_top = (WORD) (text_y - _vdi_font_ascent() + 1);
        caret_bottom = (WORD) (caret_top + _vdi_font_text_height() - 2);
        if (obj->ob_type == G_FBOXTEXT) {
            _aes_fill_rect(rect[0], rect[1], rect[2], rect[3],
                _aes_light_color());
            _aes_draw_hline(rect[0], rect[2], rect[1], _aes_dark_color());
            _aes_draw_hline(rect[0], rect[2], rect[3], _aes_dark_color());
            _aes_draw_vline(rect[0], rect[1], rect[3], _aes_dark_color());
            _aes_draw_vline(rect[2], rect[1], rect[3], _aes_dark_color());
            text_x = (WORD) (rect[0] + 3);
            if (caret_top < rect[1] + 1) {
                caret_top = (WORD) (rect[1] + 1);
            }
            if (caret_bottom > rect[3] - 1) {
                caret_bottom = (WORD) (rect[3] - 1);
            }
        } else {
            _aes_fill_rect(rect[0], rect[1], rect[2], rect[3],
                _aes_light_color());
            if (caret_top < rect[1]) {
                caret_top = rect[1];
            }
            if (caret_bottom > rect[3]) {
                caret_bottom = rect[3];
            }
        }
        if (tree == _aes.edit_tree && object == _aes.edit_object &&
            text != NULL) {
            WORD edit_index = _aes.edit_index;
            WORD text_length = (WORD) strlen(text);
            char prefix[256];

            if (edit_index < 0) {
                edit_index = 0;
            }
            if (edit_index > text_length) {
                edit_index = text_length;
            }
            if ((size_t) edit_index >= sizeof(prefix)) {
                edit_index = (WORD) (sizeof(prefix) - 1u);
            }
            memcpy(prefix, text, (size_t) edit_index);
            prefix[edit_index] = '\0';
            caret_width = (WORD) _vdi_string_width(prefix);
        }
        if ((obj->ob_state & SELECTED) != 0u) {
            caret_x = (WORD) (text_x + caret_width + 1);
            if (caret_x < rect[0] + 1) {
                caret_x = (WORD) (rect[0] + 1);
            }
            if (caret_x > rect[2] - 1) {
                caret_x = (WORD) (rect[2] - 1);
            }
            _aes_draw_vline(caret_x, caret_top, caret_bottom,
                _aes_dark_color());
        }
    } else if (obj->ob_type == G_BOXTEXT) {
        text_y = (WORD) (rect[1] +
            ((rect[3] - rect[1] + 1 - _vdi_font_text_height()) > 0 ?
            (rect[3] - rect[1] + 1 - _vdi_font_text_height()) / 2 : 0) +
            _vdi_font_ascent());
        _aes_fill_rect(rect[0], rect[1], rect[2], rect[3], _aes_light_color());
        _aes_draw_hline(rect[0], rect[2], rect[1], _aes_dark_color());
        _aes_draw_hline(rect[0], rect[2], rect[3], _aes_dark_color());
        _aes_draw_vline(rect[0], rect[1], rect[3], _aes_dark_color());
        _aes_draw_vline(rect[2], rect[1], rect[3], _aes_dark_color());
    } else {
        text_y = (WORD) (rect[1] +
            ((rect[3] - rect[1] + 1 - _vdi_font_text_height()) > 0 ?
            (rect[3] - rect[1] + 1 - _vdi_font_text_height()) / 2 : 0) +
            _vdi_font_ascent());
    }

    if (ted->te_just == TE_RIGHT) {
        text_x = (WORD) (rect[2] - text_width - 1);
        if (text_x < rect[0] + 2) {
            text_x = (WORD) (rect[0] + 2);
        }
    } else if (ted->te_just == TE_CNTR) {
        text_x = (WORD) (rect[0] + ((rect[2] - rect[0] + 1) - text_width) / 2);
        if (text_x < rect[0] + 2) {
            text_x = (WORD) (rect[0] + 2);
        }
    }

    _aes_draw_text(text_x, text_y, text_color, text);
}

static void _aes_draw_hline(WORD x0, WORD x1, WORD y, WORD color)
{
    WORD pts[4];

    if (x0 > x1) {
        return;
    }

    pts[0] = x0;
    pts[1] = y;
    pts[2] = x1;
    pts[3] = y;
    vsl_color(_aes.vdi_handle, color);
    v_pline(_aes.vdi_handle, 2, pts);
}

static void _aes_draw_vline(WORD x, WORD y0, WORD y1, WORD color)
{
    WORD pts[4];

    if (y0 > y1) {
        return;
    }

    pts[0] = x;
    pts[1] = y0;
    pts[2] = x;
    pts[3] = y1;
    vsl_color(_aes.vdi_handle, color);
    v_pline(_aes.vdi_handle, 2, pts);
}

static void _aes_draw_dialog_frame(const WORD rect[4])
{
    WORD inset;
    WORD left;
    WORD top;
    WORD right;
    WORD bottom;

    for (inset = 0; inset < 5; ++inset) {
        WORD color = (inset == 0 || inset >= 3) ?
            _aes_dark_color() : _aes_light_color();

        left = (WORD) (rect[0] + inset);
        top = (WORD) (rect[1] + inset);
        right = (WORD) (rect[2] - inset);
        bottom = (WORD) (rect[3] - inset);
        if (left > right || top > bottom) {
            break;
        }

        _aes_draw_hline(left, right, top, color);
        _aes_draw_hline(left, right, bottom, color);
        _aes_draw_vline(left, top, bottom, color);
        _aes_draw_vline(right, top, bottom, color);
    }
}

static void _aes_draw_button_frame(const WORD rect[4], WORD dark_color,
                                   WORD light_color, int default_button)
{
    WORD inset;
    WORD border_thickness = default_button != 0 ? 2 : 1;

    (void) light_color;

    for (inset = 0; inset < border_thickness; ++inset) {
        WORD left = (WORD) (rect[0] + inset);
        WORD top = (WORD) (rect[1] + inset);
        WORD right = (WORD) (rect[2] - inset);
        WORD bottom = (WORD) (rect[3] - inset);

        if (left > right || top > bottom) {
            break;
        }

        _aes_draw_hline(left, right, top, dark_color);
        _aes_draw_hline(left, right, bottom, dark_color);
        _aes_draw_vline(left, top, bottom, dark_color);
        _aes_draw_vline(right, top, bottom, dark_color);
    }
}

static void _aes_draw_icon_object(const OBJECT *obj,
                                  const ICONBLK *icon,
                                  WORD abs_x,
                                  WORD abs_y,
                                  const WORD rect[4])
{
    const char *text;
    WORD icon_box[4];
    WORD text_x;
    WORD text_y;

    if (obj == NULL || icon == NULL) {
        return;
    }

    icon_box[0] = (WORD) (abs_x + icon->ib_xicon);
    icon_box[1] = (WORD) (abs_y + icon->ib_yicon);
    icon_box[2] = (WORD) (icon_box[0] + icon->ib_wicon - 1);
    icon_box[3] = (WORD) (icon_box[1] + icon->ib_hicon - 1);

    if ((obj->ob_state & SELECTED) != 0u) {
        vsf_color(_aes.vdi_handle, _aes_light_color());
        v_bar(_aes.vdi_handle, rect);
    }

    vsl_color(_aes.vdi_handle, _aes_dark_color());
    v_rbox(_aes.vdi_handle, icon_box);

    text = (const char *) (intptr_t) icon->ib_ptext;
    text_x = (WORD) (abs_x + icon->ib_xtext);
    text_y = (WORD) (abs_y + icon->ib_ytext + AES_CHAR_HEIGHT);
    _aes_draw_trace("icon obj=%p rect=%d,%d-%d,%d icon=%d,%d %dx%d text=%p",
        (const void *) obj, rect[0], rect[1], rect[2], rect[3],
        icon_box[0], icon_box[1], icon->ib_wicon, icon->ib_hicon,
        (const void *) text);
    _aes_draw_text(text_x, text_y, _aes_dark_color(), text);
}

static void _aes_draw_user_object(const OBJECT *tree,
                                  WORD object,
                                  const OBJECT *obj,
                                  WORD abs_x,
                                  WORD abs_y,
                                  WORD clip[4])
{
    USERBLK *user;
    PARMBLK parm;
    aes_user_draw_t draw;

    if (tree == NULL || obj == NULL) {
        return;
    }

    user = (USERBLK *) (intptr_t) _aes_resolve_spec(obj);
    if (user == NULL || user->ab_code == 0) {
        return;
    }

    memset(&parm, 0, sizeof(parm));
    parm.pb_tree = (LONG) (intptr_t) tree;
    parm.pb_obj = object;
    parm.pb_prevstate = obj->ob_state;
    parm.pb_currstate = obj->ob_state;
    parm.pb_x = abs_x;
    parm.pb_y = abs_y;
    parm.pb_w = obj->ob_width;
    parm.pb_h = obj->ob_height;
    parm.pb_xc = clip[0];
    parm.pb_yc = clip[1];
    parm.pb_wc = (WORD) (clip[2] - clip[0] + 1);
    parm.pb_hc = (WORD) (clip[3] - clip[1] + 1);
    parm.pb_parm = user->ab_parm;

    draw = (aes_user_draw_t) (intptr_t) user->ab_code;
    (void) draw((LONG) (intptr_t) &parm);
}

static void _aes_draw_object(const OBJECT *tree,
                             WORD object,
                             WORD abs_x,
                             WORD abs_y,
                             WORD clip[4])
{
    char shortcut_label[128];
    char shortcut_text[64];
    const OBJECT *obj;
    LONG spec;
    WORD rect[4];
    WORD parent;
    WORD active;
    WORD fill_color;
    WORD border_color;
    WORD inner_border_color;
    WORD text_width;
    WORD text_height;
    WORD text_ascent;
    WORD text_x;
    WORD text_y;
    WORD text_color;
    WORD text_background;
    int disabled_text;
    int checked_menu_item;

    if (_aes.vdi_ready == 0 || tree == NULL || object < 0) {
        return;
    }

    obj = &tree[object];
    parent = _aes_find_parent((OBJECT *) tree, object);
    spec = _aes_resolve_spec(obj);
    checked_menu_item = (obj->ob_type == G_STRING &&
        (obj->ob_state & CHECKED) != 0u) ? 1 : 0;
    active = ((obj->ob_state & SELECTED) != 0u) ? 1 : 0;
    if (obj->ob_type == G_BUTTON && (obj->ob_state & CHECKED) != 0u) {
        active = 1;
    }
    fill_color = (active != 0) ?
        _aes_dark_color() : _aes_light_color();
    border_color = _aes_dark_color();
    inner_border_color = (active != 0) ?
        _aes_light_color() : _aes_dark_color();
    rect[0] = abs_x;
    rect[1] = abs_y;
    rect[2] = (WORD) (rect[0] + obj->ob_width - 1);
    rect[3] = (WORD) (rect[1] + obj->ob_height - 1);

    switch (obj->ob_type) {
    case G_BOX:
    case G_BOXCHAR:
        if (object == ROOT && _aes_is_window_work_root(tree) != 0) {
            vsf_color(_aes.vdi_handle, _aes_light_color());
            v_bar(_aes.vdi_handle, rect);
        } else if (object == ROOT && _aes_is_dialog_root_tree(tree) != 0) {
            vsf_color(_aes.vdi_handle, _aes_light_color());
            v_bar(_aes.vdi_handle, rect);
            _aes_draw_dialog_frame(rect);
        } else if (_aes_is_menu_bar_object(tree, object) != 0) {
            vsf_color(_aes.vdi_handle, _aes_light_color());
            v_bar(_aes.vdi_handle, rect);
            _aes_draw_hline(rect[0], rect[2], rect[3], border_color);
        } else if (object == ROOT && rect[0] == 0 &&
            rect[2] >= _aes.work_out[0] &&
            rect[3] >= (WORD) (_aes.work_out[1] / 2)) {
            _aes_fill_checker_rect(rect[0], rect[1], rect[2], rect[3]);
        } else {
            vsf_color(_aes.vdi_handle, fill_color);
            v_bar(_aes.vdi_handle, rect);
        }
        if (object != ROOT) {
            if (_aes_is_menu_bar_object(tree, object) == 0) {
                if (_aes_is_dialog_frame_object(tree, object, parent) != 0) {
                    _aes_draw_dialog_frame(rect);
                } else {
                    _aes_draw_hline(rect[0], rect[2], rect[1], border_color);
                    _aes_draw_hline(rect[0], rect[2], rect[3], border_color);
                    _aes_draw_vline(rect[0], rect[1], rect[3], border_color);
                    _aes_draw_vline(rect[2], rect[1], rect[3], border_color);
                }
            }
        }
        break;
    case G_IBOX:
        if (object == ROOT && _aes_is_window_work_root(tree) != 0) {
            vsf_color(_aes.vdi_handle, _aes_light_color());
            v_bar(_aes.vdi_handle, rect);
        } else if (_aes_is_dialog_frame_object(tree, object, parent) != 0) {
            vsf_color(_aes.vdi_handle, _aes_light_color());
            v_bar(_aes.vdi_handle, rect);
            _aes_draw_dialog_frame(rect);
        }
        break;
    case G_BUTTON:
        vsf_color(_aes.vdi_handle, fill_color);
        v_bar(_aes.vdi_handle, rect);
        _aes_draw_button_frame(rect, border_color, inner_border_color,
            (obj->ob_flags & DEFAULT) != 0u ? 1 : 0);
        break;
    case G_TEXT:
    case G_BOXTEXT:
    case G_FTEXT:
    case G_FBOXTEXT:
        _aes_draw_ted_object(tree, object, obj,
            (const TEDINFO *) (intptr_t) spec, rect);
        return;
    case G_ICON:
        _aes_draw_icon_object(obj, (const ICONBLK *) (intptr_t) spec,
            abs_x, abs_y, rect);
        return;
    case G_USERDEF:
        _aes_draw_user_object(tree, object, obj, abs_x, abs_y, clip);
        return;
    default:
        break;
    }

    _aes_draw_trace("obj type=%u rect=%d,%d-%d,%d spec=%p flags=%u state=%u",
        obj->ob_type, rect[0], rect[1], rect[2], rect[3],
        (const void *) (intptr_t) spec, obj->ob_flags, obj->ob_state);

    if (obj->ob_type == G_TITLE) {
        if (rect[1] + 1 <= rect[3] - 1) {
            _aes_fill_rect(rect[0], (WORD) (rect[1] + 1), rect[2],
                (WORD) (rect[3] - 1), _aes_light_color());
        }
    } else if (obj->ob_type == G_STRING && tree == _aes.menu_tree &&
        parent != NIL &&
        (tree[parent].ob_type == G_BOX || tree[parent].ob_type == G_IBOX) &&
        _aes_is_menu_bar_object(tree, parent) == 0) {
        WORD fill_right = rect[2];
        WORD parent_x = 0;
        WORD parent_y = 0;

        _aes_object_extent((OBJECT *) tree, parent, &parent_x, &parent_y);
        (void) parent_y;
        fill_right = (WORD) (parent_x + tree[parent].ob_width - 2);

        if (fill_right < rect[0]) {
            fill_right = rect[0];
        }
        _aes_fill_rect(rect[0], rect[1], fill_right, rect[3],
            (active != 0) ? _aes_dark_color() : _aes_light_color());
    } else if ((obj->ob_type == G_STRING || obj->ob_type == G_TITLE) &&
        active != 0) {
        vsf_color(_aes.vdi_handle, fill_color);
        v_bar(_aes.vdi_handle, rect);
    }

    text_width = 0;
    text_height = _vdi_font_text_height();
    text_ascent = _vdi_font_ascent();
    text_x = (WORD) (rect[0] + 2);
    if (obj->ob_type == G_TITLE) {
        text_y = (WORD) (rect[1] + 3 + text_ascent);
    } else {
        text_y = (WORD) (rect[1] +
            ((obj->ob_height - text_height) > 0 ?
            (obj->ob_height - text_height) / 2 : 0) + text_ascent);
    }
    if (obj->ob_type == G_TITLE) {
        text_color = _aes_dark_color();
    } else {
        text_color = (active != 0) ?
            _aes_light_color() : _aes_dark_color();
    }
    text_background = (active != 0) ?
        _aes_dark_color() : _aes_light_color();
    disabled_text = ((obj->ob_state & DISABLED) != 0u) ? 1 : 0;
    if ((obj->ob_type == G_STRING || obj->ob_type == G_TITLE ||
         obj->ob_type == G_BUTTON) && spec != 0) {
        const char *text = (const char *) (intptr_t) spec;
        int has_shortcut = 0;

        if (obj->ob_type == G_STRING && _aes_menu_is_separator_text(text)) {
            WORD sep_left = (WORD) (rect[0] + 2);
            WORD sep_right = (WORD) (rect[2] - 2);
            WORD sep_y = (WORD) (rect[1] + obj->ob_height / 2);

            if (sep_right < sep_left) {
                sep_right = sep_left;
            }
            if (sep_y > rect[3]) {
                sep_y = rect[3];
            }
            if (sep_y > rect[1]) {
                _aes_draw_hline(sep_left, sep_right, (WORD) (sep_y - 1),
                    _aes_light_color());
            }
            _aes_draw_hline(sep_left, sep_right, sep_y, _aes_dark_color());
            return;
        }

        if (obj->ob_type == G_STRING) {
            has_shortcut = _aes_menu_split_shortcut(text, shortcut_label,
                sizeof(shortcut_label), shortcut_text,
                sizeof(shortcut_text));
            if (has_shortcut != 0) {
                text = shortcut_label;
            }
        }

        text_width = (WORD) _vdi_string_width(text);
        if ((obj->ob_type == G_BUTTON || obj->ob_type == G_TITLE) &&
            text_width <= obj->ob_width) {
            text_x = (WORD) (rect[0] + (obj->ob_width - text_width) / 2);
        }
        if (checked_menu_item != 0) {
            _aes_draw_text((WORD) (rect[0] + 2), text_y, text_color, "\010");
        }
        _aes_draw_text(text_x, text_y, text_color, text);
        if (disabled_text != 0) {
            _aes_stipple_text_pixels(text_x, text_y, text_color,
                text_background, text);
        }
        if (has_shortcut != 0 && shortcut_text[0] != '\0') {
            WORD shortcut_width = (WORD) _vdi_string_width(shortcut_text);
            WORD shortcut_x = (WORD) (rect[2] - shortcut_width - 2);

            if (shortcut_x > text_x) {
                _aes_draw_text(shortcut_x, text_y, text_color,
                    shortcut_text);
                if (disabled_text != 0) {
                    _aes_stipple_text_pixels(shortcut_x, text_y, text_color,
                        text_background, shortcut_text);
                }
            }
        }
        if (obj->ob_type == G_TITLE && active != 0 &&
            rect[1] + 1 <= rect[3] - 1) {
            _aes_invert_rect(rect[0], (WORD) (rect[1] + 1), rect[2],
                (WORD) (rect[3] - 1));
        }
    }
}

void _aes_draw_tree_recursive(const OBJECT *tree,
                              WORD object,
                              WORD parent_x,
                              WORD parent_y,
                              WORD depth,
                              WORD clip[4])
{
    WORD abs_x;
    WORD abs_y;
    WORD child_clip[4];
    WORD clip_on = 0;

    if (tree == NULL || object < 0) {
        return;
    }
    if ((tree[object].ob_flags & HIDETREE) != 0u) {
        return;
    }

    abs_x = (WORD) (parent_x + tree[object].ob_x);
    abs_y = (WORD) (parent_y + tree[object].ob_y);
    if (clip != NULL) {
        vs_clip(_aes.vdi_handle, 1, clip);
        clip_on = 1;
    }
    _aes_draw_object(tree, object, abs_x, abs_y, clip);
    if (clip_on != 0) {
        vs_clip(_aes.vdi_handle, 0, clip);
    }

    if (depth == 0 || tree[object].ob_head == NIL) {
        return;
    }

    {
        WORD child = tree[object].ob_head;

        memcpy(child_clip, clip, sizeof(child_clip));
        if (object == ROOT && _aes_is_dialog_root_tree(tree) != 0) {
            WORD inner_clip[4];

            inner_clip[0] = (WORD) (abs_x + 5);
            inner_clip[1] = (WORD) (abs_y + 5);
            inner_clip[2] = (WORD) (abs_x + tree[object].ob_width - 7);
            inner_clip[3] = (WORD) (abs_y + tree[object].ob_height - 7);
            if (inner_clip[0] <= inner_clip[2] &&
                inner_clip[1] <= inner_clip[3]) {
                child_clip[0] = _aes_max_word(child_clip[0], inner_clip[0]);
                child_clip[1] = _aes_max_word(child_clip[1], inner_clip[1]);
                child_clip[2] = _aes_min_word(child_clip[2], inner_clip[2]);
                child_clip[3] = _aes_min_word(child_clip[3], inner_clip[3]);
            }
        }

        while (child != NIL) {
            WORD next = tree[child].ob_next;
            WORD *next_clip = child_clip;

            if (object == ROOT && _aes_is_dialog_frame_object(tree, child,
                    object) != 0) {
                next_clip = clip;
            }

            _aes_draw_tree_recursive(tree, child, abs_x, abs_y,
                (depth > 0) ? (WORD) (depth - 1) : depth, next_clip);
            if (child == tree[object].ob_tail || next == object ||
                next == NIL) {
                break;
            }
            child = next;
        }
    }
}

WORD _aes_find_in_subtree(OBJECT *tree,
                          WORD object,
                          WORD parent_x,
                          WORD parent_y,
                          WORD depth,
                          WORD mx,
                          WORD my)
{
    WORD abs_x;
    WORD abs_y;
    WORD hit = NIL;

    if (tree == NULL || object < 0) {
        return NIL;
    }

    abs_x = (WORD) (parent_x + tree[object].ob_x);
    abs_y = (WORD) (parent_y + tree[object].ob_y);

    if (depth != 0 && tree[object].ob_head != NIL &&
        (tree[object].ob_flags & HIDETREE) == 0u) {
        WORD child = tree[object].ob_head;

        while (child != NIL) {
            WORD next = tree[child].ob_next;
            WORD child_hit = _aes_find_in_subtree(tree, child, abs_x, abs_y,
                (depth > 0) ? (WORD) (depth - 1) : depth, mx, my);

            if (child_hit != NIL) {
                hit = child_hit;
            }
            if (child == tree[object].ob_tail || next == object ||
                next == NIL) {
                break;
            }
            child = next;
        }
    }

    if (mx >= abs_x && my >= abs_y &&
        mx < abs_x + tree[object].ob_width &&
        my < abs_y + tree[object].ob_height &&
        (tree[object].ob_flags & HIDETREE) == 0u) {
        if (hit == NIL) {
            hit = object;
        }
    }

    return hit;
}
