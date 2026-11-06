/*
 * Native filename editing and the shared File Manager/Trash context menu.
 * Real GEM windows keep these transient forms above application overlays.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (C) 2026 tomaz stih
 */
#include "desktop.h"
#include <gem/gemctl.h>
#include <stdio.h>
#include <string.h>

static WORD dialog_open(OBJECT *tree)
{
    WORD x, y, w, h, handle;
    wind_calc(WC_BORDER, 0, tree[0].ob_x, tree[0].ob_y, tree[0].ob_width,
              tree[0].ob_height, &x, &y, &w, &h);
    handle = wind_create(0, x, y, w, h);
    if (handle <= 0)
        return 0;
    wind_open(handle, x, y, w, h);
    form_dial(FMD_START, 0, 0, 0, 0, tree[0].ob_x, tree[0].ob_y,
              tree[0].ob_width, tree[0].ob_height);
    return handle;
}

static void dialog_close(OBJECT *tree, WORD handle)
{
    form_dial(FMD_FINISH, 0, 0, 0, 0, tree[0].ob_x, tree[0].ob_y,
              tree[0].ob_width, tree[0].ob_height);
    wind_close(handle);
    wind_delete(handle);
}

int desktop_name_dialog(const char *title, char *text, size_t capacity)
{
    OBJECT tree[5];
    gemctl_text_edit_t editor;
    char draft[256];
    WORD x, y, w, h, handle;
    int accepted = 0;
    snprintf(draft, sizeof(draft), "%s", text);
    desktop_init_object(&tree[0], NIL, 1, 4, G_BOX, NONE, NORMAL, 0x00021170L,
                        0, 0, 480, 132);
    desktop_init_object(&tree[1], 2, NIL, NIL, G_STRING, NONE, NORMAL,
                        (LONG)(intptr_t)title, 12, 12, 450, 20);
    desktop_init_object(&tree[2], 3, NIL, NIL, G_BOX, NONE, NORMAL, 0x00011170L,
                        12, 42, 456, 28);
    desktop_init_object(&tree[3], 4, NIL, NIL, G_BUTTON, SELECTABLE | DEFAULT,
                        NORMAL, (LONG)(intptr_t) "OK", 234, 92, 110, 26);
    desktop_init_object(&tree[4], 0, NIL, NIL, G_BUTTON, SELECTABLE | LASTOB,
                        NORMAL, (LONG)(intptr_t) "Cancel", 356, 92, 110, 26);
    form_center(tree, &x, &y, &w, &h);
    tree[0].ob_x = x;
    tree[0].ob_y = y;
    handle = dialog_open(tree);
    if (!handle)
        return 0;
    GRECT clip = {x, y, w, h};
    GRECT field = {(WORD)(x + 13), (WORD)(y + 43), 454, 26};
    gemctl_text_edit_init(&editor, draft, sizeof(draft));
    gemctl_text_edit_focus(&editor, 1);
    for (;;) {
        WORD mx, my, buttons, modifiers, key, clicks, msg[8];
        objc_draw(tree, 0, MAX_DEPTH, x, y, w, h);
        gemctl_text_edit_draw(&editor, g_desktop.vdi_handle, &field, &clip);
        WORD event = evnt_multi(MU_BUTTON | MU_KEYBD | MU_MESAG, 1, 1, 1, 0, 0,
                                0, 0, 0, 0, 0, 0, 0, 0, msg, 0, 0, &mx, &my,
                                &buttons, &modifiers, &key, &clicks);
        if (!event)
            break;
        if ((event & MU_MESAG) && msg[0] == AP_TERM)
            break;
        if (event & MU_KEYBD) {
            if ((key & 255) == 27)
                break;
            if ((key & 255) == 13 || (key & 255) == 10) {
                accepted = 1;
                break;
            }
            gemctl_text_edit_key_mod(&editor, (uint16_t)key,
                                     (uint16_t)modifiers);
        }
        if (event & MU_BUTTON) {
            WORD hit = objc_find(tree, 0, MAX_DEPTH, mx, my);
            if (hit == 3 || hit == 4) {
                accepted = hit == 3;
                break;
            }
            if (hit == 2)
                gemctl_text_edit_click(&editor, g_desktop.vdi_handle, &field,
                                       mx, modifiers & K_LSHIFT);
        }
    }
    WORD no_clip[4] = {0};
    vs_clip(g_desktop.vdi_handle, 0, no_clip);
    dialog_close(tree, handle);
    if (accepted)
        snprintf(text, capacity, "%s", draft);
    return accepted;
}

void desktop_file_popup(WORD handle, WORD x, WORD y)
{
    static const char *const labels[] = {"  Open",
                                         "  Delete",
                                         "  Restore",
                                         "  Empty Trash",
                                         "  Rename       F2",
                                         "  Cut          ^X",
                                         "  Copy         ^C",
                                         "  Paste        ^V",
                                         "  New Folder",
                                         "  New Text File",
                                         "  New Markdown File",
                                         "  New HTML File",
                                         "  New CSV File",
                                         "  Select All   ^A"};
    enum { count = sizeof(labels) / sizeof(labels[0]), row_height = 20 };
    OBJECT tree[count + 1];
    desktop_browser_window_t *browser = desktop_find_browser_by_handle(handle);
    WORD hit, chosen = NIL, hovered = NIL, window;
    if (browser == NULL)
        return;
    desktop_browser_sync_work(browser);
    if (x < browser->work.g_x || y < browser->work.g_y ||
        x >= browser->work.g_x + browser->work.g_w ||
        y >= browser->work.g_y + browser->work.g_h)
        return;
    g_desktop.active_browser_handle = handle;
    desktop_select_icon(NIL);
    hit = desktop_browser_entry_at(browser, x, y);
    if (hit == NIL || !browser->entries[hit].selected) {
        for (int i = 0; i < browser->entry_count; ++i)
            browser->entries[i].selected = i == hit;
    }
    browser->selected_index = hit;
    browser->last_click_index = NIL;
    browser->last_click_ms = 0;
    desktop_browser_redraw(browser, NULL);
    desktop_update_desk_menu_labels();
    if (x + 236 > g_desktop.screen_width)
        x = g_desktop.screen_width - 236;
    if (y + count * row_height > g_desktop.screen_height)
        y = g_desktop.screen_height - count * row_height;
    if (y < g_desktop.work.g_y)
        y = g_desktop.work.g_y;
    desktop_init_object(&tree[0], NIL, 1, count, G_BOX, NONE, NORMAL,
                        0x00011170L, x, y, 236, count * row_height);
    for (int i = 1; i <= count; ++i)
        desktop_init_object(
            &tree[i], i == count ? 0 : (WORD)(i + 1), NIL, NIL, G_STRING,
            i == count ? LASTOB : NONE,
            desktop_file_command_enabled((WORD)(MENU_FILE_OPEN + i - 1))
                ? NORMAL
                : DISABLED,
            (LONG)(intptr_t)labels[i - 1], 0, (WORD)((i - 1) * row_height), 236,
            row_height);
    window = dialog_open(tree);
    if (!window)
        return;
    for (;;) {
        WORD mx, my, buttons, modifiers, key, clicks, msg[8];
        objc_draw(tree, 0, MAX_DEPTH, x, y, tree[0].ob_width,
                  tree[0].ob_height);
        WORD event = evnt_multi(MU_BUTTON | MU_KEYBD | MU_TIMER | MU_MESAG, 1,
                                0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, msg, 30, 0,
                                &mx, &my, &buttons, &modifiers, &key, &clicks);
        if (!event || ((event & MU_KEYBD) && (key & 255) == 27))
            break;
        if ((event & MU_MESAG) && msg[0] == AP_TERM)
            break;
        hit = objc_find(tree, 0, MAX_DEPTH, mx, my);
        if (hit <= 0 || tree[hit].ob_state & DISABLED)
            hit = NIL;
        if (hovered > 0)
            tree[hovered].ob_state &= (UWORD)~SELECTED;
        hovered = hit;
        if (hovered > 0)
            tree[hovered].ob_state |= SELECTED;
        if ((event & MU_BUTTON) && (buttons & 1)) {
            chosen = hit;
            break;
        }
        if ((event & MU_BUTTON) && (buttons & 2))
            break;
    }
    dialog_close(tree, window);
    if (chosen > 0)
        desktop_file_command((WORD)(MENU_FILE_OPEN + chosen - 1));
}
