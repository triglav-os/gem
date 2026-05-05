/*
 * Demonstrates a classic GEM-style menu tree built with the historical
 * `THEBAR` / `THEACTIVE` / `THEMENUS` structure. The sample keeps a
 * normal work window below the menu bar and handles `MN_SELECTED`
 * messages through a simple redraw-driven event loop.
 *
 * MIT License (see: LICENSE)
 * Copyright (C) 2026 tomaz stih
 */

#include <gem.h>
#include <gem/os.h>

#include <stdint.h>
#include <stdio.h>
#include <string.h>

enum {
    demo23_root = 0,
    demo23_thebar,
    demo23_theactive,
    demo23_title_desk,
    demo23_title_file,
    demo23_title_help,
    demo23_themenus,
    demo23_menu_desk,
    demo23_menu_file,
    demo23_menu_help,
    demo23_desk_about,
    demo23_desk_sep,
    demo23_desk_quit,
    demo23_file_open,
    demo23_file_save,
    demo23_file_exit,
    demo23_help_about,
    demo23_object_count
};

typedef struct demo23_state {
    WORD appl_id;
    WORD vdi_handle;
    WORD work_in[11];
    WORD work_out[57];
    WORD char_w;
    WORD char_h;
    WORD box_w;
    WORD box_h;
    WORD win_handle;
    OBJECT menu_tree[demo23_object_count];
    char message[80];
} demo23_state_t;

static void demo23_init_object(OBJECT *object,
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

static void demo23_draw_menu(demo23_state_t *state)
{
    if (state == NULL) {
        return;
    }

    objc_draw(state->menu_tree, ROOT, MAX_DEPTH, 0, 0,
        state->work_out[0] + 1, state->char_h + 6);
}

static void demo23_init_menu_tree(demo23_state_t *state)
{
    WORD screen_width;
    WORD bar_height;
    WORD popup_y;
    WORD title_x;
    WORD title_width;
    WORD popup_width;

    if (state == NULL) {
        return;
    }

    screen_width = (WORD) (state->work_out[0] + 1);
    bar_height = (WORD) (state->char_h + 4);
    popup_y = bar_height;

    demo23_init_object(&state->menu_tree[demo23_root],
        NIL, demo23_thebar, demo23_themenus,
        G_IBOX, NONE, NORMAL, 0L,
        0, 0, screen_width, (WORD) (popup_y + state->char_h * 4));
    demo23_init_object(&state->menu_tree[demo23_thebar],
        demo23_themenus, demo23_theactive, demo23_theactive,
        G_BOX, NONE, NORMAL, 0L,
        0, 0, screen_width, bar_height);
    demo23_init_object(&state->menu_tree[demo23_theactive],
        demo23_thebar, demo23_title_desk, demo23_title_help,
        G_IBOX, NONE, NORMAL, 0L,
        0, 0, screen_width, bar_height);

    title_x = 8;
    title_width = (WORD) (state->char_w * 6);
    demo23_init_object(&state->menu_tree[demo23_title_desk],
        demo23_title_file, NIL, NIL,
        G_TITLE, NONE, NORMAL, (LONG) (intptr_t) "Desk",
        title_x, 1, title_width, (WORD) (state->char_h + 2));
    title_x = (WORD) (title_x + title_width);
    demo23_init_object(&state->menu_tree[demo23_title_file],
        demo23_title_help, NIL, NIL,
        G_TITLE, NONE, NORMAL, (LONG) (intptr_t) "File",
        title_x, 1, title_width, (WORD) (state->char_h + 2));
    title_x = (WORD) (title_x + title_width);
    demo23_init_object(&state->menu_tree[demo23_title_help],
        demo23_theactive, NIL, NIL,
        G_TITLE, LASTOB, NORMAL, (LONG) (intptr_t) "Help",
        title_x, 1, title_width, (WORD) (state->char_h + 2));

    demo23_init_object(&state->menu_tree[demo23_themenus],
        demo23_root, demo23_menu_desk, demo23_menu_help,
        G_IBOX, LASTOB, NORMAL, 0L,
        0, popup_y, screen_width, (WORD) (state->char_h * 4));

    popup_width = (WORD) (state->char_w * 16);
    demo23_init_object(&state->menu_tree[demo23_menu_desk],
        demo23_menu_file, demo23_desk_about, demo23_desk_quit,
        G_BOX, HIDETREE, NORMAL, 0L,
        8, 0, popup_width, (WORD) (state->char_h * 3 + 6));
    demo23_init_object(&state->menu_tree[demo23_menu_file],
        demo23_menu_help, demo23_file_open, demo23_file_exit,
        G_BOX, HIDETREE, NORMAL, 0L,
        (WORD) (8 + title_width), 0, popup_width,
        (WORD) (state->char_h * 3 + 6));
    demo23_init_object(&state->menu_tree[demo23_menu_help],
        demo23_themenus, demo23_help_about, demo23_help_about,
        G_BOX, HIDETREE | LASTOB, NORMAL, 0L,
        (WORD) (8 + title_width * 2), 0,
        (WORD) (state->char_w * 18), (WORD) (state->char_h + 6));

    demo23_init_object(&state->menu_tree[demo23_desk_about],
        demo23_desk_sep, NIL, NIL,
        G_STRING, NONE, NORMAL, (LONG) (intptr_t) "  About...  ",
        4, 4, (WORD) (popup_width - 8), state->char_h);
    demo23_init_object(&state->menu_tree[demo23_desk_sep],
        demo23_desk_quit, NIL, NIL,
        G_STRING, DISABLED, DISABLED,
        (LONG) (intptr_t) "  ----------  ",
        4, (WORD) (4 + state->char_h), (WORD) (popup_width - 8), state->char_h);
    demo23_init_object(&state->menu_tree[demo23_desk_quit],
        demo23_menu_desk, NIL, NIL,
        G_STRING, LASTOB, NORMAL, (LONG) (intptr_t) "  Quit  ",
        4, (WORD) (4 + state->char_h * 2), (WORD) (popup_width - 8), state->char_h);

    demo23_init_object(&state->menu_tree[demo23_file_open],
        demo23_file_save, NIL, NIL,
        G_STRING, NONE, NORMAL, (LONG) (intptr_t) "  Open  ",
        4, 4, (WORD) (popup_width - 8), state->char_h);
    demo23_init_object(&state->menu_tree[demo23_file_save],
        demo23_file_exit, NIL, NIL,
        G_STRING, NONE, NORMAL, (LONG) (intptr_t) "  Save  ",
        4, (WORD) (4 + state->char_h), (WORD) (popup_width - 8), state->char_h);
    demo23_init_object(&state->menu_tree[demo23_file_exit],
        demo23_menu_file, NIL, NIL,
        G_STRING, LASTOB, NORMAL, (LONG) (intptr_t) "  Exit  ",
        4, (WORD) (4 + state->char_h * 2), (WORD) (popup_width - 8), state->char_h);

    demo23_init_object(&state->menu_tree[demo23_help_about],
        demo23_menu_help, NIL, NIL,
        G_STRING, LASTOB, NORMAL,
        (LONG) (intptr_t) "  About GEM Demo  ",
        4, 4, (WORD) (state->char_w * 17), state->char_h);
}

static void demo23_redraw_window(demo23_state_t *state, const GRECT *dirty_rect)
{
    GRECT work_rect;
    GRECT clip_rect;
    WORD clip[4];
    WORD pxy[4];

    if (state == NULL) {
        return;
    }

    wind_update(BEG_UPDATE);
    wind_get(state->win_handle, WF_CXYWH, &work_rect.g_x, &work_rect.g_y,
        &work_rect.g_w, &work_rect.g_h);
    wind_get(state->win_handle, WF_FIRSTXYWH, &clip_rect.g_x, &clip_rect.g_y,
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
            clip[0] = x0;
            clip[1] = y0;
            clip[2] = x1;
            clip[3] = y1;
            vs_clip(state->vdi_handle, 1, clip);
            pxy[0] = x0;
            pxy[1] = y0;
            pxy[2] = x1;
            pxy[3] = y1;
            vr_recfl(state->vdi_handle, pxy);
            v_gtext(state->vdi_handle, (WORD) (work_rect.g_x + 10),
                (WORD) (work_rect.g_y + 20), state->message);
        }

        wind_get(state->win_handle, WF_NEXTXYWH, &clip_rect.g_x, &clip_rect.g_y,
            &clip_rect.g_w, &clip_rect.g_h);
    }

    vs_clip(state->vdi_handle, 0, NULL);
    wind_update(END_UPDATE);
}

static int demo23_handle_menu(demo23_state_t *state, WORD title, WORD item)
{
    if (state == NULL) {
        return 0;
    }

    if (title == demo23_title_desk) {
        if (item == demo23_desk_about) {
            (void) form_alert(1, "[1][ GEM Menu Demo ][ OK ]");
            strcpy(state->message, "About selected");
        } else if (item == demo23_desk_quit) {
            strcpy(state->message, "Quit selected - closing...");
            return 1;
        }
    } else if (title == demo23_title_file) {
        if (item == demo23_file_open) {
            strcpy(state->message, "Open selected");
        } else if (item == demo23_file_save) {
            strcpy(state->message, "Save selected");
        } else if (item == demo23_file_exit) {
            strcpy(state->message, "Exit selected");
            return 1;
        }
    } else if (title == demo23_title_help) {
        strcpy(state->message, "Help -> About GEM Demo");
    }

    demo23_draw_menu(state);
    demo23_redraw_window(state, NULL);
    return 0;
}

int main(void)
{
    WORD msg[8] = { 0 };
    WORD event_flags;
    WORD mouse_x = 0;
    WORD mouse_y = 0;
    WORD mouse_buttons = 0;
    WORD key_state = 0;
    WORD key_code = 0;
    WORD button_return = 0;
    GRECT full;
    GRECT current_rect;
    demo23_state_t state;

    memset(&state, 0, sizeof(state));
    strcpy(state.message, "Select a menu item...");
    for (int i = 0; i < 10; ++i) {
        state.work_in[i] = 1;
    }
    state.work_in[10] = 2;

    if (!gem_os_init()) {
        fprintf(stderr, "gem_os_init() failed\n");
        return 1;
    }

    state.appl_id = appl_init();
    if (state.appl_id < 0) {
        gem_os_shutdown();
        return 1;
    }

    state.vdi_handle = graf_handle(&state.char_w, &state.char_h,
        &state.box_w, &state.box_h);
    v_opnvwk(state.work_in, &state.vdi_handle, state.work_out);

    demo23_init_menu_tree(&state);
    (void) menu_bar(state.menu_tree, 1);

    wind_get(0, WF_WXYWH, &full.g_x, &full.g_y, &full.g_w, &full.g_h);
    state.win_handle = wind_create((UWORD) (NAME | CLOSER | FULLER | MOVER |
        SIZER), full.g_x, full.g_y, full.g_w, full.g_h);
    if (state.win_handle < 0) {
        (void) menu_bar(NULL, 0);
        v_clsvwk(state.vdi_handle);
        appl_exit();
        gem_os_shutdown();
        return 1;
    }

    (void) wind_set_str(state.win_handle, WF_NAME, "GEM Menu Demo");
    (void) wind_open(state.win_handle, (WORD) (full.g_x + 20),
        (WORD) (full.g_y + state.char_h + 28), (WORD) (full.g_w - 40),
        (WORD) (full.g_h - state.char_h - 60));
    (void) wind_get(state.win_handle, WF_WXYWH, &current_rect.g_x,
        &current_rect.g_y, &current_rect.g_w, &current_rect.g_h);

    demo23_draw_menu(&state);
    demo23_redraw_window(&state, NULL);

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
            switch (msg[0]) {
            case WM_REDRAW: {
                GRECT dirty_rect;

                dirty_rect.g_x = msg[4];
                dirty_rect.g_y = msg[5];
                dirty_rect.g_w = msg[6];
                dirty_rect.g_h = msg[7];
                demo23_redraw_window(&state, &dirty_rect);
                break;
            }
            case WM_CLOSED:
                goto cleanup;
            case WM_MOVED:
            case WM_SIZED:
                (void) wind_set(state.win_handle, WF_WXYWH, msg[4], msg[5],
                    msg[6], msg[7]);
                break;
            case MN_SELECTED:
                if (demo23_handle_menu(&state, msg[3], msg[4]) != 0) {
                    goto cleanup;
                }
                (void) menu_tnormal(state.menu_tree, msg[3], 1);
                break;
            default:
                break;
            }
        }

        if ((event_flags & MU_KEYBD) != 0 && key_code == 0x011b) {
            break;
        }
    }

cleanup:
    (void) menu_bar(NULL, 0);
    if (state.win_handle > 0) {
        (void) wind_close(state.win_handle);
        (void) wind_delete(state.win_handle);
    }
    v_clsvwk(state.vdi_handle);
    appl_exit();
    gem_os_shutdown();
    return 0;
}
