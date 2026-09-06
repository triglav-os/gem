/*
 * Implements the AES object-tree calls (objc_add/offset/find/draw/
 * change) for gemd RPC clients. Unlike menu_bar(), these never cross
 * the wire: a client already holds its whole tree in local memory, so
 * tree linking/hit-testing is pure local computation, and rendering
 * is just a walk that emits the same VDI primitives client.c already
 * forwards to gemd (v_bar/v_pline/v_gtext/vs_clip/...). No new RPC
 * opcodes are needed.
 *
 * This covers the object kinds apps in this codebase actually use for
 * their own windows (G_IBOX, G_BOX, G_BUTTON, G_BOXTEXT, G_STRING);
 * it intentionally skips menu-bar/dialog-frame special casing and
 * G_ICON/G_USERDEF/editable-field rendering, which belong to the
 * hosted AES engine's own tree (menus, alerts), not a client's
 * private window content.
 *
 * MIT License (see: LICENSE)
 * Copyright (C) 2026 tomaz stih
 */

#include "gem_protocol.h"
#include "gem/gemd.h"

#include <string.h>

static WORD gem_objc_find_parent(const OBJECT *tree, WORD object)
{
    WORD parent;
    WORD last_object;

    if (tree == NULL || object <= ROOT) {
        return NIL;
    }

    last_object = ROOT;
    while (last_object < 255 && (tree[last_object].ob_flags & LASTOB) == 0u) {
        ++last_object;
    }

    for (parent = ROOT; parent <= last_object; ++parent) {
        WORD child;

        if (parent == object || tree[parent].ob_head == NIL) {
            continue;
        }

        child = tree[parent].ob_head;
        while (child != NIL) {
            WORD next = tree[child].ob_next;

            if (child == object) {
                return parent;
            }
            if (child == tree[parent].ob_tail || next == parent ||
                next == NIL) {
                break;
            }
            child = next;
        }
    }

    return NIL;
}

static void gem_objc_extent(const OBJECT *tree, WORD object, WORD *x, WORD *y)
{
    WORD abs_x = 0;
    WORD abs_y = 0;
    WORD current = object;

    if (tree == NULL || object < 0) {
        if (x != NULL) {
            *x = 0;
        }
        if (y != NULL) {
            *y = 0;
        }
        return;
    }

    while (current != NIL) {
        abs_x = (WORD)(abs_x + tree[current].ob_x);
        abs_y = (WORD)(abs_y + tree[current].ob_y);
        current = gem_objc_find_parent(tree, current);
    }

    if (x != NULL) {
        *x = abs_x;
    }
    if (y != NULL) {
        *y = abs_y;
    }
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

WORD objc_offset(OBJECT *tree, WORD object, WORD *x, WORD *y)
{
    if (tree == NULL || object < 0) {
        return 0;
    }

    gem_objc_extent(tree, object, x, y);
    return 1;
}

static WORD gem_objc_find_in_subtree(const OBJECT *tree, WORD object,
                                     WORD parent_x, WORD parent_y, WORD depth,
                                     WORD mx, WORD my)
{
    WORD abs_x;
    WORD abs_y;
    WORD hit = NIL;

    if (tree == NULL || object < 0) {
        return NIL;
    }

    abs_x = (WORD)(parent_x + tree[object].ob_x);
    abs_y = (WORD)(parent_y + tree[object].ob_y);

    if (depth != 0 && tree[object].ob_head != NIL &&
        (tree[object].ob_flags & HIDETREE) == 0u) {
        WORD child = tree[object].ob_head;

        while (child != NIL) {
            WORD next = tree[child].ob_next;
            WORD child_hit = gem_objc_find_in_subtree(
                tree, child, abs_x, abs_y,
                (depth > 0) ? (WORD)(depth - 1) : depth, mx, my);

            if (child_hit != NIL) {
                hit = child_hit;
            }
            if (child == tree[object].ob_tail || next == object ||
                next == NIL) {
                break;
            }
            child = next;
        }
    }

    if (mx >= abs_x && my >= abs_y && mx < abs_x + tree[object].ob_width &&
        my < abs_y + tree[object].ob_height &&
        (tree[object].ob_flags & HIDETREE) == 0u) {
        if (hit == NIL) {
            hit = object;
        }
    }

    return hit;
}

WORD objc_find(OBJECT *tree, WORD startob, WORD depth, WORD mx, WORD my)
{
    if (tree == NULL || startob < 0) {
        return NIL;
    }

    return gem_objc_find_in_subtree(tree, startob, 0, 0, depth, mx, my);
}

WORD objc_draw(OBJECT *tree, WORD startob, WORD depth, WORD xc, WORD yc,
               WORD wc, WORD hc)
{
    WORD args[12] = {startob, depth, xc, yc, wc, hc};
    return gem_client_tree(tree, tree_draw, args);
}

WORD objc_change(OBJECT *tree, WORD object, WORD depth, WORD xc, WORD yc,
                 WORD wc, WORD hc, WORD newstate, WORD redraw)
{
    if (tree == NULL || object < 0) {
        return 0;
    }

    tree[object].ob_state = (UWORD)newstate;
    if (redraw != 0) {
        return objc_draw(tree, object, depth, xc, yc, wc, hc);
    }
    return 1;
}
