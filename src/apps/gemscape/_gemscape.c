/*
 * Implements the first hosted GEM Gemscape window shell. This initial
 * version focuses on a browser-like frame with a toolbar band, address
 * band, content pane, and status bar sized from native GEM metrics so
 * it fits visually with titles, menu rows, and scroll controls.
 *
 * MIT License (see: LICENSE)
 * Copyright (C) 2026 tomaz stih
 */

#include <gem.h>

#include "gemscape.h"
#include "platform/os.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum {
    gemscape_work_width = 560,
    gemscape_work_height = 360,
    gemscape_margin = 6,
    gemscape_inner_gap = 4,
    gemscape_toolbar_icon_count = 4
};

enum {
    gemscape_paper = BLACK,
    gemscape_ink = WHITE,
    gemscape_content_paper = BLACK
};

typedef struct gemscape_state {
    WORD handle;
    WORD vdi_handle;
    WORD char_w;
    WORD char_h;
    WORD box_w;
    WORD box_h;
    BITBLK toolbar_icons[gemscape_toolbar_icon_count];
    int toolbar_icons_loaded;
    GRECT normal_rect;
    int full_open;
} gemscape_state_t;

static void gemscape_free_bitblk(BITBLK *bitblk)
{
    if (bitblk == NULL) {
        return;
    }

    free((void *) (intptr_t) bitblk->bi_pdata);
    memset(bitblk, 0, sizeof(*bitblk));
}

static void gemscape_free_toolbar_icons(gemscape_state_t *state)
{
    WORD ii;

    if (state == NULL) {
        return;
    }

    for (ii = 0; ii < gemscape_toolbar_icon_count; ++ii) {
        gemscape_free_bitblk(&state->toolbar_icons[ii]);
    }
    state->toolbar_icons_loaded = 0;
}

static int gemscape_clone_bitblk(BITBLK *dst, const BITBLK *src)
{
    size_t plane_bytes;
    void *data_copy;

    if (dst == NULL || src == NULL || src->bi_pdata == 0 ||
        src->bi_wb <= 0 || src->bi_hl <= 0) {
        return 0;
    }

    plane_bytes = (size_t) src->bi_wb * (size_t) src->bi_hl;
    data_copy = malloc(plane_bytes);
    if (data_copy == NULL) {
        return 0;
    }

    memcpy(data_copy, (const void *) (intptr_t) src->bi_pdata, plane_bytes);
    *dst = *src;
    dst->bi_pdata = (LONG) (intptr_t) data_copy;
    return 1;
}

static int gemscape_load_bitblks_from_resource(const char *primary_path,
    const char *fallback_path, BITBLK *bitblks, WORD count)
{
    BITBLK *bitblk;
    WORD ii;

    if (bitblks == NULL || count <= 0) {
        return 0;
    }
    if (rsrc_load((char *) primary_path) == 0 &&
        rsrc_load((char *) fallback_path) == 0) {
        return 0;
    }

    for (ii = 0; ii < count; ++ii) {
        if (rsrc_gaddr(R_BITBLK, ii, (void **) &bitblk) == 0 ||
            bitblk == NULL) {
            while (--ii >= 0) {
                gemscape_free_bitblk(&bitblks[ii]);
            }
            rsrc_free();
            return 0;
        }
        if (!gemscape_clone_bitblk(&bitblks[ii], bitblk)) {
            while (--ii >= 0) {
                gemscape_free_bitblk(&bitblks[ii]);
            }
            rsrc_free();
            return 0;
        }
    }

    rsrc_free();
    return 1;
}

static int gemscape_load_toolbar_icons(gemscape_state_t *state)
{
    if (state == NULL) {
        return 0;
    }

    state->toolbar_icons_loaded = gemscape_load_bitblks_from_resource(
        "bin/resources/gemscape_toolbar.rsc",
        "gemscape_toolbar.rsc",
        state->toolbar_icons,
        gemscape_toolbar_icon_count);
    return state->toolbar_icons_loaded;
}

static void gemscape_intersect_rect(const GRECT *a, const GRECT *b,
    GRECT *out)
{
    WORD x0;
    WORD y0;
    WORD x1;
    WORD y1;

    if (a == NULL || b == NULL || out == NULL) {
        return;
    }

    x0 = (a->g_x > b->g_x) ? a->g_x : b->g_x;
    y0 = (a->g_y > b->g_y) ? a->g_y : b->g_y;
    x1 = (WORD) (((a->g_x + a->g_w - 1) < (b->g_x + b->g_w - 1))
        ? (a->g_x + a->g_w - 1) : (b->g_x + b->g_w - 1));
    y1 = (WORD) (((a->g_y + a->g_h - 1) < (b->g_y + b->g_h - 1))
        ? (a->g_y + a->g_h - 1) : (b->g_y + b->g_h - 1));

    if (x1 < x0 || y1 < y0) {
        out->g_x = 0;
        out->g_y = 0;
        out->g_w = 0;
        out->g_h = 0;
        return;
    }

    out->g_x = x0;
    out->g_y = y0;
    out->g_w = (WORD) (x1 - x0 + 1);
    out->g_h = (WORD) (y1 - y0 + 1);
}

static void gemscape_fill_rect(WORD handle, WORD color, const GRECT *rect)
{
    WORD pxy[4];

    if (rect == NULL || rect->g_w <= 0 || rect->g_h <= 0) {
        return;
    }

    pxy[0] = rect->g_x;
    pxy[1] = rect->g_y;
    pxy[2] = (WORD) (rect->g_x + rect->g_w - 1);
    pxy[3] = (WORD) (rect->g_y + rect->g_h - 1);

    (void) vswr_mode(handle, MD_REPLACE);
    (void) vsf_interior(handle, FIS_SOLID);
    (void) vsf_style(handle, 8);
    (void) vsf_perimeter(handle, 0);
    vsf_color(handle, color);
    v_bar(handle, pxy);
}

static void gemscape_draw_hline(WORD handle, WORD x0, WORD x1, WORD y)
{
    WORD pxy[4];

    if (x1 < x0) {
        return;
    }

    (void) vsl_type(handle, 1);
    (void) vsl_width(handle, 1);
    vsl_color(handle, gemscape_ink);
    pxy[0] = x0;
    pxy[1] = y;
    pxy[2] = x1;
    pxy[3] = y;
    v_pline(handle, 2, pxy);
}

static void gemscape_draw_vline(WORD handle, WORD x, WORD y0, WORD y1)
{
    WORD pxy[4];

    if (y1 < y0) {
        return;
    }

    (void) vsl_type(handle, 1);
    (void) vsl_width(handle, 1);
    vsl_color(handle, gemscape_ink);
    pxy[0] = x;
    pxy[1] = y0;
    pxy[2] = x;
    pxy[3] = y1;
    v_pline(handle, 2, pxy);
}

static void gemscape_draw_bitmap(WORD handle, const BITBLK *bitblk,
    WORD dst_x, WORD dst_y)
{
    MFDB src;
    WORD pxy[8];
    WORD colors[2];
    WORD width;

    if (bitblk == NULL || bitblk->bi_pdata == 0 ||
        bitblk->bi_wb <= 0 || bitblk->bi_hl <= 0) {
        return;
    }

    width = (WORD) (bitblk->bi_wb * 8);
    memset(&src, 0, sizeof(src));
    src.fd_addr = (VOID *) (intptr_t) bitblk->bi_pdata;
    src.fd_w = width;
    src.fd_h = bitblk->bi_hl;
    src.fd_wdwidth = (WORD) (bitblk->bi_wb / 2);
    src.fd_stand = 1;
    src.fd_nplanes = 1;

    pxy[0] = 0;
    pxy[1] = 0;
    pxy[2] = (WORD) (width - 1);
    pxy[3] = (WORD) (bitblk->bi_hl - 1);
    pxy[4] = dst_x;
    pxy[5] = dst_y;
    pxy[6] = (WORD) (dst_x + width - 1);
    pxy[7] = (WORD) (dst_y + bitblk->bi_hl - 1);
    colors[0] = BLACK;
    colors[1] = WHITE;

    vrt_cpyfm(handle, MD_REPLACE, pxy, &src, NULL, colors);
}

static void gemscape_draw_toolbar_icon(const gemscape_state_t *state,
    WORD slot_x, WORD slot_y, WORD slot_size, WORD icon_index)
{
    const BITBLK *bitblk;
    WORD width;
    WORD icon_x;
    WORD icon_y;

    if (state == NULL || icon_index < 0 ||
        icon_index >= gemscape_toolbar_icon_count ||
        state->toolbar_icons_loaded == 0 || slot_size <= 0) {
        return;
    }

    bitblk = &state->toolbar_icons[icon_index];
    if (bitblk->bi_pdata == 0 || bitblk->bi_wb <= 0 || bitblk->bi_hl <= 0) {
        return;
    }

    width = (WORD) (bitblk->bi_wb * 8);
    icon_x = (WORD) (slot_x + (slot_size - width) / 2);
    icon_y = (WORD) (slot_y + (slot_size - bitblk->bi_hl) / 2);
    gemscape_draw_bitmap(state->vdi_handle, bitblk, icon_x, icon_y);
}

static void gemscape_draw_text(const gemscape_state_t *state,
    const GRECT *rect, const char *text, int centered)
{
    WORD distances[5];
    WORD extent[8];
    WORD text_height;
    WORD text_width;
    WORD ascent;
    WORD top_y;
    WORD x;
    WORD y;

    if (state == NULL || rect == NULL || text == NULL) {
        return;
    }

    memset(distances, 0, sizeof(distances));
    (void) vqt_fontinfo(state->vdi_handle, NULL, NULL, distances, NULL, NULL);
    (void) vqt_extent(state->vdi_handle, (char *) text, extent);
    text_height = (WORD) (extent[5] - extent[1] + 1);
    text_width = (WORD) (extent[2] - extent[0] + 1);
    ascent = (WORD) (distances[1] + 2);

    if (centered != 0) {
        x = (WORD) (rect->g_x + (rect->g_w - text_width) / 2);
    } else {
        x = (WORD) (rect->g_x + gemscape_margin);
    }
    top_y = (WORD) (rect->g_y + (rect->g_h - text_height) / 2);
    y = (WORD) (top_y + ascent);

    vst_color(state->vdi_handle, gemscape_ink);
    v_gtext(state->vdi_handle, x, y, (const BYTE *) text);
}

static void gemscape_draw_top_band(WORD handle, const GRECT *rect)
{
    if (rect == NULL) {
        return;
    }

    gemscape_fill_rect(handle, gemscape_paper, rect);
    gemscape_draw_hline(handle, rect->g_x,
        (WORD) (rect->g_x + rect->g_w - 1),
        (WORD) (rect->g_y + rect->g_h - 1));
}

static void gemscape_draw_bottom_band(const gemscape_state_t *state,
    const GRECT *rect,
    const char *text)
{
    if (rect == NULL) {
        return;
    }

    gemscape_fill_rect(state->vdi_handle, gemscape_paper, rect);
    gemscape_draw_hline(state->vdi_handle, rect->g_x,
        (WORD) (rect->g_x + rect->g_w - 1), rect->g_y);
    if (text != NULL) {
        gemscape_draw_text(state, rect, text, 0);
    }
}

static void gemscape_draw_content(const gemscape_state_t *state,
    const GRECT *rect)
{
    GRECT note;

    if (state == NULL || rect == NULL) {
        return;
    }

    gemscape_fill_rect(state->vdi_handle, gemscape_content_paper, rect);

    note = *rect;
    gemscape_draw_text(state, &note, "Gemscape viewport", 0);
    note.g_y = (WORD) (note.g_y + rect->g_h / 10);
    gemscape_draw_text(state, &note,
        "Address controls and viewport frame under construction.", 0);
}

static void gemscape_layout(const gemscape_state_t *state, const GRECT *work,
    GRECT *address, GRECT *content, GRECT *status)
{
    WORD band_h;
    WORD top_y;
    WORD status_y;

    if (state == NULL || work == NULL || address == NULL || content == NULL ||
        status == NULL) {
        return;
    }

    band_h = (WORD) (state->box_h - 2);
    if (band_h < state->char_h + gemscape_inner_gap) {
        band_h = (WORD) (state->char_h + gemscape_inner_gap);
    }

    top_y = work->g_y;

    address->g_x = work->g_x;
    address->g_y = top_y;
    address->g_w = work->g_w;
    address->g_h = band_h;

    status_y = (WORD) (work->g_y + work->g_h - band_h);
    status->g_x = work->g_x;
    status->g_y = status_y;
    status->g_w = work->g_w;
    status->g_h = band_h;

    content->g_x = work->g_x;
    content->g_y = (WORD) (address->g_y + address->g_h);
    content->g_w = work->g_w;
    content->g_h = (WORD) (status->g_y - content->g_y);
    if (content->g_h < 0) {
        content->g_h = 0;
    }
}

static void gemscape_draw_frame(gemscape_state_t *state, const GRECT *dirty)
{
    GRECT work;
    GRECT visible;
    GRECT clip;
    GRECT address;
    GRECT content;
    GRECT status;
    GRECT address_text;
    WORD clip_xy[4];
    WORD button_size;
    WORD separator_x;
    WORD right_button_left;
    int i;

    if (state == NULL) {
        return;
    }

    if (wind_get(state->handle, WF_WORKXYWH, &work.g_x, &work.g_y,
            &work.g_w, &work.g_h) == 0) {
        return;
    }

    gemscape_layout(state, &work, &address, &content, &status);

    wind_update(BEG_UPDATE);
    wind_get(state->handle, WF_FIRSTXYWH, &visible.g_x, &visible.g_y,
        &visible.g_w, &visible.g_h);

    while (visible.g_w > 0 && visible.g_h > 0) {
        clip = visible;
        if (dirty != NULL) {
            gemscape_intersect_rect(&clip, dirty, &clip);
        }

        if (clip.g_w > 0 && clip.g_h > 0) {
            clip_xy[0] = clip.g_x;
            clip_xy[1] = clip.g_y;
            clip_xy[2] = (WORD) (clip.g_x + clip.g_w - 1);
            clip_xy[3] = (WORD) (clip.g_y + clip.g_h - 1);
            vs_clip(state->vdi_handle, 1, clip_xy);

            gemscape_draw_top_band(state->vdi_handle, &address);
            button_size = address.g_h;
            for (i = 1; i <= 3; ++i) {
                separator_x = (WORD) (address.g_x + i * button_size - 1);
                if (separator_x < (WORD) (address.g_x + address.g_w)) {
                    gemscape_draw_vline(state->vdi_handle, separator_x,
                        address.g_y,
                        (WORD) (address.g_y + address.g_h - 1));
                }
            }
            for (i = 0; i < 3; ++i) {
                gemscape_draw_toolbar_icon(state,
                    (WORD) (address.g_x + i * button_size),
                    address.g_y,
                    button_size,
                    (WORD) i);
            }
            right_button_left = (WORD) (address.g_x + address.g_w -
                button_size);
            if (right_button_left > address.g_x) {
                gemscape_draw_vline(state->vdi_handle, right_button_left,
                    address.g_y,
                    (WORD) (address.g_y + address.g_h - 1));
                gemscape_draw_toolbar_icon(state, right_button_left,
                    address.g_y, button_size, 3);
            }
            address_text = address;
            address_text.g_x = (WORD) (address.g_x + 3 * button_size);
            address_text.g_w = (WORD) (right_button_left -
                address_text.g_x);
            if (address_text.g_w < 0) {
                address_text.g_w = 0;
            }
            gemscape_draw_text(state, &address_text,
                "https://gemscape.local/", 0);
            gemscape_draw_content(state, &content);
            gemscape_draw_bottom_band(state, &status,
                "Ready. Gemscape frame initialized.");

            vs_clip(state->vdi_handle, 0, clip_xy);
        }

        wind_get(state->handle, WF_NEXTXYWH, &visible.g_x, &visible.g_y,
            &visible.g_w, &visible.g_h);
    }

    wind_update(END_UPDATE);
}

static void gemscape_toggle_full(gemscape_state_t *state)
{
    GRECT full;
    GRECT current;

    if (state == NULL) {
        return;
    }

    if (wind_get(0, WF_WORKXYWH, &full.g_x, &full.g_y, &full.g_w,
            &full.g_h) == 0) {
        return;
    }
    if (wind_get(state->handle, WF_WXYWH, &current.g_x, &current.g_y,
            &current.g_w, &current.g_h) == 0) {
        return;
    }

    if (state->full_open == 0) {
        state->normal_rect = current;
        (void) wind_set(state->handle, WF_WXYWH, full.g_x, full.g_y,
            full.g_w, full.g_h);
        state->full_open = 1;
    } else {
        (void) wind_set(state->handle, WF_WXYWH, state->normal_rect.g_x,
            state->normal_rect.g_y, state->normal_rect.g_w,
            state->normal_rect.g_h);
        state->full_open = 0;
    }
}

int gemscape_main(void)
{
    gemscape_state_t state;
    WORD appl_id;
    WORD event_flags;
    WORD mouse_x;
    WORD mouse_y;
    WORD mouse_buttons;
    WORD key_state;
    WORD key_code;
    WORD button_return;
    WORD msg[8];
    WORD outer_x;
    WORD outer_y;
    WORD outer_w;
    WORD outer_h;
    UWORD window_kind;

    if (!gem_os_init()) {
        fprintf(stderr, "gem_os_init() failed\n");
        return 1;
    }

    appl_id = appl_init();
    if (appl_id < 0) {
        gem_os_shutdown();
        return 1;
    }

    memset(&state, 0, sizeof(state));
    state.vdi_handle = graf_handle(&state.char_w, &state.char_h,
        &state.box_w, &state.box_h);
    if (state.vdi_handle == 0) {
        appl_exit();
        gem_os_shutdown();
        return 1;
    }

    (void) gemscape_load_toolbar_icons(&state);

    window_kind = (UWORD) (NAME | CLOSER | FULLER | MOVER | SIZER |
        UPARROW | DNARROW | VSLIDE | LFARROW | RTARROW | HSLIDE);

    state.handle = wind_create(window_kind, 0, 0, 640, 400);
    if (state.handle <= 0) {
        appl_exit();
        gem_os_shutdown();
        return 1;
    }

    (void) wind_set_str(state.handle, WF_NAME, "Gemscape");
    (void) wind_calc(WC_BORDER, window_kind, 72, 48, gemscape_work_width,
        gemscape_work_height, &outer_x, &outer_y, &outer_w, &outer_h);
    state.normal_rect.g_x = outer_x;
    state.normal_rect.g_y = outer_y;
    state.normal_rect.g_w = outer_w;
    state.normal_rect.g_h = outer_h;
    (void) wind_open(state.handle, outer_x, outer_y, outer_w, outer_h);
    (void) graf_mouse(M_ON, NULL);

    FOREVER {
        event_flags = evnt_multi((UWORD) (MU_MESAG | MU_KEYBD),
            1, 1, 1,
            0, 0, 0, 0, 0,
            0, 0, 0, 0, 0,
            msg,
            0, 0,
            &mouse_x, &mouse_y, &mouse_buttons, &key_state,
            &key_code, &button_return);

        if ((event_flags & MU_MESAG) != 0 && msg[3] == state.handle) {
            if (msg[0] == WM_CLOSED) {
                break;
            } else if (msg[0] == WM_REDRAW) {
                GRECT redraw;

                redraw.g_x = msg[4];
                redraw.g_y = msg[5];
                redraw.g_w = msg[6];
                redraw.g_h = msg[7];
                gemscape_draw_frame(&state, &redraw);
            } else if (msg[0] == WM_MOVED || msg[0] == WM_SIZED) {
                (void) wind_set(state.handle, WF_WXYWH, msg[4], msg[5],
                    msg[6], msg[7]);
                state.normal_rect.g_x = msg[4];
                state.normal_rect.g_y = msg[5];
                state.normal_rect.g_w = msg[6];
                state.normal_rect.g_h = msg[7];
                state.full_open = 0;
            } else if (msg[0] == WM_FULLED) {
                gemscape_toggle_full(&state);
            } else if (msg[0] == WM_TOPPED) {
                (void) wind_set(state.handle, WF_TOP, 0, 0, 0, 0);
            }
        }

        if ((event_flags & MU_KEYBD) != 0 && (key_code & 0x00ff) == 27) {
            break;
        }
    }

    if (state.handle > 0) {
        (void) wind_close(state.handle);
        (void) wind_delete(state.handle);
    }
    gemscape_free_toolbar_icons(&state);
    appl_exit();
    gem_os_shutdown();
    return 0;
}
