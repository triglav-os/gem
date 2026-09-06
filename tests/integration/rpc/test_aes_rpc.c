/*
 * Verifies client-owned AES trees, editable text, resources, shell state and
 * USERDEF callbacks against a real server, including pointer preservation.
 *
 * MIT License (see: LICENSE)
 * Copyright (C) 2026 tomaz stih
 */
#include <gem.h>
#include <assert.h>
#include <stdint.h>
#include <string.h>
static WORD handle;
static int callback_count;
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
    assert(app > 0);
    assert(menu_register(app, "API TEST") == app);
    assert(appl_find("API TEST") == app);
    WORD message[8] = {1000, app, 0, 11, 22, 33, 44, 55}, received[8] = {0};
    assert(appl_write(app, 8, message));
    assert(appl_read(app, 8, received));
    assert(!memcmp(message, received, sizeof(message)));
    char cmd[260], tail[260], dir[260];
    assert(shel_write(0, 0, 0, "test.prg", "args"));
    assert(shel_read(cmd, tail));
    assert(!strcmp(cmd, "test.prg") && !strcmp(tail, "args"));
    assert(shel_wdef("other.prg", "/test"));
    assert(shel_rdef(cmd, dir));
    assert(!strcmp(cmd, "other.prg") && !strcmp(dir, "/test"));
    char odd[3] = {'a', 'b', 'c'}, back[3] = {0};
    assert(shel_put(odd, 3));
    assert(shel_get(back, 3));
    assert(!memcmp(odd, back, 3));
    handle = graf_handle(NULL, NULL, NULL, NULL);
    assert(handle);
    WORD window = wind_create(NAME | CLOSER, 0, 0, 640, 400);
    assert(window > 0);
    assert(wind_open(window, 20, 20, 400, 300));
    char buffer[16] = "AB";
    TEDINFO ted = {0};
    ted.te_ptext = (LONG)(intptr_t)buffer;
    ted.te_ptmplt = (LONG)(intptr_t) "_______________";
    ted.te_pvalid = (LONG)(intptr_t) "X";
    ted.te_txtlen = sizeof(buffer);
    ted.te_tmplen = 16;
    ted.te_font = 3;
    USERBLK user = {(LONG)(intptr_t)draw_user, 1234};
    OBJECT tree[4] = {{-1, 1, 3, G_IBOX, 0, 0, 0, 50, 70, 250, 150},
                      {2, -1, -1, G_FBOXTEXT, EDITABLE, 0, (LONG)(intptr_t)&ted,
                       10, 10, 100, 20},
                      {3, -1, -1, G_BUTTON, SELECTABLE | EXIT | DEFAULT, 0,
                       (LONG)(intptr_t) "OK", 10, 45, 50, 20},
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
    wind_close(window);
    wind_delete(window);
    assert(menu_unregister(app));
    assert(appl_exit());
    return 0;
}
