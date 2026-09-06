/*
 * Demonstrates loading an external AES resource file and drawing one
 * dialog tree from it. This is the first sample in the series that
 * deliberately goes through `rsrc_load()` and `rsrc_gaddr()` instead of
 * keeping the object tree in C source.
 *
 * MIT License (see: LICENSE)
 * Copyright (C) 2026 tomaz stih
 */

#include <gem.h>
#include <string.h>

#include "demo27.h"

WORD appl_id;
VDI_HANDLE vdi_handle;
WORD work_in[11] = {1,1,1,1,1,1,1,1,1,1,2};
WORD work_out[57];

int main(void)
{
    OBJECT *tree = NULL;
    WORD cx;
    WORD cy;
    WORD cw;
    WORD ch;
    WORD mx = 0;
    WORD my = 0;
    WORD mb = 0;
    WORD ks = 0;
    WORD kr = 0;
    WORD br = 0;
    WORD event;
    WORD object;
    WORD last_object;
    GRECT rect;
    WORD ox;
    WORD oy;
    char alert_text[48];

    appl_id = appl_init();
    if (appl_id < 0) {
        return 1;
    }

    vdi_handle = graf_handle();
    v_opnvwk(work_in, &vdi_handle, work_out);

    if (!rsrc_load("demo27.rsc")) {
        form_alert(1, "[1][ Cannot load demo27.rsc ][ Abort ]");
        v_clsvwk(vdi_handle);
        appl_exit();
        return 1;
    }

    if (!rsrc_gaddr(R_TREE, DEMO27_TREE_DIALOG, (void **) &tree) ||
        tree == NULL) {
        form_alert(1, "[1][ Cannot find tree ][ Abort ]");
        rsrc_free();
        v_clsvwk(vdi_handle);
        appl_exit();
        return 1;
    }

    last_object = tree[ROOT].ob_tail;
    for (object = ROOT; object <= last_object; ++object) {
        rsrc_obfix(tree, object);
    }

    form_center(tree, &cx, &cy, &cw, &ch);
    tree[ROOT].ob_x = cx;
    tree[ROOT].ob_y = cy;

    form_dial(FMD_START, 0, 0, 0, 0, cx, cy, cw, ch);
    objc_draw(tree, ROOT, MAX_DEPTH, cx, cy, cw, ch);

    while (1) {
        event = evnt_multi(MU_BUTTON | MU_KEYBD,
            1, 1, 1,
            0, 0, 0, 0, 0,
            0, 0, 0, 0, 0,
            NULL, 0, 0, &mx, &my, &mb, &ks, &kr, &br);

        if ((event & MU_KEYBD) != 0 && (kr & 0xff) == 27) {
            break;
        }
        if ((event & MU_BUTTON) == 0) {
            continue;
        }

        object = objc_find(tree, ROOT, MAX_DEPTH, mx, my);
        if (object != DEMO27_OBJ_OK && object != DEMO27_OBJ_CANCEL) {
            continue;
        }

        objc_offset(tree, object, &ox, &oy);
        rect.g_x = ox;
        rect.g_y = oy;
        rect.g_w = tree[object].ob_width;
        rect.g_h = tree[object].ob_height;

        objc_change(tree, object, 0,
            rect.g_x, rect.g_y, rect.g_w, rect.g_h,
            (WORD) (tree[object].ob_state | SELECTED), 1);
        evnt_timer(90, 0);
        objc_change(tree, object, 0,
            rect.g_x, rect.g_y, rect.g_w, rect.g_h,
            tree[object].ob_state, 1);

        if (object == DEMO27_OBJ_OK) {
            strcpy(alert_text, "[1][ Resource file loaded ][ Nice ]");
        } else {
            strcpy(alert_text, "[1][ Resource dialog cancelled ][ OK ]");
        }
        form_alert(1, alert_text);
        break;
    }

    form_dial(FMD_FINISH, 0, 0, 0, 0, cx, cy, cw, ch);
    rsrc_free();
    v_clsvwk(vdi_handle);
    appl_exit();
    return 0;
}
