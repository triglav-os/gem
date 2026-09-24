/*
 * Implements hosted AES menu-bar management (menu_bar, item checks and
 * enables, title highlighting, registration and click mode) and the
 * client-visible object tree calls objc_add, objc_delete, objc_draw,
 * objc_find, objc_offset, objc_order and objc_change.
 *
 * MIT License (see: LICENSE)
 * Copyright (C) 2026 tomaz stih
 */

#include "aes_internal.h"
#include "system_menu.h"

#include <stddef.h>
#include <string.h>

WORD menu_bar(OBJECT *tree, WORD show)
{
    aes_app_t *app = aes_find_app_by_id(aes_state.current_app_id);
    const WORD previous_height = aes_menu_bar_height();

    /* An inactive app detaches its own menu without hiding another app's. */
    if (show == 0 && app != NULL && aes_state.menu_owner_app_id != app->id) {
        app->menu_tree = NULL;
        app->menu_visible = 0;
        return 1;
    }
    aes_state.menu_tree = tree;
    aes_state.menu_visible = (show != 0) ? 1 : 0;
    aes_state.menu_owner_app_id = aes_state.current_app_id;
    aes_state.active_app_id = aes_state.current_app_id;
    if (show != 0 && tree != NULL && aes_state.desktop_owner_app_id == 0) {
        aes_state.desktop_owner_app_id = aes_state.current_app_id;
    }
    if (app != NULL) {
        app->menu_tree = tree;
        app->menu_visible = aes_state.menu_visible;
    }
    if (show != 0 && tree != NULL) {
        aes_menu_prepare_tree(tree);
        aes_menu_hide_popups(tree);
        aes_menu_redraw_tree(tree);
    } else {
        aes_menu_clear_saved_region();
        aes_state.menu_tree = NULL;
        if (app != NULL)
            app->menu_tree = NULL;
        if (previous_height > 0) {
            GRECT bar;
            aes_set_rect(&bar, 0, 0, (WORD)(aes_state.work_out[0] + 1),
                         previous_height);
            aes_redraw_region(&bar);
        }
    }
    aes_trace("menu_bar tree=%p show=%d", (void *)tree, show);
    return 1;
}

WORD menu_icheck(OBJECT *tree, WORD item, WORD check)
{
    if (tree == NULL || item < 0) {
        return 0;
    }
    if (check != 0) {
        tree[item].ob_state |= CHECKED;
    } else {
        tree[item].ob_state &= (UWORD)~CHECKED;
    }
    return 1;
}

WORD menu_ienable(OBJECT *tree, WORD item, WORD enable)
{
    if (tree == NULL || item < 0) {
        return 0;
    }
    if (enable != 0) {
        tree[item].ob_state &= (UWORD)~DISABLED;
    } else {
        tree[item].ob_state |= DISABLED;
    }
    return 1;
}

WORD menu_tnormal(OBJECT *tree, WORD title, WORD normal)
{
    if (tree == NULL || title < 0) {
        return 0;
    }
    if (normal != 0) {
        tree[title].ob_state &= (UWORD)~SELECTED;
    } else {
        tree[title].ob_state |= SELECTED;
    }
    return 1;
}

WORD menu_text(OBJECT *tree, WORD item, char *text)
{
    if (tree == NULL || item < 0) {
        return 0;
    }
    tree[item].ob_spec = (LONG)(intptr_t)text;
    return 1;
}

WORD menu_register(WORD apid, char *name)
{
    aes_app_t *app = aes_find_app_by_id(apid);

    if (app == NULL || name == NULL || apid != aes_state.current_app_id) {
        return -1;
    }
    {
        WORD menu_id = aes_system_menu_register(apid, name);

        if (menu_id >= 0) {
            strncpy(app->name, name, sizeof(app->name) - 1u);
            app->name[sizeof(app->name) - 1u] = '\0';
        }
        return menu_id;
    }
}

WORD menu_unregister(WORD mid)
{
    return aes_system_menu_unregister(aes_state.current_app_id, mid);
}

WORD menu_click(WORD click, WORD setit)
{
    if (setit != 0) {
        aes_state.menu_click = click;
    }
    return aes_state.menu_click;
}

WORD objc_add(OBJECT *tree, WORD parent, WORD child)
{
    WORD last;

    if (tree == NULL || parent < 0 || child < 0) {
        return 0;
    }

    if (tree[parent].ob_head == NIL) {
        tree[parent].ob_head = child;
    } else {
        last = tree[parent].ob_tail;
        tree[last].ob_next = child;
    }
    tree[parent].ob_tail = child;
    tree[child].ob_next = parent;
    return 1;
}

WORD objc_delete(OBJECT *tree, WORD object)
{
    size_t i;

    if (tree == NULL || object < 0) {
        return 0;
    }

    for (i = 0; i < 1024u; ++i) {
        if (tree[i].ob_head == object) {
            tree[i].ob_head = tree[object].ob_next;
            if (tree[i].ob_tail == object) {
                tree[i].ob_tail = NIL;
            }
            break;
        }
        if (tree[i].ob_head != NIL) {
            WORD node = tree[i].ob_head;

            while (node != NIL && node != (WORD)i) {
                if (tree[node].ob_next == object) {
                    tree[node].ob_next = tree[object].ob_next;
                    if (tree[i].ob_tail == object) {
                        tree[i].ob_tail = node;
                    }
                    break;
                }
                node = tree[node].ob_next;
            }
        }
    }
    tree[object].ob_next = NIL;
    return 1;
}

WORD objc_draw(OBJECT *tree, WORD startob, WORD depth, WORD xc, WORD yc,
               WORD wc, WORD hc)
{
    WORD clip[4];
    WORD origin_x = 0;
    WORD origin_y = 0;
    WORD text_attrib[10];
    WORD previous_font = 0;
    WORD restore_font = 0;

    (void)depth;

    if (tree == NULL || startob < 0 || aes_ensure_vdi() == 0) {
        aes_trace("objc_draw skipped tree=%p start=%d vdi=%d", (void *)tree,
                  startob, aes_state.vdi_ready);
        return 0;
    }

    aes_trace("objc_draw tree=%p start=%d depth=%d clip=%d,%d %dx%d type=%u",
              (void *)tree, startob, depth, xc, yc, wc, hc,
              (unsigned)tree[startob].ob_type);

    if (startob == ROOT && tree != aes_state.menu_tree) {
        aes_state.hover_tree = tree;
    }

    if (vqt_attributes(aes_state.vdi_handle, text_attrib) != 0) {
        previous_font = text_attrib[0];
        restore_font = 1;
    }
    (void)vst_font(aes_state.vdi_handle, 1);

    clip[0] = xc;
    clip[1] = yc;
    clip[2] = (WORD)(xc + wc - 1);
    clip[3] = (WORD)(yc + hc - 1);
    aes_object_extent(tree, startob, &origin_x, &origin_y);
    vs_clip(aes_state.vdi_handle, 1, clip);
    aes_draw_tree_recursive(tree, startob,
                            (WORD)(origin_x - tree[startob].ob_x),
                            (WORD)(origin_y - tree[startob].ob_y), depth, clip);
    vs_clip(aes_state.vdi_handle, 0, clip);
    if (restore_font != 0) {
        (void)vst_font(aes_state.vdi_handle, previous_font);
    }
    return 1;
}

WORD objc_find(OBJECT *tree, WORD startob, WORD depth, WORD mx, WORD my)
{
    if (tree == NULL || startob < 0) {
        return NIL;
    }

    return aes_find_in_subtree(tree, startob, 0, 0, depth, mx, my);
}

WORD objc_offset(OBJECT *tree, WORD object, WORD *x, WORD *y)
{
    if (tree == NULL || object < 0) {
        return 0;
    }

    aes_object_extent(tree, object, x, y);
    return 1;
}

WORD objc_order(OBJECT *tree, WORD object, WORD newpos)
{
    (void)tree;
    (void)object;
    (void)newpos;
    return 1;
}

WORD objc_change(OBJECT *tree, WORD object, WORD depth, WORD xc, WORD yc,
                 WORD wc, WORD hc, WORD newstate, WORD redraw)
{
    if (tree == NULL || object < 0) {
        return 0;
    }

    (void)depth;
    tree[object].ob_state = (UWORD)newstate;
    if (redraw != 0) {
        return objc_draw(tree, object, 0, xc, yc, wc, hc);
    }
    return 1;
}
