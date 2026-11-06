/*
 * Shared titled dialogs, parent-relative placement and clipped classic frames.
 * Blocking prompts keep edits private until accepted and handle window moves.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (C) 2026 tomaz stih
 */
#include <gem/gemctl.h>
#include <stdio.h>
#include <string.h>

WORD gemctl_dialog_open(WORD parent, const char *title, WORD width, WORD height,
                        GRECT *work)
{
    GRECT area, screen, outer;
    WORD kind = NAME | MOVER | CLOSER;
    wind_get(0, WF_CXYWH, &screen.g_x, &screen.g_y, &screen.g_w, &screen.g_h);
    area = screen;
    if (parent > 0)
        wind_get(parent, WF_WXYWH, &area.g_x, &area.g_y, &area.g_w, &area.g_h);
    wind_calc(WC_BORDER, kind, 0, 0, width, height, &outer.g_x, &outer.g_y,
              &outer.g_w, &outer.g_h);
    outer.g_x = area.g_x + (area.g_w - outer.g_w) / 2;
    outer.g_y = area.g_y + (area.g_h - outer.g_h) / 2;
    if (outer.g_x + outer.g_w > screen.g_x + screen.g_w)
        outer.g_x = screen.g_x + screen.g_w - outer.g_w;
    if (outer.g_y + outer.g_h > screen.g_y + screen.g_h)
        outer.g_y = screen.g_y + screen.g_h - outer.g_h;
    if (outer.g_x < screen.g_x)
        outer.g_x = screen.g_x;
    if (outer.g_y < screen.g_y)
        outer.g_y = screen.g_y;
    WORD handle = wind_create(kind, outer.g_x, outer.g_y, outer.g_w, outer.g_h);
    if (handle <= 0)
        return 0;
    wind_set_str(handle, WF_NAME, title);
    wind_open(handle, outer.g_x, outer.g_y, outer.g_w, outer.g_h);
    wind_get(handle, WF_CXYWH, &work->g_x, &work->g_y, &work->g_w, &work->g_h);
    return handle;
}

int gemctl_dialog_message(WORD handle, const WORD *message, GRECT *work)
{
    if (message[0] == AP_TERM ||
        (message[3] == handle && message[0] == WM_CLOSED))
        return -1;
    if (message[0] == WM_TOPPED)
        wind_set(handle, WF_TOP, 0, 0, 0, 0);
    if (message[3] == handle && message[0] == WM_MOVED)
        wind_set(handle, WF_WXYWH, message[4], message[5], message[6],
                 message[7]);
    wind_get(handle, WF_CXYWH, &work->g_x, &work->g_y, &work->g_w, &work->g_h);
    return 0;
}

void gemctl_dialog_frame(VDI_HANDLE vdi, const GRECT *work)
{
    vswr_mode(vdi, MD_REPLACE);
    vsl_type(vdi, 1);
    vsl_width(vdi, 1);
    for (WORD i = 0; i < 5; ++i) {
        WORD x = work->g_x + i, y = work->g_y + i;
        WORD r = work->g_x + work->g_w - 1 - i;
        WORD b = work->g_y + work->g_h - 1 - i;
        WORD points[10] = {x, y, r, y, r, b, x, b, x, y};
        vsl_color(vdi, i == 2 || i == 4 ? BLACK : WHITE);
        v_pline(vdi, 5, points);
    }
}

void gemctl_dialog_draw(WORD handle, VDI_HANDLE vdi, OBJECT *tree,
                        gemctl_text_edit_t *edit, WORD edit_object)
{
    GRECT work, clip;
    wind_get(handle, WF_CXYWH, &work.g_x, &work.g_y, &work.g_w, &work.g_h);
    tree[0].ob_x = work.g_x;
    tree[0].ob_y = work.g_y;
    wind_update(BEG_UPDATE);
    wind_get(handle, WF_FIRSTXYWH, &clip.g_x, &clip.g_y, &clip.g_w, &clip.g_h);
    while (clip.g_w > 0 && clip.g_h > 0) {
        objc_draw(tree, ROOT, MAX_DEPTH, clip.g_x, clip.g_y, clip.g_w,
                  clip.g_h);
        WORD bounds[4] = {clip.g_x, clip.g_y, clip.g_x + clip.g_w - 1,
                          clip.g_y + clip.g_h - 1};
        vs_clip(vdi, 1, bounds);
        gemctl_dialog_frame(vdi, &work);
        if (edit && edit_object > 0) {
            OBJECT *o = &tree[edit_object];
            GRECT field = {work.g_x + o->ob_x + 1, work.g_y + o->ob_y + 1,
                           o->ob_width - 2, o->ob_height - 2};
            gemctl_text_edit_draw(edit, vdi, &field, &clip);
        }
        vs_clip(vdi, 0, bounds);
        wind_get(handle, WF_NEXTXYWH, &clip.g_x, &clip.g_y, &clip.g_w,
                 &clip.g_h);
    }
    wind_update(END_UPDATE);
}

int gemctl_prompt(WORD parent, VDI_HANDLE vdi, const char *title,
                  const char *label, char *text, size_t capacity)
{
    OBJECT tree[5] = {
        {NIL, 1, 4, G_BOX, NONE, NORMAL, 0x00001170L, 0, 0, 480, 132},
        {2, NIL, NIL, G_STRING, NONE, NORMAL, 0, 12, 12, 450, 20},
        {3, NIL, NIL, G_BOX, NONE, NORMAL, 0x00011170L, 12, 42, 456, 28},
        {4, NIL, NIL, G_BUTTON, SELECTABLE | DEFAULT, NORMAL, 0, 234, 92, 110,
         26},
        {0, NIL, NIL, G_BUTTON, SELECTABLE | LASTOB, NORMAL, 0, 356, 92, 110,
         26}};
    char draft[1024];
    GRECT work;
    gemctl_text_edit_t edit;
    if (!text || !capacity || strlen(text) >= sizeof(draft))
        return 0;
    snprintf(draft, sizeof(draft), "%s", text);
    tree[1].ob_spec = (LONG)(intptr_t)label;
    tree[3].ob_spec = (LONG)(intptr_t) "OK";
    tree[4].ob_spec = (LONG)(intptr_t) "Cancel";
    WORD handle = gemctl_dialog_open(parent, title, 480, 132, &work);
    if (handle <= 0)
        return 0;
    gemctl_text_edit_init(&edit, draft,
                          capacity < sizeof(draft) ? capacity : sizeof(draft));
    gemctl_text_edit_focus(&edit, 1);
    int accepted = 0;
    for (;;) {
        WORD mx, my, buttons, modifiers, key, clicks, msg[8];
        gemctl_dialog_draw(handle, vdi, tree, &edit, 2);
        WORD event = evnt_multi(MU_BUTTON | MU_KEYBD | MU_MESAG, 1, 1, 1, 0, 0,
                                0, 0, 0, 0, 0, 0, 0, 0, msg, 0, 0, &mx, &my,
                                &buttons, &modifiers, &key, &clicks);
        if (!event || ((event & MU_MESAG) &&
                       gemctl_dialog_message(handle, msg, &work) < 0))
            break;
        if (event & MU_KEYBD) {
            gemctl_text_edit_result_t result =
                gemctl_text_edit_key_mod(&edit, key, modifiers);
            if (result == GEMCTL_TEXT_EDIT_CANCEL)
                break;
            if (result == GEMCTL_TEXT_EDIT_ACCEPT) {
                accepted = 1;
                break;
            }
        }
        if ((event & MU_BUTTON) && wind_find(mx, my) == handle) {
            WORD hit = objc_find(tree, 0, MAX_DEPTH, mx, my);
            if (hit == 3 || hit == 4) {
                accepted = hit == 3;
                break;
            }
            if (hit == 2) {
                GRECT field = {work.g_x + 13, work.g_y + 43, 454, 26};
                gemctl_text_edit_click(&edit, vdi, &field, mx,
                                       modifiers & K_LSHIFT);
            }
        }
    }
    wind_close(handle);
    wind_delete(handle);
    if (accepted)
        snprintf(text, capacity, "%s", draft);
    return accepted;
}
