/*
 * Demonstrates two classic GEM windows with independent titles and VDI
 * fonts so overlap, topping, and redraw behavior can be checked
 * against Atari ST expectations.
 *
 * MIT License (see: LICENSE)
 * Copyright (C) 2026 tomaz stih
 */

#include <gem.h>
#include <string.h>

WORD appl_id;
VDI_HANDLE vdi_handle;
WORD work_in[11] = {1,1,1,1,1,1,1,1,1,1,2};
WORD work_out[57];
WORD msg[8];
WORD win1_handle;
WORD win2_handle;
GRECT full_rect;
GRECT win1_normal;
GRECT win2_normal;

char *win1_title = "Hello GEM";
char *win2_title = "Second Font Window";

char *win1_line1 = "Hello from GEM AES!";
char *win1_line2 = "This is a basic AES window.";
char *win1_line3 = "Close the window or press ESC to exit.";

char *win2_line1 = "Small system font window";
char *win2_line2 = "Overlap me to test redraw.";
char *win2_line3 = "This one uses the second built-in VDI font.";

WORD win1_font = 1;
WORD win2_font = 2;

void draw_window(WORD handle, WORD font_id,
    char *line1, char *line2, char *line3, GRECT *clip);

int main(void)
{
    appl_id = appl_init();
    if (appl_id < 0) {
        return -1;
    }

    vdi_handle = graf_handle();
    v_opnvwk(work_in, &vdi_handle, work_out);

    wind_get(0, WF_WORKXYWH, &full_rect.g_x, &full_rect.g_y,
        &full_rect.g_w, &full_rect.g_h);

    win1_handle = wind_create(NAME | CLOSER | FULLER | MOVER | SIZER,
        full_rect.g_x, full_rect.g_y, full_rect.g_w, full_rect.g_h);
    win2_handle = wind_create(NAME | CLOSER | FULLER | MOVER | SIZER,
        full_rect.g_x, full_rect.g_y, full_rect.g_w, full_rect.g_h);

    if (win1_handle < 0 || win2_handle < 0) {
        if (win1_handle >= 0) {
            wind_delete(win1_handle);
        }
        if (win2_handle >= 0) {
            wind_delete(win2_handle);
        }
        v_clsvwk(vdi_handle);
        appl_exit();
        return -1;
    }

    wind_set(win1_handle, WF_NAME, win1_title, 0, 0);
    wind_set(win2_handle, WF_NAME, win2_title, 0, 0);

    wind_open(win1_handle, full_rect.g_x + 60, full_rect.g_y + 60, 480, 240);
    wind_open(win2_handle, full_rect.g_x + 220, full_rect.g_y + 150,
        420, 210);

    wind_get(win1_handle, WF_CURRXYWH, &win1_normal.g_x, &win1_normal.g_y,
        &win1_normal.g_w, &win1_normal.g_h);
    wind_get(win2_handle, WF_CURRXYWH, &win2_normal.g_x, &win2_normal.g_y,
        &win2_normal.g_w, &win2_normal.g_h);

    while (1) {
        evnt_mesag(msg);

        switch (msg[0]) {
        case WM_REDRAW:
        {
            GRECT r;

            r.g_x = msg[4];
            r.g_y = msg[5];
            r.g_w = msg[6];
            r.g_h = msg[7];

            if (msg[3] == win1_handle) {
                draw_window(win1_handle, win1_font,
                    win1_line1, win1_line2, win1_line3, &r);
            } else if (msg[3] == win2_handle) {
                draw_window(win2_handle, win2_font,
                    win2_line1, win2_line2, win2_line3, &r);
            }
            break;
        }

        case WM_CLOSED:
            if (msg[3] == win1_handle) {
                wind_close(win1_handle);
                wind_delete(win1_handle);
                win1_handle = -1;
            } else if (msg[3] == win2_handle) {
                wind_close(win2_handle);
                wind_delete(win2_handle);
                win2_handle = -1;
            }

            if (win1_handle < 0 && win2_handle < 0) {
                goto cleanup;
            }
            break;

        case WM_MOVED:
        case WM_SIZED:
            wind_set(msg[3], WF_CURRXYWH, msg[4], msg[5], msg[6], msg[7]);
            if (msg[3] == win1_handle) {
                win1_normal.g_x = msg[4];
                win1_normal.g_y = msg[5];
                win1_normal.g_w = msg[6];
                win1_normal.g_h = msg[7];
            } else if (msg[3] == win2_handle) {
                win2_normal.g_x = msg[4];
                win2_normal.g_y = msg[5];
                win2_normal.g_w = msg[6];
                win2_normal.g_h = msg[7];
            }
            break;

        case WM_TOPPED:
            wind_set(msg[3], WF_TOP, 0, 0, 0, 0);
            break;

        case WM_FULLED:
        {
            GRECT current;
            GRECT *normal;

            if (msg[3] == win1_handle) {
                normal = &win1_normal;
            } else if (msg[3] == win2_handle) {
                normal = &win2_normal;
            } else {
                break;
            }

            wind_get(msg[3], WF_CURRXYWH, &current.g_x, &current.g_y,
                &current.g_w, &current.g_h);

            if (current.g_x == full_rect.g_x && current.g_y == full_rect.g_y &&
                current.g_w == full_rect.g_w && current.g_h == full_rect.g_h) {
                wind_set(msg[3], WF_CURRXYWH, normal->g_x, normal->g_y,
                    normal->g_w, normal->g_h);
            } else {
                *normal = current;
                wind_set(msg[3], WF_CURRXYWH, full_rect.g_x, full_rect.g_y,
                    full_rect.g_w, full_rect.g_h);
            }
            break;
        }
        }
    }

cleanup:
    if (win1_handle >= 0) {
        wind_close(win1_handle);
        wind_delete(win1_handle);
    }
    if (win2_handle >= 0) {
        wind_close(win2_handle);
        wind_delete(win2_handle);
    }

    v_clsvwk(vdi_handle);
    appl_exit();
    return 0;
}

void draw_window(WORD handle, WORD font_id,
    char *line1, char *line2, char *line3, GRECT *clip)
{
    GRECT work;
    GRECT box;
    WORD clip_xy[4];
    WORD pxy[4];
    WORD text_attr[6];
    WORD old_font = 0;

    wind_update(BEG_UPDATE);

    wind_get(handle, WF_WORKXYWH, &work.g_x, &work.g_y, &work.g_w, &work.g_h);
    wind_get(handle, WF_FIRSTXYWH, &box.g_x, &box.g_y, &box.g_w, &box.g_h);

    if (vqt_attributes(vdi_handle, text_attr) != 0) {
        old_font = text_attr[0];
    }

    while (box.g_w > 0 && box.g_h > 0) {
        WORD x0 = box.g_x;
        WORD y0 = box.g_y;
        WORD x1 = (WORD) (box.g_x + box.g_w - 1);
        WORD y1 = (WORD) (box.g_y + box.g_h - 1);

        if (clip != NULL) {
            WORD cx1 = (WORD) (clip->g_x + clip->g_w - 1);
            WORD cy1 = (WORD) (clip->g_y + clip->g_h - 1);

            if (x0 < clip->g_x) {
                x0 = clip->g_x;
            }
            if (y0 < clip->g_y) {
                y0 = clip->g_y;
            }
            if (x1 > cx1) {
                x1 = cx1;
            }
            if (y1 > cy1) {
                y1 = cy1;
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

            vst_font(vdi_handle, font_id);
            v_gtext(vdi_handle, (WORD) (work.g_x + 24),
                (WORD) (work.g_y + 36), (BYTE *) line1);
            v_gtext(vdi_handle, (WORD) (work.g_x + 24),
                (WORD) (work.g_y + 60), (BYTE *) line2);
            v_gtext(vdi_handle, (WORD) (work.g_x + 24),
                (WORD) (work.g_y + 84), (BYTE *) line3);
        }

        wind_get(handle, WF_NEXTXYWH,
            &box.g_x, &box.g_y, &box.g_w, &box.g_h);
    }

    if (old_font != 0) {
        vst_font(vdi_handle, old_font);
    }

    vs_clip(vdi_handle, 0, NULL);
    v_updwk(vdi_handle);
    wind_update(END_UPDATE);
}
