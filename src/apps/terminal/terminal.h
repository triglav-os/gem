/*
 * Declares the Terminal application's screen model and helpers shared by
 * its grid, drawing and shell modules. Private to the sample.
 *
 * MIT License (see: LICENSE)
 * Copyright (C) 2026 tomaz stih
 */

#ifndef GEM_SAMPLE_TERMINAL_H
#define GEM_SAMPLE_TERMINAL_H

#include <gem.h>
#include <gem/os.h>

#include <stddef.h>
#include <stdint.h>

enum {
    TERM_MAX_LINES = 2048,
    TERM_MAX_COLS = 512,
    TERM_MARGIN = 2,
    TERM_ROW_TIGHTEN = 4,
    TERM_TIMER_MS = 10,
    TERM_READ_BUF = 512,
    TERM_CSI_PARAMS = 16,
    TERM_INITIAL_ROWS = 25,
    TERM_INITIAL_COLS = 80
};

enum {
    TERM_ATTR_BOLD = 1,
    TERM_ATTR_UNDERLINE = 2,
    TERM_ATTR_REVERSE = 4
};

enum {
    TERM_PARSE_GROUND = 0,
    TERM_PARSE_ESCAPE,
    TERM_PARSE_CSI,
    TERM_PARSE_OSC,
    TERM_PARSE_OSC_ESCAPE,
    TERM_PARSE_CHARSET,
    TERM_PARSE_CONTROL_STRING,
    TERM_PARSE_CONTROL_STRING_ESCAPE,
    TERM_PARSE_ESCAPE_INTERMEDIATE
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
    WORD screen_top;
    WORD screen_rows;
    WORD screen_cols;
    WORD scroll_top;
    WORD scroll_bottom;
    WORD total_rows;
    WORD max_line_len;
    WORD auto_follow;
    WORD shell_alive;
    WORD console_app_id;
    WORD cursor_visible;
    WORD auto_wrap;
    WORD origin_mode;
    WORD insert_mode;
    WORD application_cursor_keys;
    WORD wrap_pending;
    WORD parser_state;
    WORD charset_target;
    WORD g0_special;
    WORD g1_special;
    WORD active_charset;
    WORD csi_private;
    WORD csi_count;
    WORD csi_params[TERM_CSI_PARAMS];
    WORD saved_cursor_row;
    WORD saved_cursor_col;
    WORD saved_attr;
    WORD saved_g0_special;
    WORD saved_g1_special;
    WORD saved_active_charset;
    WORD alternate_active;
    WORD primary_cursor_row;
    WORD primary_cursor_col;
    WORD primary_screen_top;
    WORD primary_total_rows;
    WORD primary_max_line_len;
    WORD primary_scroll_row;
    WORD primary_scroll_col;
    WORD primary_scroll_top;
    WORD primary_scroll_bottom;
    WORD primary_current_attr;
    WORD primary_cursor_visible;
    WORD primary_auto_wrap;
    WORD primary_origin_mode;
    WORD primary_insert_mode;
    WORD primary_application_cursor_keys;
    WORD primary_g0_special;
    WORD primary_g1_special;
    WORD primary_active_charset;
    WORD screen_dirty;
    unsigned char current_attr;
    unsigned char last_printed;
    WORD utf8_remaining;
    uint32_t utf8_codepoint;
    unsigned char tab_stops[TERM_MAX_COLS];
    GRECT work;
    gem_os_pty_t pty;
    char *cells;
    unsigned char *attrs;
    char *primary_cells;
    unsigned char *primary_attrs;
    char *alternate_cells;
    unsigned char *alternate_attrs;
} term_state_t;

extern VDI_HANDLE vdi_handle;

/* Clamp a value into [low, high]. */
WORD clamp_word(WORD value, WORD low, WORD high);
/* Blank one row. */
void clear_line(term_state_t *state, WORD row);
/* Screen rectangle of the cursor cell. */
int cursor_rect(term_state_t *state, GRECT *rect);
/* Repaint the grid and cursor within a rectangle. */
void draw_terminal(term_state_t *state, const GRECT *dirty);
/* Scroll so the newest row is visible. */
void follow_bottom(term_state_t *state);
/* Pointer to the cells of a scrollback row. */
char *line_at(term_state_t *state, WORD row);
/* Pointer to the attributes of a scrollback row. */
unsigned char *attr_line_at(term_state_t *state, WORD row);
/* Visible content length of a row, including attributed blank cells. */
WORD line_length(term_state_t *state, WORD row);
/* Largest scroll offset for total rows and visible rows. */
WORD max_offset(WORD total, WORD visible);
/* Scroll offset for a GEM slider position. */
WORD offset_from_slider(WORD slider, WORD total, WORD visible);
/* Apply one byte of shell output to the grid. */
void process_shell_byte(term_state_t *state, unsigned char byte);
/* Recompute the widest line for horizontal scrolling. */
void recompute_max_line_len(term_state_t *state);
/* Damage rectangle covering a row range. */
int row_range_dirty_rect(term_state_t *state, WORD first_row, WORD last_row,
                         GRECT *rect);
/* GEM slider position (0..1000) for an offset. */
WORD slider_pos(WORD offset, WORD total, WORD visible);
/* GEM slider size (1..1000) for a viewport. */
WORD slider_size(WORD total, WORD visible);
/* Recompute rows/columns from the work area. */
void sync_layout(term_state_t *state);
/* Initialize VT100 modes, parser state, margins and tab stops. */
void term_vt100_init(term_state_t *state);
/* Update VT100 screen dimensions after a GEM window resize. */
void term_vt100_resize(term_state_t *state, WORD rows, WORD columns);
/* Grow a damage rectangle to include another. */
void union_dirty(GRECT *target, const GRECT *add);

#endif
