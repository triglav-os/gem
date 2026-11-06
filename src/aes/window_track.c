/*
 * Tracks mouse interaction with hosted AES window chrome: routes a press
 * to the closer, fuller, scroll arrows and page areas, slider thumbs or
 * the move and size drags in window_drag.c, and queues the resulting WM_*
 * messages for the owning application.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (C) 2026 tomaz stih
 */

#include "window_track.h"

#include "platform/os.h"

#include <stdint.h>
#include <string.h>

void aes_queue_window_message(const aes_window_t *window, WORD message, WORD w4,
                              WORD w5, WORD w6, WORD w7)
{
    WORD msg[8];

    if (window == NULL || window->owner == 0) {
        return;
    }

    msg[0] = message;
    msg[1] = aes_state.current_app_id;
    msg[2] = 0;
    msg[3] = window->handle;
    msg[4] = w4;
    msg[5] = w5;
    msg[6] = w6;
    msg[7] = w7;
    (void)appl_write(window->owner, 8, msg);
}

static void aes_toggle_window_iconified(aes_window_t *window)
{
    GRECT previous_outer;
    WORD title_height;
    WORD bottom_border;

    if (window == NULL) {
        return;
    }

    previous_outer = window->outer;
    if (window->iconified != 0) {
        window->outer = window->restored_outer;
        window->iconified = 0;
    } else {
        window->restored_outer = window->outer;
        title_height = (WORD)(window->work.g_y - window->outer.g_y);
        if (title_height <= 0) {
            title_height = aes_chrome_height();
        }
        bottom_border =
            (WORD)(window->outer.g_h - (window->work.g_y - window->outer.g_y) -
                   window->work.g_h);
        if (bottom_border < 1) {
            bottom_border = 1;
        }
        window->outer.g_h = (WORD)(title_height + bottom_border);
        window->iconified = 1;
    }

    window->previous_outer = previous_outer;
    aes_compute_work(window);
    if (window->open != 0) {
        aes_redraw_window_change(&previous_outer, &window->outer);
    }
}

static WORD aes_track_closer(const aes_track_t *track)
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

    gem_hid_event_t evt;

    if (aes_point_in_rect(start_x, start_y, &window->outer) == 0) {
        return 0;
    }

    if (aes_window_hit_part(window, start_x, start_y) !=
        AES_WINDOW_PART_CLOSER) {
        return 0;
    }

    FOREVER
    {
        if (gem_hid_poll(&evt) == 0) {
            gem_os_sleep_ms(1u);
            continue;
        }
        if (evt.type == GEM_HID_MOUSE_MOVE ||
            evt.type == GEM_HID_MOUSE_BUTTON) {
            aes_store_mouse_state(&evt);
            graf_mkstate(pmx, pmy, pmb, pks);
        }
        if (evt.type == GEM_HID_MOUSE_BUTTON &&
            evt.button == GEM_HID_BUTTON_LEFT &&
            (evt.flags & GEM_HID_BUTTON_LEFT) == 0u) {
            if (aes_window_hit_part(window, (WORD)evt.x, (WORD)evt.y) ==
                AES_WINDOW_PART_CLOSER) {
                aes_queue_window_message(window, WM_CLOSED, 0, 0, 0, 0);
                if ((flags & MU_MESAG) != 0u && mepbuff != NULL &&
                    aes_dequeue_message(mepbuff) != 0) {
                    return MU_MESAG;
                }
            }
            return 0;
        }
    }
}

/* Queue WM_FULLED, or toggle iconification on a right press. */
static WORD aes_track_fuller(const aes_track_t *track)
{
    aes_window_t *window = track->window;
    const gem_hid_event_t *first_evt = track->first_evt;
    WORD flags = track->flags;
    WORD *mepbuff = track->mepbuff;
    WORD *pmx = track->pmx;
    WORD *pmy = track->pmy;
    WORD *pmb = track->pmb;
    WORD *pks = track->pks;

    if (first_evt->button == GEM_HID_BUTTON_RIGHT) {
        aes_toggle_window_iconified(window);
        return 0;
    }
    aes_queue_window_message(window, WM_FULLED, 0, 0, 0, 0);
    if ((flags & MU_MESAG) != 0u && mepbuff != NULL &&
        aes_dequeue_message(mepbuff) != 0) {
        graf_mkstate(pmx, pmy, pmb, pks);
        return MU_MESAG;
    }
    return 0;
}

/* Queue WM_ARROWED for a scroll arrow or page area. */
static WORD aes_track_arrow(const aes_track_t *track, WORD part)
{
    aes_window_t *window = track->window;
    WORD flags = track->flags;
    WORD *mepbuff = track->mepbuff;
    WORD arrow_code = WA_UPLINE;

    switch (part) {
        case AES_WINDOW_PART_VUP:
            arrow_code = WA_UPLINE;
            break;
        case AES_WINDOW_PART_VDOWN:
            arrow_code = WA_DNLINE;
            break;
        case AES_WINDOW_PART_HLEFT:
            arrow_code = WA_LFLINE;
            break;
        case AES_WINDOW_PART_HRIGHT:
            arrow_code = WA_RTLINE;
            break;
        case AES_WINDOW_PART_VPAGE_UP:
            arrow_code = WA_UPPAGE;
            break;
        case AES_WINDOW_PART_VPAGE_DOWN:
            arrow_code = WA_DNPAGE;
            break;
        case AES_WINDOW_PART_HPAGE_LEFT:
            arrow_code = WA_LFPAGE;
            break;
        case AES_WINDOW_PART_HPAGE_RIGHT:
            arrow_code = WA_RTPAGE;
            break;
        default:
            break;
    }

    aes_queue_window_message(window, WM_ARROWED, arrow_code, 0, 0, 0);
    if ((flags & MU_MESAG) != 0u && mepbuff != NULL &&
        aes_dequeue_message(mepbuff) != 0) {
        return MU_MESAG;
    }
    return 0;
}

/* Drag a slider thumb with XOR feedback and queue WM_VSLID/WM_HSLID. */
static WORD aes_track_slider(const aes_track_t *track, WORD part)
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

    gem_hid_event_t evt;
    GRECT slot;
    GRECT thumb;
    GRECT drag_rect;
    GRECT last_rect;
    WORD press_offset;
    int drag_drawn = 0;

    if (part == AES_WINDOW_PART_VSLIDE) {
        if (aes_window_vslot_rect(window, &slot) == 0 ||
            aes_window_vthumb_rect(window, &thumb) == 0) {
            aes_end_interaction_lock();
            return 0;
        }
        press_offset = (WORD)(start_y - thumb.g_y);
    } else {
        if (aes_window_hslot_rect(window, &slot) == 0 ||
            aes_window_hthumb_rect(window, &thumb) == 0) {
            aes_end_interaction_lock();
            return 0;
        }
        press_offset = (WORD)(start_x - thumb.g_x);
    }

    drag_rect = thumb;
    last_rect = thumb;
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
            vdi_begin_update();
            v_hide_c(aes_state.vdi_handle);
            aes_store_mouse_state(&evt);
            if (evt.type == GEM_HID_MOUSE_MOVE) {
                if (part == AES_WINDOW_PART_VSLIDE) {
                    WORD limit = (WORD)(slot.g_h - thumb.g_h);
                    WORD pos = (WORD)(evt.y - slot.g_y - press_offset);

                    if (limit > 0) {
                        pos = aes_max_word(0, aes_min_word(pos, limit));
                    } else {
                        pos = 0;
                    }
                    if (drag_drawn != 0) {
                        aes_draw_drag_outline(&last_rect);
                    }
                    aes_set_rect(&drag_rect, thumb.g_x, (WORD)(slot.g_y + pos),
                                 thumb.g_w, thumb.g_h);
                } else {
                    WORD limit = (WORD)(slot.g_w - thumb.g_w);
                    WORD pos = (WORD)(evt.x - slot.g_x - press_offset);

                    if (limit > 0) {
                        pos = aes_max_word(0, aes_min_word(pos, limit));
                    } else {
                        pos = 0;
                    }
                    if (drag_drawn != 0) {
                        aes_draw_drag_outline(&last_rect);
                    }
                    aes_set_rect(&drag_rect, (WORD)(slot.g_x + pos), thumb.g_y,
                                 thumb.g_w, thumb.g_h);
                }
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
            vdi_begin_update();
            v_hide_c(aes_state.vdi_handle);
            aes_store_mouse_state(&evt);
            if (drag_drawn != 0) {
                aes_draw_drag_outline(&last_rect);
            }
            v_show_c(aes_state.vdi_handle, 1);
            vdi_end_update();
            break;
        }
    }

    if (part == AES_WINDOW_PART_VSLIDE) {
        WORD limit = (WORD)(slot.g_h - thumb.g_h);
        WORD pos = (WORD)(drag_rect.g_y - slot.g_y);
        WORD slider = 0;

        if (limit > 0) {
            pos = aes_max_word(0, aes_min_word(pos, limit));
            slider = (WORD)((1000L * pos) / limit);
        }
        aes_queue_window_message(window, WM_VSLID, slider, 0, 0, 0);
    } else {
        WORD limit = (WORD)(slot.g_w - thumb.g_w);
        WORD pos = (WORD)(drag_rect.g_x - slot.g_x);
        WORD slider = 0;

        if (limit > 0) {
            pos = aes_max_word(0, aes_min_word(pos, limit));
            slider = (WORD)((1000L * pos) / limit);
        }
        aes_queue_window_message(window, WM_HSLID, slider, 0, 0, 0);
    }

    if ((flags & MU_MESAG) != 0u && mepbuff != NULL &&
        aes_dequeue_message(mepbuff) != 0) {
        aes_end_interaction_lock();
        return MU_MESAG;
    }
    aes_end_interaction_lock();
    return 0;
}

WORD aes_track_window_interaction(const gem_hid_event_t *first_evt, WORD flags,
                                  WORD mepbuff[8], WORD *pmx, WORD *pmy,
                                  WORD *pmb, WORD *pks)
{
    aes_track_t track;
    aes_window_t *window;
    WORD handle;
    WORD part;
    WORD start_x;
    WORD start_y;
    WORD raised;
    int defer_raise = 0;
    int interaction_lock = 0;

    if (first_evt == NULL || first_evt->type != GEM_HID_MOUSE_BUTTON ||
        ((first_evt->button != GEM_HID_BUTTON_LEFT &&
          first_evt->button != GEM_HID_BUTTON_RIGHT)) ||
        ((first_evt->button == GEM_HID_BUTTON_LEFT &&
          (first_evt->flags & GEM_HID_BUTTON_LEFT) == 0u) ||
         (first_evt->button == GEM_HID_BUTTON_RIGHT &&
          (first_evt->flags & GEM_HID_BUTTON_RIGHT) == 0u))) {
        return 0;
    }

    handle = wind_find((WORD)first_evt->x, (WORD)first_evt->y);
    if (handle == 0) {
        return 0;
    }

    window = aes_find_window(handle);
    if (window == NULL) {
        return 0;
    }

    part = aes_window_hit_part(window, (WORD)first_evt->x, (WORD)first_evt->y);
    start_x = (WORD)first_evt->x;
    start_y = (WORD)first_evt->y;
    raised = (aes_window_is_top(window) == 0) ? 1 : 0;
    defer_raise = raised != 0 && part == AES_WINDOW_PART_TITLE &&
                  (window->kind & MOVER) != 0u;
    interaction_lock =
        (part == AES_WINDOW_PART_TITLE && (window->kind & MOVER) != 0u) ||
        (part == AES_WINDOW_PART_SIZER && (window->kind & SIZER) != 0u) ||
        part == AES_WINDOW_PART_VSLIDE || part == AES_WINDOW_PART_HSLIDE;

    if (interaction_lock != 0) {
        aes_begin_interaction_lock();
    }

    if (raised != 0 && defer_raise == 0) {
        aes_top_window(window);
    }

    track.window = window;
    track.first_evt = first_evt;
    track.flags = flags;
    track.mepbuff = mepbuff;
    track.pmx = pmx;
    track.pmy = pmy;
    track.pmb = pmb;
    track.pks = pks;
    track.start_x = start_x;
    track.start_y = start_y;
    track.defer_raise = defer_raise;

    if (part == AES_WINDOW_PART_CLOSER) {
        return aes_track_closer(&track);
    }
    if (part == AES_WINDOW_PART_FULLER) {
        return aes_track_fuller(&track);
    }
    if (part == AES_WINDOW_PART_VUP || part == AES_WINDOW_PART_VDOWN ||
        part == AES_WINDOW_PART_HLEFT || part == AES_WINDOW_PART_HRIGHT ||
        part == AES_WINDOW_PART_VPAGE_UP ||
        part == AES_WINDOW_PART_VPAGE_DOWN ||
        part == AES_WINDOW_PART_HPAGE_LEFT ||
        part == AES_WINDOW_PART_HPAGE_RIGHT) {
        return aes_track_arrow(&track, part);
    }
    if (part == AES_WINDOW_PART_VSLIDE || part == AES_WINDOW_PART_HSLIDE) {
        return aes_track_slider(&track, part);
    }
    if (part == AES_WINDOW_PART_TITLE && (window->kind & MOVER) != 0u) {
        return aes_track_move(&track);
    }
    if (part == AES_WINDOW_PART_SIZER && (window->kind & SIZER) != 0u) {
        return aes_track_size(&track);
    }

    if (raised != 0) {
        aes_queue_window_message(window, WM_TOPPED, 0, 0, 0, 0);
        if ((flags & MU_MESAG) != 0u && mepbuff != NULL &&
            aes_dequeue_message(mepbuff) != 0) {
            return MU_MESAG;
        }
    }
    return 0;
}
