/*
 * Implements the Terminal sample's DEC VT100-compatible parser and screen
 * editing model. It handles cursor motion, erasure, insertion, scroll margins,
 * SGR attributes, DEC character sets, reports and alternate-screen modes.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (C) 2026 tomaz stih
 */

#include "terminal.h"

#include <stdio.h>
#include <string.h>

static size_t term_cell_bytes(void)
{
    return (size_t)TERM_MAX_LINES * (TERM_MAX_COLS + 1u);
}

static size_t term_attr_bytes(void)
{
    return (size_t)TERM_MAX_LINES * TERM_MAX_COLS;
}

static WORD term_cursor_relative_row(const term_state_t *state)
{
    return clamp_word((WORD)(state->cursor_row - state->screen_top), 0,
                      (WORD)(state->screen_rows - 1));
}

static void term_scroll_history(term_state_t *state)
{
    size_t line_size = (size_t)TERM_MAX_COLS + 1u;

    memmove(state->cells, state->cells + line_size,
            line_size * (TERM_MAX_LINES - 1));
    memset(state->cells + line_size * (TERM_MAX_LINES - 1), 0, line_size);
    memmove(state->attrs, state->attrs + TERM_MAX_COLS,
            (size_t)TERM_MAX_COLS * (TERM_MAX_LINES - 1));
    memset(state->attrs + (size_t)TERM_MAX_COLS * (TERM_MAX_LINES - 1), 0,
           TERM_MAX_COLS);
    if (state->cursor_row > 0) {
        --state->cursor_row;
    }
    if (state->screen_top > 0) {
        --state->screen_top;
    }
    if (state->total_rows > 0) {
        --state->total_rows;
    }
    if (state->scroll_row > 0) {
        --state->scroll_row;
    }
}

static WORD term_ensure_row(term_state_t *state, WORD row)
{
    WORD index;

    while (row >= TERM_MAX_LINES) {
        term_scroll_history(state);
        --row;
    }
    if (row >= state->total_rows) {
        for (index = state->total_rows; index <= row; ++index) {
            clear_line(state, index);
        }
        state->total_rows = (WORD)(row + 1);
    }
    return row;
}

static void term_clear_columns(term_state_t *state, WORD row, WORD first,
                               WORD last)
{
    char *line;
    unsigned char *attrs;
    size_t count;

    if (row < 0 || row >= TERM_MAX_LINES || first > last) {
        return;
    }
    first = clamp_word(first, 0, (WORD)(state->screen_cols - 1));
    last = clamp_word(last, 0, (WORD)(state->screen_cols - 1));
    count = (size_t)(last - first + 1);
    line = line_at(state, row);
    attrs = attr_line_at(state, row);
    memset(line + first, 0, count);
    memset(attrs + first, 0, count);
}

static void term_scroll_region(term_state_t *state, WORD top, WORD bottom,
                               WORD count, int down)
{
    size_t cell_line = (size_t)TERM_MAX_COLS + 1u;
    size_t attr_line = TERM_MAX_COLS;
    WORD abs_top;
    WORD abs_bottom;
    WORD height;
    WORD index;

    top = clamp_word(top, 0, (WORD)(state->screen_rows - 1));
    bottom = clamp_word(bottom, top, (WORD)(state->screen_rows - 1));
    height = (WORD)(bottom - top + 1);
    count = clamp_word(count, 1, height);
    (void)term_ensure_row(state, (WORD)(state->screen_top + bottom));
    abs_top = (WORD)(state->screen_top + top);
    abs_bottom = (WORD)(state->screen_top + bottom);

    if (down != 0) {
        memmove(line_at(state, (WORD)(abs_top + count)),
                line_at(state, abs_top), (size_t)(height - count) * cell_line);
        memmove(attr_line_at(state, (WORD)(abs_top + count)),
                attr_line_at(state, abs_top),
                (size_t)(height - count) * attr_line);
        for (index = 0; index < count; ++index) {
            clear_line(state, (WORD)(abs_top + index));
        }
    } else {
        memmove(line_at(state, abs_top), line_at(state, (WORD)(abs_top + count)),
                (size_t)(height - count) * cell_line);
        memmove(attr_line_at(state, abs_top),
                attr_line_at(state, (WORD)(abs_top + count)),
                (size_t)(height - count) * attr_line);
        for (index = 0; index < count; ++index) {
            clear_line(state, (WORD)(abs_bottom - index));
        }
    }
    state->screen_dirty = 1;
}

static void term_index(term_state_t *state)
{
    WORD row = term_cursor_relative_row(state);

    if (row == state->scroll_bottom) {
        if (state->scroll_top == 0 &&
            state->scroll_bottom == state->screen_rows - 1 &&
            state->alternate_active == 0) {
            state->cursor_row =
                term_ensure_row(state, (WORD)(state->cursor_row + 1));
            state->screen_top = (WORD)(state->screen_top + 1);
            clear_line(state, state->cursor_row);
            state->screen_dirty = 1;
        } else {
            term_scroll_region(state, state->scroll_top, state->scroll_bottom,
                               1, 0);
        }
    } else if (row < state->screen_rows - 1) {
        state->cursor_row =
            term_ensure_row(state, (WORD)(state->cursor_row + 1));
    }
    state->wrap_pending = 0;
}

static void term_reverse_index(term_state_t *state)
{
    WORD row = term_cursor_relative_row(state);

    if (row == state->scroll_top) {
        term_scroll_region(state, state->scroll_top, state->scroll_bottom, 1,
                           1);
    } else if (row > 0) {
        --state->cursor_row;
    }
    state->wrap_pending = 0;
}

static void term_set_cursor(term_state_t *state, WORD row, WORD col)
{
    WORD minimum = state->origin_mode != 0 ? state->scroll_top : 0;
    WORD maximum = state->origin_mode != 0
                       ? state->scroll_bottom
                       : (WORD)(state->screen_rows - 1);

    row = clamp_word(row, minimum, maximum);
    col = clamp_word(col, 0, (WORD)(state->screen_cols - 1));
    state->cursor_row =
        term_ensure_row(state, (WORD)(state->screen_top + row));
    state->cursor_col = col;
    state->wrap_pending = 0;
}

static unsigned char term_map_character(const term_state_t *state,
                                        unsigned char byte)
{
    int special = state->active_charset != 0 ? state->g1_special
                                              : state->g0_special;

    if (special == 0) {
        return byte;
    }
    switch (byte) {
        case '`':
        case '~':
            return '*';
        case 'a':
            return '#';
        case 'f':
            return 'o';
        case 'g':
        case 'j':
        case 'k':
        case 'l':
        case 'm':
        case 'n':
        case 't':
        case 'u':
        case 'v':
        case 'w':
            return '+';
        case 'q':
        case 'o':
        case 'p':
        case 'r':
        case 's':
            return '-';
        case 'x':
            return '|';
        case 'y':
            return '<';
        case 'z':
            return '>';
        case '{':
            return 'p';
        case '|':
            return '!';
        case '}':
            return 'L';
        default:
            return byte;
    }
}

static unsigned char term_map_unicode(uint32_t codepoint)
{
    if ((codepoint >= 0x2500u && codepoint <= 0x2501u) ||
        (codepoint >= 0x2504u && codepoint <= 0x2505u) ||
        (codepoint >= 0x2508u && codepoint <= 0x2509u)) {
        return '-';
    }
    if ((codepoint >= 0x2502u && codepoint <= 0x2503u) ||
        (codepoint >= 0x2506u && codepoint <= 0x2507u) ||
        (codepoint >= 0x250au && codepoint <= 0x250bu)) {
        return '|';
    }
    if (codepoint >= 0x250cu && codepoint <= 0x257fu) {
        return '+';
    }
    if (codepoint >= 0x2580u && codepoint <= 0x259fu) {
        return '#';
    }
    if (codepoint >= 0x2800u && codepoint <= 0x28ffu) {
        return '*';
    }
    switch (codepoint) {
        case 0x00a0u:
            return ' ';
        case 0x2010u:
        case 0x2011u:
        case 0x2012u:
        case 0x2013u:
        case 0x2014u:
        case 0x2015u:
            return '-';
        case 0x2018u:
        case 0x2019u:
            return '\'';
        case 0x201cu:
        case 0x201du:
            return '"';
        case 0x2022u:
        case 0x25cfu:
            return '*';
        case 0x2026u:
            return '.';
        case 0x2190u:
            return '<';
        case 0x2191u:
            return '^';
        case 0x2192u:
            return '>';
        case 0x2193u:
            return 'v';
        case 0x26a0u:
            return '!';
        case 0x2713u:
        case 0x2714u:
            return 'x';
        default:
            return '?';
    }
}

static void term_put_character(term_state_t *state, unsigned char byte)
{
    char *line;
    unsigned char *attrs;
    WORD col;

    if (state->wrap_pending != 0) {
        state->cursor_col = 0;
        term_index(state);
    }
    state->cursor_row = term_ensure_row(state, state->cursor_row);
    col = clamp_word(state->cursor_col, 0, (WORD)(state->screen_cols - 1));
    line = line_at(state, state->cursor_row);
    attrs = attr_line_at(state, state->cursor_row);
    if (state->insert_mode != 0 && col < state->screen_cols - 1) {
        memmove(line + col + 1, line + col,
                (size_t)(state->screen_cols - col - 1));
        memmove(attrs + col + 1, attrs + col,
                (size_t)(state->screen_cols - col - 1));
    }
    byte = term_map_character(state, byte);
    line[col] = (char)byte;
    attrs[col] = state->current_attr;
    state->last_printed = byte;
    if (col + 1 > state->max_line_len) {
        state->max_line_len = (WORD)(col + 1);
    }
    if (col == state->screen_cols - 1) {
        state->wrap_pending = state->auto_wrap;
    } else {
        state->cursor_col = (WORD)(col + 1);
    }
}

static void term_begin_utf8(term_state_t *state, unsigned char byte)
{
    if (byte >= 0xc2u && byte <= 0xdfu) {
        state->utf8_codepoint = byte & 0x1fu;
        state->utf8_remaining = 1;
    } else if (byte >= 0xe0u && byte <= 0xefu) {
        state->utf8_codepoint = byte & 0x0fu;
        state->utf8_remaining = 2;
    } else if (byte >= 0xf0u && byte <= 0xf4u) {
        state->utf8_codepoint = byte & 0x07u;
        state->utf8_remaining = 3;
    }
}

static void term_continue_utf8(term_state_t *state, unsigned char byte)
{
    if (byte < 0x80u || byte > 0xbfu) {
        state->utf8_remaining = 0;
        state->utf8_codepoint = 0u;
        return;
    }
    state->utf8_codepoint =
        (state->utf8_codepoint << 6) | (uint32_t)(byte & 0x3fu);
    --state->utf8_remaining;
    if (state->utf8_remaining == 0) {
        term_put_character(state, term_map_unicode(state->utf8_codepoint));
        state->utf8_codepoint = 0u;
    }
}

static void term_tab(term_state_t *state)
{
    WORD col;

    for (col = (WORD)(state->cursor_col + 1); col < state->screen_cols; ++col) {
        if (state->tab_stops[col] != 0u) {
            state->cursor_col = col;
            state->wrap_pending = 0;
            return;
        }
    }
    state->cursor_col = (WORD)(state->screen_cols - 1);
    state->wrap_pending = 0;
}

static WORD term_param(const term_state_t *state, WORD index,
                       WORD default_value)
{
    if (index < 0 || index >= state->csi_count ||
        state->csi_params[index] < 0) {
        return default_value;
    }
    return state->csi_params[index];
}

static void term_erase_display(term_state_t *state, WORD mode)
{
    WORD row = term_cursor_relative_row(state);
    WORD index;

    if (mode == 0) {
        term_clear_columns(state, state->cursor_row, state->cursor_col,
                           (WORD)(state->screen_cols - 1));
        for (index = (WORD)(row + 1); index < state->screen_rows; ++index) {
            clear_line(state, term_ensure_row(
                                  state, (WORD)(state->screen_top + index)));
        }
    } else if (mode == 1) {
        for (index = 0; index < row; ++index) {
            clear_line(state, term_ensure_row(
                                  state, (WORD)(state->screen_top + index)));
        }
        term_clear_columns(state, state->cursor_row, 0, state->cursor_col);
    } else if (mode == 2 || mode == 3) {
        for (index = 0; index < state->screen_rows; ++index) {
            clear_line(state, term_ensure_row(
                                  state, (WORD)(state->screen_top + index)));
        }
    }
    state->screen_dirty = 1;
}

static void term_erase_line(term_state_t *state, WORD mode)
{
    if (mode == 0) {
        term_clear_columns(state, state->cursor_row, state->cursor_col,
                           (WORD)(state->screen_cols - 1));
    } else if (mode == 1) {
        term_clear_columns(state, state->cursor_row, 0, state->cursor_col);
    } else if (mode == 2) {
        clear_line(state, state->cursor_row);
    }
}

static void term_insert_delete_chars(term_state_t *state, WORD count,
                                     int insert)
{
    char *line = line_at(state, state->cursor_row);
    unsigned char *attrs = attr_line_at(state, state->cursor_row);
    WORD available = (WORD)(state->screen_cols - state->cursor_col);

    count = clamp_word(count, 1, available);
    if (insert != 0) {
        memmove(line + state->cursor_col + count, line + state->cursor_col,
                (size_t)(available - count));
        memmove(attrs + state->cursor_col + count,
                attrs + state->cursor_col, (size_t)(available - count));
        memset(line + state->cursor_col, 0, (size_t)count);
        memset(attrs + state->cursor_col, 0, (size_t)count);
    } else {
        memmove(line + state->cursor_col, line + state->cursor_col + count,
                (size_t)(available - count));
        memmove(attrs + state->cursor_col,
                attrs + state->cursor_col + count,
                (size_t)(available - count));
        memset(line + state->screen_cols - count, 0, (size_t)count);
        memset(attrs + state->screen_cols - count, 0, (size_t)count);
    }
}

static void term_save_cursor(term_state_t *state)
{
    state->saved_cursor_row = term_cursor_relative_row(state);
    state->saved_cursor_col = state->cursor_col;
    state->saved_attr = state->current_attr;
    state->saved_g0_special = state->g0_special;
    state->saved_g1_special = state->g1_special;
    state->saved_active_charset = state->active_charset;
}

static void term_restore_cursor(term_state_t *state)
{
    term_set_cursor(state, state->saved_cursor_row, state->saved_cursor_col);
    state->current_attr = (unsigned char)state->saved_attr;
    state->g0_special = state->saved_g0_special;
    state->g1_special = state->saved_g1_special;
    state->active_charset = state->saved_active_charset;
}

static void term_enter_alternate(term_state_t *state)
{
    if (state->alternate_active != 0) {
        return;
    }
    state->primary_cursor_row = state->cursor_row;
    state->primary_cursor_col = state->cursor_col;
    state->primary_screen_top = state->screen_top;
    state->primary_total_rows = state->total_rows;
    state->primary_max_line_len = state->max_line_len;
    state->primary_scroll_row = state->scroll_row;
    state->primary_scroll_col = state->scroll_col;
    state->primary_scroll_top = state->scroll_top;
    state->primary_scroll_bottom = state->scroll_bottom;
    state->primary_current_attr = state->current_attr;
    state->primary_cursor_visible = state->cursor_visible;
    state->primary_auto_wrap = state->auto_wrap;
    state->primary_origin_mode = state->origin_mode;
    state->primary_insert_mode = state->insert_mode;
    state->primary_application_cursor_keys = state->application_cursor_keys;
    state->primary_g0_special = state->g0_special;
    state->primary_g1_special = state->g1_special;
    state->primary_active_charset = state->active_charset;
    state->cells = state->alternate_cells;
    state->attrs = state->alternate_attrs;
    memset(state->cells, 0, term_cell_bytes());
    memset(state->attrs, 0, term_attr_bytes());
    state->alternate_active = 1;
    state->cursor_row = 0;
    state->cursor_col = 0;
    state->screen_top = 0;
    state->total_rows = 1;
    state->max_line_len = 0;
    state->scroll_row = 0;
    state->scroll_col = 0;
    state->scroll_top = 0;
    state->scroll_bottom = (WORD)(state->screen_rows - 1);
    state->current_attr = 0u;
    state->wrap_pending = 0;
    state->screen_dirty = 1;
}

static void term_leave_alternate(term_state_t *state)
{
    if (state->alternate_active == 0) {
        return;
    }
    state->cells = state->primary_cells;
    state->attrs = state->primary_attrs;
    state->alternate_active = 0;
    state->cursor_row = state->primary_cursor_row;
    state->cursor_col = state->primary_cursor_col;
    state->screen_top = state->primary_screen_top;
    state->total_rows = state->primary_total_rows;
    state->max_line_len = state->primary_max_line_len;
    state->scroll_row = state->primary_scroll_row;
    state->scroll_col = state->primary_scroll_col;
    state->scroll_top = state->primary_scroll_top;
    state->scroll_bottom = state->primary_scroll_bottom;
    state->current_attr = (unsigned char)state->primary_current_attr;
    state->cursor_visible = state->primary_cursor_visible;
    state->auto_wrap = state->primary_auto_wrap;
    state->origin_mode = state->primary_origin_mode;
    state->insert_mode = state->primary_insert_mode;
    state->application_cursor_keys = state->primary_application_cursor_keys;
    state->g0_special = state->primary_g0_special;
    state->g1_special = state->primary_g1_special;
    state->active_charset = state->primary_active_charset;
    state->wrap_pending = 0;
    state->screen_dirty = 1;
}

static void term_set_mode(term_state_t *state, WORD mode, int enabled)
{
    if (state->csi_private != 0) {
        switch (mode) {
            case 1:
                state->application_cursor_keys = enabled;
                break;
            case 6:
                state->origin_mode = enabled;
                term_set_cursor(state, enabled != 0 ? state->scroll_top : 0,
                                0);
                break;
            case 7:
                state->auto_wrap = enabled;
                state->wrap_pending = 0;
                break;
            case 25:
                state->cursor_visible = enabled;
                state->screen_dirty = 1;
                break;
            case 47:
            case 1047:
            case 1049:
                if (enabled != 0) {
                    term_enter_alternate(state);
                } else {
                    term_leave_alternate(state);
                }
                break;
            default:
                break;
        }
    } else if (mode == 4) {
        state->insert_mode = enabled;
    }
}

static void term_sgr(term_state_t *state)
{
    WORD index;

    for (index = 0; index < state->csi_count; ++index) {
        WORD value = term_param(state, index, 0);

        switch (value) {
            case 0:
                state->current_attr = 0u;
                break;
            case 1:
                state->current_attr |= TERM_ATTR_BOLD;
                break;
            case 4:
                state->current_attr |= TERM_ATTR_UNDERLINE;
                break;
            case 7:
                state->current_attr |= TERM_ATTR_REVERSE;
                break;
            case 22:
                state->current_attr &= (unsigned char)~TERM_ATTR_BOLD;
                break;
            case 24:
                state->current_attr &= (unsigned char)~TERM_ATTR_UNDERLINE;
                break;
            case 27:
                state->current_attr &= (unsigned char)~TERM_ATTR_REVERSE;
                break;
            default:
                break;
        }
    }
}

static void term_report(term_state_t *state, const char *format, WORD row,
                        WORD col)
{
    char response[32];
    int length = snprintf(response, sizeof(response), format, row, col);

    if (length > 0 && (size_t)length < sizeof(response)) {
        (void)gem_os_pty_write(&state->pty, response, (uint32_t)length);
    }
}

static void term_dispatch_csi(term_state_t *state, unsigned char final)
{
    WORD count = term_param(state, 0, 1);
    WORD row = term_cursor_relative_row(state);
    WORD index;

    if (count < 1) {
        count = 1;
    }
    switch (final) {
        case 'A':
            term_set_cursor(state, (WORD)(row - count), state->cursor_col);
            break;
        case 'B':
        case 'e':
            term_set_cursor(state, (WORD)(row + count), state->cursor_col);
            break;
        case 'C':
        case 'a':
            term_set_cursor(state, row,
                            (WORD)(state->cursor_col + count));
            break;
        case 'D':
            term_set_cursor(state, row,
                            (WORD)(state->cursor_col - count));
            break;
        case 'E':
            term_set_cursor(state, (WORD)(row + count), 0);
            break;
        case 'F':
            term_set_cursor(state, (WORD)(row - count), 0);
            break;
        case 'G':
        case '`':
            term_set_cursor(state, row, (WORD)(count - 1));
            break;
        case 'H':
        case 'f': {
            WORD target_row = (WORD)(term_param(state, 0, 1) - 1);
            WORD target_col = (WORD)(term_param(state, 1, 1) - 1);

            if (state->origin_mode != 0) {
                target_row = (WORD)(target_row + state->scroll_top);
            }
            term_set_cursor(state, target_row, target_col);
            break;
        }
        case 'd':
            term_set_cursor(state, (WORD)(count - 1), state->cursor_col);
            break;
        case 'J':
            term_erase_display(state, term_param(state, 0, 0));
            break;
        case 'K':
            term_erase_line(state, term_param(state, 0, 0));
            break;
        case '@':
            term_insert_delete_chars(state, count, 1);
            break;
        case 'P':
            term_insert_delete_chars(state, count, 0);
            break;
        case 'X':
            term_clear_columns(
                state, state->cursor_row, state->cursor_col,
                (WORD)(state->cursor_col + count - 1));
            break;
        case 'L':
            if (row >= state->scroll_top && row <= state->scroll_bottom) {
                term_scroll_region(state, row, state->scroll_bottom, count, 1);
            }
            break;
        case 'M':
            if (row >= state->scroll_top && row <= state->scroll_bottom) {
                term_scroll_region(state, row, state->scroll_bottom, count, 0);
            }
            break;
        case 'S':
            term_scroll_region(state, state->scroll_top, state->scroll_bottom,
                               count, 0);
            break;
        case 'T':
            term_scroll_region(state, state->scroll_top, state->scroll_bottom,
                               count, 1);
            break;
        case 'm':
            term_sgr(state);
            break;
        case 'r': {
            WORD top = (WORD)(term_param(state, 0, 1) - 1);
            WORD bottom = (WORD)(term_param(state, 1, state->screen_rows) - 1);

            if (top >= 0 && bottom < state->screen_rows && top < bottom) {
                state->scroll_top = top;
                state->scroll_bottom = bottom;
                term_set_cursor(state,
                                state->origin_mode != 0 ? top : 0, 0);
            }
            break;
        }
        case 'h':
        case 'l':
            for (index = 0; index < state->csi_count; ++index) {
                term_set_mode(state, term_param(state, index, 0), final == 'h');
            }
            break;
        case 's':
            term_save_cursor(state);
            break;
        case 'u':
            term_restore_cursor(state);
            break;
        case 'g':
            if (term_param(state, 0, 0) == 3) {
                memset(state->tab_stops, 0, sizeof(state->tab_stops));
            } else {
                state->tab_stops[state->cursor_col] = 0u;
            }
            break;
        case 'b':
            if (state->last_printed >= 32u) {
                for (index = 0; index < count; ++index) {
                    term_put_character(state, state->last_printed);
                }
            }
            break;
        case 'n':
            if (term_param(state, 0, 0) == 5) {
                term_report(state, "\033[0n", 0, 0);
            } else if (term_param(state, 0, 0) == 6) {
                term_report(state, "\033[%d;%dR", (WORD)(row + 1),
                            (WORD)(state->cursor_col + 1));
            }
            break;
        case 'c':
            term_report(state, "\033[?1;2c", 0, 0);
            break;
        default:
            break;
    }
}

static void term_reset_parser(term_state_t *state)
{
    WORD index;

    state->csi_private = 0;
    state->csi_count = 1;
    for (index = 0; index < TERM_CSI_PARAMS; ++index) {
        state->csi_params[index] = -1;
    }
}

static void term_reset_screen(term_state_t *state)
{
    memset(state->cells, 0, term_cell_bytes());
    memset(state->attrs, 0, term_attr_bytes());
    state->cursor_row = 0;
    state->cursor_col = 0;
    state->screen_top = 0;
    state->scroll_top = 0;
    state->scroll_bottom = (WORD)(state->screen_rows - 1);
    state->total_rows = 1;
    state->max_line_len = 0;
    state->scroll_row = 0;
    state->scroll_col = 0;
    state->current_attr = 0u;
    state->cursor_visible = 1;
    state->auto_wrap = 1;
    state->origin_mode = 0;
    state->insert_mode = 0;
    state->application_cursor_keys = 0;
    state->wrap_pending = 0;
    state->g0_special = 0;
    state->g1_special = 0;
    state->active_charset = 0;
    state->utf8_remaining = 0;
    state->utf8_codepoint = 0u;
    state->screen_dirty = 1;
}

void term_vt100_init(term_state_t *state)
{
    WORD col;

    state->screen_rows = TERM_INITIAL_ROWS;
    state->screen_cols = TERM_INITIAL_COLS;
    state->parser_state = TERM_PARSE_GROUND;
    state->charset_target = 0;
    memset(state->tab_stops, 0, sizeof(state->tab_stops));
    for (col = 8; col < TERM_MAX_COLS; col = (WORD)(col + 8)) {
        state->tab_stops[col] = 1u;
    }
    term_reset_parser(state);
    term_reset_screen(state);
}

void term_vt100_resize(term_state_t *state, WORD rows, WORD columns)
{
    WORD old_rows = state->screen_rows;
    int full_margins = state->scroll_top == 0 &&
                       state->scroll_bottom == old_rows - 1;

    rows = clamp_word(rows, 1, TERM_MAX_LINES);
    columns = clamp_word(columns, 1, TERM_MAX_COLS);
    if (rows == state->screen_rows && columns == state->screen_cols) {
        return;
    }

    state->screen_rows = rows;
    state->screen_cols = columns;
    if (full_margins != 0) {
        state->scroll_bottom = (WORD)(state->screen_rows - 1);
    } else {
        state->scroll_top = clamp_word(
            state->scroll_top, 0, (WORD)(state->screen_rows - 1));
        state->scroll_bottom = clamp_word(
            state->scroll_bottom, state->scroll_top,
            (WORD)(state->screen_rows - 1));
    }
    term_set_cursor(state, term_cursor_relative_row(state), state->cursor_col);
}

void process_shell_byte(term_state_t *state, unsigned char byte)
{
    if (state->parser_state == TERM_PARSE_GROUND &&
        state->utf8_remaining != 0) {
        term_continue_utf8(state, byte);
        return;
    }
    if (byte == 0x18u || byte == 0x1au) {
        state->parser_state = TERM_PARSE_GROUND;
        state->utf8_remaining = 0;
        return;
    }
    if (state->parser_state == TERM_PARSE_OSC) {
        if (byte == 0x07u) {
            state->parser_state = TERM_PARSE_GROUND;
        } else if (byte == 0x1bu) {
            state->parser_state = TERM_PARSE_OSC_ESCAPE;
        }
        return;
    }
    if (state->parser_state == TERM_PARSE_OSC_ESCAPE) {
        state->parser_state = byte == '\\' ? TERM_PARSE_GROUND
                                            : TERM_PARSE_OSC;
        return;
    }
    if (state->parser_state == TERM_PARSE_CONTROL_STRING) {
        if (byte == 0x1bu) {
            state->parser_state = TERM_PARSE_CONTROL_STRING_ESCAPE;
        } else if (byte == 0x9cu) {
            state->parser_state = TERM_PARSE_GROUND;
        }
        return;
    }
    if (state->parser_state == TERM_PARSE_CONTROL_STRING_ESCAPE) {
        state->parser_state = byte == '\\' ? TERM_PARSE_GROUND
                                            : TERM_PARSE_CONTROL_STRING;
        return;
    }
    if (state->parser_state == TERM_PARSE_ESCAPE_INTERMEDIATE) {
        state->parser_state = TERM_PARSE_GROUND;
        return;
    }
    if (state->parser_state == TERM_PARSE_CHARSET) {
        if (state->charset_target == 0) {
            state->g0_special = byte == '0';
        } else {
            state->g1_special = byte == '0';
        }
        state->parser_state = TERM_PARSE_GROUND;
        return;
    }
    if (state->parser_state == TERM_PARSE_CSI) {
        if (byte >= '0' && byte <= '9') {
            WORD *value = &state->csi_params[state->csi_count - 1];

            if (*value < 0) {
                *value = 0;
            }
            if (*value < 1000) {
                *value = (WORD)(*value * 10 + byte - '0');
            }
            return;
        }
        if (byte == ';' || byte == ':') {
            if (state->csi_count < TERM_CSI_PARAMS) {
                ++state->csi_count;
            }
            return;
        }
        if (byte == '?' && state->csi_count == 1 &&
            state->csi_params[0] < 0) {
            state->csi_private = 1;
            return;
        }
        if (byte >= 0x40u && byte <= 0x7eu) {
            term_dispatch_csi(state, byte);
            state->parser_state = TERM_PARSE_GROUND;
        }
        return;
    }
    if (state->parser_state == TERM_PARSE_ESCAPE) {
        state->parser_state = TERM_PARSE_GROUND;
        switch (byte) {
            case '[':
                term_reset_parser(state);
                state->parser_state = TERM_PARSE_CSI;
                break;
            case ']':
                state->parser_state = TERM_PARSE_OSC;
                break;
            case 'P':
            case 'X':
            case '^':
            case '_':
                state->parser_state = TERM_PARSE_CONTROL_STRING;
                break;
            case '(':
            case ')':
                state->charset_target = byte == ')' ? 1 : 0;
                state->parser_state = TERM_PARSE_CHARSET;
                break;
            case '7':
                term_save_cursor(state);
                break;
            case '8':
                term_restore_cursor(state);
                break;
            case 'D':
                term_index(state);
                break;
            case 'E':
                state->cursor_col = 0;
                term_index(state);
                break;
            case 'H':
                state->tab_stops[state->cursor_col] = 1u;
                break;
            case 'M':
                term_reverse_index(state);
                break;
            case 'Z':
                term_report(state, "\033[?1;2c", 0, 0);
                break;
            case 'c':
                term_reset_screen(state);
                break;
            default:
                if (byte >= 0x20u && byte <= 0x2fu) {
                    state->parser_state = TERM_PARSE_ESCAPE_INTERMEDIATE;
                }
                break;
        }
        return;
    }

    switch (byte) {
        case 0x1b:
            state->parser_state = TERM_PARSE_ESCAPE;
            break;
        case 0x9b:
            term_reset_parser(state);
            state->parser_state = TERM_PARSE_CSI;
            break;
        case 0x84:
            term_index(state);
            break;
        case 0x85:
            state->cursor_col = 0;
            term_index(state);
            break;
        case 0x88:
            state->tab_stops[state->cursor_col] = 1u;
            break;
        case 0x8d:
            term_reverse_index(state);
            break;
        case 0x90:
        case 0x98:
        case 0x9e:
        case 0x9f:
            state->parser_state = TERM_PARSE_CONTROL_STRING;
            break;
        case 0x9d:
            state->parser_state = TERM_PARSE_OSC;
            break;
        case 0x9c:
            break;
        case '\r':
            state->cursor_col = 0;
            state->wrap_pending = 0;
            break;
        case '\n':
        case '\v':
        case '\f':
            term_index(state);
            break;
        case '\b':
            if (state->cursor_col > 0) {
                --state->cursor_col;
            }
            state->wrap_pending = 0;
            break;
        case '\t':
            term_tab(state);
            break;
        case 0x0e:
            state->active_charset = 1;
            break;
        case 0x0f:
            state->active_charset = 0;
            break;
        default:
            if (byte >= 32u && byte <= 126u) {
                term_put_character(state, byte);
            } else if (byte >= 0xc2u) {
                term_begin_utf8(state, byte);
            }
            break;
    }
}
