/*
 * Ted's compact 16x16 monochrome command toolbar. Drawing and hit testing
 * share the standard chrome's square slots; partial slots stay hidden.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (C) 2026 tomaz stih
 */
#include "ted.h"

/* Standard-form bitmap words: one is ink, zero is the white background. */
static const UWORD toolbar_icons[][16] = {
    /* New */
    {0x0000, 0x1fc0, 0x1060, 0x1050, 0x1078, 0x1008, 0x1008, 0x1008, 0x1008,
     0x1008, 0x1008, 0x1008, 0x1008, 0x1008, 0x1ff8, 0x0000},
    /* Open */
    {0x0000, 0x0000, 0x0000, 0x0000, 0x7c00, 0x4200, 0x41fc, 0x4004, 0x4ffe,
     0x5002, 0x5004, 0x6004, 0x6008, 0x7ff8, 0x0000, 0x0000},
    /* Save */
    {0x0000, 0x3ffc, 0x2464, 0x2464, 0x2464, 0x27e4, 0x2004, 0x2004, 0x2ff4,
     0x2814, 0x2bd4, 0x2814, 0x2bd4, 0x2814, 0x3ffc, 0x0000},
    /* Save As */
    {0x0000, 0x3ffc, 0x2464, 0x2464, 0x2464, 0x27e4, 0x2000, 0x2000, 0x2e06,
     0x2809, 0x2a12, 0x2824, 0x2a48, 0x2890, 0x3ea0, 0x00c0},
    /* Undo */
    {0x0000, 0x0000, 0x0200, 0x0400, 0x0800, 0x1000, 0x3fe0, 0x1010, 0x0808,
     0x0404, 0x0204, 0x0004, 0x0004, 0x0008, 0x0010, 0x0000},
    /* Cut */
    {0x0000, 0x0000, 0x1008, 0x0810, 0x0420, 0x0240, 0x0180, 0x0180, 0x0240,
     0x0420, 0x7c3e, 0x4422, 0x4422, 0x4422, 0x7c3e, 0x0000},
    /* Copy */
    {0x0000, 0x7f00, 0x4180, 0x4140, 0x41e0, 0x47f0, 0x4438, 0x4434, 0x443e,
     0x4422, 0x4422, 0x7fe2, 0x0402, 0x0402, 0x07fe, 0x0000},
    /* Paste */
    {0x0000, 0x03e0, 0x0220, 0x1ffc, 0x13e4, 0x1004, 0x1004, 0x17f4, 0x1004,
     0x17f4, 0x1004, 0x17e4, 0x1004, 0x1004, 0x1ffc, 0x0000},
};
static const WORD toolbar_actions[] = {act_new,     act_open, act_save,
                                       act_save_as, act_undo, act_cut,
                                       act_copy,    act_paste};
static const char *const toolbar_hints[] = {
    "New (Ctrl+N)",           "Open (Ctrl+O)",        "Save (Ctrl+S)",
    "Save As (Ctrl+Shift+S)", "Undo / Redo (Ctrl+Z)", "Cut (Ctrl+X)",
    "Copy (Ctrl+C)",          "Paste (Ctrl+V)"};
enum { toolbar_count = sizeof(toolbar_actions) / sizeof(toolbar_actions[0]) };

static int toolbar_enabled(const ted_state_t *s, WORD action)
{
    if (action == act_cut || action == act_copy)
        return s->doc.cursor != s->doc.anchor;
    if (action == act_undo)
        return s->doc.undo != NULL;
    if (action == act_paste)
        return s->clipboard && s->clipboard[0];
    return 1;
}

WORD ted_toolbar_action(const ted_state_t *s, WORD x, WORD y)
{
    const GRECT *bar = &s->chrome.toolbar;
    WORD side = s->chrome.band_height;
    if (side < 18 || x < bar->g_x || x >= bar->g_x + bar->g_w || y < bar->g_y ||
        y >= bar->g_y + bar->g_h - 1)
        return 0;
    int slot = (x - bar->g_x) / side;
    if (slot >= toolbar_count || (slot + 1) * side > bar->g_w ||
        (x - bar->g_x) % side == side - 1)
        return 0;
    WORD action = toolbar_actions[slot];
    return toolbar_enabled(s, action) ? action : 0;
}

const char *ted_toolbar_hint(WORD action)
{
    for (int i = 0; i < toolbar_count; ++i)
        if (toolbar_actions[i] == action)
            return toolbar_hints[i];
    return NULL;
}

void ted_toolbar_draw(ted_state_t *s)
{
    const GRECT *bar = &s->chrome.toolbar;
    WORD side = s->chrome.band_height;
    gemctl_toolbar_draw(s->vdi, bar);
    if (side < 18)
        return;
    for (int i = 0; i < toolbar_count && (i + 1) * side <= bar->g_w; ++i) {
        UWORD words[16];
        for (int row = 0; row < 16; ++row) {
            UWORD mask = toolbar_enabled(s, toolbar_actions[i])
                             ? 0xffff
                             : (row & 1 ? 0x5555 : 0xaaaa);
            words[row] = toolbar_icons[i][row] & mask;
        }
        BITBLK bitmap = {(LONG)(intptr_t)words, 2, 16, 0, 0, WHITE};
        GRECT slot = {bar->g_x + i * side, bar->g_y, side, side};
        gemctl_toolbar_bitmap(s->vdi, &bitmap, &slot);
        gemctl_toolbar_separator(s->vdi, slot.g_x + side - 1, bar->g_y,
                                 bar->g_y + bar->g_h - 1);
    }
}
