/*
 * Declares the calculator sample's state, object ids and the helpers its
 * user interface and arithmetic engine share. Private to the sample.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (C) 2026 tomaz stih
 */

#ifndef GEM_SAMPLE_CALC_PRIVATE_H
#define GEM_SAMPLE_CALC_PRIVATE_H

#include <gem/aes.h>
#include <gem/portab.h>
#include <gem/vdi.h>

#include "calc.h"

#include <stdint.h>

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

enum { calc_work_width = 264, calc_work_height = 238 };


/* Reset entry, pending operation and error. */
void calc_clear_all(calc_state_t *state);
/* Apply a pressed button to the calculator state. */
void calc_handle_button(calc_state_t *state, WORD object);
/* Button object for a keyboard character, or NIL. */
WORD calc_map_key(WORD key_code);
/* Redraw one object of the calculator tree. */
void calc_redraw_object(calc_state_t *state, WORD object);
/* Store text right-justified in the display field. */
void calc_right_justify(calc_state_t *state, const char *text);
/* Refresh the display text from the current entry. */
void calc_update_display(calc_state_t *state);

#endif
