/*
 * Enumerates visible window regions for the hosted AES: the top window,
 * cover rectangles including the drop shadow, occlusion clipping shared by
 * client WF_FIRSTXYWH/WF_NEXTXYWH enumeration and AES painting, and the
 * scroll bar delta repaint after slider changes.
 *
 * MIT License (see: LICENSE)
 * Copyright (C) 2026 tomaz stih
 */

#include "window_internal.h"

#include <string.h>

aes_window_t *aes_find_top_window(void)
{
    aes_window_t *best = NULL;
    size_t i;

    for (i = 0; i < AES_MAX_WINDOWS; ++i) {
        aes_window_t *window = &aes_state.windows[i];

        if (window->used == 0 || window->open == 0) {
            continue;
        }
        if (best == NULL || window->z_order > best->z_order) {
            best = window;
        }
    }

    return best;
}

static void aes_window_cover_rect(const aes_window_t *window, GRECT *rect)
{
    if (rect == NULL) {
        return;
    }

    if (window == NULL || window->used == 0 || window->open == 0 ||
        window->outer.g_w <= 0 || window->outer.g_h <= 0) {
        aes_set_rect(rect, 0, 0, 0, 0);
        return;
    }

    /*
     * Hosted AES draws a one-pixel shadow on the right and bottom
     * edges, so visible-region enumeration must treat that shadow as
     * occupied by the covering window too.
     */
    aes_set_rect(rect, window->outer.g_x, window->outer.g_y,
                 (WORD)(window->outer.g_w + 1), (WORD)(window->outer.g_h + 1));
}

WORD aes_build_visible_rects(WORD handle, GRECT out[], WORD max_rects)
{
    GRECT base;
    aes_window_t *target = NULL;

    if (handle == 0) {
        aes_desktop_rect(&base);
    } else {
        aes_window_t *window = aes_find_window(handle);

        if (window == NULL || window->open == 0 || window->work.g_w <= 0 ||
            window->work.g_h <= 0) {
            return 0;
        }
        target = window;
        base = window->work;
    }

    return aes_clip_visible_rects(target, &base, out, max_rects);
}

/* Share occlusion clipping between client enumeration and AES painting. */
static WORD aes_clip_layer_rects(const aes_window_t *target,
                                 uint32_t target_z, int modal_form_target,
                                 const GRECT *base, GRECT out[],
                                 WORD max_rects)
{
    GRECT pending[64];
    GRECT next_pending[64];
    WORD pending_count = 1;
    size_t i;
    const WORD handle = target != NULL ? target->handle : 0;

    if (out == NULL || max_rects <= 0 || base == NULL || base->g_w <= 0 ||
        base->g_h <= 0) {
        return 0;
    }
    max_rects = aes_min_word(max_rects, 64);

    aes_trace("visible_rects begin handle=%d base=%d,%d %dx%d target_z=%lu",
              handle, base->g_x, base->g_y, base->g_w, base->g_h,
              (unsigned long)target_z);

    pending[0] = *base;

    for (i = 0; i <= AES_MAX_WINDOWS + 1 && pending_count > 0; ++i) {
        const aes_window_t *cover =
            i < AES_MAX_WINDOWS ? &aes_state.windows[i] : NULL;
        GRECT cover_rect;
        WORD next_count = 0;
        WORD j;

        if (cover) {
            if (cover->used == 0 || cover->open == 0)
                continue;
            if (cover->handle == handle || cover->z_order <= target_z)
                continue;
            aes_window_cover_rect(cover, &cover_rect);
        } else if (i == AES_MAX_WINDOWS) {
            if (!aes_modal_cover)
                continue;
            cover_rect = *aes_modal_cover;
        } else {
            if (modal_form_target != 0 ||
                aes_state.modal_form_owner_app_id == 0 ||
                aes_state.modal_form_z <= target_z)
                continue;
            cover_rect = aes_state.modal_form_rect;
        }
        if (cover_rect.g_w <= 0 || cover_rect.g_h <= 0) {
            continue;
        }
        aes_trace(
            "visible_rects cover handle=%d z=%lu rect=%d,%d %dx%d pending=%d",
            cover ? cover->handle : 0,
            (unsigned long)(cover ? cover->z_order : 0), cover_rect.g_x,
            cover_rect.g_y, cover_rect.g_w, cover_rect.g_h, pending_count);

        for (j = 0; j < pending_count; ++j) {
            GRECT fragments[4];
            WORD fragment_count =
                aes_subtract_rect(&pending[j], &cover_rect, fragments);
            WORD k;

            for (k = 0; k < fragment_count && next_count < max_rects; ++k) {
                next_pending[next_count++] = fragments[k];
            }
        }

        memcpy(pending, next_pending, (size_t)next_count * sizeof(GRECT));
        pending_count = next_count;
    }

    for (i = 0; i < (size_t)pending_count; ++i) {
        aes_trace("visible_rects out[%lu]=%d,%d %dx%d", (unsigned long)i,
                  pending[i].g_x, pending[i].g_y, pending[i].g_w,
                  pending[i].g_h);
    }
    aes_trace("visible_rects end handle=%d count=%d", handle, pending_count);
    memcpy(out, pending, (size_t)pending_count * sizeof(GRECT));
    return pending_count;
}

WORD aes_clip_visible_rects(const aes_window_t *target, const GRECT *base,
                            GRECT out[], WORD max_rects)
{
    return aes_clip_layer_rects(target,
                                target != NULL ? target->z_order : 0u, 0,
                                base, out, max_rects);
}

WORD aes_clip_modal_form_rects(const GRECT *base, GRECT out[], WORD max_rects)
{
    return aes_clip_layer_rects(NULL, aes_state.modal_form_z, 1, base, out,
                                max_rects);
}

static void aes_union_rects(const GRECT *left, const GRECT *right, GRECT *out)
{
    WORD x0;
    WORD y0;
    WORD x1;
    WORD y1;

    if (left == NULL || right == NULL || out == NULL) {
        return;
    }

    x0 = aes_min_word(left->g_x, right->g_x);
    y0 = aes_min_word(left->g_y, right->g_y);
    x1 = aes_max_word((WORD)(left->g_x + left->g_w - 1),
                      (WORD)(right->g_x + right->g_w - 1));
    y1 = aes_max_word((WORD)(left->g_y + left->g_h - 1),
                      (WORD)(right->g_y + right->g_h - 1));
    aes_set_rect(out, x0, y0, (WORD)(x1 - x0 + 1), (WORD)(y1 - y0 + 1));
}

void aes_redraw_scrollbar_delta(const aes_window_t *before,
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
            have_old = aes_window_vthumb_rect(before, &old_rect);
            have_new = aes_window_vthumb_rect(after, &new_rect);
            break;
        case WF_HSLIDE:
        case WF_HSLSIZ:
            have_old = aes_window_hthumb_rect(before, &old_rect);
            have_new = aes_window_hthumb_rect(after, &new_rect);
            break;
        default:
            break;
    }

    if (have_old != 0 && have_new != 0) {
        if (old_rect.g_x == new_rect.g_x && old_rect.g_y == new_rect.g_y &&
            old_rect.g_w == new_rect.g_w && old_rect.g_h == new_rect.g_h) {
            return;
        }
        aes_union_rects(&old_rect, &new_rect, &dirty);
        aes_redraw_region(&dirty);
    } else if (have_old != 0) {
        aes_redraw_region(&old_rect);
    } else if (have_new != 0) {
        aes_redraw_region(&new_rect);
    }
}
