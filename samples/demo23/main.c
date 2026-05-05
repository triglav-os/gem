#include <gem.h>
#include <stdio.h>
#include <string.h>

WORD appl_id;
VDI_HANDLE vdi_handle;
WORD work_in[11] = {1,1,1,1,1,1,1,1,1,1,2};
WORD work_out[57];
WORD win_handle;
char message[80] = "Select a menu item...";

#define MENU_ID 0  /* We'll build a simple menu tree manually */
#define TITLE_DESK 2
#define TITLE_FILE 3
#define TITLE_HELP 4
#define ITEM_DESK_ABOUT 6
#define ITEM_DESK_QUIT 8
#define ITEM_FILE_OPEN 10
#define ITEM_FILE_SAVE 11
#define ITEM_FILE_EXIT 12
#define ITEM_HELP_ABOUT 14

/* Simple menu tree (TITLE + ITEMs) */
OBJECT menu_tree[] = {
    /* Root */
    {-1,1,3, G_IBOX, NONE, NORMAL, 0L, 0,0,0,0},
    /* Menu bar */
    {0,2,2, G_BOX, NONE, NORMAL, 0x1100L, 0,0,80,1},  /* Adjust width */
    /* Titles */
    {1, -1,-1, G_TITLE, NONE, NORMAL, (long)" Desk ", 0,0,6,1},
    {1, -1,-1, G_TITLE, NONE, NORMAL, (long)" File ", 6,0,6,1},
    {1, -1,-1, G_TITLE, NONE, NORMAL, (long)" Help ",12,0,6,1},
    /* Desk menu */
    {0,5,8, G_BOX, NONE, NORMAL, 0x1100L, 0,0,20,4},
    {4, -1,-1, G_STRING, NONE, NORMAL, (long)"  About... ", 0,0,20,1},
    {4, -1,-1, G_STRING, NONE, NORMAL, (long)"  ------------ ", 0,1,20,1},
    {4, -1,-1, G_STRING, NONE, NORMAL, (long)"  Quit     ", 0,2,20,1},
    /* File menu */
    {0,9,11, G_BOX, NONE, NORMAL, 0x1100L, 0,0,20,3},
    {8, -1,-1, G_STRING, NONE, NORMAL, (long)"  Open     ", 0,0,20,1},
    {8, -1,-1, G_STRING, NONE, NORMAL, (long)"  Save     ", 0,1,20,1},
    {8, -1,-1, G_STRING, NONE, NORMAL, (long)"  Exit     ", 0,2,20,1},
    /* Help menu */
    {0,12,12, G_BOX, NONE, NORMAL, 0x1100L, 0,0,20,1},
    {11,-1,-1, G_STRING, LASTOB, NORMAL, (long)"  About GEM Demo ", 0,0,20,1},
};

/* Forward declarations */
void redraw_window(GRECT *clip);
void redraw_message_area(void);
void handle_menu(int title, int item);

/* Main */
int main(void)
{
    WORD msg[8];
    GRECT work, full;

    appl_id = appl_init();
    if (appl_id < 0) return -1;

    /* Open VDI workstation */
    vdi_handle = graf_handle();
    v_opnvwk(work_in, &vdi_handle, work_out);

    /* Create window */
    wind_get(0, WF_WORKXYWH, &full.g_x, &full.g_y, &full.g_w, &full.g_h);
    win_handle = wind_create(NAME|CLOSER|FULLER|MOVER|SIZER, full.g_x, full.g_y, full.g_w, full.g_h);
    if (win_handle < 0) {
        v_clsvwk(vdi_handle);
        appl_exit();
        return -1;
    }

    wind_set(win_handle, WF_NAME, "GEM Menu Demo", 0, 0);
    wind_open(win_handle, full.g_x+20, full.g_y+20, full.g_w-40, full.g_h-60);

    /* Attach menu */
    menu_bar(menu_tree, 1);

    /* Event loop */
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
                wind_set(win_handle, WF_CURRXYWH, msg[4], msg[5], msg[6], msg[7]);
                break;

            case MN_SELECTED:
                handle_menu(msg[3], msg[4]);
                menu_tnormal(menu_tree, msg[3], 1);  /* Deselect title */
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
                (WORD) (work.g_y + 20), message);
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
    dirty.g_h = 40;
    redraw_window(&dirty);
}

void handle_menu(int title, int item)
{
    char buf[64];

    (void) buf;

    /* Find which menu was selected */
    if (title == TITLE_DESK) {
        if (item == ITEM_DESK_ABOUT) {
            form_alert(1, "[1][ GEM Menu Demo ][ OK ]");
            strcpy(message, "About selected");
        } else if (item == ITEM_DESK_QUIT) {
            strcpy(message, "Quit selected - closing...");
            /* In real code you'd exit here */
        }
    } else if (title == TITLE_FILE) {
        if (item == ITEM_FILE_OPEN) {
            strcpy(message, "Open selected");
        } else if (item == ITEM_FILE_SAVE) {
            strcpy(message, "Save selected");
        } else if (item == ITEM_FILE_EXIT) {
            strcpy(message, "Exit selected");
        }
    } else if (title == TITLE_HELP && item == ITEM_HELP_ABOUT) {
        strcpy(message, "Help -> About GEM Demo");
    }

    redraw_message_area();
}
