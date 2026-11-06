/*
 * Declares the Maestro sample's tree model and the painting helpers its
 * shell and drawing modules share. Private to the sample.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (C) 2026 tomaz stih
 */

#ifndef GEM_SAMPLE_MAESTRO_PRIVATE_H
#define GEM_SAMPLE_MAESTRO_PRIVATE_H

#include <gem.h>
#include "sample_resources.h"

#include "maestro.h"

#include <stdint.h>

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

/* The demo tree, defined in maestro_ui.c; nodes are pre-order with depths. */
extern const maestro_node_t g_maestro_tree_nodes[];

/* Paint the whole window frame and panes within a rectangle. */
void maestro_draw_frame(maestro_state_t *state, const GRECT *dirty);
/* Compute pane rectangles from the work area. */
void maestro_layout(const maestro_state_t *state, const GRECT *work,
                    GRECT *tree, GRECT *files, GRECT *status);
/* Nonzero when a tree node has children. */
int maestro_node_has_children(WORD index);
/* Nonzero when every ancestor of a node is expanded. */
int maestro_node_is_visible(const maestro_state_t *state, WORD index);
/* Number of nodes in the tree. */
WORD maestro_tree_node_count(void);

#endif
