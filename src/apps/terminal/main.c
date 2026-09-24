/*
 * Hosts an interactive shell inside a GEM window using the platform PTY
 * abstraction, running as a gemd RPC client so it can share the desktop with
 * other GEM processes. This module owns the shell process, keyboard
 * forwarding, scroll bar messages and the event loop; the character grid
 * lives in screen.c and painting in draw.c.
 *
 * MIT License (see: LICENSE)
 * Copyright (C) 2026 tomaz stih
 */

#include "terminal.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static WORD appl_id;
VDI_HANDLE vdi_handle;
static WORD work_in[11] = {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 2};
static WORD work_out[57];

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
                     max_offset(state->total_rows, state->visible_rows))
                        ? 1
                        : 0;
    old_cursor_row = state->cursor_row;
    first_changed_row = state->cursor_row;
    last_changed_row = state->cursor_row;
    if (dirty != NULL) {
        dirty->g_w = 0;
        dirty->g_h = 0;
    }
    state->screen_dirty = 0;

    for (;;) {
        count = gem_os_pty_read(&state->pty, buf, sizeof(buf));
        if (count <= 0) {
            break;
        }
        changed = 1;
        for (i = 0; i < count; ++i) {
            process_shell_byte(state, (unsigned char)buf[i]);
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
    if (state->screen_dirty != 0) {
        if (state->auto_follow != 0 || was_at_bottom != 0) {
            follow_bottom(state);
            state->auto_follow = 1;
        }
        if (dirty != NULL) {
            *dirty = state->work;
        }
        return 1;
    }
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

static void send_bytes(term_state_t *state, const char *bytes)
{
    (void)gem_os_pty_write(&state->pty, bytes, (uint32_t)strlen(bytes));
}

static void send_console_key(term_state_t *state, WORD key, WORD modifiers)
{
    unsigned char character = (unsigned char)(key & 0xff);
    WORD message[8];

    if (state->console_app_id <= 0 || character == 0) return;
    if ((modifiers & K_CTRL) != 0 && character >= '@' && character <= 127)
        character = (unsigned char)(character & 0x1f);
    message[0] = GEM_MSG_CONSOLE_INPUT;
    message[1] = character;
    memset(message + 2, 0, sizeof(message) - 4u);
    (void)appl_write(state->console_app_id, 8, message);
}

static int send_special_key(term_state_t *state, WORD scan, WORD modifiers)
{
    const char *sequence = NULL;

    if (scan == 43 &&
        (modifiers & (K_LSHIFT | K_RSHIFT)) != 0) {
        sequence = "\033[Z";
    } else {
        switch (scan) {
            case 82:
                sequence = state->application_cursor_keys != 0 ? "\033OA"
                                                                : "\033[A";
                break;
            case 81:
                sequence = state->application_cursor_keys != 0 ? "\033OB"
                                                                : "\033[B";
                break;
            case 79:
                sequence = state->application_cursor_keys != 0 ? "\033OC"
                                                                : "\033[C";
                break;
            case 80:
                sequence = state->application_cursor_keys != 0 ? "\033OD"
                                                                : "\033[D";
                break;
            case 74:
                sequence = "\033[H";
                break;
            case 77:
                sequence = "\033[F";
                break;
            case 73:
                sequence = "\033[2~";
                break;
            case 76:
                sequence = "\033[3~";
                break;
            case 75:
                sequence = "\033[5~";
                break;
            case 78:
                sequence = "\033[6~";
                break;
            case 58:
                sequence = "\033OP";
                break;
            case 59:
                sequence = "\033OQ";
                break;
            case 60:
                sequence = "\033OR";
                break;
            case 61:
                sequence = "\033OS";
                break;
            case 62:
                sequence = "\033[15~";
                break;
            case 63:
                sequence = "\033[17~";
                break;
            case 64:
                sequence = "\033[18~";
                break;
            case 65:
                sequence = "\033[19~";
                break;
            case 66:
                sequence = "\033[20~";
                break;
            case 67:
                sequence = "\033[21~";
                break;
            case 68:
                sequence = "\033[23~";
                break;
            case 69:
                sequence = "\033[24~";
                break;
            default:
                break;
        }
    }
    if (sequence == NULL) {
        return 0;
    }
    send_bytes(state, sequence);
    return 1;
}

static void send_key(term_state_t *state, WORD key, WORD modifiers)
{
    unsigned char ch;
    WORD scan = (WORD)((key >> 8) & 0xff);

    if (state->shell_alive == 0) {
        return;
    }
    if (send_special_key(state, scan, modifiers) != 0) {
        return;
    }
    /* Modifier-only events have no character; never inject a NUL for them.
     * HID backends provide Control separately from the printable character. */
    if ((key & 0xff) == 0) {
        return;
    }
    if ((modifiers & K_CTRL) != 0 && (key & 0xff) >= '@' &&
        (key & 0xff) <= 127) {
        ch = (unsigned char)(key & 0x1f);
        (void)gem_os_pty_write(&state->pty, &ch, 1u);
        return;
    }

    switch (key & 0xff) {
        case 8:
        case 127:
            ch = 127;
            break;
        case 10:
        case 13:
            ch = '\r';
            break;
        case 27:
            ch = 27;
            break;
        default:
            ch = (unsigned char)(key & 0xff);
            break;
    }

    if ((modifiers & K_ALT) != 0) {
        send_bytes(state, "\033");
    }
    (void)gem_os_pty_write(&state->pty, &ch, 1u);
}

static void handle_arrow(term_state_t *state, WORD arrow_code)
{
    WORD row_page = (WORD)(state->visible_rows - 1);
    WORD col_page = (WORD)(state->visible_cols - 4);

    if (row_page < 1) {
        row_page = 1;
    }
    if (col_page < 4) {
        col_page = 4;
    }

    switch (arrow_code) {
        case WA_UPPAGE:
            state->scroll_row =
                clamp_word((WORD)(state->scroll_row - row_page), 0,
                           max_offset(state->total_rows, state->visible_rows));
            break;
        case WA_DNPAGE:
            state->scroll_row =
                clamp_word((WORD)(state->scroll_row + row_page), 0,
                           max_offset(state->total_rows, state->visible_rows));
            break;
        case WA_UPLINE:
            state->scroll_row =
                clamp_word((WORD)(state->scroll_row - 1), 0,
                           max_offset(state->total_rows, state->visible_rows));
            break;
        case WA_DNLINE:
            state->scroll_row =
                clamp_word((WORD)(state->scroll_row + 1), 0,
                           max_offset(state->total_rows, state->visible_rows));
            break;
        case WA_LFPAGE:
            state->scroll_col = clamp_word(
                (WORD)(state->scroll_col - col_page), 0,
                max_offset(state->max_line_len, state->visible_cols));
            break;
        case WA_RTPAGE:
            state->scroll_col = clamp_word(
                (WORD)(state->scroll_col + col_page), 0,
                max_offset(state->max_line_len, state->visible_cols));
            break;
        case WA_LFLINE:
            state->scroll_col = clamp_word(
                (WORD)(state->scroll_col - 1), 0,
                max_offset(state->max_line_len, state->visible_cols));
            break;
        case WA_RTLINE:
            state->scroll_col = clamp_word(
                (WORD)(state->scroll_col + 1), 0,
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
    const char *shell;
    size_t cell_bytes = (size_t)TERM_MAX_LINES * (TERM_MAX_COLS + 1u);
    size_t attr_bytes = (size_t)TERM_MAX_LINES * TERM_MAX_COLS;
    WORD extent[8];

    memset(state, 0, sizeof(*state));
    state->primary_cells = gem_os_alloc(cell_bytes);
    state->primary_attrs = gem_os_alloc(attr_bytes);
    state->alternate_cells = gem_os_alloc(cell_bytes);
    state->alternate_attrs = gem_os_alloc(attr_bytes);
    if (state->primary_cells == NULL || state->primary_attrs == NULL ||
        state->alternate_cells == NULL || state->alternate_attrs == NULL) {
        gem_os_free(state->primary_cells);
        gem_os_free(state->primary_attrs);
        gem_os_free(state->alternate_cells);
        gem_os_free(state->alternate_attrs);
        return 0;
    }
    state->cells = state->primary_cells;
    state->attrs = state->primary_attrs;
    memset(state->primary_cells, 0, cell_bytes);
    memset(state->primary_attrs, 0, attr_bytes);
    memset(state->alternate_cells, 0, cell_bytes);
    memset(state->alternate_attrs, 0, attr_bytes);
    term_vt100_init(state);

    vdi_handle = graf_handle(&state->char_w, &state->char_h, NULL, NULL);
    v_opnvwk(work_in, &vdi_handle, work_out);
    (void)vst_font(vdi_handle, ATARI);
    if (vqt_extent(vdi_handle, "M", extent) != 0) {
        state->cell_w = (WORD)(extent[2] - extent[0] + 1);
        state->cell_h = (WORD)(extent[5] - extent[1] + 1);
    }
    if (state->cell_w <= 0) {
        state->cell_w = (state->char_w > 0) ? state->char_w : 8;
    }
    if (state->cell_h <= 0) {
        state->cell_h = (state->char_h > 0) ? state->char_h : 16;
    }
    state->text_ascent = (WORD)(state->cell_h - 2);
    if (state->text_ascent <= 0) {
        state->text_ascent = state->cell_h;
    }
    state->row_h = (WORD)(state->cell_h + 2);
    if (state->row_h < state->text_ascent) {
        state->row_h = state->text_ascent;
    }

    state->auto_follow = 1;
    state->shell_alive = 1;

    if (!gem_os_getcwd(cwd, sizeof(cwd))) {
        cwd[0] = '\0';
    }
    /* Interactive bash; platform falls back if the path is unusable. */
    shell = gem_os_getenv_ref("GEM_SHELL");
    if (!gem_os_pty_spawn_shell(&state->pty,
                                shell != NULL ? shell : "/bin/bash", cwd,
                                TERM_INITIAL_COLS, TERM_INITIAL_ROWS)) {
        gem_os_free(state->primary_cells);
        gem_os_free(state->primary_attrs);
        gem_os_free(state->alternate_cells);
        gem_os_free(state->alternate_attrs);
        state->cells = NULL;
        return 0;
    }
    return 1;
}

static void shutdown_terminal(term_state_t *state)
{
    gem_os_pty_close(&state->pty);
    gem_os_free(state->primary_cells);
    gem_os_free(state->primary_attrs);
    gem_os_free(state->alternate_cells);
    gem_os_free(state->alternate_attrs);
    state->cells = NULL;
    state->attrs = NULL;
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
    {
        WORD menu_id = menu_register(appl_id, "TERMINAL");
        if (menu_id >= 0) (void)menu_unregister(menu_id);
    }
    if (!init_terminal(&state)) {
        appl_exit();
        return 1;
    }

    wind_get(0, WF_WORKXYWH, &desk.g_x, &desk.g_y, &desk.g_w, &desk.g_h);
    work_x = (WORD)(desk.g_x + 36);
    work_y = (WORD)(desk.g_y + 28);
    (void)wind_calc(WC_BORDER,
                    NAME | CLOSER | MOVER | SIZER | UPARROW | DNARROW | VSLIDE |
                        LFARROW | RTARROW | HSLIDE,
                    work_x, work_y, work_w, work_h, &outer.g_x, &outer.g_y,
                    &outer.g_w, &outer.g_h);
    state.handle =
        wind_create(NAME | CLOSER | MOVER | SIZER | UPARROW | DNARROW | VSLIDE |
                        LFARROW | RTARROW | HSLIDE,
                    0, 0, desk.g_w, desk.g_h);
    if (state.handle <= 0) {
        shutdown_terminal(&state);
        v_clsvwk(vdi_handle);
        appl_exit();
        return 1;
    }

    wind_open(state.handle, outer.g_x, outer.g_y, outer.g_w, outer.g_h);
    (void)process_shell_output(&state, NULL);
    sync_layout(&state);
    (void)gem_os_pty_resize(&state.pty, (uint16_t)state.visible_cols,
                            (uint16_t)state.visible_rows);
    draw_terminal(&state, NULL);

    printf("terminal: window %d open, shell spawned.\n", state.handle);
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
        event = evnt_multi(MU_MESAG | MU_KEYBD | MU_TIMER, 0, 0, 0, 0, 0, 0, 0,
                           0, 0, 0, 0, 0, 0, msg, TERM_TIMER_MS, 0, &mx, &my,
                           &mb, &ks, &kr, &br);

        (void)mx;
        (void)my;
        (void)mb;
        (void)ks;
        (void)br;

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
                if (state.console_app_id > 0)
                    send_console_key(&state, kr, ks);
                else
                    send_key(&state, kr, ks);
                if (process_shell_output(&state, &dirty) != 0) {
                    draw_terminal(&state, &dirty);
                }
            }
        }

        if ((event & MU_MESAG) != 0) {
            switch (msg[0]) {
                case GEM_MSG_CONSOLE_REGISTER:
                    state.console_app_id = msg[1];
                    break;
                case GEM_MSG_CONSOLE_OUTPUT:
                    process_shell_byte(&state, (unsigned char)(msg[1] & 0xff));
                    dirty = state.work;
                    redraw_full = 1;
                    break;
                case WM_REDRAW: {
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
                    wind_set(state.handle, WF_CURRXYWH, msg[4], msg[5], msg[6],
                             msg[7]);
                    sync_layout(&state);
                    (void)gem_os_pty_resize(&state.pty,
                                            (uint16_t)state.visible_cols,
                                            (uint16_t)state.visible_rows);
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
                    state.scroll_row = offset_from_slider(
                        msg[4], state.total_rows, state.visible_rows);
                    state.auto_follow = 0;
                    redraw_full = 1;
                    break;
                case WM_HSLID:
                    state.scroll_col = offset_from_slider(
                        msg[4], state.max_line_len, state.visible_cols);
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
