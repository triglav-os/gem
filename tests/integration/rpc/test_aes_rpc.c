/*
 * Verifies client-owned AES trees, editable text, resources, shell state and
 * USERDEF callbacks against a real server, including pointer preservation.
 *
 * MIT License (see: LICENSE)
 * Copyright (C) 2026 tomaz stih
 */
#include <gem.h>
#include <gem/gemconf.h>
#include <gem/os.h>
#include "gem_protocol.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

typedef struct shell_child_result {
    WORD app_id;
    WORD argument_count;
    char first_argument[128];
    char command[256];
    char tail[128];
} shell_child_result_t;

static WORD handle;
static int callback_count;

_Static_assert(sizeof(gem_rpc_location_list_t) == 18564u,
               "location-list wire ABI changed");

static int location_present(const gem_rpc_location_list_t *locations,
                            const char *path, const char *label)
{
    uint32_t index;

    for (index = 0u; index < locations->count; ++index)
        if (strcmp(locations->entries[index].path, path) == 0 &&
            strcmp(locations->entries[index].label, label) == 0)
            return 1;
    return 0;
}

static void read_shell_result(const char *path, shell_child_result_t *result)
{
    gem_os_file_info_t info;
    unsigned attempt;
    int fd = -1;

    for (attempt = 0u; attempt < 100u; ++attempt) {
        if (gem_os_stat_path(path, &info) != 0 &&
            info.size_bytes == sizeof(*result)) {
            fd = gem_os_open_read(path);
        }
        if (fd >= 0) {
            break;
        }
        gem_os_sleep_ms(20u);
    }
    assert(fd >= 0);
    assert(gem_os_read(fd, result, sizeof(*result)) ==
           (int32_t)sizeof(*result));
    assert(gem_os_close(fd) == 0);
}

static WORD draw_user(LONG value)
{
    PARMBLK *parm = (PARMBLK *)(intptr_t)value;
    assert(parm->pb_parm == 1234);
    assert(parm->pb_obj == 3 && parm->pb_tree);
    WORD box[4] = {parm->pb_x, parm->pb_y, (WORD)(parm->pb_x + parm->pb_w - 1),
                   (WORD)(parm->pb_y + parm->pb_h - 1)};
    vsf_color(handle, WHITE);
    v_bar(handle, box);
    ++callback_count;
    return 0;
}
int main(void)
{
    WORD app = appl_init();
    WORD menu_id;
    OBJECT bar[7] = {{-1, 1, 4, G_IBOX, 0, 0, 0, 0, 0, 640, 20},
                     {4, 2, 2, G_BOX, 0, 0, 0, 0, 0, 640, 20},
                     {1, 3, 3, G_IBOX, 0, 0, 0, 0, 0, 640, 20},
                     {2, -1, -1, (UWORD)(0x2100 | G_TITLE), 0, 0,
                      (LONG)(intptr_t)" Menu ", 0, 0, 48, 20},
                     {0, 5, 5, G_IBOX, 0, 0, 0, 0, 20, 640, 380},
                     {4, 6, 6, G_BOX, 0, 0, 0, 0, 0, 100, 16},
                     {5, -1, -1, (UWORD)(0x2100 | G_STRING), LASTOB, 0,
                      (LONG)(intptr_t)"  Item", 0, 0, 100, 16}};
    assert(app > 0);
    assert(fsel_options(0, 0) == 0);
    assert(fsel_options(FSEL_SHOW_HIDDEN, 1) == FSEL_SHOW_HIDDEN);
    assert(fsel_options(0, 0) == FSEL_SHOW_HIDDEN);
    assert(fsel_options(0, 1) == 0);
    menu_id = menu_register(app, "API TEST");
    assert(menu_id >= 0 && menu_id < 6);
    assert(appl_find("API TEST") == app);
    assert(menu_unregister(menu_id));
    assert(menu_bar(bar, 1) == 1);
    WORD message[8] = {1000, app, 0, 11, 22, 33, 44, 55}, received[8] = {0};
    assert(appl_write(app, 8, message));
    assert(appl_read(app, 8, received));
    assert(!memcmp(message, received, sizeof(message)));
    char cmd[260], tail[128] = {0}, dir[260];
    assert(shel_read(cmd, tail));
    assert(strstr(cmd, "test_aes_rpc") != NULL && tail[0] == 0);
    assert(shel_write(0, 0, 0, "test.prg", tail));
    assert(shel_read(cmd, tail));
    assert(strstr(cmd, "test_aes_rpc") != NULL && tail[0] == 0);
    assert(shel_wdef("other.prg", "/test"));
    assert(shel_rdef(cmd, dir));
    assert(!strcmp(cmd, "other.prg") && !strcmp(dir, "/test"));
    {
        static char desktop_inf[] =
            "#E3A01\r\n#G0C2C EDIT.APP@ *.TXT@\r\n\x1a\0";
        char back[128];
        WORD available;

        memset(back, 'x', sizeof(back));
        assert(shel_put(desktop_inf, (WORD)(sizeof(desktop_inf) - 1u)));
        available = shel_get(NULL, SHEL_BUFSIZE);
        assert(available > 2 && available < (WORD)sizeof(back));
        assert(shel_get(back, sizeof(back)));
        assert(strstr(back, "EDIT.APP@") != NULL);
        assert(strstr(back, "*.TXT@") != NULL);
        assert(back[available - 2] == 0x1a && back[available - 1] == '\0');
        assert(back[available] == 'x');
    }
    {
        gemconf_t config;
        gem_rpc_location_list_t locations;
        char back[256] = {0};
        unsigned attempt;
        int found = 0;

        gemconf_init(&config);
        assert(gemconf_load(&config, "desktop"));
        assert(gemconf_set(&config, "desktop", "show_hidden", "true"));
        assert(gemconf_set(&config, "locations", "/tmp", "Watched"));
        assert(gemconf_save(&config));
        gemconf_clear(&config);
        for (attempt = 0u; attempt < 50u && found == 0; ++attempt) {
            gem_os_sleep_ms(20u);
            assert(gem_location_list(&locations));
            found = location_present(&locations, "/tmp", "Watched");
        }
        assert(found);
        assert((GEM_RPC_LOCATION_LIST_OPTIONS(locations) &
                GEM_RPC_DESKTOP_SHOW_HIDDEN) != 0u);
        assert(shel_get(back, sizeof(back)));
        assert(strstr(back, "Watched@") != NULL);
    }
    {
        char *home = NULL;
        char *leaked = NULL;
        char clock_path[260] = "clock.app";
        char clock_result_path[GEM_OS_PATH_MAX];
        char term_result_path[GEM_OS_PATH_MAX];
        char clock_tail[128] = {0};
        char term_tail[128] = {9, 'n', 'o', 't', 'e', 's', '.', 't', 'x', 't'};
        shell_child_result_t clock_result;
        shell_child_result_t term_result;
        int length;

        assert(shel_envrn(&home, "GEMIX_HOME="));
        assert(!shel_envrn(&leaked, "GEM_SHEL_CMD="));
        length = snprintf(clock_result_path, sizeof(clock_result_path),
                          "%s/clock.result", home);
        assert(length > 0 && (size_t)length < sizeof(clock_result_path));
        length = snprintf(term_result_path, sizeof(term_result_path),
                          "%s/term.result", home);
        assert(length > 0 && (size_t)length < sizeof(term_result_path));
        (void)gem_os_unlink(clock_result_path);
        (void)gem_os_unlink(term_result_path);
        assert(shel_find(clock_path));
        assert(clock_path[0] == '/');
        assert(shel_write(1, 1, 1, "clock.app", clock_tail));
        assert(shel_write(1, 1, 1, "term.app", term_tail));
        assert(!shel_write(1, 1, 1, "missing-gem-app", clock_tail));
        read_shell_result(clock_result_path, &clock_result);
        read_shell_result(term_result_path, &term_result);
        assert(clock_result.app_id > 0 && term_result.app_id > 0);
        assert(clock_result.app_id != term_result.app_id);
        assert(!strcmp(clock_result.command, "clock.app"));
        assert(clock_result.argument_count == 1);
        assert(clock_result.tail[0] == 0);
        assert(!strcmp(term_result.command, "term.app"));
        assert(term_result.argument_count == 2);
        assert(!strcmp(term_result.first_argument, "notes.txt"));
        assert((unsigned char)term_result.tail[0] == 9u);
        assert(!memcmp(term_result.tail + 1, "notes.txt", 9u));
        assert(!shel_envrn(&leaked, "GEM_SHEL_CMD="));
    }
    handle = graf_handle(NULL, NULL, NULL, NULL);
    assert(handle);
    WORD window = wind_create(NAME | CLOSER, 0, 0, 640, 400);
    assert(window > 0);
    assert(wind_open(window, 20, 20, 400, 300));
    /* The work-area selector must round-trip; the frame then differs from
     * it by the kind's borders in both directions. */
    WORD wx, wy, ww, wh, fx, fy, fw, fh;
    assert(wind_set(window, WF_CXYWH, 40, 60, 300, 200) == 1);
    assert(wind_get(window, WF_CXYWH, &wx, &wy, &ww, &wh));
    assert(wx == 40 && wy == 60 && ww == 300 && wh == 200);
    assert(wind_get(window, WF_WXYWH, &fx, &fy, &fw, &fh));
    assert(fx < wx && fy < wy && fw > ww && fh > wh);
    assert(wind_set(window, WF_WXYWH, 20, 20, 400, 300) == 1);
    char buffer[16] = "AB";
    TEDINFO ted = {0};
    ted.te_ptext = (LONG)(intptr_t)buffer;
    ted.te_ptmplt = (LONG)(intptr_t)"_______________";
    ted.te_pvalid = (LONG)(intptr_t)"X";
    ted.te_txtlen = sizeof(buffer);
    ted.te_tmplen = 16;
    ted.te_font = 3;
    USERBLK user = {(LONG)(intptr_t)draw_user, 1234};
    OBJECT tree[4] = {{-1, 1, 3, G_IBOX, 0, 0, 0, 50, 70, 250, 150},
                      {2, -1, -1, G_FBOXTEXT, EDITABLE, 0, (LONG)(intptr_t)&ted,
                       10, 10, 100, 20},
                      {3, -1, -1, G_BUTTON, SELECTABLE | EXIT | DEFAULT, 0,
                       (LONG)(intptr_t)"OK", 10, 45, 50, 20},
                      {0, -1, -1, G_USERDEF, LASTOB, 0, (LONG)(intptr_t)&user,
                       80, 45, 50, 20}};
    WORD index = 2;
    assert(objc_edit(tree, 1, 'C', &index, EDCHAR));
    assert(index == 3 && !strcmp(buffer, "ABC"));
    assert(tree[1].ob_spec == (LONG)(intptr_t)&ted &&
           ted.te_ptext == (LONG)(intptr_t)buffer);
    assert(objc_draw(tree, ROOT, MAX_DEPTH, 50, 70, 250, 150));
    assert(callback_count == 1);
    WORD pel, color;
    assert(v_get_pixel(handle, 135, 120, &pel, &color) && pel == 1);
    WORD x, y, w, h;
    assert(form_center(tree, &x, &y, &w, &h));
    assert(w == 250 && h == 150);
    WORD next = 0, character = 0;
    (void)form_keybd(tree, 1, 2, 9, &next, &character);
    assert(form_button(tree, 2, 1, &next) == 1 && next == 2);
    assert(graf_watchbox(tree, 2, SELECTED, NORMAL));
    assert(objc_delete(tree, 2));
    assert(objc_add(tree, ROOT, 2));
    assert(objc_order(tree, 2, 0));
    assert(tree[ROOT].ob_head == 2);
    assert(rsrc_load("demo27.rsc"));
    OBJECT *loaded = NULL;
    assert(rsrc_gaddr(R_TREE, 0, (void **)&loaded));
    assert(!rsrc_gaddr(R_TREE, 32767, (void **)&loaded));
    for (WORD i = 0; i < 4; ++i)
        assert(rsrc_obfix(loaded, i));
    assert(
        !strcmp((char *)(intptr_t)loaded[1].ob_spec, "Loaded from demo27.rsc"));
    assert(rsrc_free());
    /* Extended object types keep the low byte; the wrapper must still send
     * the title/entry strings gemd requires for that byte. */
    assert(menu_bar(bar, 0) == 1);
    wind_close(window);
    wind_delete(window);
    assert(!menu_unregister(menu_id));
    assert(appl_exit());
    return 0;
}
