/*
 * Implements the Maestro file explorer sample's shell: the tree model with
 * expand/collapse state, resource icon loading, layout, window lifecycle
 * and the event loop. Painting lives in maestro_draw.c.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (C) 2026 tomaz stih
 */

#include "maestro_private.h"
#include <gem/os.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

const maestro_node_t g_maestro_tree_nodes[] = {
    {"Computer", 0, maestro_folder_icon},
    {"Home", 1, maestro_folder_icon},
    {"Documents", 2, maestro_folder_icon},
    {"Roadmap.txt", 3, maestro_document_icon},
    {"Mail", 2, maestro_folder_icon},
    {"Inbox.msg", 3, maestro_document_icon},
    {"System", 1, maestro_folder_icon},
    {"Readme.doc", 2, maestro_document_icon}};

int maestro_node_has_children(WORD index)
{
    WORD ii;
    WORD count;
    WORD depth;

    count =
        (WORD)(sizeof(g_maestro_tree_nodes) / sizeof(g_maestro_tree_nodes[0]));
    if (index < 0 || index >= count) {
        return 0;
    }
    depth = g_maestro_tree_nodes[index].depth;
    for (ii = (WORD)(index + 1); ii < count; ++ii) {
        if (g_maestro_tree_nodes[ii].depth <= depth) {
            return 0;
        }
        if (g_maestro_tree_nodes[ii].depth == (WORD)(depth + 1)) {
            return 1;
        }
    }

    return 0;
}

WORD maestro_tree_node_count(void)
{
    return (WORD)(sizeof(g_maestro_tree_nodes) /
                  sizeof(g_maestro_tree_nodes[0]));
}

int maestro_node_is_visible(const maestro_state_t *state, WORD index)
{
    WORD target_depth;
    WORD ii;

    if (state == NULL || index < 0 || index >= maestro_tree_node_count()) {
        return 0;
    }
    if (g_maestro_tree_nodes[index].depth <= 0) {
        return 1;
    }

    target_depth = (WORD)(g_maestro_tree_nodes[index].depth - 1);
    for (ii = (WORD)(index - 1); ii >= 0; --ii) {
        if (g_maestro_tree_nodes[ii].depth == target_depth) {
            if (maestro_node_has_children(ii) != 0 &&
                state->expanded[ii] == 0) {
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

static void maestro_free_tree_icons(maestro_state_t *state)
{
    WORD ii;

    if (state == NULL) {
        return;
    }

    for (ii = 0; ii < maestro_tree_icon_count; ++ii) {
        sample_free_bitblk(&state->tree_icons[ii]);
    }
    state->tree_icons_loaded = 0;
}

static int maestro_load_tree_icons(maestro_state_t *state)
{
    if (state == NULL) {
        return 0;
    }

    state->tree_icons_loaded =
        sample_load_bitblks("maestro_tree.rsc", "maestro_tree.rsc",
                            state->tree_icons, maestro_tree_icon_count);
    return state->tree_icons_loaded;
}

void maestro_layout(const maestro_state_t *state, const GRECT *work,
                    GRECT *tree, GRECT *files, GRECT *status)
{
    WORD band_h;
    WORD status_y;
    WORD tree_w;

    if (state == NULL || work == NULL || tree == NULL || files == NULL ||
        status == NULL) {
        return;
    }

    band_h = (WORD)(state->box_h - 2);
    if (band_h < state->char_h + maestro_inner_gap) {
        band_h = (WORD)(state->char_h + maestro_inner_gap);
    }

    status_y = (WORD)(work->g_y + work->g_h - band_h);
    status->g_x = work->g_x;
    status->g_y = status_y;
    status->g_w = work->g_w;
    status->g_h = band_h;

    tree_w = (WORD)(work->g_w / 3);
    if (tree_w < 180) {
        tree_w = 180;
    }
    if (tree_w > work->g_w - 120) {
        tree_w = (WORD)(work->g_w - 120);
    }

    tree->g_x = work->g_x;
    tree->g_y = work->g_y;
    tree->g_w = tree_w;
    tree->g_h = (WORD)(status->g_y - work->g_y);

    files->g_x = (WORD)(tree->g_x + tree->g_w + 1);
    files->g_y = work->g_y;
    files->g_w = (WORD)(work->g_x + work->g_w - files->g_x);
    files->g_h = tree->g_h;
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
    if (wind_get(state->handle, WF_WORKXYWH, &work.g_x, &work.g_y, &work.g_w,
                 &work.g_h) == 0) {
        return 0;
    }

    maestro_layout(state, &work, &tree, &files, &status);
    if (mouse_x < tree.g_x || mouse_x >= (WORD)(tree.g_x + tree.g_w) ||
        mouse_y < tree.g_y || mouse_y >= (WORD)(tree.g_y + tree.g_h)) {
        return 0;
    }

    row_h = (WORD)(state->char_h + 2);
    if (row_h < 18) {
        row_h = 18;
    }
    indent_w = 12;
    row_y = (WORD)(tree.g_y + maestro_margin);

    for (ii = 0; ii < maestro_tree_node_count(); ++ii) {
        WORD lane_x;
        WORD control_x;
        WORD control_y;

        if (maestro_node_is_visible(state, ii) == 0) {
            continue;
        }
        if (mouse_y >= row_y && mouse_y < (WORD)(row_y + row_h) &&
            maestro_node_has_children(ii) != 0) {
            lane_x = (WORD)(tree.g_x + 2 +
                            g_maestro_tree_nodes[ii].depth * indent_w);
            control_x = (WORD)(lane_x + maestro_tree_pair_shift);
            control_y = row_y;
            if (mouse_x >= control_x &&
                mouse_x < (WORD)(control_x + maestro_tree_control_width) &&
                mouse_y >= control_y &&
                mouse_y < (WORD)(control_y + maestro_tree_control_height)) {
                state->expanded[ii] = (uint8_t)(state->expanded[ii] == 0);
                return 1;
            }
        }
        row_y = (WORD)(row_y + row_h);
    }

    return 0;
}

static void maestro_toggle_full(maestro_state_t *state)
{
    GRECT full;
    GRECT current;

    if (state == NULL) {
        return;
    }

    if (wind_get(0, WF_WORKXYWH, &full.g_x, &full.g_y, &full.g_w, &full.g_h) ==
        0) {
        return;
    }
    if (wind_get(state->handle, WF_WXYWH, &current.g_x, &current.g_y,
                 &current.g_w, &current.g_h) == 0) {
        return;
    }

    if (state->full_open == 0) {
        state->normal_rect = current;
        (void)wind_set(state->handle, WF_WXYWH, full.g_x, full.g_y, full.g_w,
                       full.g_h);
        state->full_open = 1;
    } else {
        (void)wind_set(state->handle, WF_WXYWH, state->normal_rect.g_x,
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
    state.vdi_handle =
        graf_handle(&state.char_w, &state.char_h, &state.box_w, &state.box_h);
    if (state.vdi_handle == 0) {
        appl_exit();
        gem_os_shutdown();
        return 1;
    }

    (void)maestro_load_tree_icons(&state);

    window_kind = (UWORD)(NAME | CLOSER | FULLER | MOVER | SIZER | UPARROW |
                          DNARROW | VSLIDE | LFARROW | RTARROW | HSLIDE);

    state.handle = wind_create(window_kind, 0, 0, 640, 400);
    if (state.handle <= 0) {
        maestro_free_tree_icons(&state);
        appl_exit();
        gem_os_shutdown();
        return 1;
    }

    (void)wind_set_str(state.handle, WF_NAME, "Maestro");
    (void)wind_calc(WC_BORDER, window_kind, 72, 48, maestro_work_width,
                    maestro_work_height, &outer_x, &outer_y, &outer_w,
                    &outer_h);
    state.normal_rect.g_x = outer_x;
    state.normal_rect.g_y = outer_y;
    state.normal_rect.g_w = outer_w;
    state.normal_rect.g_h = outer_h;
    (void)wind_open(state.handle, outer_x, outer_y, outer_w, outer_h);
    (void)graf_mouse(M_ON, NULL);

    FOREVER
    {
        event_flags =
            evnt_multi((UWORD)(MU_MESAG | MU_BUTTON | MU_KEYBD), 1, 1, 1, 0, 0,
                       0, 0, 0, 0, 0, 0, 0, 0, msg, 0, 0, &mouse_x, &mouse_y,
                       &mouse_buttons, &key_state, &key_code, &button_return);

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
                (void)wind_set(state.handle, WF_WXYWH, msg[4], msg[5], msg[6],
                               msg[7]);
                state.normal_rect.g_x = msg[4];
                state.normal_rect.g_y = msg[5];
                state.normal_rect.g_w = msg[6];
                state.normal_rect.g_h = msg[7];
                state.full_open = 0;
            } else if (msg[0] == WM_FULLED) {
                maestro_toggle_full(&state);
            } else if (msg[0] == WM_TOPPED) {
                (void)wind_set(state.handle, WF_TOP, 0, 0, 0, 0);
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
        (void)wind_close(state.handle);
        (void)wind_delete(state.handle);
    }
    maestro_free_tree_icons(&state);
    appl_exit();
    gem_os_shutdown();
    return 0;
}
