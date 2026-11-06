/*
 * Checks hostile hosted framebuffer paths and application-id rollover using
 * private implementation state, without changing the public GEM interfaces.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (C) 2026 tomaz stih
 */
#define _POSIX_C_SOURCE 200809L
#include "../../../src/aes/window_internal.h"
#include <gem/input.h>
#include "../../../src/aes/fsel_private.h"
#include "gem/gemctl.h"
#include "platform/raster.h"

#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static void framebuffer_paths(void)
{
    char directory[] = "/tmp/gem-host-test.XXXXXXXX";
    char target[128], path[128];
    struct stat st;
    FILE *file;
    assert(mkdtemp(directory));
    snprintf(target, sizeof(target), "%s/target", directory);
    snprintf(path, sizeof(path), "%s/framebuffer", directory);
    file = fopen(target, "wb");
    assert(file && fputs("preserved", file) >= 0 && fclose(file) == 0);
    assert(setenv("GEM_RASTA_FRAMEBUFFER", path, 1) == 0);
    assert(symlink(target, path) == 0);
    assert(!gem_raster_init(80, 80, GEM_RASTER_MONO1));
    assert(stat(target, &st) == 0 && st.st_size == 9);
    assert(unlink(path) == 0 && link(target, path) == 0);
    assert(!gem_raster_init(80, 80, GEM_RASTER_MONO1));
    assert(stat(target, &st) == 0 && st.st_size == 9);
    assert(unlink(path) == 0 && mkfifo(path, 0600) == 0);
    assert(!gem_raster_init(80, 80, GEM_RASTER_MONO1));
    assert(unlink(path) == 0);
    assert(gem_raster_init(80, 80, GEM_RASTER_MONO1));
    assert(stat(path, &st) == 0 && st.st_size == 80 * 80 * 4 &&
           (st.st_mode & 0777) == 0600);
    memset(gem_raster_surface()->pixels, 0x5a, 800);
    gem_raster_present();
    file = fopen(path, "rb");
    assert(file && fgetc(file) == 0xff && fgetc(file) == 0xff &&
           fgetc(file) == 0xff && fgetc(file) == 0xff &&
           fgetc(file) == 0 && fgetc(file) == 0 && fgetc(file) == 0 &&
           fgetc(file) == 0xff && fclose(file) == 0);
    /* The viewer may replace its file when its geometry changes. */
    assert(unlink(path) == 0);
    file = fopen(path, "wb");
    assert(file && fclose(file) == 0);
    gem_raster_present_rect(0, 0, 1, 1);
    file = fopen(path, "rb");
    assert(file && fseek(file, 80 * 80 * 4 - 1, SEEK_SET) == 0 &&
           fgetc(file) == 0xff);
    assert(fclose(file) == 0);
    assert(truncate(path, 0) == 0);
    gem_raster_present_rect(0, 0, 1, 1);
    file = fopen(path, "rb");
    assert(file && fseek(file, 80 * 80 * 4 - 1, SEEK_SET) == 0 &&
           fgetc(file) == 0xff);
    assert(fclose(file) == 0);
    gem_raster_present_rect(INT_MAX, INT_MAX, INT_MAX, INT_MAX);
    gem_raster_present_rect(INT_MIN, INT_MIN, INT_MAX, INT_MAX);
    assert(gem_raster_resync());
    file = fopen(path, "rb");
    assert(file && fgetc(file) == 0xff && fclose(file) == 0);
    gem_raster_shutdown();
    assert(unlink(path) == 0 && unlink(target) == 0 && rmdir(directory) == 0);
}

static void application_ids(void)
{
    WORD first = appl_init(), last, wrapped;
    assert(first == 1);
    assert(aes_state.active_app_id == first &&
           aes_state.input_focus_app_id == first);
    assert(form_dial(FMD_START, 0, 0, 0, 0, 10, 20, 100, 80));
    assert(aes_state.modal_form_owner_app_id == first);
    assert(aes_state.modal_form_rect.g_x == 10 &&
           aes_state.modal_form_rect.g_y == 20 &&
           aes_state.modal_form_rect.g_w == 100 &&
           aes_state.modal_form_rect.g_h == 80);
    assert(form_dial(FMD_FINISH, 0, 0, 0, 0, 10, 20, 100, 80));
    assert(aes_state.modal_form_owner_app_id == 0);
    aes_state.next_app_id = 32767;
    last = appl_init();
    wrapped = appl_init();
    assert(last == 32767 && wrapped == 2);
    assert(aes_find_app_by_id(first) && aes_find_app_by_id(last));
    assert(appl_exit());
    aes_state.current_app_id = last;
    assert(appl_exit());
    aes_state.current_app_id = first;
    assert(appl_exit());
}

static OBJECT *expected_modal_tree;

static int stop_form_wait(void)
{
    assert(aes_state.modal_form_tree == expected_modal_tree);
    return 0;
}

static void custom_form_input_scope(void)
{
    gem_hid_event_t event;
    GRECT visible[64];
    OBJECT form_tree[1];
    WORD visible_count;
    aes_app_t *dialog_app;
    aes_app_t *window_app;
    WORD dialog_id;
    WORD window_id;

    aes_reset_state();
    aes_state.initialized = 1;
    dialog_id = appl_init();
    window_id = appl_init();
    dialog_app = aes_find_app_by_id(dialog_id);
    window_app = aes_find_app_by_id(window_id);
    assert(dialog_app != NULL && window_app != NULL);

    aes_state.windows[0].used = 1;
    aes_state.windows[0].handle = 1;
    aes_state.windows[0].owner = window_id;
    aes_state.windows[0].open = 1;
    aes_state.windows[0].z_order = 1u;
    aes_state.next_window_z = 2u;
    aes_set_rect(&aes_state.windows[0].outer, 200, 200, 100, 100);
    aes_state.windows[0].work = aes_state.windows[0].outer;

    aes_state.current_app_id = dialog_id;
    assert(form_dial(FMD_START, 0, 0, 0, 0, 10, 20, 100, 80));
    assert(aes_state.modal_form_z == 2u);

    memset(form_tree, 0, sizeof(form_tree));
    form_tree[ROOT].ob_next = NIL;
    form_tree[ROOT].ob_head = NIL;
    form_tree[ROOT].ob_tail = NIL;
    expected_modal_tree = form_tree;
    aes_wait_hook = stop_form_wait;
    assert(form_do(form_tree, NIL) == NIL);
    assert(aes_state.modal_form_tree == NULL);
    aes_wait_hook = NULL;

    memset(&event, 0, sizeof(event));
    event.type = GEM_HID_MOUSE_BUTTON;
    event.flags = GEM_HID_BUTTON_LEFT;
    event.button = GEM_HID_BUTTON_LEFT;
    event.x = 250;
    event.y = 250;
    aes_dispatch_hid_event(&event);
    assert(window_app->input_event_count == 1);
    assert(dialog_app->input_event_count == 0);
    assert(aes_state.input_focus_app_id == window_id);

    aes_state.windows[0].z_order = 3u;
    aes_state.next_window_z = 4u;
    event.x = 50;
    event.y = 50;
    aes_dispatch_hid_event(&event);
    assert(window_app->input_event_count == 1);
    assert(dialog_app->input_event_count == 1);
    assert(aes_state.input_focus_app_id == dialog_id);
    assert(aes_state.modal_form_z == 4u);

    aes_set_rect(&aes_state.windows[0].outer, 40, 30, 30, 30);
    aes_state.windows[0].work = aes_state.windows[0].outer;
    aes_state.windows[0].z_order = 5u;
    visible_count =
        aes_clip_modal_form_rects(&aes_state.modal_form_rect, visible, 64);
    assert(visible_count > 0);
    for (WORD i = 0; i < visible_count; ++i) {
        assert(aes_rects_intersect(&visible[i], &aes_state.windows[0].outer) ==
               0);
    }
}

static void selector_patterns(void)
{
    static const struct {
        const char *pattern, *name;
        int matches;
    } cases[] = {{"", "", 1},
                 {"", "a", 0},
                 {"*", "", 1},
                 {"?", "", 0},
                 {"?", "a", 1},
                 {"?", "ab", 0},
                 {"*.TXT", "notes.txt", 1},
                 {"*.txt", "notes.png", 0},
                 {"a*b*c", "axbyc", 1},
                 {"a*b*c", "axbycd", 0},
                 {"**a***?*", "AB", 1},
                 {"*ab*ab", "abab", 1},
                 {"*ab*ab", "aba", 0},
                 {"*.?", "name.c", 1},
                 {"*.?", "name.cpp", 0}};
    char hostile[202], name[201];
    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); ++i)
        assert(aes_fsel_match_pattern(cases[i].pattern, cases[i].name) ==
               cases[i].matches);
    for (size_t i = 0; i < 100; ++i) {
        hostile[i * 2] = '*';
        hostile[i * 2 + 1] = 'a';
    }
    hostile[200] = 'b';
    hostile[201] = '\0';
    memset(name, 'a', 200);
    name[200] = '\0';
    assert(!aes_fsel_match_pattern(hostile, name));
    hostile[200] = '*';
    assert(aes_fsel_match_pattern(hostile, name));
}

static int selector_has_entry(const aes_fsel_state_t *state, const char *name)
{
    WORD index;

    for (index = 0; index < state->entry_count; ++index) {
        if (strcmp(state->entries[index].name, name) == 0) {
            return 1;
        }
    }
    return 0;
}

static void selector_hidden_entries(void)
{
    char directory[] = "/tmp/gem-fsel-test.XXXXXXXX";
    char visible[128];
    char hidden[128];
    aes_fsel_state_t state;
    FILE *file;

    assert(mkdtemp(directory));
    snprintf(visible, sizeof(visible), "%s/visible.pdf", directory);
    snprintf(hidden, sizeof(hidden), "%s/.hidden.pdf", directory);
    file = fopen(visible, "wb");
    assert(file != NULL && fclose(file) == 0);
    file = fopen(hidden, "wb");
    assert(file != NULL && fclose(file) == 0);

    memset(&state, 0, sizeof(state));
    strcpy(state.directory, directory);
    strcpy(state.pattern, "*");
    state.mode = AES_FSEL_OPEN;
    state.selected = NIL;
    aes_fsel_reload_entries(&state);
    assert(selector_has_entry(&state, "visible.pdf"));
    assert(!selector_has_entry(&state, ".hidden.pdf"));
    assert(fsel_options(FSEL_SHOW_HIDDEN, 1) == FSEL_SHOW_HIDDEN);
    state.show_hidden = 1;
    aes_fsel_reload_entries(&state);
    assert(selector_has_entry(&state, ".hidden.pdf"));
    assert(fsel_options(0, 1) == 0);

    assert(unlink(hidden) == 0 && unlink(visible) == 0 && rmdir(directory) == 0);
}

static void text_editor_hid_collisions(void)
{
    char text[32] = "replace me";
    gemctl_text_edit_t editor;

    gemctl_text_edit_init(&editor, text, sizeof(text));
    gemctl_text_edit_focus(&editor, 1);
    assert(gemctl_text_edit_key(&editor, (uint16_t)((28u << 8) | 'y')) ==
           GEMCTL_TEXT_EDIT_CHANGED);
    assert(gemctl_text_edit_key(&editor, (uint16_t)((14u << 8) | 'k')) ==
           GEMCTL_TEXT_EDIT_CHANGED);
    assert(gemctl_text_edit_key(&editor, (uint16_t)((15u << 8) | 'l')) ==
           GEMCTL_TEXT_EDIT_CHANGED);
    assert(strcmp(text, "ykl") == 0);
    assert(gemctl_text_edit_key(&editor, (uint16_t)(40u << 8)) ==
           GEMCTL_TEXT_EDIT_ACCEPT);
}

static void physical_keyboard_extension(void)
{
    WORD key = 0, state = 0;
    gem_hid_event_t event = {0};
    aes_reset_state();
    aes_state.initialized = 1;
    WORD app = appl_init();
    aes_state.input_focus_app_id = app;
    aes_external_input = 1;
    event.type = GEM_HID_KEY;
    event.key = (4u << 8) | 'a';
    event.flags = 1;
    aes_dispatch_hid_event(&event);
    event.flags = 0;
    aes_dispatch_hid_event(&event);
    for (int pressed = 1; pressed >= 0; --pressed) {
        assert(evnt_multi(GEM_MU_KEY | MU_TIMER, 0, 0, 0,
                          0, 0, 0, 0, 0, 0, 0, 0, 0, 0, NULL,
                          0, 0, NULL, NULL, NULL, NULL, &key, &state) ==
               GEM_MU_KEY);
        assert(key == (WORD)((4u << 8) | 'a') && state == pressed);
    }
    /* Standard clients still receive only the original key-down packet. */
    event.flags = 1;
    aes_dispatch_hid_event(&event);
    event.flags = 0;
    aes_dispatch_hid_event(&event);
    assert(evnt_multi(MU_KEYBD | MU_TIMER, 0, 0, 0,
                      0, 0, 0, 0, 0, 0, 0, 0, 0, 0, NULL,
                      0, 0, NULL, NULL, NULL, NULL, &key, &state) == MU_KEYBD);
    assert(key == (WORD)((4u << 8) | 'a'));
    assert(evnt_multi(MU_KEYBD | MU_TIMER, 0, 0, 0,
                      0, 0, 0, 0, 0, 0, 0, 0, 0, 0, NULL,
                      0, 0, NULL, NULL, NULL, NULL, &key, &state) == MU_TIMER);
    for (int i = 0; i <= AES_APP_INPUT_QUEUE_SIZE; ++i)
        aes_dispatch_hid_event(&event);
    assert(evnt_multi(GEM_MU_KEY | MU_TIMER, 0, 0, 0,
                      0, 0, 0, 0, 0, 0, 0, 0, 0, 0, NULL,
                      0, 0, NULL, NULL, NULL, NULL, &key, &state) == GEM_MU_KEY);
    assert(state == GEM_KEY_RESET);
    assert(aes_find_app_by_id(app)->input_event_count == 0);
    aes_external_input = 0;
    assert(appl_exit());
}

/* A background title update must not paint its scrollbar over the top window. */
static void background_window_title(void)
{
    char directory[] = "/tmp/gem-title-test.XXXXXXXX";
    char path[128];
    WORD back, front, handle, x, y, w, h, pel, color;
    WORD fill[4];

    assert(mkdtemp(directory));
    snprintf(path, sizeof(path), "%s/framebuffer", directory);
    assert(setenv("GEM_RASTA_FRAMEBUFFER", path, 1) == 0);
    assert(appl_init() > 0);
    handle = graf_handle(NULL, NULL, NULL, NULL);
    assert(handle > 0);
    back = wind_create(NAME | MOVER | VSLIDE | HSLIDE, 0, 0, 640, 400);
    front = wind_create(NAME | MOVER, 0, 0, 640, 400);
    assert(back > 0 && front > 0);
    assert(wind_open(back, 100, 100, 240, 200));
    assert(wind_open(front, 40, 40, 340, 280));
    for (int partial = 0; partial < 2; ++partial) {
        if (partial != 0) {
            assert(wind_set(front, WF_WXYWH, 180, 140, 180, 150));
        }
        assert(wind_get(front, WF_CXYWH, &x, &y, &w, &h));
        fill[0] = x;
        fill[1] = y;
        fill[2] = (WORD)(x + w - 1);
        fill[3] = (WORD)(y + h - 1);
        vs_clip(handle, 0, fill);
        vsf_interior(handle, 1);
        vsf_color(handle, 1);
        vswr_mode(handle, MD_REPLACE);
        v_bar(handle, fill);
        WORD *before = malloc((size_t)w * (size_t)h * sizeof(*before));
        assert(before != NULL);
        for (WORD row = 0; row < h; ++row) {
            for (WORD col = 0; col < w; ++col) {
                assert(v_get_pixel(handle, x + col, y + row, &pel, &color));
                before[(size_t)row * (size_t)w + (size_t)col] = pel;
            }
        }
        assert(wind_set_str(back, WF_NAME, partial ? "PC 1234" : "PC 5678"));
        assert(wind_set_str(back, WF_INFO, partial ? "Running" : "Paused"));
        for (WORD row = y; row < y + h; ++row) {
            for (WORD col = x; col < x + w; ++col) {
                assert(v_get_pixel(handle, col, row, &pel, &color));
                assert(pel == before[(size_t)(row - y) * (size_t)w +
                                     (size_t)(col - x)]);
            }
        }
        free(before);
    }
    assert(wind_close(front) && wind_delete(front));
    assert(strcmp(aes_find_window(back)->name, "PC 1234") == 0);
    assert(wind_close(back) && wind_delete(back));
    assert(appl_exit());
    assert(unlink(path) == 0 && rmdir(directory) == 0);
}

/* SIZER alone keeps a thin frame and never steals a client-area click. */
static void compact_window_geometry(void)
{
    aes_window_t window = {0};
    GRECT outer = {40, 50, 320, 240};
    GRECT work;
    GRECT round_trip;
    const UWORD base = NAME | CLOSER | FULLER | MOVER | SIZER;
    const UWORD scroll[] = {0, VSLIDE, HSLIDE, VSLIDE | HSLIDE,
                            UPARROW, DNARROW, LFARROW, RTARROW};

    for (size_t i = 0; i < sizeof(scroll) / sizeof(scroll[0]); ++i) {
        window.kind = (UWORD)(base | scroll[i]);
        window.open = 1;
        window.outer = outer;
        aes_compute_work(&window);
        work = window.work;
        assert(wind_calc(WC_WORK, window.kind, outer.g_x, outer.g_y,
                         outer.g_w, outer.g_h, &round_trip.g_x,
                         &round_trip.g_y, &round_trip.g_w, &round_trip.g_h));
        assert(memcmp(&work, &round_trip, sizeof(work)) == 0);
        assert(wind_calc(WC_BORDER, window.kind, work.g_x, work.g_y,
                         work.g_w, work.g_h, &round_trip.g_x,
                         &round_trip.g_y, &round_trip.g_w, &round_trip.g_h));
        assert(memcmp(&outer, &round_trip, sizeof(outer)) == 0);
        if (scroll[i] == 0) {
            assert(outer.g_x + outer.g_w - work.g_x - work.g_w == 4);
            assert(outer.g_y + outer.g_h - work.g_y - work.g_h == 4);
            for (WORD y = (WORD)(outer.g_y + outer.g_h - 12);
                 y < outer.g_y + outer.g_h; ++y) {
                for (WORD x = (WORD)(outer.g_x + outer.g_w - 12);
                     x < outer.g_x + outer.g_w; ++x) {
                    WORD part = aes_window_hit_part(&window, x, y);
                    assert((part == AES_WINDOW_PART_SIZER) ==
                           !aes_point_in_rect(x, y, &work));
                }
            }
        } else {
            assert(aes_window_right_border(&window) ==
                   aes_window_scroll_span(&window));
            assert(aes_window_bottom_border(&window) ==
                   aes_window_scroll_span(&window));
        }
    }
    window.kind = base;
    for (WORD side = 1; side < 24; ++side) {
        GRECT grip;
        window.outer.g_w = side;
        window.outer.g_h = side;
        if (aes_window_sizer_rect(&window, &grip)) {
            assert(grip.g_x >= window.outer.g_x &&
                   grip.g_y >= window.outer.g_y);
            assert(grip.g_w <= side && grip.g_h <= side);
        }
    }
    window.outer = outer;
    window.kind = NAME | CLOSER | MOVER;
    aes_compute_work(&window);
    assert(aes_window_hit_part(&window, 359, 289) != AES_WINDOW_PART_SIZER);
}

int main(void)
{
    compact_window_geometry();
    framebuffer_paths();
    application_ids();
    custom_form_input_scope();
    selector_patterns();
    selector_hidden_entries();
    text_editor_hid_collisions();
    physical_keyboard_extension();
    background_window_title();
    puts("hostile framebuffer paths, viewer replacement and app-id rollover "
         "passed");
    return 0;
}
