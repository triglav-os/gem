/*
 * Schedules and performs hosted AES redraws: damage expansion, region
 * repaint with occlusion, WM_REDRAW message queuing, window change and
 * title-state repaints, and z-order raising.
 *
 * MIT License (see: LICENSE)
 * Copyright (C) 2026 tomaz stih
 */

#include "window_internal.h"
#include "system_menu.h"

#include "../vdi/vdi_internal.h"

#include <string.h>

/* The AES library is also used without gemd; gemd supplies this optional hook. */
extern void gemd_overlay_window_top_changed(void) __attribute__((weak));

static void aes_window_draw_cover_rect(const aes_window_t *window, GRECT *out);
static void aes_queue_desktop_redraw(const GRECT *dirty);
static void aes_queue_window_redraw(const aes_window_t *window,
                                    const GRECT *dirty);

void aes_present_window_frame(const aes_window_t *window)
{
    GRECT cover;

    if (aes_state.update_depth > 0 || window == NULL) {
        return;
    }
    aes_window_draw_cover_rect(window, &cover);
    if (cover.g_w > 0 && cover.g_h > 0) {
        vdi_flush_rect(cover.g_x, cover.g_y, cover.g_w, cover.g_h);
    }
}

static void aes_desktop_rect_local(GRECT *rect)
{
    WORD menu_height;

    if (rect == NULL) {
        return;
    }

    if (aes_state.vdi_ready == 0) {
        aes_set_rect(rect, 0, 0, 0, 0);
        return;
    }

    menu_height = aes_menu_bar_height();
    aes_set_rect(rect, 0, menu_height, (WORD)(aes_state.work_out[0] + 1),
                 (WORD)(aes_max_word((WORD)0, (WORD)(aes_state.work_out[1] + 1 -
                                                     menu_height))));
}

static void aes_expand_window_damage_rect(const GRECT *src, GRECT *out)
{
    if (out == NULL) {
        return;
    }
    if (src == NULL || src->g_w <= 0 || src->g_h <= 0) {
        aes_set_rect(out, 0, 0, 0, 0);
        return;
    }

    /*
     * Hosted AES draws a one-pixel frame shadow on the right and bottom
     * edges, so damage tracking must include that extra extent.
     */
    out->g_x = src->g_x;
    out->g_y = src->g_y;
    out->g_w = (WORD)(src->g_w + 1);
    out->g_h = (WORD)(src->g_h + 1);
}

static void aes_window_draw_cover_rect(const aes_window_t *window, GRECT *out)
{
    if (out == NULL) {
        return;
    }

    if (window == NULL || window->open == 0 || window->used == 0) {
        aes_set_rect(out, 0, 0, 0, 0);
        return;
    }

    aes_expand_window_damage_rect(&window->outer, out);
}

void aes_redraw_region(const GRECT *dirty)
{
    WORD clip[4];
    GRECT desktop;
    GRECT menu_rect;
    WORD bar;
    GRECT damage;
    GRECT visible[64];
    WORD visible_count;
    WORD piece;
    int redraw_menu = 0;
    int redraw_empty_bar = 0;

    if (dirty == NULL || dirty->g_w <= 0 || dirty->g_h <= 0 ||
        aes_state.vdi_ready == 0) {
        return;
    }

    aes_trace("redraw_region dirty=%d,%d %dx%d", dirty->g_x, dirty->g_y,
              dirty->g_w, dirty->g_h);

    /* Any overlap with the top strip repaints the whole bar (menu titles or
     * an empty strip) so the system title and app titles come back. */
    if (aes_menu_bar_height() > 0) {
        aes_set_rect(&menu_rect, 0, 0, (WORD)(aes_state.work_out[0] + 1),
                     aes_menu_bar_height());
        if (aes_rects_intersect(&menu_rect, dirty) != 0) {
            if (aes_state.menu_visible != 0 && aes_state.menu_tree != NULL) {
                bar = aes_state.menu_tree[ROOT].ob_head;
                redraw_menu = 1;
            } else {
                redraw_empty_bar = 1;
            }
        }
    }
    (void)bar;

    aes_desktop_rect_local(&desktop);
    if (aes_rects_intersect(&desktop, dirty) == 0) {
        if (redraw_menu != 0) {
            aes_menu_redraw_tree(aes_state.menu_tree);
        } else if (redraw_empty_bar != 0) {
            aes_menu_draw_empty_bar();
        }
        return;
    }

    clip[0] = aes_max_word(desktop.g_x, dirty->g_x);
    clip[1] = aes_max_word(desktop.g_y, dirty->g_y);
    clip[2] = aes_min_word((WORD)(desktop.g_x + desktop.g_w - 1),
                           (WORD)(dirty->g_x + dirty->g_w - 1));
    clip[3] = aes_min_word((WORD)(desktop.g_y + desktop.g_h - 1),
                           (WORD)(dirty->g_y + dirty->g_h - 1));

    ++aes_state.update_depth;
    vdi_begin_update();
    aes_set_rect(&damage, clip[0], clip[1], (WORD)(clip[2] - clip[0] + 1),
                 (WORD)(clip[3] - clip[1] + 1));
    visible_count = aes_clip_visible_rects(NULL, &damage, visible, 64);
    for (piece = 0; piece < visible_count; ++piece) {
        WORD desktop_clip[4] = {
            visible[piece].g_x, visible[piece].g_y,
            (WORD)(visible[piece].g_x + visible[piece].g_w - 1),
            (WORD)(visible[piece].g_y + visible[piece].g_h - 1)};
        vs_clip(aes_state.vdi_handle, 1, desktop_clip);
        aes_fill_checker_rect(desktop_clip[0], desktop_clip[1], desktop_clip[2],
                              desktop_clip[3]);
    }
    if (visible_count > 0) {
        aes_queue_desktop_redraw(&damage);
    }
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

        for (a = 0; a < (WORD)AES_MAX_WINDOWS; ++a) {
            GRECT cover_rect;

            aes_window_draw_cover_rect(&aes_state.windows[a], &cover_rect);
            if (aes_state.windows[a].used != 0 &&
                aes_state.windows[a].open != 0 &&
                aes_rects_intersect(&cover_rect, dirty) != 0) {
                painted[painted_count++] = &aes_state.windows[a];
            }
        }
        for (a = 1; a < painted_count; ++a) {
            aes_window_t *key = painted[a];
            uint32_t key_z = key->z_order;

            b = (WORD)(a - 1);
            while (b >= 0 && painted[b]->z_order > key_z) {
                painted[(size_t)b + 1] = painted[b];
                --b;
            }
            painted[(size_t)b + 1] = key;
        }
        for (a = 0; a < painted_count; ++a) {
            GRECT cover;
            GRECT frame_damage;
            int redraw_work = 0;

            aes_trace("redraw_region draw handle=%d z=%lu outer=%d,%d %dx%d",
                      painted[a]->handle, (unsigned long)painted[a]->z_order,
                      painted[a]->outer.g_x, painted[a]->outer.g_y,
                      painted[a]->outer.g_w, painted[a]->outer.g_h);
            aes_window_draw_cover_rect(painted[a], &cover);
            if (aes_intersect_rects(&cover, &damage, &frame_damage) == 0) {
                continue;
            }
            visible_count =
                aes_clip_visible_rects(painted[a], &frame_damage, visible, 64);
            for (piece = 0; piece < visible_count; ++piece) {
                WORD frame_clip[4] = {
                    visible[piece].g_x, visible[piece].g_y,
                    (WORD)(visible[piece].g_x + visible[piece].g_w - 1),
                    (WORD)(visible[piece].g_y + visible[piece].g_h - 1)};
                vs_clip(aes_state.vdi_handle, 1, frame_clip);
                /* Chrome only; never draw behind a higher window. */
                aes_draw_window_frame(painted[a]);
                if (aes_rects_intersect(&visible[piece], &painted[a]->work) !=
                    0) {
                    redraw_work = 1;
                }
            }
            if (redraw_work != 0) {
                aes_queue_window_redraw(painted[a], &damage);
            }
        }
    }
    vs_clip(aes_state.vdi_handle, 0, clip);
    vdi_mark_dirty(clip[0], clip[1], clip[2], clip[3]);
    vdi_present_screen();
    --aes_state.update_depth;
    /*
     * Do not full-screen present here. VDI primitives set present_pending
     * during the batch; push only the dirty clip to the physical FB.
     */
    vdi_end_update_no_present();
    if (aes_state.update_depth == 0) {
        vdi_flush_rect(clip[0], clip[1], (WORD)(clip[2] - clip[0] + 1),
                       (WORD)(clip[3] - clip[1] + 1));
    }

    if (redraw_menu != 0) {
        aes_menu_redraw_tree(aes_state.menu_tree);
    } else if (redraw_empty_bar != 0) {
        aes_menu_draw_empty_bar();
    }
}

static void aes_queue_desktop_redraw(const GRECT *dirty)
{
    GRECT desktop;
    GRECT redraw;
    WORD msg[8];

    if (dirty == NULL || aes_state.desktop_owner_app_id == 0) {
        return;
    }

    aes_desktop_rect_local(&desktop);
    if (aes_intersect_rects(&desktop, dirty, &redraw) == 0) {
        return;
    }

    msg[0] = WM_REDRAW;
    msg[1] = aes_state.current_app_id;
    msg[2] = 0;
    msg[3] = 0;
    msg[4] = redraw.g_x;
    msg[5] = redraw.g_y;
    msg[6] = redraw.g_w;
    msg[7] = redraw.g_h;
    (void)appl_write(aes_state.desktop_owner_app_id, 8, msg);
}

static void aes_queue_window_redraw(const aes_window_t *window,
                                    const GRECT *dirty)
{
    GRECT redraw;
    WORD msg[8];

    if (window == NULL || dirty == NULL || window->owner == 0 ||
        window->work.g_w <= 0 || window->work.g_h <= 0) {
        return;
    }

    if (aes_intersect_rects(&window->work, dirty, &redraw) == 0) {
        aes_trace(
            "queue_redraw skip handle=%d dirty=%d,%d %dx%d work=%d,%d %dx%d",
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
    msg[1] = aes_state.current_app_id;
    msg[2] = 0;
    msg[3] = window->handle;
    msg[4] = redraw.g_x;
    msg[5] = redraw.g_y;
    msg[6] = redraw.g_w;
    msg[7] = redraw.g_h;
    aes_trace(
        "queue_redraw handle=%d owner=%d dirty=%d,%d %dx%d redraw=%d,%d %dx%d",
        window->handle, window->owner, dirty->g_x, dirty->g_y, dirty->g_w,
        dirty->g_h, redraw.g_x, redraw.g_y, redraw.g_w, redraw.g_h);
    (void)appl_write(window->owner, 8, msg);
}

void aes_redraw_window_change(const GRECT *before, const GRECT *after)
{
    GRECT expanded_before;
    GRECT expanded_after;
    GRECT exposed[4];
    WORD exposed_count;
    WORD i;

    /* Native overlays are composited outside the VDI framebuffer.  Update
     * their visibility mask before presenting damage caused by any open,
     * close, move, resize, full or iconify transition. */
    aes_notify_window_topology_changed();

    aes_expand_window_damage_rect(before, &expanded_before);
    aes_expand_window_damage_rect(after, &expanded_after);
    exposed_count =
        aes_subtract_rect(&expanded_before, &expanded_after, exposed);
    aes_trace("window_change before=%d,%d %dx%d after=%d,%d %dx%d exposed=%d",
              expanded_before.g_x, expanded_before.g_y, expanded_before.g_w,
              expanded_before.g_h, expanded_after.g_x, expanded_after.g_y,
              expanded_after.g_w, expanded_after.g_h, exposed_count);

    for (i = 0; i < exposed_count; ++i) {
        aes_trace("window_change exposed[%d]=%d,%d %dx%d", i, exposed[i].g_x,
                  exposed[i].g_y, exposed[i].g_w, exposed[i].g_h);
        aes_redraw_region(&exposed[i]);
    }
    aes_trace("window_change redraw_after=%d,%d %dx%d", expanded_after.g_x,
              expanded_after.g_y, expanded_after.g_w, expanded_after.g_h);
    aes_redraw_region(&expanded_after);
}

void aes_notify_window_topology_changed(void)
{
    if (gemd_overlay_window_top_changed != NULL) {
        gemd_overlay_window_top_changed();
    }
}

void aes_redraw_window_title_states(const aes_window_t *previous_top,
                                    const aes_window_t *new_top)
{
    GRECT dirty;

    /*
     * Title strip only — do not extend into work (old +1 included the
     * first work row, so checker fill punched a hole in client content).
     */
    if (previous_top != NULL && previous_top->open != 0 &&
        previous_top->used != 0) {
        dirty.g_x = previous_top->outer.g_x;
        dirty.g_y = previous_top->outer.g_y;
        dirty.g_w = (WORD)(previous_top->outer.g_w + 1);
        dirty.g_h = (WORD)(previous_top->work.g_y - previous_top->outer.g_y);
        if (dirty.g_w > 0 && dirty.g_h > 0) {
            aes_redraw_region(&dirty);
        }
    }

    if (new_top != NULL && new_top->open != 0 && new_top->used != 0 &&
        new_top != previous_top) {
        aes_menu_switch_to_app(new_top->owner);
        dirty.g_x = new_top->outer.g_x;
        dirty.g_y = new_top->outer.g_y;
        dirty.g_w = (WORD)(new_top->outer.g_w + 1);
        dirty.g_h = (WORD)(new_top->work.g_y - new_top->outer.g_y);
        if (dirty.g_w > 0 && dirty.g_h > 0) {
            aes_redraw_region(&dirty);
        }
    }
}

/*
 * True if any open window with a higher z-order intersects `rect`.
 * Call BEFORE raising so z-order still reflects the old stacking.
 */
static int aes_window_rect_is_obscured(const aes_window_t *window,
                                       const GRECT *rect)
{
    size_t i;

    if (window == NULL || rect == NULL || rect->g_w <= 0 || rect->g_h <= 0) {
        return 0;
    }

    for (i = 0; i < AES_MAX_WINDOWS; ++i) {
        const aes_window_t *other = &aes_state.windows[i];

        if (other == window || other->used == 0 || other->open == 0) {
            continue;
        }
        if (other->z_order <= window->z_order) {
            continue;
        }
        if (aes_rects_intersect(&other->outer, rect) != 0) {
            return 1;
        }
    }
    return 0;
}

/*
 * Bring `window` to the front visually and in z-order.
 *
 * Old bug #1: aes_redraw_region(outer) wiped checker through every
 * client area and queued WM_REDRAW for desktop + all overlapping apps.
 * Old bug #2: partial raise drew chrome before raise and never forced
 * a correct on-top paint, so previous-top pixels stayed visible
 * ("windows overlap").
 * Old bug #3: paper-filling work then client-painting = double draw.
 *
 * Correct path:
 *  1. Note whether any higher window covers us (old z-order).
 *  2. Raise z-order.
 *  3. Title strips for previous/new top (active/inactive).
 *  4. If we were covered: draw our frame on top (AES chrome only),
 *     queue one WM_REDRAW for our full work so the client paints once
 *     over whatever still shows from the old top. Siblings and desktop
 *     are not wiped and not asked to repaint.
 */
void aes_top_window(aes_window_t *window)
{
    aes_window_t *previous_top;
    int was_obscured;
    GRECT cover;

    if (window == NULL || window->used == 0 || window->open == 0) {
        return;
    }

    previous_top = aes_find_top_window();
    if (previous_top == window) {
        return;
    }

    was_obscured = (aes_window_rect_is_obscured(window, &window->outer) != 0) ||
                   (aes_window_rect_is_obscured(window, &window->work) != 0);

    aes_raise_window(window);
    aes_notify_window_topology_changed();
    aes_redraw_window_title_states(previous_top, window);

    if (was_obscured == 0 || aes_state.vdi_ready == 0) {
        return;
    }

    /*
     * Frame on top after title_states so active chrome wins stacking.
     * Do not fill work — client owns it via the single WM_REDRAW below.
     */
    ++aes_state.update_depth;
    vdi_begin_update();
    aes_draw_window_frame(window);
    --aes_state.update_depth;
    vdi_end_update_no_present();

    aes_window_draw_cover_rect(window, &cover);
    if (aes_state.update_depth == 0 && cover.g_w > 0 && cover.g_h > 0) {
        vdi_flush_rect(cover.g_x, cover.g_y, cover.g_w, cover.g_h);
    }

    aes_queue_window_redraw(window, &window->work);
}

void aes_refresh_raised_window(const aes_window_t *window)
{
    /* Legacy name: full top including z-order. */
    aes_top_window((aes_window_t *)window);
}

void aes_finish_window_raise(aes_window_t *previous_top, aes_window_t *window,
                             int was_obscured)
{
    /* Legacy; prefer aes_top_window(). */
    GRECT cover;

    (void)previous_top;
    if (window == NULL || was_obscured == 0 || aes_state.vdi_ready == 0) {
        return;
    }
    ++aes_state.update_depth;
    vdi_begin_update();
    aes_draw_window_frame(window);
    --aes_state.update_depth;
    vdi_end_update_no_present();
    aes_window_draw_cover_rect(window, &cover);
    if (aes_state.update_depth == 0 && cover.g_w > 0 && cover.g_h > 0) {
        vdi_flush_rect(cover.g_x, cover.g_y, cover.g_w, cover.g_h);
    }
    aes_queue_window_redraw(window, &window->work);
}

void aes_redraw_open_windows(void)
{
    GRECT desktop;

    aes_desktop_rect_local(&desktop);
    aes_redraw_region(&desktop);
}

void aes_raise_window(aes_window_t *window)
{
    if (window == NULL) {
        return;
    }

    window->z_order = aes_state.next_window_z++;
}
