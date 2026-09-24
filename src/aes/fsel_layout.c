/*
 * Computes modern file-chooser geometry and hit testing. Keeping layout in
 * one module makes painting and input share the same responsive rectangles.
 *
 * MIT License (see: LICENSE)
 * Copyright (C) 2026 tomaz stih
 */

#include "fsel_private.h"

#include <string.h>

enum { AES_FSEL_TOOL_BUTTON = 24 };

static WORD aes_fsel_action_y(const aes_fsel_state_t *state)
{
    return (WORD)(state->work.g_y + state->work.g_h - AES_FSEL_MARGIN -
                  AES_FSEL_BUTTON_HEIGHT);
}

static WORD aes_fsel_pane_top(const aes_fsel_state_t *state)
{
    return (WORD)(state->work.g_y + AES_FSEL_MARGIN + AES_FSEL_TOOL_HEIGHT +
                  AES_FSEL_MARGIN + state->row_h);
}

WORD aes_fsel_rows_top(const aes_fsel_state_t *state)
{
    return (WORD)(aes_fsel_pane_top(state) + state->row_h);
}

WORD aes_fsel_body_bottom(const aes_fsel_state_t *state)
{
    return (WORD)(aes_fsel_action_y(state) - AES_FSEL_MARGIN);
}

void aes_fsel_file_rect(const aes_fsel_state_t *state, GRECT *rect)
{
    rect->g_x = (WORD)(state->work.g_x + AES_FSEL_MARGIN +
                       AES_FSEL_LOCATION_WIDTH + AES_FSEL_MARGIN);
    rect->g_y = aes_fsel_pane_top(state);
    rect->g_w =
        (WORD)(state->work.g_w - AES_FSEL_MARGIN * 3 - AES_FSEL_LOCATION_WIDTH);
    rect->g_h = (WORD)(aes_fsel_body_bottom(state) - rect->g_y);
}

void aes_fsel_location_rect(const aes_fsel_state_t *state, GRECT *rect)
{
    rect->g_x = (WORD)(state->work.g_x + AES_FSEL_MARGIN);
    rect->g_y = aes_fsel_pane_top(state);
    rect->g_w = AES_FSEL_LOCATION_WIDTH;
    rect->g_h = (WORD)(aes_fsel_body_bottom(state) - rect->g_y);
}

void aes_fsel_name_rect(const aes_fsel_state_t *state, GRECT *rect)
{
    GRECT accept;

    aes_fsel_control_rect(state, AES_FSEL_CONTROL_ACCEPT, &accept);
    rect->g_x = (WORD)(state->work.g_x + AES_FSEL_MARGIN + 58);
    rect->g_y = aes_fsel_action_y(state);
    rect->g_w = (WORD)(accept.g_x - AES_FSEL_MARGIN - rect->g_x);
    rect->g_h = AES_FSEL_BUTTON_HEIGHT;
}

void aes_fsel_path_rect(const aes_fsel_state_t *state, GRECT *rect)
{
    GRECT up;

    if (state == NULL || rect == NULL) {
        return;
    }
    aes_fsel_control_rect(state, AES_FSEL_CONTROL_UP, &up);
    rect->g_x = (WORD)(up.g_x + up.g_w + AES_FSEL_MARGIN);
    rect->g_y = (WORD)(state->work.g_y + AES_FSEL_MARGIN);
    rect->g_w = (WORD)(state->work.g_x + state->work.g_w -
                       AES_FSEL_MARGIN - rect->g_x);
    rect->g_h = AES_FSEL_TOOL_BUTTON;
}

void aes_fsel_sync_work(aes_fsel_state_t *state)
{
    WORD available;

    if (state == NULL) {
        return;
    }
    wind_get(state->handle, WF_CXYWH, &state->work.g_x, &state->work.g_y,
             &state->work.g_w, &state->work.g_h);
    available = (WORD)(aes_fsel_body_bottom(state) - aes_fsel_rows_top(state));
    state->visible_rows = available > 0 ? (WORD)(available / state->row_h) : 1;
    if (state->visible_rows < 1) {
        state->visible_rows = 1;
    }
    if (state->handle > 0) {
        WORD size = state->entry_count > 0
                        ? (WORD)((long)state->visible_rows * 1000L /
                                 state->entry_count)
                        : 1000;
        WORD maximum = state->entry_count > state->visible_rows
                           ? (WORD)(state->entry_count - state->visible_rows)
                           : 0;
        WORD position = maximum > 0
                            ? (WORD)((long)state->top_index * 1000L / maximum)
                            : 0;
        wind_set(state->handle, WF_VSLSIZ, size, 0, 0, 0);
        wind_set(state->handle, WF_VSLIDE, position, 0, 0, 0);
    }
}

void aes_fsel_control_rect(const aes_fsel_state_t *state, WORD control,
                           GRECT *rect)
{
    WORD toolbar_x;
    WORD cancel_x;

    if (state == NULL || rect == NULL) {
        return;
    }
    memset(rect, 0, sizeof(*rect));
    toolbar_x = (WORD)(state->work.g_x + AES_FSEL_MARGIN);
    cancel_x = (WORD)(state->work.g_x + state->work.g_w - AES_FSEL_MARGIN -
                      AES_FSEL_BUTTON_WIDTH);
    switch (control) {
        case AES_FSEL_CONTROL_ACCEPT:
            rect->g_x =
                (WORD)(cancel_x - AES_FSEL_MARGIN - AES_FSEL_BUTTON_WIDTH);
            rect->g_y = aes_fsel_action_y(state);
            rect->g_w = AES_FSEL_BUTTON_WIDTH;
            rect->g_h = AES_FSEL_BUTTON_HEIGHT;
            break;
        case AES_FSEL_CONTROL_CANCEL:
            rect->g_x = cancel_x;
            rect->g_y = aes_fsel_action_y(state);
            rect->g_w = AES_FSEL_BUTTON_WIDTH;
            rect->g_h = AES_FSEL_BUTTON_HEIGHT;
            break;
        case AES_FSEL_CONTROL_BACK:
        case AES_FSEL_CONTROL_FORWARD:
        case AES_FSEL_CONTROL_UP:
            rect->g_x = (WORD)(toolbar_x + (control - AES_FSEL_CONTROL_BACK) *
                                               (AES_FSEL_TOOL_BUTTON + 4));
            rect->g_y = (WORD)(state->work.g_y + AES_FSEL_MARGIN);
            rect->g_w = AES_FSEL_TOOL_BUTTON;
            rect->g_h = AES_FSEL_TOOL_BUTTON;
            break;
        default:
            break;
    }
}

WORD aes_fsel_entry_at(const aes_fsel_state_t *state, WORD x, WORD y)
{
    GRECT file;
    WORD row;

    aes_fsel_file_rect(state, &file);
    file.g_y = aes_fsel_rows_top(state);
    file.g_h = (WORD)(aes_fsel_body_bottom(state) - file.g_y);
    if (aes_point_in_rect(x, y, &file) == 0) {
        return NIL;
    }
    row = (WORD)((y - file.g_y) / state->row_h);
    if (row < 0 || row >= state->visible_rows ||
        state->top_index + row >= state->entry_count) {
        return NIL;
    }
    return (WORD)(state->top_index + row);
}

WORD aes_fsel_location_at(const aes_fsel_state_t *state, WORD x, WORD y)
{
    GRECT pane;
    WORD row;

    aes_fsel_location_rect(state, &pane);
    pane.g_y = aes_fsel_rows_top(state);
    pane.g_h = (WORD)(aes_fsel_body_bottom(state) - pane.g_y);
    if (aes_point_in_rect(x, y, &pane) == 0) {
        return NIL;
    }
    row = (WORD)((y - pane.g_y) / state->row_h);
    return row >= 0 && row < state->location_count ? row : NIL;
}

int aes_fsel_name_hit(const aes_fsel_state_t *state, WORD x, WORD y)
{
    GRECT rect;

    if (state == NULL || state->mode == AES_FSEL_DIRECTORY) {
        return 0;
    }
    aes_fsel_name_rect(state, &rect);
    return aes_point_in_rect(x, y, &rect);
}

int aes_fsel_path_hit(const aes_fsel_state_t *state, WORD x, WORD y)
{
    GRECT rect;

    if (state == NULL) {
        return 0;
    }
    aes_fsel_path_rect(state, &rect);
    return aes_point_in_rect(x, y, &rect);
}

void aes_fsel_scroll_into_view(aes_fsel_state_t *state)
{
    WORD max_top;

    if (state == NULL || state->selected == NIL) {
        return;
    }
    if (state->selected < state->top_index) {
        state->top_index = state->selected;
    } else if (state->selected >= state->top_index + state->visible_rows) {
        state->top_index = (WORD)(state->selected - state->visible_rows + 1);
    }
    max_top = state->entry_count > state->visible_rows
                  ? (WORD)(state->entry_count - state->visible_rows)
                  : 0;
    if (state->top_index < 0) {
        state->top_index = 0;
    } else if (state->top_index > max_top) {
        state->top_index = max_top;
    }
    aes_fsel_sync_work(state);
}
