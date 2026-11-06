/*
 * Implements a modeless, tabbed native GEM browser settings dialog. Browser
 * pumping continues in the application event loop while this window is open.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (C) 2026 tomaz stih
 */
#include "browser_settings.h"
#include <stdio.h>
#include <string.h>

enum { settings_count = 19, settings_width = 520, settings_height = 250 };

int gemscape_script_dialog(int kind, const char *origin, const char *message,
                           char *text, size_t capacity)
{
    char title[64], lines[2][64], prompt[256];
    OBJECT tree[7] = {{0}};
    TEDINFO edit = {0};
    WORD x, y, width, height, result, outer_x, outer_y, outer_w, outer_h,
        window;
    snprintf(title, sizeof(title), "%.62s", origin);
    snprintf(lines[0], sizeof(lines[0]), "%.62s", message);
    snprintf(lines[1], sizeof(lines[1]), "%.62s",
             strlen(message) > 62 ? message + 62 : "");
    snprintf(prompt, sizeof(prompt), "%s", text != NULL ? text : "");
    edit.te_ptext = (intptr_t)prompt;
    edit.te_ptmplt = (intptr_t) "";
    edit.te_pvalid = (intptr_t) "";
    edit.te_font = 3;
    edit.te_color = 0x1180;
    edit.te_thickness = -1;
    edit.te_txtlen = sizeof(prompt);
    edit.te_tmplen = 1;
    for (int i = 0; i < 7; ++i) {
        tree[i].ob_head = tree[i].ob_tail = NIL;
        tree[i].ob_next = (WORD)(i + 1);
        tree[i].ob_type = G_STRING;
        tree[i].ob_x = 12;
        tree[i].ob_y = (WORD)(10 + (i - 1) * 24);
        tree[i].ob_width = 496;
        tree[i].ob_height = 20;
    }
    tree[0].ob_next = NIL;
    tree[0].ob_head = 1;
    tree[0].ob_tail = 6;
    tree[0].ob_type = G_BOX;
    tree[0].ob_spec = 0x00021170L;
    tree[0].ob_width = 520;
    tree[0].ob_height = 158;
    tree[1].ob_spec = (LONG)(intptr_t)title;
    tree[2].ob_spec = (LONG)(intptr_t)lines[0];
    tree[3].ob_spec = (LONG)(intptr_t)lines[1];
    tree[4].ob_type = G_BOXTEXT;
    tree[4].ob_spec = (LONG)(intptr_t)&edit;
    tree[4].ob_flags = kind == 2 ? EDITABLE : HIDETREE;
    for (int i = 5; i <= 6; ++i) {
        tree[i].ob_type = G_BUTTON;
        tree[i].ob_y = 122;
        tree[i].ob_x = (WORD)(270 + (i - 5) * 120);
        tree[i].ob_width = 110;
        tree[i].ob_height = 24;
        tree[i].ob_flags = SELECTABLE | EXIT;
    }
    tree[5].ob_spec = (LONG)(intptr_t) "OK";
    tree[5].ob_flags |= DEFAULT;
    tree[6].ob_spec = (LONG)(intptr_t) "Cancel";
    tree[6].ob_flags |= LASTOB | (kind == 0 ? HIDETREE : 0);
    tree[6].ob_next = 0;
    form_center(tree, &x, &y, &width, &height);
    tree[0].ob_x = x;
    tree[0].ob_y = y;
    /* A real GEM window also occludes the browser's BGRA presenter layer.
     * A bare form_dial rectangle clips VDI but does not cover that layer. */
    wind_calc(WC_BORDER, 0, x, y, width, height, &outer_x, &outer_y, &outer_w,
              &outer_h);
    window = wind_create(0, outer_x, outer_y, outer_w, outer_h);
    if (window <= 0)
        return 0;
    wind_open(window, outer_x, outer_y, outer_w, outer_h);
    form_dial(FMD_START, 0, 0, 0, 0, x, y, width, height);
    objc_draw(tree, 0, MAX_DEPTH, x, y, width, height);
    {
        gemctl_text_edit_t editor;
        WORD cw, ch, bw, bh, vdi = graf_handle(&cw, &ch, &bw, &bh);
        GRECT clip = {x, y, width, height};
        GRECT field = {(WORD)(x + tree[4].ob_x), (WORD)(y + tree[4].ob_y),
                       tree[4].ob_width, tree[4].ob_height};
        gemctl_text_edit_init(&editor, prompt, sizeof(prompt));
        if (kind == 2)
            gemctl_text_edit_focus(&editor, 1);
        result = 6;
        for (;;) {
            WORD mx, my, buttons, modifiers, key, clicks;
            if (kind == 2)
                gemctl_text_edit_draw(&editor, vdi, &field, &clip);
            WORD flags = evnt_multi(MU_BUTTON | MU_KEYBD, 1, 1, 1, 0, 0, 0, 0,
                                    0, 0, 0, 0, 0, 0, NULL, 0, 0, &mx, &my,
                                    &buttons, &modifiers, &key, &clicks);
            if (flags == 0)
                break;
            if (flags & MU_KEYBD) {
                if ((key & 255) == 27)
                    break;
                if ((key & 255) == 13 || (key & 255) == 10 ||
                    (key >> 8) == 40) {
                    result = 5;
                    break;
                }
                if (kind == 2)
                    gemctl_text_edit_key_mod(&editor, (uint16_t)key,
                                             (uint16_t)modifiers);
            }
            if (flags & MU_BUTTON) {
                WORD hit = objc_find(tree, 0, MAX_DEPTH, mx, my);
                if (hit == 5 || (hit == 6 && kind != 0)) {
                    result = hit;
                    break;
                }
                if (hit == 4 && kind == 2)
                    gemctl_text_edit_click(&editor, vdi, &field, mx, 0);
            }
        }
        WORD no_clip[4] = {0};
        vs_clip(vdi, 0, no_clip);
    }
    form_dial(FMD_FINISH, 0, 0, 0, 0, x, y, width, height);
    wind_close(window);
    wind_delete(window);
    if (result == 5 && text != NULL && capacity > 0)
        snprintf(text, capacity, "%s", prompt);
    return result == 5;
}

static void settings_tree(gemscape_settings_dialog_t *dialog, OBJECT *tree,
                          char labels[settings_count][128])
{
    GRECT work;
    wind_get(dialog->handle, WF_CXYWH, &work.g_x, &work.g_y, &work.g_w,
             &work.g_h);
    memset(tree, 0, settings_count * sizeof(*tree));
    for (int i = 0; i < settings_count; ++i) {
        tree[i].ob_head = tree[i].ob_tail = NIL;
        tree[i].ob_next = (WORD)(i + 1);
        tree[i].ob_type = G_BUTTON;
        tree[i].ob_flags = HIDETREE;
        tree[i].ob_spec = (LONG)(intptr_t)labels[i];
        tree[i].ob_width = 150;
        tree[i].ob_height = 24;
    }
    tree[0].ob_next = NIL;
    tree[0].ob_head = 1;
    tree[0].ob_tail = settings_count - 1;
    tree[0].ob_type = G_BOX;
    tree[0].ob_spec = 0x00001170L;
    tree[0].ob_flags = NONE;
    tree[0].ob_x = work.g_x;
    tree[0].ob_y = work.g_y;
    tree[0].ob_width = work.g_w;
    tree[0].ob_height = work.g_h;
    tree[settings_count - 1].ob_next = 0;
    const char *tabs[] = {"General", "Privacy", "Advanced"};
    for (int i = 1; i <= 3; ++i) {
        snprintf(labels[i], 128, "%s", tabs[i - 1]);
        tree[i].ob_x = (WORD)(10 + (i - 1) * 166);
        tree[i].ob_y = 10;
        tree[i].ob_width = 166;
        tree[i].ob_flags = SELECTABLE;
        tree[i].ob_state = dialog->tab == i - 1 ? SELECTED : NORMAL;
    }
    if (dialog->tab == 0) {
        snprintf(labels[4], 128, "Home page");
        tree[4].ob_type = G_STRING;
        tree[4].ob_x = 14;
        tree[4].ob_y = 45;
        tree[4].ob_flags = NONE;
        tree[5].ob_type = G_BOX;
        tree[5].ob_spec = 0x00011170L;
        tree[5].ob_x = 14;
        tree[5].ob_y = 65;
        tree[5].ob_width = 490;
        tree[5].ob_flags = NONE;
    }
    if (dialog->tab != 1) {
        snprintf(labels[6], 128, "%s",
                 dialog->tab == 0 ? "Page zoom" : "Frame rate");
        tree[6].ob_type = G_STRING;
        tree[6].ob_x = 14;
        tree[6].ob_y = 114;
        tree[6].ob_flags = NONE;
        snprintf(labels[7], 128, "-");
        snprintf(labels[9], 128, "+");
        snprintf(labels[8], 128, dialog->tab == 0 ? "%d%%" : "%d fps",
                 dialog->tab == 0 ? dialog->draft.zoom
                                  : dialog->draft.frame_rate);
        for (int i = 7; i <= 9; ++i) {
            tree[i].ob_x = (WORD)(190 + (i - 7) * 70);
            tree[i].ob_y = 105;
            tree[i].ob_width = 65;
            tree[i].ob_flags = SELECTABLE;
        }
        tree[8].ob_type = G_STRING;
        tree[8].ob_y = 114;
    }
    if (dialog->tab != 0) {
        int values[3] = {dialog->draft.persistent, dialog->draft.cookies,
                         dialog->draft.session_cookies};
        const char *names[3] = {"Keep profile, cookies and cache on disk",
                                "Allow cookies",
                                "Restore session cookies after restart"};
        if (dialog->tab == 2) {
            values[0] = dialog->draft.javascript;
            values[1] = dialog->draft.images;
            names[0] = "Enable JavaScript";
            names[1] = "Load images";
        }
        for (int i = 0; i < (dialog->tab == 1 ? 3 : 2); ++i) {
            int index = 10 + i;
            snprintf(labels[index], 128, "[%c] %s", values[i] ? 'x' : ' ',
                     names[i]);
            tree[index].ob_x = 14;
            tree[index].ob_y = (WORD)(45 + i * 30);
            tree[index].ob_width = 490;
            tree[index].ob_flags = SELECTABLE;
        }
    }
    if (dialog->tab == 1) {
        const char *actions[] = {"Clear cookies", "Clear cache",
                                 "Clear this site"};
        for (int i = 13; i <= 15; ++i) {
            snprintf(labels[i], 128, "%s", actions[i - 13]);
            tree[i].ob_x = (WORD)(14 + (i - 13) * 164);
            tree[i].ob_y = 145;
            tree[i].ob_width = 160;
            tree[i].ob_flags = SELECTABLE;
        }
    }
    snprintf(
        labels[16], 128, "%.60s",
        dialog->message[0]
            ? dialog->message
            : (dialog->tab == 1
                   ? "Restart required. Memory mode keeps old disk data."
                   : "Zoom applies on Save; other changes on next launch."));
    tree[16].ob_type = G_STRING;
    tree[16].ob_x = 14;
    tree[16].ob_y = 185;
    tree[16].ob_flags = NONE;
    snprintf(labels[17], 128, "Save");
    snprintf(labels[18], 128, "Cancel");
    tree[17].ob_x = 270;
    tree[18].ob_x = 392;
    tree[17].ob_y = tree[18].ob_y = 212;
    tree[17].ob_width = tree[18].ob_width = 110;
    tree[17].ob_flags = SELECTABLE | DEFAULT;
    tree[18].ob_flags = SELECTABLE | LASTOB;
}

void gemscape_settings_open(gemscape_settings_dialog_t *dialog, WORD vdi,
                            const gemscape_preferences_t *preferences)
{
    GRECT desktop;
    WORD x, y, width, height;
    if (dialog->handle > 0) {
        wind_set(dialog->handle, WF_TOP, 0, 0, 0, 0);
        return;
    }
    memset(dialog, 0, sizeof(*dialog));
    dialog->vdi = vdi;
    dialog->draft = *preferences;
    gemctl_text_edit_init(&dialog->home_edit, dialog->draft.homepage,
                          sizeof(dialog->draft.homepage));
    wind_get(0, WF_CXYWH, &desktop.g_x, &desktop.g_y, &desktop.g_w,
             &desktop.g_h);
    wind_calc(WC_BORDER, NAME | CLOSER | MOVER, 0, 0, settings_width,
              settings_height, &x, &y, &width, &height);
    x = (WORD)(desktop.g_x + (desktop.g_w - width) / 2);
    y = (WORD)(desktop.g_y + (desktop.g_h - height) / 2);
    dialog->handle = wind_create(NAME | CLOSER | MOVER, x, y, width, height);
    if (dialog->handle <= 0)
        return;
    wind_set_str(dialog->handle, WF_NAME, "Gemscape Settings");
    wind_open(dialog->handle, x, y, width, height);
    gemscape_settings_draw(dialog);
}

void gemscape_settings_close(gemscape_settings_dialog_t *dialog)
{
    if (dialog->handle > 0) {
        wind_close(dialog->handle);
        wind_delete(dialog->handle);
    }
    dialog->handle = 0;
}

void gemscape_settings_draw(gemscape_settings_dialog_t *dialog)
{
    OBJECT tree[settings_count];
    char labels[settings_count][128] = {{0}};
    GRECT clip;
    if (dialog->handle <= 0)
        return;
    settings_tree(dialog, tree, labels);
    wind_update(BEG_UPDATE);
    wind_get(dialog->handle, WF_FIRSTXYWH, &clip.g_x, &clip.g_y, &clip.g_w,
             &clip.g_h);
    while (clip.g_w > 0 && clip.g_h > 0) {
        objc_draw(tree, 0, MAX_DEPTH, clip.g_x, clip.g_y, clip.g_w, clip.g_h);
        if (dialog->tab == 0) {
            GRECT field = {(WORD)(tree[0].ob_x + tree[5].ob_x + 1),
                           (WORD)(tree[0].ob_y + tree[5].ob_y + 1),
                           (WORD)(tree[5].ob_width - 2),
                           (WORD)(tree[5].ob_height - 2)};
            gemctl_text_edit_draw(&dialog->home_edit, dialog->vdi, &field,
                                  &clip);
            WORD no_clip[4] = {0};
            vs_clip(dialog->vdi, 0, no_clip);
        }
        wind_get(dialog->handle, WF_NEXTXYWH, &clip.g_x, &clip.g_y, &clip.g_w,
                 &clip.g_h);
    }
    wind_update(END_UPDATE);
}

void gemscape_settings_message(gemscape_settings_dialog_t *dialog,
                               const WORD *message)
{
    if (message[0] == WM_CLOSED)
        gemscape_settings_close(dialog);
    else if (message[0] == WM_TOPPED)
        wind_set(dialog->handle, WF_TOP, 0, 0, 0, 0);
    else if (message[0] == WM_MOVED) {
        wind_set(dialog->handle, WF_WXYWH, message[4], message[5], message[6],
                 message[7]);
        gemscape_settings_draw(dialog);
    } else if (message[0] == WM_REDRAW)
        gemscape_settings_draw(dialog);
}

static int settings_save(gemscape_settings_dialog_t *dialog,
                         gemscape_preferences_t *preferences)
{
    if (!dialog->draft.homepage[0] ||
        !gemscape_preferences_save(&dialog->draft)) {
        snprintf(dialog->message, sizeof(dialog->message),
                 "Could not save settings. Check home page and storage.");
        gemscape_settings_draw(dialog);
        return gemscape_settings_none;
    }
    *preferences = dialog->draft;
    gemscape_settings_close(dialog);
    return gemscape_settings_saved;
}

int gemscape_settings_click(gemscape_settings_dialog_t *dialog, WORD x, WORD y,
                            gemscape_preferences_t *preferences)
{
    OBJECT tree[settings_count];
    char labels[settings_count][128] = {{0}};
    settings_tree(dialog, tree, labels);
    WORD hit = objc_find(tree, 0, MAX_DEPTH, x, y);
    gemctl_text_edit_blur(&dialog->home_edit);
    if (hit >= 1 && hit <= 3)
        dialog->tab = hit - 1;
    else if (hit == 5 && dialog->tab == 0)
        gemctl_text_edit_focus(&dialog->home_edit, 1);
    else if (hit == 7 || hit == 9) {
        if (dialog->tab == 0) {
            dialog->draft.zoom += hit == 7 ? -10 : 10;
            if (dialog->draft.zoom < 50)
                dialog->draft.zoom = 50;
            if (dialog->draft.zoom > 200)
                dialog->draft.zoom = 200;
        } else if (dialog->tab == 2) {
            dialog->draft.frame_rate =
                hit == 7 ? (dialog->draft.frame_rate == 60 ? 30 : 15)
                         : (dialog->draft.frame_rate == 15 ? 30 : 60);
        }
    } else if (dialog->tab == 1) {
        if (hit == 10)
            dialog->draft.persistent = !dialog->draft.persistent;
        if (hit == 11)
            dialog->draft.cookies = !dialog->draft.cookies;
        if (hit == 12)
            dialog->draft.session_cookies = !dialog->draft.session_cookies;
        if (hit >= 13 && hit <= 15)
            return gemscape_settings_cookies + hit - 13;
    } else if (dialog->tab == 2) {
        if (hit == 10)
            dialog->draft.javascript = !dialog->draft.javascript;
        if (hit == 11)
            dialog->draft.images = !dialog->draft.images;
    }
    if (hit == 17)
        return settings_save(dialog, preferences);
    if (hit == 18)
        gemscape_settings_close(dialog);
    gemscape_settings_draw(dialog);
    return gemscape_settings_none;
}

int gemscape_settings_key(gemscape_settings_dialog_t *dialog, WORD key,
                          WORD modifiers, gemscape_preferences_t *preferences)
{
    if ((key & 0xff) == 27)
        gemscape_settings_close(dialog);
    else if ((key & 0xff) == 13 || (key & 0xff) == 10 || (key >> 8) == 40)
        return settings_save(dialog, preferences);
    else if (gemctl_text_edit_active(&dialog->home_edit)) {
        gemctl_text_edit_key_mod(&dialog->home_edit, (uint16_t)key,
                                 (uint16_t)modifiers);
        gemscape_settings_draw(dialog);
    }
    return gemscape_settings_none;
}
