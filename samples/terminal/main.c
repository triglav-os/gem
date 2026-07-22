/*
 * Hosts an interactive shell inside a GEM window using the Linux PTY
 * backend, running as a gemd RPC client so it can share the desktop
 * with other independent GEM processes (see samples/menu_demo). Ported
 * from samples/demo31: renders a simple scrollback terminal with a
 * blinking block cursor, forwards keyboard input to the shell, maps
 * the window scroll bars onto the terminal viewport, and installs its
 * own menu_bar() so the shared menu switches to it when its window is
 * topped.
 *
 * MIT License (see: LICENSE)
 * Copyright (C) 2026 tomaz stih
 */

#include <gem.h>
#include <gem/os.h>

#include <ctype.h>
#include <stdio.h>
#include <string.h>

enum {
    TERM_MAX_LINES = 2048,
    TERM_MAX_COLS = 512,
    TERM_MARGIN = 2,
    TERM_ROW_TIGHTEN = 4,
    TERM_TIMER_MS = 10,
    TERM_READ_BUF = 512
};

typedef struct term_state {
    WORD handle;
    WORD char_w;
    WORD char_h;
    WORD cell_w;
    WORD cell_h;
    WORD row_h;
    WORD text_ascent;
    WORD scroll_row;
    WORD scroll_col;
    WORD visible_rows;
    WORD visible_cols;
    WORD cursor_row;
    WORD cursor_col;
    WORD total_rows;
    WORD max_line_len;
    WORD auto_follow;
    WORD shell_alive;
    GRECT work;
    gem_os_pty_t pty;
    char *cells;
} term_state_t;

static WORD appl_id;
static VDI_HANDLE vdi_handle;
static WORD work_in[11] = {1,1,1,1,1,1,1,1,1,1,2};
static WORD work_out[57];

static char *line_at(term_state_t *state, WORD row)
{
    return state->cells + (size_t) row * (TERM_MAX_COLS + 1);
}

static WORD clamp_word(WORD value, WORD low, WORD high)
{
    if (value < low) {
        return low;
    }
    if (value > high) {
        return high;
    }
    return value;
}

static WORD max_offset(WORD total, WORD visible)
{
    if (visible >= total) {
        return 0;
    }
    return (WORD) (total - visible);
}

static WORD slider_size(WORD total, WORD visible)
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
    return (WORD) size;
}

static WORD slider_pos(WORD offset, WORD total, WORD visible)
{
    WORD limit = max_offset(total, visible);

    if (limit <= 0) {
        return 0;
    }
    return (WORD) ((1000L * offset) / limit);
}

static WORD offset_from_slider(WORD slider, WORD total, WORD visible)
{
    WORD limit = max_offset(total, visible);

    slider = clamp_word(slider, 0, 1000);
    if (limit <= 0) {
        return 0;
    }
    return (WORD) ((slider * (LONG) limit + 500L) / 1000L);
}

static void clear_line(term_state_t *state, WORD row)
{
    char *line = line_at(state, row);

    memset(line, 0, (size_t) TERM_MAX_COLS + 1u);
}

static void scroll_lines_up(term_state_t *state)
{
    size_t line_size = (size_t) TERM_MAX_COLS + 1u;

    memmove(state->cells, state->cells + line_size,
        line_size * (TERM_MAX_LINES - 1));
    memset(state->cells + line_size * (TERM_MAX_LINES - 1), 0, line_size);
    if (state->cursor_row > 0) {
        --state->cursor_row;
    }
    if (state->total_rows > 0) {
        --state->total_rows;
    }
    if (state->scroll_row > 0) {
        --state->scroll_row;
    }
}

static void ensure_row(term_state_t *state, WORD row)
{
    WORD i;

    while (row >= TERM_MAX_LINES) {
        scroll_lines_up(state);
        --row;
    }
    if (state->total_rows <= row) {
        for (i = state->total_rows; i <= row; ++i) {
            clear_line(state, i);
        }
        state->total_rows = (WORD) (row + 1);
    }
}

static void recompute_max_line_len(term_state_t *state)
{
    WORD row;
    WORD best = 0;

    for (row = 0; row < state->total_rows; ++row) {
        WORD len = (WORD) strlen(line_at(state, row));

        if (len > best) {
            best = len;
        }
    }
    state->max_line_len = best;
}

static void set_char_at_cursor(term_state_t *state, char ch)
{
    char *line;
    WORD len;
    WORD i;

    if (state->cursor_col >= TERM_MAX_COLS) {
        state->cursor_col = 0;
        ++state->cursor_row;
        ensure_row(state, state->cursor_row);
    }

    ensure_row(state, state->cursor_row);
    line = line_at(state, state->cursor_row);
    len = (WORD) strlen(line);
    if (state->cursor_col > len) {
        for (i = len; i < state->cursor_col; ++i) {
            line[i] = ' ';
        }
        line[state->cursor_col] = '\0';
    }
    line[state->cursor_col] = ch;
    if (state->cursor_col >= len) {
        line[state->cursor_col + 1] = '\0';
    }
    if ((WORD) strlen(line) > state->max_line_len) {
        state->max_line_len = (WORD) strlen(line);
    }
    ++state->cursor_col;
}

static void newline(term_state_t *state)
{
    state->cursor_col = 0;
    ++state->cursor_row;
    ensure_row(state, state->cursor_row);
}

static void process_shell_byte(term_state_t *state, unsigned char byte)
{
    static WORD esc_state;

    if (esc_state == 1) {
        if (byte == '[') {
            esc_state = 2;
        } else {
            esc_state = 0;
        }
        return;
    }
    if (esc_state == 2) {
        if ((byte >= '@' && byte <= '~') || isalpha(byte)) {
            esc_state = 0;
        }
        return;
    }

    switch (byte) {
    case 0x1b:
        esc_state = 1;
        break;
    case '\r':
        state->cursor_col = 0;
        break;
    case '\n':
        newline(state);
        break;
    case '\b':
        if (state->cursor_col > 0) {
            --state->cursor_col;
        }
        break;
    case '\t':
        do {
            set_char_at_cursor(state, ' ');
        } while ((state->cursor_col & 7) != 0);
        break;
    default:
        if (byte >= 32 && byte <= 126) {
            set_char_at_cursor(state, (char) byte);
        }
        break;
    }
}

static void sync_layout(term_state_t *state)
{
    wind_get(state->handle, WF_WORKXYWH,
        &state->work.g_x, &state->work.g_y,
        &state->work.g_w, &state->work.g_h);

    state->visible_rows = (WORD) ((state->work.g_h - 2 * TERM_MARGIN) /
        state->row_h);
    state->visible_cols = (WORD) ((state->work.g_w - 2 * TERM_MARGIN) /
        state->cell_w);
    if (state->visible_rows < 1) {
        state->visible_rows = 1;
    }
    if (state->visible_cols < 1) {
        state->visible_cols = 1;
    }

    state->scroll_row = clamp_word(state->scroll_row, 0,
        max_offset(state->total_rows, state->visible_rows));
    state->scroll_col = clamp_word(state->scroll_col, 0,
        max_offset(state->max_line_len, state->visible_cols));
}

static void update_window_controls(term_state_t *state)
{
    wind_set_str(state->handle, WF_NAME, "Terminal");
    wind_set(state->handle, WF_VSLSIZ,
        slider_size(state->total_rows, state->visible_rows), 0, 0, 0);
    wind_set(state->handle, WF_HSLSIZ,
        slider_size(state->max_line_len, state->visible_cols), 0, 0, 0);
    wind_set(state->handle, WF_VSLIDE,
        slider_pos(state->scroll_row, state->total_rows, state->visible_rows),
        0, 0, 0);
    wind_set(state->handle, WF_HSLIDE,
        slider_pos(state->scroll_col, state->max_line_len, state->visible_cols),
        0, 0, 0);
}

static int cursor_rect_for_position(term_state_t *state, WORD row,
                                    WORD col, GRECT *rect)
{
    WORD cell_row;
    WORD cell_col;

    if (row < state->scroll_row ||
        row >= state->scroll_row + state->visible_rows ||
        col < state->scroll_col ||
        col >= state->scroll_col + state->visible_cols) {
        return 0;
    }

    cell_row = (WORD) (row - state->scroll_row);
    cell_col = (WORD) (col - state->scroll_col);
    rect->g_x = (WORD) (state->work.g_x + TERM_MARGIN +
        cell_col * state->cell_w);
    rect->g_y = (WORD) (state->work.g_y + TERM_MARGIN +
        cell_row * state->row_h);
    rect->g_w = state->cell_w;
    rect->g_h = state->row_h;
    return 1;
}

static int cursor_rect(term_state_t *state, GRECT *rect)
{
    return cursor_rect_for_position(state, state->cursor_row,
        state->cursor_col, rect);
}

static int row_range_dirty_rect(term_state_t *state, WORD first_row,
                                WORD last_row, GRECT *rect)
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
    bottom_row = (last_row <
        (WORD) (state->scroll_row + state->visible_rows)) ?
        last_row : (WORD) (state->scroll_row + state->visible_rows);
    if (bottom_row < top_row) {
        return 0;
    }

    y0 = (WORD) (state->work.g_y + TERM_MARGIN +
        (top_row - state->scroll_row) * state->row_h);
    y1 = (WORD) (state->work.g_y + TERM_MARGIN +
        (bottom_row - state->scroll_row + 1) * state->row_h - 1);
    if (y1 > state->work.g_y + state->work.g_h - 1) {
        y1 = (WORD) (state->work.g_y + state->work.g_h - 1);
    }
    rect->g_x = state->work.g_x;
    rect->g_y = y0;
    rect->g_w = state->work.g_w;
    rect->g_h = (WORD) (y1 - y0 + 1);
    return rect->g_h > 0;
}

static void union_dirty(GRECT *target, const GRECT *add)
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
    x1 = ((WORD) (target->g_x + target->g_w - 1) >
        (WORD) (add->g_x + add->g_w - 1)) ?
        (WORD) (target->g_x + target->g_w - 1) :
        (WORD) (add->g_x + add->g_w - 1);
    y1 = ((WORD) (target->g_y + target->g_h - 1) >
        (WORD) (add->g_y + add->g_h - 1)) ?
        (WORD) (target->g_y + target->g_h - 1) :
        (WORD) (add->g_y + add->g_h - 1);
    target->g_x = x0;
    target->g_y = y0;
    target->g_w = (WORD) (x1 - x0 + 1);
    target->g_h = (WORD) (y1 - y0 + 1);
}

static void draw_terminal(term_state_t *state, const GRECT *dirty)
{
    GRECT box;
    WORD previous_font;

    sync_layout(state);
    update_window_controls(state);

    wind_update(BEG_UPDATE);
    previous_font = vst_font(vdi_handle, ATARI);
    wind_get(state->handle, WF_FIRSTXYWH,
        &box.g_x, &box.g_y, &box.g_w, &box.g_h);
    while (box.g_w > 0 && box.g_h > 0) {
        WORD x0 = box.g_x;
        WORD y0 = box.g_y;
        WORD x1 = (WORD) (box.g_x + box.g_w - 1);
        WORD y1 = (WORD) (box.g_y + box.g_h - 1);
        WORD clip_xy[4];
        WORD fill_xy[4];
        WORD row;

        if (dirty != NULL) {
            WORD dx1 = (WORD) (dirty->g_x + dirty->g_w - 1);
            WORD dy1 = (WORD) (dirty->g_y + dirty->g_h - 1);

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
            (void) vswr_mode(vdi_handle, MD_REPLACE);
            (void) vsf_interior(vdi_handle, FIS_SOLID);
            (void) vsf_style(vdi_handle, 1);

            fill_xy[0] = state->work.g_x;
            fill_xy[1] = state->work.g_y;
            fill_xy[2] = (WORD) (state->work.g_x + state->work.g_w - 1);
            fill_xy[3] = (WORD) (state->work.g_y + state->work.g_h - 1);
            /*
             * The VDI color-index-to-pixel mapping is inverted (see
             * _vdi_color_to_pixel), so BLACK paints white pixels and
             * WHITE paints black ones -- matching how the AES
             * engine's own _aes_light_color()/_aes_dark_color() do.
             */
            vsf_color(vdi_handle, BLACK);
            vr_recfl(vdi_handle, fill_xy);

            vst_color(vdi_handle, WHITE);
            for (row = 0; row <= state->visible_rows; ++row) {
                WORD logical_row = (WORD) (state->scroll_row + row);
                WORD row_top = (WORD) (state->work.g_y + TERM_MARGIN +
                    row * state->row_h);
                WORD row_bottom = (WORD) (row_top + state->row_h - 1);
                WORD text_y;
                char view[TERM_MAX_COLS + 1];
                char *line;
                WORD len;

                if (dirty != NULL &&
                    (row_bottom < dirty->g_y ||
                    row_top > dirty->g_y + dirty->g_h - 1)) {
                    continue;
                }
                if (logical_row >= state->total_rows) {
                    break;
                }
                line = line_at(state, logical_row);
                len = (WORD) strlen(line);
                if (state->scroll_col >= len) {
                    view[0] = '\0';
                } else {
                    strncpy(view, line + state->scroll_col,
                        (size_t) state->visible_cols);
                    view[state->visible_cols] = '\0';
                }

                text_y = (WORD) (state->work.g_y + TERM_MARGIN +
                    row * state->row_h + state->text_ascent);
                v_gtext(vdi_handle,
                    (WORD) (state->work.g_x + TERM_MARGIN),
                    text_y,
                    (CONST BYTE *) view);
            }

            {
                GRECT cursor;

                if (cursor_rect(state, &cursor) != 0) {
                    WORD dirty_x1 = (dirty != NULL) ?
                        (WORD) (dirty->g_x + dirty->g_w - 1) : 0;
                    WORD dirty_y1 = (dirty != NULL) ?
                        (WORD) (dirty->g_y + dirty->g_h - 1) : 0;

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
                        cursor_xy[2] = (WORD) (cursor.g_x + cursor.g_w - 1);
                        cursor_xy[3] = (WORD) (cursor.g_y + cursor.g_h - 1);
                        vsf_color(vdi_handle, WHITE);
                        vr_recfl(vdi_handle, cursor_xy);

                        if (state->cursor_row < state->total_rows) {
                            line = line_at(state, state->cursor_row);
                            if (state->cursor_col < (WORD) strlen(line)) {
                                glyph[0] = line[state->cursor_col];
                            }
                        }
                        vst_color(vdi_handle, BLACK);
                        text_y = (WORD) (cursor.g_y + state->text_ascent);
                        v_gtext(vdi_handle, cursor.g_x, text_y,
                            (CONST BYTE *) glyph);
                        vst_color(vdi_handle, WHITE);
                    }
                }
            }
            vs_clip(vdi_handle, 0, clip_xy);
        }

        wind_get(state->handle, WF_NEXTXYWH,
            &box.g_x, &box.g_y, &box.g_w, &box.g_h);
    }
    (void) vst_font(vdi_handle, previous_font);
    wind_update(END_UPDATE);
}

static void follow_bottom(term_state_t *state)
{
    state->scroll_row = max_offset(state->total_rows, state->visible_rows);
    state->scroll_col = 0;
}

static int process_shell_output(term_state_t *state, GRECT *dirty)
{
    char buf[TERM_READ_BUF];
    int32_t count;
    int i;
    int changed = 0;
    WORD first_changed_row;
    WORD last_changed_row;
    WORD old_cursor_row;
    WORD was_at_bottom;
    GRECT rect;
    GRECT cursor_dirty;

    sync_layout(state);
    was_at_bottom = (state->scroll_row >=
        max_offset(state->total_rows, state->visible_rows)) ? 1 : 0;
    old_cursor_row = state->cursor_row;
    first_changed_row = state->cursor_row;
    last_changed_row = state->cursor_row;
    if (dirty != NULL) {
        dirty->g_w = 0;
        dirty->g_h = 0;
    }

    for (;;) {
        count = gem_os_pty_read(&state->pty, buf, sizeof(buf));
        if (count <= 0) {
            break;
        }
        changed = 1;
        for (i = 0; i < count; ++i) {
            process_shell_byte(state, (unsigned char) buf[i]);
            if (state->cursor_row < first_changed_row) {
                first_changed_row = state->cursor_row;
            }
            if (state->cursor_row > last_changed_row) {
                last_changed_row = state->cursor_row;
            }
        }
    }

    if (changed == 0) {
        return 0;
    }

    recompute_max_line_len(state);
    if (state->auto_follow != 0 || was_at_bottom != 0) {
        follow_bottom(state);
        state->auto_follow = 1;
        if (dirty != NULL) {
            *dirty = state->work;
        }
        return 1;
    }

    if (dirty != NULL) {
        if (old_cursor_row < first_changed_row) {
            first_changed_row = old_cursor_row;
        }
        if (state->cursor_row > last_changed_row) {
            last_changed_row = state->cursor_row;
        }
        if (row_range_dirty_rect(state, first_changed_row, last_changed_row,
                &rect) != 0) {
            union_dirty(dirty, &rect);
        }
        if (cursor_rect(state, &cursor_dirty) != 0) {
            union_dirty(dirty, &cursor_dirty);
        }
    }
    return 1;
}

static void send_key(term_state_t *state, WORD key)
{
    unsigned char ch;

    if (state->shell_alive == 0) {
        return;
    }

    switch (key & 0xff) {
    case 8:
    case 127:
        ch = 127;
        break;
    case 13:
        ch = '\n';
        break;
    case 27:
        ch = 27;
        break;
    default:
        ch = (unsigned char) (key & 0xff);
        break;
    }

    (void) gem_os_pty_write(&state->pty, &ch, 1);
}

static void handle_arrow(term_state_t *state, WORD arrow_code)
{
    WORD row_page = (WORD) (state->visible_rows - 1);
    WORD col_page = (WORD) (state->visible_cols - 4);

    if (row_page < 1) {
        row_page = 1;
    }
    if (col_page < 4) {
        col_page = 4;
    }

    switch (arrow_code) {
    case WA_UPPAGE:
        state->scroll_row = clamp_word((WORD) (state->scroll_row - row_page), 0,
            max_offset(state->total_rows, state->visible_rows));
        break;
    case WA_DNPAGE:
        state->scroll_row = clamp_word((WORD) (state->scroll_row + row_page), 0,
            max_offset(state->total_rows, state->visible_rows));
        break;
    case WA_UPLINE:
        state->scroll_row = clamp_word((WORD) (state->scroll_row - 1), 0,
            max_offset(state->total_rows, state->visible_rows));
        break;
    case WA_DNLINE:
        state->scroll_row = clamp_word((WORD) (state->scroll_row + 1), 0,
            max_offset(state->total_rows, state->visible_rows));
        break;
    case WA_LFPAGE:
        state->scroll_col = clamp_word((WORD) (state->scroll_col - col_page), 0,
            max_offset(state->max_line_len, state->visible_cols));
        break;
    case WA_RTPAGE:
        state->scroll_col = clamp_word((WORD) (state->scroll_col + col_page), 0,
            max_offset(state->max_line_len, state->visible_cols));
        break;
    case WA_LFLINE:
        state->scroll_col = clamp_word((WORD) (state->scroll_col - 1), 0,
            max_offset(state->max_line_len, state->visible_cols));
        break;
    case WA_RTLINE:
        state->scroll_col = clamp_word((WORD) (state->scroll_col + 1), 0,
            max_offset(state->max_line_len, state->visible_cols));
        break;
    default:
        break;
    }
    state->auto_follow = 0;
}

static int init_terminal(term_state_t *state)
{
    char cwd[GEM_OS_PATH_MAX];
    WORD extent[8];

    memset(state, 0, sizeof(*state));
    state->cells = gem_os_alloc((size_t) TERM_MAX_LINES *
        (TERM_MAX_COLS + 1u));
    if (state->cells == NULL) {
        return 0;
    }
    memset(state->cells, 0, (size_t) TERM_MAX_LINES *
        (TERM_MAX_COLS + 1u));

    vdi_handle = graf_handle(&state->char_w, &state->char_h, NULL, NULL);
    v_opnvwk(work_in, &vdi_handle, work_out);
    (void) vst_font(vdi_handle, ATARI);
    if (vqt_extent(vdi_handle, "M", extent) != 0) {
        state->cell_w = (WORD) (extent[2] - extent[0] + 1);
        state->cell_h = (WORD) (extent[5] - extent[1] + 1);
    }
    if (state->cell_w <= 0) {
        state->cell_w = (state->char_w > 0) ? state->char_w : 8;
    }
    if (state->cell_h <= 0) {
        state->cell_h = (state->char_h > 0) ? state->char_h : 16;
    }
    state->text_ascent = (WORD) (state->cell_h - 2);
    if (state->text_ascent <= 0) {
        state->text_ascent = state->cell_h;
    }
    state->row_h = (WORD) (state->cell_h + 2);
    if (state->row_h < state->text_ascent) {
        state->row_h = state->text_ascent;
    }

    state->total_rows = 1;
    state->auto_follow = 1;
    state->shell_alive = 1;
    clear_line(state, 0);

    if (!gem_os_getcwd(cwd, sizeof(cwd))) {
        cwd[0] = '\0';
    }
    if (!gem_os_pty_spawn_shell(&state->pty, NULL, cwd, 80, 25)) {
        gem_os_free(state->cells);
        state->cells = NULL;
        return 0;
    }
    return 1;
}

static void shutdown_terminal(term_state_t *state)
{
    gem_os_pty_close(&state->pty);
    if (state->cells != NULL) {
        gem_os_free(state->cells);
        state->cells = NULL;
    }
}

int main(void)
{
    term_state_t state;
    GRECT desk;
    GRECT outer;
    GRECT dirty;
    WORD msg[8];
    WORD done = 0;
    WORD work_x;
    WORD work_y;
    WORD work_w = 560;
    WORD work_h = 320;

    appl_id = appl_init();
    if (appl_id <= 0) {
        fprintf(stderr, "terminal: appl_init failed -- is gemd running?\n");
        return 1;
    }
    if (!init_terminal(&state)) {
        appl_exit();
        return 1;
    }

    wind_get(0, WF_WORKXYWH, &desk.g_x, &desk.g_y, &desk.g_w, &desk.g_h);
    work_x = (WORD) (desk.g_x + 36);
    work_y = (WORD) (desk.g_y + 28);
    (void) wind_calc(WC_BORDER, NAME | CLOSER | MOVER | SIZER |
        UPARROW | DNARROW | VSLIDE | LFARROW | RTARROW | HSLIDE,
        work_x, work_y, work_w, work_h,
        &outer.g_x, &outer.g_y, &outer.g_w, &outer.g_h);
    state.handle = wind_create(NAME | CLOSER | MOVER | SIZER |
        UPARROW | DNARROW | VSLIDE | LFARROW | RTARROW | HSLIDE,
        0, 0, desk.g_w, desk.g_h);
    if (state.handle <= 0) {
        shutdown_terminal(&state);
        v_clsvwk(vdi_handle);
        appl_exit();
        return 1;
    }

    wind_open(state.handle, outer.g_x, outer.g_y, outer.g_w, outer.g_h);
    (void) process_shell_output(&state, NULL);
    sync_layout(&state);
    (void) gem_os_pty_resize(&state.pty, (uint16_t) state.visible_cols,
        (uint16_t) state.visible_rows);
    draw_terminal(&state, NULL);

    printf("terminal: window %d open, shell spawned.\n",
        state.handle);
    fflush(stdout);

    while (done == 0) {
        WORD event;
        WORD mx = 0;
        WORD my = 0;
        WORD mb = 0;
        WORD ks = 0;
        WORD kr = 0;
        WORD br = 0;
        int redraw_full = 0;
        event = evnt_multi(MU_MESAG | MU_KEYBD | MU_TIMER,
            0, 0, 0,
            0, 0, 0, 0, 0,
            0, 0, 0, 0, 0,
            msg, TERM_TIMER_MS, 0, &mx, &my, &mb, &ks, &kr, &br);

        (void) mx;
        (void) my;
        (void) mb;
        (void) ks;
        (void) br;

        if (process_shell_output(&state, &dirty) != 0) {
            draw_terminal(&state, &dirty);
        }
        if (!gem_os_pty_is_alive(&state.pty)) {
            state.shell_alive = 0;
        }

        if ((event & MU_KEYBD) != 0) {
            if ((kr & 0xff) == 27 && state.shell_alive == 0) {
                done = 1;
            } else {
                send_key(&state, kr);
                if (process_shell_output(&state, &dirty) != 0) {
                    draw_terminal(&state, &dirty);
                }
            }
        }

        if ((event & MU_MESAG) != 0) {
            switch (msg[0]) {
            case WM_REDRAW:
            {
                GRECT redraw_dirty;

                redraw_dirty.g_x = msg[4];
                redraw_dirty.g_y = msg[5];
                redraw_dirty.g_w = msg[6];
                redraw_dirty.g_h = msg[7];
                draw_terminal(&state, &redraw_dirty);
                break;
            }
            case WM_TOPPED:
                wind_set(state.handle, WF_TOP, 0, 0, 0, 0);
                break;
            case WM_CLOSED:
                done = 1;
                break;
            case WM_MOVED:
            case WM_SIZED:
                wind_set(state.handle, WF_CURRXYWH,
                    msg[4], msg[5], msg[6], msg[7]);
                sync_layout(&state);
                (void) gem_os_pty_resize(&state.pty,
                    (uint16_t) state.visible_cols,
                    (uint16_t) state.visible_rows);
                if (state.auto_follow != 0) {
                    follow_bottom(&state);
                }
                redraw_full = 1;
                break;
            case WM_ARROWED:
                handle_arrow(&state, msg[4]);
                redraw_full = 1;
                break;
            case WM_VSLID:
                state.scroll_row = offset_from_slider(msg[4],
                    state.total_rows, state.visible_rows);
                state.auto_follow = 0;
                redraw_full = 1;
                break;
            case WM_HSLID:
                state.scroll_col = offset_from_slider(msg[4],
                    state.max_line_len, state.visible_cols);
                state.auto_follow = 0;
                redraw_full = 1;
                break;
            default:
                break;
            }
        }

        if (redraw_full != 0) {
            draw_terminal(&state, NULL);
        }

        if (state.shell_alive == 0 && done == 0) {
            done = 1;
        }
    }

    wind_close(state.handle);
    wind_delete(state.handle);
    shutdown_terminal(&state);
    v_clsvwk(vdi_handle);
    appl_exit();
    printf("terminal: exiting\n");
    return 0;
}
