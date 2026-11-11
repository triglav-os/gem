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

/* Context rows in display order; a zero command is a group separator. */
static const struct {
    const char *label;
    WORD command;
} file_popup_rows[] = {
    {"  Open", MENU_FILE_OPEN},
    {"  Delete", MENU_FILE_DELETE},
    {"  Restore", MENU_FILE_RESTORE},
    {"  Empty Trash", MENU_FILE_EMPTY_TRASH},
    {NULL, 0},
    {"  Rename       F2", MENU_FILE_RENAME},
    {"  Cut          ^X", MENU_FILE_CUT},
    {"  Copy         ^C", MENU_FILE_COPY},
    {"  Paste        ^V", MENU_FILE_PASTE},
    {NULL, 0},
    {"  New Folder", MENU_FILE_NEW_FOLDER},
    {"  New Text File", MENU_FILE_NEW_TEXT},
    {"  New Markdown File", MENU_FILE_NEW_MARKDOWN},
    {"  New HTML File", MENU_FILE_NEW_HTML},
    {"  New CSV File", MENU_FILE_NEW_CSV},
    {NULL, 0},
    {"  Select All   ^A", MENU_FILE_SELECT_ALL},
    {"  Add to Desktop", MENU_FILE_ADD_DESKTOP},
};

void desktop_file_popup(WORD handle, WORD x, WORD y)
{
    enum {
        count = sizeof(file_popup_rows) / sizeof(file_popup_rows[0]),
        row_height = 20,
        separator_height = 8,
        width = 236
    };
    OBJECT tree[count + 1];
    desktop_browser_window_t *browser = desktop_find_browser_by_handle(handle);
    WORD hit, chosen = NIL, hovered = NIL, window;
    WORD height = 4;

    if (browser == NULL)
        return;
    for (int i = 0; i < count; ++i)
        height = (WORD)(height + (file_popup_rows[i].command != 0
                                      ? row_height
                                      : separator_height));
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
    if (x + width > g_desktop.screen_width)
        x = g_desktop.screen_width - width;
    if (y + height > g_desktop.screen_height)
        y = g_desktop.screen_height - height;
    if (y < g_desktop.work.g_y)
        y = g_desktop.work.g_y;
    desktop_init_object(&tree[0], NIL, 1, count, G_BOX, NONE, NORMAL,
                        0x00011170L, x, y, width, height);
    {
        WORD top = 2;

        for (int i = 1; i <= count; ++i) {
            WORD command = file_popup_rows[i - 1].command;
            WORD rows = command != 0 ? row_height : separator_height;

            /* Separators are inert, invisible boxes; the rule is drawn
             * below so it spans the popup without a text glyph row. */
            desktop_init_object(
                &tree[i], i == count ? 0 : (WORD)(i + 1), NIL, NIL,
                command != 0 ? G_STRING : G_IBOX, i == count ? LASTOB : NONE,
                command != 0 && desktop_file_command_enabled(command)
                    ? NORMAL
                    : DISABLED,
                command != 0 ? (LONG)(intptr_t)file_popup_rows[i - 1].label
                             : 0L,
                2, top, width - 4, rows);
            top = (WORD)(top + rows);
        }
    }
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
                            (WORD)(x + width - 1),
                            y,
                            (WORD)(x + width - 1),
                            (WORD)(y + height - 1),
                            x,
                            (WORD)(y + height - 1),
                            x,
                            y};
        WORD clip[4] = {x, y, (WORD)(x + width - 1), (WORD)(y + height - 1)};
        vs_clip(g_desktop.vdi_handle, 1, clip);
        vswr_mode(g_desktop.vdi_handle, MD_REPLACE);
        vsl_color(g_desktop.vdi_handle, WHITE);
        vsl_type(g_desktop.vdi_handle, 1);
        vsl_width(g_desktop.vdi_handle, 1);
        v_pline(g_desktop.vdi_handle, 5, outline);
        for (int i = 1; i <= count; ++i) {
            if (file_popup_rows[i - 1].command != 0)
                continue;
            WORD rule_y = (WORD)(y + tree[i].ob_y + separator_height / 2);
            WORD rule[4] = {(WORD)(x + 4), rule_y, (WORD)(x + width - 5),
                            rule_y};
            v_pline(g_desktop.vdi_handle, 2, rule);
        }
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
        desktop_file_command(file_popup_rows[chosen - 1].command);
}
