/*
 * Draws and hit-tests AES object trees: per-object rendering by type,
 * recursive traversal with clipping, absolute extents and objc_find
 * support.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (C) 2026 tomaz stih
 */

#include "window_internal.h"

#include "../vdi/vdi_internal.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

void aes_object_extent(OBJECT *tree, WORD object, WORD *x, WORD *y)
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
        current = aes_find_parent(tree, current);
    }

    if (x != NULL) {
        *x = abs_x;
    }
    if (y != NULL) {
        *y = abs_y;
    }
}

static void aes_draw_object(const OBJECT *tree, WORD object, WORD abs_x,
                            WORD abs_y, WORD clip[4])
{
    char shortcut_label[128];
    char shortcut_text[64];
    const OBJECT *obj;
    LONG spec;
    WORD rect[4];
    WORD parent;
    WORD active;
    WORD fill_color;
    WORD border_color;
    WORD inner_border_color;
    WORD text_width;
    WORD text_height;
    WORD text_ascent;
    WORD text_x;
    WORD text_y;
    WORD text_color;
    WORD text_background;
    int disabled_text;
    int checked_menu_item;
    int menu_popup_item;

    if (aes_state.vdi_ready == 0 || tree == NULL || object < 0) {
        return;
    }

    obj = &tree[object];
    parent = aes_find_parent((OBJECT *)tree, object);
    spec = aes_resolve_spec(obj);
    menu_popup_item =
        obj->ob_type == G_STRING && tree == aes_state.menu_tree &&
        parent != NIL && (tree[parent].ob_type == G_BOX ||
                          tree[parent].ob_type == G_IBOX) &&
        aes_is_menu_bar_object(tree, parent) == 0;
    checked_menu_item =
        (menu_popup_item != 0 && (obj->ob_state & CHECKED) != 0u) ? 1 : 0;
    active = ((obj->ob_state & SELECTED) != 0u) ? 1 : 0;
    if (obj->ob_type == G_BUTTON && (obj->ob_state & CHECKED) != 0u) {
        active = 1;
    }
    fill_color = (active != 0) ? aes_dark_color() : aes_light_color();
    border_color = aes_dark_color();
    inner_border_color = (active != 0) ? aes_light_color() : aes_dark_color();
    rect[0] = abs_x;
    rect[1] = abs_y;
    rect[2] = (WORD)(rect[0] + obj->ob_width - 1);
    rect[3] = (WORD)(rect[1] + obj->ob_height - 1);

    switch (obj->ob_type) {
        case G_BOX:
        case G_BOXCHAR:
            if (object == ROOT && aes_is_window_work_root(tree) != 0) {
                vsf_color(aes_state.vdi_handle, aes_light_color());
                v_bar(aes_state.vdi_handle, rect);
            } else if (object == ROOT && aes_is_dialog_root_tree(tree) != 0) {
                vsf_color(aes_state.vdi_handle, aes_light_color());
                v_bar(aes_state.vdi_handle, rect);
                aes_draw_dialog_frame(rect);
            } else if (aes_is_menu_bar_object(tree, object) != 0) {
                vsf_color(aes_state.vdi_handle, aes_light_color());
                v_bar(aes_state.vdi_handle, rect);
                aes_draw_hline(rect[0], rect[2], rect[3], border_color);
            } else if (object == ROOT && rect[0] == 0 &&
                       rect[2] >= aes_state.work_out[0] &&
                       rect[3] >= (WORD)(aes_state.work_out[1] / 2)) {
                aes_fill_checker_rect(rect[0], rect[1], rect[2], rect[3]);
            } else {
                vsf_color(aes_state.vdi_handle, fill_color);
                v_bar(aes_state.vdi_handle, rect);
            }
            if (object != ROOT) {
                if (aes_is_menu_bar_object(tree, object) == 0) {
                    if (aes_is_dialog_frame_object(tree, object, parent) != 0) {
                        aes_draw_dialog_frame(rect);
                    } else {
                        aes_draw_hline(rect[0], rect[2], rect[1], border_color);
                        aes_draw_hline(rect[0], rect[2], rect[3], border_color);
                        aes_draw_vline(rect[0], rect[1], rect[3], border_color);
                        aes_draw_vline(rect[2], rect[1], rect[3], border_color);
                    }
                }
            }
            break;
        case G_IBOX:
            if (object == ROOT && aes_is_window_work_root(tree) != 0) {
                vsf_color(aes_state.vdi_handle, aes_light_color());
                v_bar(aes_state.vdi_handle, rect);
            } else if (aes_is_dialog_frame_object(tree, object, parent) != 0) {
                vsf_color(aes_state.vdi_handle, aes_light_color());
                v_bar(aes_state.vdi_handle, rect);
                aes_draw_dialog_frame(rect);
            }
            break;
        case G_BUTTON:
            vsf_color(aes_state.vdi_handle, fill_color);
            v_bar(aes_state.vdi_handle, rect);
            aes_draw_button_frame(rect, border_color, inner_border_color,
                                  (obj->ob_flags & DEFAULT) != 0u ? 1 : 0);
            break;
        case G_TEXT:
        case G_BOXTEXT:
        case G_FTEXT:
        case G_FBOXTEXT:
            aes_draw_ted_object(tree, object, obj,
                                (const TEDINFO *)(intptr_t)spec, rect);
            return;
        case G_ICON:
            aes_draw_icon_object(obj, (const ICONBLK *)(intptr_t)spec, abs_x,
                                 abs_y, rect);
            return;
        case G_USERDEF:
            aes_draw_user_object(tree, object, obj, abs_x, abs_y, clip);
            return;
        default:
            break;
    }

    aes_draw_trace("obj type=%u rect=%d,%d-%d,%d spec=%p flags=%u state=%u",
                   obj->ob_type, rect[0], rect[1], rect[2], rect[3],
                   (const void *)(intptr_t)spec, obj->ob_flags, obj->ob_state);

    if (obj->ob_type == G_TITLE) {
        if (rect[1] + 1 <= rect[3] - 1) {
            aes_fill_rect(rect[0], (WORD)(rect[1] + 1), rect[2],
                          (WORD)(rect[3] - 1), aes_light_color());
        }
    } else if (menu_popup_item != 0) {
        WORD fill_right = rect[2];
        WORD parent_x = 0;
        WORD parent_y = 0;

        aes_object_extent((OBJECT *)tree, parent, &parent_x, &parent_y);
        (void)parent_y;
        fill_right = (WORD)(parent_x + tree[parent].ob_width - 2);

        if (fill_right < rect[0]) {
            fill_right = rect[0];
        }
        aes_fill_rect(rect[0], rect[1], fill_right, rect[3],
                      (active != 0) ? aes_dark_color() : aes_light_color());
    } else if ((obj->ob_type == G_STRING || obj->ob_type == G_TITLE) &&
               active != 0) {
        vsf_color(aes_state.vdi_handle, fill_color);
        v_bar(aes_state.vdi_handle, rect);
    }

    text_height = vdi_font_text_height();
    text_ascent = vdi_font_ascent();
    text_x = (WORD)(rect[0] + 2);
    if (obj->ob_type == G_TITLE) {
        text_y = (WORD)(rect[1] + 3 + text_ascent);
    } else {
        text_y = (WORD)(rect[1] +
                        ((obj->ob_height - text_height) > 0
                             ? (obj->ob_height - text_height) / 2
                             : 0) +
                        text_ascent);
    }
    if (obj->ob_type == G_TITLE) {
        text_color = aes_dark_color();
    } else {
        text_color = (active != 0) ? aes_light_color() : aes_dark_color();
    }
    text_background = (active != 0) ? aes_dark_color() : aes_light_color();
    disabled_text = ((obj->ob_state & DISABLED) != 0u) ? 1 : 0;
    if ((obj->ob_type == G_STRING || obj->ob_type == G_TITLE ||
         obj->ob_type == G_BUTTON) &&
        spec != 0) {
        const char *text = (const char *)(intptr_t)spec;
        int has_shortcut = 0;

        if (obj->ob_type == G_STRING && aes_menu_is_separator_text(text)) {
            WORD sep_left = (WORD)(rect[0] + 2);
            WORD sep_right = (WORD)(rect[2] - 2);
            WORD sep_y = (WORD)(rect[1] + obj->ob_height / 2);

            if (sep_right < sep_left) {
                sep_right = sep_left;
            }
            if (sep_y > rect[3]) {
                sep_y = rect[3];
            }
            if (sep_y > rect[1]) {
                aes_draw_hline(sep_left, sep_right, (WORD)(sep_y - 1),
                               aes_light_color());
            }
            aes_draw_hline(sep_left, sep_right, sep_y, aes_dark_color());
            return;
        }

        if (menu_popup_item != 0) {
            has_shortcut = aes_menu_split_shortcut(
                text, shortcut_label, sizeof(shortcut_label), shortcut_text,
                sizeof(shortcut_text));
            text = shortcut_label;
            text_x = (WORD)(rect[0] + AES_MENU_ITEM_PADDING);
        }

        text_width = (WORD)vdi_string_width(text);
        if ((obj->ob_type == G_BUTTON || obj->ob_type == G_TITLE) &&
            text_width <= obj->ob_width) {
            text_x = (WORD)(rect[0] + (obj->ob_width - text_width) / 2);
        }
        if (checked_menu_item != 0) {
            aes_draw_text((WORD)(rect[0] + AES_MENU_TICK_OFFSET), text_y,
                          text_color, "\010");
        }
        aes_draw_text(text_x, text_y, text_color, text);
        if (disabled_text != 0) {
            aes_stipple_text_pixels(text_x, text_y, text_color, text_background,
                                    text);
        }
        if (has_shortcut != 0 && shortcut_text[0] != '\0') {
            WORD shortcut_width = (WORD)vdi_string_width(shortcut_text);
            WORD shortcut_x = (WORD)(rect[2] - shortcut_width -
                                     AES_MENU_ITEM_PADDING + 1);

            if (shortcut_x > text_x) {
                aes_draw_text(shortcut_x, text_y, text_color, shortcut_text);
                if (disabled_text != 0) {
                    aes_stipple_text_pixels(shortcut_x, text_y, text_color,
                                            text_background, shortcut_text);
                }
            }
        }
        if (obj->ob_type == G_TITLE && active != 0 &&
            rect[1] + 1 <= rect[3] - 1) {
            aes_invert_rect(rect[0], (WORD)(rect[1] + 1), rect[2],
                            (WORD)(rect[3] - 1));
        }
    }
}

void aes_draw_tree_recursive(const OBJECT *tree, WORD object, WORD parent_x,
                             WORD parent_y, WORD depth, WORD clip[4])
{
    WORD abs_x;
    WORD abs_y;
    WORD child_clip[4];
    WORD clip_on = 0;

    if (tree == NULL || object < 0) {
        return;
    }
    WORD screen_clip[4] = {0, 0, aes_state.work_out[0], aes_state.work_out[1]};
    if (clip == NULL)
        clip = screen_clip;
    if (clip[0] > clip[2] || clip[1] > clip[3])
        return;
    if ((tree[object].ob_flags & HIDETREE) != 0u) {
        return;
    }

    abs_x = (WORD)(parent_x + tree[object].ob_x);
    abs_y = (WORD)(parent_y + tree[object].ob_y);
    if (clip != NULL) {
        vs_clip(aes_state.vdi_handle, 1, clip);
        clip_on = 1;
    }
    aes_draw_object(tree, object, abs_x, abs_y, clip);
    if (clip_on != 0) {
        vs_clip(aes_state.vdi_handle, 0, clip);
    }

    if (depth == 0 || tree[object].ob_head == NIL) {
        return;
    }

    {
        WORD child = tree[object].ob_head;

        memcpy(child_clip, clip, sizeof(child_clip));
        if (object == ROOT && aes_is_dialog_root_tree(tree) != 0) {
            WORD inner_clip[4];

            inner_clip[0] = (WORD)(abs_x + 5);
            inner_clip[1] = (WORD)(abs_y + 5);
            inner_clip[2] = (WORD)(abs_x + tree[object].ob_width - 7);
            inner_clip[3] = (WORD)(abs_y + tree[object].ob_height - 7);
            if (inner_clip[0] <= inner_clip[2] &&
                inner_clip[1] <= inner_clip[3]) {
                child_clip[0] = aes_max_word(child_clip[0], inner_clip[0]);
                child_clip[1] = aes_max_word(child_clip[1], inner_clip[1]);
                child_clip[2] = aes_min_word(child_clip[2], inner_clip[2]);
                child_clip[3] = aes_min_word(child_clip[3], inner_clip[3]);
            }
        }

        while (child != NIL) {
            WORD next = tree[child].ob_next;
            WORD *next_clip = child_clip;

            if (object == ROOT &&
                aes_is_dialog_frame_object(tree, child, object) != 0) {
                next_clip = clip;
            }

            aes_draw_tree_recursive(tree, child, abs_x, abs_y,
                                    (depth > 0) ? (WORD)(depth - 1) : depth,
                                    next_clip);
            if (child == tree[object].ob_tail || next == object ||
                next == NIL) {
                break;
            }
            child = next;
        }
    }
}

WORD aes_find_in_subtree(OBJECT *tree, WORD object, WORD parent_x,
                         WORD parent_y, WORD depth, WORD mx, WORD my)
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
            WORD child_hit = aes_find_in_subtree(
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
