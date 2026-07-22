/*
 * Implements the hosted GEM calculator UI, redraw, and arithmetic
 * logic. The file keeps the current working hosted behavior in one
 * place so the legacy accessory-era sources are no longer needed.
 *
 * MIT License (see: LICENSE)
 * Copyright (C) 2026 tomaz stih
 */

#include <gem/aes.h>
#include <gem/portab.h>
#include <gem/vdi.h>

#include "calc.h"
#include "platform/os.h"

#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum {
    calc_root = 0,
    calc_display_box,
    calc_memory_label,
    calc_ce,
    calc_clear,
    calc_mem_clear,
    calc_mem_recall,
    calc_mem_plus,
    calc_mem_minus,
    calc_sign,
    calc_percent_button,
    calc_divide,
    calc_multiply,
    calc_digit_7,
    calc_digit_8,
    calc_digit_9,
    calc_minus,
    calc_digit_4,
    calc_digit_5,
    calc_digit_6,
    calc_plus,
    calc_digit_1,
    calc_digit_2,
    calc_digit_3,
    calc_equals_button,
    calc_digit_0,
    calc_point,
    calc_object_count
};

typedef struct calc_state {
    WORD handle;
    GRECT normal_rect;
    OBJECT tree[calc_object_count];
    TEDINFO display_ted;
    char display_text[15];
    char input_text[32];
    double accumulator;
    double memory_value;
    char pending_op;
    int entering_value;
    int error_state;
    int memory_set;
} calc_state_t;

enum {
    calc_work_width = 264,
    calc_work_height = 238
};

enum {
    calc_menu_title_file = 3,
    calc_menu_item_quit = 6
};

static OBJECT calc_menu_tree[] = {
    {-1, 1, 4, G_IBOX, NONE, NORMAL, 0L, 0, 0, 0, 0},
    {4, 2, 2, G_BOX, NONE, NORMAL, 0x1100L, 0, 0, 80, 1},
    {1, 3, 3, G_IBOX, NONE, NORMAL, 0L, 0, 0, 80, 1},
    {2, -1, -1, G_TITLE, NONE, NORMAL, (LONG) " File ", 0, 0, 6, 1},
    {0, 5, 5, G_IBOX, NONE, NORMAL, 0L, 0, 0, 0, 0},
    {4, 6, 6, G_BOX, NONE, NORMAL, 0x1100L, 0, 0, 20, 1},
    {5, -1, -1, G_STRING, LASTOB, NORMAL, (LONG) "  Quit \t^Q", 0, 0, 20, 1}
};

static void calc_init_object(OBJECT *object,
                             UWORD type,
                             UWORD flags,
                             UWORD state,
                             LONG spec,
                             WORD x,
                             WORD y,
                             WORD width,
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

static void calc_button(OBJECT *object,
                        const char *label,
                        WORD x,
                        WORD y,
                        WORD width,
                        WORD height)
{
    calc_init_object(object, G_BUTTON, SELECTABLE | EXIT, NORMAL,
        (LONG) (intptr_t) label, x, y, width, height);
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
        WORD x1 = (WORD) (clip_rect.g_x + clip_rect.g_w - 1);
        WORD y1 = (WORD) (clip_rect.g_y + clip_rect.g_h - 1);

        if (dirty_rect != NULL) {
            WORD dirty_x1 = (WORD) (dirty_rect->g_x + dirty_rect->g_w - 1);
            WORD dirty_y1 = (WORD) (dirty_rect->g_y + dirty_rect->g_h - 1);

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
            WORD fill[4] = { x0, y0, x1, y1 };
            WORD handle = graf_handle(NULL, NULL, NULL, NULL);

            (void) vswr_mode(handle, MD_REPLACE);
            vsf_color(handle, BLACK);
            v_bar(handle, fill);
            objc_draw(state->tree, ROOT, MAX_DEPTH, x0, y0,
                (WORD) (x1 - x0 + 1), (WORD) (y1 - y0 + 1));
        }

        wind_get(state->handle, WF_NEXTXYWH, &clip_rect.g_x, &clip_rect.g_y,
            &clip_rect.g_w, &clip_rect.g_h);
    }
    wind_update(END_UPDATE);
}

static void calc_redraw_object(calc_state_t *state, WORD object)
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

static void calc_right_justify(calc_state_t *state, const char *text)
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

static void calc_set_error(calc_state_t *state)
{
    if (state == NULL) {
        return;
    }

    state->error_state = 1;
    state->entering_value = 0;
    strcpy(state->input_text, "0");
    calc_right_justify(state, "error");
}

static int calc_format_value(double value, char *buffer, size_t size)
{
    int rc;

    if (buffer == NULL || size == 0u) {
        return 0;
    }
    if (!isfinite(value)) {
        return 0;
    }

    rc = snprintf(buffer, size, "%.13g", value);
    if (rc < 0 || (size_t) rc >= size) {
        return 0;
    }
    if ((size_t) rc > 14u) {
        return 0;
    }
    return 1;
}

static double calc_current_value(const calc_state_t *state)
{
    if (state == NULL) {
        return 0.0;
    }
    return strtod(state->input_text, NULL);
}

static void calc_update_display(calc_state_t *state)
{
    if (state == NULL) {
        return;
    }

    if (state->error_state) {
        calc_right_justify(state, "error");
    } else {
        calc_right_justify(state, state->input_text);
    }

    state->tree[calc_memory_label].ob_spec = state->memory_set ?
        (LONG) (intptr_t) "M" : (LONG) (intptr_t) "";
    calc_redraw_object(state, calc_display_box);
    calc_redraw_object(state, calc_memory_label);
}

static void calc_set_input_from_value(calc_state_t *state, double value)
{
    char buffer[32];

    if (state == NULL) {
        return;
    }

    if (!calc_format_value(value, buffer, sizeof(buffer))) {
        calc_set_error(state);
        return;
    }

    strcpy(state->input_text, buffer);
    state->error_state = 0;
}

static int calc_apply_pending(calc_state_t *state, double operand)
{
    if (state == NULL) {
        return 0;
    }

    switch (state->pending_op) {
    case '+':
        state->accumulator += operand;
        break;
    case '-':
        state->accumulator -= operand;
        break;
    case '*':
        state->accumulator *= operand;
        break;
    case '/':
        if (operand == 0.0) {
            calc_set_error(state);
            return 0;
        }
        state->accumulator /= operand;
        break;
    default:
        state->accumulator = operand;
        break;
    }

    if (!isfinite(state->accumulator)) {
        calc_set_error(state);
        return 0;
    }
    return 1;
}

static void calc_clear_all(calc_state_t *state)
{
    if (state == NULL) {
        return;
    }

    state->accumulator = 0.0;
    state->pending_op = '\0';
    state->entering_value = 0;
    state->error_state = 0;
    strcpy(state->input_text, "0");
}

static void calc_clear_entry(calc_state_t *state)
{
    if (state == NULL) {
        return;
    }

    state->error_state = 0;
    strcpy(state->input_text, "0");
    state->entering_value = 0;
}

static void calc_append_char(calc_state_t *state, char ch)
{
    size_t len;

    if (state == NULL) {
        return;
    }

    if (state->error_state || !state->entering_value) {
        state->error_state = 0;
        state->input_text[0] = '\0';
        state->entering_value = 1;
    }

    len = strlen(state->input_text);
    if (ch == '.') {
        if (strchr(state->input_text, '.') != NULL) {
            return;
        }
        if (len == 0u) {
            strcpy(state->input_text, "0.");
            return;
        }
    } else if (len == 1u && state->input_text[0] == '0') {
        state->input_text[0] = '\0';
        len = 0u;
    } else if (len == 2u && strcmp(state->input_text, "-0") == 0) {
        state->input_text[1] = '\0';
        len = 1u;
    }

    if (len + 1u >= sizeof(state->input_text)) {
        return;
    }

    state->input_text[len] = ch;
    state->input_text[len + 1u] = '\0';
}

static void calc_toggle_sign(calc_state_t *state)
{
    size_t len;

    if (state == NULL || state->error_state) {
        return;
    }

    if (!state->entering_value) {
        state->entering_value = 1;
    }

    if (strcmp(state->input_text, "0") == 0) {
        strcpy(state->input_text, "-0");
        return;
    }
    if (strcmp(state->input_text, "-0") == 0) {
        strcpy(state->input_text, "0");
        return;
    }
    if (state->input_text[0] == '-') {
        memmove(state->input_text, state->input_text + 1u,
            strlen(state->input_text));
        return;
    }

    len = strlen(state->input_text);
    if (len + 1u >= sizeof(state->input_text)) {
        return;
    }
    memmove(state->input_text + 1u, state->input_text, len + 1u);
    state->input_text[0] = '-';
}

static void calc_percent(calc_state_t *state)
{
    double value;

    if (state == NULL || state->error_state) {
        return;
    }

    value = calc_current_value(state);
    if (state->pending_op == '+' || state->pending_op == '-') {
        value = state->accumulator * value / 100.0;
    } else {
        value /= 100.0;
    }

    calc_set_input_from_value(state, value);
    state->entering_value = 0;
}

static void calc_operator(calc_state_t *state, char op)
{
    double value;

    if (state == NULL) {
        return;
    }

    if (state->error_state) {
        calc_clear_all(state);
    }

    value = calc_current_value(state);
    if (state->pending_op == '\0') {
        state->accumulator = value;
    } else if (!calc_apply_pending(state, value)) {
        return;
    }

    state->pending_op = op;
    state->entering_value = 0;
    calc_set_input_from_value(state, state->accumulator);
}

static void calc_equals(calc_state_t *state)
{
    double value;

    if (state == NULL || state->pending_op == '\0') {
        return;
    }
    if (state->error_state) {
        return;
    }

    value = calc_current_value(state);
    if (!calc_apply_pending(state, value)) {
        return;
    }

    state->pending_op = '\0';
    state->entering_value = 0;
    calc_set_input_from_value(state, state->accumulator);
}

static void calc_memory_store(calc_state_t *state, double value)
{
    if (state == NULL) {
        return;
    }

    state->memory_value = value;
    state->memory_set = (value != 0.0);
}

static void calc_handle_button(calc_state_t *state, WORD object)
{
    double value;

    if (state == NULL) {
        return;
    }

    switch (object) {
    case calc_digit_0:
        calc_append_char(state, '0');
        break;
    case calc_digit_1:
        calc_append_char(state, '1');
        break;
    case calc_digit_2:
        calc_append_char(state, '2');
        break;
    case calc_digit_3:
        calc_append_char(state, '3');
        break;
    case calc_digit_4:
        calc_append_char(state, '4');
        break;
    case calc_digit_5:
        calc_append_char(state, '5');
        break;
    case calc_digit_6:
        calc_append_char(state, '6');
        break;
    case calc_digit_7:
        calc_append_char(state, '7');
        break;
    case calc_digit_8:
        calc_append_char(state, '8');
        break;
    case calc_digit_9:
        calc_append_char(state, '9');
        break;
    case calc_point:
        calc_append_char(state, '.');
        break;
    case calc_sign:
        calc_toggle_sign(state);
        break;
    case calc_percent_button:
        calc_percent(state);
        break;
    case calc_plus:
        calc_operator(state, '+');
        break;
    case calc_minus:
        calc_operator(state, '-');
        break;
    case calc_multiply:
        calc_operator(state, '*');
        break;
    case calc_divide:
        calc_operator(state, '/');
        break;
    case calc_equals_button:
        calc_equals(state);
        break;
    case calc_ce:
        calc_clear_entry(state);
        break;
    case calc_clear:
        calc_clear_all(state);
        break;
    case calc_mem_clear:
        calc_memory_store(state, 0.0);
        break;
    case calc_mem_recall:
        if (state->memory_set) {
            calc_set_input_from_value(state, state->memory_value);
            state->entering_value = 0;
        }
        break;
    case calc_mem_plus:
        value = calc_current_value(state);
        calc_memory_store(state, state->memory_value + value);
        break;
    case calc_mem_minus:
        value = calc_current_value(state);
        calc_memory_store(state, state->memory_value - value);
        break;
    default:
        break;
    }

    calc_update_display(state);
}

static WORD calc_map_key(WORD key_code)
{
    WORD ch;
    UWORD scan;

    ch = (WORD) (key_code & 0x00ff);
    scan = (UWORD) ((UWORD) key_code >> 8);
    switch (ch) {
    case '0':
        return calc_digit_0;
    case '1':
        return calc_digit_1;
    case '2':
        return calc_digit_2;
    case '3':
        return calc_digit_3;
    case '4':
        return calc_digit_4;
    case '5':
        return calc_digit_5;
    case '6':
        return calc_digit_6;
    case '7':
        return calc_digit_7;
    case '8':
        return calc_digit_8;
    case '9':
        return calc_digit_9;
    case '.':
        return calc_point;
    case '+':
        return calc_plus;
    case '-':
        return calc_minus;
    case '*':
        return calc_multiply;
    case '/':
        return calc_divide;
    case '%':
        return calc_percent_button;
    case '=':
    case '\r':
        return calc_equals_button;
    case 'c':
    case 'C':
        return calc_clear;
    case 'e':
    case 'E':
        return calc_ce;
    case 'm':
    case 'M':
        return calc_mem_recall;
    case 92:
        return calc_sign;
    default:
        break;
    }

    switch (scan) {
    case 84u:
        return calc_divide;
    case 85u:
        return calc_multiply;
    case 86u:
        return calc_minus;
    case 87u:
        return calc_plus;
    case 88u:
        return calc_equals_button;
    case 89u:
        return calc_digit_1;
    case 90u:
        return calc_digit_2;
    case 91u:
        return calc_digit_3;
    case 92u:
        return calc_digit_4;
    case 93u:
        return calc_digit_5;
    case 94u:
        return calc_digit_6;
    case 95u:
        return calc_digit_7;
    case 96u:
        return calc_digit_8;
    case 97u:
        return calc_digit_9;
    case 98u:
        return calc_digit_0;
    case 99u:
        return calc_point;
    default:
        return -1;
    }
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
    pressed_state = (WORD) (state->tree[object].ob_state | SELECTED);
    (void) objc_change(state->tree, object, 0, x, y,
        state->tree[object].ob_width, state->tree[object].ob_height,
        pressed_state, 1);
    gem_os_sleep_ms(90u);
    state->tree[object].ob_state &= (UWORD) ~SELECTED;
    (void) objc_change(state->tree, object, 0, x, y,
        state->tree[object].ob_width, state->tree[object].ob_height,
        state->tree[object].ob_state, 1);
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

    state->display_ted.te_ptext = (LONG) (intptr_t) state->display_text;
    state->display_ted.te_font = IBM;
    state->display_ted.te_just = TE_RIGHT;
    state->display_ted.te_color = 0x1100;
    state->display_ted.te_txtlen = 15;
    state->display_ted.te_tmplen = 15;

    calc_init_object(&state->tree[calc_root], G_IBOX, LASTOB, NORMAL, 0L,
        0, 0, calc_work_width, calc_work_height);
    calc_init_object(&state->tree[calc_display_box], G_BOXTEXT, NONE,
        NORMAL, (LONG) (intptr_t) &state->display_ted,
        left, top, 232, 22);
    calc_init_object(&state->tree[calc_memory_label], G_STRING, NONE,
        NORMAL, (LONG) (intptr_t) "", left, top + 7, 8, 8);

    col_0 = left;
    col_1 = (WORD) (col_0 + button_w + gap);
    col_2 = (WORD) (col_1 + button_w + gap);
    col_3 = (WORD) (col_2 + button_w + gap);
    row_y = (WORD) (top + 36);

    calc_button(&state->tree[calc_ce], "CE", col_0, row_y, button_w, button_h);
    calc_button(&state->tree[calc_clear], "C", col_1, row_y, button_w, button_h);
    calc_button(&state->tree[calc_mem_clear], "MC", col_2, row_y, button_w,
        button_h);
    calc_button(&state->tree[calc_mem_recall], "MR", col_3, row_y, button_w,
        button_h);

    row_y = (WORD) (row_y + button_h + gap);
    calc_button(&state->tree[calc_mem_plus], "M+", col_0, row_y, button_w,
        button_h);
    calc_button(&state->tree[calc_mem_minus], "M-", col_1, row_y, button_w,
        button_h);
    calc_button(&state->tree[calc_sign], "+/-", col_2, row_y, button_w,
        button_h);
    calc_button(&state->tree[calc_percent_button], "%", col_3, row_y, button_w,
        button_h);

    row_y = (WORD) (row_y + button_h + gap);
    calc_button(&state->tree[calc_digit_7], "7", col_0, row_y, button_w,
        button_h);
    calc_button(&state->tree[calc_digit_8], "8", col_1, row_y, button_w,
        button_h);
    calc_button(&state->tree[calc_digit_9], "9", col_2, row_y, button_w,
        button_h);
    calc_button(&state->tree[calc_divide], "/", col_3, row_y, button_w,
        button_h);

    row_y = (WORD) (row_y + button_h + gap);
    calc_button(&state->tree[calc_digit_4], "4", col_0, row_y, button_w,
        button_h);
    calc_button(&state->tree[calc_digit_5], "5", col_1, row_y, button_w,
        button_h);
    calc_button(&state->tree[calc_digit_6], "6", col_2, row_y, button_w,
        button_h);
    calc_button(&state->tree[calc_multiply], "*", col_3, row_y, button_w,
        button_h);

    row_y = (WORD) (row_y + button_h + gap);
    calc_button(&state->tree[calc_digit_1], "1", col_0, row_y, button_w,
        button_h);
    calc_button(&state->tree[calc_digit_2], "2", col_1, row_y, button_w,
        button_h);
    calc_button(&state->tree[calc_digit_3], "3", col_2, row_y, button_w,
        button_h);
    calc_button(&state->tree[calc_minus], "-", col_3, row_y, button_w,
        button_h);

    row_y = (WORD) (row_y + button_h + gap);
    calc_button(&state->tree[calc_digit_0], "0", col_0, row_y, button_w,
        button_h);
    calc_button(&state->tree[calc_point], ".", col_1, row_y, button_w,
        button_h);
    calc_button(&state->tree[calc_equals_button], "=", col_2, row_y, button_w,
        button_h);
    state->tree[calc_equals_button].ob_flags |= DEFAULT;
    calc_button(&state->tree[calc_plus], "+", col_3, row_y, button_w,
        button_h);

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
    const UWORD window_kind = (UWORD) (NAME | MOVER | CLOSER);
    WORD appl_id;
    WORD msg[8] = { 0 };
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

    (void) wind_set_str(state.handle, WF_NAME, "Calculator");
    (void) wind_calc(WC_BORDER, window_kind,
        100, 60, calc_work_width, calc_work_height,
        &outer_x, &outer_y, &outer_w, &outer_h);
    (void) wind_open(state.handle, outer_x, outer_y, outer_w, outer_h);
    (void) wind_get(state.handle, WF_WXYWH, &state.normal_rect.g_x,
        &state.normal_rect.g_y, &state.normal_rect.g_w,
        &state.normal_rect.g_h);
    wind_get(0, WF_WXYWH, &full_rect.g_x, &full_rect.g_y, &full_rect.g_w,
        &full_rect.g_h);
    (void) graf_mouse(M_ON, NULL);
    (void) menu_bar(calc_menu_tree, 1);
    calc_draw(&state, NULL);

    FOREVER {
        event_flags = evnt_multi((UWORD) (MU_MESAG | MU_BUTTON | MU_KEYBD),
            1, 1, 1,
            0, 0, 0, 0, 0,
            0, 0, 0, 0, 0,
            msg,
            0, 0,
            &mouse_x, &mouse_y, &mouse_buttons, &key_state,
            &key_code, &button_return);

        if ((event_flags & MU_MESAG) != 0) {
            if (msg[0] == MN_SELECTED) {
                if (msg[3] == calc_menu_title_file &&
                    msg[4] == calc_menu_item_quit) {
                    break;
                }
            } else if (msg[3] != state.handle) {
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
                    (void) wind_set(state.handle, WF_WXYWH, msg[4], msg[5],
                        msg[6], msg[7]);
                    (void) wind_get(state.handle, WF_WXYWH,
                        &current_rect.g_x, &current_rect.g_y,
                        &current_rect.g_w, &current_rect.g_h);
                    if (current_rect.g_x != full_rect.g_x ||
                        current_rect.g_y != full_rect.g_y ||
                        current_rect.g_w != full_rect.g_w ||
                        current_rect.g_h != full_rect.g_h) {
                        state.normal_rect = current_rect;
                    }
                    wind_update(END_UPDATE);
                } else if (msg[0] == WM_TOPPED) {
                    wind_update(BEG_UPDATE);
                    (void) wind_set(state.handle, WF_TOP, 0, 0, 0, 0);
                    wind_update(END_UPDATE);
                } else if (msg[0] == WM_FULLED) {
                    wind_update(BEG_UPDATE);
                    (void) wind_get(state.handle, WF_WXYWH,
                        &current_rect.g_x, &current_rect.g_y,
                        &current_rect.g_w, &current_rect.g_h);
                    if (current_rect.g_x == full_rect.g_x &&
                        current_rect.g_y == full_rect.g_y &&
                        current_rect.g_w == full_rect.g_w &&
                        current_rect.g_h == full_rect.g_h) {
                        (void) wind_set(state.handle, WF_WXYWH,
                            state.normal_rect.g_x, state.normal_rect.g_y,
                            state.normal_rect.g_w, state.normal_rect.g_h);
                    } else {
                        state.normal_rect = current_rect;
                        (void) wind_set(state.handle, WF_WXYWH, full_rect.g_x,
                            full_rect.g_y, full_rect.g_w, full_rect.g_h);
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
        (void) wind_close(state.handle);
        (void) wind_delete(state.handle);
    }
    appl_exit();
    gem_os_shutdown();
    return 0;
}
