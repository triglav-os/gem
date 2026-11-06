/*
 * Paints the Maestro sample: clipped rectangles and lines, padded text,
 * resource bitmaps, the tree pane with its expand/collapse icons, the
 * file pane, status line and window frame.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (C) 2026 tomaz stih
 */

#include "maestro_private.h"

#include <stdio.h>
#include <string.h>

static void maestro_intersect_rect(const GRECT *a, const GRECT *b, GRECT *out)
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
    x1 = (WORD)(((a->g_x + a->g_w - 1) < (b->g_x + b->g_w - 1))
                    ? (a->g_x + a->g_w - 1)
                    : (b->g_x + b->g_w - 1));
    y1 = (WORD)(((a->g_y + a->g_h - 1) < (b->g_y + b->g_h - 1))
                    ? (a->g_y + a->g_h - 1)
                    : (b->g_y + b->g_h - 1));

    if (x1 < x0 || y1 < y0) {
        out->g_x = 0;
        out->g_y = 0;
        out->g_w = 0;
        out->g_h = 0;
        return;
    }

    out->g_x = x0;
    out->g_y = y0;
    out->g_w = (WORD)(x1 - x0 + 1);
    out->g_h = (WORD)(y1 - y0 + 1);
}

static void maestro_fill_rect(WORD handle, WORD color, const GRECT *rect)
{
    WORD pxy[4];

    if (rect == NULL || rect->g_w <= 0 || rect->g_h <= 0) {
        return;
    }

    pxy[0] = rect->g_x;
    pxy[1] = rect->g_y;
    pxy[2] = (WORD)(rect->g_x + rect->g_w - 1);
    pxy[3] = (WORD)(rect->g_y + rect->g_h - 1);

    (void)vswr_mode(handle, MD_REPLACE);
    (void)vsf_interior(handle, FIS_SOLID);
    (void)vsf_style(handle, 8);
    (void)vsf_perimeter(handle, 0);
    vsf_color(handle, color);
    v_bar(handle, pxy);
}

static void maestro_draw_hline(WORD handle, WORD x0, WORD x1, WORD y)
{
    WORD pxy[4];

    if (x1 < x0) {
        return;
    }

    (void)vsl_type(handle, 1);
    (void)vsl_width(handle, 1);
    vsl_color(handle, maestro_ink);
    pxy[0] = x0;
    pxy[1] = y;
    pxy[2] = x1;
    pxy[3] = y;
    v_pline(handle, 2, pxy);
}

static void maestro_draw_vline(WORD handle, WORD x, WORD y0, WORD y1)
{
    WORD pxy[4];

    if (y1 < y0) {
        return;
    }

    (void)vsl_type(handle, 1);
    (void)vsl_width(handle, 1);
    vsl_color(handle, maestro_ink);
    pxy[0] = x;
    pxy[1] = y0;
    pxy[2] = x;
    pxy[3] = y1;
    v_pline(handle, 2, pxy);
}

static void maestro_draw_text_with_padding(const maestro_state_t *state,
                                           const GRECT *rect, const char *text,
                                           int centered, WORD left_padding)
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
    (void)vqt_fontinfo(state->vdi_handle, NULL, NULL, distances, NULL, NULL);
    (void)vqt_extent(state->vdi_handle, (char *)text, extent);
    text_height = (WORD)(extent[5] - extent[1] + 1);
    text_width = (WORD)(extent[2] - extent[0] + 1);
    ascent = (WORD)(distances[1] + 2);

    if (centered != 0) {
        x = (WORD)(rect->g_x + (rect->g_w - text_width) / 2);
    } else {
        x = (WORD)(rect->g_x + left_padding);
    }
    top_y = (WORD)(rect->g_y + (rect->g_h - text_height) / 2);
    y = (WORD)(top_y + ascent);

    vst_color(state->vdi_handle, maestro_ink);
    v_gtext(state->vdi_handle, x, y, (const BYTE *)text);
}

static void maestro_draw_text(const maestro_state_t *state, const GRECT *rect,
                              const char *text, int centered)
{
    maestro_draw_text_with_padding(state, rect, text, centered, maestro_margin);
}

static void maestro_draw_bitmap(WORD handle, const BITBLK *bitblk, WORD dst_x,
                                WORD dst_y)
{
    MFDB src;
    WORD pxy[8];
    WORD colors[2];
    WORD width;

    if (bitblk == NULL || bitblk->bi_pdata == 0 || bitblk->bi_wb <= 0 ||
        bitblk->bi_hl <= 0) {
        return;
    }

    width = (WORD)(bitblk->bi_wb * 8);
    memset(&src, 0, sizeof(src));
    src.fd_addr = (VOID *)(intptr_t)bitblk->bi_pdata;
    src.fd_w = width;
    src.fd_h = bitblk->bi_hl;
    src.fd_wdwidth = (WORD)(bitblk->bi_wb / 2);
    src.fd_stand = 1;
    src.fd_nplanes = 1;

    pxy[0] = 0;
    pxy[1] = 0;
    pxy[2] = (WORD)(width - 1);
    pxy[3] = (WORD)(bitblk->bi_hl - 1);
    pxy[4] = dst_x;
    pxy[5] = dst_y;
    pxy[6] = (WORD)(dst_x + width - 1);
    pxy[7] = (WORD)(dst_y + bitblk->bi_hl - 1);
    colors[0] = BLACK;
    colors[1] = WHITE;

    vrt_cpyfm(handle, MD_REPLACE, pxy, &src, NULL, colors);
}

static void maestro_draw_status(const maestro_state_t *state, const GRECT *rect)
{
    if (state == NULL || rect == NULL) {
        return;
    }

    maestro_fill_rect(state->vdi_handle, maestro_paper, rect);
    maestro_draw_hline(state->vdi_handle, rect->g_x,
                       (WORD)(rect->g_x + rect->g_w - 1), rect->g_y);
    maestro_draw_text(state, rect, "Ready. Maestro tree prototype.", 0);
}

static void maestro_draw_tree(const maestro_state_t *state, const GRECT *rect)
{
    enum {
        maestro_tree_icon_width = 8,
        maestro_tree_icon_height = 16,
        maestro_tree_icon_gap = 8,
        maestro_tree_text_gap = 2,
        maestro_tree_control_width = 8,
        maestro_tree_control_height = 16,
        maestro_tree_pair_shift = 2
    };

    WORD row_h;
    WORD indent_w;
    WORD start_y;
    WORD visible_row;
    WORD ii;

    if (state == NULL || rect == NULL) {
        return;
    }

    maestro_fill_rect(state->vdi_handle, maestro_content_paper, rect);

    row_h = (WORD)(state->char_h + 2);
    if (row_h < 18) {
        row_h = 18;
    }
    indent_w = 12;
    if (indent_w < 12) {
        indent_w = 12;
    }
    start_y = (WORD)(rect->g_y + maestro_margin);
    visible_row = 0;

    for (ii = 0; ii < maestro_tree_node_count(); ++ii) {
        GRECT row_rect;
        GRECT text_rect;
        WORD lane_x;
        WORD control_x;
        WORD control_y;
        WORD icon_x;
        WORD icon_y;
        WORD icon_h = maestro_tree_icon_height;
        WORD icon_w = maestro_tree_icon_width;
        WORD control_h = maestro_tree_control_height;
        WORD control_icon = maestro_expand_icon;
        const maestro_node_t *node = &g_maestro_tree_nodes[ii];

        if (maestro_node_is_visible(state, ii) == 0) {
            continue;
        }

        row_rect.g_x = rect->g_x;
        row_rect.g_y = (WORD)(start_y + visible_row * row_h);
        row_rect.g_w = rect->g_w;
        row_rect.g_h = row_h;
        if (row_rect.g_y >= (WORD)(rect->g_y + rect->g_h)) {
            break;
        }

        lane_x = (WORD)(rect->g_x + 2 + node->depth * indent_w);
        control_x = (WORD)(lane_x + maestro_tree_pair_shift);
        icon_x = (WORD)(control_x + maestro_tree_control_width +
                        maestro_tree_icon_gap);
        if (maestro_node_has_children(ii) != 0) {
            control_icon = (state->expanded[ii] != 0) ? maestro_collapse_icon
                                                      : maestro_expand_icon;
        }
        if (maestro_node_has_children(ii) != 0 &&
            state->tree_icons_loaded != 0 && control_icon >= 0 &&
            control_icon < maestro_tree_icon_count) {
            control_h = state->tree_icons[control_icon].bi_hl;
            control_y = (WORD)(row_rect.g_y + (row_h - control_h) / 2);
            maestro_draw_bitmap(state->vdi_handle,
                                &state->tree_icons[control_icon], control_x,
                                control_y);
        }
        if (state->tree_icons_loaded != 0 && node->icon_index >= 0 &&
            node->icon_index < maestro_tree_icon_count) {
            icon_w = (WORD)(state->tree_icons[node->icon_index].bi_wb * 8);
            icon_h = state->tree_icons[node->icon_index].bi_hl;
        }
        icon_y = (WORD)(row_rect.g_y + (row_h - icon_h) / 2);
        if (state->tree_icons_loaded != 0 && node->icon_index >= 0 &&
            node->icon_index < maestro_tree_icon_count) {
            maestro_draw_bitmap(state->vdi_handle,
                                &state->tree_icons[node->icon_index], icon_x,
                                icon_y);
        }

        text_rect = row_rect;
        text_rect.g_x = (WORD)(icon_x + icon_w + maestro_tree_text_gap);
        text_rect.g_w = (WORD)(rect->g_x + rect->g_w - text_rect.g_x);
        maestro_draw_text_with_padding(state, &text_rect, node->label, 0, 0);
        ++visible_row;
    }
}

static void maestro_draw_file_pane(const maestro_state_t *state,
                                   const GRECT *rect)
{
    GRECT text_rect;

    if (state == NULL || rect == NULL) {
        return;
    }

    maestro_fill_rect(state->vdi_handle, maestro_content_paper, rect);
    text_rect = *rect;
    maestro_draw_text(state, &text_rect, "File pane placeholder", 0);
    text_rect.g_y = (WORD)(text_rect.g_y + state->char_h + maestro_margin);
    maestro_draw_text(state, &text_rect,
                      "Later this side will show file icons and details.", 0);
}

void maestro_draw_frame(maestro_state_t *state, const GRECT *dirty)
{
    GRECT work;
    GRECT visible;
    GRECT clip;
    GRECT tree;
    GRECT files;
    GRECT status;
    WORD clip_xy[4];
    WORD split_x;

    if (state == NULL) {
        return;
    }

    if (wind_get(state->handle, WF_WORKXYWH, &work.g_x, &work.g_y, &work.g_w,
                 &work.g_h) == 0) {
        return;
    }

    maestro_layout(state, &work, &tree, &files, &status);

    wind_update(BEG_UPDATE);
    wind_get(state->handle, WF_FIRSTXYWH, &visible.g_x, &visible.g_y,
             &visible.g_w, &visible.g_h);

    while (visible.g_w > 0 && visible.g_h > 0) {
        clip = visible;
        if (dirty != NULL) {
            maestro_intersect_rect(&clip, dirty, &clip);
        }

        if (clip.g_w > 0 && clip.g_h > 0) {
            clip_xy[0] = clip.g_x;
            clip_xy[1] = clip.g_y;
            clip_xy[2] = (WORD)(clip.g_x + clip.g_w - 1);
            clip_xy[3] = (WORD)(clip.g_y + clip.g_h - 1);
            vs_clip(state->vdi_handle, 1, clip_xy);

            maestro_draw_tree(state, &tree);
            maestro_draw_file_pane(state, &files);
            split_x = (WORD)(tree.g_x + tree.g_w);
            maestro_draw_vline(state->vdi_handle, split_x, tree.g_y,
                               (WORD)(tree.g_y + tree.g_h - 1));
            maestro_draw_status(state, &status);

            vs_clip(state->vdi_handle, 0, clip_xy);
        }

        wind_get(state->handle, WF_NEXTXYWH, &visible.g_x, &visible.g_y,
                 &visible.g_w, &visible.g_h);
    }

    wind_update(END_UPDATE);
}
