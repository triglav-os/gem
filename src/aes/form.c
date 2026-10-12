/*
 * Implements hosted AES form handling: form_do's modal loop over a dialog
 * tree, keyboard navigation between fields, default exits, button
 * flashing, dialog grow/shrink effects and form_center.
 *
 * MIT License (see: LICENSE)
 * Copyright (C) 2026 tomaz stih
 */

#include "form_internal.h"

#include "platform/os.h"

#include <stdint.h>
#include <string.h>

static WORD aes_form_last_reachable_object(const OBJECT *tree, WORD object,
                                           uint8_t visited[256]);

WORD aes_form_last_object(OBJECT *tree)
{
    uint8_t visited[256] = {0};

    if (tree == NULL) {
        return ROOT;
    }

    return aes_form_last_reachable_object(tree, ROOT, visited);
}

static WORD aes_form_last_reachable_object(const OBJECT *tree, WORD object,
                                           uint8_t visited[256])
{
    WORD last;
    WORD child;

    if (tree == NULL || visited == NULL || object < ROOT || object >= 256) {
        return ROOT;
    }
    if (visited[object] != 0u) {
        return ROOT;
    }

    visited[object] = 1u;
    last = object;
    child = tree[object].ob_head;
    if (child == NIL || child == object) {
        return last;
    }

    while (child != NIL && child != object) {
        WORD child_last;
        WORD next;

        child_last = aes_form_last_reachable_object(tree, child, visited);
        if (child_last > last) {
            last = child_last;
        }

        next = tree[child].ob_next;
        if (next == child || next == object) {
            break;
        }
        child = next;
    }

    return last;
}

void aes_form_object_rect(OBJECT *tree, WORD object, GRECT *rect)
{
    WORD x;
    WORD y;

    if (rect == NULL) {
        return;
    }

    aes_set_rect(rect, 0, 0, 0, 0);
    if (tree == NULL || object < 0) {
        return;
    }

    objc_offset(tree, object, &x, &y);
    rect->g_x = x;
    rect->g_y = y;
    rect->g_w = tree[object].ob_width;
    rect->g_h = tree[object].ob_height;
}

static void aes_form_redraw_tree(OBJECT *tree)
{
    if (tree == NULL) {
        return;
    }

    objc_draw(tree, ROOT, MAX_DEPTH, tree[ROOT].ob_x, tree[ROOT].ob_y,
              tree[ROOT].ob_width, tree[ROOT].ob_height);
}

void aes_form_redraw_object(OBJECT *tree, WORD object)
{
    GRECT rect;

    if (tree == NULL || object < 0) {
        return;
    }

    aes_form_object_rect(tree, object, &rect);
    objc_draw(tree, object, 0, rect.g_x, rect.g_y, rect.g_w, rect.g_h);
}

static WORD aes_form_find_default_exit(OBJECT *tree)
{
    WORD i;
    WORD last = aes_form_last_object(tree);

    if (tree == NULL) {
        return NIL;
    }

    for (i = ROOT; i <= last; ++i) {
        if ((tree[i].ob_flags & DEFAULT) != 0u &&
            (tree[i].ob_flags & EXIT) != 0u) {
            return i;
        }
    }
    return NIL;
}

static void aes_form_flash_object(OBJECT *tree, WORD object)
{
    GRECT rect;
    UWORD old_state;

    if (tree == NULL || object < 0) {
        return;
    }

    aes_form_object_rect(tree, object, &rect);
    old_state = tree[object].ob_state;
    objc_change(tree, object, 0, rect.g_x, rect.g_y, rect.g_w, rect.g_h,
                (WORD)(old_state | SELECTED), 1);
    evnt_timer(90, 0);
    objc_change(tree, object, 0, rect.g_x, rect.g_y, rect.g_w, rect.g_h,
                old_state, 1);
}

WORD form_do(OBJECT *tree, WORD startob)
{
    WORD active;
    WORD idx = 0;
    WORD result = NIL;
    OBJECT *previous_modal_tree;

    if (tree == NULL) {
        return 0;
    }

    previous_modal_tree = aes_state.modal_form_tree;
    if (aes_state.modal_form_owner_app_id == aes_state.current_app_id) {
        aes_state.modal_form_tree = tree;
    }

    active = (aes_form_is_editable(tree, startob) != 0)
                 ? startob
                 : aes_form_find_next_editable(tree, startob, 0);
    if (active != NIL) {
        TEDINFO *ted = (TEDINFO *)(intptr_t)tree[active].ob_spec;
        char *buffer = (ted != NULL) ? (char *)(intptr_t)ted->te_ptext : NULL;

        idx = (buffer != NULL) ? (WORD)strlen(buffer) : 0;
        aes_form_set_active_field(tree, active, &idx);
    }

    FOREVER
    {
        WORD event;
        WORD mx = 0;
        WORD my = 0;
        WORD mb = 0;
        WORD ks = 0;
        WORD kr = 0;
        WORD br = 0;

        event = evnt_multi(MU_BUTTON | MU_KEYBD, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0,
                           0, 0, 0, NULL, 0, 0, &mx, &my, &mb, &ks, &kr, &br);
        if (event == 0 && aes_wait_hook != NULL)
            goto done;

        if ((event & MU_KEYBD) != 0) {
            WORD ascii = (WORD)(kr & 0xffu);
            WORD scancode = (WORD)((kr >> 8) & 0xffu);

            if (ascii == 27) {
                WORD cancel = aes_form_find_default_exit(tree);

                if (cancel != NIL) {
                    result = cancel;
                    goto done;
                }
                result = 0;
                goto done;
            }
            if (ascii == '\t' ||
                (ascii == 0 && (scancode == 43 || scancode == 15))) {
                active = aes_form_find_next_editable(
                    tree, active,
                    (aes_state.key_state & (K_LSHIFT | K_RSHIFT)) != 0u);
                if (active != NIL) {
                    TEDINFO *ted = (TEDINFO *)(intptr_t)tree[active].ob_spec;
                    char *buffer =
                        (ted != NULL) ? (char *)(intptr_t)ted->te_ptext : NULL;

                    idx = (buffer != NULL) ? (WORD)strlen(buffer) : 0;
                    aes_form_set_active_field(tree, active, &idx);
                }
                continue;
            }
            if (ascii == '\r' || ascii == '\n' ||
                (ascii == 0 && (scancode == 41 || scancode == 28))) {
                WORD button = aes_form_find_default_exit(tree);

                if (button != NIL) {
                    aes_form_flash_object(tree, button);
                    result = button;
                    goto done;
                }
            }
            if (active != NIL &&
                aes_form_apply_key(tree, active, kr, &idx) != 0) {
                aes_form_redraw_object(tree, active);
            }
        }

        if ((event & MU_BUTTON) != 0) {
            WORD object = objc_find(tree, ROOT, MAX_DEPTH, mx, my);

            if (object == NIL) {
                continue;
            }
            if (aes_form_is_editable(tree, object) != 0) {
                if (object != active) {
                    active = object;
                    idx = 0;
                    aes_form_set_active_field(tree, active, &idx);
                }
                aes_form_set_caret_from_click(tree, object, mx, &idx);
                aes_form_redraw_object(tree, object);
                continue;
            }
            if ((tree[object].ob_flags & EXIT) != 0u) {
                aes_form_flash_object(tree, object);
                result = object;
                goto done;
            }
            if ((tree[object].ob_flags & SELECTABLE) != 0u) {
                WORD newobj = object;

                (void)form_button(tree, object, 1, &newobj);
                aes_form_redraw_object(tree, object);
                if ((tree[object].ob_flags & RBUTTON) != 0u) {
                    aes_form_redraw_tree(tree);
                }
            }
        }
    }

done:
    if (aes_state.modal_form_tree == tree) {
        aes_state.modal_form_tree = previous_modal_tree;
    }
    return result;
}

WORD form_dial(WORD flag, WORD x1, WORD y1, WORD w1, WORD h1, WORD x2, WORD y2,
               WORD w2, WORD h2)
{
    GRECT dirty;

    (void)flag;
    (void)x1;
    (void)y1;
    (void)w1;
    (void)h1;
    (void)x2;
    (void)y2;
    (void)w2;
    (void)h2;

    if (flag == FMD_START || flag == FMD_GROW) {
        aes_state.modal_form_owner_app_id = aes_state.current_app_id;
        aes_set_rect(&aes_state.modal_form_rect, x2, y2, w2, h2);
        aes_state.modal_form_z = aes_state.next_window_z++;
        aes_state.active_app_id = aes_state.current_app_id;
        aes_state.input_focus_app_id = aes_state.current_app_id;
    }

    if (flag == FMD_FINISH || flag == FMD_SHRINK) {
        if (aes_state.modal_form_owner_app_id == aes_state.current_app_id) {
            aes_state.modal_form_owner_app_id = 0;
            aes_set_rect(&aes_state.modal_form_rect, 0, 0, 0, 0);
            aes_state.modal_form_z = 0u;
            aes_state.modal_form_tree = NULL;
        }
        aes_state.hover_tree = NULL;
        aes_set_rect(&dirty, x2, y2, w2, h2);
        aes_redraw_region(&dirty);
    }

    return 1;
}

WORD form_center(OBJECT *tree, WORD *cx, WORD *cy, WORD *cw, WORD *ch)
{
    WORD width = 200;
    WORD height = 100;

    if (tree != NULL) {
        width = tree[ROOT].ob_width;
        height = tree[ROOT].ob_height;
    }
    if (aes_ensure_vdi() != 0) {
        if (cx != NULL) {
            *cx = (WORD)((aes_state.work_out[0] + 1 - width) / 2);
        }
        if (cy != NULL) {
            *cy = (WORD)((aes_state.work_out[1] + 1 - height) / 2);
        }
    } else {
        if (cx != NULL) {
            *cx = 0;
        }
        if (cy != NULL) {
            *cy = 0;
        }
    }
    if (cw != NULL) {
        *cw = width;
    }
    if (ch != NULL) {
        *ch = height;
    }
    return 1;
}

WORD form_keybd(OBJECT *tree, WORD object, WORD next, WORD thechar,
                WORD *newobj, WORD *newchar)
{
    (void)tree;
    if (newobj != NULL) {
        *newobj = next;
    }
    if (newchar != NULL) {
        *newchar = thechar;
    }
    return object;
}

WORD form_button(OBJECT *tree, WORD object, WORD clicks, WORD *newobj)
{
    (void)clicks;

    if (tree == NULL || object < 0) {
        return 0;
    }
    tree[object].ob_state ^= SELECTED;
    if (newobj != NULL) {
        *newobj = object;
    }
    return 1;
}
