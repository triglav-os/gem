/*
 * Implements Gemread's Display/Files settings dialog and XDG persistence.
 * Draft changes are discarded on Cancel; Save applies immediately.
 *
 * SPDX-License-Identifier: AGPL-3.0-or-later
 * Copyright (C) 2026 tomaz stih
 */
#include "reader_settings.h"
#include <gem/gemconf.h>
#include <gem/gemctl.h>
#include <stdio.h>
#include <string.h>

void gemread_preferences_load(gemread_preferences_t *p)
{
    gemconf_t config;
    gemconf_init(&config);
    gemconf_load(&config, "gemread");
    long zoom = gemconf_get_long(&config, "display", "zoom", 100);
    p->zoom = zoom >= 50 && zoom <= 400 ? (int)zoom : 100;
    p->fit_width = gemconf_get_bool(&config, "display", "fit_width", 0);
    p->thumbnails = gemconf_get_bool(&config, "display", "thumbnails", 1);
    p->antialias = gemconf_get_bool(&config, "display", "antialias", 1);
    p->remember_folder =
        gemconf_get_bool(&config, "files", "remember_folder", 1);
    snprintf(p->folder, sizeof(p->folder), "%s",
             p->remember_folder ? gemconf_get(&config, "files", "folder", "")
                                : "");
    if (p->folder[0] != '/')
        p->folder[0] = '\0';
    gemconf_clear(&config);
}

int gemread_preferences_save(const gemread_preferences_t *p)
{
    gemconf_t config;
    char zoom[16];
    int result;
    gemconf_init(&config);
    gemconf_load(&config, "gemread");
    snprintf(zoom, sizeof(zoom), "%d", p->zoom);
    result = gemconf_set(&config, "display", "zoom", zoom) &&
             gemconf_set(&config, "display", "fit_width",
                         p->fit_width ? "true" : "false") &&
             gemconf_set(&config, "display", "thumbnails",
                         p->thumbnails ? "true" : "false") &&
             gemconf_set(&config, "display", "antialias",
                         p->antialias ? "true" : "false") &&
             gemconf_set(&config, "files", "remember_folder",
                         p->remember_folder ? "true" : "false") &&
             gemconf_set(&config, "files", "folder",
                         p->remember_folder ? p->folder : "") &&
             gemconf_save(&config);
    gemconf_clear(&config);
    return result;
}

static void settings_tree(OBJECT tree[12], char labels[12][96],
                          const gemread_preferences_t *p, int tab, WORD x,
                          WORD y)
{
    memset(tree, 0, 12 * sizeof(*tree));
    for (int i = 0; i < 12; ++i) {
        tree[i].ob_next = (WORD)(i + 1);
        tree[i].ob_head = tree[i].ob_tail = NIL;
        tree[i].ob_type = G_BUTTON;
        tree[i].ob_flags = SELECTABLE;
        tree[i].ob_x = 16;
        tree[i].ob_y = (WORD)(50 + (i - 3) * 32);
        tree[i].ob_width = 448;
        tree[i].ob_height = 26;
        tree[i].ob_spec = (LONG)(intptr_t)labels[i];
    }
    tree[0].ob_next = NIL;
    tree[0].ob_head = 1;
    tree[0].ob_tail = 11;
    tree[0].ob_type = G_BOX;
    tree[0].ob_flags = NONE;
    tree[0].ob_spec = 0x00021170L;
    tree[0].ob_x = x;
    tree[0].ob_y = y;
    tree[0].ob_width = 480;
    tree[0].ob_height = 256;
    snprintf(labels[1], 96, "Display");
    snprintf(labels[2], 96, "Files");
    for (int i = 1; i <= 2; ++i) {
        tree[i].ob_x = (WORD)(16 + (i - 1) * 224);
        tree[i].ob_y = 12;
        tree[i].ob_width = 224;
        tree[i].ob_state = tab == i - 1 ? SELECTED : NORMAL;
    }
    if (!tab) {
        snprintf(labels[3], 96, "Fit: %s",
                 p->fit_width ? "Page width" : "Whole page");
        snprintf(labels[4], 96, "[%c] Show page thumbnails",
                 p->thumbnails ? 'x' : ' ');
        snprintf(labels[5], 96, "[%c] Smooth text and graphics",
                 p->antialias ? 'x' : ' ');
        snprintf(labels[6], 96, "Default zoom: %d%%", p->zoom);
        tree[6].ob_type = G_STRING;
        tree[6].ob_flags = NONE;
        snprintf(labels[7], 96, "-");
        snprintf(labels[8], 96, "+");
        tree[7].ob_x = 300;
        tree[8].ob_x = 384;
        tree[7].ob_y = tree[8].ob_y = 146;
        tree[7].ob_width = tree[8].ob_width = 80;
    } else {
        snprintf(labels[3], 96, "[%c] Remember last opened folder",
                 p->remember_folder ? 'x' : ' ');
        snprintf(labels[4], 96, "Forget saved folder");
        snprintf(labels[5], 96, "PDF, EPUB, XPS, CBZ and supported images");
        tree[5].ob_type = G_STRING;
        tree[5].ob_flags = NONE;
        for (int i = 6; i <= 8; ++i)
            tree[i].ob_flags = HIDETREE;
    }
    snprintf(labels[9], 96, "Settings apply now and to future documents.");
    tree[9].ob_type = G_STRING;
    tree[9].ob_flags = NONE;
    tree[9].ob_y = 186;
    snprintf(labels[10], 96, "Save");
    snprintf(labels[11], 96, "Cancel");
    tree[10].ob_x = 234;
    tree[11].ob_x = 356;
    tree[10].ob_y = tree[11].ob_y = 218;
    tree[10].ob_width = tree[11].ob_width = 110;
    tree[10].ob_flags |= DEFAULT;
    tree[11].ob_flags |= LASTOB;
    tree[11].ob_next = 0;
}

int gemread_settings_dialog(gemread_preferences_t *preferences, WORD parent,
                            WORD vdi)
{
    gemread_preferences_t draft = *preferences;
    OBJECT tree[12];
    char labels[12][96] = {{0}};
    GRECT work;
    WORD handle;
    int tab = 0, saved = 0;
    settings_tree(tree, labels, &draft, tab, 0, 0);
    handle = gemctl_dialog_open(parent, "Gemread Settings", 480, 256, &work);
    if (handle <= 0)
        return 0;
    for (;;) {
        WORD mx, my, buttons, modifiers, key, clicks, msg[8], hit = NIL;
        settings_tree(tree, labels, &draft, tab, work.g_x, work.g_y);
        gemctl_dialog_draw(handle, vdi, tree, NULL, 0);
        WORD event = evnt_multi(MU_BUTTON | MU_KEYBD | MU_MESAG, 1, 1, 1, 0, 0,
                                0, 0, 0, 0, 0, 0, 0, 0, msg, 0, 0, &mx, &my,
                                &buttons, &modifiers, &key, &clicks);
        if (!event || ((event & MU_MESAG) &&
                       gemctl_dialog_message(handle, msg, &work) < 0))
            break;
        if (event & MU_KEYBD) {
            if ((key & 255) == 27)
                break;
            if ((key & 255) == 13 || (key & 255) == 10)
                hit = 10;
            if ((key & 255) == 9)
                tab = !tab;
        }
        if (event & MU_BUTTON)
            hit = objc_find(tree, 0, MAX_DEPTH, mx, my);
        if (hit == 11)
            break;
        if (hit == 10) {
            if (gemread_preferences_save(&draft)) {
                *preferences = draft;
                saved = 1;
                break;
            }
            form_alert(1, "[1][ Could not save Gemread settings. ][ OK ]");
        } else if (hit == 1 || hit == 2)
            tab = hit - 1;
        else if (!tab) {
            if (hit == 3)
                draft.fit_width = !draft.fit_width;
            if (hit == 4)
                draft.thumbnails = !draft.thumbnails;
            if (hit == 5)
                draft.antialias = !draft.antialias;
            if (hit == 7 && draft.zoom > 50)
                draft.zoom -= 25;
            if (hit == 8 && draft.zoom < 400)
                draft.zoom += 25;
            if (draft.zoom < 50)
                draft.zoom = 50;
            if (draft.zoom > 400)
                draft.zoom = 400;
        } else {
            if (hit == 3)
                draft.remember_folder = !draft.remember_folder;
            if (hit == 4)
                draft.folder[0] = '\0';
        }
    }
    wind_close(handle);
    wind_delete(handle);
    return saved;
}
