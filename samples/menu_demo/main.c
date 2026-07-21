/*
 * Demonstrates gemd's multi-process menu support: run two instances
 * of this program (see the "Debug Menu Demo" compound launch config)
 * against one gemd. Each instance installs its own menu_bar() over
 * the GEM_RPC_MENU_BAR wire path; clicking between the two windows
 * should switch the shared top menu bar to whichever instance's
 * window is on top, while both windows stay visible throughout.
 *
 * MIT License (see: LICENSE)
 * Copyright (C) 2026 tomaz stih
 */
#include <gem.h>

#include <ctype.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

/*
 * evnt_mesag() busy-polls gemd internally and only returns once a
 * message arrives, so a flag checked between calls would never be
 * seen while blocked inside it. Terminate immediately instead --
 * gemd notices the closed socket and tears down this app's windows
 * and menu on its own.
 */
static void handle_signal(int signum)
{
    (void) signum;
    _exit(0);
}

enum {
    TITLE_LABEL = 3,
    ITEM_QUIT = 6
};

static char g_title_text[32];

static OBJECT g_tree[] = {
    {-1, 1, 3, G_IBOX, NONE, NORMAL, 0L, 0, 0, 0, 0},
    {0, 2, 2, G_BOX, NONE, NORMAL, 0x1100L, 0, 0, 80, 1},
    {1, 3, 3, G_IBOX, NONE, NORMAL, 0L, 0, 0, 80, 1},
    {2, -1, -1, G_TITLE, NONE, NORMAL, 0L, 0, 0, 8, 1},
    {0, 5, 5, G_IBOX, NONE, NORMAL, 0L, 0, 0, 0, 0},
    {4, 6, 6, G_BOX, NONE, NORMAL, 0x1100L, 0, 0, 20, 1},
    {5, -1, -1, G_STRING, LASTOB, NORMAL, (LONG) "  Quit \t^Q", 0, 0, 20, 1}
};

static void redraw_window(WORD handle, VDI_HANDLE vdi_handle, char label)
{
    GRECT work;
    GRECT visible;
    WORD clip_xy[4];
    WORD fill[4];
    char text[32];

    snprintf(text, sizeof(text), "Menu Demo %c", label);

    wind_update(BEG_UPDATE);
    wind_get(handle, WF_WORKXYWH, &work.g_x, &work.g_y, &work.g_w, &work.g_h);
    wind_get(handle, WF_FIRSTXYWH, &visible.g_x, &visible.g_y, &visible.g_w,
        &visible.g_h);

    while (visible.g_w > 0 && visible.g_h > 0) {
        clip_xy[0] = visible.g_x;
        clip_xy[1] = visible.g_y;
        clip_xy[2] = (WORD) (visible.g_x + visible.g_w - 1);
        clip_xy[3] = (WORD) (visible.g_y + visible.g_h - 1);
        vs_clip(vdi_handle, 1, clip_xy);

        fill[0] = visible.g_x;
        fill[1] = visible.g_y;
        fill[2] = clip_xy[2];
        fill[3] = clip_xy[3];
        /*
         * The VDI color-index-to-pixel mapping is inverted (see
         * _vdi_color_to_pixel), so BLACK paints white pixels and
         * WHITE paints black ones -- matching how the AES engine's
         * own _aes_light_color()/_aes_dark_color() do it.
         */
        vsf_color(vdi_handle, BLACK);
        vr_recfl(vdi_handle, fill);
        vst_color(vdi_handle, WHITE);
        v_gtext(vdi_handle, (WORD) (work.g_x + 12), (WORD) (work.g_y + 18),
            (CONST BYTE *) text);
        v_gtext(vdi_handle, (WORD) (work.g_x + 12), (WORD) (work.g_y + 34),
            (CONST BYTE *) "Click the other window,");
        v_gtext(vdi_handle, (WORD) (work.g_x + 12), (WORD) (work.g_y + 46),
            (CONST BYTE *) "then look at the menu bar.");

        wind_get(handle, WF_NEXTXYWH, &visible.g_x, &visible.g_y,
            &visible.g_w, &visible.g_h);
    }

    vs_clip(vdi_handle, 0, clip_xy);
    wind_update(END_UPDATE);
}

int main(int argc, char *argv[])
{
    WORD app_id;
    VDI_HANDLE vdi_handle;
    GRECT full;
    GRECT outer;
    WORD win_handle;
    WORD msg[8];
    char label = 'A';
    char window_title[32];
    WORD x_offset = 0;
    int running = 1;

    if (argc > 1 && argv[1][0] != '\0') {
        label = (char) toupper((unsigned char) argv[1][0]);
    }
    if (label == 'B') {
        x_offset = 260;
    }

    snprintf(g_title_text, sizeof(g_title_text), " Demo %c ", label);
    snprintf(window_title, sizeof(window_title), "Menu Demo %c", label);
    g_tree[TITLE_LABEL].ob_spec = (LONG) (intptr_t) g_title_text;

    (void) signal(SIGINT, handle_signal);
    (void) signal(SIGTERM, handle_signal);

    app_id = appl_init();
    if (app_id <= 0) {
        fprintf(stderr,
            "menu_demo %c: appl_init failed -- is gemd running?\n", label);
        return 1;
    }

    vdi_handle = graf_handle();
    if (vdi_handle == 0) {
        fprintf(stderr, "menu_demo %c: graf_handle failed\n", label);
        (void) appl_exit();
        return 1;
    }

    wind_get(0, WF_WORKXYWH, &full.g_x, &full.g_y, &full.g_w, &full.g_h);
    (void) wind_calc(WC_BORDER, NAME | CLOSER | MOVER,
        (WORD) (full.g_x + 20 + x_offset), (WORD) (full.g_y + 40), 220, 110,
        &outer.g_x, &outer.g_y, &outer.g_w, &outer.g_h);

    win_handle = wind_create(NAME | CLOSER | MOVER, full.g_x, full.g_y,
        full.g_w, full.g_h);
    if (win_handle <= 0) {
        fprintf(stderr, "menu_demo %c: wind_create failed\n", label);
        (void) appl_exit();
        return 1;
    }

    wind_set(win_handle, WF_NAME, window_title, 0, 0);
    if (!wind_open(win_handle, outer.g_x, outer.g_y, outer.g_w, outer.g_h)) {
        fprintf(stderr, "menu_demo %c: wind_open failed\n", label);
        (void) wind_delete(win_handle);
        (void) appl_exit();
        return 1;
    }

    if (!menu_bar(g_tree, 1)) {
        fprintf(stderr, "menu_demo %c: menu_bar failed\n", label);
    }

    redraw_window(win_handle, vdi_handle, label);

    printf("menu_demo %c: window %d open, menu \"%s\" installed.\n",
        label, win_handle, g_title_text);
    printf("menu_demo %c: click between windows and watch the top menu\n"
           "  switch; Ctrl-Q or the Quit menu item exits this instance.\n",
        label);
    fflush(stdout);

    while (running) {
        if (!evnt_mesag(msg)) {
            break;
        }

        switch (msg[0]) {
        case WM_REDRAW:
            if (msg[3] == win_handle) {
                redraw_window(win_handle, vdi_handle, label);
            }
            break;

        case WM_MOVED:
        case WM_SIZED:
            if (msg[3] == win_handle) {
                (void) wind_set(win_handle, WF_CURRXYWH, msg[4], msg[5],
                    msg[6], msg[7]);
            }
            break;

        case WM_TOPPED:
            if (msg[3] == win_handle) {
                (void) wind_set(win_handle, WF_TOP, 0, 0, 0, 0);
            }
            break;

        case WM_CLOSED:
            if (msg[3] == win_handle) {
                running = 0;
            }
            break;

        case MN_SELECTED:
            if (msg[3] == TITLE_LABEL && msg[4] == ITEM_QUIT) {
                running = 0;
            }
            break;

        default:
            break;
        }
    }

    (void) wind_close(win_handle);
    (void) wind_delete(win_handle);
    (void) appl_exit();
    printf("menu_demo %c: exiting\n", label);
    return 0;
}
