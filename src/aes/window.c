/*
 * Implements the hosted AES window management entry points: creation,
 * opening, closing, deletion, field queries including visible-rectangle
 * enumeration, field updates, hit testing, update locking and the
 * border/work-area conversion.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (C) 2026 tomaz stih
 */

#include "window_internal.h"

#include <string.h>

WORD wind_create(UWORD kind, WORD x, WORD y, WORD w, WORD h);
WORD wind_open(WORD handle, WORD x, WORD y, WORD w, WORD h);
WORD wind_close(WORD handle);
WORD wind_delete(WORD handle);
WORD wind_get(WORD handle, WORD field, WORD *w1, WORD *w2, WORD *w3, WORD *w4);
WORD wind_set(WORD handle, WORD field, WORD w1, WORD w2, WORD w3, WORD w4);
WORD wind_set_str(WORD handle, WORD field, const char *text);
WORD wind_find(WORD x, WORD y);
WORD wind_update(WORD flag);
WORD wind_calc(WORD type, UWORD kind, WORD inx, WORD iny, WORD inw, WORD inh,
               WORD *outx, WORD *outy, WORD *outw, WORD *outh);

WORD wind_create(UWORD kind, WORD x, WORD y, WORD w, WORD h)
{
    size_t i;

    for (i = 0; i < AES_MAX_WINDOWS; ++i) {
        if (aes_state.windows[i].used == 0) {
            aes_state.windows[i].used = 1;
            aes_state.windows[i].handle = (WORD)(i + 1);
            aes_state.windows[i].owner = aes_state.current_app_id;
            aes_state.windows[i].z_order = aes_state.next_window_z++;
            aes_state.windows[i].kind = kind;
            aes_set_rect(&aes_state.windows[i].outer, x, y, w, h);
            aes_state.windows[i].previous_outer = aes_state.windows[i].outer;
            aes_state.windows[i].restored_outer = aes_state.windows[i].outer;
            aes_state.windows[i].hslide = 0;
            aes_state.windows[i].vslide = 0;
            aes_state.windows[i].hslsize = 1000;
            aes_state.windows[i].vslsize = 1000;
            aes_compute_work(&aes_state.windows[i]);
            return aes_state.windows[i].handle;
        }
    }
    return 0;
}

WORD wind_open(WORD handle, WORD x, WORD y, WORD w, WORD h)
{
    aes_window_t *window = aes_find_window(handle);
    aes_window_t *previous_top = NULL;
    GRECT previous_outer;
    int was_open = 0;

    if (window == NULL) {
        return 0;
    }
    was_open = (window->open != 0);
    previous_top = aes_find_top_window();
    if (was_open != 0) {
        previous_outer = window->outer;
    } else {
        aes_set_rect(&previous_outer, 0, 0, 0, 0);
        aes_raise_window(window);
    }
    aes_set_rect(&window->outer, x, y, w, h);
    window->restored_outer = window->outer;
    window->iconified = 0;
    window->previous_outer = window->outer;
    aes_compute_work(window);
    window->open = 1;
    aes_redraw_window_change(&previous_outer, &window->outer);
    if (was_open == 0) {
        aes_redraw_window_title_states(previous_top, window);
    }
    return 1;
}

WORD wind_close(WORD handle)
{
    aes_window_t *window = aes_find_window(handle);
    aes_window_t *previous_top = aes_find_top_window();
    GRECT previous_outer;

    if (window == NULL) {
        return 0;
    }
    previous_outer = window->outer;
    window->open = 0;
    aes_redraw_window_change(&previous_outer, NULL);
    if (window == previous_top) {
        aes_redraw_window_title_states(NULL, aes_find_top_window());
    }
    return 1;
}

WORD wind_delete(WORD handle)
{
    aes_window_t *window = aes_find_window(handle);

    if (window == NULL) {
        return 0;
    }
    memset(window, 0, sizeof(*window));
    return 1;
}

WORD wind_get(WORD handle, WORD field, WORD *w1, WORD *w2, WORD *w3, WORD *w4)
{
    aes_app_t *app = aes_find_app_by_id(aes_state.current_app_id);
    aes_window_t *window = aes_find_window(handle);
    const GRECT *rect;
    GRECT rect_value;

    if (handle == 0) {
        WORD menu_height;

        aes_trace("wind_get desktop field=%d", field);
        if (aes_ensure_vdi() == 0) {
            return 0;
        }
        aes_trace("wind_get desktop ptrs %p %p %p %p", (void *)w1, (void *)w2,
                  (void *)w3, (void *)w4);

        rect_value.g_x = 0;
        rect_value.g_y = 0;
        rect_value.g_w = (WORD)(aes_state.work_out[0] + 1);
        rect_value.g_h = (WORD)(aes_state.work_out[1] + 1);

        menu_height = aes_menu_bar_height();

        rect_value.g_y = menu_height;
        rect_value.g_h =
            (WORD)aes_max_word((WORD)0, (WORD)(rect_value.g_h - menu_height));

        if (field == WF_FIRSTXYWH) {
            if (app != NULL) {
                app->enum_handle = 0;
                app->enum_index = 0;
                app->enum_count =
                    aes_build_visible_rects(0, app->enum_rects, 64);
                if (app->enum_count > 0) {
                    rect_value = app->enum_rects[0];
                    app->enum_stage = 1;
                } else {
                    app->enum_stage = 0;
                    aes_set_rect(&rect_value, 0, 0, 0, 0);
                }
            } else {
                aes_set_rect(&rect_value, 0, 0, 0, 0);
            }
        } else if (field == WF_NEXTXYWH) {
            if (app != NULL && app->enum_handle == 0 && app->enum_stage != 0 &&
                app->enum_index + 1 < app->enum_count) {
                ++app->enum_index;
                rect_value = app->enum_rects[app->enum_index];
            } else {
                if (app != NULL) {
                    app->enum_stage = 0;
                    app->enum_count = 0;
                    app->enum_index = 0;
                }
                aes_set_rect(&rect_value, 0, 0, 0, 0);
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
            app->enum_count =
                aes_build_visible_rects(handle, app->enum_rects, 64);
            if (app->enum_count > 0) {
                rect = &app->enum_rects[0];
                app->enum_stage = 1;
            } else {
                rect = &rect_value;
                app->enum_stage = 0;
                aes_set_rect(&rect_value, 0, 0, 0, 0);
            }
        } else {
            rect = &rect_value;
            aes_set_rect(&rect_value, 0, 0, 0, 0);
        }
    } else if (field == WF_NEXTXYWH) {
        if (app != NULL && app->enum_handle == handle && app->enum_stage != 0 &&
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
            aes_set_rect(&rect_value, 0, 0, 0, 0);
        }
    } else if (field == WF_KIND) {
        aes_set_rect(&rect_value, (WORD)window->kind, 0, 0, 0);
        rect = &rect_value;
    } else if (field == WF_WXYWH) {
        rect = &window->outer;
    } else if (field == WF_CXYWH) {
        rect = &window->work;
    } else if (field == WF_PXYWH) {
        rect = &window->previous_outer;
    } else if (field == WF_FXYWH) {
        rect_value.g_x = 0;
        rect_value.g_y = aes_menu_bar_height();
        rect_value.g_w = (WORD)(aes_state.work_out[0] + 1);
        rect_value.g_h = (WORD)aes_max_word(
            (WORD)0, (WORD)(aes_state.work_out[1] + 1 - rect_value.g_y));
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
    aes_window_t *window = aes_find_window(handle);
    aes_window_t before;

    if (window == NULL) {
        return 0;
    }

    before = *window;

    switch (field) {
        case WF_NAME:
            return aes_wind_set_text(handle, field, (const char *)(intptr_t)w1);
        case WF_INFO:
            return aes_wind_set_text(handle, field, (const char *)(intptr_t)w1);
        case WF_TOP:
            aes_top_window(window);
            break;
        case WF_CXYWH:
            /* The selector names the work area, as wind_get reports it;
             * derive the outer rectangle so both directions agree. */
            (void)wind_calc(WC_BORDER, window->kind, w1, w2, w3, w4, &w1, &w2,
                            &w3, &w4);
            /* fall through */
        case WF_WXYWH:
            window->previous_outer = window->outer;
            aes_set_rect(&window->outer, w1, w2, w3, w4);
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
                aes_compute_work(window);
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
            aes_compute_work(window);
            if (window->open != 0) {
                aes_redraw_window_change(&window->previous_outer,
                                         &window->outer);
            }
            break;
        case WF_HSLIDE: {
            WORD value = aes_max_word(0, aes_min_word(w1, 1000));
            if (window->hslide == value) {
                break;
            }
            window->hslide = value;
            if (window->open != 0) {
                aes_redraw_scrollbar_delta(&before, window, field);
            }
            break;
        }
        case WF_VSLIDE: {
            WORD value = aes_max_word(0, aes_min_word(w1, 1000));
            if (window->vslide == value) {
                break;
            }
            window->vslide = value;
            if (window->open != 0) {
                aes_redraw_scrollbar_delta(&before, window, field);
            }
            break;
        }
        case WF_HSLSIZ: {
            WORD value = aes_max_word(0, aes_min_word(w1, 1000));
            if (window->hslsize == value) {
                break;
            }
            window->hslsize = value;
            if (window->open != 0) {
                aes_redraw_scrollbar_delta(&before, window, field);
            }
            break;
        }
        case WF_VSLSIZ: {
            WORD value = aes_max_word(0, aes_min_word(w1, 1000));
            if (window->vslsize == value) {
                break;
            }
            window->vslsize = value;
            if (window->open != 0) {
                aes_redraw_scrollbar_delta(&before, window, field);
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
    return aes_wind_set_text(handle, field, text);
}

WORD wind_find(WORD x, WORD y)
{
    size_t i;
    aes_window_t *best = NULL;

    for (i = 0; i < AES_MAX_WINDOWS; ++i) {
        aes_window_t *window = &aes_state.windows[i];

        if (window->used != 0 && window->open != 0 &&
            aes_point_in_rect(x, y, &window->outer)) {
            if (best == NULL || window->z_order > best->z_order) {
                best = window;
            }
        }
    }
    return (best != NULL) ? best->handle : 0;
}

WORD wind_update(WORD flag)
{
    aes_app_t *app = aes_find_app_by_id(aes_state.current_app_id);

    if (flag == BEG_UPDATE) {
        if (aes_state.update_depth >= 32766 ||
            (app && app->update_depth >= 32766))
            return 0;
        ++aes_state.update_depth;
        if (app != NULL) {
            ++app->update_depth;
        }
        vdi_begin_update();
    } else if (flag == END_UPDATE && aes_state.update_depth > 0 &&
               (app == NULL || app->update_depth > 0)) {
        --aes_state.update_depth;
        if (app != NULL && app->update_depth > 0) {
            --app->update_depth;
        }
        vdi_end_update();
    }
    return 1;
}

WORD wind_calc(WORD type, UWORD kind, WORD inx, WORD iny, WORD inw, WORD inh,
               WORD *outx, WORD *outy, WORD *outw, WORD *outh)
{
    aes_window_t temp_window;
    WORD left_border;
    WORD right_border;
    WORD bottom_border;
    WORD title_height;

    memset(&temp_window, 0, sizeof(temp_window));
    temp_window.kind = kind;
    aes_set_rect(&temp_window.outer, inx, iny, inw, inh);
    aes_compute_work(&temp_window);
    left_border = (WORD)(temp_window.work.g_x - temp_window.outer.g_x);
    title_height = (WORD)(temp_window.work.g_y - temp_window.outer.g_y);
    right_border = (WORD)((temp_window.outer.g_x + temp_window.outer.g_w) -
                          (temp_window.work.g_x + temp_window.work.g_w));
    bottom_border = (WORD)((temp_window.outer.g_y + temp_window.outer.g_h) -
                           (temp_window.work.g_y + temp_window.work.g_h));

    if (type == WC_BORDER) {
        if (outx != NULL) {
            *outx = (WORD)(inx - left_border);
        }
        if (outy != NULL) {
            *outy = (WORD)(iny - title_height);
        }
        if (outw != NULL) {
            *outw = (WORD)(inw + left_border + right_border);
        }
        if (outh != NULL) {
            *outh = (WORD)(inh + title_height + bottom_border);
        }
    } else {
        if (outx != NULL) {
            *outx = (WORD)(inx + left_border);
        }
        if (outy != NULL) {
            *outy = (WORD)(iny + title_height);
        }
        if (outw != NULL) {
            *outw = (WORD)(inw - left_border - right_border);
        }
        if (outh != NULL) {
            *outh = (WORD)(inh - title_height - bottom_border);
        }
    }
    return 1;
}
