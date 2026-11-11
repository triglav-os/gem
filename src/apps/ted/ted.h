/*
 * Private Ted window state shared by editing and viewport rendering.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (C) 2026 tomaz stih
 */
#ifndef TED_H
#define TED_H
#include "document.h"
#include <gem.h>
#include <gem/gemctl.h>
#include <platform/os.h>

enum {
    menu_root,
    menu_bar_object,
    menu_titles,
    menu_file,
    menu_edit,
    menu_boxes,
    menu_file_box,
    act_new,
    act_open,
    act_save,
    act_save_as,
    act_quit,
    menu_edit_box,
    act_undo,
    act_cut,
    act_copy,
    act_paste,
    act_all,
    menu_count
};
typedef struct ted_state {
    ted_document_t doc;
    WORD window, vdi, cw, ch, char_h, box_h;
    OBJECT menu[menu_count];
    GRECT work, body;
    gemctl_chrome_t chrome;
    WORD hover_action;
    size_t top, left;
    int dragging, running, crlf;
    char path[GEM_OS_PATH_MAX], *clipboard;
    /* Nonzero while Ted shows the I-beam over its text area. */
    int text_pointer;
} ted_state_t;

/* Refresh the editable work rectangle after opening, moving or resizing. */
void ted_layout(ted_state_t *s);
/* Draw the 16x16 toolbar icons using the shared chrome dimensions. */
void ted_toolbar_draw(ted_state_t *s);
/* Return the enabled toolbar command at a pointer position, or zero. */
WORD ted_toolbar_action(const ted_state_t *s, WORD x, WORD y);
/* Return a toolbar command's status hint, or NULL for ordinary document status.
 */
const char *ted_toolbar_hint(WORD action);
/* Return the tab-expanded character column between byte offsets. */
size_t ted_column(ted_state_t *s, size_t start, size_t end);
/* Map a visual line and column to a character boundary. */
size_t ted_offset(ted_state_t *s, size_t line, size_t column);
/* Scroll enough to reveal the current cursor. */
void ted_follow(ted_state_t *s);
/* Repaint visible work areas and refresh the title and scrollbar metrics. */
void ted_draw(ted_state_t *s);
#endif
