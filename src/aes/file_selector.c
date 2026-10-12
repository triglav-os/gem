/*
 * Implements the blocking GEM file chooser. Open, save and directory modes
 * share a modern GEM interface while fsel_input() retains its classic path,
 * selection and button calling convention.
 *
 * MIT License (see: LICENSE)
 * Copyright (C) 2026 tomaz stih
 */

#include "fsel_private.h"

#include "platform/os.h"

#include <stdlib.h>
#include <string.h>

static aes_fsel_state_t *g_fsel_cursor_state;

static int aes_fsel_cursor_at(WORD x, WORD y)
{
    if (g_fsel_cursor_state == NULL) {
        return ARROW;
    }
    if (aes_fsel_path_hit(g_fsel_cursor_state, x, y) != 0 ||
        aes_fsel_name_hit(g_fsel_cursor_state, x, y) != 0) {
        return TEXT_CRSR;
    }
    return ARROW;
}

static void aes_fsel_cancel(aes_fsel_state_t *state)
{
    state->confirmed = 0;
    state->done = 1;
}

static void aes_fsel_confirm_file(aes_fsel_state_t *state)
{
    char path[AES_PATH_LEN];

    if (state == NULL || state->selection[0] == '\0') {
        return;
    }
    if (state->mode == AES_FSEL_SAVE &&
        aes_fsel_join_path(state->directory, state->selection, path,
                           sizeof(path)) != 0 &&
        gem_os_path_exists(path) != 0) {
        char prompt[] = "[2][ That file already exists. | Replace it? ][ "
                        "Replace | Cancel ]";

        if (form_alert(1, prompt) != 1) {
            return;
        }
    }
    state->confirmed = 1;
    state->done = 1;
}

static void aes_fsel_confirm_directory(aes_fsel_state_t *state)
{
    char path[AES_PATH_LEN];

    if (state == NULL) {
        return;
    }
    aes_fsel_copy_text(path, sizeof(path), state->directory);
    if (state->selected != NIL && state->selected < state->entry_count &&
        state->entries[state->selected].is_directory != 0) {
        if (strcmp(state->entries[state->selected].name, "..") == 0) {
            aes_fsel_parent_dir(path);
        } else if (aes_fsel_join_path(state->directory,
                                      state->entries[state->selected].name,
                                      path, sizeof(path)) == 0) {
            return;
        }
    }
    aes_fsel_copy_text(state->directory, sizeof(state->directory), path);
    state->confirmed = 1;
    state->done = 1;
}

static void aes_fsel_confirm_location(aes_fsel_state_t *state)
{
    if (state == NULL) {
        return;
    }
    if (state->selected != NIL && state->selected < state->entry_count &&
        state->entries[state->selected].is_directory == 0) {
        aes_fsel_set_selection_from_index(state);
        aes_fsel_confirm_file(state);
    } else {
        aes_fsel_confirm_directory(state);
    }
}

static void aes_fsel_accept_action(aes_fsel_state_t *state)
{
    if (state == NULL) {
        return;
    }
    if (state->mode == AES_FSEL_DIRECTORY) {
        aes_fsel_confirm_directory(state);
        return;
    }
    if (state->mode == AES_FSEL_LOCATION) {
        aes_fsel_confirm_location(state);
        return;
    }
    if (state->selected != NIL && state->selected < state->entry_count &&
        state->entries[state->selected].is_directory != 0) {
        aes_fsel_enter_directory(state, state->entries[state->selected].name);
        return;
    }
    if (state->selected != NIL) {
        aes_fsel_set_selection_from_index(state);
    }
    aes_fsel_confirm_file(state);
}

static void aes_fsel_activate_entry(aes_fsel_state_t *state)
{
    if (state == NULL || state->selected == NIL ||
        state->selected >= state->entry_count) {
        return;
    }
    if (state->entries[state->selected].is_directory != 0) {
        aes_fsel_enter_directory(state, state->entries[state->selected].name);
        return;
    }
    aes_fsel_set_selection_from_index(state);
    aes_fsel_confirm_file(state);
}

static void aes_fsel_move_selection(aes_fsel_state_t *state, WORD delta)
{
    WORD next;

    if (state == NULL || state->entry_count <= 0) {
        return;
    }
    next = state->selected == NIL ? 0 : (WORD)(state->selected + delta);
    if (next < 0) {
        next = 0;
    } else if (next >= state->entry_count) {
        next = (WORD)(state->entry_count - 1);
    }
    state->selected = next;
    aes_fsel_set_selection_from_index(state);
    aes_fsel_scroll_into_view(state);
}

static void aes_fsel_scroll(aes_fsel_state_t *state, WORD delta)
{
    WORD maximum;
    WORD next;

    if (state == NULL) {
        return;
    }
    maximum = state->entry_count > state->visible_rows
                  ? (WORD)(state->entry_count - state->visible_rows)
                  : 0;
    next = (WORD)(state->top_index + delta);
    if (next < 0) {
        next = 0;
    } else if (next > maximum) {
        next = maximum;
    }
    state->top_index = next;
    aes_fsel_sync_work(state);
}

static WORD aes_fsel_control_at(const aes_fsel_state_t *state, WORD x, WORD y)
{
    WORD control;

    for (control = AES_FSEL_CONTROL_ACCEPT;
         control <= AES_FSEL_CONTROL_UP; ++control) {
        GRECT rect;

        aes_fsel_control_rect(state, control, &rect);
        if (rect.g_w > 0 && aes_point_in_rect(x, y, &rect) != 0) {
            return control;
        }
    }
    return AES_FSEL_CONTROL_NONE;
}

static void aes_fsel_run_control(aes_fsel_state_t *state, WORD control)
{
    switch (control) {
        case AES_FSEL_CONTROL_ACCEPT:
            aes_fsel_accept_action(state);
            break;
        case AES_FSEL_CONTROL_CANCEL:
            aes_fsel_cancel(state);
            break;
        case AES_FSEL_CONTROL_BACK:
            aes_fsel_move_history(state, -1);
            break;
        case AES_FSEL_CONTROL_FORWARD:
            aes_fsel_move_history(state, 1);
            break;
        case AES_FSEL_CONTROL_UP:
            aes_fsel_enter_directory(state, "..");
            break;
        case AES_FSEL_CONTROL_SCROLL_UP:
            aes_fsel_scroll(state, -1);
            break;
        case AES_FSEL_CONTROL_SCROLL_DOWN:
            aes_fsel_scroll(state, 1);
            break;
        default:
            break;
    }
}

static void aes_fsel_accept_result(const aes_fsel_state_t *state, char *pipath,
                                   char *pisel, WORD *pbutton)
{
    if (pbutton != NULL) {
        *pbutton = state->confirmed != 0 ? 1 : 0;
    }
    if (state->mode == AES_FSEL_DIRECTORY) {
        if (pipath != NULL && state->confirmed != 0) {
            strcpy(pipath, state->directory);
        }
        return;
    }
    if (state->mode == AES_FSEL_LOCATION) {
        if (pipath != NULL && state->confirmed != 0) {
            strcpy(pipath, state->directory);
        }
        if (pisel != NULL) {
            strcpy(pisel, state->confirmed != 0 ? state->selection : "");
        }
        return;
    }
    if (pipath != NULL) {
        char path[AES_PATH_LEN];

        if (aes_fsel_join_path(state->directory, state->pattern, path,
                               sizeof(path)) == 0) {
            path[0] = '\0';
        }
        strcpy(pipath, path);
    }
    if (pisel != NULL) {
        strcpy(pisel, state->selection);
    }
}

static int aes_fsel_prepare(aes_fsel_state_t *state, char *pipath, char *pisel,
                            WORD mode, const char *title)
{
    GRECT desk;
    gem_os_file_info_t info;
    char cwd[AES_PATH_LEN];
    WORD width;
    WORD height;

    memset(state, 0, sizeof(*state));
    state->mode = mode;
    state->show_hidden =
        (aes_state.fsel_options & FSEL_SHOW_HIDDEN) != 0u ? 1 : 0;
    state->selected = NIL;
    state->selected_location = NIL;
    state->last_click_entry = NIL;
    aes_fsel_copy_text(state->title, sizeof(state->title),
                       title == NULL ? "Open File" : title);
    if (mode == AES_FSEL_DIRECTORY || mode == AES_FSEL_LOCATION) {
        const char *initial = pipath;

        if (initial == NULL || initial[0] == '\0' ||
            gem_os_stat_path(initial, &info) == 0 || info.is_directory == 0) {
            initial = gem_os_getcwd(cwd, sizeof(cwd)) != 0 ? cwd : "/";
        }
        aes_fsel_copy_text(state->directory, sizeof(state->directory), initial);
        strcpy(state->pattern, "*");
        aes_fsel_normalize_dir(state->directory);
    } else {
        aes_fsel_split_input(pipath, pisel, state->directory,
                             sizeof(state->directory), state->pattern,
                             sizeof(state->pattern), state->selection,
                             sizeof(state->selection));
    }
    if (gem_os_stat_path(state->directory, &info) == 0 ||
        info.is_directory == 0) {
        const char *fallback = gem_os_getcwd(cwd, sizeof(cwd)) != 0 ? cwd : "/";

        aes_fsel_copy_text(state->directory, sizeof(state->directory),
                           fallback);
    }
    if (aes_ensure_vdi() == 0) {
        return 0;
    }

    gemctl_text_edit_init(&state->path_edit, state->directory,
                          sizeof(state->directory));

    wind_get(0, WF_CXYWH, &desk.g_x, &desk.g_y, &desk.g_w, &desk.g_h);
    width = desk.g_w > 620 ? 620 : (WORD)(desk.g_w - 20);
    height = desk.g_h > 390 ? 390 : (WORD)(desk.g_h - 20);
    state->handle =
        wind_create(NAME | CLOSER | MOVER | UPARROW | DNARROW | VSLIDE,
                    desk.g_x, desk.g_y, width, height);
    if (state->handle <= 0) {
        return 0;
    }
    state->char_w = vdi_string_width("M");
    state->char_h = vdi_font_text_height();
    state->text_ascent = vdi_font_ascent();
    state->row_h = (WORD)(state->char_h + 5);
    aes_fsel_build_locations(state);
    aes_fsel_copy_text(state->history[0], sizeof(state->history[0]),
                       state->directory);
    state->history_count = 1;
    state->history_index = 0;
    aes_fsel_reload_entries(state);

    aes_fsel_snapshot(state);
    wind_open(state->handle, (WORD)(desk.g_x + (desk.g_w - width) / 2),
              (WORD)(desk.g_y + (desk.g_h - height) / 2), width, height);
    wind_set_str(state->handle, WF_NAME, state->title);
    aes_fsel_sync_work(state);
    aes_fsel_scroll_into_view(state);
    return 1;
}

static void aes_fsel_key(aes_fsel_state_t *state, WORD key)
{
    WORD ascii = (WORD)(key & 0xffu);
    WORD scancode = (WORD)((key >> 8) & 0xffu);
    gemctl_text_edit_result_t path_result;

    path_result = gemctl_text_edit_key(&state->path_edit, (uint16_t)key);
    if (path_result == GEMCTL_TEXT_EDIT_ACCEPT) {
        char path[AES_PATH_LEN];

        strcpy(path, state->directory);
        aes_fsel_navigate(state, path, 1);
        return;
    }
    if (path_result == GEMCTL_TEXT_EDIT_CANCEL) {
        aes_fsel_cancel(state);
        return;
    }
    if (path_result != GEMCTL_TEXT_EDIT_IGNORED) {
        return;
    }

    if (ascii == 27) {
        aes_fsel_cancel(state);
    } else if (ascii == '\r' || ascii == '\n' ||
               (ascii == 0 && (scancode == 40 || scancode == 28))) {
        aes_fsel_accept_action(state);
    } else if (ascii == '\b' ||
               (ascii == 0 && (scancode == 42 || scancode == 14))) {
        size_t length;

        if (state->mode != AES_FSEL_DIRECTORY &&
            state->editing_name != 0) {
            length = strlen(state->selection);
            if (length > 0u) {
                state->selection[length - 1u] = '\0';
            }
        } else {
            aes_fsel_enter_directory(state, "..");
        }
    } else if (scancode == 81 || scancode == 80) {
        aes_fsel_move_selection(state, 1);
    } else if (scancode == 82 || scancode == 72) {
        aes_fsel_move_selection(state, -1);
    } else if (scancode == 78) {
        aes_fsel_move_selection(state, state->visible_rows);
    } else if (scancode == 75) {
        aes_fsel_move_selection(state, (WORD)-state->visible_rows);
    } else if (state->mode != AES_FSEL_DIRECTORY && ascii >= 32 &&
               ascii < 127 && ascii != '/') {
        size_t length;

        if (state->editing_name == 0) {
            state->selection[0] = '\0';
        }
        state->editing_name = 1;
        state->selected = NIL;
        length = strlen(state->selection);
        if (length + 1u < sizeof(state->selection)) {
            state->selection[length] = (char)ascii;
            state->selection[length + 1u] = '\0';
        }
    }
}

static void aes_fsel_message(aes_fsel_state_t *state, WORD msg[8])
{
    if (msg[3] != state->handle) {
        if (msg[0] == WM_REDRAW) {
            aes_fsel_restore_owner(state, msg);
        }
        return;
    }
    switch (msg[0]) {
        case WM_REDRAW: {
            GRECT dirty = {msg[4], msg[5], msg[6], msg[7]};

            aes_fsel_sync_work(state);
            aes_fsel_draw(state, &dirty);
            break;
        }
        case WM_TOPPED:
            wind_set(state->handle, WF_TOP, 0, 0, 0, 0);
            break;
        case WM_MOVED:
            wind_set(state->handle, WF_WXYWH, msg[4], msg[5], msg[6], msg[7]);
            aes_fsel_sync_work(state);
            aes_fsel_scroll_into_view(state);
            aes_fsel_draw(state, NULL);
            break;
        case WM_ARROWED:
            switch (msg[4]) {
                case WA_UPPAGE:
                    aes_fsel_scroll(state, (WORD)-state->visible_rows);
                    break;
                case WA_DNPAGE:
                    aes_fsel_scroll(state, state->visible_rows);
                    break;
                case WA_UPLINE:
                    aes_fsel_scroll(state, -1);
                    break;
                case WA_DNLINE:
                    aes_fsel_scroll(state, 1);
                    break;
                default:
                    break;
            }
            aes_fsel_draw(state, NULL);
            break;
        case WM_VSLID: {
            WORD maximum = state->entry_count > state->visible_rows
                               ? (WORD)(state->entry_count - state->visible_rows)
                               : 0;
            state->top_index = (WORD)((long)maximum * msg[4] / 1000L);
            aes_fsel_draw(state, NULL);
            break;
        }
        case WM_CLOSED:
            aes_fsel_cancel(state);
            break;
        default:
            break;
    }
}

static void aes_fsel_button(aes_fsel_state_t *state, WORD x, WORD y)
{
    if (state->pressed_control != AES_FSEL_CONTROL_NONE) {
        WORD pressed = state->pressed_control;

        state->pressed_control = AES_FSEL_CONTROL_NONE;
        if (aes_fsel_control_at(state, x, y) == pressed) {
            aes_fsel_run_control(state, pressed);
        }
        aes_fsel_draw(state, NULL);
        return;
    }
    if (wind_find(x, y) != state->handle) {
        return;
    }
    state->pressed_control = aes_fsel_control_at(state, x, y);
    if (state->pressed_control != AES_FSEL_CONTROL_NONE) {
        aes_fsel_draw(state, NULL);
        return;
    }
    if (aes_fsel_name_hit(state, x, y) != 0) {
        gemctl_text_edit_blur(&state->path_edit);
        state->editing_name = 1;
        state->selected = NIL;
        aes_fsel_draw(state, NULL);
        return;
    }
    if (aes_fsel_path_hit(state, x, y) != 0) {
        gemctl_text_edit_focus(&state->path_edit, 1);
        state->editing_name = 0;
        state->selected = NIL;
        aes_fsel_draw(state, NULL);
        return;
    }
    {
        WORD location = aes_fsel_location_at(state, x, y);

        if (location != NIL) {
            aes_fsel_navigate(state, state->locations[location].path, 1);
            aes_fsel_draw(state, NULL);
            return;
        }
    }
    {
        WORD entry = aes_fsel_entry_at(state, x, y);

        if (entry != NIL) {
            uint32_t now = gem_os_ticks_ms();
            int double_click = state->last_click_entry == entry &&
                               now - state->last_click_ms <= 400u;

            state->last_click_entry = entry;
            state->last_click_ms = now;
            if (double_click != 0) {
                aes_fsel_activate_entry(state);
            } else {
                state->selected = entry;
                aes_fsel_set_selection_from_index(state);
                aes_fsel_scroll_into_view(state);
            }
            aes_fsel_draw(state, NULL);
        }
    }
}

static WORD aes_fsel_run(char *pipath, char *pisel, WORD *pbutton, WORD mode,
                         const char *title)
{
    aes_fsel_state_t state;
    WORD msg[8];

    if (pbutton != NULL) {
        *pbutton = 0;
    }
    if (pipath == NULL || pbutton == NULL ||
        (mode != AES_FSEL_DIRECTORY && pisel == NULL) ||
        aes_fsel_prepare(&state, pipath, pisel, mode, title) == 0) {
        return 0;
    }
    aes_fsel_draw(&state, NULL);
    g_fsel_cursor_state = &state;
    aes_hover_cursor_hook = aes_fsel_cursor_at;
    while (state.done == 0) {
        WORD event;
        WORD mx = 0;
        WORD my = 0;
        WORD mb = 0;
        WORD ks = 0;
        WORD kr = 0;
        WORD br = 0;

        if (aes_wait_hook != NULL && aes_wait_hook() == 0) {
            break;
        }
        event = evnt_multi(
            MU_MESAG | MU_BUTTON | MU_KEYBD, 1, 1,
            state.pressed_control != AES_FSEL_CONTROL_NONE ? 0 : 1, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, msg, 0, 0, &mx, &my, &mb, &ks, &kr, &br);
        (void)mb;
        (void)ks;
        (void)br;
        if ((event & MU_MESAG) != 0) {
            aes_fsel_message(&state, msg);
        }
        if ((event & MU_KEYBD) != 0) {
            aes_fsel_key(&state, kr);
            aes_fsel_draw(&state, NULL);
        }
        if ((event & MU_BUTTON) != 0) {
            aes_fsel_button(&state, mx, my);
        }
    }

    aes_fsel_accept_result(&state, pipath, pisel, pbutton);
    g_fsel_cursor_state = NULL;
    aes_hover_cursor_hook = NULL;
    (void)graf_mouse(ARROW, NULL);
    wind_close(state.handle);
    wind_delete(state.handle);
    free(state.background.fd_addr);
    return 1;
}

WORD fsel_input(char *pipath, char *pisel, WORD *pbutton)
{
    return aes_fsel_run(pipath, pisel, pbutton, AES_FSEL_OPEN, "Open File");
}

WORD fsel_exinput(char *pipath, char *pisel, WORD *pbutton, char *title)
{
    return aes_fsel_run(pipath, pisel, pbutton, AES_FSEL_OPEN,
                        title == NULL ? "Open File" : title);
}

WORD fsel_save(char *pipath, char *pisel, WORD *pbutton)
{
    return aes_fsel_run(pipath, pisel, pbutton, AES_FSEL_SAVE, "Save File");
}

WORD fsel_directory(char *path, WORD *pbutton)
{
    return aes_fsel_run(path, NULL, pbutton, AES_FSEL_DIRECTORY,
                        "Select Folder");
}

WORD fsel_location(char *path, char *name, WORD *pbutton)
{
    return aes_fsel_run(path, name, pbutton, AES_FSEL_LOCATION,
                        "Select Location");
}

WORD fsel_options(UWORD options, WORD setit)
{
    if (setit != 0) {
        aes_state.fsel_options = (UWORD)(options & FSEL_SHOW_HIDDEN);
    }
    return (WORD)aes_state.fsel_options;
}
