/*
 * Implements the private hosted AES menu-tree normalization,
 * popup tracking, and saved-region management helpers.
 *
 * MIT License (see: LICENSE)
 * Copyright (C) 2026 tomaz stih
 */

#include "_aes.h"

#include "../vdi/_internal.h"

#include "platform/os.h"

#include <string.h>
static int _aes_menu_redraw_in_progress;

static WORD _aes_menu_popup_container(OBJECT *tree);
static WORD _aes_menu_first_popup_child(OBJECT *tree, WORD popup_parent);
static WORD _aes_menu_last_object(OBJECT *tree);
static WORD _aes_menu_title_container(OBJECT *tree);
static WORD _aes_menu_nth_child(OBJECT *tree, WORD parent, WORD index);
static WORD _aes_menu_index_for_title(OBJECT *tree, WORD title);
static WORD _aes_menu_popup_for_title(OBJECT *tree, WORD title);
static void _aes_menu_set_item_selected(OBJECT *tree,
                                        WORD item,
                                        int selected);
static int _aes_menu_shortcut_matches(const char *shortcut,
                                      const gem_hid_event_t *evt);
void _aes_menu_hide_popups(OBJECT *tree);
static WORD _aes_menu_hit_title(OBJECT *tree, WORD x, WORD y);
static void _aes_menu_set_title_selected(OBJECT *tree,
                                         WORD title,
                                         int selected);
static void _aes_menu_clear_title_selection(OBJECT *tree);
void _aes_menu_redraw_tree(OBJECT *tree);
void _aes_menu_clear_saved_region(void);
void _aes_menu_prepare_tree(OBJECT *tree);
static int _aes_menu_show_popup(OBJECT *tree, WORD title, WORD popup);
WORD _aes_menu_event(OBJECT *tree,
                     const gem_hid_event_t *first_evt,
                     WORD mepbuff[8]);
int _aes_menu_subtree_rect(OBJECT *tree, WORD object, GRECT *rect);
static void _aes_menu_expand_saved_rect(OBJECT *tree, WORD object, GRECT *rect);
static int _aes_menu_item_selectable(OBJECT *tree, WORD item);
static void _aes_menu_free_saved_pixels(void);
static void _aes_menu_restore_saved_region(void);
static int _aes_menu_save_region(const GRECT *rect);

static WORD _aes_menu_popup_container(OBJECT *tree)
{
    if (tree == NULL) {
        return NIL;
    }

    if (_aes.menu_popup_root_direct != 0) {
        return ROOT;
    }

    return tree[ROOT].ob_tail;
}

static WORD _aes_menu_first_popup_child(OBJECT *tree, WORD popup_parent)
{
    WORD child;

    if (tree == NULL || popup_parent == NIL) {
        return NIL;
    }

    child = tree[popup_parent].ob_head;
    if (child == NIL) {
        return NIL;
    }

    if (popup_parent == ROOT && _aes.menu_popup_root_direct != 0) {
        child = tree[child].ob_next;
        if (child == ROOT) {
            return NIL;
        }
    }

    return child;
}

static WORD _aes_menu_last_object(OBJECT *tree)
{
    WORD i;

    if (tree == NULL) {
        return ROOT;
    }

    for (i = ROOT; i < 256; ++i) {
        if ((tree[i].ob_flags & LASTOB) != 0u) {
            return i;
        }
    }

    return ROOT;
}

static WORD _aes_menu_title_container(OBJECT *tree)
{
    WORD root_child;
    WORD first_child;

    if (tree == NULL) {
        return NIL;
    }

    root_child = tree[ROOT].ob_head;
    if (root_child == NIL) {
        return NIL;
    }

    first_child = tree[root_child].ob_head;
    if (first_child == NIL) {
        return root_child;
    }
    if (tree[first_child].ob_head != NIL) {
        return first_child;
    }
    return root_child;
}

static WORD _aes_menu_nth_child(OBJECT *tree, WORD parent, WORD index)
{
    WORD child;
    WORD current_index = 0;

    if (tree == NULL || parent == NIL) {
        return NIL;
    }

    child = _aes_menu_first_popup_child(tree, parent);
    while (child != NIL) {
        WORD next = tree[child].ob_next;

        if (current_index == index) {
            return child;
        }
        ++current_index;
        if (child == tree[parent].ob_tail || next == parent || next == NIL ||
            next == ROOT) {
            break;
        }
        child = next;
    }

    return NIL;
}

static WORD _aes_menu_index_for_title(OBJECT *tree, WORD title)
{
    WORD parent = _aes_menu_title_container(tree);
    WORD child;
    WORD index = 0;

    if (tree == NULL || parent == NIL) {
        return -1;
    }

    child = tree[parent].ob_head;
    while (child != NIL) {
        WORD next = tree[child].ob_next;

        if (child == title) {
            return index;
        }
        ++index;
        if (child == tree[parent].ob_tail || next == parent || next == NIL) {
            break;
        }
        child = next;
    }

    return -1;
}

static WORD _aes_menu_popup_for_title(OBJECT *tree, WORD title)
{
    WORD popup_parent = _aes_menu_popup_container(tree);
    WORD title_index = _aes_menu_index_for_title(tree, title);

    if (title_index < 0 || popup_parent == NIL) {
        return NIL;
    }

    return _aes_menu_nth_child(tree, popup_parent, title_index);
}

static void _aes_menu_set_item_selected(OBJECT *tree,
                                        WORD item,
                                        int selected)
{
    if (tree == NULL || item == NIL) {
        return;
    }

    if (selected) {
        tree[item].ob_state |= SELECTED;
    } else {
        tree[item].ob_state &= (UWORD) ~SELECTED;
    }
}

void _aes_menu_hide_popups(OBJECT *tree)
{
    WORD popup_parent;
    WORD child;

    if (tree == NULL) {
        return;
    }

    popup_parent = _aes_menu_popup_container(tree);
    if (popup_parent == NIL) {
        return;
    }

    child = _aes_menu_first_popup_child(tree, popup_parent);
    while (child != NIL) {
        WORD next = tree[child].ob_next;

        tree[child].ob_flags |= HIDETREE;
        if (child == tree[popup_parent].ob_tail || next == popup_parent ||
            next == NIL || next == ROOT) {
            break;
        }
        child = next;
    }
}

static WORD _aes_menu_hit_title(OBJECT *tree, WORD x, WORD y)
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
        hit = _aes_find_parent(tree, hit);
    }

    return NIL;
}

static void _aes_menu_set_title_selected(OBJECT *tree,
                                         WORD title,
                                         int selected)
{
    if (tree == NULL || title == NIL) {
        return;
    }

    if (selected) {
        tree[title].ob_state |= SELECTED;
    } else {
        tree[title].ob_state &= (UWORD) ~SELECTED;
    }
}

static void _aes_menu_clear_title_selection(OBJECT *tree)
{
    WORD parent;
    WORD child;

    if (tree == NULL) {
        return;
    }

    parent = _aes_menu_title_container(tree);
    if (parent == NIL) {
        return;
    }

    child = tree[parent].ob_head;
    while (child != NIL) {
        WORD next = tree[child].ob_next;

        if (tree[child].ob_type == G_TITLE) {
            tree[child].ob_state &= (UWORD) ~SELECTED;
        }
        if (child == tree[parent].ob_tail || next == parent || next == NIL) {
            break;
        }
        child = next;
    }
}

void _aes_menu_redraw_tree(OBJECT *tree)
{
    WORD bar;
    WORD popup_parent;
    WORD popup;
    WORD visible_popup_count = 0;
    GRECT redraw_rect;

    if (tree == NULL || _aes_ensure_vdi() == 0) {
        return;
    }

    if (_aes_menu_redraw_in_progress != 0) {
        return;
    }

    _aes_menu_redraw_in_progress = 1;

    _vdi_begin_update();
    v_hide_c(_aes.vdi_handle);

    _aes_menu_restore_saved_region();

    bar = tree[ROOT].ob_head;
    popup_parent = _aes_menu_popup_container(tree);
    popup = _aes_menu_first_popup_child(tree, popup_parent);
    _aes_menu_free_saved_pixels();

    if (bar != NIL && _aes_menu_subtree_rect(tree, bar, &redraw_rect) != 0) {
        _aes_menu_expand_saved_rect(tree, bar, &redraw_rect);
        (void) _aes_menu_save_region(&redraw_rect);
    }

    while (popup != NIL) {
        WORD next = tree[popup].ob_next;

        if ((tree[popup].ob_flags & HIDETREE) == 0u &&
            _aes_menu_subtree_rect(tree, popup, &redraw_rect) != 0) {
            ++visible_popup_count;
            _aes_menu_expand_saved_rect(tree, popup, &redraw_rect);
            (void) _aes_menu_save_region(&redraw_rect);
        }

        if (popup == tree[popup_parent].ob_tail || next == popup_parent ||
            next == NIL || next == ROOT) {
            break;
        }
        popup = next;
    }

    if (bar != NIL && _aes_menu_subtree_rect(tree, bar, &redraw_rect) != 0) {
        WORD fill[4];
        GRECT seam;

        fill[0] = redraw_rect.g_x;
        fill[1] = redraw_rect.g_y;
        fill[2] = (WORD) (redraw_rect.g_x + redraw_rect.g_w - 1);
        fill[3] = (WORD) (redraw_rect.g_y + redraw_rect.g_h - 1);
        vsf_color(_aes.vdi_handle, _aes_light_color());
        v_bar(_aes.vdi_handle, fill);
        objc_draw(tree, bar, MAX_DEPTH, redraw_rect.g_x, redraw_rect.g_y,
            redraw_rect.g_w, redraw_rect.g_h);

        /*
         * Repaint the first row below the menu bar from the underlying
         * desktop/windows before popups are drawn. This clears any stale
         * seam pixels left from previous popup geometry changes while
         * still allowing the active popup to paint over its own span.
         */
        _aes_set_rect(&seam, redraw_rect.g_x,
            (WORD) (redraw_rect.g_y + redraw_rect.g_h), redraw_rect.g_w, 1);
        _aes_redraw_region(&seam);
    }

    popup = _aes_menu_first_popup_child(tree, popup_parent);
    while (popup != NIL) {
        WORD next = tree[popup].ob_next;

        if ((tree[popup].ob_flags & HIDETREE) == 0u &&
            _aes_menu_subtree_rect(tree, popup, &redraw_rect) != 0) {
            WORD fill[4];

            fill[0] = redraw_rect.g_x;
            fill[1] = redraw_rect.g_y;
            fill[2] = (WORD) (redraw_rect.g_x + redraw_rect.g_w - 1);
            fill[3] = (WORD) (redraw_rect.g_y + redraw_rect.g_h - 1);
            vsf_color(_aes.vdi_handle, _aes_light_color());
            v_bar(_aes.vdi_handle, fill);
            objc_draw(tree, popup, MAX_DEPTH, redraw_rect.g_x,
                redraw_rect.g_y, redraw_rect.g_w, redraw_rect.g_h);
        }

        if (popup == tree[popup_parent].ob_tail || next == popup_parent ||
            next == NIL || next == ROOT) {
            break;
        }
        popup = next;
    }

    if (visible_popup_count == 0 && bar != NIL &&
        _aes_menu_subtree_rect(tree, bar, &redraw_rect) != 0) {
        GRECT repair;

        repair.g_x = 0;
        repair.g_y = (WORD) (redraw_rect.g_y + redraw_rect.g_h - 1);
        repair.g_w = (WORD) (_aes.work_out[0] + 1);
        repair.g_h = 2;
        if (repair.g_y < 0) {
            repair.g_y = 0;
        }
        if (repair.g_y <= _aes.work_out[1]) {
            if (repair.g_y + repair.g_h - 1 > _aes.work_out[1]) {
                repair.g_h = (WORD) (_aes.work_out[1] - repair.g_y + 1);
            }
            if (repair.g_h > 0) {
                _aes_redraw_region(&repair);
            }
        }
    }

    v_show_c(_aes.vdi_handle, 1);
    _vdi_end_update();
    _aes_menu_redraw_in_progress = 0;
}

void _aes_menu_clear_saved_region(void)
{
    _vdi_begin_update();
    v_hide_c(_aes.vdi_handle);
    _aes_menu_restore_saved_region();
    v_show_c(_aes.vdi_handle, 1);
    _vdi_end_update();
}

void _aes_menu_prepare_tree(OBJECT *tree)
{
    WORD last_object;
    WORD wchar = AES_CHAR_WIDTH;
    WORD hchar = AES_CHAR_HEIGHT;
    WORD boxw = AES_DECOR;
    WORD boxh = _aes_menu_chrome_height();
    WORD menu_bar_height;
    WORD menu_item_height;
    WORD menu_separator_height;
    WORD bar;
    WORD title_parent;
    WORD popup_parent;
    WORD i;
    int looks_char_sized = 0;
    WORD max_right = 0;
    WORD max_bottom = 0;
    WORD first_title = NIL;
    WORD last_title = NIL;
    WORD popup_roots[16];
    WORD popup_count = 0;

    if (tree == NULL || _aes_ensure_vdi() == 0) {
        return;
    }

    _aes.menu_popup_root_direct = 0;
    last_object = _aes_menu_last_object(tree);
    bar = tree[ROOT].ob_head;
    title_parent = _aes_menu_title_container(tree);
    popup_parent = _aes_menu_popup_container(tree);

    if (bar != NIL) {
        WORD candidate = tree[bar].ob_head;
        WORD j;

        if (candidate == NIL || tree[candidate].ob_type != G_TITLE) {
            if (title_parent != NIL && tree[title_parent].ob_head != NIL &&
                tree[tree[title_parent].ob_head].ob_type == G_TITLE) {
                candidate = tree[title_parent].ob_head;
            } else {
                candidate = (WORD) (bar + 1);
            }
        }
        if (candidate <= last_object && tree[candidate].ob_type == G_TITLE) {
            first_title = candidate;
            last_title = candidate;
            for (j = (WORD) (candidate + 1); j <= last_object; ++j) {
                if (tree[j].ob_type != G_TITLE) {
                    break;
                }
                last_title = j;
            }
        }
    }

    if (bar != NIL && first_title != NIL && last_title != NIL &&
        (title_parent == bar || title_parent == tree[bar].ob_head) &&
        tree[ROOT].ob_tail <= last_title) {
        WORD j;
        WORD first_popup = NIL;
        WORD previous_popup = NIL;

        tree[ROOT].ob_head = bar;
        if (title_parent != bar && title_parent != NIL) {
            tree[bar].ob_head = title_parent;
            tree[bar].ob_tail = title_parent;
            tree[title_parent].ob_next = bar;
            tree[title_parent].ob_head = first_title;
            tree[title_parent].ob_tail = last_title;
        } else {
            tree[bar].ob_head = first_title;
            tree[bar].ob_tail = last_title;
        }
        for (j = first_title; j <= last_title; ++j) {
            if (j < last_title) {
                tree[j].ob_next = (WORD) (j + 1);
            } else {
                tree[j].ob_next = (title_parent != bar && title_parent != NIL) ?
                    title_parent : bar;
            }
        }

        if ((WORD) (last_title + 1) <= last_object &&
            (tree[last_title + 1].ob_type == G_BOX ||
             tree[last_title + 1].ob_type == G_IBOX) &&
            tree[last_title + 1].ob_head != NIL &&
            (tree[tree[last_title + 1].ob_head].ob_type == G_BOX ||
             tree[tree[last_title + 1].ob_head].ob_type == G_IBOX)) {
            WORD popup_container = (WORD) (last_title + 1);
            WORD popup_child;

            for (popup_child = (WORD) (popup_container + 1);
                 popup_child <= last_object;
                 ++popup_child) {
                if (tree[popup_child].ob_type == G_BOX ||
                    tree[popup_child].ob_type == G_IBOX) {
                    if (popup_count < (WORD) (sizeof(popup_roots) /
                        sizeof(popup_roots[0]))) {
                        popup_roots[popup_count++] = popup_child;
                    }
                }
            }
        } else {
            for (j = (WORD) (last_title + 1); j <= last_object; ++j) {
                if (tree[j].ob_type == G_BOX || tree[j].ob_type == G_IBOX) {
                    if (popup_count < (WORD) (sizeof(popup_roots) /
                        sizeof(popup_roots[0]))) {
                        popup_roots[popup_count++] = j;
                    }
                }
            }
        }

        for (j = 0; j < popup_count; ++j) {
            WORD popup_root = popup_roots[j];
            WORD child_first = (WORD) (popup_root + 1);
            WORD child_last = (j + 1 < popup_count) ?
                (WORD) (popup_roots[j + 1] - 1) : last_object;
            WORD k;

            if (first_popup == NIL) {
                first_popup = popup_root;
            }
            if (previous_popup != NIL) {
                tree[previous_popup].ob_next = popup_root;
            }
            previous_popup = popup_root;

            if (child_first <= child_last) {
                tree[popup_root].ob_head = child_first;
                tree[popup_root].ob_tail = child_last;
                for (k = child_first; k <= child_last; ++k) {
                    if (k < child_last) {
                        tree[k].ob_next = (WORD) (k + 1);
                    } else {
                        tree[k].ob_next = popup_root;
                    }
                }
            } else {
                tree[popup_root].ob_head = NIL;
                tree[popup_root].ob_tail = NIL;
            }
        }

        if (first_popup != NIL) {
            tree[bar].ob_next = first_popup;
            tree[ROOT].ob_tail = popup_roots[popup_count - 1];
            tree[popup_roots[popup_count - 1]].ob_next = ROOT;
            _aes.menu_popup_root_direct = 1;
        } else {
            tree[bar].ob_next = ROOT;
            tree[ROOT].ob_tail = bar;
        }

        popup_parent = _aes_menu_popup_container(tree);
    }

    (void) graf_handle(&wchar, &hchar, &boxw, &boxh);
    menu_bar_height = boxh;
    menu_item_height = _aes_chrome_height();
    menu_separator_height = (WORD) _aes_max_word((WORD) 8,
        (WORD) (boxh - 4));
    if (bar != NIL && tree[bar].ob_height > 0 && tree[bar].ob_height <= 4) {
        looks_char_sized = 1;
    }
    if (tree[ROOT].ob_width > 0 && tree[ROOT].ob_width <= 160 &&
        tree[ROOT].ob_height <= 8) {
        looks_char_sized = 1;
    }
    if (title_parent != NIL && tree[title_parent].ob_height > 0 &&
        tree[title_parent].ob_height <= 4) {
        looks_char_sized = 1;
    }

    if (looks_char_sized) {
        for (i = 1; i <= last_object; ++i) {
            tree[i].ob_x = (WORD) (tree[i].ob_x * wchar);
            tree[i].ob_y = (WORD) (tree[i].ob_y * boxh);
            tree[i].ob_width = (WORD) _aes_max_word(wchar,
                (WORD) (tree[i].ob_width * wchar));
            tree[i].ob_height = (WORD) _aes_max_word(boxh,
                (WORD) (tree[i].ob_height * boxh));
        }
    }

    if (first_title != NIL && last_title != NIL) {
        WORD title = first_title;
        WORD title_x = tree[first_title].ob_x;

        while (title != NIL) {
            const char *text = (const char *) (intptr_t)
                _aes_resolve_spec(&tree[title]);
            WORD title_width = tree[title].ob_width;

            if (text != NULL && *text != '\0') {
                WORD rendered_width = (WORD) _vdi_string_width(text);

                if (rendered_width > 0) {
                    title_width = (WORD) (rendered_width + 4);
                }
            }

            tree[title].ob_x = title_x;
            tree[title].ob_width = title_width;
            title_x = (WORD) (title_x + title_width);

            if (title == last_title) {
                break;
            }
            title = tree[title].ob_next;
            if (title == bar || title == ROOT || title == NIL) {
                break;
            }
        }
    }

    if (bar != NIL) {
        tree[bar].ob_x = 0;
        tree[bar].ob_y = 0;
        tree[bar].ob_width = (WORD) (_aes.work_out[0] + 1);
        tree[bar].ob_height = (WORD) _aes_max_word(tree[bar].ob_height,
            menu_bar_height);
    }
    if (title_parent != NIL && title_parent != bar) {
        tree[title_parent].ob_y = 0;
        tree[title_parent].ob_height = (WORD) _aes_max_word(
            tree[title_parent].ob_height, menu_bar_height);
    }
    if (first_title != NIL && last_title != NIL) {
        WORD title = first_title;

        while (title != NIL) {
            tree[title].ob_height = menu_bar_height;
            if (title == last_title) {
                break;
            }
            title = tree[title].ob_next;
            if (title == bar || title == ROOT || title == NIL) {
                break;
            }
        }
    }
    if (popup_parent != NIL && tree[popup_parent].ob_height <= 0) {
        tree[popup_parent].ob_height = menu_item_height;
    }

    if (_aes.menu_popup_root_direct != 0) {
        WORD popup = _aes_menu_first_popup_child(tree, ROOT);
        WORD title = first_title;

        while (popup != NIL && title != NIL && title <= last_title) {
            WORD title_x = 0;
            WORD title_y = 0;

            _aes_object_extent(tree, title, &title_x, &title_y);
            tree[popup].ob_x = title_x;
            tree[popup].ob_y = (WORD) _aes_max_word((WORD) 0,
                (WORD) (tree[bar].ob_height - 1));

            if (popup == tree[ROOT].ob_tail || tree[popup].ob_next == ROOT ||
                tree[popup].ob_next == NIL) {
                break;
            }
            popup = tree[popup].ob_next;
            if (title == last_title) {
                title = NIL;
            } else {
                title = tree[title].ob_next;
            }
        }
    }

    if (_aes.menu_popup_root_direct != 0) {
        WORD popup = _aes_menu_first_popup_child(tree, ROOT);
        const WORD popup_inner_right_padding = 3;

        while (popup != NIL) {
            WORD child = tree[popup].ob_head;
            WORD popup_width = tree[popup].ob_width;
            WORD popup_height = 0;
            WORD row_y = 1;

            while (child != NIL) {
                LONG child_spec = _aes_resolve_spec(&tree[child]);
                const char *text = (const char *) (intptr_t) child_spec;
                WORD row_height = _aes_menu_is_separator_text(text) ?
                    menu_separator_height : menu_item_height;

                tree[child].ob_x = 1;
                tree[child].ob_y = row_y;
                tree[child].ob_height = row_height;
                row_y = (WORD) (row_y + row_height);
                if (row_y > popup_height) {
                    popup_height = row_y;
                }

                if (tree[child].ob_type == G_STRING && child_spec != 0) {
                    char shortcut_label[128];
                    char shortcut_text[64];
                    WORD rendered_width;
                    WORD needed_width;
                    int separator = _aes_menu_is_separator_text(text);
                    int has_shortcut = _aes_menu_split_shortcut(text,
                        shortcut_label, sizeof(shortcut_label),
                        shortcut_text, sizeof(shortcut_text));

                    rendered_width = 0;
                    needed_width = popup_width;
                    if (separator == 0) {
                        rendered_width = (WORD) _vdi_string_width(
                            has_shortcut != 0 ? shortcut_label : text);
                        needed_width = (WORD) (tree[child].ob_x +
                            rendered_width + 4 +
                            popup_inner_right_padding);

                        if (has_shortcut != 0 && shortcut_text[0] != '\0') {
                            needed_width = (WORD) (needed_width +
                                _vdi_string_width(shortcut_text) + 12);
                        }

                        if (rendered_width > 0) {
                            tree[child].ob_width = (WORD) (rendered_width + 2);
                        }
                    }
                    if (needed_width > popup_width) {
                        popup_width = needed_width;
                    }
                }

                if (child == tree[popup].ob_tail || tree[child].ob_next == popup ||
                    tree[child].ob_next == NIL) {
                    break;
                }
                child = tree[child].ob_next;
            }

            child = tree[popup].ob_head;
            while (child != NIL) {
                tree[child].ob_width = (WORD) _aes_max_word((WORD) 1,
                    (WORD) (popup_width - 3));
                if (child == tree[popup].ob_tail || tree[child].ob_next == popup ||
                    tree[child].ob_next == NIL) {
                    break;
                }
                child = tree[child].ob_next;
            }

            tree[popup].ob_width = popup_width;
            tree[popup].ob_height = (WORD) _aes_max_word((WORD) 2,
                (WORD) (popup_height + 1));

            if (popup == tree[ROOT].ob_tail || tree[popup].ob_next == ROOT ||
                tree[popup].ob_next == NIL) {
                break;
            }
            popup = tree[popup].ob_next;
        }
    }

    for (i = 1; i <= last_object; ++i) {
        WORD abs_x = 0;
        WORD abs_y = 0;
        WORD right;
        WORD bottom;

        _aes_object_extent(tree, i, &abs_x, &abs_y);
        right = (WORD) (abs_x + tree[i].ob_width);
        bottom = (WORD) (abs_y + tree[i].ob_height);
        max_right = _aes_max_word(max_right, right);
        max_bottom = _aes_max_word(max_bottom, bottom);
    }

    tree[ROOT].ob_x = 0;
    tree[ROOT].ob_y = 0;
    tree[ROOT].ob_width = (WORD) _aes_max_word((WORD) (_aes.work_out[0] + 1),
        max_right);
    tree[ROOT].ob_height = (WORD) _aes_max_word(tree[bar].ob_height, max_bottom);
}

static int _aes_menu_show_popup(OBJECT *tree, WORD title, WORD popup)
{
    if (tree == NULL || title == NIL || popup == NIL) {
        return 0;
    }

    _aes_menu_hide_popups(tree);
    tree[popup].ob_flags &= (UWORD) ~HIDETREE;
    _aes_menu_set_title_selected(tree, title, 1);
    _aes_menu_redraw_tree(tree);
    return 1;
}

static int _aes_menu_shortcut_matches(const char *shortcut,
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
            ch = (char) (ch - 'a' + 'A');
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

    ascii = (WORD) (evt->key & 0xffu);
    scancode = (WORD) ((evt->key >> 8) & 0xffu);

    if (strcmp(token, "ESC") == 0) {
        return ascii == 27 || scancode == 41;
    }

    if (token[0] == 'F' && token[1] >= '1' && token[1] <= '9') {
        index = token[1] - '0';
        if (token[2] >= '0' && token[2] <= '9') {
            index = index * 10 + (token[2] - '0');
        }
        return scancode == (WORD) (57 + index);
    }

    if (token[0] == '^' && token[1] != '\0' && token[2] == '\0') {
        WORD upper = (WORD) token[1];
        WORD lower = upper;

        if (lower >= 'A' && lower <= 'Z') {
            lower = (WORD) (lower - 'A' + 'a');
        }
        return ascii == (upper & 0x1f) ||
            ascii == upper || ascii == lower;
    }

    if (length == 1u) {
        WORD upper = (WORD) token[0];
        WORD lower = upper;

        if (lower >= 'A' && lower <= 'Z') {
            lower = (WORD) (lower - 'A' + 'a');
        }
        return ascii == upper || ascii == lower;
    }

    return 0;
}

WORD _aes_menu_key_event(OBJECT *tree,
                         const gem_hid_event_t *evt,
                         WORD mepbuff[8])
{
    WORD parent;
    WORD title;

    if (tree == NULL || evt == NULL || mepbuff == NULL ||
        evt->type != GEM_HID_KEY || (evt->flags & 1u) == 0u) {
        return 0;
    }

    parent = _aes_menu_title_container(tree);
    if (parent == NIL) {
        return 0;
    }

    memset(mepbuff, 0, sizeof(WORD) * 8u);
    title = tree[parent].ob_head;
    while (title != NIL) {
        WORD popup = _aes_menu_popup_for_title(tree, title);
        WORD item;

        if (popup != NIL) {
            item = tree[popup].ob_head;
            while (item != NIL) {
                LONG spec = _aes_resolve_spec(&tree[item]);
                const char *text = (const char *) (intptr_t) spec;
                char shortcut_text[64];
                char shortcut_label[128];
                WORD next = tree[item].ob_next;

                if (_aes_menu_item_selectable(tree, item) != 0 &&
                    _aes_menu_split_shortcut(text, shortcut_label,
                        sizeof(shortcut_label), shortcut_text,
                        sizeof(shortcut_text)) != 0 &&
                    _aes_menu_shortcut_matches(shortcut_text, evt) != 0) {
                    mepbuff[0] = MN_SELECTED;
                    mepbuff[1] = _aes.current_app_id;
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

WORD _aes_menu_event(OBJECT *tree,
                     const gem_hid_event_t *first_evt,
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

    title = _aes_menu_hit_title(tree, (WORD) first_evt->x, (WORD) first_evt->y);
    if (title == NIL) {
        return 0;
    }

    popup = _aes_menu_popup_for_title(tree, title);
    if (popup == NIL) {
        return 0;
    }

    if (!_aes_menu_show_popup(tree, title, popup)) {
        return 0;
    }
    sticky = (_aes.menu_click != 0);
    latched = 0;

    FOREVER {
        gem_hid_event_t evt;

        if (gem_hid_poll(&evt) == 0) {
            gem_os_sleep_ms(1u);
            continue;
        }

        if (evt.type == GEM_HID_MOUSE_MOVE ||
            evt.type == GEM_HID_MOUSE_BUTTON) {
            _aes_store_mouse_state(&evt);
        }

        if (evt.type == GEM_HID_MOUSE_BUTTON &&
            (evt.flags & GEM_HID_BUTTON_LEFT) != 0u) {
            WORD press_title = _aes_menu_hit_title(tree,
                (WORD) evt.x, (WORD) evt.y);

            if (press_title != NIL && press_title != title) {
                WORD next_popup = _aes_menu_popup_for_title(tree, press_title);

                if (next_popup != NIL) {
                    _aes_menu_set_title_selected(tree, title, 0);
                    title = press_title;
                    popup = next_popup;
                    item = NIL;
                    latched = 0;
                    (void) _aes_menu_show_popup(tree, title, popup);
                }
            }
            continue;
        }

        if (evt.type == GEM_HID_MOUSE_MOVE) {
            WORD hover_title = _aes_menu_hit_title(tree,
                (WORD) evt.x, (WORD) evt.y);

            if (hover_title != NIL && hover_title != title) {
                WORD next_popup = _aes_menu_popup_for_title(tree,
                    hover_title);

                if (next_popup != NIL) {
                    _aes_menu_set_title_selected(tree, title, 0);
                    title = hover_title;
                    popup = next_popup;
                    item = NIL;
                    (void) _aes_menu_show_popup(tree, title, popup);
                    continue;
                }
            }

            {
                WORD hover = objc_find(tree, popup, MAX_DEPTH,
                    (WORD) evt.x, (WORD) evt.y);

                if (_aes_menu_item_selectable(tree, hover)) {
                    item = hover;
                } else if (objc_find(tree, title, 0, (WORD) evt.x,
                           (WORD) evt.y) == title) {
                    item = NIL;
                }
            }

            if (item != highlighted_item) {
                if (highlighted_item != NIL) {
                    _aes_menu_set_item_selected(tree, highlighted_item, 0);
                }
                if (item != NIL) {
                    _aes_menu_set_item_selected(tree, item, 1);
                }
                highlighted_item = item;
                _aes_menu_redraw_tree(tree);
            }
            continue;
        }

        if (evt.type == GEM_HID_MOUSE_BUTTON &&
            (evt.flags & GEM_HID_BUTTON_LEFT) == 0u) {
            WORD release_title = _aes_menu_hit_title(tree,
                (WORD) evt.x, (WORD) evt.y);
            WORD release_hit = objc_find(tree, popup, MAX_DEPTH,
                (WORD) evt.x, (WORD) evt.y);

            if (_aes_menu_item_selectable(tree, release_hit)) {
                item = release_hit;
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
                _aes_menu_set_item_selected(tree, highlighted_item, 0);
                highlighted_item = NIL;
            }
            _aes_menu_hide_popups(tree);
            _aes_menu_clear_title_selection(tree);
            _aes_menu_redraw_tree(tree);

            if (item == NIL) {
                return 0;
            }

            mepbuff[0] = MN_SELECTED;
            mepbuff[1] = _aes.current_app_id;
            mepbuff[3] = title;
            mepbuff[4] = item;
            return 1;
        }
    }
}

int _aes_menu_subtree_rect(OBJECT *tree, WORD object, GRECT *rect)
{
    WORD last_object;
    WORD i;
    WORD min_x = 0;
    WORD min_y = 0;
    WORD max_x = 0;
    WORD max_y = 0;
    int found = 0;

    if (tree == NULL || object == NIL || rect == NULL) {
        return 0;
    }

    last_object = _aes_menu_last_object(tree);
    for (i = object; i <= last_object; ++i) {
        WORD abs_x;
        WORD abs_y;
        WORD right;
        WORD bottom;

        if (i != object && _aes_find_parent(tree, i) != object) {
            continue;
        }

        _aes_object_extent(tree, i, &abs_x, &abs_y);
        right = (WORD) (abs_x + tree[i].ob_width - 1);
        bottom = (WORD) (abs_y + tree[i].ob_height - 1);

        if (!found) {
            min_x = abs_x;
            min_y = abs_y;
            max_x = right;
            max_y = bottom;
            found = 1;
        } else {
            min_x = _aes_min_word(min_x, abs_x);
            min_y = _aes_min_word(min_y, abs_y);
            max_x = _aes_max_word(max_x, right);
            max_y = _aes_max_word(max_y, bottom);
        }
    }

    if (!found) {
        return 0;
    }

    _aes_set_rect(rect, min_x, min_y,
        (WORD) (max_x - min_x + 1), (WORD) (max_y - min_y + 1));
    return 1;
}

static void _aes_menu_expand_saved_rect(OBJECT *tree, WORD object, GRECT *rect)
{
    if (tree == NULL || rect == NULL || rect->g_w <= 0 || rect->g_h <= 0) {
        return;
    }

    if (object == tree[ROOT].ob_head) {
        rect->g_h = (WORD) (rect->g_h + 1);
    }
}

static int _aes_menu_item_selectable(OBJECT *tree, WORD item)
{
    LONG spec;
    const char *text;

    if (tree == NULL || item == NIL || tree[item].ob_type != G_STRING ||
        (tree[item].ob_state & DISABLED) != 0u) {
        return 0;
    }

    spec = _aes_resolve_spec(&tree[item]);
    text = (const char *) (intptr_t) spec;
    return !_aes_menu_is_separator_text(text);
}

static void _aes_menu_free_saved_pixels(void)
{
    WORD i;

    for (i = 0; i < AES_MAX_MENU_SAVED_REGIONS; ++i) {
        if (_aes.menu_saved_pixels[i] != NULL) {
            gem_os_free(_aes.menu_saved_pixels[i]);
            _aes.menu_saved_pixels[i] = NULL;
        }
        _aes_set_rect(&_aes.menu_saved_rects[i], 0, 0, 0, 0);
    }
    _aes.menu_saved_count = 0;
}

static void _aes_menu_restore_saved_region(void)
{
    WORD i;

    if (_aes.menu_saved_count <= 0) {
        _aes_menu_free_saved_pixels();
        return;
    }

    for (i = 0; i < _aes.menu_saved_count; ++i) {
        WORD x;
        WORD y;
        size_t index = 0;
        GRECT rect = _aes.menu_saved_rects[i];
        uint8_t *pixels = _aes.menu_saved_pixels[i];

        if (pixels == NULL || rect.g_w <= 0 || rect.g_h <= 0) {
            continue;
        }

        for (y = 0; y < rect.g_h; ++y) {
            for (x = 0; x < rect.g_w; ++x) {
                _vdi_set_screen_pixel((WORD) (rect.g_x + x),
                    (WORD) (rect.g_y + y), (WORD) pixels[index++]);
            }
        }
    }

    _aes_menu_free_saved_pixels();
}

static int _aes_menu_save_region(const GRECT *rect)
{
    WORD x;
    WORD y;
    size_t count;
    size_t index = 0;
    uint8_t *pixels;

    if (rect == NULL || rect->g_w <= 0 || rect->g_h <= 0 ||
        _aes.menu_saved_count >= AES_MAX_MENU_SAVED_REGIONS) {
        return 0;
    }

    count = (size_t) rect->g_w * (size_t) rect->g_h;
    pixels = (uint8_t *) gem_os_alloc(count);
    if (pixels == NULL) {
        return 0;
    }

    for (y = 0; y < rect->g_h; ++y) {
        for (x = 0; x < rect->g_w; ++x) {
            pixels[index++] = (uint8_t) _vdi_get_screen_pixel(
                (WORD) (rect->g_x + x), (WORD) (rect->g_y + y));
        }
    }

    _aes.menu_saved_pixels[_aes.menu_saved_count] = pixels;
    _aes.menu_saved_rects[_aes.menu_saved_count] = *rect;
    ++_aes.menu_saved_count;
    return 1;
}
