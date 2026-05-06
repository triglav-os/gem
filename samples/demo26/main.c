/*
 * Demonstrates a classic GEM application that combines a menu bar,
 * multiple windows, and a modal dialog. It is meant to stress how the
 * hosted AES coordinates menu tracking, redraw clipping, and top-window
 * changes in one old-style application.
 *
 * MIT License (see: LICENSE)
 * Copyright (C) 2026 tomaz stih
 */

#include <gem.h>
#include <string.h>

enum {
    D26_DIALOG_ROOT = 0,
    D26_DIALOG_TEXT,
    D26_DIALOG_OK,
    D26_DIALOG_COUNT
};

#define D26_TITLE_DESK       3
#define D26_TITLE_WINDOWS    4
#define D26_TITLE_DIALOG     5
#define D26_ITEM_ABOUT       8
#define D26_ITEM_QUIT        10
#define D26_ITEM_TOP_MAIN    12
#define D26_ITEM_TOP_LOG     13
#define D26_ITEM_TOGGLE_LOG  14
#define D26_ITEM_SHOW_DIALOG 16

WORD appl_id;
VDI_HANDLE vdi_handle;
WORD work_in[11] = {1,1,1,1,1,1,1,1,1,1,2};
WORD work_out[57];
WORD main_handle = -1;
WORD log_handle = -1;
WORD log_open = 1;
GRECT full_rect;
GRECT main_normal;
GRECT log_normal;
char main_message[80] = "Use menu commands to top windows and open dialog.";
char log_message[80] = "Second window helps stress topping and redraw.";

OBJECT menu_tree[] = {
    {-1, 1, 3, G_IBOX, NONE, NORMAL, 0L, 0, 0, 0, 0},
    {0, 2, 2, G_BOX, NONE, NORMAL, 0x1100L, 0, 0, 80, 1},
    {1, 3, 5, G_IBOX, NONE, NORMAL, 0L, 0, 0, 80, 1},
    {2, -1, -1, G_TITLE, NONE, NORMAL, (LONG) " Desk ", 0, 0, 6, 1},
    {2, -1, -1, G_TITLE, NONE, NORMAL, (LONG) " Windows ", 6, 0, 9, 1},
    {2, -1, -1, G_TITLE, NONE, NORMAL, (LONG) " Dialog ", 15, 0, 8, 1},
    {0, 7, 16, G_IBOX, NONE, NORMAL, 0L, 0, 0, 0, 0},
    {6, 8, 10, G_BOX, NONE, NORMAL, 0x1100L, 0, 0, 20, 4},
    {7, -1, -1, G_STRING, NONE, NORMAL, (LONG) "  About Demo 26 ", 0, 0, 20, 1},
    {7, -1, -1, G_STRING, NONE, NORMAL, (LONG) "  ------------ ", 0, 1, 20, 1},
    {7, -1, -1, G_STRING, NONE, NORMAL, (LONG) "  Quit ", 0, 2, 20, 1},
    {6, 12, 14, G_BOX, NONE, NORMAL, 0x1100L, 0, 0, 22, 4},
    {11, -1, -1, G_STRING, NONE, NORMAL, (LONG) "  Top main window ", 0, 0, 22, 1},
    {11, -1, -1, G_STRING, NONE, NORMAL, (LONG) "  Top log window ", 0, 1, 22, 1},
    {11, -1, -1, G_STRING, NONE, NORMAL, (LONG) "  Toggle log window ", 0, 2, 22, 1},
    {6, 16, 16, G_BOX, NONE, NORMAL, 0x1100L, 0, 0, 18, 2},
    {15, -1, -1, G_STRING, LASTOB, NORMAL, (LONG) "  Show dialog ", 0, 0, 18, 1}
};

OBJECT dialog_tree[D26_DIALOG_COUNT];

void init_dialog(void)
{
    dialog_tree[D26_DIALOG_ROOT].ob_next = NIL;
    dialog_tree[D26_DIALOG_ROOT].ob_head = NIL;
    dialog_tree[D26_DIALOG_ROOT].ob_tail = NIL;
    dialog_tree[D26_DIALOG_ROOT].ob_type = G_BOX;
    dialog_tree[D26_DIALOG_ROOT].ob_flags = LASTOB;
    dialog_tree[D26_DIALOG_ROOT].ob_state = NORMAL;
    dialog_tree[D26_DIALOG_ROOT].ob_spec = 0L;
    dialog_tree[D26_DIALOG_ROOT].ob_x = 0;
    dialog_tree[D26_DIALOG_ROOT].ob_y = 0;
    dialog_tree[D26_DIALOG_ROOT].ob_width = 220;
    dialog_tree[D26_DIALOG_ROOT].ob_height = 96;

    dialog_tree[D26_DIALOG_TEXT].ob_next = NIL;
    dialog_tree[D26_DIALOG_TEXT].ob_head = NIL;
    dialog_tree[D26_DIALOG_TEXT].ob_tail = NIL;
    dialog_tree[D26_DIALOG_TEXT].ob_type = G_STRING;
    dialog_tree[D26_DIALOG_TEXT].ob_flags = NONE;
    dialog_tree[D26_DIALOG_TEXT].ob_state = NORMAL;
    dialog_tree[D26_DIALOG_TEXT].ob_spec =
        (LONG) (intptr_t) "Dialog from menu and window demo.";
    dialog_tree[D26_DIALOG_TEXT].ob_x = 16;
    dialog_tree[D26_DIALOG_TEXT].ob_y = 22;
    dialog_tree[D26_DIALOG_TEXT].ob_width = 180;
    dialog_tree[D26_DIALOG_TEXT].ob_height = 8;

    dialog_tree[D26_DIALOG_OK].ob_next = NIL;
    dialog_tree[D26_DIALOG_OK].ob_head = NIL;
    dialog_tree[D26_DIALOG_OK].ob_tail = NIL;
    dialog_tree[D26_DIALOG_OK].ob_type = G_BUTTON;
    dialog_tree[D26_DIALOG_OK].ob_flags = SELECTABLE | EXIT | DEFAULT;
    dialog_tree[D26_DIALOG_OK].ob_state = NORMAL;
    dialog_tree[D26_DIALOG_OK].ob_spec = (LONG) (intptr_t) "OK";
    dialog_tree[D26_DIALOG_OK].ob_x = 144;
    dialog_tree[D26_DIALOG_OK].ob_y = 58;
    dialog_tree[D26_DIALOG_OK].ob_width = 56;
    dialog_tree[D26_DIALOG_OK].ob_height = 22;

    objc_add(dialog_tree, D26_DIALOG_ROOT, D26_DIALOG_TEXT);
    objc_add(dialog_tree, D26_DIALOG_ROOT, D26_DIALOG_OK);
}

void draw_window(WORD handle, const char *message, GRECT *clip)
{
    GRECT work;
    GRECT box;
    WORD clip_xy[4];
    WORD pxy[4];

    wind_update(BEG_UPDATE);
    wind_get(handle, WF_WORKXYWH, &work.g_x, &work.g_y, &work.g_w, &work.g_h);
    wind_get(handle, WF_FIRSTXYWH, &box.g_x, &box.g_y, &box.g_w, &box.g_h);

    while (box.g_w > 0 && box.g_h > 0) {
        WORD x0 = box.g_x;
        WORD y0 = box.g_y;
        WORD x1 = (WORD) (box.g_x + box.g_w - 1);
        WORD y1 = (WORD) (box.g_y + box.g_h - 1);

        if (clip != NULL) {
            WORD dx1 = (WORD) (clip->g_x + clip->g_w - 1);
            WORD dy1 = (WORD) (clip->g_y + clip->g_h - 1);

            if (x0 < clip->g_x) x0 = clip->g_x;
            if (y0 < clip->g_y) y0 = clip->g_y;
            if (x1 > dx1) x1 = dx1;
            if (y1 > dy1) y1 = dy1;
        }

        if (x0 <= x1 && y0 <= y1) {
            clip_xy[0] = x0;
            clip_xy[1] = y0;
            clip_xy[2] = x1;
            clip_xy[3] = y1;
            vs_clip(vdi_handle, 1, clip_xy);

            pxy[0] = x0;
            pxy[1] = y0;
            pxy[2] = x1;
            pxy[3] = y1;
            vr_recfl(vdi_handle, pxy);
            v_gtext(vdi_handle, (WORD) (work.g_x + 16),
                (WORD) (work.g_y + 28), (BYTE *) message);
        }

        wind_get(handle, WF_NEXTXYWH, &box.g_x, &box.g_y, &box.g_w, &box.g_h);
    }

    vs_clip(vdi_handle, 0, NULL);
    v_updwk(vdi_handle);
    wind_update(END_UPDATE);
}

void show_dialog(void)
{
    WORD cx;
    WORD cy;
    WORD cw;
    WORD ch;
    WORD mx = 0;
    WORD my = 0;
    WORD mb = 0;
    WORD ks = 0;
    WORD kr = 0;
    WORD br = 0;
    WORD event;

    form_center(dialog_tree, &cx, &cy, &cw, &ch);
    dialog_tree[D26_DIALOG_ROOT].ob_x = cx;
    dialog_tree[D26_DIALOG_ROOT].ob_y = cy;
    form_dial(FMD_START, 0, 0, 0, 0, cx, cy, cw, ch);
    objc_draw(dialog_tree, ROOT, MAX_DEPTH, cx, cy, cw, ch);

    while (1) {
        WORD object;

        event = evnt_multi(MU_BUTTON | MU_KEYBD,
            1, 1, 1,
            0, 0, 0, 0, 0,
            0, 0, 0, 0, 0,
            NULL, 0, 0, &mx, &my, &mb, &ks, &kr, &br);
        if ((event & MU_KEYBD) != 0 && (kr & 0xff) == 27) {
            break;
        }
        if ((event & MU_BUTTON) == 0) {
            continue;
        }

        object = objc_find(dialog_tree, ROOT, MAX_DEPTH, mx, my);
        if (object == D26_DIALOG_OK) {
            GRECT rect;
            WORD ox;
            WORD oy;
            UWORD old_state;

            objc_offset(dialog_tree, D26_DIALOG_OK, &ox, &oy);
            rect.g_x = ox;
            rect.g_y = oy;
            rect.g_w = dialog_tree[D26_DIALOG_OK].ob_width;
            rect.g_h = dialog_tree[D26_DIALOG_OK].ob_height;
            old_state = dialog_tree[D26_DIALOG_OK].ob_state;
            objc_change(dialog_tree, D26_DIALOG_OK, 0,
                rect.g_x, rect.g_y, rect.g_w, rect.g_h,
                (WORD) (old_state | SELECTED), 1);
            evnt_timer(90, 0);
            objc_change(dialog_tree, D26_DIALOG_OK, 0,
                rect.g_x, rect.g_y, rect.g_w, rect.g_h,
                old_state, 1);
            break;
        }
    }

    form_dial(FMD_FINISH, 0, 0, 0, 0, cx, cy, cw, ch);
}

void open_log_window(void)
{
    if (log_handle >= 0) {
        return;
    }

    log_handle = wind_create(NAME | CLOSER | FULLER | MOVER | SIZER,
        full_rect.g_x, full_rect.g_y, full_rect.g_w, full_rect.g_h);
    if (log_handle < 0) {
        return;
    }

    wind_set(log_handle, WF_NAME, "Demo 26 Log", 0, 0);
    wind_open(log_handle, log_normal.g_x, log_normal.g_y,
        log_normal.g_w, log_normal.g_h);
    log_open = 1;
}

void close_log_window(void)
{
    if (log_handle < 0) {
        return;
    }
    wind_close(log_handle);
    wind_delete(log_handle);
    log_handle = -1;
    log_open = 0;
}

int main(void)
{
    WORD msg[8];

    appl_id = appl_init();
    if (appl_id < 0) {
        return 1;
    }

    vdi_handle = graf_handle();
    v_opnvwk(work_in, &vdi_handle, work_out);
    init_dialog();

    wind_get(0, WF_WORKXYWH, &full_rect.g_x, &full_rect.g_y,
        &full_rect.g_w, &full_rect.g_h);
    main_handle = wind_create(NAME | CLOSER | FULLER | MOVER | SIZER,
        full_rect.g_x, full_rect.g_y, full_rect.g_w, full_rect.g_h);
    log_handle = wind_create(NAME | CLOSER | FULLER | MOVER | SIZER,
        full_rect.g_x, full_rect.g_y, full_rect.g_w, full_rect.g_h);

    if (main_handle < 0 || log_handle < 0) {
        if (main_handle >= 0) wind_delete(main_handle);
        if (log_handle >= 0) wind_delete(log_handle);
        v_clsvwk(vdi_handle);
        appl_exit();
        return 1;
    }

    wind_set(main_handle, WF_NAME, "Demo 26 Main", 0, 0);
    wind_set(log_handle, WF_NAME, "Demo 26 Log", 0, 0);
    wind_open(main_handle, full_rect.g_x + 44, full_rect.g_y + 70, 440, 210);
    wind_open(log_handle, full_rect.g_x + 180, full_rect.g_y + 170, 380, 160);

    wind_get(main_handle, WF_CURRXYWH, &main_normal.g_x, &main_normal.g_y,
        &main_normal.g_w, &main_normal.g_h);
    wind_get(log_handle, WF_CURRXYWH, &log_normal.g_x, &log_normal.g_y,
        &log_normal.g_w, &log_normal.g_h);

    menu_bar(menu_tree, 1);

    while (1) {
        evnt_mesag(msg);

        switch (msg[0]) {
        case WM_REDRAW:
        {
            GRECT dirty;

            dirty.g_x = msg[4];
            dirty.g_y = msg[5];
            dirty.g_w = msg[6];
            dirty.g_h = msg[7];
            if (msg[3] == main_handle) {
                draw_window(main_handle, main_message, &dirty);
            } else if (msg[3] == log_handle) {
                draw_window(log_handle, log_message, &dirty);
            }
            break;
        }

        case WM_CLOSED:
            if (msg[3] == main_handle) {
                goto cleanup;
            } else if (msg[3] == log_handle) {
                close_log_window();
            }
            break;

        case WM_MOVED:
        case WM_SIZED:
            wind_set(msg[3], WF_CURRXYWH, msg[4], msg[5], msg[6], msg[7]);
            if (msg[3] == main_handle) {
                main_normal.g_x = msg[4];
                main_normal.g_y = msg[5];
                main_normal.g_w = msg[6];
                main_normal.g_h = msg[7];
            } else if (msg[3] == log_handle) {
                log_normal.g_x = msg[4];
                log_normal.g_y = msg[5];
                log_normal.g_w = msg[6];
                log_normal.g_h = msg[7];
            }
            break;

        case WM_TOPPED:
            wind_set(msg[3], WF_TOP, 0, 0, 0, 0);
            break;

        case MN_SELECTED:
            if (msg[3] == D26_TITLE_DESK) {
                if (msg[4] == D26_ITEM_ABOUT) {
                    strcpy(main_message, "Desk -> About selected");
                    show_dialog();
                } else if (msg[4] == D26_ITEM_QUIT) {
                    goto cleanup;
                }
            } else if (msg[3] == D26_TITLE_WINDOWS) {
                if (msg[4] == D26_ITEM_TOP_MAIN) {
                    wind_set(main_handle, WF_TOP, 0, 0, 0, 0);
                    strcpy(main_message, "Main window topped");
                } else if (msg[4] == D26_ITEM_TOP_LOG) {
                    if (log_handle >= 0) {
                        wind_set(log_handle, WF_TOP, 0, 0, 0, 0);
                        strcpy(log_message, "Log window topped");
                    }
                } else if (msg[4] == D26_ITEM_TOGGLE_LOG) {
                    if (log_handle >= 0) {
                        close_log_window();
                        strcpy(main_message, "Log window closed");
                    } else {
                        open_log_window();
                        strcpy(main_message, "Log window reopened");
                    }
                }
            } else if (msg[3] == D26_TITLE_DIALOG &&
                       msg[4] == D26_ITEM_SHOW_DIALOG) {
                strcpy(main_message, "Dialog opened from menu");
                show_dialog();
            }

            menu_tnormal(menu_tree, msg[3], 1);
            break;
        }
    }

cleanup:
    menu_bar(menu_tree, 0);
    if (main_handle >= 0) {
        wind_close(main_handle);
        wind_delete(main_handle);
    }
    if (log_handle >= 0) {
        wind_close(log_handle);
        wind_delete(log_handle);
    }
    v_clsvwk(vdi_handle);
    appl_exit();
    return 0;
}
