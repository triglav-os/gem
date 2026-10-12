/*
 * Renders the component parts of AES objects: dialog and window tree
 * classification, editable text fields, dialog and button frames, icons
 * and USERDEF callbacks.
 *
 * MIT License (see: LICENSE)
 * Copyright (C) 2026 tomaz stih
 */

#include "window_internal.h"

#include "../vdi/vdi_internal.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

int aes_is_window_work_root(const OBJECT *tree)
{
    size_t i;

    if (tree == NULL) {
        return 0;
    }

    for (i = 0; i < AES_MAX_WINDOWS; ++i) {
        const aes_window_t *window = &aes_state.windows[i];

        if (window->used == 0 || window->open == 0) {
            continue;
        }
        if (tree[ROOT].ob_x == window->work.g_x &&
            tree[ROOT].ob_y == window->work.g_y &&
            tree[ROOT].ob_width == window->work.g_w &&
            tree[ROOT].ob_height == window->work.g_h) {
            return 1;
        }
    }

    return 0;
}

int aes_is_dialog_root_tree(const OBJECT *tree)
{
    if (tree == NULL || tree == aes_state.menu_tree) {
        return 0;
    }

    if (aes_is_window_work_root(tree) != 0) {
        return 0;
    }

    if (tree[ROOT].ob_head == NIL) {
        return 0;
    }

    return (tree[ROOT].ob_type == G_IBOX || tree[ROOT].ob_type == G_BOX) ? 1
                                                                         : 0;
}

int aes_is_menu_bar_object(const OBJECT *tree, WORD object)
{
    WORD bar;

    if (tree == NULL || tree != aes_state.menu_tree || object < 0) {
        return 0;
    }

    bar = tree[ROOT].ob_head;
    return (bar != NIL && object == bar) ? 1 : 0;
}

int aes_is_dialog_frame_object(const OBJECT *tree, WORD object, WORD parent)
{
    if (tree == NULL || object <= ROOT || parent != ROOT) {
        return 0;
    }

    if (aes_is_dialog_root_tree(tree) == 0) {
        return 0;
    }

    return (tree[object].ob_type == G_BOX || tree[object].ob_type == G_IBOX)
               ? 1
               : 0;
}

void aes_draw_ted_object(const OBJECT *tree, WORD object, const OBJECT *obj,
                         const TEDINFO *ted, const WORD rect[4])
{
    char clipped_text[256];
    const char *text;
    const char *rendered_text;
    WORD text_x;
    WORD text_y;
    WORD text_width;
    WORD caret_width;
    WORD caret_top;
    WORD caret_bottom;
    WORD caret_x;
    WORD text_color;
    WORD attributes[10] = {0};
    WORD previous_font = 0;
    WORD text_left;
    WORD text_right;
    WORD text_top;
    WORD text_bottom;
    WORD inset;
    size_t text_length;
    int classic_ted;

    if (obj == NULL || ted == NULL) {
        return;
    }

    if (vqt_attributes(aes_state.vdi_handle, attributes) != 0) {
        previous_font = attributes[0];
    }
    (void)vst_font(aes_state.vdi_handle, ted->te_font);

    text = (const char *)(intptr_t)ted->te_ptext;
    aes_draw_trace("ted object=%d type=%u font=%d just=%d rect=%d,%d-%d,%d "
                   "text=\"%s\"",
                   object, (unsigned)obj->ob_type, ted->te_font, ted->te_just,
                   rect[0], rect[1], rect[2], rect[3],
                   text != NULL ? text : "");
    classic_ted = ted->te_font == -3 || ted->te_font == -5;
    inset = classic_ted != 0 && ted->te_thickness > 0 ? ted->te_thickness : 0;
    text_left = classic_ted != 0 ? (WORD)(rect[0] + inset)
                                 : (WORD)(rect[0] + 2);
    text_top = (WORD)(rect[1] + inset);
    text_right = (WORD)(rect[2] - inset);
    text_bottom = (WORD)(rect[3] - inset);
    if (text_right < text_left) {
        text_right = text_left;
    }
    if (text_bottom < text_top) {
        text_bottom = text_top;
    }
    text_x = text_left;
    text_color = aes_dark_color();
    rendered_text = text != NULL ? text : "";
    if (classic_ted != 0) {
        text_length = 0u;
        text_width = 0;
        while (rendered_text[text_length] != '\0' &&
               text_length + 1u < sizeof(clipped_text)) {
            WORD width = vdi_char_cell_width(rendered_text[text_length]);

            if (width <= 0 ||
                (LONG)text_width + width > text_right - text_left + 1) {
                break;
            }
            clipped_text[text_length] = rendered_text[text_length];
            text_width = (WORD)(text_width + width);
            ++text_length;
        }
        clipped_text[text_length] = '\0';
        rendered_text = clipped_text;
    } else {
        text_width = (WORD)vdi_string_width(rendered_text);
    }
    caret_width = text_width;

    if (obj->ob_type == G_FTEXT || obj->ob_type == G_FBOXTEXT) {
        text_y =
            (WORD)(text_top +
                   ((text_bottom - text_top + 1 - vdi_font_text_height()) > 0
                        ? (text_bottom - text_top +
                           (classic_ted != 0 ? 2 : 1) -
                           vdi_font_text_height()) /
                              2
                        : 0) +
                   vdi_font_ascent());
        caret_top = (WORD)(text_y - vdi_font_ascent() + 1);
        caret_bottom = (WORD)(caret_top + vdi_font_text_height() - 2);
        if (obj->ob_type == G_FBOXTEXT) {
            aes_fill_rect(rect[0], rect[1], rect[2], rect[3],
                          aes_light_color());
            aes_draw_hline(rect[0], rect[2], rect[1], aes_dark_color());
            aes_draw_hline(rect[0], rect[2], rect[3], aes_dark_color());
            aes_draw_vline(rect[0], rect[1], rect[3], aes_dark_color());
            aes_draw_vline(rect[2], rect[1], rect[3], aes_dark_color());
            text_x = classic_ted != 0 ? text_left : (WORD)(rect[0] + 3);
            if (caret_top < rect[1] + 1) {
                caret_top = (WORD)(rect[1] + 1);
            }
            if (caret_bottom > rect[3] - 1) {
                caret_bottom = (WORD)(rect[3] - 1);
            }
        } else {
            aes_fill_rect(rect[0], rect[1], rect[2], rect[3],
                          aes_light_color());
            if (caret_top < rect[1]) {
                caret_top = rect[1];
            }
            if (caret_bottom > rect[3]) {
                caret_bottom = rect[3];
            }
        }
        if (tree == aes_state.edit_tree && object == aes_state.edit_object &&
            text != NULL) {
            WORD edit_index = aes_state.edit_index;
            WORD text_length = (WORD)strlen(text);
            char prefix[256];

            if (edit_index < 0) {
                edit_index = 0;
            }
            if (edit_index > text_length) {
                edit_index = text_length;
            }
            if ((size_t)edit_index >= sizeof(prefix)) {
                edit_index = (WORD)(sizeof(prefix) - 1u);
            }
            memcpy(prefix, text, (size_t)edit_index);
            prefix[edit_index] = '\0';
            caret_width = (WORD)vdi_string_width(prefix);
        }
        if ((obj->ob_state & SELECTED) != 0u) {
            caret_x = (WORD)(text_x + caret_width + 1);
            if (caret_x < rect[0] + 1) {
                caret_x = (WORD)(rect[0] + 1);
            }
            if (caret_x > rect[2] - 1) {
                caret_x = (WORD)(rect[2] - 1);
            }
            aes_draw_vline(caret_x, caret_top, caret_bottom, aes_dark_color());
        }
    } else if (obj->ob_type == G_BOXTEXT) {
        text_y =
            (WORD)(text_top +
                   ((text_bottom - text_top + 1 - vdi_font_text_height()) > 0
                        ? (text_bottom - text_top +
                           (classic_ted != 0 ? 2 : 1) -
                           vdi_font_text_height()) /
                              2
                        : 0) +
                   vdi_font_ascent());
        aes_fill_rect(rect[0], rect[1], rect[2], rect[3], aes_light_color());
        aes_draw_hline(rect[0], rect[2], rect[1], aes_dark_color());
        aes_draw_hline(rect[0], rect[2], rect[3], aes_dark_color());
        aes_draw_vline(rect[0], rect[1], rect[3], aes_dark_color());
        aes_draw_vline(rect[2], rect[1], rect[3], aes_dark_color());
    } else {
        text_y =
            (WORD)(text_top +
                   ((text_bottom - text_top + 1 - vdi_font_text_height()) > 0
                        ? (text_bottom - text_top +
                           (classic_ted != 0 ? 2 : 1) -
                           vdi_font_text_height()) /
                              2
                        : 0) +
                   vdi_font_ascent());
    }

    if (classic_ted != 0) {
        if (ted->te_just == TE_RIGHT) {
            text_x = (WORD)(text_right - text_width + 1);
        } else if (ted->te_just == TE_CNTR) {
            text_x = (WORD)(text_left +
                            (text_right - text_left + 2 - text_width) / 2);
        }
    } else if (ted->te_just == TE_RIGHT) {
        text_x = (WORD)(rect[2] - text_width - 1);
        if (text_x < rect[0] + 2) {
            text_x = (WORD)(rect[0] + 2);
        }
    } else if (ted->te_just == TE_CNTR) {
        text_x = (WORD)(rect[0] + ((rect[2] - rect[0] + 1) - text_width) / 2);
        if (text_x < rect[0] + 2) {
            text_x = (WORD)(rect[0] + 2);
        }
    }

    if (text_bottom - text_top + 1 >= vdi_font_text_height()) {
        aes_draw_text(text_x, text_y, text_color, rendered_text);
    }
    if (previous_font != 0) {
        (void)vst_font(aes_state.vdi_handle, previous_font);
    }
}

void aes_draw_dialog_frame(const WORD rect[4])
{
    WORD inset;
    WORD left;
    WORD top;
    WORD right;
    WORD bottom;

    for (inset = 0; inset < 5; ++inset) {
        WORD color =
            (inset == 0 || inset >= 3) ? aes_dark_color() : aes_light_color();

        left = (WORD)(rect[0] + inset);
        top = (WORD)(rect[1] + inset);
        right = (WORD)(rect[2] - inset);
        bottom = (WORD)(rect[3] - inset);
        if (left > right || top > bottom) {
            break;
        }

        aes_draw_hline(left, right, top, color);
        aes_draw_hline(left, right, bottom, color);
        aes_draw_vline(left, top, bottom, color);
        aes_draw_vline(right, top, bottom, color);
    }
}

void aes_draw_button_frame(const WORD rect[4], WORD dark_color,
                           WORD light_color, int default_button)
{
    WORD inset;
    WORD border_thickness = default_button != 0 ? 2 : 1;

    (void)light_color;

    for (inset = 0; inset < border_thickness; ++inset) {
        WORD left = (WORD)(rect[0] + inset);
        WORD top = (WORD)(rect[1] + inset);
        WORD right = (WORD)(rect[2] - inset);
        WORD bottom = (WORD)(rect[3] - inset);

        if (left > right || top > bottom) {
            break;
        }

        aes_draw_hline(left, right, top, dark_color);
        aes_draw_hline(left, right, bottom, dark_color);
        aes_draw_vline(left, top, bottom, dark_color);
        aes_draw_vline(right, top, bottom, dark_color);
    }
}

void aes_draw_icon_object(const OBJECT *obj, const ICONBLK *icon, WORD abs_x,
                          WORD abs_y, const WORD rect[4])
{
    const char *text;
    WORD icon_box[4];
    WORD text_x;
    WORD text_y;

    if (obj == NULL || icon == NULL) {
        return;
    }

    icon_box[0] = (WORD)(abs_x + icon->ib_xicon);
    icon_box[1] = (WORD)(abs_y + icon->ib_yicon);
    icon_box[2] = (WORD)(icon_box[0] + icon->ib_wicon - 1);
    icon_box[3] = (WORD)(icon_box[1] + icon->ib_hicon - 1);

    if ((obj->ob_state & SELECTED) != 0u) {
        vsf_color(aes_state.vdi_handle, aes_light_color());
        v_bar(aes_state.vdi_handle, rect);
    }

    vsl_color(aes_state.vdi_handle, aes_dark_color());
    v_rbox(aes_state.vdi_handle, icon_box);

    text = (const char *)(intptr_t)icon->ib_ptext;
    text_x = (WORD)(abs_x + icon->ib_xtext);
    text_y = (WORD)(abs_y + icon->ib_ytext + AES_CHAR_HEIGHT);
    aes_draw_trace("icon obj=%p rect=%d,%d-%d,%d icon=%d,%d %dx%d text=%p",
                   (const void *)obj, rect[0], rect[1], rect[2], rect[3],
                   icon_box[0], icon_box[1], icon->ib_wicon, icon->ib_hicon,
                   (const void *)text);
    aes_draw_text(text_x, text_y, aes_dark_color(), text);
}

void aes_draw_user_object(const OBJECT *tree, WORD object, const OBJECT *obj,
                          WORD abs_x, WORD abs_y, WORD clip[4])
{
    USERBLK *user;
    PARMBLK parm;
    aes_user_draw_t draw;

    if (tree == NULL || obj == NULL) {
        return;
    }

    user = (USERBLK *)(intptr_t)aes_resolve_spec(obj);
    if (user == NULL || user->ab_code == 0) {
        return;
    }

    memset(&parm, 0, sizeof(parm));
    parm.pb_tree = (LONG)(intptr_t)tree;
    parm.pb_obj = object;
    parm.pb_prevstate = obj->ob_state;
    parm.pb_currstate = obj->ob_state;
    parm.pb_x = abs_x;
    parm.pb_y = abs_y;
    parm.pb_w = obj->ob_width;
    parm.pb_h = obj->ob_height;
    parm.pb_xc = clip[0];
    parm.pb_yc = clip[1];
    parm.pb_wc = (WORD)(clip[2] - clip[0] + 1);
    parm.pb_hc = (WORD)(clip[3] - clip[1] + 1);
    parm.pb_parm = user->ab_parm;

    draw = (aes_user_draw_t)(intptr_t)user->ab_code;
    (void)draw((LONG)(intptr_t)&parm);
}
