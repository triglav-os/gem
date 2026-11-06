/*
 * Normalizes an application menu tree for the hosted AES: relinks the bar,
 * titles and popup boxes into the classic shape, then lays out title and
 * item geometry in the current font so tracking and drawing share one
 * measured model.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (C) 2026 tomaz stih
 */

#include "menu_private.h"
#include "system_menu.h"

#include "../vdi/vdi_internal.h"

#include <string.h>

void aes_menu_prepare_tree(OBJECT *tree)
{
    WORD last_object;
    WORD wchar = AES_CHAR_WIDTH;
    WORD hchar = AES_CHAR_HEIGHT;
    WORD boxw = AES_DECOR;
    WORD boxh = aes_menu_chrome_height();
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

    if (tree == NULL || aes_ensure_vdi() == 0) {
        return;
    }

    aes_state.menu_popup_root_direct = 0;
    last_object = aes_menu_last_object(tree);
    bar = tree[ROOT].ob_head;
    title_parent = aes_menu_title_container(tree);
    popup_parent = aes_menu_popup_container(tree);

    if (bar != NIL) {
        WORD candidate = tree[bar].ob_head;
        WORD j;

        if (candidate == NIL || tree[candidate].ob_type != G_TITLE) {
            if (title_parent != NIL && tree[title_parent].ob_head != NIL &&
                tree[tree[title_parent].ob_head].ob_type == G_TITLE) {
                candidate = tree[title_parent].ob_head;
            } else {
                candidate = (WORD)(bar + 1);
            }
        }
        if (candidate <= last_object && tree[candidate].ob_type == G_TITLE) {
            first_title = candidate;
            last_title = candidate;
            for (j = (WORD)(candidate + 1); j <= last_object; ++j) {
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
                tree[j].ob_next = (WORD)(j + 1);
            } else {
                tree[j].ob_next = (title_parent != bar && title_parent != NIL)
                                      ? title_parent
                                      : bar;
            }
        }

        if ((WORD)(last_title + 1) <= last_object &&
            (tree[last_title + 1].ob_type == G_BOX ||
             tree[last_title + 1].ob_type == G_IBOX) &&
            tree[last_title + 1].ob_head != NIL &&
            (tree[tree[last_title + 1].ob_head].ob_type == G_BOX ||
             tree[tree[last_title + 1].ob_head].ob_type == G_IBOX)) {
            WORD popup_container = (WORD)(last_title + 1);
            WORD popup_child;

            for (popup_child = (WORD)(popup_container + 1);
                 popup_child <= last_object; ++popup_child) {
                if (tree[popup_child].ob_type == G_BOX ||
                    tree[popup_child].ob_type == G_IBOX) {
                    if (popup_count <
                        (WORD)(sizeof(popup_roots) / sizeof(popup_roots[0]))) {
                        popup_roots[popup_count++] = popup_child;
                    }
                }
            }
        } else {
            for (j = (WORD)(last_title + 1); j <= last_object; ++j) {
                if (tree[j].ob_type == G_BOX || tree[j].ob_type == G_IBOX) {
                    if (popup_count <
                        (WORD)(sizeof(popup_roots) / sizeof(popup_roots[0]))) {
                        popup_roots[popup_count++] = j;
                    }
                }
            }
        }

        for (j = 0; j < popup_count; ++j) {
            WORD popup_root = popup_roots[j];
            WORD child_first = (WORD)(popup_root + 1);
            WORD child_last = (j + 1 < popup_count)
                                  ? (WORD)(popup_roots[j + 1] - 1)
                                  : last_object;
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
                        tree[k].ob_next = (WORD)(k + 1);
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
            aes_state.menu_popup_root_direct = 1;
        } else {
            tree[bar].ob_next = ROOT;
            tree[ROOT].ob_tail = bar;
        }

        popup_parent = aes_menu_popup_container(tree);
    }

    (void)graf_handle(&wchar, &hchar, &boxw, &boxh);
    menu_bar_height = boxh;
    menu_item_height = aes_chrome_height();
    menu_separator_height = (WORD)aes_max_word((WORD)8, (WORD)(boxh - 4));
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
            tree[i].ob_x = (WORD)(tree[i].ob_x * wchar);
            tree[i].ob_y = (WORD)(tree[i].ob_y * boxh);
            tree[i].ob_width =
                (WORD)aes_max_word(wchar, (WORD)(tree[i].ob_width * wchar));
            tree[i].ob_height =
                (WORD)aes_max_word(boxh, (WORD)(tree[i].ob_height * boxh));
        }
    }

    if (first_title != NIL && last_title != NIL) {
        WORD title = first_title;
        /* Leave room for the always-present system title at the left. */
        WORD title_x = aes_system_menu_width();

        while (title != NIL) {
            const char *text =
                (const char *)(intptr_t)aes_resolve_spec(&tree[title]);
            WORD title_width = tree[title].ob_width;

            if (text != NULL && *text != '\0') {
                WORD rendered_width = (WORD)vdi_string_width(text);

                if (rendered_width > 0) {
                    title_width = (WORD)(rendered_width + 4);
                }
            }

            tree[title].ob_x = title_x;
            tree[title].ob_width = title_width;
            title_x = (WORD)(title_x + title_width);

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
        tree[bar].ob_width = (WORD)(aes_state.work_out[0] + 1);
        tree[bar].ob_height =
            (WORD)aes_max_word(tree[bar].ob_height, menu_bar_height);
    }
    if (title_parent != NIL && title_parent != bar) {
        tree[title_parent].ob_y = 0;
        tree[title_parent].ob_height =
            (WORD)aes_max_word(tree[title_parent].ob_height, menu_bar_height);
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

    if (popup_parent != NIL) {
        WORD popup = aes_menu_first_popup_child(tree, popup_parent);
        WORD title = first_title;

        while (popup != NIL && title != NIL && title <= last_title) {
            WORD title_x = 0;
            WORD title_y = 0;
            WORD next = tree[popup].ob_next;

            aes_object_extent(tree, title, &title_x, &title_y);
            tree[popup].ob_x = title_x;
            tree[popup].ob_y =
                (WORD)aes_max_word((WORD)0, (WORD)(tree[bar].ob_height - 1));

            if (popup == tree[popup_parent].ob_tail || next == popup_parent ||
                next == ROOT || next == NIL) {
                break;
            }
            popup = next;
            if (title == last_title) {
                title = NIL;
            } else {
                title = tree[title].ob_next;
            }
        }
    }

    if (popup_parent != NIL) {
        WORD popup = aes_menu_first_popup_child(tree, popup_parent);

        while (popup != NIL) {
            WORD child = tree[popup].ob_head;
            WORD popup_width = 2;
            WORD popup_height = 0;
            WORD row_y = 1;
            WORD next = tree[popup].ob_next;

            while (child != NIL) {
                /* Hidden dynamic entries stay linked, but consume no rows. */
                if ((tree[child].ob_flags & HIDETREE) != 0u) {
                    if (child == tree[popup].ob_tail ||
                        tree[child].ob_next == popup ||
                        tree[child].ob_next == NIL) {
                        break;
                    }
                    child = tree[child].ob_next;
                    continue;
                }
                LONG child_spec = aes_resolve_spec(&tree[child]);
                const char *text = (const char *)(intptr_t)child_spec;
                WORD row_height = aes_menu_is_separator_text(text)
                                      ? menu_separator_height
                                      : menu_item_height;

                tree[child].ob_x = 1;
                tree[child].ob_y = row_y;
                tree[child].ob_height = row_height;
                row_y = (WORD)(row_y + row_height);
                if (row_y > popup_height) {
                    popup_height = row_y;
                }

                if (tree[child].ob_type == G_STRING && child_spec != 0) {
                    char shortcut_label[128];
                    char shortcut_text[64];
                    WORD rendered_width;
                    WORD needed_width;
                    int separator = aes_menu_is_separator_text(text);
                    int has_shortcut = aes_menu_split_shortcut(
                        text, shortcut_label, sizeof(shortcut_label),
                        shortcut_text, sizeof(shortcut_text));

                    needed_width = popup_width;
                    if (separator == 0) {
                        rendered_width =
                            (WORD)vdi_string_width(shortcut_label);
                        needed_width = (WORD)(rendered_width +
                                                   2 * AES_MENU_ITEM_PADDING +
                                               2);

                        if (has_shortcut != 0 && shortcut_text[0] != '\0') {
                            needed_width =
                                (WORD)(needed_width +
                                       vdi_string_width(shortcut_text) +
                                       AES_MENU_SHORTCUT_GAP);
                        }

                        if (rendered_width > 0) {
                            tree[child].ob_width = (WORD)(rendered_width + 2);
                        }
                    }
                    if (needed_width > popup_width) {
                        popup_width = needed_width;
                    }
                }

                if (child == tree[popup].ob_tail ||
                    tree[child].ob_next == popup ||
                    tree[child].ob_next == NIL) {
                    break;
                }
                child = tree[child].ob_next;
            }

            child = tree[popup].ob_head;
            while (child != NIL) {
                tree[child].ob_width =
                    (WORD)aes_max_word((WORD)1, (WORD)(popup_width - 2));
                if (child == tree[popup].ob_tail ||
                    tree[child].ob_next == popup ||
                    tree[child].ob_next == NIL) {
                    break;
                }
                child = tree[child].ob_next;
            }

            tree[popup].ob_width = popup_width;
            tree[popup].ob_height =
                (WORD)aes_max_word((WORD)2, (WORD)(popup_height + 1));

            if (popup == tree[popup_parent].ob_tail || next == popup_parent ||
                next == ROOT || next == NIL) {
                break;
            }
            popup = next;
        }
    }

    for (i = 1; i <= last_object; ++i) {
        WORD abs_x = 0;
        WORD abs_y = 0;
        WORD right;
        WORD bottom;

        aes_object_extent(tree, i, &abs_x, &abs_y);
        right = (WORD)(abs_x + tree[i].ob_width);
        bottom = (WORD)(abs_y + tree[i].ob_height);
        max_right = aes_max_word(max_right, right);
        max_bottom = aes_max_word(max_bottom, bottom);
    }

    tree[ROOT].ob_x = 0;
    tree[ROOT].ob_y = 0;
    tree[ROOT].ob_width =
        (WORD)aes_max_word((WORD)(aes_state.work_out[0] + 1), max_right);
    tree[ROOT].ob_height = (WORD)aes_max_word(tree[bar].ob_height, max_bottom);
}
