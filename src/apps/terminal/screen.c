/*
 * Maintains the Terminal application's character grid: line storage, cursor
 * placement, viewport calculations and dirty-rectangle bookkeeping. DEC
 * control-sequence parsing and screen editing live in vt100.c.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (C) 2026 tomaz stih
 */

#include "terminal.h"

#include <string.h>

char *line_at(term_state_t *state, WORD row)
{
    return state->cells + (size_t)row * (TERM_MAX_COLS + 1);
}

unsigned char *attr_line_at(term_state_t *state, WORD row)
{
    return state->attrs + (size_t)row * TERM_MAX_COLS;
}

WORD line_length(term_state_t *state, WORD row)
{
    char *line = line_at(state, row);
    unsigned char *attrs = attr_line_at(state, row);
    WORD length = TERM_MAX_COLS;

    while (length > 0 && line[length - 1] == '\0' &&
           attrs[length - 1] == 0u) {
        --length;
    }
    return length;
}

WORD clamp_word(WORD value, WORD low, WORD high)
{
    if (value < low) {
        return low;
    }
    if (value > high) {
        return high;
    }
    return value;
}

WORD max_offset(WORD total, WORD visible)
{
    if (visible >= total) {
        return 0;
    }
    return (WORD)(total - visible);
}

WORD slider_size(WORD total, WORD visible)
{
    LONG size;

    if (total <= 0 || visible >= total) {
        return 1000;
    }
    size = (1000L * visible) / total;
    if (size < 32) {
        size = 32;
    }
    if (size > 1000) {
        size = 1000;
    }
    return (WORD)size;
}

WORD slider_pos(WORD offset, WORD total, WORD visible)
{
    WORD limit = max_offset(total, visible);

    if (limit <= 0) {
        return 0;
    }
    return (WORD)((1000L * offset) / limit);
}

WORD offset_from_slider(WORD slider, WORD total, WORD visible)
{
    WORD limit = max_offset(total, visible);

    slider = clamp_word(slider, 0, 1000);
    if (limit <= 0) {
        return 0;
    }
    return (WORD)((slider * (LONG)limit + 500L) / 1000L);
}

void clear_line(term_state_t *state, WORD row)
{
    char *line = line_at(state, row);

    memset(line, 0, (size_t)TERM_MAX_COLS + 1u);
    memset(attr_line_at(state, row), 0, (size_t)TERM_MAX_COLS);
}

void recompute_max_line_len(term_state_t *state)
{
    WORD row;
    WORD best = 0;

    for (row = 0; row < state->total_rows; ++row) {
        WORD len = line_length(state, row);

        if (len > best) {
            best = len;
        }
    }
    state->max_line_len = best;
}

static int cursor_rect_for_position(term_state_t *state, WORD row, WORD col,
                                    GRECT *rect)
{
    WORD cell_row;
    WORD cell_col;

    if (row < state->scroll_row ||
        row >= state->scroll_row + state->visible_rows ||
        col < state->scroll_col ||
        col >= state->scroll_col + state->visible_cols) {
        return 0;
    }

    cell_row = (WORD)(row - state->scroll_row);
    cell_col = (WORD)(col - state->scroll_col);
    rect->g_x =
        (WORD)(state->work.g_x + TERM_MARGIN + cell_col * state->cell_w);
    rect->g_y = (WORD)(state->work.g_y + TERM_MARGIN + cell_row * state->row_h);
    rect->g_w = state->cell_w;
    rect->g_h = state->row_h;
    return 1;
}

int cursor_rect(term_state_t *state, GRECT *rect)
{
    return cursor_rect_for_position(state, state->cursor_row, state->cursor_col,
                                    rect);
}

int row_range_dirty_rect(term_state_t *state, WORD first_row, WORD last_row,
                         GRECT *rect)
{
    WORD top_row;
    WORD bottom_row;
    WORD y0;
    WORD y1;

    if (state == NULL || rect == NULL || state->visible_rows <= 0 ||
        state->work.g_w <= 0 || state->work.g_h <= 0) {
        return 0;
    }

    top_row = (first_row > state->scroll_row) ? first_row : state->scroll_row;
    bottom_row = (last_row < (WORD)(state->scroll_row + state->visible_rows))
                     ? last_row
                     : (WORD)(state->scroll_row + state->visible_rows);
    if (bottom_row < top_row) {
        return 0;
    }

    y0 = (WORD)(state->work.g_y + TERM_MARGIN +
                (top_row - state->scroll_row) * state->row_h);
    y1 = (WORD)(state->work.g_y + TERM_MARGIN +
                (bottom_row - state->scroll_row + 1) * state->row_h - 1);
    if (y1 > state->work.g_y + state->work.g_h - 1) {
        y1 = (WORD)(state->work.g_y + state->work.g_h - 1);
    }
    rect->g_x = state->work.g_x;
    rect->g_y = y0;
    rect->g_w = state->work.g_w;
    rect->g_h = (WORD)(y1 - y0 + 1);
    return rect->g_h > 0;
}

void union_dirty(GRECT *target, const GRECT *add)
{
    WORD x0;
    WORD y0;
    WORD x1;
    WORD y1;

    if (target == NULL || add == NULL || add->g_w <= 0 || add->g_h <= 0) {
        return;
    }
    if (target->g_w <= 0 || target->g_h <= 0) {
        *target = *add;
        return;
    }

    x0 = (target->g_x < add->g_x) ? target->g_x : add->g_x;
    y0 = (target->g_y < add->g_y) ? target->g_y : add->g_y;
    x1 = ((WORD)(target->g_x + target->g_w - 1) >
          (WORD)(add->g_x + add->g_w - 1))
             ? (WORD)(target->g_x + target->g_w - 1)
             : (WORD)(add->g_x + add->g_w - 1);
    y1 = ((WORD)(target->g_y + target->g_h - 1) >
          (WORD)(add->g_y + add->g_h - 1))
             ? (WORD)(target->g_y + target->g_h - 1)
             : (WORD)(add->g_y + add->g_h - 1);
    target->g_x = x0;
    target->g_y = y0;
    target->g_w = (WORD)(x1 - x0 + 1);
    target->g_h = (WORD)(y1 - y0 + 1);
}

void follow_bottom(term_state_t *state)
{
    state->scroll_row = max_offset(state->total_rows, state->visible_rows);
    state->scroll_col = 0;
}
