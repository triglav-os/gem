/*
 * Demonstrates a classic GEM modal dialog with checkbox, radio button,
 * and default/exit buttons. The dialog is built from a plain AES
 * object tree and uses explicit classic event handling so it stays a
 * useful compatibility probe while `form_do()` is still incomplete.
 *
 * MIT License (see: LICENSE)
 * Copyright (C) 2026 tomaz stih
 */

#include <gem.h>
#include <string.h>

enum {
    D24_ROOT = 0,
    D24_PANEL,
    D24_TITLE,
    D24_PROMPT,
    D24_CHECK,
    D24_RADIO_A,
    D24_RADIO_B,
    D24_STATUS,
    D24_OK,
    D24_CANCEL,
    D24_COUNT
};

WORD appl_id;
VDI_HANDLE vdi_handle;
WORD work_in[11] = {1,1,1,1,1,1,1,1,1,1,2};
WORD work_out[57];
OBJECT dialog_tree[D24_COUNT];
char dialog_status[80] = "Vintage selected, mode A.";
WORD dialog_done = 0;
WORD checkbox_on = 1;
WORD radio_mode = 0;

void init_object(OBJECT *object,
    UWORD type, UWORD flags, UWORD state, LONG spec,
    WORD x, WORD y, WORD w, WORD h)
{
    object->ob_next = NIL;
    object->ob_head = NIL;
    object->ob_tail = NIL;
    object->ob_type = type;
    object->ob_flags = flags;
    object->ob_state = state;
    object->ob_spec = spec;
    object->ob_x = x;
    object->ob_y = y;
    object->ob_width = w;
    object->ob_height = h;
}

void update_dialog_status(void)
{
    strcpy(dialog_status, checkbox_on ?
        "Vintage selected, " : "Vintage cleared, ");
    strcat(dialog_status, (radio_mode == 0) ? "mode A." : "mode B.");
    dialog_tree[D24_STATUS].ob_spec = (LONG) (intptr_t) dialog_status;

    if (checkbox_on != 0) {
        dialog_tree[D24_CHECK].ob_state |= CHECKED;
    } else {
        dialog_tree[D24_CHECK].ob_state &= (UWORD) ~CHECKED;
    }

    if (radio_mode == 0) {
        dialog_tree[D24_RADIO_A].ob_state |= CHECKED;
        dialog_tree[D24_RADIO_B].ob_state &= (UWORD) ~CHECKED;
    } else {
        dialog_tree[D24_RADIO_A].ob_state &= (UWORD) ~CHECKED;
        dialog_tree[D24_RADIO_B].ob_state |= CHECKED;
    }
}

void init_dialog_tree(void)
{
    init_object(&dialog_tree[D24_ROOT], G_IBOX, LASTOB, NORMAL, 0L,
        0, 0, 300, 170);
    init_object(&dialog_tree[D24_PANEL], G_BOX, NONE, NORMAL, 0L,
        0, 0, 300, 170);
    init_object(&dialog_tree[D24_TITLE], G_STRING, NONE, NORMAL,
        (LONG) (intptr_t) "Demo 24 Dialog", 18, 14, 120, 8);
    init_object(&dialog_tree[D24_PROMPT], G_STRING, NONE, NORMAL,
        (LONG) (intptr_t) "Classic form objects, no helper magic.",
        18, 34, 220, 8);
    init_object(&dialog_tree[D24_CHECK], G_BUTTON, SELECTABLE, NORMAL,
        (LONG) (intptr_t) "Vintage style", 18, 56, 122, 22);
    init_object(&dialog_tree[D24_RADIO_A], G_BUTTON,
        SELECTABLE | RBUTTON, NORMAL,
        (LONG) (intptr_t) "Mode A", 18, 84, 100, 22);
    init_object(&dialog_tree[D24_RADIO_B], G_BUTTON,
        SELECTABLE | RBUTTON, NORMAL,
        (LONG) (intptr_t) "Mode B", 128, 84, 100, 22);
    init_object(&dialog_tree[D24_STATUS], G_STRING, NONE, NORMAL, 0L,
        18, 118, 220, 8);
    init_object(&dialog_tree[D24_OK], G_BUTTON,
        SELECTABLE | EXIT | DEFAULT, NORMAL,
        (LONG) (intptr_t) "OK", 150, 140, 58, 22);
    init_object(&dialog_tree[D24_CANCEL], G_BUTTON,
        SELECTABLE | EXIT, NORMAL,
        (LONG) (intptr_t) "Cancel", 218, 140, 64, 22);

    objc_add(dialog_tree, D24_ROOT, D24_PANEL);
    objc_add(dialog_tree, D24_ROOT, D24_TITLE);
    objc_add(dialog_tree, D24_ROOT, D24_PROMPT);
    objc_add(dialog_tree, D24_ROOT, D24_CHECK);
    objc_add(dialog_tree, D24_ROOT, D24_RADIO_A);
    objc_add(dialog_tree, D24_ROOT, D24_RADIO_B);
    objc_add(dialog_tree, D24_ROOT, D24_STATUS);
    objc_add(dialog_tree, D24_ROOT, D24_OK);
    objc_add(dialog_tree, D24_ROOT, D24_CANCEL);

    update_dialog_status();
}

void object_rect(OBJECT *tree, WORD object, GRECT *rect)
{
    WORD x;
    WORD y;

    objc_offset(tree, object, &x, &y);
    rect->g_x = x;
    rect->g_y = y;
    rect->g_w = tree[object].ob_width;
    rect->g_h = tree[object].ob_height;
}

void flash_object(WORD object)
{
    GRECT rect;
    WORD old_state;

    object_rect(dialog_tree, object, &rect);
    old_state = dialog_tree[object].ob_state;
    objc_change(dialog_tree, object, 0,
        rect.g_x, rect.g_y, rect.g_w, rect.g_h,
        (WORD) (old_state | SELECTED), 1);
    evnt_timer(180, 0);
    objc_change(dialog_tree, object, 0,
        rect.g_x, rect.g_y, rect.g_w, rect.g_h,
        old_state, 1);
}

void redraw_dialog(void)
{
    objc_draw(dialog_tree, ROOT, MAX_DEPTH,
        dialog_tree[ROOT].ob_x, dialog_tree[ROOT].ob_y,
        dialog_tree[ROOT].ob_width, dialog_tree[ROOT].ob_height);
}

int main(void)
{
    WORD mouse_x = 0;
    WORD mouse_y = 0;
    WORD mouse_buttons = 0;
    WORD key_state = 0;
    WORD key_return = 0;
    WORD dummy = 0;
    WORD event;
    WORD cx;
    WORD cy;
    WORD cw;
    WORD ch;

    appl_id = appl_init();
    if (appl_id < 0) {
        return 1;
    }

    vdi_handle = graf_handle();
    v_opnvwk(work_in, &vdi_handle, work_out);
    init_dialog_tree();

    form_center(dialog_tree, &cx, &cy, &cw, &ch);
    dialog_tree[ROOT].ob_x = cx;
    dialog_tree[ROOT].ob_y = cy;

    form_dial(FMD_START, 0, 0, 0, 0, cx, cy, cw, ch);
    redraw_dialog();

    while (dialog_done == 0) {
        WORD object;

        event = evnt_multi(MU_BUTTON | MU_KEYBD,
            1, 1, 1,
            0, 0, 0, 0, 0,
            0, 0, 0, 0, 0,
            NULL, 0, 0,
            &mouse_x, &mouse_y, &mouse_buttons, &key_state,
            &key_return, &dummy);

        if ((event & MU_KEYBD) != 0 && (key_return & 0xff) == 27) {
            dialog_done = 1;
            break;
        }

        if ((event & MU_BUTTON) == 0) {
            continue;
        }

        object = objc_find(dialog_tree, ROOT, MAX_DEPTH, mouse_x, mouse_y);
        if (object < 0) {
            continue;
        }

        flash_object(object);

        switch (object) {
        case D24_CHECK:
            checkbox_on = !checkbox_on;
            update_dialog_status();
            redraw_dialog();
            break;
        case D24_RADIO_A:
            radio_mode = 0;
            update_dialog_status();
            redraw_dialog();
            break;
        case D24_RADIO_B:
            radio_mode = 1;
            update_dialog_status();
            redraw_dialog();
            break;
        case D24_OK:
        case D24_CANCEL:
            dialog_done = 1;
            break;
        default:
            break;
        }
    }

    form_dial(FMD_FINISH, 0, 0, 0, 0, cx, cy, cw, ch);
    v_clsvwk(vdi_handle);
    appl_exit();
    return 0;
}
