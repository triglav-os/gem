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

#include "_gem.h"

#include <string.h>

enum {
    GEM_OBJC_LIGHT_COLOR = BLACK,
    GEM_OBJC_DARK_COLOR = WHITE
};

static WORD g_objc_char_w;
static WORD g_objc_char_h;

static void gem_objc_ensure_metrics(void)
{
    WORD cw = 0;
    WORD ch = 0;
    WORD bw = 0;
    WORD bh = 0;

    if (g_objc_char_w > 0 && g_objc_char_h > 0) {
        return;
    }

    (void) graf_handle(&cw, &ch, &bw, &bh);
    g_objc_char_w = (cw > 0) ? cw : 6;
    g_objc_char_h = (ch > 0) ? ch : 8;
}

static WORD gem_objc_string_width(const char *text)
{
    if (text == NULL) {
        return 0;
    }
    gem_objc_ensure_metrics();
    return (WORD) (strlen(text) * (size_t) g_objc_char_w);
}

static LONG gem_objc_resolve_spec(const OBJECT *obj)
{
    LONG spec;

    if (obj == NULL) {
        return 0;
    }

    spec = obj->ob_spec;
    if ((obj->ob_flags & INDIRECT) != 0u && spec != 0) {
        spec = *(LONG *) (intptr_t) spec;
    }
    return spec;
}

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
        abs_x = (WORD) (abs_x + tree[current].ob_x);
        abs_y = (WORD) (abs_y + tree[current].ob_y);
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
    WORD parent_x, WORD parent_y, WORD depth, WORD mx, WORD my)
{
    WORD abs_x;
    WORD abs_y;
    WORD hit = NIL;

    if (tree == NULL || object < 0) {
        return NIL;
    }

    abs_x = (WORD) (parent_x + tree[object].ob_x);
    abs_y = (WORD) (parent_y + tree[object].ob_y);

    if (depth != 0 && tree[object].ob_head != NIL &&
        (tree[object].ob_flags & HIDETREE) == 0u) {
        WORD child = tree[object].ob_head;

        while (child != NIL) {
            WORD next = tree[child].ob_next;
            WORD child_hit = gem_objc_find_in_subtree(tree, child, abs_x,
                abs_y, (depth > 0) ? (WORD) (depth - 1) : depth, mx, my);

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

    if (mx >= abs_x && my >= abs_y &&
        mx < abs_x + tree[object].ob_width &&
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

static void gem_objc_draw_hline(WORD x0, WORD x1, WORD y, WORD color)
{
    WORD pts[4];

    if (x0 > x1) {
        return;
    }
    pts[0] = x0;
    pts[1] = y;
    pts[2] = x1;
    pts[3] = y;
    vsl_color(0, color);
    v_pline(0, 2, pts);
}

static void gem_objc_draw_vline(WORD x, WORD y0, WORD y1, WORD color)
{
    WORD pts[4];

    if (y0 > y1) {
        return;
    }
    pts[0] = x;
    pts[1] = y0;
    pts[2] = x;
    pts[3] = y1;
    vsl_color(0, color);
    v_pline(0, 2, pts);
}

static void gem_objc_fill_rect(WORD x0, WORD y0, WORD x1, WORD y1, WORD color)
{
    WORD rect[4];

    if (x0 > x1 || y0 > y1) {
        return;
    }
    rect[0] = x0;
    rect[1] = y0;
    rect[2] = x1;
    rect[3] = y1;
    vsf_color(0, color);
    v_bar(0, rect);
}

static void gem_objc_draw_text(WORD x, WORD y, WORD color, const char *text)
{
    if (text == NULL || *text == '\0') {
        return;
    }
    vst_color(0, color);
    v_gtext(0, x, y, (CONST BYTE *) text);
}

static void gem_objc_draw_button_frame(const WORD rect[4], WORD dark_color,
    int default_button)
{
    WORD inset;
    WORD border_thickness = default_button != 0 ? 2 : 1;

    for (inset = 0; inset < border_thickness; ++inset) {
        WORD left = (WORD) (rect[0] + inset);
        WORD top = (WORD) (rect[1] + inset);
        WORD right = (WORD) (rect[2] - inset);
        WORD bottom = (WORD) (rect[3] - inset);

        if (left > right || top > bottom) {
            break;
        }
        gem_objc_draw_hline(left, right, top, dark_color);
        gem_objc_draw_hline(left, right, bottom, dark_color);
        gem_objc_draw_vline(left, top, bottom, dark_color);
        gem_objc_draw_vline(right, top, bottom, dark_color);
    }
}

static void gem_objc_draw_ted(const OBJECT *obj, const TEDINFO *ted,
    const WORD rect[4])
{
    const char *text;
    WORD text_x;
    WORD text_y;
    WORD text_width;

    if (obj == NULL || ted == NULL) {
        return;
    }

    text = (const char *) (intptr_t) ted->te_ptext;
    gem_objc_ensure_metrics();
    text_y = (WORD) (rect[1] +
        ((rect[3] - rect[1] + 1 - g_objc_char_h) > 0 ?
        (rect[3] - rect[1] + 1 - g_objc_char_h) / 2 : 0) + g_objc_char_h - 2);

    gem_objc_fill_rect(rect[0], rect[1], rect[2], rect[3],
        GEM_OBJC_LIGHT_COLOR);
    gem_objc_draw_hline(rect[0], rect[2], rect[1], GEM_OBJC_DARK_COLOR);
    gem_objc_draw_hline(rect[0], rect[2], rect[3], GEM_OBJC_DARK_COLOR);
    gem_objc_draw_vline(rect[0], rect[1], rect[3], GEM_OBJC_DARK_COLOR);
    gem_objc_draw_vline(rect[2], rect[1], rect[3], GEM_OBJC_DARK_COLOR);

    text_width = gem_objc_string_width(text != NULL ? text : "");
    text_x = (WORD) (rect[0] + 2);
    if (ted->te_just == TE_RIGHT) {
        text_x = (WORD) (rect[2] - text_width - 1);
        if (text_x < rect[0] + 2) {
            text_x = (WORD) (rect[0] + 2);
        }
    } else if (ted->te_just == TE_CNTR) {
        text_x = (WORD) (rect[0] + ((rect[2] - rect[0] + 1) - text_width) / 2);
        if (text_x < rect[0] + 2) {
            text_x = (WORD) (rect[0] + 2);
        }
    }
    gem_objc_draw_text(text_x, text_y, GEM_OBJC_DARK_COLOR, text);
}

static void gem_objc_draw_object(const OBJECT *tree, WORD object, WORD abs_x,
    WORD abs_y)
{
    const OBJECT *obj;
    LONG spec;
    WORD rect[4];
    WORD active;
    WORD fill_color;

    if (tree == NULL || object < 0) {
        return;
    }

    obj = &tree[object];
    spec = gem_objc_resolve_spec(obj);
    active = ((obj->ob_state & SELECTED) != 0u) ? 1 : 0;
    if (obj->ob_type == G_BUTTON && (obj->ob_state & CHECKED) != 0u) {
        active = 1;
    }
    fill_color = (active != 0) ? GEM_OBJC_DARK_COLOR : GEM_OBJC_LIGHT_COLOR;

    rect[0] = abs_x;
    rect[1] = abs_y;
    rect[2] = (WORD) (rect[0] + obj->ob_width - 1);
    rect[3] = (WORD) (rect[1] + obj->ob_height - 1);

    switch (obj->ob_type) {
    case G_BOX:
    case G_IBOX:
        gem_objc_fill_rect(rect[0], rect[1], rect[2], rect[3],
            GEM_OBJC_LIGHT_COLOR);
        if (object != ROOT) {
            gem_objc_draw_hline(rect[0], rect[2], rect[1],
                GEM_OBJC_DARK_COLOR);
            gem_objc_draw_hline(rect[0], rect[2], rect[3],
                GEM_OBJC_DARK_COLOR);
            gem_objc_draw_vline(rect[0], rect[1], rect[3],
                GEM_OBJC_DARK_COLOR);
            gem_objc_draw_vline(rect[2], rect[1], rect[3],
                GEM_OBJC_DARK_COLOR);
        }
        return;
    case G_BUTTON:
        gem_objc_fill_rect(rect[0], rect[1], rect[2], rect[3], fill_color);
        gem_objc_draw_button_frame(rect, GEM_OBJC_DARK_COLOR,
            (obj->ob_flags & DEFAULT) != 0u ? 1 : 0);
        break;
    case G_TEXT:
    case G_BOXTEXT:
    case G_FTEXT:
    case G_FBOXTEXT:
        gem_objc_draw_ted(obj, (const TEDINFO *) (intptr_t) spec, rect);
        return;
    default:
        break;
    }

    if (obj->ob_type == G_STRING && active != 0) {
        gem_objc_fill_rect(rect[0], rect[1], rect[2], rect[3], fill_color);
    }

    if ((obj->ob_type == G_STRING || obj->ob_type == G_BUTTON) &&
        spec != 0) {
        const char *text = (const char *) (intptr_t) spec;
        WORD text_width = gem_objc_string_width(text);
        WORD text_x = (WORD) (rect[0] + 2);
        WORD text_y;
        WORD text_color = (active != 0) ?
            GEM_OBJC_LIGHT_COLOR : GEM_OBJC_DARK_COLOR;

        gem_objc_ensure_metrics();
        text_y = (WORD) (rect[1] +
            ((obj->ob_height - g_objc_char_h) > 0 ?
            (obj->ob_height - g_objc_char_h) / 2 : 0) + g_objc_char_h - 2);
        if (obj->ob_type == G_BUTTON && text_width <= obj->ob_width) {
            text_x = (WORD) (rect[0] + (obj->ob_width - text_width) / 2);
        }
        gem_objc_draw_text(text_x, text_y, text_color, text);
    }
}

static void gem_objc_draw_tree_recursive(const OBJECT *tree, WORD object,
    WORD parent_x, WORD parent_y, WORD depth, const WORD clip[4])
{
    WORD abs_x;
    WORD abs_y;

    if (tree == NULL || object < 0) {
        return;
    }
    if ((tree[object].ob_flags & HIDETREE) != 0u) {
        return;
    }

    abs_x = (WORD) (parent_x + tree[object].ob_x);
    abs_y = (WORD) (parent_y + tree[object].ob_y);
    gem_objc_draw_object(tree, object, abs_x, abs_y);

    if (depth == 0 || tree[object].ob_head == NIL) {
        return;
    }

    {
        WORD child = tree[object].ob_head;

        while (child != NIL) {
            WORD next = tree[child].ob_next;

            gem_objc_draw_tree_recursive(tree, child, abs_x, abs_y,
                (depth > 0) ? (WORD) (depth - 1) : depth, clip);
            if (child == tree[object].ob_tail || next == object ||
                next == NIL) {
                break;
            }
            child = next;
        }
    }
}

WORD objc_draw(OBJECT *tree, WORD startob, WORD depth, WORD xc, WORD yc,
    WORD wc, WORD hc)
{
    WORD clip[4];
    WORD origin_x = 0;
    WORD origin_y = 0;

    if (tree == NULL || startob < 0) {
        return 0;
    }

    clip[0] = xc;
    clip[1] = yc;
    clip[2] = (WORD) (xc + wc - 1);
    clip[3] = (WORD) (yc + hc - 1);
    gem_objc_extent(tree, startob, &origin_x, &origin_y);
    vs_clip(0, 1, clip);
    gem_objc_draw_tree_recursive(tree, startob,
        (WORD) (origin_x - tree[startob].ob_x),
        (WORD) (origin_y - tree[startob].ob_y), depth, clip);
    vs_clip(0, 0, clip);
    return 1;
}

WORD objc_change(OBJECT *tree, WORD object, WORD depth, WORD xc, WORD yc,
    WORD wc, WORD hc, WORD newstate, WORD redraw)
{
    if (tree == NULL || object < 0) {
        return 0;
    }

    tree[object].ob_state = (UWORD) newstate;
    if (redraw != 0) {
        return objc_draw(tree, object, depth, xc, yc, wc, hc);
    }
    return 1;
}
