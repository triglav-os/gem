/*
 * Implements hosted AES event polling, mouse helpers, and
 * interactive graphics calls such as drag boxes and slider tracking.
 *
 * MIT License (see: LICENSE)
 * Copyright (C) 2026 tomaz stih
 */

#include "aes_internal.h"

#include "platform/os.h"

#include <stdint.h>
#include <string.h>
extern WORD vdi_select_system_mouse_form(WORD selector);

static void aes_queue_window_message(const aes_window_t *window, WORD message,
                                     WORD w4, WORD w5, WORD w6, WORD w7);
static void aes_draw_drag_outline(const GRECT *rect);
static void aes_begin_interaction_lock(void);
static void aes_end_interaction_lock(void);
static void aes_clamp_dragged_window_position(const aes_window_t *window,
                                              const GRECT *desktop, WORD *x,
                                              WORD *y);
static void aes_toggle_window_iconified(aes_window_t *window);
static WORD aes_track_window_interaction(const gem_hid_event_t *first_evt,
                                         WORD flags, WORD mepbuff[8], WORD *pmx,
                                         WORD *pmy, WORD *pmb, WORD *pks);
static WORD aes_input_event_owner(const gem_hid_event_t *evt);
static void aes_activate_input_target(const gem_hid_event_t *evt);
static void aes_queue_input_event(WORD app_id, const gem_hid_event_t *evt);
static int aes_dequeue_input_event(WORD app_id, gem_hid_event_t *evt);
WORD evnt_keybd(void);
WORD evnt_button(WORD clicks, UWORD mask, UWORD state, WORD *pmx, WORD *pmy,
                 WORD *pmb, WORD *pks);
WORD evnt_mouse(WORD flags, WORD x, WORD y, WORD w, WORD h, WORD *pmx,
                WORD *pmy, WORD *pmb, WORD *pks);
WORD evnt_mesag(WORD msg[8]);
WORD evnt_timer(WORD count_low, WORD count_high);
WORD evnt_multi(UWORD flags, UWORD bclk, UWORD bmsk, UWORD bst, UWORD m1flags,
                WORD m1x, WORD m1y, WORD m1w, WORD m1h, UWORD m2flags, WORD m2x,
                WORD m2y, WORD m2w, WORD m2h, WORD mepbuff[8], UWORD tlc,
                UWORD thc, WORD *pmx, WORD *pmy, WORD *pmb, WORD *pks,
                WORD *pkr, WORD *pbr);
WORD evnt_dclick(WORD clicks, WORD setget);
WORD graf_rubbox(WORD xorigin, WORD yorigin, WORD wmin, WORD hmin, WORD *pwend,
                 WORD *phend);
WORD graf_dragbox(WORD w, WORD h, WORD sx, WORD sy, WORD xc, WORD yc, WORD wc,
                  WORD hc, WORD *pdx, WORD *pdy);
WORD graf_mbox(WORD w, WORD h, WORD srcx, WORD srcy, WORD dstx, WORD dsty);
WORD graf_growbox(WORD x1, WORD y1, WORD w1, WORD h1, WORD x2, WORD y2, WORD w2,
                  WORD h2);
WORD graf_shrinkbox(WORD x1, WORD y1, WORD w1, WORD h1, WORD x2, WORD y2,
                    WORD w2, WORD h2);
WORD graf_watchbox(OBJECT *tree, WORD object, UWORD in_state, UWORD out_state);
WORD graf_slidebox(OBJECT *tree, WORD parent, WORD object, WORD orientation);
WORD graf_handle(WORD *charw, WORD *charh, WORD *boxw, WORD *boxh);
WORD graf_mouse(WORD mode, void *form);
VOID graf_mkstate(WORD *pmx, WORD *pmy, WORD *pmb, WORD *pks);

static void aes_post_menu_selection(WORD mepbuff[8])
{
    if (aes_state.menu_owner_app_id == 0) {
        return;
    }
    (void)appl_write(aes_state.menu_owner_app_id, 8, mepbuff);
}

/* Route work-area presses to their window owner and desktop presses to
 * the application that owns the menu bar. */
static WORD aes_input_event_owner(const gem_hid_event_t *evt)
{
    aes_window_t *window;
    WORD handle;
    WORD fallback = aes_state.active_app_id != 0 ? aes_state.active_app_id
                                                 : aes_state.menu_owner_app_id;

    if (evt == NULL) {
        return 0;
    }
    /* A standalone form may have neither a window nor a menu. It still
     * needs an input recipient when gemd, rather than the form, polls HID. */
    if (aes_find_app_by_id(fallback) == NULL) {
        fallback = 0;
        for (size_t i = 0; i < AES_MAX_APPS; ++i) {
            if (aes_state.apps[i].used) {
                fallback = aes_state.apps[i].id;
                break;
            }
        }
    }
    if (evt->type == GEM_HID_KEY) {
        return fallback;
    }
    if (evt->type != GEM_HID_MOUSE_BUTTON) {
        return 0;
    }

    handle = wind_find((WORD)evt->x, (WORD)evt->y);
    if (handle == 0) {
        return aes_state.desktop_owner_app_id != 0
                   ? aes_state.desktop_owner_app_id
                   : fallback;
    }

    window = aes_find_window(handle);
    return (window != NULL) ? window->owner : 0;
}

static void aes_activate_input_target(const gem_hid_event_t *evt)
{
    aes_window_t *window;
    GRECT desktop;
    WORD handle;

    if (evt == NULL || evt->type != GEM_HID_MOUSE_BUTTON ||
        (evt->flags & evt->button) == 0u) {
        return;
    }

    handle = wind_find((WORD)evt->x, (WORD)evt->y);
    if (handle == 0) {
        aes_desktop_rect(&desktop);
        if (aes_point_in_rect((WORD)evt->x, (WORD)evt->y, &desktop) != 0) {
            aes_menu_switch_to_app(aes_state.desktop_owner_app_id);
        }
        return;
    }

    window = aes_find_window(handle);
    if (window != NULL) {
        aes_menu_switch_to_app(window->owner);
    }
}

static void aes_queue_input_event(WORD app_id, const gem_hid_event_t *evt)
{
    aes_app_t *app = aes_find_app_by_id(app_id);
    WORD tail;

    if (app == NULL || evt == NULL ||
        app->input_event_count >= AES_APP_INPUT_QUEUE_SIZE) {
        return;
    }

    tail = (WORD)((app->input_event_head + app->input_event_count) %
                  AES_APP_INPUT_QUEUE_SIZE);
    app->input_events[tail] = *evt;
    ++app->input_event_count;
}

static int aes_dequeue_input_event(WORD app_id, gem_hid_event_t *evt)
{
    aes_app_t *app = aes_find_app_by_id(app_id);

    if (app == NULL || evt == NULL || app->input_event_count == 0) {
        return 0;
    }

    *evt = app->input_events[app->input_event_head];
    app->input_event_head =
        (WORD)((app->input_event_head + 1) % AES_APP_INPUT_QUEUE_SIZE);
    --app->input_event_count;
    return 1;
}

void aes_dispatch_hid_event(const gem_hid_event_t *evt)
{
    WORD mepbuff[8];

    if (evt == NULL) {
        return;
    }

    if (evt->type == GEM_HID_KEY) {
        aes_store_key_state(evt);
        if (aes_state.menu_visible != 0 && aes_state.menu_tree != NULL &&
            aes_menu_key_event(aes_state.menu_tree, evt, mepbuff) != 0) {
            aes_post_menu_selection(mepbuff);
            return;
        }
        aes_queue_input_event(aes_input_event_owner(evt), evt);
        return;
    }

    if (evt->type == GEM_HID_MOUSE_MOVE || evt->type == GEM_HID_MOUSE_BUTTON) {
        aes_store_mouse_state(evt);
        if (evt->type == GEM_HID_MOUSE_BUTTON) {
            aes_window_t *window =
                aes_find_window(wind_find((WORD)evt->x, (WORD)evt->y));
            int chrome =
                window != NULL &&
                aes_window_hit_part(window, (WORD)evt->x, (WORD)evt->y) !=
                    AES_WINDOW_PART_WORK;
            aes_activate_input_target(evt);
            if (aes_state.menu_visible != 0 && aes_state.menu_tree != NULL &&
                aes_menu_event(aes_state.menu_tree, evt, mepbuff) != 0) {
                aes_post_menu_selection(mepbuff);
                return;
            }
            (void)aes_track_window_interaction(evt, 0, NULL, NULL, NULL, NULL,
                                               NULL);
            /* Tracking consumes the release and can move the window away
             * from this press. Replaying it could start another interaction
             * in the newly exposed application with no release left. */
            if (chrome)
                return;
            if (evt->button == GEM_HID_BUTTON_LEFT ||
                evt->button == GEM_HID_BUTTON_RIGHT) {
                aes_queue_input_event(aes_input_event_owner(evt), evt);
            }
        }
    }
}

static void aes_queue_window_message(const aes_window_t *window, WORD message,
                                     WORD w4, WORD w5, WORD w6, WORD w7)
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

static void aes_draw_drag_outline(const GRECT *rect)
{
    WORD box[10];
    WORD previous_mode;

    if (rect == NULL || rect->g_w <= 0 || rect->g_h <= 0 ||
        aes_ensure_vdi() == 0) {
        return;
    }

    box[0] = rect->g_x;
    box[1] = rect->g_y;
    box[2] = (WORD)(rect->g_x + rect->g_w - 1);
    box[3] = box[1];
    box[4] = box[2];
    box[5] = (WORD)(rect->g_y + rect->g_h - 1);
    box[6] = box[0];
    box[7] = box[5];
    box[8] = box[0];
    box[9] = box[1];

    previous_mode = vdi_write_mode();
    (void)vswr_mode(aes_state.vdi_handle, MD_XOR);
    /*
     * Line color is a VDI color *index* then mapped by vdi_color_to_pixel:
     * WHITE→1, BLACK→0. XOR only toggles when the pixel value is non-zero,
     * so WHITE is required here (BLACK becomes 0 and is a no-op).
     */
    vsl_color(aes_state.vdi_handle, WHITE);
    v_pline(aes_state.vdi_handle, 5, box);
    (void)vswr_mode(aes_state.vdi_handle, previous_mode);
    /*
     * Rubber-band feedback is drawn under wind_update / begin_update, which
     * defers normal presents. Flush this rect so the XOR box is visible
     * during drag and scrollbar tracking.
     */
    vdi_flush_rect((WORD)(rect->g_x - 1), (WORD)(rect->g_y - 1),
                   (WORD)(rect->g_w + 2), (WORD)(rect->g_h + 2));
}

static void aes_begin_interaction_lock(void)
{
    ++aes_state.update_depth;
    vdi_begin_update();
}

static void aes_end_interaction_lock(void)
{
    if (aes_state.update_depth > 0) {
        --aes_state.update_depth;
        vdi_end_update();
    }
}

static void aes_clamp_dragged_window_position(const aes_window_t *window,
                                              const GRECT *desktop, WORD *x,
                                              WORD *y)
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

static WORD aes_track_window_interaction(const gem_hid_event_t *first_evt,
                                         WORD flags, WORD mepbuff[8], WORD *pmx,
                                         WORD *pmy, WORD *pmb, WORD *pks)
{
    aes_window_t *window;
    GRECT desktop;
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

    if (part == AES_WINDOW_PART_CLOSER) {
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

    if (part == AES_WINDOW_PART_FULLER) {
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

    if (part == AES_WINDOW_PART_VUP || part == AES_WINDOW_PART_VDOWN ||
        part == AES_WINDOW_PART_HLEFT || part == AES_WINDOW_PART_HRIGHT ||
        part == AES_WINDOW_PART_VPAGE_UP ||
        part == AES_WINDOW_PART_VPAGE_DOWN ||
        part == AES_WINDOW_PART_HPAGE_LEFT ||
        part == AES_WINDOW_PART_HPAGE_RIGHT) {
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

    if (part == AES_WINDOW_PART_VSLIDE || part == AES_WINDOW_PART_HSLIDE) {
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
                        aes_set_rect(&drag_rect, thumb.g_x,
                                     (WORD)(slot.g_y + pos), thumb.g_w,
                                     thumb.g_h);
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
                        aes_set_rect(&drag_rect, (WORD)(slot.g_x + pos),
                                     thumb.g_y, thumb.g_w, thumb.g_h);
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

    if (part == AES_WINDOW_PART_TITLE && (window->kind & MOVER) != 0u) {
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
                aes_clamp_dragged_window_position(window, &desktop, &new_x,
                                                  &new_y);
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
                aes_window_t *previous_top = NULL;

                vdi_begin_update();
                v_hide_c(aes_state.vdi_handle);
                aes_store_mouse_state(&evt);
                if (drag_drawn != 0) {
                    aes_draw_drag_outline(&last_rect);
                }
                v_show_c(aes_state.vdi_handle, 1);
                vdi_end_update();
                if (defer_raise != 0) {
                    previous_top = aes_find_top_window();
                    aes_raise_window(window);
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
                if (defer_raise != 0) {
                    aes_redraw_window_title_states(previous_top, window);
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

    if (part == AES_WINDOW_PART_SIZER && (window->kind & SIZER) != 0u) {
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

    if (raised != 0) {
        aes_queue_window_message(window, WM_TOPPED, 0, 0, 0, 0);
        if ((flags & MU_MESAG) != 0u && mepbuff != NULL &&
            aes_dequeue_message(mepbuff) != 0) {
            return MU_MESAG;
        }
    }
    return 0;
}

WORD evnt_keybd(void)
{
    gem_hid_event_t evt;

    FOREVER
    {
        if (gem_hid_poll(&evt) != 0) {
            if (evt.type == GEM_HID_MOUSE_MOVE ||
                evt.type == GEM_HID_MOUSE_BUTTON) {
                aes_store_mouse_state(&evt);
                continue;
            }
            if (evt.type == GEM_HID_KEY && (evt.flags & 1u) != 0u) {
                aes_store_key_state(&evt);
                return (WORD)evt.key;
            }
        }
        gem_os_sleep_ms(1u);
    }
}

WORD evnt_button(WORD clicks, UWORD mask, UWORD state, WORD *pmx, WORD *pmy,
                 WORD *pmb, WORD *pks)
{
    WORD buttons;

    (void)clicks;

    graf_mkstate(pmx, pmy, &buttons, pks);
    if (pmb != NULL) {
        *pmb = buttons;
    }
    return (((UWORD)buttons & mask) == state) ? 1 : 0;
}

WORD evnt_mouse(WORD flags, WORD x, WORD y, WORD w, WORD h, WORD *pmx,
                WORD *pmy, WORD *pmb, WORD *pks)
{
    WORD mx;
    WORD my;
    WORD mb;
    WORD ks;
    GRECT rect;
    int inside;

    graf_mkstate(&mx, &my, &mb, &ks);
    aes_set_rect(&rect, x, y, w, h);
    inside = aes_point_in_rect(mx, my, &rect);
    if (pmx != NULL) {
        *pmx = mx;
    }
    if (pmy != NULL) {
        *pmy = my;
    }
    if (pmb != NULL) {
        *pmb = mb;
    }
    if (pks != NULL) {
        *pks = ks;
    }
    return (flags != 0) ? inside : !inside;
}

WORD evnt_mesag(WORD msg[8])
{
    FOREVER
    {
        gem_hid_event_t evt;

        if (aes_dequeue_message(msg) != 0) {
            return 1;
        }

        if (gem_hid_poll(&evt) != 0) {
            if (evt.type == GEM_HID_KEY) {
                aes_store_key_state(&evt);
            }
            if (aes_state.menu_visible != 0 && aes_state.menu_tree != NULL &&
                aes_menu_key_event(aes_state.menu_tree, &evt, msg) != 0) {
                return 1;
            }

            if (aes_state.menu_visible != 0 && aes_state.menu_tree != NULL &&
                aes_menu_event(aes_state.menu_tree, &evt, msg) != 0) {
                return 1;
            }

            if (aes_track_window_interaction(&evt, MU_MESAG, msg, NULL, NULL,
                                             NULL, NULL) == MU_MESAG) {
                return 1;
            }

            if (evt.type == GEM_HID_MOUSE_MOVE ||
                evt.type == GEM_HID_MOUSE_BUTTON) {
                aes_store_mouse_state(&evt);
            }
        }

        gem_os_sleep_ms(1u);
    }
}

WORD evnt_timer(WORD count_low, WORD count_high)
{
    uint32_t duration =
        (uint32_t)(uint16_t)count_low | ((uint32_t)(uint16_t)count_high << 16);

    gem_os_sleep_ms(duration);
    return 1;
}

WORD evnt_multi(UWORD flags, UWORD bclk, UWORD bmsk, UWORD bst, UWORD m1flags,
                WORD m1x, WORD m1y, WORD m1w, WORD m1h, UWORD m2flags, WORD m2x,
                WORD m2y, WORD m2w, WORD m2h, WORD mepbuff[8], UWORD tlc,
                UWORD thc, WORD *pmx, WORD *pmy, WORD *pmb, WORD *pks,
                WORD *pkr, WORD *pbr)
{
    uint32_t timeout = (uint32_t)tlc | ((uint32_t)thc << 16);
    uint32_t start = gem_os_ticks_ms();

    (void)bclk;

    FOREVER
    {
        gem_hid_event_t evt;
        WORD mx = 0;
        WORD my = 0;
        WORD mb = 0;
        WORD ks = 0;

        if (aes_wait_hook && !aes_wait_hook())
            return 0;

        if ((flags & MU_MESAG) != 0u && mepbuff != NULL &&
            aes_dequeue_message(mepbuff) != 0) {
            graf_mkstate(pmx, pmy, pmb, pks);
            return MU_MESAG;
        }

        int queued = aes_dequeue_input_event(aes_state.current_app_id, &evt);
        if (queued || ((!aes_external_input || aes_wait_hook) &&
                       gem_hid_poll(&evt) != 0)) {
            WORD event_owner = aes_input_event_owner(&evt);

            if (!queued)
                aes_activate_input_target(&evt);
            event_owner = aes_input_event_owner(&evt);

            if ((evt.type == GEM_HID_KEY || evt.type == GEM_HID_MOUSE_BUTTON) &&
                event_owner != 0 && event_owner != aes_state.current_app_id) {
                aes_queue_input_event(event_owner, &evt);
                continue;
            }
            if (!queued && evt.type == GEM_HID_KEY) {
                aes_store_key_state(&evt);
            }
            if (!queued && (flags & MU_MESAG) != 0u && mepbuff != NULL &&
                aes_state.menu_visible != 0 && aes_state.menu_tree != NULL &&
                aes_menu_key_event(aes_state.menu_tree, &evt, mepbuff) != 0) {
                graf_mkstate(pmx, pmy, pmb, pks);
                return MU_MESAG;
            }

            if (!queued && (flags & MU_MESAG) != 0u && mepbuff != NULL &&
                aes_state.menu_visible != 0 && aes_state.menu_tree != NULL &&
                aes_menu_event(aes_state.menu_tree, &evt, mepbuff) != 0) {
                graf_mkstate(pmx, pmy, pmb, pks);
                return MU_MESAG;
            }

            if (!queued && (flags & MU_MESAG) != 0u && mepbuff != NULL) {
                WORD window_event = aes_track_window_interaction(
                    &evt, flags, mepbuff, pmx, pmy, pmb, pks);

                if (window_event != 0) {
                    return window_event;
                }
            }

            if (evt.type == GEM_HID_MOUSE_MOVE ||
                evt.type == GEM_HID_MOUSE_BUTTON) {
                /* Queued events already updated the physical pointer when
                 * dispatched. Deliver their coordinates without rewinding it.
                 */
                if (!queued)
                    aes_store_mouse_state(&evt);
                mx = (WORD)evt.x;
                my = (WORD)evt.y;
                mb = (WORD)evt.flags;
                ks = aes_state.key_state;
                if (pmx != NULL) {
                    *pmx = mx;
                }
                if (pmy != NULL) {
                    *pmy = my;
                }
                if (pmb != NULL) {
                    *pmb = mb;
                }
                if (pks != NULL) {
                    *pks = ks;
                }

                if ((flags & MU_BUTTON) != 0u &&
                    evt.type == GEM_HID_MOUSE_BUTTON &&
                    (((UWORD)mb & bmsk) == bst)) {
                    if (pbr != NULL) {
                        *pbr = 1;
                    }
                    return MU_BUTTON;
                }
                if ((flags & MU_M1) != 0u &&
                    evnt_mouse((WORD)m1flags, m1x, m1y, m1w, m1h, pmx, pmy, pmb,
                               pks) != 0) {
                    return MU_M1;
                }
                if ((flags & MU_M2) != 0u &&
                    evnt_mouse((WORD)m2flags, m2x, m2y, m2w, m2h, pmx, pmy, pmb,
                               pks) != 0) {
                    return MU_M2;
                }
                continue;
            }

            if ((flags & MU_KEYBD) != 0u && evt.type == GEM_HID_KEY &&
                (evt.flags & 1u) != 0u) {
                if (pkr != NULL) {
                    *pkr = (WORD)evt.key;
                }
                graf_mkstate(pmx, pmy, pmb, pks);
                if (pks)
                    *pks = (WORD)evt.mod;
                return MU_KEYBD;
            }
        }

        if ((flags & MU_TIMER) != 0u) {
            uint32_t now = gem_os_ticks_ms();

            if ((uint32_t)(now - start) >= timeout) {
                graf_mkstate(pmx, pmy, pmb, pks);
                return MU_TIMER;
            }
        }

        gem_os_sleep_ms(1u);
    }
}

WORD evnt_dclick(WORD clicks, WORD setget)
{
    if (setget != 0) {
        aes_state.dclick_rate = clicks;
    }
    return aes_state.dclick_rate;
}

WORD graf_rubbox(WORD xorigin, WORD yorigin, WORD wmin, WORD hmin, WORD *pwend,
                 WORD *phend)
{
    if (pwend != NULL) {
        *pwend = wmin;
    }
    if (phend != NULL) {
        *phend = hmin;
    }
    return (xorigin | yorigin) == (xorigin | yorigin);
}

WORD graf_dragbox(WORD w, WORD h, WORD sx, WORD sy, WORD xc, WORD yc, WORD wc,
                  WORD hc, WORD *pdx, WORD *pdy)
{
    if (pdx != NULL) {
        *pdx = aes_max_word(xc, aes_min_word(sx, (WORD)(xc + wc - w)));
    }
    if (pdy != NULL) {
        *pdy = aes_max_word(yc, aes_min_word(sy, (WORD)(yc + hc - h)));
    }
    return 1;
}

WORD graf_mbox(WORD w, WORD h, WORD srcx, WORD srcy, WORD dstx, WORD dsty)
{
    (void)w;
    (void)h;
    (void)srcx;
    (void)srcy;
    (void)dstx;
    (void)dsty;
    return 1;
}

WORD graf_growbox(WORD x1, WORD y1, WORD w1, WORD h1, WORD x2, WORD y2, WORD w2,
                  WORD h2)
{
    (void)x1;
    (void)y1;
    (void)w1;
    (void)h1;
    (void)x2;
    (void)y2;
    (void)w2;
    (void)h2;
    return 1;
}

WORD graf_shrinkbox(WORD x1, WORD y1, WORD w1, WORD h1, WORD x2, WORD y2,
                    WORD w2, WORD h2)
{
    return graf_growbox(x1, y1, w1, h1, x2, y2, w2, h2);
}

WORD graf_watchbox(OBJECT *tree, WORD object, UWORD in_state, UWORD out_state)
{
    if (tree == NULL || object < 0) {
        return 0;
    }
    tree[object].ob_state = in_state;
    tree[object].ob_state = out_state;
    return 1;
}

WORD graf_slidebox(OBJECT *tree, WORD parent, WORD object, WORD orientation)
{
    OBJECT *parent_obj;
    OBJECT *child_obj;

    (void)orientation;

    if (tree == NULL || parent < 0 || object < 0) {
        return 0;
    }

    parent_obj = &tree[parent];
    child_obj = &tree[object];
    if (parent_obj->ob_height == 0) {
        return 0;
    }
    return (WORD)((1000L * child_obj->ob_y) /
                  aes_max_word(parent_obj->ob_height, 1));
}

WORD graf_handle(WORD *charw, WORD *charh, WORD *boxw, WORD *boxh)
{
    if (aes_ensure_vdi() == 0) {
        return 0;
    }
    if (charw != NULL) {
        *charw = AES_CHAR_WIDTH;
    }
    if (charh != NULL) {
        *charh = AES_CHAR_HEIGHT;
    }
    if (boxw != NULL) {
        *boxw = 8;
    }
    if (boxh != NULL) {
        *boxh = aes_menu_chrome_height();
    }
    return aes_state.vdi_handle;
}

WORD graf_mouse(WORD mode, void *form)
{
    WORD status = 0;
    WORD x = 0;
    WORD y = 0;

    if (aes_ensure_vdi() == 0) {
        return 0;
    }

    if (mode == M_OFF) {
        aes_state.mouse_cursor_hidden = 1;
        v_hide_c(aes_state.vdi_handle);
    } else if (mode == M_ON) {
        aes_state.mouse_cursor_hidden = 0;
        v_show_c(aes_state.vdi_handle, 1);
        vq_mouse(aes_state.vdi_handle, &status, &x, &y);
        aes_store_mouse_state(&(gem_hid_event_t){
            .type = GEM_HID_MOUSE_MOVE, .x = x, .y = y, .flags = status});
    } else if (mode == USER_DEF && form != NULL) {
        aes_state.mouse_base_cursor = USER_DEF;
        (void)vsc_form(aes_state.vdi_handle, (MFORM *)form);
    } else if (mode >= ARROW && mode <= OUTLN_CROSS) {
        aes_state.mouse_base_cursor = mode;
        if (aes_state.mouse_cursor_hidden == 0) {
            if (mode == ARROW) {
                vq_mouse(aes_state.vdi_handle, &status, &x, &y);
                aes_store_mouse_state(
                    &(gem_hid_event_t){.type = GEM_HID_MOUSE_MOVE,
                                       .x = x,
                                       .y = y,
                                       .flags = status});
            } else {
                (void)vdi_select_system_mouse_form(mode);
                aes_state.mouse_applied_cursor = mode;
            }
        }
    }
    return 1;
}

VOID graf_mkstate(WORD *pmx, WORD *pmy, WORD *pmb, WORD *pks)
{
    WORD status = 0;
    WORD x = 0;
    WORD y = 0;

    if (aes_ensure_vdi() != 0) {
        vq_mouse(aes_state.vdi_handle, &status, &x, &y);
    }
    if (pmx != NULL) {
        *pmx = x;
    }
    if (pmy != NULL) {
        *pmy = y;
    }
    if (pmb != NULL) {
        *pmb = status;
    }
    if (pks != NULL) {
        *pks = aes_state.key_state;
    }
}
