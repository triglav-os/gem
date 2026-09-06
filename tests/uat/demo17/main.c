/*
 * Exercises the first libgem/gemd client-server slice with one simple
 * top-level window. The sample intentionally avoids menus, events, and
 * object trees so the initial multitasking transport can be validated
 * through the smallest possible GEM surface.
 *
 * MIT License (see: LICENSE)
 * Copyright (C) 2026 tomaz stih
 */

#include <gem.h>

#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>

enum {
    MULTI_DEFAULT_SECONDS = 30
};

static volatile sig_atomic_t g_multi_running = 1;

static void multi_handle_signal(int signum)
{
    (void) signum;
    g_multi_running = 0;
}

static int parse_seconds(int argc, char *argv[])
{
    long value;
    char *end = NULL;

    if (argc < 2 || argv[1] == NULL) {
        return MULTI_DEFAULT_SECONDS;
    }

    value = strtol(argv[1], &end, 10);
    if (end == argv[1] || *end != '\0' || value <= 0 || value > 3600) {
        return MULTI_DEFAULT_SECONDS;
    }
    return (int) value;
}

static void multi_redraw_window(WORD handle, VDI_HANDLE vdi_handle,
    const char *title)
{
    GRECT work;
    GRECT visible;
    WORD clip_xy[4];
    WORD fill[4];

    wind_update(BEG_UPDATE);
    wind_get(handle, WF_WORKXYWH, &work.g_x, &work.g_y, &work.g_w, &work.g_h);
    wind_get(handle, WF_FIRSTXYWH, &visible.g_x, &visible.g_y, &visible.g_w,
        &visible.g_h);
    fprintf(stderr, "multi: redraw work=%d,%d %dx%d first=%d,%d %dx%d\n",
        work.g_x, work.g_y, work.g_w, work.g_h,
        visible.g_x, visible.g_y, visible.g_w, visible.g_h);

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
        vsf_color(vdi_handle, WHITE);
        vr_recfl(vdi_handle, fill);
        vst_color(vdi_handle, BLACK);
        v_gtext(vdi_handle, (WORD) (work.g_x + 12), (WORD) (work.g_y + 18),
            (CONST BYTE *) title);

        wind_get(handle, WF_NEXTXYWH, &visible.g_x, &visible.g_y,
            &visible.g_w, &visible.g_h);
    }

    vs_clip(vdi_handle, 0, clip_xy);
    wind_update(END_UPDATE);
}

int main(int argc, char *argv[])
{
    WORD app_id;
    WORD char_w;
    WORD char_h;
    WORD box_w;
    WORD box_h;
    VDI_HANDLE vdi_handle;
    GRECT full;
    GRECT outer;
    WORD msg[8];
    WORD win_handle;
    int seconds = parse_seconds(argc, argv);
    int running = 1;

    (void) signal(SIGINT, multi_handle_signal);
    (void) signal(SIGTERM, multi_handle_signal);

    app_id = appl_init();
    if (app_id == 0) {
        fprintf(stderr, "multi: appl_init failed\n");
        return 1;
    }

    vdi_handle = graf_handle(&char_w, &char_h, &box_w, &box_h);
    if (vdi_handle == 0) {
        fprintf(stderr, "multi: graf_handle failed\n");
        (void) appl_exit();
        return 1;
    }

    if (!wind_get(0, WF_WORKXYWH, &full.g_x, &full.g_y, &full.g_w,
            &full.g_h)) {
        fprintf(stderr, "multi: wind_get(WF_WORKXYWH) failed\n");
        (void) appl_exit();
        return 1;
    }

    if (!wind_calc(WC_BORDER, NAME | CLOSER | MOVER,
            (WORD) (full.g_x + char_w * 4),
            (WORD) (full.g_y + char_h * 4),
            (WORD) (48 * char_w),
            (WORD) (12 * char_h),
            &outer.g_x, &outer.g_y, &outer.g_w, &outer.g_h)) {
        fprintf(stderr, "multi: wind_calc failed\n");
        (void) appl_exit();
        return 1;
    }

    win_handle = wind_create(NAME | CLOSER | MOVER,
        full.g_x, full.g_y, full.g_w, full.g_h);
    if (win_handle <= 0) {
        fprintf(stderr, "multi: wind_create failed\n");
        (void) appl_exit();
        return 1;
    }

    if (!wind_set_str(win_handle, WF_NAME, "multi")) {
        fprintf(stderr, "multi: wind_set_str failed\n");
    }
    if (!wind_open(win_handle, outer.g_x, outer.g_y, outer.g_w, outer.g_h)) {
        fprintf(stderr, "multi: wind_open failed\n");
        (void) wind_delete(win_handle);
        (void) appl_exit();
        return 1;
    }
    multi_redraw_window(win_handle, vdi_handle, "multi client via gemd");

    (void) seconds;

    printf("multi: opened window %d\n", win_handle);
    fflush(stdout);

    while (running && g_multi_running) {
        if (!evnt_mesag(msg)) {
            break;
        }

        switch (msg[0]) {
        case WM_REDRAW:
            if (msg[3] == win_handle) {
                multi_redraw_window(win_handle, vdi_handle,
                    "multi client via gemd");
            }
            break;

        case WM_MOVED:
        case WM_SIZED:
            if (msg[3] == win_handle) {
                (void) wind_set(win_handle, WF_CURRXYWH, msg[4], msg[5],
                    msg[6], msg[7]);
                multi_redraw_window(win_handle, vdi_handle,
                    "multi client via gemd");
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

        default:
            break;
        }
    }

    (void) wind_close(win_handle);
    (void) wind_delete(win_handle);
    (void) appl_exit();
    return 0;
}
