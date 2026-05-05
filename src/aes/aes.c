/*
 * Exposes the hosted AES public entry points used by the Linux GEM
 * port. The private runtime machinery lives in `_aes.c`, while this
 * file keeps the exported AES API readable and easier to debug.
 *
 * MIT License (see: LICENSE)
 * Copyright (C) 2026 tomaz stih
 */

#include "_aes.h"

#include "platform/os.h"

#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

WORD contrl[12] __attribute__((weak));
WORD intin[128] __attribute__((weak));
WORD ptsin[256] __attribute__((weak));
WORD intout[128] __attribute__((weak));
WORD ptsout[256] __attribute__((weak));

WORD global[15];

static void _aes_desktop_rect(GRECT *rect);
static int _aes_window_is_top(const aes_window_t *window);
static void _aes_queue_window_message(const aes_window_t *window,
    WORD message, WORD w4, WORD w5, WORD w6, WORD w7);
static WORD _aes_dequeue_message(WORD msg[8]);
static void _aes_draw_drag_outline(const GRECT *rect);
static int _aes_rects_intersect_local(const GRECT *left, const GRECT *right);
static int _aes_intersect_rects_local(const GRECT *left, const GRECT *right,
    GRECT *out);
static WORD _aes_subtract_rect_local(const GRECT *source, const GRECT *cover,
    GRECT out[4]);
static WORD _aes_build_visible_rects(WORD handle, GRECT out[], WORD max_rects);
static WORD _aes_track_window_interaction(const gem_hid_event_t *first_evt,
    WORD flags, WORD mepbuff[8], WORD *pmx, WORD *pmy, WORD *pmb, WORD *pks);

static void _aes_desktop_rect(GRECT *rect)
{
    if (rect == NULL) {
        return;
    }

    if (_aes_ensure_vdi() == 0) {
        _aes_set_rect(rect, 0, 0, 0, 0);
        return;
    }

    _aes_set_rect(rect, 0, 0, (WORD) (_aes.work_out[0] + 1),
        (WORD) (_aes.work_out[1] + 1));
}

static int _aes_window_is_top(const aes_window_t *window)
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

static aes_window_t *_aes_find_top_window(void)
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

static void _aes_queue_window_message(const aes_window_t *window,
    WORD message, WORD w4, WORD w5, WORD w6, WORD w7)
{
    WORD msg[8];

    if (window == NULL || window->owner == 0) {
        return;
    }

    msg[0] = message;
    msg[1] = _aes.current_app_id;
    msg[2] = 0;
    msg[3] = window->handle;
    msg[4] = w4;
    msg[5] = w5;
    msg[6] = w6;
    msg[7] = w7;
    (void) appl_write(window->owner, 8, msg);
}

static WORD _aes_dequeue_message(WORD msg[8])
{
    if (msg == NULL) {
        return 0;
    }

    return appl_read(_aes.current_app_id, 8, msg);
}

static void _aes_draw_drag_outline(const GRECT *rect)
{
    WORD box[10];
    WORD previous_mode;

    if (rect == NULL || rect->g_w <= 0 || rect->g_h <= 0 ||
        _aes_ensure_vdi() == 0) {
        return;
    }

    box[0] = rect->g_x;
    box[1] = rect->g_y;
    box[2] = (WORD) (rect->g_x + rect->g_w - 1);
    box[3] = box[1];
    box[4] = box[2];
    box[5] = (WORD) (rect->g_y + rect->g_h - 1);
    box[6] = box[0];
    box[7] = box[5];
    box[8] = box[0];
    box[9] = box[1];

    previous_mode = _vdi_write_mode();
    (void) vswr_mode(_aes.vdi_handle, MD_XOR);
    vsl_color(_aes.vdi_handle, WHITE);
    v_pline(_aes.vdi_handle, 5, box);
    (void) vswr_mode(_aes.vdi_handle, previous_mode);
}

static int _aes_rects_intersect_local(const GRECT *left, const GRECT *right)
{
    WORD left_right;
    WORD left_bottom;
    WORD right_right;
    WORD right_bottom;

    if (left == NULL || right == NULL || left->g_w <= 0 || left->g_h <= 0 ||
        right->g_w <= 0 || right->g_h <= 0) {
        return 0;
    }

    left_right = (WORD) (left->g_x + left->g_w - 1);
    left_bottom = (WORD) (left->g_y + left->g_h - 1);
    right_right = (WORD) (right->g_x + right->g_w - 1);
    right_bottom = (WORD) (right->g_y + right->g_h - 1);

    if (left_right < right->g_x || right_right < left->g_x ||
        left_bottom < right->g_y || right_bottom < left->g_y) {
        return 0;
    }
    return 1;
}

static int _aes_intersect_rects_local(const GRECT *left, const GRECT *right,
                                      GRECT *out)
{
    WORD left_right;
    WORD left_bottom;
    WORD right_right;
    WORD right_bottom;
    WORD x0;
    WORD y0;
    WORD x1;
    WORD y1;

    if (out == NULL || _aes_rects_intersect_local(left, right) == 0) {
        return 0;
    }

    left_right = (WORD) (left->g_x + left->g_w - 1);
    left_bottom = (WORD) (left->g_y + left->g_h - 1);
    right_right = (WORD) (right->g_x + right->g_w - 1);
    right_bottom = (WORD) (right->g_y + right->g_h - 1);

    x0 = _aes_max_word(left->g_x, right->g_x);
    y0 = _aes_max_word(left->g_y, right->g_y);
    x1 = _aes_min_word(left_right, right_right);
    y1 = _aes_min_word(left_bottom, right_bottom);
    _aes_set_rect(out, x0, y0, (WORD) (x1 - x0 + 1), (WORD) (y1 - y0 + 1));
    return 1;
}

static WORD _aes_subtract_rect_local(const GRECT *source, const GRECT *cover,
                                     GRECT out[4])
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
        _aes_intersect_rects_local(source, cover, &overlap) == 0) {
        out[0] = *source;
        return 1;
    }

    source_right = (WORD) (source->g_x + source->g_w - 1);
    source_bottom = (WORD) (source->g_y + source->g_h - 1);
    overlap_right = (WORD) (overlap.g_x + overlap.g_w - 1);
    overlap_bottom = (WORD) (overlap.g_y + overlap.g_h - 1);

    if (source->g_y < overlap.g_y) {
        _aes_set_rect(&out[count++], source->g_x, source->g_y, source->g_w,
            (WORD) (overlap.g_y - source->g_y));
    }
    if (overlap_bottom < source_bottom) {
        _aes_set_rect(&out[count++], source->g_x, (WORD) (overlap_bottom + 1),
            source->g_w, (WORD) (source_bottom - overlap_bottom));
    }
    if (source->g_x < overlap.g_x) {
        _aes_set_rect(&out[count++], source->g_x, overlap.g_y,
            (WORD) (overlap.g_x - source->g_x), overlap.g_h);
    }
    if (overlap_right < source_right) {
        _aes_set_rect(&out[count++], (WORD) (overlap_right + 1), overlap.g_y,
            (WORD) (source_right - overlap_right), overlap.g_h);
    }

    return count;
}

static WORD _aes_build_visible_rects(WORD handle, GRECT out[], WORD max_rects)
{
    GRECT pending[64];
    GRECT next_pending[64];
    GRECT base;
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
        base = window->work;
    }

    if (base.g_w <= 0 || base.g_h <= 0) {
        return 0;
    }

    pending[0] = base;
    pending_count = 1;

    for (i = 0; i < AES_MAX_WINDOWS && pending_count > 0; ++i) {
        const aes_window_t *cover = &_aes.windows[i];
        WORD next_count = 0;
        WORD j;

        if (cover->used == 0 || cover->open == 0) {
            continue;
        }
        if (handle != 0 &&
            (cover->handle == handle ||
            cover->z_order <= _aes_find_window(handle)->z_order)) {
            continue;
        }

        for (j = 0; j < pending_count; ++j) {
            GRECT fragments[4];
            WORD fragment_count = _aes_subtract_rect_local(&pending[j],
                &cover->outer, fragments);
            WORD k;

            for (k = 0; k < fragment_count && next_count < max_rects; ++k) {
                next_pending[next_count++] = fragments[k];
            }
        }

        memcpy(pending, next_pending, (size_t) next_count * sizeof(GRECT));
        pending_count = next_count;
    }

    memcpy(out, pending, (size_t) pending_count * sizeof(GRECT));
    return pending_count;
}

static WORD _aes_track_window_interaction(const gem_hid_event_t *first_evt,
    WORD flags, WORD mepbuff[8], WORD *pmx, WORD *pmy, WORD *pmb, WORD *pks)
{
    aes_window_t *window;
    GRECT desktop;
    WORD handle;
    WORD part;
    WORD start_x;
    WORD start_y;
    WORD raised;

    if (first_evt == NULL || first_evt->type != GEM_HID_MOUSE_BUTTON ||
        first_evt->button != GEM_HID_BUTTON_LEFT ||
        (first_evt->flags & GEM_HID_BUTTON_LEFT) == 0u) {
        return 0;
    }

    handle = wind_find((WORD) first_evt->x, (WORD) first_evt->y);
    if (handle == 0) {
        return 0;
    }

    window = _aes_find_window(handle);
    if (window == NULL) {
        return 0;
    }

    raised = (_aes_window_is_top(window) == 0) ? 1 : 0;
    if (raised != 0) {
        aes_window_t *previous_top = _aes_find_top_window();
        _aes_raise_window(window);
        _aes_redraw_window_title_states(previous_top, window);
    }

    part = _aes_window_hit_part(window, (WORD) first_evt->x, (WORD) first_evt->y);
    start_x = (WORD) first_evt->x;
    start_y = (WORD) first_evt->y;

    if (part == AES_WINDOW_PART_CLOSER) {
        gem_hid_event_t evt;

        if (_aes_point_in_rect(start_x, start_y, &window->outer) == 0) {
            return 0;
        }

        if (_aes_window_hit_part(window, start_x, start_y) != AES_WINDOW_PART_CLOSER) {
            return 0;
        }

        FOREVER {
            if (gem_hid_poll(&evt) == 0) {
                gem_os_sleep_ms(1u);
                continue;
            }
            if (evt.type == GEM_HID_MOUSE_MOVE ||
                evt.type == GEM_HID_MOUSE_BUTTON) {
                _aes_store_mouse_state(&evt);
                graf_mkstate(pmx, pmy, pmb, pks);
            }
            if (evt.type == GEM_HID_MOUSE_BUTTON &&
                evt.button == GEM_HID_BUTTON_LEFT &&
                (evt.flags & GEM_HID_BUTTON_LEFT) == 0u) {
                if (_aes_window_hit_part(window, (WORD) evt.x,
                    (WORD) evt.y) == AES_WINDOW_PART_CLOSER) {
                    _aes_queue_window_message(window, WM_CLOSED, 0, 0, 0, 0);
                    if ((flags & MU_MESAG) != 0u && mepbuff != NULL &&
                        _aes_dequeue_message(mepbuff) != 0) {
                        return MU_MESAG;
                    }
                }
                return 0;
            }
        }
    }

    if (part == AES_WINDOW_PART_FULLER) {
        _aes_queue_window_message(window, WM_FULLED, 0, 0, 0, 0);
        if ((flags & MU_MESAG) != 0u && mepbuff != NULL &&
            _aes_dequeue_message(mepbuff) != 0) {
            return MU_MESAG;
        }
        return 0;
    }

    if (part == AES_WINDOW_PART_TITLE && (window->kind & MOVER) != 0u) {
        gem_hid_event_t evt;
        GRECT original = window->outer;
        GRECT drag_rect = original;
        GRECT last_rect = original;
        int drag_drawn = 0;

        _aes_desktop_rect(&desktop);
        v_hide_c(_aes.vdi_handle);
        _aes_draw_drag_outline(&drag_rect);
        v_show_c(_aes.vdi_handle, 1);
        drag_drawn = 1;

        FOREVER {
            if (gem_hid_poll(&evt) == 0) {
                gem_os_sleep_ms(1u);
                continue;
            }
            if (evt.type == GEM_HID_MOUSE_MOVE ||
                evt.type == GEM_HID_MOUSE_BUTTON) {
                WORD new_x;
                WORD new_y;

                v_hide_c(_aes.vdi_handle);
                _aes_store_mouse_state(&evt);

                new_x = (WORD) (original.g_x + evt.x - start_x);
                new_y = (WORD) (original.g_y + evt.y - start_y);
                new_x = _aes_max_word(desktop.g_x, _aes_min_word(new_x,
                    (WORD) (desktop.g_x + desktop.g_w - window->outer.g_w)));
                new_y = _aes_max_word(desktop.g_y, _aes_min_word(new_y,
                    (WORD) (desktop.g_y + desktop.g_h - window->outer.g_h)));
                if (new_x != drag_rect.g_x || new_y != drag_rect.g_y) {
                    if (drag_drawn != 0) {
                        _aes_draw_drag_outline(&last_rect);
                    }
                    _aes_set_rect(&drag_rect, new_x, new_y, original.g_w,
                        original.g_h);
                    _aes_draw_drag_outline(&drag_rect);
                    last_rect = drag_rect;
                    drag_drawn = 1;
                }
                v_show_c(_aes.vdi_handle, 1);
                graf_mkstate(pmx, pmy, pmb, pks);
            }
            if (evt.type == GEM_HID_MOUSE_BUTTON &&
                evt.button == GEM_HID_BUTTON_LEFT &&
                (evt.flags & GEM_HID_BUTTON_LEFT) == 0u) {
                v_hide_c(_aes.vdi_handle);
                _aes_store_mouse_state(&evt);
                if (drag_drawn != 0) {
                    _aes_draw_drag_outline(&last_rect);
                }
                v_show_c(_aes.vdi_handle, 1);
                _aes_queue_window_message(window, WM_MOVED, drag_rect.g_x,
                    drag_rect.g_y, drag_rect.g_w, drag_rect.g_h);
                if ((flags & MU_MESAG) != 0u && mepbuff != NULL &&
                    _aes_dequeue_message(mepbuff) != 0) {
                    return MU_MESAG;
                }
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

        _aes_desktop_rect(&desktop);
        v_hide_c(_aes.vdi_handle);
        _aes_draw_drag_outline(&drag_rect);
        v_show_c(_aes.vdi_handle, 1);
        drag_drawn = 1;

        FOREVER {
            if (gem_hid_poll(&evt) == 0) {
                gem_os_sleep_ms(1u);
                continue;
            }
            if (evt.type == GEM_HID_MOUSE_MOVE ||
                evt.type == GEM_HID_MOUSE_BUTTON) {
                WORD new_w;
                WORD new_h;

                v_hide_c(_aes.vdi_handle);
                _aes_store_mouse_state(&evt);
                new_w = (WORD) (original.g_w + evt.x - start_x);
                new_h = (WORD) (original.g_h + evt.y - start_y);
                new_w = _aes_max_word(96, _aes_min_word(new_w,
                    (WORD) (desktop.g_x + desktop.g_w - original.g_x)));
                new_h = _aes_max_word(64, _aes_min_word(new_h,
                    (WORD) (desktop.g_y + desktop.g_h - original.g_y)));
                if (new_w != drag_rect.g_w || new_h != drag_rect.g_h) {
                    if (drag_drawn != 0) {
                        _aes_draw_drag_outline(&last_rect);
                    }
                    _aes_set_rect(&drag_rect, original.g_x, original.g_y,
                        new_w, new_h);
                    _aes_draw_drag_outline(&drag_rect);
                    last_rect = drag_rect;
                    drag_drawn = 1;
                }
                v_show_c(_aes.vdi_handle, 1);
                graf_mkstate(pmx, pmy, pmb, pks);
            }
            if (evt.type == GEM_HID_MOUSE_BUTTON &&
                evt.button == GEM_HID_BUTTON_LEFT &&
                (evt.flags & GEM_HID_BUTTON_LEFT) == 0u) {
                v_hide_c(_aes.vdi_handle);
                _aes_store_mouse_state(&evt);
                if (drag_drawn != 0) {
                    _aes_draw_drag_outline(&last_rect);
                }
                v_show_c(_aes.vdi_handle, 1);
                _aes_queue_window_message(window, WM_SIZED, drag_rect.g_x,
                    drag_rect.g_y, drag_rect.g_w, drag_rect.g_h);
                if ((flags & MU_MESAG) != 0u && mepbuff != NULL &&
                    _aes_dequeue_message(mepbuff) != 0) {
                    return MU_MESAG;
                }
                return 0;
            }
        }
    }

    if (raised != 0) {
        _aes_queue_window_message(window, WM_TOPPED, 0, 0, 0, 0);
        if ((flags & MU_MESAG) != 0u && mepbuff != NULL &&
            _aes_dequeue_message(mepbuff) != 0) {
            return MU_MESAG;
        }
    }
    return 0;
}

/*
 * Allocates one hosted AES application slot for the current caller and
 * lazily initializes the shared runtime state on first use.
 */
WORD appl_init(void)
{
    size_t i;
    FILE *trace_fp;

    trace_fp = fopen("/tmp/gem_appl_init_seen.log", "a");
    if (trace_fp != NULL) {
        fputs("appl_init\n", trace_fp);
        fclose(trace_fp);
    }

    if (_aes.initialized == 0) {
        _aes_reset_state();
        _aes.initialized = 1;
    }

    for (i = 0; i < AES_MAX_APPS; ++i) {
        if (_aes.apps[i].used == 0) {
            _aes.apps[i].used = 1;
            _aes.apps[i].id = _aes.next_app_id++;
            strcpy(_aes.apps[i].name, "APP");
            _aes.current_app_id = _aes.apps[i].id;
            global[2] = _aes.current_app_id;
            trace_fp = fopen("/tmp/gem_appl_init_seen.log", "a");
            if (trace_fp != NULL) {
                fputs("appl_init_return\n", trace_fp);
                fclose(trace_fp);
            }
            _aes_trace("appl_init return id=%d", _aes.current_app_id);
            return _aes.current_app_id;
        }
    }
    _aes_trace("appl_init failed");
    return 0;
}

/*
 * Releases the current AES application slot and closes the hosted VDI
 * workstation when the last active application exits.
 */
WORD appl_exit(void)
{
    aes_app_t *app = _aes_find_app_by_id(_aes.current_app_id);

    if (app != NULL) {
        memset(app, 0, sizeof(*app));
    }
    if (_aes.vdi_ready != 0 && _aes.vdi_handle != 0) {
        v_clsvwk(_aes.vdi_handle);
        _aes.vdi_handle = 0;
        _aes.vdi_ready = 0;
    }
    _aes.current_app_id = 0;
    global[2] = 0;
    return 1;
}

/*
 * Looks up an AES application id by its registered short name.
 */
WORD appl_find(char *name)
{
    size_t i;

    if (name == NULL) {
        return 0;
    }

    for (i = 0; i < AES_MAX_APPS; ++i) {
        if (_aes.apps[i].used != 0 &&
            strncmp(_aes.apps[i].name, name, 8) == 0) {
            return _aes.apps[i].id;
        }
    }
    return 0;
}

/*
 * Stores the desktop boot-volume bitmasks reported by the legacy
 * desktop shell.
 */
WORD appl_bvset(WORD bvdisk, WORD bvhard)
{
    _aes.desktop_bvdisk = bvdisk;
    _aes.desktop_bvhard = bvhard;
    return 1;
}

/*
 * Queues one 8-word AES message for another hosted application.
 */
WORD appl_write(WORD ap_wid, WORD ap_wlength, void *ap_wpbuff)
{
    size_t i;
    WORD *words = (WORD *) ap_wpbuff;

    if (ap_wpbuff == NULL || ap_wlength < 8) {
        return 0;
    }

    for (i = 0; i < AES_MAX_MESSAGES; ++i) {
        if (_aes.messages[i].used == 0) {
            _aes.messages[i].used = 1;
            _aes.messages[i].dest = ap_wid;
            memcpy(_aes.messages[i].data, words,
                sizeof(_aes.messages[i].data));
            return 1;
        }
    }
    return 0;
}

/*
 * Dequeues the next pending AES message for one hosted application id.
 */
WORD appl_read(WORD ap_rwid, WORD ap_rlength, void *ap_rpbuff)
{
    size_t i;

    if (ap_rpbuff == NULL || ap_rlength < 8) {
        return 0;
    }

    for (i = 0; i < AES_MAX_MESSAGES; ++i) {
        if (_aes.messages[i].used != 0 &&
            _aes.messages[i].dest == ap_rwid) {
            memcpy(ap_rpbuff, _aes.messages[i].data,
                sizeof(_aes.messages[i].data));
            _aes.messages[i].used = 0;
            return 1;
        }
    }
    return 0;
}

/*
 * Accepts a tape-play request and reports success without emulating the
 * historical playback stream.
 */
WORD appl_tplay(void *pbuff, WORD length, WORD scale)
{
    (void) pbuff;
    (void) length;
    (void) scale;
    return 1;
}

/*
 * Accepts a tape-record request and clears the caller's buffer.
 */
WORD appl_trecord(void *pbuff, WORD length)
{
    if (pbuff != NULL && length > 0) {
        memset(pbuff, 0, (size_t) length);
    }
    return 1;
}

/*
 * Yields briefly to the host so polling loops can stay responsive.
 */
WORD appl_yield(void)
{
    gem_os_sleep_ms(1u);
    return 1;
}

/*
 * Blocks until the next key-down event and returns its hosted key code.
 */
WORD evnt_keybd(void)
{
    gem_hid_event_t evt;

    FOREVER {
        if (gem_hid_poll(&evt) != 0) {
            if (evt.type == GEM_HID_MOUSE_MOVE ||
                evt.type == GEM_HID_MOUSE_BUTTON) {
                _aes_store_mouse_state(&evt);
                continue;
            }
            if (evt.type == GEM_HID_KEY && (evt.flags & 1u) != 0u) {
                return (WORD) evt.key;
            }
        }
        gem_os_sleep_ms(1u);
    }
}

/*
 * Samples the current mouse-button state and compares it against the
 * requested mask and state.
 */
WORD evnt_button(WORD clicks,
                 UWORD mask,
                 UWORD state,
                 WORD *pmx,
                 WORD *pmy,
                 WORD *pmb,
                 WORD *pks)
{
    WORD buttons;

    (void) clicks;

    graf_mkstate(pmx, pmy, &buttons, pks);
    if (pmb != NULL) {
        *pmb = buttons;
    }
    return (((UWORD) buttons & mask) == state) ? 1 : 0;
}

/*
 * Tests whether the current mouse position is inside or outside one
 * rectangle and returns the result according to the AES flag contract.
 */
WORD evnt_mouse(WORD flags,
                WORD x,
                WORD y,
                WORD w,
                WORD h,
                WORD *pmx,
                WORD *pmy,
                WORD *pmb,
                WORD *pks)
{
    WORD mx;
    WORD my;
    WORD mb;
    WORD ks;
    GRECT rect;
    int inside;

    graf_mkstate(&mx, &my, &mb, &ks);
    _aes_set_rect(&rect, x, y, w, h);
    inside = _aes_point_in_rect(mx, my, &rect);
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

/*
 * Fetches the next queued AES message for the current application.
 */
WORD evnt_mesag(WORD msg[8])
{
    FOREVER {
        gem_hid_event_t evt;

        if (_aes_dequeue_message(msg) != 0) {
            return 1;
        }

        if (gem_hid_poll(&evt) != 0) {
            if (_aes.menu_visible != 0 && _aes.menu_tree != NULL &&
                _aes_menu_event(_aes.menu_tree, &evt, msg) != 0) {
                return 1;
            }

            if (_aes_track_window_interaction(&evt, MU_MESAG, msg,
                NULL, NULL, NULL, NULL) == MU_MESAG) {
                return 1;
            }

            if (evt.type == GEM_HID_MOUSE_MOVE ||
                evt.type == GEM_HID_MOUSE_BUTTON) {
                _aes_store_mouse_state(&evt);
            }
        }

        gem_os_sleep_ms(1u);
    }
}

/*
 * Sleeps for the requested 32-bit AES timer duration.
 */
WORD evnt_timer(WORD count_low, WORD count_high)
{
    uint32_t duration = (uint32_t) (uint16_t) count_low |
        ((uint32_t) (uint16_t) count_high << 16);

    gem_os_sleep_ms(duration);
    return 1;
}

/*
 * Waits for the next hosted event that matches the requested AES event
 * mask, including menu interactions synthesized as `MU_MESAG`.
 */
WORD evnt_multi(UWORD flags,
                UWORD bclk,
                UWORD bmsk,
                UWORD bst,
                UWORD m1flags,
                WORD m1x,
                WORD m1y,
                WORD m1w,
                WORD m1h,
                UWORD m2flags,
                WORD m2x,
                WORD m2y,
                WORD m2w,
                WORD m2h,
                WORD mepbuff[8],
                UWORD tlc,
                UWORD thc,
                WORD *pmx,
                WORD *pmy,
                WORD *pmb,
                WORD *pks,
                WORD *pkr,
                WORD *pbr)
{
    uint32_t timeout = (uint32_t) tlc | ((uint32_t) thc << 16);
    uint32_t start = gem_os_ticks_ms();

    (void) bclk;

    FOREVER {
        gem_hid_event_t evt;
        WORD mx = 0;
        WORD my = 0;
        WORD mb = 0;
        WORD ks = 0;

        if ((flags & MU_MESAG) != 0u && mepbuff != NULL &&
            _aes_dequeue_message(mepbuff) != 0) {
            return MU_MESAG;
        }

        if (gem_hid_poll(&evt) != 0) {
            if ((flags & MU_MESAG) != 0u && mepbuff != NULL &&
                _aes.menu_visible != 0 && _aes.menu_tree != NULL &&
                _aes_menu_event(_aes.menu_tree, &evt, mepbuff) != 0) {
                graf_mkstate(pmx, pmy, pmb, pks);
                return MU_MESAG;
            }

            if ((flags & MU_MESAG) != 0u && mepbuff != NULL) {
                WORD window_event = _aes_track_window_interaction(&evt, flags,
                    mepbuff, pmx, pmy, pmb, pks);

                if (window_event != 0) {
                    return window_event;
                }
            }

            if (evt.type == GEM_HID_MOUSE_MOVE ||
                evt.type == GEM_HID_MOUSE_BUTTON) {
                _aes_store_mouse_state(&evt);
                graf_mkstate(&mx, &my, &mb, &ks);
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
                    (((UWORD) mb & bmsk) == bst)) {
                    if (pbr != NULL) {
                        *pbr = 1;
                    }
                    return MU_BUTTON;
                }
                if ((flags & MU_M1) != 0u &&
                    evnt_mouse((WORD) m1flags, m1x, m1y, m1w, m1h,
                        pmx, pmy, pmb, pks) != 0) {
                    return MU_M1;
                }
                if ((flags & MU_M2) != 0u &&
                    evnt_mouse((WORD) m2flags, m2x, m2y, m2w, m2h,
                        pmx, pmy, pmb, pks) != 0) {
                    return MU_M2;
                }
                continue;
            }

            if ((flags & MU_KEYBD) != 0u &&
                evt.type == GEM_HID_KEY &&
                (evt.flags & 1u) != 0u) {
                if (pkr != NULL) {
                    *pkr = (WORD) evt.key;
                }
                graf_mkstate(pmx, pmy, pmb, pks);
                return MU_KEYBD;
            }
        }

        if ((flags & MU_TIMER) != 0u) {
            uint32_t now = gem_os_ticks_ms();

            if ((uint32_t) (now - start) >= timeout) {
                graf_mkstate(pmx, pmy, pmb, pks);
                return MU_TIMER;
            }
        }

        gem_os_sleep_ms(1u);
    }
}

/*
 * Gets or sets the desktop double-click preference value.
 */
WORD evnt_dclick(WORD clicks, WORD setget)
{
    if (setget != 0) {
        _aes.dclick_rate = clicks;
    }
    return _aes.dclick_rate;
}

/*
 * Registers or unregisters the active menu tree with the hosted AES.
 */
WORD menu_bar(OBJECT *tree, WORD show)
{
    _aes.menu_tree = tree;
    _aes.menu_visible = (show != 0) ? 1 : 0;
    if (show != 0 && tree != NULL) {
        _aes_menu_prepare_tree(tree);
        _aes_menu_hide_popups(tree);
        _aes_menu_redraw_tree(tree);
    } else {
        _aes_menu_clear_saved_region();
    }
    _aes_trace("menu_bar tree=%p show=%d", (void *) tree, show);
    return 1;
}

/*
 * Sets or clears the checkmark state on one menu item.
 */
WORD menu_icheck(OBJECT *tree, WORD item, WORD check)
{
    if (tree == NULL || item < 0) {
        return 0;
    }
    if (check != 0) {
        tree[item].ob_state |= CHECKED;
    } else {
        tree[item].ob_state &= (UWORD) ~CHECKED;
    }
    return 1;
}

/*
 * Enables or disables one menu item.
 */
WORD menu_ienable(OBJECT *tree, WORD item, WORD enable)
{
    if (tree == NULL || item < 0) {
        return 0;
    }
    if (enable != 0) {
        tree[item].ob_state &= (UWORD) ~DISABLED;
    } else {
        tree[item].ob_state |= DISABLED;
    }
    return 1;
}

/*
 * Sets or clears the selected state on one menu title.
 */
WORD menu_tnormal(OBJECT *tree, WORD title, WORD normal)
{
    if (tree == NULL || title < 0) {
        return 0;
    }
    if (normal != 0) {
        tree[title].ob_state &= (UWORD) ~SELECTED;
    } else {
        tree[title].ob_state |= SELECTED;
    }
    return 1;
}

/*
 * Replaces one menu item's text pointer.
 */
WORD menu_text(OBJECT *tree, WORD item, char *text)
{
    if (tree == NULL || item < 0) {
        return 0;
    }
    tree[item].ob_spec = (LONG) (intptr_t) text;
    return 1;
}

/*
 * Stores a short display name for one hosted AES application.
 */
WORD menu_register(WORD apid, char *name)
{
    aes_app_t *app = _aes_find_app_by_id(apid);

    if (app == NULL || name == NULL) {
        return 0;
    }
    strncpy(app->name, name, sizeof(app->name) - 1u);
    app->name[sizeof(app->name) - 1u] = '\0';
    return apid;
}

/*
 * Reports whether a registered menu owner id still exists.
 */
WORD menu_unregister(WORD mid)
{
    return (_aes_find_app_by_id(mid) != NULL) ? 1 : 0;
}

/*
 * Gets or sets the hosted "menu click" preference.
 */
WORD menu_click(WORD click, WORD setit)
{
    if (setit != 0) {
        _aes.menu_click = click;
    }
    return _aes.menu_click;
}

/*
 * Appends one child object to a parent's child list.
 */
WORD objc_add(OBJECT *tree, WORD parent, WORD child)
{
    WORD last;

    if (tree == NULL || parent < 0 || child < 0) {
        return 0;
    }

    if (tree[parent].ob_head == NIL) {
        tree[parent].ob_head = child;
    } else {
        last = tree[parent].ob_tail;
        tree[last].ob_next = child;
    }
    tree[parent].ob_tail = child;
    tree[child].ob_next = parent;
    return 1;
}

/*
 * Removes one object from its parent's child list.
 */
WORD objc_delete(OBJECT *tree, WORD object)
{
    size_t i;

    if (tree == NULL || object < 0) {
        return 0;
    }

    for (i = 0; i < 1024u; ++i) {
        if (tree[i].ob_head == object) {
            tree[i].ob_head = tree[object].ob_next;
            if (tree[i].ob_tail == object) {
                tree[i].ob_tail = NIL;
            }
            break;
        }
        if (tree[i].ob_head != NIL) {
            WORD node = tree[i].ob_head;

            while (node != NIL && node != (WORD) i) {
                if (tree[node].ob_next == object) {
                    tree[node].ob_next = tree[object].ob_next;
                    if (tree[i].ob_tail == object) {
                        tree[i].ob_tail = node;
                    }
                    break;
                }
                node = tree[node].ob_next;
            }
        }
    }
    tree[object].ob_next = NIL;
    return 1;
}

/*
 * Draws an object subtree clipped to the requested rectangle.
 */
WORD objc_draw(OBJECT *tree,
               WORD startob,
               WORD depth,
               WORD xc,
               WORD yc,
               WORD wc,
               WORD hc)
{
    WORD clip[4];

    (void) depth;

    if (tree == NULL || startob < 0 || _aes_ensure_vdi() == 0) {
        _aes_trace("objc_draw skipped tree=%p start=%d vdi=%d",
            (void *) tree, startob, _aes.vdi_ready);
        return 0;
    }

    _aes_trace("objc_draw tree=%p start=%d depth=%d clip=%d,%d %dx%d type=%u",
        (void *) tree, startob, depth, xc, yc, wc, hc,
        (unsigned) tree[startob].ob_type);

    clip[0] = xc;
    clip[1] = yc;
    clip[2] = (WORD) (xc + wc - 1);
    clip[3] = (WORD) (yc + hc - 1);
    vs_clip(_aes.vdi_handle, 1, clip);
    _aes_draw_tree_recursive(tree, startob, 0, 0, depth, clip);
    vs_clip(_aes.vdi_handle, 0, clip);
    return 1;
}

/*
 * Returns the deepest object hit within one subtree.
 */
WORD objc_find(OBJECT *tree, WORD startob, WORD depth, WORD mx, WORD my)
{
    if (tree == NULL || startob < 0) {
        return NIL;
    }

    return _aes_find_in_subtree(tree, startob, 0, 0, depth, mx, my);
}

/*
 * Computes the absolute coordinates of one object.
 */
WORD objc_offset(OBJECT *tree, WORD object, WORD *x, WORD *y)
{
    if (tree == NULL || object < 0) {
        return 0;
    }

    _aes_object_extent(tree, object, x, y);
    return 1;
}

/*
 * Accepts an object-reordering request but currently leaves the object
 * list unchanged.
 */
WORD objc_order(OBJECT *tree, WORD object, WORD newpos)
{
    (void) tree;
    (void) object;
    (void) newpos;
    return 1;
}

/*
 * Appends one printable character into a hosted editable text field.
 */
WORD objc_edit(OBJECT *tree, WORD object, WORD charidx, WORD *idx, WORD kind)
{
    TEDINFO *ted;
    char *text;
    size_t length;

    if (tree == NULL || object < 0 || idx == NULL) {
        return 0;
    }
    if (kind != EDCHAR || tree[object].ob_spec == 0) {
        return 1;
    }

    ted = (TEDINFO *) (intptr_t) tree[object].ob_spec;
    text = (char *) (intptr_t) ted->te_ptext;
    if (text == NULL) {
        return 0;
    }

    length = strlen(text);
    if (*idx < ted->te_txtlen - 1 && charidx >= 32 && charidx <= 126) {
        text[*idx] = (char) charidx;
        if ((size_t) *idx >= length) {
            text[*idx + 1] = '\0';
        }
        ++(*idx);
    }
    return 1;
}

/*
 * Changes one object's state and optionally redraws it.
 */
WORD objc_change(OBJECT *tree,
                 WORD object,
                 WORD depth,
                 WORD xc,
                 WORD yc,
                 WORD wc,
                 WORD hc,
                 WORD newstate,
                 WORD redraw)
{
    if (tree == NULL || object < 0) {
        return 0;
    }

    (void) depth;
    tree[object].ob_state = (UWORD) newstate;
    if (redraw != 0) {
        return objc_draw(tree, object, 0, xc, yc, wc, hc);
    }
    return 1;
}

/*
 * Starts a form interaction and currently returns the starting object.
 */
WORD form_do(OBJECT *tree, WORD startob)
{
    if (tree == NULL) {
        return 0;
    }

    return startob;
}

/*
 * Accepts a form-dial animation request without animating on the host.
 */
WORD form_dial(WORD flag,
               WORD x1,
               WORD y1,
               WORD w1,
               WORD h1,
               WORD x2,
               WORD y2,
               WORD w2,
               WORD h2)
{
    (void) flag;
    (void) x1;
    (void) y1;
    (void) w1;
    (void) h1;
    (void) x2;
    (void) y2;
    (void) w2;
    (void) h2;
    return 1;
}

/*
 * Displays an alert in the most minimal hosted form by returning the
 * default button.
 */
WORD form_alert(WORD defbtn, char *str)
{
    (void) str;
    return (defbtn > 0) ? defbtn : 1;
}

/*
 * Converts an AES form-error request into a simple alert return value.
 */
WORD form_error(WORD errnum)
{
    return form_alert(1, NULL) + errnum - errnum;
}

/*
 * Centers one form on the hosted desktop work area.
 */
WORD form_center(OBJECT *tree, WORD *cx, WORD *cy, WORD *cw, WORD *ch)
{
    WORD width = 200;
    WORD height = 100;

    if (tree != NULL) {
        width = tree[ROOT].ob_width;
        height = tree[ROOT].ob_height;
    }
    if (_aes_ensure_vdi() != 0) {
        if (cx != NULL) {
            *cx = (WORD) ((_aes.work_out[0] + 1 - width) / 2);
        }
        if (cy != NULL) {
            *cy = (WORD) ((_aes.work_out[1] + 1 - height) / 2);
        }
    } else {
        if (cx != NULL) {
            *cx = 0;
        }
        if (cy != NULL) {
            *cy = 0;
        }
    }
    if (cw != NULL) {
        *cw = width;
    }
    if (ch != NULL) {
        *ch = height;
    }
    return 1;
}

/*
 * Passes one key event through the hosted form keyboard helper.
 */
WORD form_keybd(OBJECT *tree,
                WORD object,
                WORD next,
                WORD thechar,
                WORD *newobj,
                WORD *newchar)
{
    (void) tree;
    if (newobj != NULL) {
        *newobj = next;
    }
    if (newchar != NULL) {
        *newchar = thechar;
    }
    return object;
}

/*
 * Toggles the selected state on a form button object.
 */
WORD form_button(OBJECT *tree, WORD object, WORD clicks, WORD *newobj)
{
    (void) clicks;

    if (tree == NULL || object < 0) {
        return 0;
    }
    tree[object].ob_state ^= SELECTED;
    if (newobj != NULL) {
        *newobj = object;
    }
    return 1;
}

/*
 * Accepts a rubber-box request and reports the requested minimum size.
 */
WORD graf_rubbox(WORD xorigin,
                 WORD yorigin,
                 WORD wmin,
                 WORD hmin,
                 WORD *pwend,
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

/*
 * Constrains a dragged box inside a bounding rectangle.
 */
WORD graf_dragbox(WORD w,
                  WORD h,
                  WORD sx,
                  WORD sy,
                  WORD xc,
                  WORD yc,
                  WORD wc,
                  WORD hc,
                  WORD *pdx,
                  WORD *pdy)
{
    if (pdx != NULL) {
        *pdx = _aes_max_word(xc, _aes_min_word(sx, (WORD) (xc + wc - w)));
    }
    if (pdy != NULL) {
        *pdy = _aes_max_word(yc, _aes_min_word(sy, (WORD) (yc + hc - h)));
    }
    return 1;
}

/*
 * Accepts a grow-box copy request without animating it.
 */
WORD graf_mbox(WORD w, WORD h, WORD srcx, WORD srcy, WORD dstx, WORD dsty)
{
    (void) w;
    (void) h;
    (void) srcx;
    (void) srcy;
    (void) dstx;
    (void) dsty;
    return 1;
}

/*
 * Accepts a grow-box animation request without animating it.
 */
WORD graf_growbox(WORD x1,
                  WORD y1,
                  WORD w1,
                  WORD h1,
                  WORD x2,
                  WORD y2,
                  WORD w2,
                  WORD h2)
{
    (void) x1;
    (void) y1;
    (void) w1;
    (void) h1;
    (void) x2;
    (void) y2;
    (void) w2;
    (void) h2;
    return 1;
}

/*
 * Reuses the grow-box stub for shrink-box requests.
 */
WORD graf_shrinkbox(WORD x1,
                    WORD y1,
                    WORD w1,
                    WORD h1,
                    WORD x2,
                    WORD y2,
                    WORD w2,
                    WORD h2)
{
    return graf_growbox(x1, y1, w1, h1, x2, y2, w2, h2);
}

/*
 * Simulates a watch-box transition by storing the requested in/out
 * states sequentially.
 */
WORD graf_watchbox(OBJECT *tree, WORD object, UWORD in_state, UWORD out_state)
{
    if (tree == NULL || object < 0) {
        return 0;
    }
    tree[object].ob_state = in_state;
    tree[object].ob_state = out_state;
    return 1;
}

/*
 * Returns a scrollbar slider position scaled into the historical
 * 0..1000 AES range.
 */
WORD graf_slidebox(OBJECT *tree, WORD parent, WORD object, WORD orientation)
{
    OBJECT *parent_obj;
    OBJECT *child_obj;

    (void) orientation;

    if (tree == NULL || parent < 0 || object < 0) {
        return 0;
    }

    parent_obj = &tree[parent];
    child_obj = &tree[object];
    if (parent_obj->ob_height == 0) {
        return 0;
    }
    return (WORD) ((1000L * child_obj->ob_y) /
        _aes_max_word(parent_obj->ob_height, 1));
}

/*
 * Returns the hosted character and box metrics together with the VDI
 * handle.
 */
WORD graf_handle(WORD *charw, WORD *charh, WORD *boxw, WORD *boxh)
{
    if (_aes_ensure_vdi() == 0) {
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
        *boxh = 16;
    }
    return _aes.vdi_handle;
}

/*
 * Shows, hides, or replaces the hosted mouse form.
 */
WORD graf_mouse(WORD mode, void *form)
{
    if (_aes_ensure_vdi() == 0) {
        return 0;
    }

    if (mode == M_OFF) {
        v_hide_c(_aes.vdi_handle);
    } else if (mode == M_ON) {
        v_show_c(_aes.vdi_handle, 1);
    } else if (mode == USER_DEF && form != NULL) {
        (void) vsc_form(_aes.vdi_handle, (MFORM *) form);
    }
    return 1;
}

/*
 * Samples the current hosted mouse position and button state.
 */
VOID graf_mkstate(WORD *pmx, WORD *pmy, WORD *pmb, WORD *pks)
{
    WORD status = 0;
    WORD x = 0;
    WORD y = 0;

    if (_aes_ensure_vdi() != 0) {
        vq_mouse(_aes.vdi_handle, &status, &x, &y);
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
        *pks = 0;
    }
}

/*
 * Copies the hosted scrap path into the caller's buffer.
 */
WORD scrp_read(char *pscrap)
{
    if (pscrap == NULL) {
        return 0;
    }
    strcpy(pscrap, _aes.scrap_path);
    return (_aes.scrap_path[0] != '\0') ? 1 : 0;
}

/*
 * Replaces the hosted scrap path string.
 */
WORD scrp_write(char *pscrap)
{
    if (pscrap == NULL) {
        return 0;
    }
    strncpy(_aes.scrap_path, pscrap, sizeof(_aes.scrap_path) - 1u);
    _aes.scrap_path[sizeof(_aes.scrap_path) - 1u] = '\0';
    return 1;
}

/*
 * Clears the hosted scrap path string.
 */
WORD scrp_clear(void)
{
    _aes.scrap_path[0] = '\0';
    return 1;
}

/*
 * Initializes the hosted file-selector path and selection strings.
 */
WORD fsel_input(char *pipath, char *pisel, WORD *pbutton)
{
    if (pbutton != NULL) {
        *pbutton = 0;
    }
    if (pipath != NULL && pipath[0] == '\0') {
        strcpy(pipath, "./");
    }
    if (pisel != NULL && pisel[0] == '\0') {
        strcpy(pisel, "*.*");
    }
    return 1;
}

/*
 * Allocates one hosted window slot and initializes its outer and work
 * rectangles.
 */
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
            _aes_compute_work(&_aes.windows[i]);
            return _aes.windows[i].handle;
        }
    }
    return 0;
}

/*
 * Opens one hosted window and updates its geometry.
 */
WORD wind_open(WORD handle, WORD x, WORD y, WORD w, WORD h)
{
    aes_window_t *window = _aes_find_window(handle);
    GRECT previous_outer;

    if (window == NULL) {
        return 0;
    }
    previous_outer = window->outer;
    _aes_set_rect(&window->outer, x, y, w, h);
    window->previous_outer = window->outer;
    _aes_compute_work(window);
    window->open = 1;
    _aes_redraw_window_change(&previous_outer, &window->outer);
    return 1;
}

/*
 * Marks one hosted window as closed.
 */
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

/*
 * Releases one hosted window slot entirely.
 */
WORD wind_delete(WORD handle)
{
    aes_window_t *window = _aes_find_window(handle);

    if (window == NULL) {
        return 0;
    }
    memset(window, 0, sizeof(*window));
    return 1;
}

/*
 * Returns desktop or window geometry according to the requested
 * `wind_get()` field.
 */
WORD wind_get(WORD handle, WORD field, WORD *w1, WORD *w2, WORD *w3, WORD *w4)
{
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
            menu_height = (box_height > 0) ? box_height : 16;
            if (menu_height <= 0) {
                menu_height = 16;
            }
        } else {
            menu_height = 0;
        }

        rect_value.g_y = menu_height;
        rect_value.g_h = (WORD) _aes_max_word((WORD) 0,
            (WORD) (rect_value.g_h - menu_height));

        if (field == WF_FIRSTXYWH) {
            _aes.enum_handle = 0;
            _aes.enum_index = 0;
            _aes.enum_count = _aes_build_visible_rects(0, _aes.enum_rects, 64);
            if (_aes.enum_count > 0) {
                rect_value = _aes.enum_rects[0];
                _aes.enum_stage = 1;
            } else {
                _aes.enum_stage = 0;
                _aes_set_rect(&rect_value, 0, 0, 0, 0);
            }
        } else if (field == WF_NEXTXYWH) {
            if (_aes.enum_handle == 0 &&
                _aes.enum_stage != 0 &&
                _aes.enum_index + 1 < _aes.enum_count) {
                ++_aes.enum_index;
                rect_value = _aes.enum_rects[_aes.enum_index];
            } else {
                _aes.enum_stage = 0;
                _aes.enum_count = 0;
                _aes.enum_index = 0;
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
        _aes.enum_handle = handle;
        _aes.enum_index = 0;
        _aes.enum_count = _aes_build_visible_rects(handle, _aes.enum_rects, 64);
        if (_aes.enum_count > 0) {
            rect = &_aes.enum_rects[0];
            _aes.enum_stage = 1;
        } else {
            rect = &rect_value;
            _aes.enum_stage = 0;
            _aes_set_rect(&rect_value, 0, 0, 0, 0);
        }
    } else if (field == WF_NEXTXYWH) {
        if (_aes.enum_handle == handle &&
            _aes.enum_stage != 0 &&
            _aes.enum_index + 1 < _aes.enum_count) {
            ++_aes.enum_index;
            rect = &_aes.enum_rects[_aes.enum_index];
        } else {
            rect = &rect_value;
            _aes.enum_stage = 0;
            _aes.enum_count = 0;
            _aes.enum_index = 0;
            _aes_set_rect(&rect_value, 0, 0, 0, 0);
        }
    } else {
        rect = (field == WF_WXYWH) ? &window->outer : &window->work;
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

/*
 * Updates one hosted window's text fields or geometry.
 */
WORD wind_set(WORD handle, WORD field, WORD w1, WORD w2, WORD w3, WORD w4)
{
    aes_window_t *window = _aes_find_window(handle);

    if (window == NULL) {
        return 0;
    }

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
        _aes_raise_window(window);
        _aes_redraw_window_change(&window->outer, &window->outer);
        _aes_redraw_window_title_states(previous_top, window);
        break;
    }
    case WF_WXYWH:
    case WF_CXYWH:
        window->previous_outer = window->outer;
        _aes_set_rect(&window->outer, w1, w2, w3, w4);
        _aes_compute_work(window);
        if (window->open != 0) {
            _aes_redraw_window_change(&window->previous_outer, &window->outer);
        }
        break;
    default:
        break;
    }
    return 1;
}

/*
 * Updates one hosted window string field using a full host pointer.
 */
WORD wind_set_str(WORD handle, WORD field, const char *text)
{
    return _aes_wind_set_text(handle, field, text);
}

/*
 * Returns the topmost open hosted window covering one screen point.
 */
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

/*
 * Starts or ends a deferred hosted redraw batch.
 */
WORD wind_update(WORD flag)
{
    if (flag == BEG_UPDATE) {
        ++_aes.update_depth;
        _vdi_begin_update();
    } else if (flag == END_UPDATE && _aes.update_depth > 0) {
        --_aes.update_depth;
        _vdi_end_update();
    }
    return 1;
}

/*
 * Converts between border and work rectangles using the hosted window
 * decoration thickness.
 */
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

/*
 * Loads one AES resource file either from the built-in provider or from
 * a hosted file path.
 */
WORD rsrc_load(char *filename)
{
    char resolved[260];

    if (filename == NULL) {
        return 0;
    }
    if (_aes.resource_data != NULL) {
        gem_os_free(_aes.resource_data);
        _aes.resource_data = NULL;
        _aes.resource_size = 0;
    }
    _aes.resource_is_builtin = 0;
    if (gem_builtin_rsrc_load(filename) != 0) {
        _aes.resource_is_builtin = 1;
        return 1;
    }
    if (_aes_try_resolve_path(filename, resolved, sizeof(resolved))) {
        return _aes_load_file(resolved, &_aes.resource_data,
            &_aes.resource_size);
    }
    if (_aes_load_file(filename, &_aes.resource_data,
        &_aes.resource_size) != 0) {
        return 1;
    }
    return 0;
}

/*
 * Releases the currently loaded AES resource image.
 */
WORD rsrc_free(void)
{
    if (_aes.resource_data != NULL) {
        gem_os_free(_aes.resource_data);
        _aes.resource_data = NULL;
        _aes.resource_size = 0;
    }
    if (_aes.resource_is_builtin != 0) {
        gem_builtin_rsrc_free();
        _aes.resource_is_builtin = 0;
    }
    return 1;
}

/*
 * Returns a typed address within the currently loaded AES resource
 * image.
 */
WORD rsrc_gaddr(WORD type, WORD index, void **addr)
{
    RSHDR *header;
    uint8_t *base;

    if (addr == NULL) {
        return 0;
    }
    if (_aes.resource_is_builtin != 0) {
        return gem_builtin_rsrc_gaddr(type, index, addr);
    }
    if (_aes.resource_data == NULL) {
        return 0;
    }

    base = (uint8_t *) _aes.resource_data;
    header = (RSHDR *) _aes.resource_data;
    switch (type) {
    case R_TREE: {
        WORD *trindex = (WORD *) (base + header->rsh_trindex);
        *addr = base + trindex[index];
        return 1;
    }
    case R_OBJECT:
        *addr = base + header->rsh_object + index * sizeof(OBJECT);
        return 1;
    case R_TEDINFO:
        *addr = base + header->rsh_tedinfo + index * sizeof(TEDINFO);
        return 1;
    case R_ICONBLK:
        *addr = base + header->rsh_iconblk + index * sizeof(ICONBLK);
        return 1;
    case R_BITBLK:
        *addr = base + header->rsh_bitblk + index * sizeof(BITBLK);
        return 1;
    case R_STRING: {
        LONG *strings = (LONG *) (base + header->rsh_string);
        *addr = base + strings[index];
        return 1;
    }
    default:
        return 0;
    }
}

/*
 * Accepts a resource-address override request when any resource image
 * is currently loaded.
 */
WORD rsrc_saddr(WORD type, WORD index, void *addr)
{
    (void) type;
    (void) index;
    (void) addr;
    return (_aes.resource_data != NULL || _aes.resource_is_builtin != 0) ?
        1 : 0;
}

/*
 * Accepts an object-fix request without modifying hosted coordinates.
 */
WORD rsrc_obfix(OBJECT *tree, WORD object)
{
    (void) tree;
    (void) object;
    return 1;
}

/*
 * Returns the last shell command and tail remembered by the hosted AES.
 */
WORD shel_read(char *cmd, char *tail)
{
    if (cmd != NULL) {
        strcpy(cmd, _aes.shell_cmd);
    }
    if (tail != NULL) {
        strcpy(tail, _aes.shell_tail);
    }
    return 1;
}

/*
 * Stores the next shell command and tail requested by the caller.
 */
WORD shel_write(WORD doex, WORD isgr, WORD iscr, char *cmd, char *tail)
{
    (void) doex;
    (void) isgr;
    (void) iscr;

    if (cmd != NULL) {
        strncpy(_aes.shell_cmd, cmd, sizeof(_aes.shell_cmd) - 1u);
        _aes.shell_cmd[sizeof(_aes.shell_cmd) - 1u] = '\0';
    }
    if (tail != NULL) {
        strncpy(_aes.shell_tail, tail, sizeof(_aes.shell_tail) - 1u);
        _aes.shell_tail[sizeof(_aes.shell_tail) - 1u] = '\0';
    }
    return 1;
}

/*
 * Returns the in-memory `DESKTOP.INF` image, loading it on demand the
 * first time the shell asks for it.
 */
WORD shel_get(char *buf, WORD length)
{
    if (_aes.shell_buf_len == 0) {
        void *data = NULL;
        size_t size = 0u;
        char resolved[260];

        if (_aes_try_resolve_path("DESKTOP.INF", resolved, sizeof(resolved)) &&
            _aes_load_file(resolved, &data, &size)) {
            if (size >= sizeof(_aes.shell_buf)) {
                size = sizeof(_aes.shell_buf) - 1u;
            }
            memcpy(_aes.shell_buf, data, size);
            _aes.shell_buf[size] = '\0';
            _aes.shell_buf_len = (WORD) size;
            gem_os_free(data);
        }
    }

    if (buf == NULL || length <= 0) {
        return 0;
    }

    if ((size_t) length > sizeof(_aes.shell_buf)) {
        length = (WORD) sizeof(_aes.shell_buf);
    }
    memcpy(buf, _aes.shell_buf, (size_t) length);
    return 1;
}

/*
 * Replaces the in-memory `DESKTOP.INF` image and mirrors it to
 * `bin/desktop.inf`.
 */
WORD shel_put(char *buf, WORD length)
{
    int fd;

    if (buf == NULL || length < 0) {
        return 0;
    }

    if ((size_t) length >= sizeof(_aes.shell_buf)) {
        length = (WORD) (sizeof(_aes.shell_buf) - 1u);
    }
    memcpy(_aes.shell_buf, buf, (size_t) length);
    _aes.shell_buf[length] = '\0';
    _aes.shell_buf_len = length;

    fd = gem_os_open_write("bin/desktop.inf");
    if (fd >= 0) {
        (void) gem_os_write(fd, _aes.shell_buf,
            (uint32_t) _aes.shell_buf_len);
        (void) gem_os_close(fd);
    }

    return 1;
}

/*
 * Resolves one shell path against the hosted desktop search locations.
 */
WORD shel_find(char *path)
{
    char resolved[260];

    if (path == NULL) {
        return 0;
    }
    if (_aes_try_resolve_path(path, resolved, sizeof(resolved))) {
        strcpy(path, resolved);
        return 1;
    }
    return 0;
}

/*
 * Returns one process environment variable through the historical AES
 * shell interface.
 */
WORD shel_envrn(char **env, char *var)
{
    char *value;

    if (env == NULL || var == NULL) {
        return 0;
    }

    value = getenv(var);
    if (value == NULL) {
        return 0;
    }
    *env = value;
    return 1;
}

/*
 * Returns the hosted shell default command and working directory.
 */
WORD shel_rdef(char *lpcmd, char *lpdir)
{
    if (lpcmd != NULL) {
        strcpy(lpcmd, _aes.shell_cmd);
    }
    if (lpdir != NULL) {
        strcpy(lpdir, _aes.shell_dir);
    }
    return 1;
}

/*
 * Stores the hosted shell default command and working directory.
 */
WORD shel_wdef(char *lpcmd, char *lpdir)
{
    if (lpcmd != NULL) {
        strncpy(_aes.shell_cmd, lpcmd, sizeof(_aes.shell_cmd) - 1u);
        _aes.shell_cmd[sizeof(_aes.shell_cmd) - 1u] = '\0';
    }
    if (lpdir != NULL) {
        strncpy(_aes.shell_dir, lpdir, sizeof(_aes.shell_dir) - 1u);
        _aes.shell_dir[sizeof(_aes.shell_dir) - 1u] = '\0';
    }
    return 1;
}
