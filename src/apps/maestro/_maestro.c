/*
 * Implements the first hosted GEM Maestro file explorer shell. This
 * version focuses on a split-pane frame with a custom-drawn tree view
 * on the left, a placeholder file pane on the right, and a status bar
 * that fits visually with native GEM metrics.
 *
 * MIT License (see: LICENSE)
 * Copyright (C) 2026 tomaz stih
 */

#include <gem.h>

#include "maestro.h"
#include "platform/os.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

enum {
    maestro_work_width = 560,
    maestro_work_height = 360,
    maestro_margin = 6,
    maestro_inner_gap = 4,
    maestro_max_nodes = 32,
    maestro_tree_icon_count = 4,
    maestro_folder_icon = 0,
    maestro_document_icon = 1,
    maestro_expand_icon = 2,
    maestro_collapse_icon = 3
};

enum {
    maestro_paper = BLACK,
    maestro_ink = WHITE,
    maestro_content_paper = BLACK
};

typedef struct maestro_node {
    const char *label;
    WORD depth;
    WORD icon_index;
} maestro_node_t;

typedef struct maestro_state {
    WORD handle;
    WORD vdi_handle;
    WORD char_w;
    WORD char_h;
    WORD box_w;
    WORD box_h;
    BITBLK tree_icons[maestro_tree_icon_count];
    int tree_icons_loaded;
    uint8_t expanded[maestro_max_nodes];
    GRECT normal_rect;
    int full_open;
} maestro_state_t;

static const maestro_node_t g_maestro_tree_nodes[] = {
    { "Computer", 0, maestro_folder_icon },
    { "Home", 1, maestro_folder_icon },
    { "Documents", 2, maestro_folder_icon },
    { "Roadmap.txt", 3, maestro_document_icon },
    { "Mail", 2, maestro_folder_icon },
    { "Inbox.msg", 3, maestro_document_icon },
    { "System", 1, maestro_folder_icon },
    { "Readme.doc", 2, maestro_document_icon }
};

static int maestro_node_has_children(WORD index)
{
    WORD ii;
    WORD count;
    WORD depth;

    count = (WORD) (sizeof(g_maestro_tree_nodes) /
        sizeof(g_maestro_tree_nodes[0]));
    if (index < 0 || index >= count) {
        return 0;
    }
    depth = g_maestro_tree_nodes[index].depth;
    for (ii = (WORD) (index + 1); ii < count; ++ii) {
        if (g_maestro_tree_nodes[ii].depth <= depth) {
            return 0;
        }
        if (g_maestro_tree_nodes[ii].depth == (WORD) (depth + 1)) {
            return 1;
        }
    }

    return 0;
}

static WORD maestro_tree_node_count(void)
{
    return (WORD) (sizeof(g_maestro_tree_nodes) /
        sizeof(g_maestro_tree_nodes[0]));
}

static int maestro_node_is_visible(const maestro_state_t *state, WORD index)
{
    WORD target_depth;
    WORD ii;

    if (state == NULL || index < 0 || index >= maestro_tree_node_count()) {
        return 0;
    }
    if (g_maestro_tree_nodes[index].depth <= 0) {
        return 1;
    }

    target_depth = (WORD) (g_maestro_tree_nodes[index].depth - 1);
    for (ii = (WORD) (index - 1); ii >= 0; --ii) {
        if (g_maestro_tree_nodes[ii].depth == target_depth) {
            if (maestro_node_has_children(ii) != 0 && state->expanded[ii] == 0) {
                return 0;
            }
            if (target_depth == 0) {
                return 1;
            }
            --target_depth;
        }
        if (ii == 0) {
            break;
        }
    }

    return 1;
}

static void maestro_init_tree_state(maestro_state_t *state)
{
    WORD ii;
    WORD count;

    if (state == NULL) {
        return;
    }

    memset(state->expanded, 0, sizeof(state->expanded));
    count = maestro_tree_node_count();
    for (ii = 0; ii < count; ++ii) {
        if (maestro_node_has_children(ii) != 0) {
            state->expanded[ii] = 1;
        }
    }
}

static void maestro_free_bitblk(BITBLK *bitblk)
{
    if (bitblk == NULL) {
        return;
    }

    free((void *) (intptr_t) bitblk->bi_pdata);
    memset(bitblk, 0, sizeof(*bitblk));
}

static void maestro_free_tree_icons(maestro_state_t *state)
{
    WORD ii;

    if (state == NULL) {
        return;
    }

    for (ii = 0; ii < maestro_tree_icon_count; ++ii) {
        maestro_free_bitblk(&state->tree_icons[ii]);
    }
    state->tree_icons_loaded = 0;
}

static int maestro_clone_bitblk(BITBLK *dst, const BITBLK *src)
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

static int maestro_load_bitblks_from_resource(const char *primary_path,
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
                maestro_free_bitblk(&bitblks[ii]);
            }
            rsrc_free();
            return 0;
        }
        if (!maestro_clone_bitblk(&bitblks[ii], bitblk)) {
            while (--ii >= 0) {
                maestro_free_bitblk(&bitblks[ii]);
            }
            rsrc_free();
            return 0;
        }
    }

    rsrc_free();
    return 1;
}

static int maestro_load_tree_icons(maestro_state_t *state)
{
    if (state == NULL) {
        return 0;
    }

    state->tree_icons_loaded = maestro_load_bitblks_from_resource(
        "bin/maestro_tree.rsc",
        "maestro_tree.rsc",
        state->tree_icons,
        maestro_tree_icon_count);
    return state->tree_icons_loaded;
}

static void maestro_intersect_rect(const GRECT *a, const GRECT *b,
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

static void maestro_fill_rect(WORD handle, WORD color, const GRECT *rect)
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

static void maestro_draw_hline(WORD handle, WORD x0, WORD x1, WORD y)
{
    WORD pxy[4];

    if (x1 < x0) {
        return;
    }

    (void) vsl_type(handle, 1);
    (void) vsl_width(handle, 1);
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

    (void) vsl_type(handle, 1);
    (void) vsl_width(handle, 1);
    vsl_color(handle, maestro_ink);
    pxy[0] = x;
    pxy[1] = y0;
    pxy[2] = x;
    pxy[3] = y1;
    v_pline(handle, 2, pxy);
}

static void maestro_draw_text_with_padding(const maestro_state_t *state,
    const GRECT *rect, const char *text, int centered, WORD left_padding)
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
        x = (WORD) (rect->g_x + left_padding);
    }
    top_y = (WORD) (rect->g_y + (rect->g_h - text_height) / 2);
    y = (WORD) (top_y + ascent);

    vst_color(state->vdi_handle, maestro_ink);
    v_gtext(state->vdi_handle, x, y, (const BYTE *) text);
}

static void maestro_draw_text(const maestro_state_t *state,
    const GRECT *rect, const char *text, int centered)
{
    maestro_draw_text_with_padding(state, rect, text, centered,
        maestro_margin);
}

static void maestro_draw_bitmap(WORD handle, const BITBLK *bitblk,
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

static void maestro_layout(const maestro_state_t *state, const GRECT *work,
    GRECT *tree, GRECT *files, GRECT *status)
{
    WORD band_h;
    WORD status_y;
    WORD tree_w;

    if (state == NULL || work == NULL || tree == NULL || files == NULL ||
        status == NULL) {
        return;
    }

    band_h = (WORD) (state->box_h - 2);
    if (band_h < state->char_h + maestro_inner_gap) {
        band_h = (WORD) (state->char_h + maestro_inner_gap);
    }

    status_y = (WORD) (work->g_y + work->g_h - band_h);
    status->g_x = work->g_x;
    status->g_y = status_y;
    status->g_w = work->g_w;
    status->g_h = band_h;

    tree_w = (WORD) (work->g_w / 3);
    if (tree_w < 180) {
        tree_w = 180;
    }
    if (tree_w > work->g_w - 120) {
        tree_w = (WORD) (work->g_w - 120);
    }

    tree->g_x = work->g_x;
    tree->g_y = work->g_y;
    tree->g_w = tree_w;
    tree->g_h = (WORD) (status->g_y - work->g_y);

    files->g_x = (WORD) (tree->g_x + tree->g_w + 1);
    files->g_y = work->g_y;
    files->g_w = (WORD) (work->g_x + work->g_w - files->g_x);
    files->g_h = tree->g_h;
}

static void maestro_draw_status(const maestro_state_t *state,
    const GRECT *rect)
{
    if (state == NULL || rect == NULL) {
        return;
    }

    maestro_fill_rect(state->vdi_handle, maestro_paper, rect);
    maestro_draw_hline(state->vdi_handle, rect->g_x,
        (WORD) (rect->g_x + rect->g_w - 1), rect->g_y);
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

    row_h = (WORD) (state->char_h + 2);
    if (row_h < 18) {
        row_h = 18;
    }
    indent_w = 12;
    if (indent_w < 12) {
        indent_w = 12;
    }
    start_y = (WORD) (rect->g_y + maestro_margin);
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
        row_rect.g_y = (WORD) (start_y + visible_row * row_h);
        row_rect.g_w = rect->g_w;
        row_rect.g_h = row_h;
        if (row_rect.g_y >= (WORD) (rect->g_y + rect->g_h)) {
            break;
        }

        lane_x = (WORD) (rect->g_x + 2 + node->depth * indent_w);
        control_x = (WORD) (lane_x + maestro_tree_pair_shift);
        control_y = row_rect.g_y;
        icon_x = (WORD) (control_x + maestro_tree_control_width +
            maestro_tree_icon_gap);
        if (maestro_node_has_children(ii) != 0) {
            control_icon = (state->expanded[ii] != 0) ?
                maestro_collapse_icon : maestro_expand_icon;
        }
        if (maestro_node_has_children(ii) != 0 &&
            state->tree_icons_loaded != 0 &&
            control_icon >= 0 && control_icon < maestro_tree_icon_count) {
            control_h = state->tree_icons[control_icon].bi_hl;
            control_y = (WORD) (row_rect.g_y + (row_h - control_h) / 2);
            maestro_draw_bitmap(state->vdi_handle,
                &state->tree_icons[control_icon], control_x, control_y);
        }
        if (state->tree_icons_loaded != 0 &&
            node->icon_index >= 0 &&
            node->icon_index < maestro_tree_icon_count) {
            icon_w = (WORD) (state->tree_icons[node->icon_index].bi_wb * 8);
            icon_h = state->tree_icons[node->icon_index].bi_hl;
        }
        icon_y = (WORD) (row_rect.g_y + (row_h - icon_h) / 2);
        if (state->tree_icons_loaded != 0 &&
            node->icon_index >= 0 &&
            node->icon_index < maestro_tree_icon_count) {
            maestro_draw_bitmap(state->vdi_handle,
                &state->tree_icons[node->icon_index], icon_x, icon_y);
        }

        text_rect = row_rect;
        text_rect.g_x = (WORD) (icon_x + icon_w + maestro_tree_text_gap);
        text_rect.g_w = (WORD) (rect->g_x + rect->g_w - text_rect.g_x);
        maestro_draw_text_with_padding(state, &text_rect, node->label, 0, 0);
        ++visible_row;
    }
}

static int maestro_toggle_tree_node_at(maestro_state_t *state, WORD mouse_x,
    WORD mouse_y)
{
    enum {
        maestro_tree_control_width = 8,
        maestro_tree_control_height = 16,
        maestro_tree_pair_shift = 2
    };

    GRECT work;
    GRECT tree;
    GRECT files;
    GRECT status;
    WORD row_h;
    WORD indent_w;
    WORD row_y;
    WORD ii;

    if (state == NULL) {
        return 0;
    }
    if (wind_get(state->handle, WF_WORKXYWH, &work.g_x, &work.g_y,
            &work.g_w, &work.g_h) == 0) {
        return 0;
    }

    maestro_layout(state, &work, &tree, &files, &status);
    if (mouse_x < tree.g_x || mouse_x >= (WORD) (tree.g_x + tree.g_w) ||
        mouse_y < tree.g_y || mouse_y >= (WORD) (tree.g_y + tree.g_h)) {
        return 0;
    }

    row_h = (WORD) (state->char_h + 2);
    if (row_h < 18) {
        row_h = 18;
    }
    indent_w = 12;
    row_y = (WORD) (tree.g_y + maestro_margin);

    for (ii = 0; ii < maestro_tree_node_count(); ++ii) {
        WORD lane_x;
        WORD control_x;
        WORD control_y;

        if (maestro_node_is_visible(state, ii) == 0) {
            continue;
        }
        if (mouse_y >= row_y && mouse_y < (WORD) (row_y + row_h) &&
            maestro_node_has_children(ii) != 0) {
            lane_x = (WORD) (tree.g_x + 2 + g_maestro_tree_nodes[ii].depth *
                indent_w);
            control_x = (WORD) (lane_x + maestro_tree_pair_shift);
            control_y = row_y;
            if (mouse_x >= control_x &&
                mouse_x < (WORD) (control_x + maestro_tree_control_width) &&
                mouse_y >= control_y &&
                mouse_y < (WORD) (control_y + maestro_tree_control_height)) {
                state->expanded[ii] = (uint8_t) (state->expanded[ii] == 0);
                return 1;
            }
        }
        row_y = (WORD) (row_y + row_h);
    }

    return 0;
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
    text_rect.g_y = (WORD) (text_rect.g_y + state->char_h + maestro_margin);
    maestro_draw_text(state, &text_rect,
        "Later this side will show file icons and details.", 0);
}

static void maestro_draw_frame(maestro_state_t *state, const GRECT *dirty)
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

    if (wind_get(state->handle, WF_WORKXYWH, &work.g_x, &work.g_y,
            &work.g_w, &work.g_h) == 0) {
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
            clip_xy[2] = (WORD) (clip.g_x + clip.g_w - 1);
            clip_xy[3] = (WORD) (clip.g_y + clip.g_h - 1);
            vs_clip(state->vdi_handle, 1, clip_xy);

            maestro_draw_tree(state, &tree);
            maestro_draw_file_pane(state, &files);
            split_x = (WORD) (tree.g_x + tree.g_w);
            maestro_draw_vline(state->vdi_handle, split_x, tree.g_y,
                (WORD) (tree.g_y + tree.g_h - 1));
            maestro_draw_status(state, &status);

            vs_clip(state->vdi_handle, 0, clip_xy);
        }

        wind_get(state->handle, WF_NEXTXYWH, &visible.g_x, &visible.g_y,
            &visible.g_w, &visible.g_h);
    }

    wind_update(END_UPDATE);
}

static void maestro_toggle_full(maestro_state_t *state)
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

int maestro_main(void)
{
    maestro_state_t state;
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
    maestro_init_tree_state(&state);
    state.vdi_handle = graf_handle(&state.char_w, &state.char_h,
        &state.box_w, &state.box_h);
    if (state.vdi_handle == 0) {
        appl_exit();
        gem_os_shutdown();
        return 1;
    }

    (void) maestro_load_tree_icons(&state);

    window_kind = (UWORD) (NAME | CLOSER | FULLER | MOVER | SIZER |
        UPARROW | DNARROW | VSLIDE | LFARROW | RTARROW | HSLIDE);

    state.handle = wind_create(window_kind, 0, 0, 640, 400);
    if (state.handle <= 0) {
        maestro_free_tree_icons(&state);
        appl_exit();
        gem_os_shutdown();
        return 1;
    }

    (void) wind_set_str(state.handle, WF_NAME, "Maestro");
    (void) wind_calc(WC_BORDER, window_kind, 72, 48, maestro_work_width,
        maestro_work_height, &outer_x, &outer_y, &outer_w, &outer_h);
    state.normal_rect.g_x = outer_x;
    state.normal_rect.g_y = outer_y;
    state.normal_rect.g_w = outer_w;
    state.normal_rect.g_h = outer_h;
    (void) wind_open(state.handle, outer_x, outer_y, outer_w, outer_h);
    (void) graf_mouse(M_ON, NULL);

    FOREVER {
        event_flags = evnt_multi((UWORD) (MU_MESAG | MU_BUTTON | MU_KEYBD),
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
                maestro_draw_frame(&state, &redraw);
            } else if (msg[0] == WM_MOVED || msg[0] == WM_SIZED) {
                (void) wind_set(state.handle, WF_WXYWH, msg[4], msg[5],
                    msg[6], msg[7]);
                state.normal_rect.g_x = msg[4];
                state.normal_rect.g_y = msg[5];
                state.normal_rect.g_w = msg[6];
                state.normal_rect.g_h = msg[7];
                state.full_open = 0;
            } else if (msg[0] == WM_FULLED) {
                maestro_toggle_full(&state);
            } else if (msg[0] == WM_TOPPED) {
                (void) wind_set(state.handle, WF_TOP, 0, 0, 0, 0);
            }
        }

        if ((event_flags & MU_BUTTON) != 0 && (mouse_buttons & 1) != 0) {
            if (maestro_toggle_tree_node_at(&state, mouse_x, mouse_y) != 0) {
                maestro_draw_frame(&state, NULL);
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
    maestro_free_tree_icons(&state);
    appl_exit();
    gem_os_shutdown();
    return 0;
}
