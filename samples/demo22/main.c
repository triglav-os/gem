/*
 * Demonstrates a standalone AES application with a real main menu bar.
 * The sample registers a menu tree through `menu_bar()`, reacts to
 * `MN_SELECTED` messages, and keeps one normal work window beneath the
 * global menu area so later demos can build on menu-first behavior.
 *
 * MIT License (see: LICENSE)
 * Copyright (C) 2026 tomaz stih
 */

#include <gem/aes.h>
#include <gem/os.h>
#include <gem/vdi.h>

#include <stdint.h>
#include <stdio.h>
#include <string.h>

enum {
    menu_root = 0,
    menu_bar_box,
    menu_title_parent,
    menu_title_file,
    menu_title_options,
    menu_title_help,
    menu_popup_parent,
    menu_popup_file,
    menu_popup_options,
    menu_popup_help,
    menu_file_about,
    menu_file_separator,
    menu_file_quit,
    menu_options_toggle,
    menu_options_reset,
    menu_help_keys,
    menu_help_about,
    menu_object_count
};

enum {
    demo22_menu_bar_height = 22,
    demo22_menu_title_height = 16
};

typedef struct demo22_state {
    WORD handle;
    GRECT normal_rect;
    OBJECT menu[menu_object_count];
    WORD screen_width;
    int menu_click_mode;
    int option_enabled;
    char status[96];
} demo22_state_t;

static void demo22_init_object(OBJECT *object,
                               WORD next,
                               WORD head,
                               WORD tail,
                               UWORD type,
                               UWORD flags,
                               UWORD state,
                               LONG spec,
                               WORD x,
                               WORD y,
                               WORD width,
                               WORD height)
{
    if (object == NULL) {
        return;
    }

    object->ob_next = next;
    object->ob_head = head;
    object->ob_tail = tail;
    object->ob_type = type;
    object->ob_flags = flags;
    object->ob_state = state;
    object->ob_spec = spec;
    object->ob_x = x;
    object->ob_y = y;
    object->ob_width = width;
    object->ob_height = height;
}

static void demo22_set_status(demo22_state_t *state, const char *text)
{
    if (state == NULL) {
        return;
    }

    if (text == NULL) {
        state->status[0] = '\0';
    } else {
        strncpy(state->status, text, sizeof(state->status) - 1u);
        state->status[sizeof(state->status) - 1u] = '\0';
    }
}

static void demo22_sync_menu_state(demo22_state_t *state)
{
    if (state == NULL) {
        return;
    }

    if (state->option_enabled) {
        state->menu[menu_options_toggle].ob_state |= CHECKED;
    } else {
        state->menu[menu_options_toggle].ob_state &= (UWORD) ~CHECKED;
    }
}

static void demo22_draw_menu(demo22_state_t *state)
{
    if (state == NULL) {
        return;
    }

    objc_draw(state->menu, ROOT, MAX_DEPTH, 0, 0, state->screen_width,
        demo22_menu_bar_height);
}

static void demo22_init_menu_tree(demo22_state_t *state, WORD screen_width)
{
    if (state == NULL) {
        return;
    }

    demo22_init_object(&state->menu[menu_root],
        NIL, menu_bar_box, menu_popup_parent,
        G_IBOX, NONE, NORMAL, 0L,
        0, 0, screen_width, 74);
    demo22_init_object(&state->menu[menu_bar_box],
        menu_popup_parent, menu_title_parent, menu_title_parent,
        G_BOX, NONE, NORMAL, 0L,
        0, 0, screen_width, demo22_menu_bar_height);
    demo22_init_object(&state->menu[menu_title_parent],
        menu_bar_box, menu_title_file, menu_title_help,
        G_IBOX, NONE, NORMAL, 0L,
        0, 0, screen_width, demo22_menu_bar_height);
    demo22_init_object(&state->menu[menu_title_file],
        menu_title_options, NIL, NIL,
        G_TITLE, NONE, NORMAL, (LONG) (intptr_t) "File",
        8, 3, 44, demo22_menu_title_height);
    demo22_init_object(&state->menu[menu_title_options],
        menu_title_help, NIL, NIL,
        G_TITLE, NONE, NORMAL, (LONG) (intptr_t) "Options",
        56, 3, 70, demo22_menu_title_height);
    demo22_init_object(&state->menu[menu_title_help],
        menu_title_parent, NIL, NIL,
        G_TITLE, LASTOB, NORMAL, (LONG) (intptr_t) "Help",
        132, 3, 48, demo22_menu_title_height);

    demo22_init_object(&state->menu[menu_popup_parent],
        menu_root, menu_popup_file, menu_popup_help,
        G_IBOX, LASTOB, NORMAL, 0L,
        0, demo22_menu_bar_height, screen_width, 56);
    demo22_init_object(&state->menu[menu_popup_file],
        menu_popup_options, menu_file_about, menu_file_quit,
        G_BOX, HIDETREE, NORMAL, 0L,
        8, 0, 116, 42);
    demo22_init_object(&state->menu[menu_popup_options],
        menu_popup_help, menu_options_toggle, menu_options_reset,
        G_BOX, HIDETREE, NORMAL, 0L,
        56, 0, 124, 30);
    demo22_init_object(&state->menu[menu_popup_help],
        menu_popup_parent, menu_help_keys, menu_help_about,
        G_BOX, HIDETREE | LASTOB, NORMAL, 0L,
        132, 0, 124, 30);

    demo22_init_object(&state->menu[menu_file_about],
        menu_file_separator, NIL, NIL,
        G_STRING, NONE, NORMAL, (LONG) (intptr_t) "About Demo 22",
        6, 6, 96, 8);
    demo22_init_object(&state->menu[menu_file_separator],
        menu_file_quit, NIL, NIL,
        G_STRING, DISABLED, DISABLED, (LONG) (intptr_t) "------------",
        6, 18, 96, 8);
    demo22_init_object(&state->menu[menu_file_quit],
        menu_popup_file, NIL, NIL,
        G_STRING, LASTOB, NORMAL, (LONG) (intptr_t) "Quit",
        6, 30, 40, 8);
    demo22_init_object(&state->menu[menu_options_toggle],
        menu_options_reset, NIL, NIL,
        G_STRING, NONE, NORMAL, (LONG) (intptr_t) "Sticky menu click",
        6, 6, 112, 8);
    demo22_init_object(&state->menu[menu_options_reset],
        menu_popup_options, NIL, NIL,
        G_STRING, LASTOB, NORMAL, (LONG) (intptr_t) "Reset status",
        6, 20, 80, 8);
    demo22_init_object(&state->menu[menu_help_keys],
        menu_help_about, NIL, NIL,
        G_STRING, NONE, NORMAL, (LONG) (intptr_t) "Esc closes app",
        6, 6, 88, 8);
    demo22_init_object(&state->menu[menu_help_about],
        menu_popup_help, NIL, NIL,
        G_STRING, LASTOB, NORMAL, (LONG) (intptr_t) "Menus are live now",
        6, 20, 104, 8);

    state->menu_click_mode = 1;
    state->option_enabled = 1;
    demo22_sync_menu_state(state);
}

static void demo22_draw_content(const demo22_state_t *state,
                                const GRECT *dirty_rect)
{
    GRECT work_rect;
    GRECT clip_rect;

    if (state == NULL) {
        return;
    }

    if (wind_get(state->handle, WF_CXYWH, &work_rect.g_x, &work_rect.g_y,
        &work_rect.g_w, &work_rect.g_h) == 0) {
        return;
    }

    wind_update(BEG_UPDATE);
    wind_get(state->handle, WF_FIRSTXYWH, &clip_rect.g_x, &clip_rect.g_y,
        &clip_rect.g_w, &clip_rect.g_h);
    while (clip_rect.g_w > 0 && clip_rect.g_h > 0) {
        WORD x0 = clip_rect.g_x;
        WORD y0 = clip_rect.g_y;
        WORD x1 = (WORD) (clip_rect.g_x + clip_rect.g_w - 1);
        WORD y1 = (WORD) (clip_rect.g_y + clip_rect.g_h - 1);

        if (dirty_rect != NULL) {
            WORD dirty_x1 = (WORD) (dirty_rect->g_x + dirty_rect->g_w - 1);
            WORD dirty_y1 = (WORD) (dirty_rect->g_y + dirty_rect->g_h - 1);

            if (x0 < dirty_rect->g_x) {
                x0 = dirty_rect->g_x;
            }
            if (y0 < dirty_rect->g_y) {
                y0 = dirty_rect->g_y;
            }
            if (x1 > dirty_x1) {
                x1 = dirty_x1;
            }
            if (y1 > dirty_y1) {
                y1 = dirty_y1;
            }
        }

        if (x0 <= x1 && y0 <= y1) {
            OBJECT content[5];

            demo22_init_object(&content[0], NIL, 1, 4,
                G_BOX, NONE, NORMAL, 0L,
                work_rect.g_x, work_rect.g_y, work_rect.g_w, work_rect.g_h);
            demo22_init_object(&content[1], 2, NIL, NIL,
                G_STRING, NONE, NORMAL,
                (LONG) (intptr_t) "Demo 22 enables the main menu bar.",
                (WORD) (work_rect.g_x + 16), (WORD) (work_rect.g_y + 18),
                200, 8);
            demo22_init_object(&content[2], 3, NIL, NIL,
                G_STRING, NONE, NORMAL,
                (LONG) (intptr_t) "Use File, Options, and Help above.",
                (WORD) (work_rect.g_x + 16), (WORD) (work_rect.g_y + 36),
                196, 8);
            demo22_init_object(&content[3], 4, NIL, NIL,
                G_STRING, NONE, NORMAL,
                (LONG) (intptr_t) "Menu selections arrive as MN_SELECTED.",
                (WORD) (work_rect.g_x + 16), (WORD) (work_rect.g_y + 54),
                212, 8);
            demo22_init_object(&content[4], 0, NIL, NIL,
                G_STRING, LASTOB, NORMAL,
                (LONG) (intptr_t) state->status,
                (WORD) (work_rect.g_x + 16), (WORD) (work_rect.g_y + 82),
                (WORD) (work_rect.g_w - 32), 8);

            objc_draw(content, ROOT, MAX_DEPTH, x0, y0,
                (WORD) (x1 - x0 + 1), (WORD) (y1 - y0 + 1));
        }

        wind_get(state->handle, WF_NEXTXYWH, &clip_rect.g_x, &clip_rect.g_y,
            &clip_rect.g_w, &clip_rect.g_h);
    }
    wind_update(END_UPDATE);
}

static int demo22_handle_menu(demo22_state_t *state, WORD title, WORD item)
{
    if (state == NULL) {
        return 0;
    }

    if (title == menu_title_file) {
        if (item == menu_file_about) {
            demo22_set_status(state, "File/About selected.");
        } else if (item == menu_file_quit) {
            return 1;
        }
    } else if (title == menu_title_options) {
        if (item == menu_options_toggle) {
            state->option_enabled = !state->option_enabled;
            state->menu_click_mode = state->option_enabled ? 1 : 0;
            (void) menu_click((WORD) state->menu_click_mode, 1);
            demo22_sync_menu_state(state);
            demo22_set_status(state, state->menu_click_mode != 0 ?
                "Sticky menu click enabled." :
                "Sticky menu click disabled.");
        } else if (item == menu_options_reset) {
            demo22_set_status(state, "Status reset from Options.");
        }
    } else if (title == menu_title_help) {
        if (item == menu_help_keys) {
            demo22_set_status(state, "Press Esc to leave demo22.");
        } else if (item == menu_help_about) {
            demo22_set_status(state, "Hosted AES menu support is active.");
        }
    }

    demo22_draw_menu(state);
    demo22_draw_content(state, NULL);
    return 0;
}

int main(void)
{
    WORD appl_id;
    WORD vdi_handle;
    WORD work_in[11] = { 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 2 };
    WORD work_out[57] = { 0 };
    WORD char_w = 0;
    WORD char_h = 0;
    WORD box_w = 0;
    WORD box_h = 0;
    WORD msg[8] = { 0 };
    WORD event_flags;
    WORD mouse_x = 0;
    WORD mouse_y = 0;
    WORD mouse_buttons = 0;
    WORD key_state = 0;
    WORD key_code = 0;
    WORD button_return = 0;
    GRECT current_rect;
    GRECT full_rect;
    demo22_state_t state;

    memset(&state, 0, sizeof(state));

    if (!gem_os_init()) {
        fprintf(stderr, "gem_os_init() failed\n");
        return 1;
    }

    appl_id = appl_init();
    if (appl_id < 0) {
        gem_os_shutdown();
        return 1;
    }

    vdi_handle = graf_handle(&char_w, &char_h, &box_w, &box_h);
    if (vdi_handle == 0) {
        appl_exit();
        gem_os_shutdown();
        return 1;
    }

    v_opnvwk(work_in, &vdi_handle, work_out);

    demo22_set_status(&state, "Choose a menu item to begin.");
    state.screen_width = (WORD) (work_out[0] + 1);
    demo22_init_menu_tree(&state, state.screen_width);
    (void) menu_click(1, 1);
    (void) menu_bar(state.menu, 1);

    state.handle = wind_create((UWORD) (NAME | MOVER | CLOSER | FULLER | SIZER),
        0, 0, (WORD) (work_out[0] + 1), (WORD) (work_out[1] + 1));
    if (state.handle <= 0) {
        (void) menu_bar(NULL, 0);
        v_clsvwk(vdi_handle);
        appl_exit();
        gem_os_shutdown();
        return 1;
    }

    (void) wind_set_str(state.handle, WF_NAME, "Demo 22 Main Menu");
    (void) wind_open(state.handle, 96, 72, 420, 220);
    (void) wind_get(state.handle, WF_WXYWH, &state.normal_rect.g_x,
        &state.normal_rect.g_y, &state.normal_rect.g_w,
        &state.normal_rect.g_h);
    full_rect.g_x = 0;
    full_rect.g_y = 0;
    full_rect.g_w = (WORD) (work_out[0] + 1);
    full_rect.g_h = (WORD) (work_out[1] + 1);
    (void) graf_mouse(M_ON, NULL);
    demo22_draw_menu(&state);
    demo22_draw_content(&state, NULL);

    FOREVER {
        event_flags = evnt_multi((UWORD) (MU_MESAG | MU_KEYBD),
            1, 1, 1,
            0, 0, 0, 0, 0,
            0, 0, 0, 0, 0,
            msg,
            0, 0,
            &mouse_x, &mouse_y, &mouse_buttons, &key_state,
            &key_code, &button_return);

        if ((event_flags & MU_MESAG) != 0) {
            if (msg[0] == MN_SELECTED) {
                if (demo22_handle_menu(&state, msg[3], msg[4]) != 0) {
                    break;
                }
                continue;
            }

            if (msg[3] != state.handle) {
                if ((event_flags & MU_KEYBD) != 0 && key_code == 0x011b) {
                    break;
                }
                continue;
            }

            if (msg[0] == WM_CLOSED) {
                break;
            } else if (msg[0] == WM_REDRAW) {
                GRECT redraw_rect;

                redraw_rect.g_x = msg[4];
                redraw_rect.g_y = msg[5];
                redraw_rect.g_w = msg[6];
                redraw_rect.g_h = msg[7];
                demo22_draw_content(&state, &redraw_rect);
            } else if (msg[0] == WM_MOVED || msg[0] == WM_SIZED) {
                wind_update(BEG_UPDATE);
                (void) wind_set(state.handle, WF_WXYWH, msg[4], msg[5], msg[6],
                    msg[7]);
                (void) wind_get(state.handle, WF_WXYWH, &current_rect.g_x,
                    &current_rect.g_y, &current_rect.g_w, &current_rect.g_h);
                if (current_rect.g_x != full_rect.g_x ||
                    current_rect.g_y != full_rect.g_y ||
                    current_rect.g_w != full_rect.g_w ||
                    current_rect.g_h != full_rect.g_h) {
                    state.normal_rect = current_rect;
                }
                wind_update(END_UPDATE);
            } else if (msg[0] == WM_TOPPED) {
                wind_update(BEG_UPDATE);
                (void) wind_set(state.handle, WF_TOP, 0, 0, 0, 0);
                wind_update(END_UPDATE);
            } else if (msg[0] == WM_FULLED) {
                wind_update(BEG_UPDATE);
                (void) wind_get(state.handle, WF_WXYWH, &current_rect.g_x,
                    &current_rect.g_y, &current_rect.g_w, &current_rect.g_h);
                if (current_rect.g_x == full_rect.g_x &&
                    current_rect.g_y == full_rect.g_y &&
                    current_rect.g_w == full_rect.g_w &&
                    current_rect.g_h == full_rect.g_h) {
                    (void) wind_set(state.handle, WF_WXYWH,
                        state.normal_rect.g_x, state.normal_rect.g_y,
                        state.normal_rect.g_w, state.normal_rect.g_h);
                } else {
                    state.normal_rect = current_rect;
                    (void) wind_set(state.handle, WF_WXYWH, full_rect.g_x,
                        full_rect.g_y, full_rect.g_w, full_rect.g_h);
                }
                wind_update(END_UPDATE);
            }
        }

        if ((event_flags & MU_KEYBD) != 0 && key_code == 0x011b) {
            break;
        }
    }

    (void) menu_bar(NULL, 0);
    if (state.handle > 0) {
        (void) wind_close(state.handle);
        (void) wind_delete(state.handle);
    }
    v_clsvwk(vdi_handle);
    appl_exit();
    gem_os_shutdown();
    return 0;
}
