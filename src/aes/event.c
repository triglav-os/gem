/*
 * Implements hosted AES event polling and input routing: physical input
 * dispatch into per-application queues, the evnt_* entry points and the
 * graf_* mouse, box and slider helpers.
 *
 * MIT License (see: LICENSE)
 * Copyright (C) 2026 tomaz stih
 */

#include "aes_internal.h"
#include "system_menu.h"
#include "window_track.h"

#include "platform/os.h"

#include <stdint.h>
#include <string.h>
extern WORD vdi_select_system_mouse_form(WORD selector);

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
    WORD fallback = aes_state.input_focus_app_id != 0
                        ? aes_state.input_focus_app_id
                        : (aes_state.active_app_id != 0
                               ? aes_state.active_app_id
                               : aes_state.menu_owner_app_id);

    if (evt == NULL) {
        return 0;
    }
    if (aes_state.modal_form_owner_app_id != 0 &&
        aes_find_app_by_id(aes_state.modal_form_owner_app_id) != NULL) {
        return aes_state.modal_form_owner_app_id;
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
    WORD handle;

    if (evt == NULL || evt->type != GEM_HID_MOUSE_BUTTON ||
        (evt->flags & evt->button) == 0u) {
        return;
    }
    if (aes_state.modal_form_owner_app_id != 0) {
        aes_state.input_focus_app_id = aes_state.modal_form_owner_app_id;
        return;
    }

    handle = wind_find((WORD)evt->x, (WORD)evt->y);
    if (handle == 0) {
        /* Clicking the desktop background or an exposed desktop icon gives
         * keyboard focus to the desktop owner. Keep the visible menu bar on
         * the current top window, but route the next key events to Desktop so
         * icon actions such as Delete work after another window was moved. */
        if (aes_state.desktop_owner_app_id != 0) {
            aes_state.input_focus_app_id = aes_state.desktop_owner_app_id;
        }
        return;
    }

    window = aes_find_window(handle);
    if (window != NULL) {
        aes_state.input_focus_app_id = window->owner;
        aes_menu_switch_to_app(window->owner);
    }
}

static int aes_track_system_menu_event(const gem_hid_event_t *evt)
{
    if (aes_state.modal_form_owner_app_id != 0 || evt == NULL ||
        evt->type != GEM_HID_MOUSE_BUTTON ||
        evt->button != GEM_HID_BUTTON_LEFT ||
        (evt->flags & GEM_HID_BUTTON_LEFT) == 0u ||
        aes_system_menu_hit((WORD)evt->x, (WORD)evt->y) == 0) {
        return 0;
    }
    aes_store_mouse_state(evt);
    (void)aes_system_menu_track(evt);
    return 1;
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
        if (aes_system_delete_hook != NULL &&
            ((evt->key & 0xff) == 127 || ((evt->key >> 8) & 0xff) == 0x4c ||
             ((evt->key >> 8) & 0xff) == 0x53 ||
             ((evt->key >> 8) & 0xff) == 0x7f) &&
            aes_system_delete_hook() != 0) {
            return;
        }
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
            aes_window_t *window;
            int chrome;
            if (aes_track_system_menu_event(evt) != 0) {
                return;
            }
            window = aes_find_window(wind_find((WORD)evt->x, (WORD)evt->y));
            chrome = window != NULL &&
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
            if (aes_track_system_menu_event(&evt) != 0) {
                continue;
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
            WORD event_owner;

            if (!queued)
                aes_activate_input_target(&evt);
            if (!queued && aes_track_system_menu_event(&evt) != 0) {
                continue;
            }
            event_owner = aes_input_event_owner(&evt);

            if ((evt.type == GEM_HID_KEY || evt.type == GEM_HID_MOUSE_BUTTON) &&
                event_owner != 0 && event_owner != aes_state.current_app_id) {
                aes_queue_input_event(event_owner, &evt);
                continue;
            }
            if (!queued && evt.type == GEM_HID_KEY) {
                aes_store_key_state(&evt);
            }
            if (!queued && aes_state.modal_form_owner_app_id == 0 &&
                (flags & MU_MESAG) != 0u && mepbuff != NULL &&
                aes_state.menu_visible != 0 && aes_state.menu_tree != NULL &&
                aes_menu_key_event(aes_state.menu_tree, &evt, mepbuff) != 0) {
                graf_mkstate(pmx, pmy, pmb, pks);
                return MU_MESAG;
            }

            if (!queued && aes_state.modal_form_owner_app_id == 0 &&
                (flags & MU_MESAG) != 0u && mepbuff != NULL &&
                aes_state.menu_visible != 0 && aes_state.menu_tree != NULL &&
                aes_menu_event(aes_state.menu_tree, &evt, mepbuff) != 0) {
                graf_mkstate(pmx, pmy, pmb, pks);
                return MU_MESAG;
            }

            if (!queued && aes_state.modal_form_owner_app_id == 0 &&
                (flags & MU_MESAG) != 0u && mepbuff != NULL) {
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

        /*
         * In proxied sessions the wait hook can consume mouse-motion packets
         * while updating the shared pointer state.  MU_M1/MU_M2 are state
         * conditions, so evaluate them even when no packet remains queued for
         * this application.  This also matches AES enter/leave semantics when
         * the condition is already true on entry.
         */
        if ((flags & MU_M1) != 0u && evnt_mouse((WORD)m1flags, m1x, m1y, m1w,
                                                m1h, pmx, pmy, pmb, pks) != 0) {
            return MU_M1;
        }
        if ((flags & MU_M2) != 0u && evnt_mouse((WORD)m2flags, m2x, m2y, m2w,
                                                m2h, pmx, pmy, pmb, pks) != 0) {
            return MU_M2;
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
    GRECT outline;
    gem_hid_event_t event;
    WORD mouse_x;
    WORD mouse_y;
    WORD buttons;
    WORD keys;
    WORD offset_x;
    WORD offset_y;
    WORD maximum_x;
    WORD maximum_y;

    if (w <= 0 || h <= 0 || wc <= 0 || hc <= 0 || w > wc || h > hc ||
        aes_ensure_vdi() == 0) {
        return 0;
    }
    maximum_x = (WORD)(xc + wc - w);
    maximum_y = (WORD)(yc + hc - h);
    outline.g_x = aes_max_word(xc, aes_min_word(sx, maximum_x));
    outline.g_y = aes_max_word(yc, aes_min_word(sy, maximum_y));
    outline.g_w = w;
    outline.g_h = h;
    graf_mkstate(&mouse_x, &mouse_y, &buttons, &keys);
    offset_x = (WORD)(mouse_x - outline.g_x);
    offset_y = (WORD)(mouse_y - outline.g_y);
    if ((buttons & GEM_HID_BUTTON_LEFT) == 0) {
        if (pdx != NULL) {
            *pdx = outline.g_x;
        }
        if (pdy != NULL) {
            *pdy = outline.g_y;
        }
        return 1;
    }

    aes_begin_interaction_lock();
    v_hide_c(aes_state.vdi_handle);
    aes_draw_drag_outline(&outline);
    v_show_c(aes_state.vdi_handle, 1);

    FOREVER
    {
        if (aes_wait_hook != NULL && aes_wait_hook() == 0) {
            v_hide_c(aes_state.vdi_handle);
            aes_draw_drag_outline(&outline);
            v_show_c(aes_state.vdi_handle, 1);
            aes_end_interaction_lock();
            return 0;
        }
        if (gem_hid_poll(&event) == 0) {
            gem_os_sleep_ms(1u);
            continue;
        }
        if (event.type == GEM_HID_MOUSE_MOVE ||
            event.type == GEM_HID_MOUSE_BUTTON) {
            GRECT moved = outline;

            aes_store_mouse_state(&event);
            moved.g_x = aes_max_word(
                xc, aes_min_word((WORD)(event.x - offset_x), maximum_x));
            moved.g_y = aes_max_word(
                yc, aes_min_word((WORD)(event.y - offset_y), maximum_y));
            if (moved.g_x != outline.g_x || moved.g_y != outline.g_y) {
                v_hide_c(aes_state.vdi_handle);
                aes_draw_drag_outline(&outline);
                aes_draw_drag_outline(&moved);
                v_show_c(aes_state.vdi_handle, 1);
                outline = moved;
            }
        }
        if (event.type == GEM_HID_MOUSE_BUTTON &&
            event.button == GEM_HID_BUTTON_LEFT &&
            (event.flags & GEM_HID_BUTTON_LEFT) == 0u) {
            v_hide_c(aes_state.vdi_handle);
            aes_draw_drag_outline(&outline);
            v_show_c(aes_state.vdi_handle, 1);
            aes_end_interaction_lock();
            if (pdx != NULL) {
                *pdx = outline.g_x;
            }
            if (pdy != NULL) {
                *pdy = outline.g_y;
            }
            return 1;
        }
    }
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
