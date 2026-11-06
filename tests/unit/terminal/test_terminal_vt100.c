/*
 * Verifies DEC VT100 parsing, editing, attributes, character sets and the
 * alternate-screen round trip used by full-screen terminal applications.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (C) 2026 tomaz stih
 */

#include "terminal.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>

static size_t test_cell_bytes(void)
{
    return (size_t)TERM_MAX_LINES * (TERM_MAX_COLS + 1u);
}

static size_t test_attr_bytes(void)
{
    return (size_t)TERM_MAX_LINES * TERM_MAX_COLS;
}

static void test_feed(term_state_t *state, const char *bytes)
{
    while (*bytes != '\0') {
        process_shell_byte(state, (unsigned char)*bytes++);
    }
}

static void test_initialize(term_state_t *state)
{
    memset(state, 0, sizeof(*state));
    state->primary_cells = calloc(1u, test_cell_bytes());
    state->primary_attrs = calloc(1u, test_attr_bytes());
    state->alternate_cells = calloc(1u, test_cell_bytes());
    state->alternate_attrs = calloc(1u, test_attr_bytes());
    assert(state->primary_cells != NULL && state->primary_attrs != NULL &&
           state->alternate_cells != NULL && state->alternate_attrs != NULL);
    state->cells = state->primary_cells;
    state->attrs = state->primary_attrs;
    state->pty.master_fd = -1;
    term_vt100_init(state);
    term_vt100_resize(state, 6, 12);
}

static void test_destroy(term_state_t *state)
{
    free(state->primary_cells);
    free(state->primary_attrs);
    free(state->alternate_cells);
    free(state->alternate_attrs);
}

int main(void)
{
    term_state_t state;

    test_initialize(&state);

    test_feed(&state, "abcdef\033[3DXY");
    assert(memcmp(line_at(&state, 0), "abcXYf", 6u) == 0);

    test_feed(&state, "\033[2;4HQ");
    assert(line_at(&state, 1)[3] == 'Q');
    test_feed(&state, "\033[7mR\033[0mN");
    assert(line_at(&state, 1)[4] == 'R');
    assert((attr_line_at(&state, 1)[4] & TERM_ATTR_REVERSE) != 0u);
    assert(line_at(&state, 1)[5] == 'N');
    assert(attr_line_at(&state, 1)[5] == 0u);

    test_feed(&state, "\033[1;1H\033[2Kabcde\033[1;3H\033[2@XY");
    assert(memcmp(line_at(&state, 0), "abXYcde", 7u) == 0);
    test_feed(&state, "\033[1;3H\033[2P");
    assert(memcmp(line_at(&state, 0), "abcde", 5u) == 0);

    test_feed(&state, "\033[3;1Hsaved\0337\033[6;10HZ\0338!");
    assert(memcmp(line_at(&state, 2), "saved!", 6u) == 0);

    test_feed(&state, "\033[4;1H\033(0lqk\033(B");
    assert(memcmp(line_at(&state, 3), "+-+", 3u) == 0);

    test_feed(&state, "\033[5;1H\342\224\214\342\224\200\342\224\220");
    assert(memcmp(line_at(&state, 4), "+-+", 3u) == 0);
    test_feed(&state, "\033P1$r0m\033\\\033%Gplain");
    assert(memcmp(line_at(&state, 4) + 3, "plain", 5u) == 0);
    test_feed(&state, "\033[6;1H\033[38;2;255;0;0mcolor\033[0m");
    assert(memcmp(line_at(&state, 5), "color", 5u) == 0);

    test_feed(&state, "\033[1;1Hprimary\033[?1049halt");
    assert(state.alternate_active != 0);
    assert(memcmp(line_at(&state, 0), "alt", 3u) == 0);
    test_feed(&state, "\033[?1049l");
    assert(state.alternate_active == 0);
    assert(memcmp(line_at(&state, 0), "primary", 7u) == 0);

    test_feed(&state, "\033[2J");
    assert(line_length(&state, 0) == 0);
    assert(line_length(&state, 3) == 0);

    test_destroy(&state);
    return 0;
}
