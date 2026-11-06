/*
 * Tracks hosted AES menu interaction: title hit testing and highlighting,
 * popup display, item selection state, keyboard shortcuts and the mouse
 * tracking loop that yields MN_SELECTED messages.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (C) 2026 tomaz stih
 */

#include "menu_private.h"

#include "../vdi/vdi_internal.h"

#include "platform/os.h"

#include <string.h>

static int aes_menu_item_selectable(OBJECT *tree, WORD item);

static void aes_menu_set_item_selected(OBJECT *tree, WORD item, int selected)
{
    if (tree == NULL || item == NIL) {
        return;
    }

    if (selected) {
        tree[item].ob_state |= SELECTED;
    } else {
        tree[item].ob_state &= (UWORD)~SELECTED;
    }
}

static WORD aes_menu_hit_title(OBJECT *tree, WORD x, WORD y)
{
    WORD hit;

    if (tree == NULL) {
        return NIL;
    }

    hit = objc_find(tree, ROOT, MAX_DEPTH, x, y);
    while (hit != NIL) {
        if (tree[hit].ob_type == G_TITLE) {
            return hit;
        }
        hit = aes_find_parent(tree, hit);
    }

    return NIL;
}

static void aes_menu_set_title_selected(OBJECT *tree, WORD title, int selected)
{
    if (tree == NULL || title == NIL) {
        return;
    }

    if (selected) {
        tree[title].ob_state |= SELECTED;
    } else {
        tree[title].ob_state &= (UWORD)~SELECTED;
    }
}

static void aes_menu_clear_title_selection(OBJECT *tree)
{
    WORD parent;
    WORD child;

    if (tree == NULL) {
        return;
    }

    parent = aes_menu_title_container(tree);
    if (parent == NIL) {
        return;
    }

    child = tree[parent].ob_head;
    while (child != NIL) {
        WORD next = tree[child].ob_next;

        if (tree[child].ob_type == G_TITLE) {
            tree[child].ob_state &= (UWORD)~SELECTED;
        }
        if (child == tree[parent].ob_tail || next == parent || next == NIL) {
            break;
        }
        child = next;
    }
}

static int aes_menu_show_popup(OBJECT *tree, WORD title, WORD popup)
{
    if (tree == NULL || title == NIL || popup == NIL) {
        return 0;
    }

    aes_menu_hide_popups(tree);
    tree[popup].ob_flags &= (UWORD)~HIDETREE;
    aes_menu_set_title_selected(tree, title, 1);
    aes_menu_redraw_tree(tree);
    return 1;
}

static int aes_menu_shortcut_matches(const char *shortcut,
                                     const gem_hid_event_t *evt)
{
    char token[32];
    size_t length = 0;
    const char *scan;
    WORD ascii;
    WORD scancode;
    int index;

    if (shortcut == NULL || evt == NULL || evt->type != GEM_HID_KEY ||
        (evt->flags & 1u) == 0u) {
        return 0;
    }

    scan = shortcut;
    while (*scan == ' ' || *scan == '\t') {
        ++scan;
    }
    while (*scan != '\0' && *scan != ' ' && *scan != '\t' &&
           length + 1u < sizeof(token)) {
        char ch = *scan++;

        if (ch >= 'a' && ch <= 'z') {
            ch = (char)(ch - 'a' + 'A');
        }
        token[length++] = ch;
    }
    while (length > 0u &&
           (token[length - 1u] == ' ' || token[length - 1u] == '\t')) {
        --length;
    }
    token[length] = '\0';
    if (length == 0u) {
        return 0;
    }

    ascii = (WORD)(evt->key & 0xffu);
    scancode = (WORD)((evt->key >> 8) & 0xffu);

    if (strcmp(token, "ESC") == 0) {
        return ascii == 27 || scancode == 41;
    }

    if (token[0] == 'F' && token[1] >= '1' && token[1] <= '9') {
        index = token[1] - '0';
        if (token[2] >= '0' && token[2] <= '9') {
            index = index * 10 + (token[2] - '0');
        }
        return scancode == (WORD)(57 + index);
    }

    if (token[0] == '^' && token[1] != '\0' && token[2] == '\0') {
        WORD upper = (WORD)token[1];
        WORD lower = upper;

        if (lower >= 'A' && lower <= 'Z') {
            lower = (WORD)(lower - 'A' + 'a');
        }
        return ascii == (upper & 0x1f) || ascii == upper || ascii == lower;
    }

    if (length == 1u) {
        WORD upper = (WORD)token[0];
        WORD lower = upper;

        if (lower >= 'A' && lower <= 'Z') {
            lower = (WORD)(lower - 'A' + 'a');
        }
        return ascii == upper || ascii == lower;
    }

    return 0;
}

WORD aes_menu_key_event(OBJECT *tree, const gem_hid_event_t *evt,
                        WORD mepbuff[8])
{
    WORD parent;
    WORD title;

    if (tree == NULL || evt == NULL || mepbuff == NULL ||
        evt->type != GEM_HID_KEY || (evt->flags & 1u) == 0u) {
        return 0;
    }

    parent = aes_menu_title_container(tree);
    if (parent == NIL) {
        return 0;
    }

    memset(mepbuff, 0, sizeof(WORD) * 8u);
    title = tree[parent].ob_head;
    while (title != NIL) {
        WORD popup = aes_menu_popup_for_title(tree, title);
        WORD item;

        if (popup != NIL) {
            item = tree[popup].ob_head;
            while (item != NIL) {
                LONG spec = aes_resolve_spec(&tree[item]);
                const char *text = (const char *)(intptr_t)spec;
                char shortcut_text[64];
                char shortcut_label[128];
                WORD next = tree[item].ob_next;

                if (aes_menu_item_selectable(tree, item) != 0 &&
                    aes_menu_split_shortcut(
                        text, shortcut_label, sizeof(shortcut_label),
                        shortcut_text, sizeof(shortcut_text)) != 0 &&
                    aes_menu_shortcut_matches(shortcut_text, evt) != 0) {
                    mepbuff[0] = MN_SELECTED;
                    mepbuff[1] = aes_state.current_app_id;
                    mepbuff[3] = title;
                    mepbuff[4] = item;
                    return 1;
                }

                if (item == tree[popup].ob_tail || next == popup ||
                    next == NIL) {
                    break;
                }
                item = next;
            }
        }

        if (title == tree[parent].ob_tail || tree[title].ob_next == parent ||
            tree[title].ob_next == NIL) {
            break;
        }
        title = tree[title].ob_next;
    }

    return 0;
}

WORD aes_menu_event(OBJECT *tree, const gem_hid_event_t *first_evt,
                    WORD mepbuff[8])
{
    int latched;
    int sticky;
    WORD title;
    WORD popup;
    WORD item = NIL;
    WORD highlighted_item = NIL;

    if (tree == NULL || first_evt == NULL || mepbuff == NULL ||
        first_evt->type != GEM_HID_MOUSE_BUTTON ||
        (first_evt->flags & GEM_HID_BUTTON_LEFT) == 0u) {
        return 0;
    }

    memset(mepbuff, 0, sizeof(WORD) * 8u);

    title = aes_menu_hit_title(tree, (WORD)first_evt->x, (WORD)first_evt->y);
    if (title == NIL) {
        return 0;
    }

    popup = aes_menu_popup_for_title(tree, title);
    if (popup == NIL) {
        return 0;
    }

    if (!aes_menu_show_popup(tree, title, popup)) {
        return 0;
    }
    sticky = (aes_state.menu_click != 0);
    latched = 0;

    FOREVER
    {
        gem_hid_event_t evt;

        if (gem_hid_poll(&evt) == 0) {
            gem_os_sleep_ms(1u);
            continue;
        }

        if (evt.type == GEM_HID_MOUSE_MOVE ||
            evt.type == GEM_HID_MOUSE_BUTTON) {
            aes_store_mouse_state(&evt);
        }

        if (evt.type == GEM_HID_MOUSE_BUTTON &&
            (evt.flags & GEM_HID_BUTTON_LEFT) != 0u) {
            WORD press_title =
                aes_menu_hit_title(tree, (WORD)evt.x, (WORD)evt.y);

            if (press_title != NIL && press_title != title) {
                WORD next_popup = aes_menu_popup_for_title(tree, press_title);

                if (next_popup != NIL) {
                    aes_menu_set_title_selected(tree, title, 0);
                    title = press_title;
                    popup = next_popup;
                    item = NIL;
                    latched = 0;
                    (void)aes_menu_show_popup(tree, title, popup);
                }
            }
            continue;
        }

        if (evt.type == GEM_HID_MOUSE_MOVE) {
            WORD hover_title =
                aes_menu_hit_title(tree, (WORD)evt.x, (WORD)evt.y);

            if (hover_title != NIL && hover_title != title) {
                WORD next_popup = aes_menu_popup_for_title(tree, hover_title);

                if (next_popup != NIL) {
                    aes_menu_set_title_selected(tree, title, 0);
                    title = hover_title;
                    popup = next_popup;
                    item = NIL;
                    (void)aes_menu_show_popup(tree, title, popup);
                    continue;
                }
            }

            {
                WORD hover =
                    objc_find(tree, popup, MAX_DEPTH, (WORD)evt.x, (WORD)evt.y);

                if (aes_menu_item_selectable(tree, hover)) {
                    item = hover;
                } else if (objc_find(tree, title, 0, (WORD)evt.x,
                                     (WORD)evt.y) == title) {
                    item = NIL;
                }
            }

            if (item != highlighted_item) {
                if (highlighted_item != NIL) {
                    aes_menu_set_item_selected(tree, highlighted_item, 0);
                }
                if (item != NIL) {
                    aes_menu_set_item_selected(tree, item, 1);
                }
                highlighted_item = item;
                aes_menu_redraw_tree(tree);
            }
            continue;
        }

        if (evt.type == GEM_HID_MOUSE_BUTTON &&
            (evt.flags & GEM_HID_BUTTON_LEFT) == 0u) {
            WORD release_title =
                aes_menu_hit_title(tree, (WORD)evt.x, (WORD)evt.y);
            WORD release_hit =
                objc_find(tree, popup, MAX_DEPTH, (WORD)evt.x, (WORD)evt.y);

            if (aes_menu_item_selectable(tree, release_hit)) {
                item = release_hit;
            } else {
                /*
                 * Releasing off any selectable item must cancel,
                 * not fall back to whatever was last highlighted
                 * while dragging through the popup -- otherwise a
                 * release anywhere (even outside the whole menu)
                 * reports that stale item as chosen.
                 */
                item = NIL;
            }

            if (item == NIL && sticky) {
                if (release_title == title && latched == 0) {
                    latched = 1;
                    continue;
                }
                if (release_title != NIL && release_title != title) {
                    latched = 1;
                    continue;
                }
            }

            if (highlighted_item != NIL) {
                aes_menu_set_item_selected(tree, highlighted_item, 0);
            }
            aes_menu_hide_popups(tree);
            aes_menu_clear_title_selection(tree);
            aes_menu_redraw_tree(tree);

            if (item == NIL) {
                return 0;
            }

            mepbuff[0] = MN_SELECTED;
            mepbuff[1] = aes_state.current_app_id;
            mepbuff[3] = title;
            mepbuff[4] = item;
            return 1;
        }
    }
}

static int aes_menu_item_selectable(OBJECT *tree, WORD item)
{
    LONG spec;
    const char *text;

    if (tree == NULL || item == NIL || tree[item].ob_type != G_STRING ||
        (tree[item].ob_state & DISABLED) != 0u) {
        return 0;
    }

    spec = aes_resolve_spec(&tree[item]);
    text = (const char *)(intptr_t)spec;
    return !aes_menu_is_separator_text(text);
}
