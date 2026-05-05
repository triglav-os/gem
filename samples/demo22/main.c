/*
 * Demonstrates a classic GEM AES application with a hand-built menu
 * tree, one document window, and message-driven redraw handling.
 * The source is intentionally written in an Atari ST-compatible style
 * so AES compatibility issues surface in the runtime layer instead of
 * being hidden in sample-side hosted glue.
 *
 * MIT License (see: LICENSE)
 * Copyright (C) 2026 tomaz stih
 */

#include <gem.h>
#include <stdio.h>
#include <string.h>

WORD appl_id;
VDI_HANDLE vdi_handle;
WORD work_in[11] = {1,1,1,1,1,1,1,1,1,1,2};
WORD work_out[57];
WORD win_handle;
char message[96] = "Choose a menu item to begin.";
WORD option_checked = 0;

#define TITLE_FILE           3
#define TITLE_OPTIONS        4
#define TITLE_HELP           5
#define ITEM_FILE_ABOUT      8
#define ITEM_FILE_QUIT       10
#define ITEM_OPTIONS_TOGGLE  12
#define ITEM_OPTIONS_RESET   13
#define ITEM_HELP_KEYS       15
#define ITEM_HELP_ABOUT      16

OBJECT menu_tree[] = {
    {-1, 1, 3, G_IBOX, NONE, NORMAL, 0L, 0, 0, 0, 0},
    {0, 2, 2, G_BOX, NONE, NORMAL, 0x1100L, 0, 0, 80, 1},
    {1, 3, 5, G_IBOX, NONE, NORMAL, 0L, 0, 0, 80, 1},
    {2, -1, -1, G_TITLE, NONE, NORMAL, (LONG) " File ", 0, 0, 6, 1},
    {2, -1, -1, G_TITLE, NONE, NORMAL, (LONG) " Options ", 6, 0, 9, 1},
    {2, -1, -1, G_TITLE, NONE, NORMAL, (LONG) " Help ", 15, 0, 6, 1},
    {0, 7, 16, G_IBOX, NONE, NORMAL, 0L, 0, 0, 0, 0},
    {6, 8, 10, G_BOX, NONE, NORMAL, 0x1100L, 0, 0, 20, 4},
    {7, -1, -1, G_STRING, NONE, NORMAL, (LONG) "  About Demo 22 ", 0, 0, 20, 1},
    {7, -1, -1, G_STRING, NONE, NORMAL, (LONG) "  ------------ ", 0, 1, 20, 1},
    {7, -1, -1, G_STRING, NONE, NORMAL, (LONG) "  Quit ", 0, 2, 20, 1},
    {6, 12, 13, G_BOX, NONE, NORMAL, 0x1100L, 0, 0, 20, 3},
    {11, -1, -1, G_STRING, NONE, NORMAL, (LONG) "  Toggle option ", 0, 0, 20, 1},
    {11, -1, -1, G_STRING, NONE, NORMAL, (LONG) "  Reset defaults ", 0, 1, 20, 1},
    {6, 15, 16, G_BOX, NONE, NORMAL, 0x1100L, 0, 0, 24, 3},
    {14, -1, -1, G_STRING, NONE, NORMAL, (LONG) "  Esc closes app ", 0, 0, 24, 1},
    {14, -1, -1, G_STRING, LASTOB, NORMAL, (LONG) "  About GEM Demo ", 0, 1, 24, 1}
};

void redraw_window(GRECT *clip);
void redraw_message_area(void);
void handle_menu(WORD title, WORD item);

int main(void)
{
    WORD msg[8];
    GRECT full;

    appl_id = appl_init();
    if (appl_id < 0) {
        return -1;
    }

    vdi_handle = graf_handle();
    v_opnvwk(work_in, &vdi_handle, work_out);

    wind_get(0, WF_WORKXYWH, &full.g_x, &full.g_y, &full.g_w, &full.g_h);
    win_handle = wind_create(NAME | CLOSER | FULLER | MOVER | SIZER,
        full.g_x, full.g_y, full.g_w, full.g_h);
    if (win_handle < 0) {
        v_clsvwk(vdi_handle);
        appl_exit();
        return -1;
    }

    wind_set(win_handle, WF_NAME, "Demo 22 Main Menu", 0, 0);
    wind_open(win_handle, full.g_x + 20, full.g_y + 20,
        full.g_w - 80, full.g_h - 80);

    menu_click(1, 1);
    menu_bar(menu_tree, 1);
    menu_icheck(menu_tree, ITEM_OPTIONS_TOGGLE, option_checked);

    while (1) {
        evnt_mesag(msg);

        switch (msg[0]) {
        case WM_REDRAW:
            redraw_window((GRECT *) &msg[4]);
            break;

        case WM_CLOSED:
            goto cleanup;

        case WM_MOVED:
        case WM_SIZED:
            wind_set(win_handle, WF_CURRXYWH,
                msg[4], msg[5], msg[6], msg[7]);
            break;

        case WM_TOPPED:
            wind_set(win_handle, WF_TOP, 0, 0, 0, 0);
            break;

        case MN_SELECTED:
            handle_menu(msg[3], msg[4]);
            menu_tnormal(menu_tree, msg[3], 1);
            break;
        }
    }

cleanup:
    menu_bar(menu_tree, 0);
    wind_close(win_handle);
    wind_delete(win_handle);
    v_clsvwk(vdi_handle);
    appl_exit();
    return 0;
}

void redraw_window(GRECT *clip)
{
    GRECT work;
    GRECT visible;
    WORD pxy[4];
    WORD clip_xy[4];
    WORD x0;
    WORD y0;
    WORD x1;
    WORD y1;

    wind_update(BEG_UPDATE);
    wind_get(win_handle, WF_WORKXYWH,
        &work.g_x, &work.g_y, &work.g_w, &work.g_h);
    wind_get(win_handle, WF_FIRSTXYWH,
        &visible.g_x, &visible.g_y, &visible.g_w, &visible.g_h);

    while (visible.g_w > 0 && visible.g_h > 0) {
        x0 = visible.g_x;
        y0 = visible.g_y;
        x1 = (WORD) (visible.g_x + visible.g_w - 1);
        y1 = (WORD) (visible.g_y + visible.g_h - 1);

        if (clip != NULL) {
            WORD clip_x1 = (WORD) (clip->g_x + clip->g_w - 1);
            WORD clip_y1 = (WORD) (clip->g_y + clip->g_h - 1);

            if (x0 < clip->g_x) {
                x0 = clip->g_x;
            }
            if (y0 < clip->g_y) {
                y0 = clip->g_y;
            }
            if (x1 > clip_x1) {
                x1 = clip_x1;
            }
            if (y1 > clip_y1) {
                y1 = clip_y1;
            }
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

            v_gtext(vdi_handle, (WORD) (work.g_x + 10),
                (WORD) (work.g_y + 20),
                "Demo 22 enables the main menu bar.");
            v_gtext(vdi_handle, (WORD) (work.g_x + 10),
                (WORD) (work.g_y + 36),
                "Use File, Options, and Help above.");
            v_gtext(vdi_handle, (WORD) (work.g_x + 10),
                (WORD) (work.g_y + 60), message);
        }

        wind_get(win_handle, WF_NEXTXYWH,
            &visible.g_x, &visible.g_y, &visible.g_w, &visible.g_h);
    }

    vs_clip(vdi_handle, 0, NULL);
    v_updwk(vdi_handle);
    wind_update(END_UPDATE);
}

void redraw_message_area(void)
{
    GRECT work;
    GRECT dirty;

    wind_get(win_handle, WF_WORKXYWH,
        &work.g_x, &work.g_y, &work.g_w, &work.g_h);
    dirty.g_x = work.g_x;
    dirty.g_y = work.g_y;
    dirty.g_w = work.g_w;
    dirty.g_h = 80;
    redraw_window(&dirty);
}

void handle_menu(WORD title, WORD item)
{
    if (title == TITLE_FILE) {
        if (item == ITEM_FILE_ABOUT) {
            strcpy(message, "File -> About selected");
        } else if (item == ITEM_FILE_QUIT) {
            strcpy(message, "File -> Quit selected");
        }
    } else if (title == TITLE_OPTIONS) {
        if (item == ITEM_OPTIONS_TOGGLE) {
            option_checked = !option_checked;
            menu_icheck(menu_tree, ITEM_OPTIONS_TOGGLE, option_checked);
            strcpy(message, "Options -> Toggle selected");
        } else if (item == ITEM_OPTIONS_RESET) {
            option_checked = 0;
            menu_icheck(menu_tree, ITEM_OPTIONS_TOGGLE, 0);
            strcpy(message, "Options -> Reset selected");
        }
    } else if (title == TITLE_HELP) {
        if (item == ITEM_HELP_KEYS) {
            strcpy(message, "Help -> Esc closes app");
        } else if (item == ITEM_HELP_ABOUT) {
            strcpy(message, "Help -> About GEM Demo");
        }
    }

    redraw_message_area();
}
