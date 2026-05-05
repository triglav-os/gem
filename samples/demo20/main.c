/*
 * Brings up a minimal hosted GEM AES window and draws a few VDI text
 * lines into its work area. The sample is intended as the first AES
 * smoke test on Linux, with redraw handling and a simple ESC-or-close
 * event loop.
 *
 * MIT License (see: LICENSE)
 * Copyright (C) 2026 tomaz stih
 */

#include <gem/aes.h>
#include <gem/os.h>
#include <gem/vdi.h>

#include <stdio.h>

typedef struct demo20_window_state {
    WORD handle;
    WORD font_id;
    GRECT normal_rect;
    const BYTE *line1;
    const BYTE *line2;
    const BYTE *line3;
} demo20_window_state_t;

static void demo20_draw_window_content(const demo20_window_state_t *window,
                                       const GRECT *dirty_rect,
                                       WORD vdi_handle)
{
    GRECT work_rect;
    GRECT clip_rect;
    WORD text_attrib[6];
    WORD clear_rect[4];
    WORD clip[4];
    WORD previous_font;

    if (window == NULL) {
        return;
    }

    if (wind_get(window->handle, WF_CXYWH, &work_rect.g_x, &work_rect.g_y,
        &work_rect.g_w, &work_rect.g_h) == 0) {
        return;
    }

    wind_update(BEG_UPDATE);

    if (vqt_attributes(vdi_handle, text_attrib) != 0) {
        previous_font = text_attrib[0];
    } else {
        previous_font = 0;
    }

    wind_get(window->handle, WF_FIRSTXYWH, &clip_rect.g_x, &clip_rect.g_y,
        &clip_rect.g_w, &clip_rect.g_h);
    while (clip_rect.g_w > 0 && clip_rect.g_h > 0) {
        WORD clip_right = (WORD) (clip_rect.g_x + clip_rect.g_w - 1);
        WORD clip_bottom = (WORD) (clip_rect.g_y + clip_rect.g_h - 1);
        WORD x0 = clip_rect.g_x;
        WORD y0 = clip_rect.g_y;
        WORD x1 = clip_right;
        WORD y1 = clip_bottom;

        if (dirty_rect != NULL) {
            WORD dirty_right = (WORD) (dirty_rect->g_x + dirty_rect->g_w - 1);
            WORD dirty_bottom = (WORD) (dirty_rect->g_y + dirty_rect->g_h - 1);

            if (x0 < dirty_rect->g_x) {
                x0 = dirty_rect->g_x;
            }
            if (y0 < dirty_rect->g_y) {
                y0 = dirty_rect->g_y;
            }
            if (x1 > dirty_right) {
                x1 = dirty_right;
            }
            if (y1 > dirty_bottom) {
                y1 = dirty_bottom;
            }
        }

        if (x0 <= x1 && y0 <= y1) {
            clip[0] = x0;
            clip[1] = y0;
            clip[2] = x1;
            clip[3] = y1;
            vs_clip(vdi_handle, 1, clip);
            vswr_mode(vdi_handle, MD_REPLACE);
            vsf_color(vdi_handle, 1);
            clear_rect[0] = x0;
            clear_rect[1] = y0;
            clear_rect[2] = x1;
            clear_rect[3] = y1;
            v_bar(vdi_handle, clear_rect);
            vst_color(vdi_handle, 0);
            (void) vst_alignment(vdi_handle, 0, 0);
            (void) vst_font(vdi_handle, window->font_id);

            v_gtext(vdi_handle, (WORD) (work_rect.g_x + 24),
                (WORD) (work_rect.g_y + 36),
                window->line1);
            v_gtext(vdi_handle, (WORD) (work_rect.g_x + 24),
                (WORD) (work_rect.g_y + 60),
                window->line2);
            v_gtext(vdi_handle, (WORD) (work_rect.g_x + 24),
                (WORD) (work_rect.g_y + 84),
                window->line3);
        }

        wind_get(window->handle, WF_NEXTXYWH, &clip_rect.g_x, &clip_rect.g_y,
            &clip_rect.g_w, &clip_rect.g_h);
    }

    if (previous_font != 0) {
        (void) vst_font(vdi_handle, previous_font);
    }
    vs_clip(vdi_handle, 0, clip);
    v_updwk(vdi_handle);
    wind_update(END_UPDATE);
}

static demo20_window_state_t *demo20_find_window(
    demo20_window_state_t windows[2], WORD handle)
{
    if (windows[0].handle == handle) {
        return &windows[0];
    }
    if (windows[1].handle == handle) {
        return &windows[1];
    }
    return NULL;
}

int main(void)
{
    WORD appl_id;
    WORD vdi_handle;
    WORD work_in[11] = { 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 2 };
    WORD work_out[57] = { 0 };
    WORD char_w = 0;
    WORD char_h = 0;
    WORD box_w = 0;
    WORD box_h = 0;
    WORD msg[8] = { 0 };
    WORD event_flags;
    WORD mouse_x = 0;
    WORD mouse_y = 0;
    WORD mouse_buttons = 0;
    WORD key_state = 0;
    WORD key_code = 0;
    WORD button_return = 0;
    WORD open_windows = 0;
    GRECT current_rect;
    GRECT full_rect;
    demo20_window_state_t windows[2] = {
        { 0, 1, { 0, 0, 0, 0 },
            (CONST BYTE *) "Hello from GEM AES!",
            (CONST BYTE *) "This is a basic AES window on Linux.",
            (CONST BYTE *) "Close the window or press ESC to exit." },
        { 0, 2, { 0, 0, 0, 0 },
            (CONST BYTE *) "Small system font window",
            (CONST BYTE *) "Overlap me to test redraw and topping.",
            (CONST BYTE *) "This one uses the second built-in VDI font." }
    };
    demo20_window_state_t *window;

    if (!gem_os_init()) {
        fprintf(stderr, "gem_os_init() failed\n");
        return 1;
    }

    appl_id = appl_init();
    if (appl_id < 0) {
        gem_os_shutdown();
        return 1;
    }

    vdi_handle = graf_handle(&char_w, &char_h, &box_w, &box_h);
    if (vdi_handle == 0) {
        appl_exit();
        gem_os_shutdown();
        return 1;
    }

    v_opnvwk(work_in, &vdi_handle, work_out);

    windows[0].handle = wind_create(
        (UWORD) (NAME | MOVER | CLOSER | FULLER | SIZER),
        0, 0, (WORD) (work_out[0] + 1), (WORD) (work_out[1] + 1));
    windows[1].handle = wind_create(
        (UWORD) (NAME | MOVER | CLOSER | FULLER | SIZER),
        0, 0, (WORD) (work_out[0] + 1), (WORD) (work_out[1] + 1));
    if (windows[0].handle <= 0 || windows[1].handle <= 0) {
        if (windows[0].handle > 0) {
            (void) wind_delete(windows[0].handle);
        }
        if (windows[1].handle > 0) {
            (void) wind_delete(windows[1].handle);
        }
        v_clsvwk(vdi_handle);
        appl_exit();
        gem_os_shutdown();
        return 1;
    }

    (void) wind_set_str(windows[0].handle, WF_NAME, "Hello GEM on Linux");
    (void) wind_set_str(windows[1].handle, WF_NAME, "Second Font Window");
    (void) wind_open(windows[0].handle, 80, 80, 480, 240);
    (void) wind_open(windows[1].handle, 220, 180, 420, 210);
    (void) wind_get(windows[0].handle, WF_WXYWH, &windows[0].normal_rect.g_x,
        &windows[0].normal_rect.g_y, &windows[0].normal_rect.g_w,
        &windows[0].normal_rect.g_h);
    (void) wind_get(windows[1].handle, WF_WXYWH, &windows[1].normal_rect.g_x,
        &windows[1].normal_rect.g_y, &windows[1].normal_rect.g_w,
        &windows[1].normal_rect.g_h);
    full_rect.g_x = 0;
    full_rect.g_y = 0;
    full_rect.g_w = (WORD) (work_out[0] + 1);
    full_rect.g_h = (WORD) (work_out[1] + 1);
    (void) graf_mouse(M_ON, NULL);
    demo20_draw_window_content(&windows[0], NULL, vdi_handle);
    demo20_draw_window_content(&windows[1], NULL, vdi_handle);
    open_windows = 2;

    FOREVER {
        event_flags = evnt_multi((UWORD) (MU_MESAG | MU_KEYBD),
            0, 0, 0,
            0, 0, 0, 0, 0,
            0, 0, 0, 0, 0,
            msg,
            0, 0,
            &mouse_x, &mouse_y, &mouse_buttons, &key_state,
            &key_code, &button_return);

        if ((event_flags & MU_MESAG) != 0) {
            window = demo20_find_window(windows, msg[3]);
            if (window == NULL) {
                if ((event_flags & MU_KEYBD) != 0 && key_code == 0x011b) {
                    break;
                }
                continue;
            }
            if (msg[0] == WM_CLOSED) {
                wind_update(BEG_UPDATE);
                (void) wind_close(window->handle);
                (void) wind_delete(window->handle);
                window->handle = 0;
                --open_windows;
                wind_update(END_UPDATE);
                if (open_windows <= 0) {
                    break;
                }
            } else if (msg[0] == WM_REDRAW) {
                GRECT redraw_rect;

                redraw_rect.g_x = msg[4];
                redraw_rect.g_y = msg[5];
                redraw_rect.g_w = msg[6];
                redraw_rect.g_h = msg[7];
                demo20_draw_window_content(window, &redraw_rect, vdi_handle);
            } else if (msg[0] == WM_MOVED) {
                wind_update(BEG_UPDATE);
                (void) wind_set(window->handle, WF_WXYWH, msg[4], msg[5], msg[6],
                    msg[7]);
                (void) wind_get(window->handle, WF_WXYWH, &current_rect.g_x,
                    &current_rect.g_y, &current_rect.g_w, &current_rect.g_h);
                if (current_rect.g_x != full_rect.g_x ||
                    current_rect.g_y != full_rect.g_y ||
                    current_rect.g_w != full_rect.g_w ||
                    current_rect.g_h != full_rect.g_h) {
                    window->normal_rect = current_rect;
                }
                wind_update(END_UPDATE);
            } else if (msg[0] == WM_SIZED) {
                wind_update(BEG_UPDATE);
                (void) wind_set(window->handle, WF_WXYWH, msg[4], msg[5], msg[6],
                    msg[7]);
                (void) wind_get(window->handle, WF_WXYWH, &current_rect.g_x,
                    &current_rect.g_y, &current_rect.g_w, &current_rect.g_h);
                if (current_rect.g_x != full_rect.g_x ||
                    current_rect.g_y != full_rect.g_y ||
                    current_rect.g_w != full_rect.g_w ||
                    current_rect.g_h != full_rect.g_h) {
                    window->normal_rect = current_rect;
                }
                wind_update(END_UPDATE);
            } else if (msg[0] == WM_TOPPED) {
                wind_update(BEG_UPDATE);
                (void) wind_set(window->handle, WF_TOP, 0, 0, 0, 0);
                wind_update(END_UPDATE);
            } else if (msg[0] == WM_FULLED) {
                wind_update(BEG_UPDATE);
                (void) wind_get(window->handle, WF_WXYWH, &current_rect.g_x,
                    &current_rect.g_y, &current_rect.g_w, &current_rect.g_h);
                if (current_rect.g_x == full_rect.g_x &&
                    current_rect.g_y == full_rect.g_y &&
                    current_rect.g_w == full_rect.g_w &&
                    current_rect.g_h == full_rect.g_h) {
                    (void) wind_set(window->handle, WF_WXYWH,
                        window->normal_rect.g_x, window->normal_rect.g_y,
                        window->normal_rect.g_w, window->normal_rect.g_h);
                } else {
                    window->normal_rect = current_rect;
                    (void) wind_set(window->handle, WF_WXYWH, full_rect.g_x,
                        full_rect.g_y, full_rect.g_w, full_rect.g_h);
                }
                wind_update(END_UPDATE);
            }
        }

        if ((event_flags & MU_KEYBD) != 0 && key_code == 0x011b) {
            break;
        }
    }

    if (windows[0].handle > 0) {
        (void) wind_close(windows[0].handle);
        (void) wind_delete(windows[0].handle);
    }
    if (windows[1].handle > 0) {
        (void) wind_close(windows[1].handle);
        (void) wind_delete(windows[1].handle);
    }
    v_clsvwk(vdi_handle);
    appl_exit();
    gem_os_shutdown();
    return 0;
}
