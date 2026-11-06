/*
 * Implements the hosted GEM calculator's user interface: the object tree
 * of keys and display, redraw and button flashing, and the event loop that
 * feeds clicks and keystrokes to the arithmetic engine in calc_engine.c.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (C) 2026 tomaz stih
 */

#include "calc_private.h"
#include <gem/os.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void calc_init_object(OBJECT *object, UWORD type, UWORD flags,
                             UWORD state, LONG spec, WORD x, WORD y, WORD width,
                             WORD height)
{
    if (object == NULL) {
        return;
    }

    object->ob_next = NIL;
    object->ob_head = NIL;
    object->ob_tail = NIL;
    object->ob_type = type;
    object->ob_flags = flags;
    object->ob_state = state;
    object->ob_spec = spec;
    object->ob_x = x;
    object->ob_y = y;
    object->ob_width = width;
    object->ob_height = height;
}

static void calc_button(OBJECT *object, const char *label, WORD x, WORD y,
                        WORD width, WORD height)
{
    calc_init_object(object, G_BUTTON, SELECTABLE | EXIT, NORMAL,
                     (LONG)(intptr_t)label, x, y, width, height);
}

static void calc_sync_root_rect(calc_state_t *state)
{
    GRECT work_rect;

    if (state == NULL) {
        return;
    }

    if (wind_get(state->handle, WF_CXYWH, &work_rect.g_x, &work_rect.g_y,
                 &work_rect.g_w, &work_rect.g_h) == 0) {
        return;
    }

    state->tree[calc_root].ob_x = work_rect.g_x;
    state->tree[calc_root].ob_y = work_rect.g_y;
    state->tree[calc_root].ob_width = work_rect.g_w;
    state->tree[calc_root].ob_height = work_rect.g_h;
}

static void calc_draw(calc_state_t *state, const GRECT *dirty_rect)
{
    GRECT clip_rect;

    if (state == NULL) {
        return;
    }

    calc_sync_root_rect(state);
    wind_update(BEG_UPDATE);
    wind_get(state->handle, WF_FIRSTXYWH, &clip_rect.g_x, &clip_rect.g_y,
             &clip_rect.g_w, &clip_rect.g_h);
    while (clip_rect.g_w > 0 && clip_rect.g_h > 0) {
        WORD x0 = clip_rect.g_x;
        WORD y0 = clip_rect.g_y;
        WORD x1 = (WORD)(clip_rect.g_x + clip_rect.g_w - 1);
        WORD y1 = (WORD)(clip_rect.g_y + clip_rect.g_h - 1);

        if (dirty_rect != NULL) {
            WORD dirty_x1 = (WORD)(dirty_rect->g_x + dirty_rect->g_w - 1);
            WORD dirty_y1 = (WORD)(dirty_rect->g_y + dirty_rect->g_h - 1);

            if (x0 < dirty_rect->g_x) {
                x0 = dirty_rect->g_x;
            }
            if (y0 < dirty_rect->g_y) {
                y0 = dirty_rect->g_y;
            }
            if (x1 > dirty_x1) {
                x1 = dirty_x1;
            }
            if (y1 > dirty_y1) {
                y1 = dirty_y1;
            }
        }

        if (x0 <= x1 && y0 <= y1) {
            WORD fill[4] = {x0, y0, x1, y1};
            WORD handle = graf_handle(NULL, NULL, NULL, NULL);

            (void)vswr_mode(handle, MD_REPLACE);
            vsf_color(handle, BLACK);
            v_bar(handle, fill);
            objc_draw(state->tree, ROOT, MAX_DEPTH, x0, y0, (WORD)(x1 - x0 + 1),
                      (WORD)(y1 - y0 + 1));
        }

        wind_get(state->handle, WF_NEXTXYWH, &clip_rect.g_x, &clip_rect.g_y,
                 &clip_rect.g_w, &clip_rect.g_h);
    }
    wind_update(END_UPDATE);
}

void calc_redraw_object(calc_state_t *state, WORD object)
{
    WORD x;
    WORD y;
    GRECT dirty_rect;

    if (state == NULL || object < 0 || object >= calc_object_count) {
        return;
    }

    objc_offset(state->tree, object, &x, &y);
    dirty_rect.g_x = x;
    dirty_rect.g_y = y;
    dirty_rect.g_w = state->tree[object].ob_width;
    dirty_rect.g_h = state->tree[object].ob_height;
    calc_draw(state, &dirty_rect);
}

void calc_right_justify(calc_state_t *state, const char *text)
{
    size_t len;
    size_t pad;

    if (state == NULL || text == NULL) {
        return;
    }

    len = strlen(text);
    if (len > 14u) {
        len = 14u;
        text += strlen(text) - len;
    }

    pad = 14u - len;
    memset(state->display_text, ' ', 14u);
    memcpy(state->display_text + pad, text, len);
    state->display_text[14] = '\0';
}

static int calc_flash_button(calc_state_t *state, WORD object)
{
    WORD x;
    WORD y;
    WORD pressed_state;

    if (state == NULL || object < 0 || object >= calc_object_count) {
        return 0;
    }

    objc_offset(state->tree, object, &x, &y);
    pressed_state = (WORD)(state->tree[object].ob_state | SELECTED);
    (void)objc_change(state->tree, object, 0, x, y,
                      state->tree[object].ob_width,
                      state->tree[object].ob_height, pressed_state, 1);
    gem_os_sleep_ms(90u);
    state->tree[object].ob_state &= (UWORD)~SELECTED;
    (void)objc_change(
        state->tree, object, 0, x, y, state->tree[object].ob_width,
        state->tree[object].ob_height, state->tree[object].ob_state, 1);
    return 1;
}

static void calc_init_tree(calc_state_t *state)
{
    WORD row_y;
    WORD col_0;
    WORD col_1;
    WORD col_2;
    WORD col_3;
    const WORD button_w = 52;
    const WORD button_h = 22;
    const WORD gap = 8;
    const WORD left = 16;
    const WORD top = 16;
    if (state == NULL) {
        return;
    }

    memset(state, 0, sizeof(*state));
    memset(&state->display_ted, 0, sizeof(state->display_ted));

    state->display_ted.te_ptext = (LONG)(intptr_t)state->display_text;
    state->display_ted.te_font = IBM;
    state->display_ted.te_just = TE_RIGHT;
    state->display_ted.te_color = 0x1100;
    state->display_ted.te_txtlen = 15;
    state->display_ted.te_tmplen = 15;

    calc_init_object(&state->tree[calc_root], G_IBOX, NONE, NORMAL, 0L, 0, 0,
                     calc_work_width, calc_work_height);
    calc_init_object(&state->tree[calc_display_box], G_BOXTEXT, NONE, NORMAL,
                     (LONG)(intptr_t)&state->display_ted, left, top, 232, 22);
    calc_init_object(&state->tree[calc_memory_label], G_STRING, NONE, NORMAL,
                     (LONG)(intptr_t) "", left, top + 7, 8, 8);

    col_0 = left;
    col_1 = (WORD)(col_0 + button_w + gap);
    col_2 = (WORD)(col_1 + button_w + gap);
    col_3 = (WORD)(col_2 + button_w + gap);
    row_y = (WORD)(top + 36);

    calc_button(&state->tree[calc_ce], "CE", col_0, row_y, button_w, button_h);
    calc_button(&state->tree[calc_clear], "C", col_1, row_y, button_w,
                button_h);
    calc_button(&state->tree[calc_mem_clear], "MC", col_2, row_y, button_w,
                button_h);
    calc_button(&state->tree[calc_mem_recall], "MR", col_3, row_y, button_w,
                button_h);

    row_y = (WORD)(row_y + button_h + gap);
    calc_button(&state->tree[calc_mem_plus], "M+", col_0, row_y, button_w,
                button_h);
    calc_button(&state->tree[calc_mem_minus], "M-", col_1, row_y, button_w,
                button_h);
    calc_button(&state->tree[calc_sign], "+/-", col_2, row_y, button_w,
                button_h);
    calc_button(&state->tree[calc_percent_button], "%", col_3, row_y, button_w,
                button_h);

    row_y = (WORD)(row_y + button_h + gap);
    calc_button(&state->tree[calc_digit_7], "7", col_0, row_y, button_w,
                button_h);
    calc_button(&state->tree[calc_digit_8], "8", col_1, row_y, button_w,
                button_h);
    calc_button(&state->tree[calc_digit_9], "9", col_2, row_y, button_w,
                button_h);
    calc_button(&state->tree[calc_divide], "/", col_3, row_y, button_w,
                button_h);

    row_y = (WORD)(row_y + button_h + gap);
    calc_button(&state->tree[calc_digit_4], "4", col_0, row_y, button_w,
                button_h);
    calc_button(&state->tree[calc_digit_5], "5", col_1, row_y, button_w,
                button_h);
    calc_button(&state->tree[calc_digit_6], "6", col_2, row_y, button_w,
                button_h);
    calc_button(&state->tree[calc_multiply], "*", col_3, row_y, button_w,
                button_h);

    row_y = (WORD)(row_y + button_h + gap);
    calc_button(&state->tree[calc_digit_1], "1", col_0, row_y, button_w,
                button_h);
    calc_button(&state->tree[calc_digit_2], "2", col_1, row_y, button_w,
                button_h);
    calc_button(&state->tree[calc_digit_3], "3", col_2, row_y, button_w,
                button_h);
    calc_button(&state->tree[calc_minus], "-", col_3, row_y, button_w,
                button_h);

    row_y = (WORD)(row_y + button_h + gap);
    calc_button(&state->tree[calc_digit_0], "0", col_0, row_y, button_w,
                button_h);
    calc_button(&state->tree[calc_point], ".", col_1, row_y, button_w,
                button_h);
    calc_button(&state->tree[calc_equals_button], "=", col_2, row_y, button_w,
                button_h);
    state->tree[calc_equals_button].ob_flags |= DEFAULT;
    calc_button(&state->tree[calc_plus], "+", col_3, row_y, button_w, button_h);
    /* LASTOB marks the final array entry, independently of sibling order. */
    state->tree[calc_object_count - 1].ob_flags |= LASTOB;

    objc_add(state->tree, calc_root, calc_display_box);
    objc_add(state->tree, calc_root, calc_memory_label);
    objc_add(state->tree, calc_root, calc_ce);
    objc_add(state->tree, calc_root, calc_clear);
    objc_add(state->tree, calc_root, calc_mem_clear);
    objc_add(state->tree, calc_root, calc_mem_recall);
    objc_add(state->tree, calc_root, calc_mem_plus);
    objc_add(state->tree, calc_root, calc_mem_minus);
    objc_add(state->tree, calc_root, calc_sign);
    objc_add(state->tree, calc_root, calc_percent_button);
    objc_add(state->tree, calc_root, calc_divide);
    objc_add(state->tree, calc_root, calc_multiply);
    objc_add(state->tree, calc_root, calc_digit_7);
    objc_add(state->tree, calc_root, calc_digit_8);
    objc_add(state->tree, calc_root, calc_digit_9);
    objc_add(state->tree, calc_root, calc_minus);
    objc_add(state->tree, calc_root, calc_digit_4);
    objc_add(state->tree, calc_root, calc_digit_5);
    objc_add(state->tree, calc_root, calc_digit_6);
    objc_add(state->tree, calc_root, calc_plus);
    objc_add(state->tree, calc_root, calc_digit_1);
    objc_add(state->tree, calc_root, calc_digit_2);
    objc_add(state->tree, calc_root, calc_digit_3);
    objc_add(state->tree, calc_root, calc_equals_button);
    objc_add(state->tree, calc_root, calc_digit_0);
    objc_add(state->tree, calc_root, calc_point);

    calc_clear_all(state);
    calc_update_display(state);
}

int calc_main(void)
{
    const UWORD window_kind = (UWORD)(NAME | MOVER | CLOSER);
    WORD appl_id;
    WORD msg[8] = {0};
    WORD event_flags;
    WORD mouse_x = 0;
    WORD mouse_y = 0;
    WORD mouse_buttons = 0;
    WORD key_state = 0;
    WORD key_code = 0;
    WORD button_return = 0;
    WORD object;
    WORD outer_x;
    WORD outer_y;
    WORD outer_w;
    WORD outer_h;
    GRECT current_rect;
    GRECT full_rect;
    calc_state_t state;

    if (!gem_os_init()) {
        fprintf(stderr, "gem_os_init() failed\n");
        return 1;
    }

    appl_id = appl_init();
    if (appl_id < 0) {
        gem_os_shutdown();
        return 1;
    }

    calc_init_tree(&state);
    state.handle = wind_create(window_kind, 0, 0, 640, 400);
    if (state.handle <= 0) {
        appl_exit();
        gem_os_shutdown();
        return 1;
    }

    (void)wind_set_str(state.handle, WF_NAME, "Calculator");
    (void)wind_calc(WC_BORDER, window_kind, 100, 60, calc_work_width,
                    calc_work_height, &outer_x, &outer_y, &outer_w, &outer_h);
    (void)wind_open(state.handle, outer_x, outer_y, outer_w, outer_h);
    (void)wind_get(state.handle, WF_WXYWH, &state.normal_rect.g_x,
                   &state.normal_rect.g_y, &state.normal_rect.g_w,
                   &state.normal_rect.g_h);
    wind_get(0, WF_WXYWH, &full_rect.g_x, &full_rect.g_y, &full_rect.g_w,
             &full_rect.g_h);
    (void)graf_mouse(M_ON, NULL);
    calc_draw(&state, NULL);

    FOREVER
    {
        event_flags =
            evnt_multi((UWORD)(MU_MESAG | MU_BUTTON | MU_KEYBD), 1, 1, 1, 0, 0,
                       0, 0, 0, 0, 0, 0, 0, 0, msg, 0, 0, &mouse_x, &mouse_y,
                       &mouse_buttons, &key_state, &key_code, &button_return);

        if ((event_flags & MU_MESAG) != 0) {
            if (msg[3] != state.handle) {
            } else {
                if (msg[0] == WM_CLOSED) {
                    break;
                } else if (msg[0] == WM_REDRAW) {
                    GRECT redraw_rect;

                    redraw_rect.g_x = msg[4];
                    redraw_rect.g_y = msg[5];
                    redraw_rect.g_w = msg[6];
                    redraw_rect.g_h = msg[7];
                    calc_draw(&state, &redraw_rect);
                } else if (msg[0] == WM_MOVED || msg[0] == WM_SIZED) {
                    wind_update(BEG_UPDATE);
                    (void)wind_set(state.handle, WF_WXYWH, msg[4], msg[5],
                                   msg[6], msg[7]);
                    (void)wind_get(state.handle, WF_WXYWH, &current_rect.g_x,
                                   &current_rect.g_y, &current_rect.g_w,
                                   &current_rect.g_h);
                    if (current_rect.g_x != full_rect.g_x ||
                        current_rect.g_y != full_rect.g_y ||
                        current_rect.g_w != full_rect.g_w ||
                        current_rect.g_h != full_rect.g_h) {
                        state.normal_rect = current_rect;
                    }
                    wind_update(END_UPDATE);
                } else if (msg[0] == WM_TOPPED) {
                    wind_update(BEG_UPDATE);
                    (void)wind_set(state.handle, WF_TOP, 0, 0, 0, 0);
                    wind_update(END_UPDATE);
                } else if (msg[0] == WM_FULLED) {
                    wind_update(BEG_UPDATE);
                    (void)wind_get(state.handle, WF_WXYWH, &current_rect.g_x,
                                   &current_rect.g_y, &current_rect.g_w,
                                   &current_rect.g_h);
                    if (current_rect.g_x == full_rect.g_x &&
                        current_rect.g_y == full_rect.g_y &&
                        current_rect.g_w == full_rect.g_w &&
                        current_rect.g_h == full_rect.g_h) {
                        (void)wind_set(
                            state.handle, WF_WXYWH, state.normal_rect.g_x,
                            state.normal_rect.g_y, state.normal_rect.g_w,
                            state.normal_rect.g_h);
                    } else {
                        state.normal_rect = current_rect;
                        (void)wind_set(state.handle, WF_WXYWH, full_rect.g_x,
                                       full_rect.g_y, full_rect.g_w,
                                       full_rect.g_h);
                    }
                    wind_update(END_UPDATE);
                }
            }
        }

        if ((event_flags & MU_BUTTON) != 0 &&
            wind_find(mouse_x, mouse_y) == state.handle) {
            object = objc_find(state.tree, ROOT, MAX_DEPTH, mouse_x, mouse_y);
            if (object >= calc_ce && object < calc_object_count) {
                calc_flash_button(&state, object);
                calc_handle_button(&state, object);
            }
        }

        if ((event_flags & MU_KEYBD) != 0) {
            if ((key_code & 0x00ff) == 27) {
                break;
            }
            object = calc_map_key(key_code);
            if (object >= calc_ce && object < calc_object_count) {
                calc_flash_button(&state, object);
                calc_handle_button(&state, object);
            }
        }
    }

    if (state.handle > 0) {
        (void)wind_close(state.handle);
        (void)wind_delete(state.handle);
    }
    appl_exit();
    gem_os_shutdown();
    return 0;
}
