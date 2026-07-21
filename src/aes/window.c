/*
 * Implements hosted AES window management entry points and
 * visible-region enumeration helpers.
 *
 * MIT License (see: LICENSE)
 * Copyright (C) 2026 tomaz stih
 */

#include "_aes.h"

#include <string.h>

aes_window_t *_aes_find_top_window(void);
static WORD _aes_build_visible_rects(WORD handle, GRECT out[], WORD max_rects);
static void _aes_window_cover_rect(const aes_window_t *window, GRECT *rect);
static void _aes_union_rects(const GRECT *left, const GRECT *right, GRECT *out);
static void _aes_redraw_scrollbar_delta(const aes_window_t *before,
    const aes_window_t *after, WORD field);
WORD wind_create(UWORD kind, WORD x, WORD y, WORD w, WORD h);
WORD wind_open(WORD handle, WORD x, WORD y, WORD w, WORD h);
WORD wind_close(WORD handle);
WORD wind_delete(WORD handle);
WORD wind_get(WORD handle, WORD field, WORD *w1, WORD *w2, WORD *w3, WORD *w4);
WORD wind_set(WORD handle, WORD field, WORD w1, WORD w2, WORD w3, WORD w4);
WORD wind_set_str(WORD handle, WORD field, const char *text);
WORD wind_find(WORD x, WORD y);
WORD wind_update(WORD flag);
WORD wind_calc(WORD type,
               UWORD kind,
               WORD inx,
               WORD iny,
               WORD inw,
               WORD inh,
               WORD *outx,
               WORD *outy,
               WORD *outw,
               WORD *outh);

aes_window_t *_aes_find_top_window(void)
{
    aes_window_t *best = NULL;
    size_t i;

    for (i = 0; i < AES_MAX_WINDOWS; ++i) {
        aes_window_t *window = &_aes.windows[i];

        if (window->used == 0 || window->open == 0) {
            continue;
        }
        if (best == NULL || window->z_order > best->z_order) {
            best = window;
        }
    }

    return best;
}

static void _aes_window_cover_rect(const aes_window_t *window, GRECT *rect)
{
    if (rect == NULL) {
        return;
    }

    if (window == NULL || window->used == 0 || window->open == 0 ||
        window->outer.g_w <= 0 || window->outer.g_h <= 0) {
        _aes_set_rect(rect, 0, 0, 0, 0);
        return;
    }

    /*
     * Hosted AES draws a one-pixel shadow on the right and bottom
     * edges, so visible-region enumeration must treat that shadow as
     * occupied by the covering window too.
     */
    _aes_set_rect(rect, window->outer.g_x, window->outer.g_y,
        (WORD) (window->outer.g_w + 1), (WORD) (window->outer.g_h + 1));
}

static WORD _aes_build_visible_rects(WORD handle, GRECT out[], WORD max_rects)
{
    GRECT pending[64];
    GRECT next_pending[64];
    GRECT base;
    aes_window_t *target = NULL;
    WORD pending_count = 0;
    size_t i;

    if (out == NULL || max_rects <= 0) {
        return 0;
    }

    if (handle == 0) {
        _aes_desktop_rect(&base);
    } else {
        aes_window_t *window = _aes_find_window(handle);

        if (window == NULL || window->open == 0 || window->work.g_w <= 0 ||
            window->work.g_h <= 0) {
            return 0;
        }
        target = window;
        base = window->work;
    }

    if (base.g_w <= 0 || base.g_h <= 0) {
        return 0;
    }

    _aes_trace("visible_rects begin handle=%d base=%d,%d %dx%d target_z=%lu",
        handle, base.g_x, base.g_y, base.g_w, base.g_h,
        (unsigned long) ((target != NULL) ? target->z_order : 0u));

    pending[0] = base;
    pending_count = 1;

    for (i = 0; i < AES_MAX_WINDOWS && pending_count > 0; ++i) {
        const aes_window_t *cover = &_aes.windows[i];
        GRECT cover_rect;
        WORD next_count = 0;
        WORD j;

        if (cover->used == 0 || cover->open == 0) {
            continue;
        }
        if (target != NULL &&
            (cover->handle == handle || cover->z_order <= target->z_order)) {
            continue;
        }
        _aes_window_cover_rect(cover, &cover_rect);
        if (cover_rect.g_w <= 0 || cover_rect.g_h <= 0) {
            continue;
        }
        _aes_trace("visible_rects cover handle=%d z=%lu rect=%d,%d %dx%d pending=%d",
            cover->handle, (unsigned long) cover->z_order, cover_rect.g_x,
            cover_rect.g_y, cover_rect.g_w, cover_rect.g_h, pending_count);

        for (j = 0; j < pending_count; ++j) {
            GRECT fragments[4];
            WORD fragment_count = _aes_subtract_rect(&pending[j],
                &cover_rect, fragments);
            WORD k;

            for (k = 0; k < fragment_count && next_count < max_rects; ++k) {
                next_pending[next_count++] = fragments[k];
            }
        }

        memcpy(pending, next_pending, (size_t) next_count * sizeof(GRECT));
        pending_count = next_count;
    }

    for (i = 0; i < (size_t) pending_count; ++i) {
        _aes_trace("visible_rects out[%lu]=%d,%d %dx%d",
            (unsigned long) i, pending[i].g_x, pending[i].g_y,
            pending[i].g_w, pending[i].g_h);
    }
    _aes_trace("visible_rects end handle=%d count=%d", handle, pending_count);
    memcpy(out, pending, (size_t) pending_count * sizeof(GRECT));
    return pending_count;
}

static void _aes_union_rects(const GRECT *left, const GRECT *right, GRECT *out)
{
    WORD x0;
    WORD y0;
    WORD x1;
    WORD y1;

    if (left == NULL || right == NULL || out == NULL) {
        return;
    }

    x0 = _aes_min_word(left->g_x, right->g_x);
    y0 = _aes_min_word(left->g_y, right->g_y);
    x1 = _aes_max_word((WORD) (left->g_x + left->g_w - 1),
        (WORD) (right->g_x + right->g_w - 1));
    y1 = _aes_max_word((WORD) (left->g_y + left->g_h - 1),
        (WORD) (right->g_y + right->g_h - 1));
    _aes_set_rect(out, x0, y0, (WORD) (x1 - x0 + 1), (WORD) (y1 - y0 + 1));
}

static void _aes_redraw_scrollbar_delta(const aes_window_t *before,
    const aes_window_t *after, WORD field)
{
    GRECT old_rect;
    GRECT new_rect;
    GRECT dirty;
    int have_old = 0;
    int have_new = 0;

    if (before == NULL || after == NULL || after->open == 0) {
        return;
    }

    switch (field) {
    case WF_VSLIDE:
    case WF_VSLSIZ:
        have_old = _aes_window_vthumb_rect(before, &old_rect);
        have_new = _aes_window_vthumb_rect(after, &new_rect);
        break;
    case WF_HSLIDE:
    case WF_HSLSIZ:
        have_old = _aes_window_hthumb_rect(before, &old_rect);
        have_new = _aes_window_hthumb_rect(after, &new_rect);
        break;
    default:
        break;
    }

    if (have_old != 0 && have_new != 0) {
        if (old_rect.g_x == new_rect.g_x &&
            old_rect.g_y == new_rect.g_y &&
            old_rect.g_w == new_rect.g_w &&
            old_rect.g_h == new_rect.g_h) {
            return;
        }
        _aes_union_rects(&old_rect, &new_rect, &dirty);
        _aes_redraw_region(&dirty);
    } else if (have_old != 0) {
        _aes_redraw_region(&old_rect);
    } else if (have_new != 0) {
        _aes_redraw_region(&new_rect);
    }
}

WORD wind_create(UWORD kind, WORD x, WORD y, WORD w, WORD h)
{
    size_t i;

    for (i = 0; i < AES_MAX_WINDOWS; ++i) {
        if (_aes.windows[i].used == 0) {
            _aes.windows[i].used = 1;
            _aes.windows[i].handle = (WORD) (i + 1);
            _aes.windows[i].owner = _aes.current_app_id;
            _aes.windows[i].z_order = _aes.next_window_z++;
            _aes.windows[i].kind = kind;
            _aes_set_rect(&_aes.windows[i].outer, x, y, w, h);
            _aes.windows[i].previous_outer = _aes.windows[i].outer;
            _aes.windows[i].restored_outer = _aes.windows[i].outer;
            _aes.windows[i].hslide = 0;
            _aes.windows[i].vslide = 0;
            _aes.windows[i].hslsize = 1000;
            _aes.windows[i].vslsize = 1000;
            _aes_compute_work(&_aes.windows[i]);
            return _aes.windows[i].handle;
        }
    }
    return 0;
}

WORD wind_open(WORD handle, WORD x, WORD y, WORD w, WORD h)
{
    aes_window_t *window = _aes_find_window(handle);
    aes_window_t *previous_top = NULL;
    GRECT previous_outer;
    int was_open = 0;

    if (window == NULL) {
        return 0;
    }
    was_open = (window->open != 0);
    previous_top = _aes_find_top_window();
    if (was_open != 0) {
        previous_outer = window->outer;
    } else {
        _aes_set_rect(&previous_outer, 0, 0, 0, 0);
        _aes_raise_window(window);
    }
    _aes_set_rect(&window->outer, x, y, w, h);
    window->restored_outer = window->outer;
    window->iconified = 0;
    window->previous_outer = window->outer;
    _aes_compute_work(window);
    window->open = 1;
    _aes_redraw_window_change(&previous_outer, &window->outer);
    if (was_open == 0) {
        _aes_redraw_window_title_states(previous_top, window);
    }
    return 1;
}

WORD wind_close(WORD handle)
{
    aes_window_t *window = _aes_find_window(handle);
    GRECT previous_outer;

    if (window == NULL) {
        return 0;
    }
    previous_outer = window->outer;
    window->open = 0;
    _aes_redraw_window_change(&previous_outer, NULL);
    return 1;
}

WORD wind_delete(WORD handle)
{
    aes_window_t *window = _aes_find_window(handle);

    if (window == NULL) {
        return 0;
    }
    memset(window, 0, sizeof(*window));
    return 1;
}

WORD wind_get(WORD handle, WORD field, WORD *w1, WORD *w2, WORD *w3, WORD *w4)
{
    aes_app_t *app = _aes_find_app_by_id(_aes.current_app_id);
    aes_window_t *window = _aes_find_window(handle);
    const GRECT *rect;
    GRECT rect_value;

    if (handle == 0) {
        WORD box_height = 0;
        WORD menu_height;

        _aes_trace("wind_get desktop field=%d", field);
        if (_aes_ensure_vdi() == 0) {
            return 0;
        }
        _aes_trace("wind_get desktop ptrs %p %p %p %p",
            (void *) w1, (void *) w2, (void *) w3, (void *) w4);

        rect_value.g_x = 0;
        rect_value.g_y = 0;
        rect_value.g_w = (WORD) (_aes.work_out[0] + 1);
        rect_value.g_h = (WORD) (_aes.work_out[1] + 1);

        if (_aes.menu_visible != 0 && _aes.menu_tree != NULL) {
            (void) graf_handle(NULL, NULL, NULL, &box_height);
            menu_height = (box_height > 0) ? box_height :
                _aes_menu_chrome_height();
            if (menu_height <= 0) {
                menu_height = _aes_menu_chrome_height();
            }
        } else {
            menu_height = 0;
        }

        rect_value.g_y = menu_height;
        rect_value.g_h = (WORD) _aes_max_word((WORD) 0,
            (WORD) (rect_value.g_h - menu_height));

        if (field == WF_FIRSTXYWH) {
            if (app != NULL) {
                app->enum_handle = 0;
                app->enum_index = 0;
                app->enum_count = _aes_build_visible_rects(0,
                    app->enum_rects, 64);
                if (app->enum_count > 0) {
                    rect_value = app->enum_rects[0];
                    app->enum_stage = 1;
                } else {
                    app->enum_stage = 0;
                    _aes_set_rect(&rect_value, 0, 0, 0, 0);
                }
            } else {
                _aes_set_rect(&rect_value, 0, 0, 0, 0);
            }
        } else if (field == WF_NEXTXYWH) {
            if (app != NULL &&
                app->enum_handle == 0 &&
                app->enum_stage != 0 &&
                app->enum_index + 1 < app->enum_count) {
                ++app->enum_index;
                rect_value = app->enum_rects[app->enum_index];
            } else {
                if (app != NULL) {
                    app->enum_stage = 0;
                    app->enum_count = 0;
                    app->enum_index = 0;
                }
                _aes_set_rect(&rect_value, 0, 0, 0, 0);
            }
        }

        if (w1 != NULL) {
            *w1 = rect_value.g_x;
        }
        if (w2 != NULL) {
            *w2 = rect_value.g_y;
        }
        if (w3 != NULL) {
            *w3 = rect_value.g_w;
        }
        if (w4 != NULL) {
            *w4 = rect_value.g_h;
        }
        return 1;
    }

    if (window == NULL) {
        return 0;
    }

    if (field == WF_FIRSTXYWH) {
        if (app != NULL) {
            app->enum_handle = handle;
            app->enum_index = 0;
            app->enum_count = _aes_build_visible_rects(handle, app->enum_rects,
                64);
            if (app->enum_count > 0) {
                rect = &app->enum_rects[0];
                app->enum_stage = 1;
            } else {
                rect = &rect_value;
                app->enum_stage = 0;
                _aes_set_rect(&rect_value, 0, 0, 0, 0);
            }
        } else {
            rect = &rect_value;
            _aes_set_rect(&rect_value, 0, 0, 0, 0);
        }
    } else if (field == WF_NEXTXYWH) {
        if (app != NULL &&
            app->enum_handle == handle &&
            app->enum_stage != 0 &&
            app->enum_index + 1 < app->enum_count) {
            ++app->enum_index;
            rect = &app->enum_rects[app->enum_index];
        } else {
            rect = &rect_value;
            if (app != NULL) {
                app->enum_stage = 0;
                app->enum_count = 0;
                app->enum_index = 0;
            }
            _aes_set_rect(&rect_value, 0, 0, 0, 0);
        }
    } else if (field == WF_WXYWH) {
        rect = &window->outer;
    } else if (field == WF_CXYWH) {
        rect = &window->work;
    } else if (field == WF_PXYWH) {
        rect = &window->previous_outer;
    } else if (field == WF_FXYWH) {
        rect_value.g_x = 0;
        rect_value.g_y = _aes_menu_bar_height();
        rect_value.g_w = (WORD) (_aes.work_out[0] + 1);
        rect_value.g_h = (WORD) _aes_max_word((WORD) 0,
            (WORD) (_aes.work_out[1] + 1 - rect_value.g_y));
        rect = &rect_value;
    } else if (field == WF_HSLIDE) {
        rect_value.g_x = window->hslide;
        rect_value.g_y = 0;
        rect_value.g_w = 0;
        rect_value.g_h = 0;
        rect = &rect_value;
    } else if (field == WF_VSLIDE) {
        rect_value.g_x = window->vslide;
        rect_value.g_y = 0;
        rect_value.g_w = 0;
        rect_value.g_h = 0;
        rect = &rect_value;
    } else if (field == WF_HSLSIZ) {
        rect_value.g_x = window->hslsize;
        rect_value.g_y = 0;
        rect_value.g_w = 0;
        rect_value.g_h = 0;
        rect = &rect_value;
    } else if (field == WF_VSLSIZ) {
        rect_value.g_x = window->vslsize;
        rect_value.g_y = 0;
        rect_value.g_w = 0;
        rect_value.g_h = 0;
        rect = &rect_value;
    } else {
        rect = &window->work;
    }

    if (w1 != NULL) {
        *w1 = rect->g_x;
    }
    if (w2 != NULL) {
        *w2 = rect->g_y;
    }
    if (w3 != NULL) {
        *w3 = rect->g_w;
    }
    if (w4 != NULL) {
        *w4 = rect->g_h;
    }
    return 1;
}

WORD wind_set(WORD handle, WORD field, WORD w1, WORD w2, WORD w3, WORD w4)
{
    aes_window_t *window = _aes_find_window(handle);
    aes_window_t before;

    if (window == NULL) {
        return 0;
    }

    before = *window;

    switch (field) {
    case WF_NAME:
        return _aes_wind_set_text(handle, field,
            (const char *) (intptr_t) w1);
    case WF_INFO:
        return _aes_wind_set_text(handle, field,
            (const char *) (intptr_t) w1);
    case WF_TOP:
    {
        aes_window_t *previous_top = _aes_find_top_window();

        if (previous_top == window) {
            break;
        }

        _aes_raise_window(window);
        _aes_redraw_window_change(&window->outer, &window->outer);
        _aes_redraw_window_title_states(previous_top, window);
        break;
    }
    case WF_WXYWH:
    case WF_CXYWH:
        window->previous_outer = window->outer;
        _aes_set_rect(&window->outer, w1, w2, w3, w4);
        if (window->outer.g_x == window->previous_outer.g_x &&
            window->outer.g_y == window->previous_outer.g_y &&
            window->outer.g_w == window->previous_outer.g_w &&
            window->outer.g_h == window->previous_outer.g_h) {
            if (window->iconified != 0) {
                window->restored_outer.g_x = w1;
                window->restored_outer.g_y = w2;
                window->restored_outer.g_w = w3;
            } else {
                window->restored_outer = window->outer;
                window->iconified = 0;
            }
            _aes_compute_work(window);
            break;
        }
        if (window->iconified != 0) {
            window->restored_outer.g_x = w1;
            window->restored_outer.g_y = w2;
            window->restored_outer.g_w = w3;
        } else {
            window->restored_outer = window->outer;
            window->iconified = 0;
        }
        _aes_compute_work(window);
        if (window->open != 0) {
            _aes_redraw_window_change(&window->previous_outer, &window->outer);
        }
        break;
    case WF_HSLIDE:
    {
        WORD value = _aes_max_word(0, _aes_min_word(w1, 1000));
        if (window->hslide == value) {
            break;
        }
        window->hslide = value;
        if (window->open != 0) {
            _aes_redraw_scrollbar_delta(&before, window, field);
        }
        break;
    }
    case WF_VSLIDE:
    {
        WORD value = _aes_max_word(0, _aes_min_word(w1, 1000));
        if (window->vslide == value) {
            break;
        }
        window->vslide = value;
        if (window->open != 0) {
            _aes_redraw_scrollbar_delta(&before, window, field);
        }
        break;
    }
    case WF_HSLSIZ:
    {
        WORD value = _aes_max_word(0, _aes_min_word(w1, 1000));
        if (window->hslsize == value) {
            break;
        }
        window->hslsize = value;
        if (window->open != 0) {
            _aes_redraw_scrollbar_delta(&before, window, field);
        }
        break;
    }
    case WF_VSLSIZ:
    {
        WORD value = _aes_max_word(0, _aes_min_word(w1, 1000));
        if (window->vslsize == value) {
            break;
        }
        window->vslsize = value;
        if (window->open != 0) {
            _aes_redraw_scrollbar_delta(&before, window, field);
        }
        break;
    }
    default:
        break;
    }
    return 1;
}

WORD wind_set_str(WORD handle, WORD field, const char *text)
{
    return _aes_wind_set_text(handle, field, text);
}

WORD wind_find(WORD x, WORD y)
{
    size_t i;
    aes_window_t *best = NULL;

    for (i = 0; i < AES_MAX_WINDOWS; ++i) {
        aes_window_t *window = &_aes.windows[i];

        if (window->used != 0 && window->open != 0 &&
            _aes_point_in_rect(x, y, &window->outer)) {
            if (best == NULL || window->z_order > best->z_order) {
                best = window;
            }
        }
    }
    return (best != NULL) ? best->handle : 0;
}

WORD wind_update(WORD flag)
{
    aes_app_t *app = _aes_find_app_by_id(_aes.current_app_id);

    if (flag == BEG_UPDATE) {
        ++_aes.update_depth;
        if (app != NULL) {
            ++app->update_depth;
        }
        _vdi_begin_update();
    } else if (flag == END_UPDATE && _aes.update_depth > 0) {
        --_aes.update_depth;
        if (app != NULL && app->update_depth > 0) {
            --app->update_depth;
        }
        _vdi_end_update();
    }
    return 1;
}

WORD wind_calc(WORD type,
               UWORD kind,
               WORD inx,
               WORD iny,
               WORD inw,
               WORD inh,
               WORD *outx,
               WORD *outy,
               WORD *outw,
               WORD *outh)
{
    aes_window_t temp_window;
    WORD left_border;
    WORD right_border;
    WORD bottom_border;
    WORD title_height;

    memset(&temp_window, 0, sizeof(temp_window));
    temp_window.kind = kind;
    _aes_set_rect(&temp_window.outer, inx, iny, inw, inh);
    _aes_compute_work(&temp_window);
    left_border = (WORD) (temp_window.work.g_x - temp_window.outer.g_x);
    title_height = (WORD) (temp_window.work.g_y - temp_window.outer.g_y);
    right_border = (WORD) ((temp_window.outer.g_x + temp_window.outer.g_w) -
        (temp_window.work.g_x + temp_window.work.g_w));
    bottom_border = (WORD) ((temp_window.outer.g_y + temp_window.outer.g_h) -
        (temp_window.work.g_y + temp_window.work.g_h));

    if (type == WC_BORDER) {
        if (outx != NULL) {
            *outx = (WORD) (inx - left_border);
        }
        if (outy != NULL) {
            *outy = (WORD) (iny - title_height);
        }
        if (outw != NULL) {
            *outw = (WORD) (inw + left_border + right_border);
        }
        if (outh != NULL) {
            *outh = (WORD) (inh + title_height + bottom_border);
        }
    } else {
        if (outx != NULL) {
            *outx = (WORD) (inx + left_border);
        }
        if (outy != NULL) {
            *outy = (WORD) (iny + title_height);
        }
        if (outw != NULL) {
            *outw = (WORD) (inw - left_border - right_border);
        }
        if (outh != NULL) {
            *outh = (WORD) (inh - title_height - bottom_border);
        }
    }
    return 1;
}
