/*
 * Renders the Terminal application's window: layout of rows and columns from
 * the work area, scroll bar synchronization and the clipped repaint of
 * text rows and the cursor for a damaged rectangle.
 *
 * MIT License (see: LICENSE)
 * Copyright (C) 2026 tomaz stih
 */

#include "terminal.h"

void sync_layout(term_state_t *state)
{
    wind_get(state->handle, WF_WORKXYWH, &state->work.g_x, &state->work.g_y,
             &state->work.g_w, &state->work.g_h);

    state->visible_rows =
        (WORD)((state->work.g_h - 2 * TERM_MARGIN) / state->row_h);
    state->visible_cols =
        (WORD)((state->work.g_w - 2 * TERM_MARGIN) / state->cell_w);
    if (state->visible_rows < 1) {
        state->visible_rows = 1;
    }
    if (state->visible_cols < 1) {
        state->visible_cols = 1;
    }
    if (state->visible_rows > TERM_MAX_LINES) {
        state->visible_rows = TERM_MAX_LINES;
    }
    if (state->visible_cols > TERM_MAX_COLS) {
        state->visible_cols = TERM_MAX_COLS;
    }
    term_vt100_resize(state, state->visible_rows, state->visible_cols);

    state->scroll_row =
        clamp_word(state->scroll_row, 0,
                   max_offset(state->total_rows, state->visible_rows));
    state->scroll_col =
        clamp_word(state->scroll_col, 0,
                   max_offset(state->max_line_len, state->visible_cols));
}

static void update_window_controls(term_state_t *state)
{
    wind_set_str(state->handle, WF_NAME, "Terminal");
    wind_set(state->handle, WF_VSLSIZ,
             slider_size(state->total_rows, state->visible_rows), 0, 0, 0);
    wind_set(state->handle, WF_HSLSIZ,
             slider_size(state->max_line_len, state->visible_cols), 0, 0, 0);
    wind_set(
        state->handle, WF_VSLIDE,
        slider_pos(state->scroll_row, state->total_rows, state->visible_rows),
        0, 0, 0);
    wind_set(
        state->handle, WF_HSLIDE,
        slider_pos(state->scroll_col, state->max_line_len, state->visible_cols),
        0, 0, 0);
}

static void draw_terminal_row(term_state_t *state, WORD logical_row,
                              WORD row_top, WORD text_y)
{
    char *line = line_at(state, logical_row);
    unsigned char *attrs = attr_line_at(state, logical_row);
    WORD first_col = state->scroll_col;
    WORD last_col = (WORD)(first_col + state->visible_cols);
    WORD col = first_col;

    if (last_col > TERM_MAX_COLS) {
        last_col = TERM_MAX_COLS;
    }
    while (col < last_col) {
        char text[TERM_MAX_COLS + 1];
        unsigned char attr = attrs[col];
        WORD run_start = col;
        WORD length = 0;
        WORD x;

        while (col < last_col && attrs[col] == attr) {
            text[length++] = line[col] != '\0' ? line[col] : ' ';
            ++col;
        }
        text[length] = '\0';
        x = (WORD)(state->work.g_x + TERM_MARGIN +
                   (run_start - state->scroll_col) * state->cell_w);
        if ((attr & TERM_ATTR_REVERSE) != 0u) {
            WORD fill[4];

            fill[0] = x;
            fill[1] = row_top;
            fill[2] = (WORD)(x + length * state->cell_w - 1);
            fill[3] = (WORD)(row_top + state->row_h - 1);
            vsf_color(vdi_handle, WHITE);
            vr_recfl(vdi_handle, fill);
            vst_color(vdi_handle, BLACK);
        } else {
            vst_color(vdi_handle, WHITE);
        }
        v_gtext(vdi_handle, x, text_y, (CONST BYTE *)text);
        if ((attr & TERM_ATTR_BOLD) != 0u) {
            v_gtext(vdi_handle, (WORD)(x + 1), text_y,
                    (CONST BYTE *)text);
        }
        if ((attr & TERM_ATTR_UNDERLINE) != 0u) {
            WORD underline[4] = {
                x, (WORD)(text_y + 1),
                (WORD)(x + length * state->cell_w - 1),
                (WORD)(text_y + 1)};

            vsl_color(vdi_handle,
                      (attr & TERM_ATTR_REVERSE) != 0u ? BLACK : WHITE);
            v_pline(vdi_handle, 2, underline);
        }
    }
    vst_color(vdi_handle, WHITE);
}

void draw_terminal(term_state_t *state, const GRECT *dirty)
{
    GRECT box;
    WORD previous_font;
    WORD text_attributes[10] = {0};

    sync_layout(state);
    update_window_controls(state);

    wind_update(BEG_UPDATE);
    previous_font = ATARI;
    if (vqt_attributes(vdi_handle, text_attributes) != 0) {
        previous_font = text_attributes[0];
    }
    (void)vst_font(vdi_handle, ATARI);
    wind_get(state->handle, WF_FIRSTXYWH, &box.g_x, &box.g_y, &box.g_w,
             &box.g_h);
    while (box.g_w > 0 && box.g_h > 0) {
        WORD x0 = box.g_x;
        WORD y0 = box.g_y;
        WORD x1 = (WORD)(box.g_x + box.g_w - 1);
        WORD y1 = (WORD)(box.g_y + box.g_h - 1);
        WORD clip_xy[4];
        WORD fill_xy[4];
        WORD row;

        if (dirty != NULL) {
            WORD dx1 = (WORD)(dirty->g_x + dirty->g_w - 1);
            WORD dy1 = (WORD)(dirty->g_y + dirty->g_h - 1);

            if (x0 < dirty->g_x) {
                x0 = dirty->g_x;
            }
            if (y0 < dirty->g_y) {
                y0 = dirty->g_y;
            }
            if (x1 > dx1) {
                x1 = dx1;
            }
            if (y1 > dy1) {
                y1 = dy1;
            }
        }

        if (x0 <= x1 && y0 <= y1) {
            clip_xy[0] = x0;
            clip_xy[1] = y0;
            clip_xy[2] = x1;
            clip_xy[3] = y1;
            vs_clip(vdi_handle, 1, clip_xy);
            (void)vswr_mode(vdi_handle, MD_REPLACE);
            (void)vsf_interior(vdi_handle, FIS_SOLID);
            (void)vsf_style(vdi_handle, 1);

            fill_xy[0] = state->work.g_x;
            fill_xy[1] = state->work.g_y;
            fill_xy[2] = (WORD)(state->work.g_x + state->work.g_w - 1);
            fill_xy[3] = (WORD)(state->work.g_y + state->work.g_h - 1);
            /*
             * The VDI color-index-to-pixel mapping is inverted (see
             * vdi_color_to_pixel), so BLACK paints white pixels and
             * WHITE paints black ones -- matching how the AES
             * engine's own aes_light_color()/aes_dark_color() do.
             */
            vsf_color(vdi_handle, BLACK);
            vr_recfl(vdi_handle, fill_xy);

            vst_color(vdi_handle, WHITE);
            for (row = 0; row <= state->visible_rows; ++row) {
                WORD logical_row = (WORD)(state->scroll_row + row);
                WORD row_top =
                    (WORD)(state->work.g_y + TERM_MARGIN + row * state->row_h);
                WORD row_bottom = (WORD)(row_top + state->row_h - 1);
                WORD text_y;
                if (dirty != NULL && (row_bottom < dirty->g_y ||
                                      row_top > dirty->g_y + dirty->g_h - 1)) {
                    continue;
                }
                if (logical_row >= state->total_rows) {
                    break;
                }
                text_y = (WORD)(state->work.g_y + TERM_MARGIN +
                                row * state->row_h + state->text_ascent);
                draw_terminal_row(state, logical_row, row_top, text_y);
            }

            {
                GRECT cursor;

                if (state->cursor_visible != 0 &&
                    cursor_rect(state, &cursor) != 0) {
                    WORD dirty_x1 = (dirty != NULL)
                                        ? (WORD)(dirty->g_x + dirty->g_w - 1)
                                        : 0;
                    WORD dirty_y1 = (dirty != NULL)
                                        ? (WORD)(dirty->g_y + dirty->g_h - 1)
                                        : 0;

                    if (dirty == NULL ||
                        !(cursor.g_x + cursor.g_w - 1 < dirty->g_x ||
                          cursor.g_x > dirty_x1 ||
                          cursor.g_y + cursor.g_h - 1 < dirty->g_y ||
                          cursor.g_y > dirty_y1)) {
                        WORD cursor_xy[4];
                        WORD text_y;
                        char glyph[2] = {' ', '\0'};
                        char *line;

                        cursor_xy[0] = cursor.g_x;
                        cursor_xy[1] = cursor.g_y;
                        cursor_xy[2] = (WORD)(cursor.g_x + cursor.g_w - 1);
                        cursor_xy[3] = (WORD)(cursor.g_y + cursor.g_h - 1);
                        vsf_color(vdi_handle, WHITE);
                        vr_recfl(vdi_handle, cursor_xy);

                        if (state->cursor_row < state->total_rows) {
                            line = line_at(state, state->cursor_row);
                            if (state->cursor_col <
                                line_length(state, state->cursor_row)) {
                                glyph[0] = line[state->cursor_col];
                            }
                        }
                        vst_color(vdi_handle, BLACK);
                        text_y = (WORD)(cursor.g_y + state->text_ascent);
                        v_gtext(vdi_handle, cursor.g_x, text_y,
                                (CONST BYTE *)glyph);
                        vst_color(vdi_handle, WHITE);
                    }
                }
            }
            vs_clip(vdi_handle, 0, clip_xy);
        }

        wind_get(state->handle, WF_NEXTXYWH, &box.g_x, &box.g_y, &box.g_w,
                 &box.g_h);
    }
    (void)vst_font(vdi_handle, previous_font);
    wind_update(END_UPDATE);
}
