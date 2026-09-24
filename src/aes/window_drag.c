/*
 * Drags hosted AES windows: the XOR rubber-band outline, the interaction
 * lock that defers other applications' drawing, position clamping that
 * keeps a title strip reachable, and the move and size trackers that
 * commit the new frame and queue WM_MOVED or WM_SIZED.
 *
 * MIT License (see: LICENSE)
 * Copyright (C) 2026 tomaz stih
 */

#include "window_track.h"

#include "platform/os.h"

#include <string.h>

static void aes_draw_drag_dot(WORD x, WORD y)
{
    WORD point[4] = {x, y, x, y};

    v_pline(aes_state.vdi_handle, 2, point);
}

void aes_draw_drag_outline(const GRECT *rect)
{
    WORD previous_mode;
    WORD left;
    WORD top;
    WORD right;
    WORD bottom;
    unsigned int phase = 0u;
    WORD position;

    if (rect == NULL || rect->g_w <= 0 || rect->g_h <= 0 ||
        aes_ensure_vdi() == 0) {
        return;
    }

    left = rect->g_x;
    top = rect->g_y;
    right = (WORD)(left + rect->g_w - 1);
    bottom = (WORD)(top + rect->g_h - 1);

    previous_mode = vdi_write_mode();
    (void)vswr_mode(aes_state.vdi_handle, MD_XOR);
    /*
     * Line color is a VDI color *index* then mapped by vdi_color_to_pixel:
     * WHITE→1, BLACK→0. XOR only toggles when the pixel value is non-zero,
     * so WHITE is required here (BLACK becomes 0 and is a no-op).
     */
    vsl_color(aes_state.vdi_handle, WHITE);
    for (position = left; position <= right; ++position, ++phase) {
        if ((phase & 1u) == 0u) {
            aes_draw_drag_dot(position, top);
        }
    }
    for (position = (WORD)(top + 1); position <= bottom;
         ++position, ++phase) {
        if ((phase & 1u) == 0u) {
            aes_draw_drag_dot(right, position);
        }
    }
    for (position = (WORD)(right - 1); position >= left;
         --position, ++phase) {
        if ((phase & 1u) == 0u) {
            aes_draw_drag_dot(position, bottom);
        }
    }
    for (position = (WORD)(bottom - 1); position > top;
         --position, ++phase) {
        if ((phase & 1u) == 0u) {
            aes_draw_drag_dot(left, position);
        }
    }
    (void)vswr_mode(aes_state.vdi_handle, previous_mode);
    /*
     * Rubber-band feedback is drawn under wind_update / begin_update, which
     * defers normal presents. Flush this rect so the XOR box is visible
     * during drag and scrollbar tracking.
     */
    vdi_flush_rect((WORD)(rect->g_x - 1), (WORD)(rect->g_y - 1),
                   (WORD)(rect->g_w + 2), (WORD)(rect->g_h + 2));
}

void aes_begin_interaction_lock(void)
{
    ++aes_state.update_depth;
    vdi_begin_update();
}

void aes_end_interaction_lock(void)
{
    if (aes_state.update_depth > 0) {
        --aes_state.update_depth;
        vdi_end_update();
    }
}

void aes_clamp_dragged_window_position(const aes_window_t *window,
                                       const GRECT *desktop, WORD *x, WORD *y)
{
    WORD title_height;
    WORD visible_width;
    WORD min_x;
    WORD max_x;
    WORD min_y;
    WORD max_y;

    if (window == NULL || desktop == NULL || x == NULL || y == NULL) {
        return;
    }

    /*
     * Permit windows to move partially off-screen while keeping a small
     * reachable strip of the title area visible so the user can always
     * drag the window back.
     */
    title_height = (WORD)(window->work.g_y - window->outer.g_y);
    if (title_height <= 0) {
        title_height = aes_chrome_height();
    }

    visible_width = 32;
    if (visible_width > window->outer.g_w) {
        visible_width = window->outer.g_w;
    }
    if (visible_width <= 0) {
        visible_width = 1;
    }

    min_x = (WORD)(desktop->g_x - window->outer.g_w + visible_width);
    max_x = (WORD)(desktop->g_x + desktop->g_w - visible_width);
    min_y = aes_menu_bar_height();
    max_y = (WORD)(desktop->g_y + desktop->g_h - title_height);

    *x = aes_max_word(min_x, aes_min_word(*x, max_x));
    *y = aes_max_word(min_y, aes_min_word(*y, max_y));
}

/* Drag the outline of a movable window and queue WM_MOVED. */
WORD aes_track_move(const aes_track_t *track)
{
    aes_window_t *window = track->window;
    WORD flags = track->flags;
    WORD *mepbuff = track->mepbuff;
    WORD *pmx = track->pmx;
    WORD *pmy = track->pmy;
    WORD *pmb = track->pmb;
    WORD *pks = track->pks;
    WORD start_x = track->start_x;
    WORD start_y = track->start_y;
    int defer_raise = track->defer_raise;
    GRECT desktop;

    gem_hid_event_t evt;
    GRECT original = window->outer;
    GRECT drag_rect = original;
    GRECT last_rect = original;
    int drag_drawn = 0;

    aes_desktop_rect(&desktop);
    vdi_begin_update();
    v_hide_c(aes_state.vdi_handle);
    aes_draw_drag_outline(&drag_rect);
    v_show_c(aes_state.vdi_handle, 1);
    vdi_end_update();
    drag_drawn = 1;

    FOREVER
    {
        if (gem_hid_poll(&evt) == 0) {
            gem_os_sleep_ms(1u);
            continue;
        }
        if (evt.type == GEM_HID_MOUSE_MOVE ||
            evt.type == GEM_HID_MOUSE_BUTTON) {
            WORD new_x;
            WORD new_y;

            vdi_begin_update();
            v_hide_c(aes_state.vdi_handle);
            aes_store_mouse_state(&evt);

            new_x = (WORD)(original.g_x + evt.x - start_x);
            new_y = (WORD)(original.g_y + evt.y - start_y);
            aes_clamp_dragged_window_position(window, &desktop, &new_x, &new_y);
            if (new_x != drag_rect.g_x || new_y != drag_rect.g_y) {
                if (drag_drawn != 0) {
                    aes_draw_drag_outline(&last_rect);
                }
                aes_set_rect(&drag_rect, new_x, new_y, original.g_w,
                             original.g_h);
                aes_draw_drag_outline(&drag_rect);
                last_rect = drag_rect;
                drag_drawn = 1;
            }
            v_show_c(aes_state.vdi_handle, 1);
            vdi_end_update();
            graf_mkstate(pmx, pmy, pmb, pks);
        }
        if (evt.type == GEM_HID_MOUSE_BUTTON &&
            evt.button == GEM_HID_BUTTON_LEFT &&
            (evt.flags & GEM_HID_BUTTON_LEFT) == 0u) {
            GRECT previous_outer = window->outer;

            vdi_begin_update();
            v_hide_c(aes_state.vdi_handle);
            aes_store_mouse_state(&evt);
            if (drag_drawn != 0) {
                aes_draw_drag_outline(&last_rect);
            }
            v_show_c(aes_state.vdi_handle, 1);
            vdi_end_update();
            if (defer_raise != 0) {
                /* Use the complete top-window path even for a stationary
                 * title click. Besides z-order and title state it remaps
                 * overlays and queues the newly exposed client repaint. */
                aes_top_window(window);
            }
            window->previous_outer = previous_outer;
            aes_set_rect(&window->outer, drag_rect.g_x, drag_rect.g_y,
                         drag_rect.g_w, drag_rect.g_h);
            if (window->iconified != 0) {
                window->restored_outer.g_x = drag_rect.g_x;
                window->restored_outer.g_y = drag_rect.g_y;
                window->restored_outer.g_w = drag_rect.g_w;
            } else {
                window->restored_outer = window->outer;
            }
            aes_compute_work(window);
            if (window->open != 0) {
                aes_redraw_window_change(&window->previous_outer,
                                         &window->outer);
            }
            aes_queue_window_message(window, WM_MOVED, drag_rect.g_x,
                                     drag_rect.g_y, drag_rect.g_w,
                                     drag_rect.g_h);
            if (defer_raise != 0) {
                aes_queue_window_message(window, WM_TOPPED, 0, 0, 0, 0);
            }
            if ((flags & MU_MESAG) != 0u && mepbuff != NULL &&
                aes_dequeue_message(mepbuff) != 0) {
                aes_end_interaction_lock();
                return MU_MESAG;
            }
            aes_end_interaction_lock();
            return 0;
        }
    }
}

/* Drag the outline of a sizable window and queue WM_SIZED. */
WORD aes_track_size(const aes_track_t *track)
{
    aes_window_t *window = track->window;
    WORD flags = track->flags;
    WORD *mepbuff = track->mepbuff;
    WORD *pmx = track->pmx;
    WORD *pmy = track->pmy;
    WORD *pmb = track->pmb;
    WORD *pks = track->pks;
    WORD start_x = track->start_x;
    WORD start_y = track->start_y;
    GRECT desktop;

    gem_hid_event_t evt;
    GRECT original = window->outer;
    GRECT drag_rect = original;
    GRECT last_rect = original;
    int drag_drawn = 0;

    aes_desktop_rect(&desktop);
    vdi_begin_update();
    v_hide_c(aes_state.vdi_handle);
    aes_draw_drag_outline(&drag_rect);
    v_show_c(aes_state.vdi_handle, 1);
    vdi_end_update();
    drag_drawn = 1;

    FOREVER
    {
        if (gem_hid_poll(&evt) == 0) {
            gem_os_sleep_ms(1u);
            continue;
        }
        if (evt.type == GEM_HID_MOUSE_MOVE ||
            evt.type == GEM_HID_MOUSE_BUTTON) {
            WORD new_w;
            WORD new_h;

            vdi_begin_update();
            v_hide_c(aes_state.vdi_handle);
            aes_store_mouse_state(&evt);
            new_w = (WORD)(original.g_w + evt.x - start_x);
            new_h = (WORD)(original.g_h + evt.y - start_y);
            new_w = aes_max_word(
                96, aes_min_word(new_w, (WORD)(desktop.g_x + desktop.g_w -
                                               original.g_x)));
            new_h = aes_max_word(
                64, aes_min_word(new_h, (WORD)(desktop.g_y + desktop.g_h -
                                               original.g_y)));
            if (new_w != drag_rect.g_w || new_h != drag_rect.g_h) {
                if (drag_drawn != 0) {
                    aes_draw_drag_outline(&last_rect);
                }
                aes_set_rect(&drag_rect, original.g_x, original.g_y, new_w,
                             new_h);
                aes_draw_drag_outline(&drag_rect);
                last_rect = drag_rect;
                drag_drawn = 1;
            }
            v_show_c(aes_state.vdi_handle, 1);
            vdi_end_update();
            graf_mkstate(pmx, pmy, pmb, pks);
        }
        if (evt.type == GEM_HID_MOUSE_BUTTON &&
            evt.button == GEM_HID_BUTTON_LEFT &&
            (evt.flags & GEM_HID_BUTTON_LEFT) == 0u) {
            GRECT previous_outer = window->outer;

            vdi_begin_update();
            v_hide_c(aes_state.vdi_handle);
            aes_store_mouse_state(&evt);
            if (drag_drawn != 0) {
                aes_draw_drag_outline(&last_rect);
            }
            v_show_c(aes_state.vdi_handle, 1);
            vdi_end_update();
            window->previous_outer = previous_outer;
            aes_set_rect(&window->outer, drag_rect.g_x, drag_rect.g_y,
                         drag_rect.g_w, drag_rect.g_h);
            if (window->iconified == 0) {
                window->restored_outer = window->outer;
            }
            aes_compute_work(window);
            if (window->open != 0) {
                aes_redraw_window_change(&window->previous_outer,
                                         &window->outer);
            }
            aes_queue_window_message(window, WM_SIZED, drag_rect.g_x,
                                     drag_rect.g_y, drag_rect.g_w,
                                     drag_rect.g_h);
            if ((flags & MU_MESAG) != 0u && mepbuff != NULL &&
                aes_dequeue_message(mepbuff) != 0) {
                aes_end_interaction_lock();
                return MU_MESAG;
            }
            aes_end_interaction_lock();
            return 0;
        }
    }
}
