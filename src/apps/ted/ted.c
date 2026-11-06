/*
 * Ted: a small GEM plain-text editor with atomic saves, selection and undo.
 * No rendering engine or external editor process is required.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (C) 2026 tomaz stih
 */
#define _POSIX_C_SOURCE 200809L
#include "ted.h"
#include <gem.h>
#include <gem/input.h>
#include <gem/gemctl.h>
#include <platform/os.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int ted_save(ted_state_t *s, int choose);

static int ted_discard(ted_state_t *s)
{
    if (!s->doc.dirty)
        return 1;
    WORD choice = form_alert(
        3, "[2][ Save changes before continuing? ][ Save | Discard | Cancel ]");
    return choice == 2 || (choice == 1 && ted_save(s, 0));
}
static int ted_choose(ted_state_t *s, int save, char *out)
{
    char directory[GEM_OS_PATH_MAX], file[256] = "";
    WORD accepted = 0;
    if (s->path[0]) {
        snprintf(directory, sizeof(directory), "%s", s->path);
        char *slash = strrchr(directory, '/');
        if (slash) {
            snprintf(file, sizeof(file), "%s", slash + 1);
            slash[1] = 0;
        } else {
            directory[0] = 0;
        }
    } else
        directory[0] = 0;
    if (!directory[0] && !gem_os_getcwd(directory, sizeof(directory)))
        strcpy(directory, "/");
    size_t n = strlen(directory);
    if (n + 5 >= sizeof(directory))
        return 0;
    strcat(directory, n && directory[n - 1] == '/' ? "*.*" : "/*.*");
    if (save)
        fsel_save(directory, file, &accepted);
    else
        fsel_exinput(directory, file, &accepted, "Ted - Open Text");
    if (!accepted || !file[0])
        return 0;
    char *slash = strrchr(directory, '/');
    if (slash)
        slash[1] = 0;
    else
        directory[0] = 0;
    return snprintf(out, GEM_OS_PATH_MAX, "%s%s", directory, file) <
           GEM_OS_PATH_MAX;
}
static int ted_load(ted_state_t *s, const char *path)
{
    FILE *f = fopen(path, "rb");
    if (!f) {
        form_alert(1, "[1][ Cannot open this text file. ][ OK ]");
        return 0;
    }
    char *text = malloc(ted_limit + 1);
    if (!text) {
        fclose(f);
        return 0;
    }
    size_t size = fread(text, 1, ted_limit + 1, f);
    int bad = ferror(f) || size > ted_limit || memchr(text, 0, size) != NULL;
    fclose(f);
    if (bad) {
        free(text);
        form_alert(1, "[1][ Text files must be at most 1 MiB|and contain no "
                      "NUL bytes. ][ OK ]");
        return 0;
    }
    text[size] = 0;
    ted_document_free(&s->doc);
    const char *newline = strchr(text, '\n');
    s->crlf = newline && newline > text && newline[-1] == '\r';
    s->doc.text = text;
    s->doc.length = size;
    snprintf(s->path, sizeof(s->path), "%s", path);
    s->top = s->left = 0;
    return 1;
}
static int ted_save(ted_state_t *s, int choose)
{
    char path[GEM_OS_PATH_MAX];
    snprintf(path, sizeof(path), "%s", s->path);
    if ((choose || !path[0]) && !ted_choose(s, 1, path))
        return 0;
    if (!gem_os_write_file_atomic(path, s->doc.text, s->doc.length)) {
        form_alert(1, "[1][ Could not save the file.|Your edits are still "
                      "available. ][ OK ]");
        return 0;
    }
    snprintf(s->path, sizeof(s->path), "%s", path);
    s->doc.dirty = 0;
    s->doc.undo_dirty = 1;
    return 1;
}
static void ted_action(ted_state_t *s, WORD action)
{
    size_t first =
        s->doc.cursor < s->doc.anchor ? s->doc.cursor : s->doc.anchor;
    size_t last = s->doc.cursor > s->doc.anchor ? s->doc.cursor : s->doc.anchor;
    char path[GEM_OS_PATH_MAX];
    switch (action) {
        case act_new:
            if (ted_discard(s)) {
                ted_document_free(&s->doc);
                if (!ted_document_init(&s->doc))
                    s->running = 0;
                s->path[0] = 0;
                s->top = s->left = 0;
            }
            break;
        case act_open:
            if (ted_discard(s) && ted_choose(s, 0, path))
                ted_load(s, path);
            break;
        case act_save:
            ted_save(s, 0);
            break;
        case act_save_as:
            ted_save(s, 1);
            break;
        case act_quit:
            if (ted_discard(s))
                s->running = 0;
            break;
        case act_all:
            s->doc.anchor = 0;
            s->doc.cursor = s->doc.length;
            break;
        case act_undo:
            ted_document_undo(&s->doc);
            break;
        case act_copy:
        case act_cut:
            if (first < last) {
                char *copy = malloc(last - first + 1);
                if (copy) {
                    memcpy(copy, s->doc.text + first, last - first);
                    copy[last - first] = 0;
                    free(s->clipboard);
                    s->clipboard = copy;
                    if (action == act_cut)
                        ted_document_insert(&s->doc, "", 0);
                }
            }
            break;
        case act_paste:
            if (s->clipboard && !ted_document_insert(&s->doc, s->clipboard,
                                                     strlen(s->clipboard)))
                form_alert(1, "[1][ Text exceeds the 1 MiB limit. ][ OK ]");
            break;
        default:
            break;
    }
}
static void ted_menu(ted_state_t *s)
{
    static const char *files[] = {" New ^N", " Open... ^O", " Save ^S",
                                  " Save As...", " Quit ^Q"};
    static const char *edits[] = {" Undo / Redo ^Z", " Cut ^X", " Copy ^C",
                                  " Paste ^V", " Select All ^A"};
    OBJECT initial[menu_count] = {
        {NIL, 1, 5, G_IBOX, NONE, NORMAL, 0, 0, 0, 0, 0},
        {5, 2, 2, G_BOX, NONE, NORMAL, 0x1100, 0, 0, 80, 1},
        {1, 3, 4, G_IBOX, NONE, NORMAL, 0, 0, 0, 80, 1},
        {4, NIL, NIL, G_TITLE, NONE, NORMAL, 0, 0, 0, 6, 1},
        {2, NIL, NIL, G_TITLE, NONE, NORMAL, 0, 6, 0, 6, 1},
        {0, 6, 12, G_IBOX, NONE, NORMAL, 0, 0, 0, 0, 0},
        {12, 7, 11, G_BOX, NONE, NORMAL, 0x1100, 0, 0, 22, 5}};
    memcpy(s->menu, initial, sizeof(initial));
    s->menu[menu_file].ob_spec = (LONG)(intptr_t) " File ";
    s->menu[menu_edit].ob_spec = (LONG)(intptr_t) " Edit ";
    s->menu[menu_edit_box] =
        (OBJECT){5, 13, 17, G_BOX, NONE, NORMAL, 0x1100, 6, 0, 22, 5};
    for (int i = act_new; i <= act_quit; ++i)
        s->menu[i] = (OBJECT){i == act_quit ? menu_file_box : i + 1,
                              NIL,
                              NIL,
                              G_STRING,
                              NONE,
                              NORMAL,
                              (LONG)(intptr_t)files[i - act_new],
                              0,
                              i - act_new,
                              22,
                              1};
    for (int i = act_undo; i <= act_all; ++i)
        s->menu[i] = (OBJECT){i == act_all ? menu_edit_box : i + 1,
                              NIL,
                              NIL,
                              G_STRING,
                              i == act_all ? LASTOB : NONE,
                              NORMAL,
                              (LONG)(intptr_t)edits[i - act_undo],
                              0,
                              i - act_undo,
                              22,
                              1};
    menu_click(1, 1);
    menu_bar(s->menu, 1);
}
static void ted_key(ted_state_t *s, WORD key, WORD mods)
{
    unsigned scan = ((unsigned)key >> 8) & 255, ascii = key & 255;
    if (mods & K_CTRL) {
        if (scan == 74 || scan == 77) {
            s->doc.cursor = scan == 74 ? 0 : s->doc.length;
            if (!(mods & (K_LSHIFT | K_RSHIFT)))
                s->doc.anchor = s->doc.cursor;
            return;
        }
        WORD action = scan == 17   ? act_new
                      : scan == 18 ? act_open
                      : scan == 22
                          ? ((mods & K_LSHIFT) ? act_save_as : act_save)
                      : scan == 20 ? act_quit
                      : scan == 4  ? act_all
                      : scan == 6  ? act_copy
                      : scan == 27 ? act_cut
                      : scan == 25 ? act_paste
                      : scan == 29 ? act_undo
                                   : 0;
        if (action)
            ted_action(s, action);
        return;
    }
    size_t pos = s->doc.cursor, line, col;
    ted_document_position(&s->doc, pos, &line, &col);
    col = ted_column(s, ted_document_line(&s->doc, line), pos);
    if (scan == 80)
        pos = ted_document_step(&s->doc, pos, -1);
    else if (scan == 79)
        pos = ted_document_step(&s->doc, pos, 1);
    else if (scan == 82)
        pos = ted_offset(s, line ? line - 1 : 0, col);
    else if (scan == 81)
        pos = ted_offset(s, line + 1, col);
    else if (scan == 74)
        pos = ted_document_line(&s->doc, line);
    else if (scan == 77) {
        pos = ted_document_line(&s->doc, line);
        while (pos < s->doc.length && s->doc.text[pos] != '\n')
            ++pos;
    } else if (scan == 75 || scan == 78) {
        size_t rows = s->body.g_h / s->ch;
        pos = ted_offset(
            s, scan == 75 ? (line > rows ? line - rows : 0) : line + rows, col);
    } else {
        if (scan == 42 || scan == 76) {
            if (s->doc.cursor == s->doc.anchor)
                s->doc.anchor =
                    ted_document_step(&s->doc, pos, scan == 42 ? -1 : 1);
            ted_document_insert(&s->doc, "", 0);
        } else if (scan == 40)
            ted_document_insert(&s->doc, s->crlf ? "\r\n" : "\n",
                                s->crlf ? 2 : 1);
        else if (scan == 43)
            ted_document_insert(&s->doc, "\t", 1);
        else if (ascii >= 32 && ascii < 127) {
            char c = (char)ascii;
            if (!ted_document_insert(&s->doc, &c, 1))
                form_alert(1, "[1][ Text exceeds the 1 MiB limit. ][ OK ]");
        }
        return;
    }
    s->doc.cursor = pos;
    if (!(mods & (K_LSHIFT | K_RSHIFT)))
        s->doc.anchor = pos;
}
int main(int argc, char **argv)
{
    ted_state_t s = {0};
    WORD cw, ch, bw, bh;
    if (appl_init() < 0 || !ted_document_init(&s.doc))
        return 1;
    s.vdi = graf_handle(&cw, &ch, &bw, &bh);
    s.char_h = ch;
    s.box_h = bh;
    s.cw = 8;
    s.ch = 16;
    vst_font(s.vdi, ATARI);
    WORD extent[8];
    vqt_extent(s.vdi, "M", extent);
    s.cw = extent[2] + 1;
    if (s.cw < 1)
        s.cw = 8;
    ted_menu(&s);
    GRECT screen;
    wind_get(0, WF_WORKXYWH, &screen.g_x, &screen.g_y, &screen.g_w,
             &screen.g_h);
    WORD width = screen.g_w > 760 ? 760 : screen.g_w - 24,
         height = screen.g_h > 540 ? 540 : screen.g_h - 24;
    s.window =
        wind_create(NAME | CLOSER | MOVER | SIZER | UPARROW | DNARROW | VSLIDE,
                    screen.g_x + 12, screen.g_y + 12, width, height);
    if (s.window <= 0) {
        ted_document_free(&s.doc);
        appl_exit();
        return 1;
    }
    wind_open(s.window, screen.g_x + 12, screen.g_y + 12, width, height);
    s.running = 1;
    ted_layout(&s);
    if (argc > 1)
        ted_load(&s, argv[1]);
    ted_draw(&s);
    while (s.running) {
        WORD mx, my, buttons, mods, key, clicks, msg[8];
        WORD event = evnt_multi(MU_MESAG | GEM_MU_KEY | MU_BUTTON | MU_TIMER, 1,
                                1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, msg, 30, 0,
                                &mx, &my, &buttons, &mods, &key, &clicks);
        int redraw = 0;
        if (!event)
            break;
        if (event & MU_MESAG) {
            if (msg[0] == AP_TERM)
                ted_action(&s, act_quit);
            else if (msg[0] == MN_SELECTED) {
                ted_action(&s, msg[4]);
                menu_tnormal(s.menu, msg[3], 1);
                redraw = 1;
            } else if (msg[3] == s.window) {
                redraw = 1;
                if (msg[0] == WM_CLOSED)
                    ted_action(&s, act_quit);
                else if (msg[0] == WM_MOVED || msg[0] == WM_SIZED) {
                    wind_set(s.window, WF_WXYWH, msg[4], msg[5], msg[6],
                             msg[7]);
                    ted_layout(&s);
                } else if (msg[0] == WM_TOPPED)
                    wind_set(s.window, WF_TOP, 0, 0, 0, 0);
                else if (msg[0] == WM_ARROWED) {
                    size_t step = msg[4] < 2 ? (size_t)(s.body.g_h / s.ch) : 1;
                    if (msg[4] == 0 || msg[4] == 2)
                        s.top = s.top > step ? s.top - step : 0;
                    else
                        s.top += step;
                } else if (msg[0] == WM_VSLID) {
                    size_t lines, column;
                    ted_document_position(&s.doc, s.doc.length, &lines,
                                          &column);
                    size_t rows = s.body.g_h / s.ch;
                    s.top = lines + 1 > rows
                                ? (lines + 1 - rows) * msg[4] / 1000
                                : 0;
                }
            }
        }
        if ((event & GEM_MU_KEY) && (clicks & GEM_KEY_PRESS)) {
            ted_key(&s, key, mods);
            ted_follow(&s);
            redraw = 1;
        }
        if ((event & MU_BUTTON) && wind_find(mx, my) == s.window) {
            if (my >= s.chrome.toolbar.g_y && my < s.body.g_y) {
                WORD action = ted_toolbar_action(&s, mx, my);
                if (action) {
                    ted_action(&s, action);
                    ted_follow(&s);
                }
                redraw = 1;
            } else if (my >= s.body.g_y && my < s.body.g_y + s.body.g_h &&
                       mx >= s.body.g_x && mx < s.body.g_x + s.body.g_w) {
                s.doc.cursor = ted_offset(
                    &s, s.top + (my - s.body.g_y) / s.ch,
                    s.left + (mx > s.body.g_x + 4 ? (mx - s.body.g_x - 4) / s.cw
                                                  : 0));
                if (!(mods & K_LSHIFT))
                    s.doc.anchor = s.doc.cursor;
                s.dragging = 1;
                redraw = 1;
            }
        }
        if (s.dragging) {
            if (!(buttons & 1))
                s.dragging = 0;
            else if (my >= s.body.g_y && my < s.body.g_y + s.body.g_h) {
                size_t next = ted_offset(
                    &s, s.top + (my - s.body.g_y) / s.ch,
                    s.left + (mx > s.body.g_x + 4 ? (mx - s.body.g_x - 4) / s.cw
                                                  : 0));
                if (next != s.doc.cursor) {
                    s.doc.cursor = next;
                    redraw = 1;
                }
            }
        }
        WORD hover = ted_toolbar_action(&s, mx, my);
        if (hover && wind_find(mx, my) != s.window)
            hover = 0;
        if (hover != s.hover_action) {
            s.hover_action = hover;
            redraw = 1;
        }
        if (redraw && s.running)
            ted_draw(&s);
    }
    menu_bar(s.menu, 0);
    wind_close(s.window);
    wind_delete(s.window);
    free(s.clipboard);
    ted_document_free(&s.doc);
    appl_exit();
    return 0;
}
