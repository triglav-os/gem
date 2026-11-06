/*
 * Evaluates the calculator sample's arithmetic: digit and operator entry,
 * pending operations, percent, sign, memory, error state, display
 * formatting and the mapping of keyboard characters to buttons.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (C) 2026 tomaz stih
 */

#include "calc_private.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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
    if (rc < 0 || (size_t)rc >= size) {
        return 0;
    }
    if ((size_t)rc > 14u) {
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

void calc_update_display(calc_state_t *state)
{
    if (state == NULL) {
        return;
    }

    if (state->error_state) {
        calc_right_justify(state, "error");
    } else {
        calc_right_justify(state, state->input_text);
    }

    state->tree[calc_memory_label].ob_spec =
        state->memory_set ? (LONG)(intptr_t) "M" : (LONG)(intptr_t) "";
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

void calc_clear_all(calc_state_t *state)
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

void calc_handle_button(calc_state_t *state, WORD object)
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

WORD calc_map_key(WORD key_code)
{
    WORD ch;
    UWORD scan;

    ch = (WORD)(key_code & 0x00ff);
    scan = (UWORD)((UWORD)key_code >> 8);
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
