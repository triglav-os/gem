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
    return handle;
}

static void dialog_close(OBJECT *tree, WORD handle)
{
    (void)tree;
    wind_close(handle);
    wind_delete(handle);
}

int desktop_name_dialog(const char *title, char *text, size_t capacity)
{
    return gemctl_prompt(g_desktop.active_browser_handle, g_desktop.vdi_handle,
                         title, "Name", text, capacity);
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
    if (y + count * row_height + 4 > g_desktop.screen_height)
        y = g_desktop.screen_height - count * row_height - 4;
    if (y < g_desktop.work.g_y)
        y = g_desktop.work.g_y;
    desktop_init_object(&tree[0], NIL, 1, count, G_BOX, NONE, NORMAL,
                        0x00011170L, x, y, 236, count * row_height + 4);
    for (int i = 1; i <= count; ++i)
        desktop_init_object(
            &tree[i], i == count ? 0 : (WORD)(i + 1), NIL, NIL, G_STRING,
            i == count ? LASTOB : NONE,
            desktop_file_command_enabled((WORD)(MENU_FILE_OPEN + i - 1))
                ? NORMAL
                : DISABLED,
            (LONG)(intptr_t)labels[i - 1], 2, (WORD)(2 + (i - 1) * row_height),
            232, row_height);
    window = dialog_open(tree);
    if (!window)
        return;
    for (;;) {
        WORD mx, my, buttons, modifiers, key, clicks, msg[8];
        wind_update(BEG_UPDATE);
        objc_draw(tree, 0, MAX_DEPTH, x, y, tree[0].ob_width,
                  tree[0].ob_height);
        WORD outline[10] = {x,
                            y,
                            (WORD)(x + 235),
                            y,
                            (WORD)(x + 235),
                            (WORD)(y + count * row_height + 3),
                            x,
                            (WORD)(y + count * row_height + 3),
                            x,
                            y};
        WORD clip[4] = {x, y, (WORD)(x + 235),
                        (WORD)(y + count * row_height + 3)};
        vs_clip(g_desktop.vdi_handle, 1, clip);
        vswr_mode(g_desktop.vdi_handle, MD_REPLACE);
        vsl_color(g_desktop.vdi_handle, WHITE);
        vsl_type(g_desktop.vdi_handle, 1);
        vsl_width(g_desktop.vdi_handle, 1);
        v_pline(g_desktop.vdi_handle, 5, outline);
        vs_clip(g_desktop.vdi_handle, 0, clip);
        wind_update(END_UPDATE);
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
