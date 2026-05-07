/*
 * Implements the private hosted AES helper layer, including runtime
 * state management, menu tracking, object drawing, hit testing, and
 * resource-file utility routines.
 *
 * MIT License (see: LICENSE)
 * Copyright (C) 2026 tomaz stih
 */

#include "_aes.h"

#include "../vdi/_vdi.h"

#include "platform/os.h"

#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

aes_state_t _aes;
static int _aes_menu_redraw_in_progress;

static WORD _aes_find_parent(OBJECT *tree, WORD object);
static WORD _aes_window_left_border(const aes_window_t *window);
static WORD _aes_window_right_border(const aes_window_t *window);
static WORD _aes_window_bottom_border(const aes_window_t *window);
static WORD _aes_window_title_height(const aes_window_t *window);
static WORD _aes_light_color(void);
static WORD _aes_dark_color(void);
static int _aes_is_dialog_root_tree(const OBJECT *tree);
static int _aes_is_menu_bar_object(const OBJECT *tree, WORD object);
static int _aes_is_dialog_frame_object(const OBJECT *tree, WORD object,
    WORD parent);
static int _aes_window_title_rect(const aes_window_t *window, GRECT *rect);
static int _aes_window_closer_rect(const aes_window_t *window, GRECT *rect);
static int _aes_window_fuller_rect(const aes_window_t *window, GRECT *rect);
static int _aes_window_sizer_rect(const aes_window_t *window, GRECT *rect);
int _aes_window_vtrack_rect(const aes_window_t *window, GRECT *rect);
int _aes_window_htrack_rect(const aes_window_t *window, GRECT *rect);
static int _aes_window_vup_rect(const aes_window_t *window, GRECT *rect);
static int _aes_window_vdown_rect(const aes_window_t *window, GRECT *rect);
static int _aes_window_hleft_rect(const aes_window_t *window, GRECT *rect);
static int _aes_window_hright_rect(const aes_window_t *window, GRECT *rect);
int _aes_window_vthumb_rect(const aes_window_t *window, GRECT *rect);
int _aes_window_hthumb_rect(const aes_window_t *window, GRECT *rect);
static LONG _aes_resolve_spec(const OBJECT *obj);
static void _aes_draw_text(WORD x, WORD y, WORD color, const char *text);
static void _aes_fill_rect(WORD x0, WORD y0, WORD x1, WORD y1, WORD color);
static void _aes_invert_rect(WORD x0, WORD y0, WORD x1, WORD y1);
static void _aes_fill_pattern_rect(WORD x0, WORD y0, WORD x1, WORD y1,
    const uint8_t *rows, size_t row_count);
static void _aes_fill_checker_rect(WORD x0, WORD y0, WORD x1, WORD y1);
static void _aes_draw_hline(WORD x0, WORD x1, WORD y, WORD color);
static void _aes_draw_vline(WORD x, WORD y0, WORD y1, WORD color);
static void _aes_draw_dialog_frame(const WORD rect[4]);
static void _aes_draw_button_frame(const WORD rect[4], WORD dark_color,
    WORD light_color, int default_button);
static void _aes_draw_window_icon(WORD x0, WORD y0, WORD x1, WORD y1,
    char glyph);
static void _aes_draw_window_icon_offset(WORD x0, WORD y0, WORD x1, WORD y1,
    char glyph, WORD dx, WORD dy);
static void _aes_draw_sizer_glyph(const aes_window_t *window);
static void _aes_draw_window_title(const aes_window_t *window,
    const WORD outer_box[4], WORD title_height);
static void _aes_queue_window_redraw(const aes_window_t *window,
    const GRECT *dirty);
static void _aes_present_after_window_draw(void);
/* Declared in _aes.h: _aes_rects_intersect, _aes_intersect_rects,
   _aes_subtract_rect */
static void _aes_expand_window_damage_rect(const GRECT *src, GRECT *out);
void _aes_redraw_region(const GRECT *dirty);
static void _aes_desktop_rect_local(GRECT *rect);
static int _aes_menu_subtree_rect(OBJECT *tree, WORD object, GRECT *rect);
static void _aes_menu_expand_saved_rect(OBJECT *tree, WORD object, GRECT *rect);
static int _aes_menu_is_separator_text(const char *text);
static int _aes_menu_split_shortcut(const char *text,
    char *label,
    size_t label_size,
    char *shortcut,
    size_t shortcut_size);
static int _aes_menu_item_selectable(OBJECT *tree, WORD item);
static void _aes_menu_free_saved_pixels(void);
static void _aes_menu_restore_saved_region(void);
static int _aes_menu_save_region(const GRECT *rect);
/* Declared in _aes.h: _aes_window_is_top */

/*
 * Provides a default "resource not found" answer when the desktop does
 * not link a built-in resource provider.
 */
__attribute__((weak)) int gem_builtin_rsrc_load(const char *filename)
{
    (void) filename;
    return 0;
}

/*
 * Provides a default "resource address unavailable" answer when no
 * built-in resource provider is linked.
 */
__attribute__((weak)) int gem_builtin_rsrc_gaddr(WORD type,
                                                 WORD index,
                                                 void **addr)
{
    (void) type;
    (void) index;
    (void) addr;
    return 0;
}

/*
 * Provides a default no-op built-in resource cleanup hook.
 */
__attribute__((weak)) void gem_builtin_rsrc_free(void)
{
}

static void _aes_write_trace(const char *env_var, const char *logfile,
                             const char *fmt, va_list ap)
{
    const char *trace = getenv(env_var);
    FILE *fp;

    if (trace == NULL || trace[0] == '\0') {
        return;
    }
    fp = fopen(logfile, "a");
    if (fp == NULL) {
        return;
    }
    vfprintf(fp, fmt, ap);
    fputc('\n', fp);
    fclose(fp);
}

void _aes_trace(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    _aes_write_trace("GEM_TRACE_AES", "/tmp/gem_aes_trace.log", fmt, ap);
    va_end(ap);
}

static void _aes_draw_trace(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    _aes_write_trace("GEM_TRACE_DRAW", "/tmp/gem_draw_trace.log", fmt, ap);
    va_end(ap);
}

/*
 * Copies hosted mouse coordinates and button state into the shared VDI
 * cursor state.
 */
void _aes_store_mouse_state(const gem_hid_event_t *evt)
{
    if (evt == NULL) {
        return;
    }

    _vdi_set_mouse_state((WORD) evt->x, (WORD) evt->y, (WORD) evt->flags);
}

WORD _aes_chrome_height(void)
{
    WORD text_height;

    text_height = _aes.vdi_ready ? _vdi_font_text_height() : AES_CHAR_HEIGHT;
    return (WORD) (text_height + 4);
}

WORD _aes_menu_chrome_height(void)
{
    WORD text_height;

    text_height = _aes.vdi_ready ? _vdi_font_text_height() : AES_CHAR_HEIGHT;
    return (WORD) (text_height + 6);
}

WORD _aes_menu_bar_height(void)
{
    WORD box_height = 0;

    if (_aes.menu_visible == 0 || _aes.menu_tree == NULL) {
        return 0;
    }

    (void) graf_handle(NULL, NULL, NULL, &box_height);
    if (box_height <= 0) {
        box_height = _aes_menu_chrome_height();
    }
    return box_height;
}

/*
 * Returns the menu popup-container object under the current menu tree.
 */
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

/*
 * Returns the first popup-root object in the current menu layout.
 */
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

/*
 * Returns the highest object index in one menu tree.
 */
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

/*
 * Returns the menu-title container object under the current menu tree.
 */
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

/*
 * Returns the `index`th child object under `parent`.
 */
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

/*
 * Returns the zero-based title index for a menu-title object.
 */
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

/*
 * Maps a menu title to its matching popup root object.
 */
static WORD _aes_menu_popup_for_title(OBJECT *tree, WORD title)
{
    WORD popup_parent = _aes_menu_popup_container(tree);
    WORD title_index = _aes_menu_index_for_title(tree, title);

    if (title_index < 0 || popup_parent == NIL) {
        return NIL;
    }

    return _aes_menu_nth_child(tree, popup_parent, title_index);
}

/*
 * Sets or clears the selected state on one popup item.
 */
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

/*
 * Marks every popup subtree in the current menu tree as hidden.
 */
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

/*
 * Returns the title object currently under the mouse, if any.
 */
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

/*
 * Sets or clears the selected state on one menu title.
 */
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

/*
 * Clears the selected state from every title in the current menu bar.
 */
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

/*
 * Redraws the full menu tree after a title or popup state change.
 */
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

/*
 * Normalizes one hosted menu tree enough for classic samples to draw.
 */
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
                    int has_shortcut = _aes_menu_split_shortcut(text,
                        shortcut_label, sizeof(shortcut_label),
                        shortcut_text, sizeof(shortcut_text));

                    rendered_width = (WORD) _vdi_string_width(
                        has_shortcut != 0 ? shortcut_label : text);
                    needed_width = (WORD) (tree[child].ob_x +
                        rendered_width + 4);

                    if (has_shortcut != 0 && shortcut_text[0] != '\0') {
                        needed_width = (WORD) (needed_width +
                            _vdi_string_width(shortcut_text) + 12);
                    }

                    if (rendered_width > 0) {
                        tree[child].ob_width = (WORD) (rendered_width + 2);
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
                    (WORD) (popup_width - 2));
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

/*
 * Shows one popup and marks its title selected.
 */
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

/*
 * Tracks one hosted menu interaction until it either produces an
 * `MN_SELECTED` message or gets cancelled.
 */
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

/*
 * Returns the smaller of two `WORD` values.
 */
WORD _aes_min_word(WORD left, WORD right)
{
    return (left < right) ? left : right;
}

/*
 * Returns the larger of two `WORD` values.
 */
WORD _aes_max_word(WORD left, WORD right)
{
    return (left > right) ? left : right;
}

/*
 * Writes one `GRECT` in GEM coordinate form.
 */
void _aes_set_rect(GRECT *rect, WORD x, WORD y, WORD w, WORD h)
{
    if (rect == NULL) {
        return;
    }

    rect->g_x = x;
    rect->g_y = y;
    rect->g_w = w;
    rect->g_h = h;
}

/*
 * Returns non-zero when a point lies within a rectangle.
 */
int _aes_point_in_rect(WORD x, WORD y, const GRECT *rect)
{
    if (rect == NULL) {
        return 0;
    }

    return x >= rect->g_x && y >= rect->g_y &&
        x < rect->g_x + rect->g_w &&
        y < rect->g_y + rect->g_h;
}

/*
 * Resets the hosted AES runtime state to its initial defaults.
 */
void _aes_reset_state(void)
{
    memset(&_aes, 0, sizeof(_aes));
    _aes.next_app_id = 1;
    _aes.next_window_z = 1u;
    _aes.dclick_rate = 3;
    _aes.menu_click = 1;
    global[3] = 0x1100;
    global[4] = 0;
}

/*
 * Lazily opens the single hosted VDI workstation used by AES.
 */
int _aes_ensure_vdi(void)
{
    if (_aes.vdi_ready != 0 && _aes.vdi_handle != 0) {
        _aes_trace("ensure_vdi already handle=%d", _aes.vdi_handle);
        return 1;
    }

    _aes_trace("ensure_vdi opening");
    memset(_aes.work_in, 0, sizeof(_aes.work_in));
    memset(_aes.work_out, 0, sizeof(_aes.work_out));
    v_opnvwk(_aes.work_in, &_aes.vdi_handle, _aes.work_out);
    if (_aes.vdi_handle == 0) {
        _aes_trace("ensure_vdi failed");
        return 0;
    }

    /*
     * AES applications expect the default arrow cursor to be visible
     * once the hosted workstation exists. Demos that only use basic
     * window messages never call graf_mouse(M_ON) themselves.
     */
    v_show_c(_aes.vdi_handle, 1);
    _aes.vdi_ready = 1;
    _aes_trace("ensure_vdi opened handle=%d size=%dx%d",
        _aes.vdi_handle, _aes.work_out[0] + 1, _aes.work_out[1] + 1);
    return 1;
}

/*
 * Returns the registered app descriptor for an AES application id.
 */
aes_app_t *_aes_find_app_by_id(WORD id)
{
    size_t i;

    for (i = 0; i < AES_MAX_APPS; ++i) {
        if (_aes.apps[i].used != 0 && _aes.apps[i].id == id) {
            return &_aes.apps[i];
        }
    }
    return NULL;
}

/*
 * Returns the hosted window descriptor for an AES window handle.
 */
aes_window_t *_aes_find_window(WORD handle)
{
    size_t i;

    for (i = 0; i < AES_MAX_WINDOWS; ++i) {
        if (_aes.windows[i].used != 0 && _aes.windows[i].handle == handle) {
            return &_aes.windows[i];
        }
    }
    return NULL;
}

static WORD _aes_window_left_border(const aes_window_t *window)
{
    return (window != NULL && window->kind != 0u) ? 1 : 0;
}

static int _aes_window_has_vscroll(const aes_window_t *window)
{
    return window != NULL &&
        (window->kind & (UPARROW | DNARROW | VSLIDE)) != 0u;
}

static int _aes_window_has_hscroll(const aes_window_t *window)
{
    return window != NULL &&
        (window->kind & (LFARROW | RTARROW | HSLIDE)) != 0u;
}

static WORD _aes_window_scroll_span(const aes_window_t *window)
{
    WORD span;

    if (window == NULL) {
        return 0;
    }

    span = (WORD) (_aes_window_title_height(window) - 1);
    return _aes_max_word(span, 12);
}

static WORD _aes_window_scroll_button_side(const aes_window_t *window)
{
    WORD side;

    if (window == NULL) {
        return 0;
    }

    side = (WORD) (_aes_window_title_height(window) - 2);
    return _aes_max_word(side, 10);
}

static WORD _aes_window_right_border(const aes_window_t *window)
{
    if (window != NULL &&
        ((window->kind & SIZER) != 0u || _aes_window_has_vscroll(window) != 0)) {
        return _aes_window_scroll_span(window);
    }
    return _aes_window_left_border(window);
}

static WORD _aes_window_bottom_border(const aes_window_t *window)
{
    if (window != NULL &&
        ((window->kind & SIZER) != 0u || _aes_window_has_hscroll(window) != 0)) {
        return _aes_window_scroll_span(window);
    }
    return (window != NULL && window->kind != 0u) ? 2 : 0;
}

static WORD _aes_window_title_height(const aes_window_t *window)
{
    if (window == NULL || window->kind == 0u) {
        return 0;
    }
    if ((window->kind & (NAME | CLOSER | FULLER | MOVER)) == 0u) {
        return 2;
    }
    return _aes_menu_chrome_height();
}

static int _aes_is_dialog_root_tree(const OBJECT *tree)
{
    if (tree == NULL || tree == _aes.menu_tree) {
        return 0;
    }

    if (tree[ROOT].ob_head == NIL) {
        return 0;
    }

    return (tree[ROOT].ob_type == G_IBOX || tree[ROOT].ob_type == G_BOX) ?
        1 : 0;
}

static int _aes_is_menu_bar_object(const OBJECT *tree, WORD object)
{
    WORD bar;

    if (tree == NULL || tree != _aes.menu_tree || object < 0) {
        return 0;
    }

    bar = tree[ROOT].ob_head;
    return (bar != NIL && object == bar) ? 1 : 0;
}

static int _aes_is_dialog_frame_object(const OBJECT *tree, WORD object,
                                       WORD parent)
{
    if (tree == NULL || object <= ROOT || parent != ROOT) {
        return 0;
    }

    if (_aes_is_dialog_root_tree(tree) == 0) {
        return 0;
    }

    return (tree[object].ob_type == G_BOX || tree[object].ob_type == G_IBOX) ?
        1 : 0;
}

static int _aes_window_closer_rect(const aes_window_t *window, GRECT *rect)
{
    WORD title_height;
    WORD side;

    if (window == NULL || rect == NULL || (window->kind & CLOSER) == 0u) {
        return 0;
    }

    title_height = _aes_window_title_height(window);
    side = (WORD) (title_height - 2);
    _aes_set_rect(rect,
        (WORD) (window->outer.g_x + 1),
        (WORD) (window->outer.g_y + 1),
        side,
        side);
    return 1;
}

static int _aes_window_fuller_rect(const aes_window_t *window, GRECT *rect)
{
    WORD title_height;
    WORD side;

    if (window == NULL || rect == NULL || (window->kind & FULLER) == 0u) {
        return 0;
    }

    title_height = _aes_window_title_height(window);
    side = (WORD) (title_height - 2);
    _aes_set_rect(rect,
        (WORD) (window->outer.g_x + window->outer.g_w - side - 2),
        (WORD) (window->outer.g_y + 1),
        (WORD) (side + 1),
        side);
    return 1;
}

static int _aes_window_title_rect(const aes_window_t *window, GRECT *rect)
{
    GRECT closer_rect = { 0, 0, 0, 0 };
    GRECT fuller_rect = { 0, 0, 0, 0 };
    WORD left;
    WORD right;
    WORD title_height;

    if (window == NULL || rect == NULL ||
        (window->kind & (NAME | MOVER | CLOSER | FULLER)) == 0u) {
        return 0;
    }

    title_height = _aes_window_title_height(window);
    left = (WORD) (window->outer.g_x + 2);
    right = (WORD) (window->outer.g_x + window->outer.g_w - 3);
    if (_aes_window_closer_rect(window, &closer_rect) != 0) {
        left = (WORD) (closer_rect.g_x + closer_rect.g_w + 2);
    }
    if (_aes_window_fuller_rect(window, &fuller_rect) != 0) {
        right = (WORD) (fuller_rect.g_x - 2);
    }
    if (right < left) {
        return 0;
    }

    _aes_set_rect(rect, left, (WORD) (window->outer.g_y + 2),
        (WORD) (right - left + 1), (WORD) (title_height - 4));
    return 1;
}

static int _aes_window_sizer_rect(const aes_window_t *window, GRECT *rect)
{
    WORD size;
    WORD right_border;
    WORD bottom_border;

    if (window == NULL || rect == NULL || (window->kind & SIZER) == 0u) {
        return 0;
    }

    right_border = _aes_window_right_border(window);
    bottom_border = _aes_window_bottom_border(window);
    size = (WORD) (_aes_min_word(right_border, bottom_border) - 2);
    _aes_set_rect(rect,
        (WORD) (window->outer.g_x + window->outer.g_w - right_border + 2),
        (WORD) (window->outer.g_y + window->outer.g_h - bottom_border + 2),
        size,
        size);
    return 1;
}

int _aes_window_vtrack_rect(const aes_window_t *window, GRECT *rect)
{
    WORD left;
    WORD right;
    WORD top;
    WORD bottom;

    if (window == NULL || rect == NULL || _aes_window_has_vscroll(window) == 0) {
        return 0;
    }

    left = (WORD) (window->work.g_x + window->work.g_w);
    right = (WORD) (window->outer.g_x + window->outer.g_w - 2);
    top = window->work.g_y;
    bottom = (WORD) (window->work.g_y + window->work.g_h - 1);
    if (right < left || bottom < top) {
        return 0;
    }

    _aes_set_rect(rect, left, top, (WORD) (right - left + 1),
        (WORD) (bottom - top + 1));
    return 1;
}

int _aes_window_htrack_rect(const aes_window_t *window, GRECT *rect)
{
    WORD left;
    WORD right;
    WORD top;
    WORD bottom;

    if (window == NULL || rect == NULL || _aes_window_has_hscroll(window) == 0) {
        return 0;
    }

    left = (WORD) (window->outer.g_x + 1);
    right = (WORD) (window->work.g_x + window->work.g_w - 1);
    top = (WORD) (window->work.g_y + window->work.g_h);
    bottom = (WORD) (window->outer.g_y + window->outer.g_h - 2);
    if (right < left || bottom < top) {
        return 0;
    }

    _aes_set_rect(rect, left, top, (WORD) (right - left + 1),
        (WORD) (bottom - top + 1));
    return 1;
}

int _aes_window_vslot_rect(const aes_window_t *window, GRECT *rect)
{
    GRECT track;
    GRECT up_rect;
    GRECT down_rect;
    WORD top;
    WORD bottom;

    if (window == NULL || rect == NULL ||
        _aes_window_vtrack_rect(window, &track) == 0) {
        return 0;
    }

    top = track.g_y;
    bottom = (WORD) (track.g_y + track.g_h - 1);
    if (_aes_window_vup_rect(window, &up_rect) != 0) {
        top = (WORD) (up_rect.g_y + up_rect.g_h);
    }
    if (_aes_window_vdown_rect(window, &down_rect) != 0) {
        bottom = (WORD) (down_rect.g_y - 1);
    }
    if (bottom < top) {
        return 0;
    }

    _aes_set_rect(rect, track.g_x, top, track.g_w,
        (WORD) (bottom - top + 1));
    return 1;
}

int _aes_window_hslot_rect(const aes_window_t *window, GRECT *rect)
{
    GRECT track;
    GRECT left_rect;
    GRECT right_rect;
    WORD left;
    WORD right;

    if (window == NULL || rect == NULL ||
        _aes_window_htrack_rect(window, &track) == 0) {
        return 0;
    }

    left = track.g_x;
    right = (WORD) (track.g_x + track.g_w - 1);
    if (_aes_window_hleft_rect(window, &left_rect) != 0) {
        left = (WORD) (left_rect.g_x + left_rect.g_w);
    }
    if (_aes_window_hright_rect(window, &right_rect) != 0) {
        right = (WORD) (right_rect.g_x - 1);
    }
    if (right < left) {
        return 0;
    }

    _aes_set_rect(rect, left, track.g_y, (WORD) (right - left + 1), track.g_h);
    return 1;
}

static int _aes_window_vup_rect(const aes_window_t *window, GRECT *rect)
{
    GRECT track;
    WORD side;

    if (window == NULL || rect == NULL || (window->kind & UPARROW) == 0u ||
        _aes_window_vtrack_rect(window, &track) == 0) {
        return 0;
    }

    side = _aes_min_word(_aes_window_scroll_button_side(window), track.g_h);
    _aes_set_rect(rect, track.g_x, track.g_y, track.g_w, side);
    return 1;
}

static int _aes_window_vdown_rect(const aes_window_t *window, GRECT *rect)
{
    GRECT track;
    WORD side;

    if (window == NULL || rect == NULL || (window->kind & DNARROW) == 0u ||
        _aes_window_vtrack_rect(window, &track) == 0) {
        return 0;
    }

    side = _aes_min_word(_aes_window_scroll_button_side(window), track.g_h);
    _aes_set_rect(rect, track.g_x, (WORD) (track.g_y + track.g_h - side),
        track.g_w, side);
    return 1;
}

static int _aes_window_hleft_rect(const aes_window_t *window, GRECT *rect)
{
    GRECT track;
    WORD side;

    if (window == NULL || rect == NULL || (window->kind & LFARROW) == 0u ||
        _aes_window_htrack_rect(window, &track) == 0) {
        return 0;
    }

    side = _aes_min_word(_aes_window_scroll_button_side(window), track.g_w);
    _aes_set_rect(rect, track.g_x, track.g_y, side, track.g_h);
    return 1;
}

static int _aes_window_hright_rect(const aes_window_t *window, GRECT *rect)
{
    GRECT track;
    WORD side;

    if (window == NULL || rect == NULL || (window->kind & RTARROW) == 0u ||
        _aes_window_htrack_rect(window, &track) == 0) {
        return 0;
    }

    side = _aes_min_word(_aes_window_scroll_button_side(window), track.g_w);
    _aes_set_rect(rect, (WORD) (track.g_x + track.g_w - side), track.g_y,
        side, track.g_h);
    return 1;
}

int _aes_window_vthumb_rect(const aes_window_t *window, GRECT *rect)
{
    GRECT slot;
    WORD span;
    WORD size;
    WORD pos;

    if (window == NULL || rect == NULL || (window->kind & VSLIDE) == 0u ||
        _aes_window_vslot_rect(window, &slot) == 0) {
        return 0;
    }

    span = slot.g_h;
    size = (WORD) ((span * _aes_max_word(0, _aes_min_word(window->vslsize, 1000)))
        / 1000L);
    size = _aes_max_word(size, _aes_min_word(slot.g_w, span));
    size = _aes_min_word(size, span);
    pos = slot.g_y;
    if (span > size) {
        pos = (WORD) (slot.g_y + ((span - size) *
            _aes_max_word(0, _aes_min_word(window->vslide, 1000))) / 1000L);
    }

    _aes_set_rect(rect, slot.g_x, pos, slot.g_w, size);
    return 1;
}

int _aes_window_hthumb_rect(const aes_window_t *window, GRECT *rect)
{
    GRECT slot;
    WORD span;
    WORD size;
    WORD pos;

    if (window == NULL || rect == NULL || (window->kind & HSLIDE) == 0u ||
        _aes_window_hslot_rect(window, &slot) == 0) {
        return 0;
    }

    span = slot.g_w;
    size = (WORD) ((span * _aes_max_word(0, _aes_min_word(window->hslsize, 1000)))
        / 1000L);
    size = _aes_max_word(size, _aes_min_word(slot.g_h, span));
    size = _aes_min_word(size, span);
    pos = slot.g_x;
    if (span > size) {
        pos = (WORD) (slot.g_x + ((span - size) *
            _aes_max_word(0, _aes_min_word(window->hslide, 1000))) / 1000L);
    }

    _aes_set_rect(rect, pos, slot.g_y, size, slot.g_h);
    return 1;
}

/*
 * Derives the work-area rectangle from a window's outer rectangle.
 */
void _aes_compute_work(aes_window_t *window)
{
    WORD left_border;
    WORD right_border;
    WORD bottom_border;
    WORD title_height;

    if (window == NULL) {
        return;
    }

    left_border = _aes_window_left_border(window);
    right_border = _aes_window_right_border(window);
    bottom_border = _aes_window_bottom_border(window);
    title_height = _aes_window_title_height(window);
    _aes_set_rect(&window->work,
        (WORD) (window->outer.g_x + left_border),
        (WORD) (window->outer.g_y + title_height),
        (WORD) (window->outer.g_w - left_border - right_border),
        (WORD) (window->outer.g_h - title_height - bottom_border));
    if (window->work.g_w < 0) {
        window->work.g_w = 0;
    }
    if (window->work.g_h < 0) {
        window->work.g_h = 0;
    }
}

/*
 * Stores a `WF_NAME` or `WF_INFO` string into the hosted window state.
 */
WORD _aes_wind_set_text(WORD handle, WORD field, const char *text)
{
    aes_window_t *window = _aes_find_window(handle);
    char *target;
    size_t size;

    if (window == NULL || text == NULL) {
        return 0;
    }

    if (field == WF_NAME) {
        target = window->name;
        size = sizeof(window->name);
    } else if (field == WF_INFO) {
        target = window->info;
        size = sizeof(window->info);
    } else {
        return 0;
    }

    if (strncmp(target, text, size) == 0) {
        return 1;
    }

    strncpy(target, text, size - 1u);
    target[size - 1u] = '\0';
    if (window->open != 0) {
        _aes_draw_window_frame(window);
    }
    return 1;
}

static void _aes_present_after_window_draw(void)
{
    if (_aes.update_depth > 0) {
        return;
    }

    _vdi_begin_update();
    _vdi_present_screen();
    _vdi_end_update();
}

static void _aes_desktop_rect_local(GRECT *rect)
{
    WORD menu_height;

    if (rect == NULL) {
        return;
    }

    if (_aes.vdi_ready == 0) {
        _aes_set_rect(rect, 0, 0, 0, 0);
        return;
    }

    menu_height = _aes_menu_bar_height();
    _aes_set_rect(rect, 0, menu_height, (WORD) (_aes.work_out[0] + 1),
        (WORD) (_aes_max_word((WORD) 0,
            (WORD) (_aes.work_out[1] + 1 - menu_height))));
}

int _aes_rects_intersect(const GRECT *left, const GRECT *right)
{
    WORD left_right;
    WORD left_bottom;
    WORD right_right;
    WORD right_bottom;

    if (left == NULL || right == NULL || left->g_w <= 0 || left->g_h <= 0 ||
        right->g_w <= 0 || right->g_h <= 0) {
        return 0;
    }

    left_right = (WORD) (left->g_x + left->g_w - 1);
    left_bottom = (WORD) (left->g_y + left->g_h - 1);
    right_right = (WORD) (right->g_x + right->g_w - 1);
    right_bottom = (WORD) (right->g_y + right->g_h - 1);

    if (left_right < right->g_x || right_right < left->g_x ||
        left_bottom < right->g_y || right_bottom < left->g_y) {
        return 0;
    }
    return 1;
}

int _aes_intersect_rects(const GRECT *left, const GRECT *right, GRECT *out)
{
    WORD left_right;
    WORD left_bottom;
    WORD right_right;
    WORD right_bottom;
    WORD x0;
    WORD y0;
    WORD x1;
    WORD y1;

    if (out == NULL || _aes_rects_intersect(left, right) == 0) {
        return 0;
    }

    left_right = (WORD) (left->g_x + left->g_w - 1);
    left_bottom = (WORD) (left->g_y + left->g_h - 1);
    right_right = (WORD) (right->g_x + right->g_w - 1);
    right_bottom = (WORD) (right->g_y + right->g_h - 1);

    x0 = _aes_max_word(left->g_x, right->g_x);
    y0 = _aes_max_word(left->g_y, right->g_y);
    x1 = _aes_min_word(left_right, right_right);
    y1 = _aes_min_word(left_bottom, right_bottom);
    _aes_set_rect(out, x0, y0, (WORD) (x1 - x0 + 1), (WORD) (y1 - y0 + 1));
    return 1;
}

WORD _aes_subtract_rect(const GRECT *source, const GRECT *cover, GRECT out[4])
{
    GRECT overlap;
    WORD count = 0;
    WORD source_right;
    WORD source_bottom;
    WORD overlap_right;
    WORD overlap_bottom;

    if (out == NULL || source == NULL || source->g_w <= 0 || source->g_h <= 0) {
        return 0;
    }

    if (cover == NULL || cover->g_w <= 0 || cover->g_h <= 0 ||
        _aes_intersect_rects(source, cover, &overlap) == 0) {
        out[0] = *source;
        return 1;
    }

    source_right = (WORD) (source->g_x + source->g_w - 1);
    source_bottom = (WORD) (source->g_y + source->g_h - 1);
    overlap_right = (WORD) (overlap.g_x + overlap.g_w - 1);
    overlap_bottom = (WORD) (overlap.g_y + overlap.g_h - 1);

    if (source->g_y < overlap.g_y) {
        _aes_set_rect(&out[count++], source->g_x, source->g_y, source->g_w,
            (WORD) (overlap.g_y - source->g_y));
    }
    if (overlap_bottom < source_bottom) {
        _aes_set_rect(&out[count++], source->g_x, (WORD) (overlap_bottom + 1),
            source->g_w, (WORD) (source_bottom - overlap_bottom));
    }
    if (source->g_x < overlap.g_x) {
        _aes_set_rect(&out[count++], source->g_x, overlap.g_y,
            (WORD) (overlap.g_x - source->g_x), overlap.g_h);
    }
    if (overlap_right < source_right) {
        _aes_set_rect(&out[count++], (WORD) (overlap_right + 1), overlap.g_y,
            (WORD) (source_right - overlap_right), overlap.g_h);
    }

    return count;
}

static void _aes_expand_window_damage_rect(const GRECT *src, GRECT *out)
{
    if (out == NULL) {
        return;
    }
    if (src == NULL || src->g_w <= 0 || src->g_h <= 0) {
        _aes_set_rect(out, 0, 0, 0, 0);
        return;
    }

    /*
     * Hosted AES draws a one-pixel frame shadow on the right and bottom
     * edges, so damage tracking must include that extra extent.
     */
    out->g_x = src->g_x;
    out->g_y = src->g_y;
    out->g_w = (WORD) (src->g_w + 1);
    out->g_h = (WORD) (src->g_h + 1);
}

void _aes_redraw_region(const GRECT *dirty)
{
    uint32_t target_z;
    WORD clip[4];
    GRECT desktop;
    GRECT menu_rect;
    WORD bar;
    int redraw_menu = 0;

    if (dirty == NULL || dirty->g_w <= 0 || dirty->g_h <= 0 ||
        _aes.vdi_ready == 0) {
        return;
    }

    if (_aes.menu_visible != 0 && _aes.menu_tree != NULL) {
        bar = _aes.menu_tree[ROOT].ob_head;
        if (bar != NIL &&
            _aes_menu_subtree_rect(_aes.menu_tree, bar, &menu_rect) != 0 &&
            _aes_rects_intersect(&menu_rect, dirty) != 0) {
            redraw_menu = 1;
        }
    }

    _aes_desktop_rect_local(&desktop);
    if (_aes_rects_intersect(&desktop, dirty) == 0) {
        if (redraw_menu != 0) {
            _aes_menu_redraw_tree(_aes.menu_tree);
        }
        return;
    }

    clip[0] = _aes_max_word(desktop.g_x, dirty->g_x);
    clip[1] = _aes_max_word(desktop.g_y, dirty->g_y);
    clip[2] = _aes_min_word((WORD) (desktop.g_x + desktop.g_w - 1),
        (WORD) (dirty->g_x + dirty->g_w - 1));
    clip[3] = _aes_min_word((WORD) (desktop.g_y + desktop.g_h - 1),
        (WORD) (dirty->g_y + dirty->g_h - 1));

    ++_aes.update_depth;
    _vdi_begin_update();
    vs_clip(_aes.vdi_handle, 1, clip);
    _aes_fill_checker_rect(clip[0], clip[1], clip[2], clip[3]);
    for (target_z = 1u; target_z < _aes.next_window_z; ++target_z) {
        size_t i;

        for (i = 0; i < AES_MAX_WINDOWS; ++i) {
            if (_aes.windows[i].used != 0 && _aes.windows[i].open != 0 &&
                _aes.windows[i].z_order == target_z &&
                _aes_rects_intersect(&_aes.windows[i].outer, dirty) != 0) {
                _aes_draw_window_frame(&_aes.windows[i]);
                _aes_queue_window_redraw(&_aes.windows[i], dirty);
            }
        }
    }
    vs_clip(_aes.vdi_handle, 0, clip);
    --_aes.update_depth;
    _vdi_end_update();

    if (redraw_menu != 0) {
        _aes_menu_redraw_tree(_aes.menu_tree);
    }
}

static void _aes_queue_window_redraw(const aes_window_t *window,
                                     const GRECT *dirty)
{
    GRECT pending[64];
    GRECT next_pending[64];
    GRECT redraw;
    WORD msg[8];
    WORD pending_count = 0;
    size_t i;

    if (window == NULL || dirty == NULL || window->owner == 0 ||
        window->work.g_w <= 0 || window->work.g_h <= 0) {
        return;
    }

    if (_aes_intersect_rects(&window->work, dirty, &redraw) == 0) {
        return;
    }

    pending[0] = redraw;
    pending_count = 1;

    for (i = 0; i < AES_MAX_WINDOWS && pending_count > 0; ++i) {
        WORD next_count = 0;
        WORD j;

        if (_aes.windows[i].used == 0 || _aes.windows[i].open == 0 ||
            _aes.windows[i].handle == window->handle ||
            _aes.windows[i].z_order <= window->z_order) {
            continue;
        }

        for (j = 0; j < pending_count; ++j) {
            GRECT fragments[4];
            WORD fragment_count = _aes_subtract_rect(&pending[j],
                &_aes.windows[i].outer, fragments);
            WORD k;

            for (k = 0; k < fragment_count && next_count < 64; ++k) {
                next_pending[next_count++] = fragments[k];
            }
        }

        memcpy(pending, next_pending, (size_t) next_count * sizeof(GRECT));
        pending_count = next_count;
    }

    for (i = 0; i < (size_t) pending_count; ++i) {
        msg[0] = WM_REDRAW;
        msg[1] = _aes.current_app_id;
        msg[2] = 0;
        msg[3] = window->handle;
        msg[4] = pending[i].g_x;
        msg[5] = pending[i].g_y;
        msg[6] = pending[i].g_w;
        msg[7] = pending[i].g_h;
        (void) appl_write(window->owner, 8, msg);
    }
}

void _aes_redraw_window_change(const GRECT *before, const GRECT *after)
{
    GRECT expanded_before;
    GRECT expanded_after;
    GRECT exposed[4];
    WORD exposed_count;
    WORD i;

    _aes_expand_window_damage_rect(before, &expanded_before);
    _aes_expand_window_damage_rect(after, &expanded_after);
    exposed_count = _aes_subtract_rect(&expanded_before, &expanded_after,
        exposed);

    for (i = 0; i < exposed_count; ++i) {
        _aes_redraw_region(&exposed[i]);
    }
    _aes_redraw_region(&expanded_after);
}

void _aes_redraw_window_title_states(const aes_window_t *previous_top,
                                     const aes_window_t *new_top)
{
    GRECT dirty;

    if (previous_top != NULL && previous_top->open != 0 &&
        previous_top->used != 0) {
        dirty.g_x = previous_top->outer.g_x;
        dirty.g_y = previous_top->outer.g_y;
        dirty.g_w = previous_top->outer.g_w;
        dirty.g_h = (WORD) (previous_top->work.g_y - previous_top->outer.g_y);
        if (dirty.g_w > 0 && dirty.g_h > 0) {
            _aes_redraw_region(&dirty);
        }
    }

    if (new_top != NULL && new_top->open != 0 && new_top->used != 0 &&
        new_top != previous_top) {
        dirty.g_x = new_top->outer.g_x;
        dirty.g_y = new_top->outer.g_y;
        dirty.g_w = new_top->outer.g_w;
        dirty.g_h = (WORD) (new_top->work.g_y - new_top->outer.g_y);
        if (dirty.g_w > 0 && dirty.g_h > 0) {
            _aes_redraw_region(&dirty);
        }
    }
}

static void _aes_fill_rect(WORD x0, WORD y0, WORD x1, WORD y1, WORD color)
{
    WORD rect[4];

    if (x0 > x1 || y0 > y1) {
        return;
    }

    rect[0] = x0;
    rect[1] = y0;
    rect[2] = x1;
    rect[3] = y1;
    vsf_color(_aes.vdi_handle, color);
    v_bar(_aes.vdi_handle, rect);
}

static void _aes_draw_rect_edges(WORD x0, WORD y0, WORD x1, WORD y1,
                                 int draw_top,
                                 int draw_right,
                                 int draw_bottom,
                                 int draw_left)
{
    WORD line[4];

    if (x0 > x1 || y0 > y1) {
        return;
    }

    if (draw_top != 0) {
        line[0] = x0;
        line[1] = y0;
        line[2] = x1;
        line[3] = y0;
        v_pline(_aes.vdi_handle, 2, line);
    }
    if (draw_right != 0) {
        line[0] = x1;
        line[1] = y0;
        line[2] = x1;
        line[3] = y1;
        v_pline(_aes.vdi_handle, 2, line);
    }
    if (draw_bottom != 0) {
        line[0] = x0;
        line[1] = y1;
        line[2] = x1;
        line[3] = y1;
        v_pline(_aes.vdi_handle, 2, line);
    }
    if (draw_left != 0) {
        line[0] = x0;
        line[1] = y0;
        line[2] = x0;
        line[3] = y1;
        v_pline(_aes.vdi_handle, 2, line);
    }
}

static void _aes_invert_rect(WORD x0, WORD y0, WORD x1, WORD y1)
{
    WORD x;
    WORD y;

    if (x0 > x1 || y0 > y1) {
        return;
    }

    _vdi_prepare_screen_write();
    for (y = y0; y <= y1; ++y) {
        for (x = x0; x <= x1; ++x) {
            _vdi_set_screen_pixel(x, y,
                (WORD) (_vdi_get_screen_pixel(x, y) == 0 ? 1 : 0));
        }
    }
}

static void _aes_fill_pattern_rect(WORD x0, WORD y0, WORD x1, WORD y1,
                                   const uint8_t *rows, size_t row_count)
{
    WORD x;
    WORD y;
    WORD dark_pixel = (_aes_dark_color() == WHITE) ? 1 : 0;
    uint8_t row_bits;

    if (x0 > x1 || y0 > y1 || rows == NULL || row_count == 0u) {
        return;
    }

    _aes_fill_rect(x0, y0, x1, y1, _aes_light_color());
    for (y = y0; y <= y1; ++y) {
        row_bits = rows[(size_t) (y % (WORD) row_count)];
        for (x = x0; x <= x1; ++x) {
            if ((row_bits & (uint8_t) (0x80u >> (x & 7))) != 0u) {
                _vdi_plot_pixel(x, y, dark_pixel);
            }
        }
    }
}

static void _aes_fill_checker_rect(WORD x0, WORD y0, WORD x1, WORD y1)
{
    static const uint8_t desktop_rows[] = {
        0xaa, 0x55, 0xaa
    };

    if (x0 > x1 || y0 > y1) {
        return;
    }

    _aes_fill_pattern_rect(x0, y0, x1, y1, desktop_rows, sizeof(desktop_rows));
}

static int _aes_menu_subtree_rect(OBJECT *tree, WORD object, GRECT *rect)
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

static int _aes_menu_is_separator_text(const char *text)
{
    int saw_dash = 0;

    if (text == NULL) {
        return 0;
    }

    while (*text != '\0') {
        if (*text != ' ' && *text != '\t') {
            if (*text != '-') {
                return 0;
            }
            saw_dash = 1;
        }
        ++text;
    }

    return saw_dash;
}

static int _aes_menu_split_shortcut(const char *text,
                                    char *label,
                                    size_t label_size,
                                    char *shortcut,
                                    size_t shortcut_size)
{
    const char *tab;
    size_t left_len;
    size_t right_len;

    if (label != NULL && label_size > 0u) {
        label[0] = '\0';
    }
    if (shortcut != NULL && shortcut_size > 0u) {
        shortcut[0] = '\0';
    }
    if (text == NULL || label == NULL || shortcut == NULL ||
        label_size == 0u || shortcut_size == 0u) {
        return 0;
    }

    tab = strchr(text, '\t');
    if (tab == NULL) {
        strncpy(label, text, label_size - 1u);
        label[label_size - 1u] = '\0';
        return 0;
    }

    left_len = (size_t) (tab - text);
    if (left_len >= label_size) {
        left_len = label_size - 1u;
    }
    memcpy(label, text, left_len);
    label[left_len] = '\0';

    ++tab;
    right_len = strlen(tab);
    if (right_len >= shortcut_size) {
        right_len = shortcut_size - 1u;
    }
    memcpy(shortcut, tab, right_len);
    shortcut[right_len] = '\0';
    return 1;
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

int _aes_window_is_top(const aes_window_t *window)
{
    size_t i;
    uint32_t top_z = 0u;

    if (window == NULL || window->used == 0 || window->open == 0) {
        return 0;
    }

    for (i = 0; i < AES_MAX_WINDOWS; ++i) {
        if (_aes.windows[i].used != 0 && _aes.windows[i].open != 0 &&
            _aes.windows[i].z_order > top_z) {
            top_z = _aes.windows[i].z_order;
        }
    }

    return window->z_order == top_z;
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

static WORD _aes_light_color(void)
{
    const char *value = getenv("GEM_RASTA_INVERSE");

    if (value == NULL || value[0] == '\0') {
        value = getenv("RASTA_INVERSE");
    }
    if (value == NULL || value[0] == '\0') {
        return BLACK;
    }
    if (strcmp(value, "off") == 0 || strcmp(value, "false") == 0 ||
        strcmp(value, "0") == 0) {
        return WHITE;
    }
    return BLACK;
}

static WORD _aes_dark_color(void)
{
    return (_aes_light_color() == BLACK) ? WHITE : BLACK;
}

static void _aes_draw_window_icon(WORD x0, WORD y0, WORD x1, WORD y1,
                                  char glyph)
{
    _aes_draw_window_icon_offset(x0, y0, x1, y1, glyph, 0, 0);
}

static void _aes_draw_window_icon_offset(WORD x0, WORD y0, WORD x1, WORD y1,
                                         char glyph, WORD dx, WORD dy)
{
    char text[2];
    WORD text_attrib[6];
    WORD previous_font = 0;
    WORD system_font = 1;
    WORD text_width;
    WORD text_height;
    WORD text_x;
    WORD text_y;

    if (x0 > x1 || y0 > y1) {
        return;
    }

    if (vqt_attributes(_aes.vdi_handle, text_attrib) != 0) {
        previous_font = text_attrib[0];
    }
    if (previous_font != 0 && previous_font != system_font) {
        (void) vst_font(_aes.vdi_handle, system_font);
    }

    text[0] = glyph;
    text[1] = '\0';
    text_width = (WORD) _vdi_string_width(text);
    text_height = _vdi_font_text_height();
    text_x = (WORD) (x0 + ((x1 - x0 + 1) - text_width) / 2 + dx);
    text_y = (WORD) (y0 + ((y1 - y0 + 1) - text_height) / 2 +
        _vdi_font_ascent() + dy);
    _aes_draw_text(text_x, text_y, _aes_dark_color(), text);

    if (previous_font != 0 && previous_font != system_font) {
        (void) vst_font(_aes.vdi_handle, previous_font);
    }
}

static void _aes_draw_sizer_glyph(const aes_window_t *window)
{
    GRECT rect;
    WORD x1;
    WORD y1;

    if (window == NULL || _aes_window_sizer_rect(window, &rect) == 0) {
        return;
    }

    x1 = (WORD) (rect.g_x + rect.g_w - 2);
    y1 = (WORD) (rect.g_y + rect.g_h - 2);
    _aes_fill_rect(rect.g_x, rect.g_y, x1, y1, _aes_light_color());
    _aes_draw_window_icon(rect.g_x, rect.g_y, x1, y1, 6);
}

static void _aes_draw_window_title(const aes_window_t *window,
                                   const WORD outer_box[4],
                                   WORD title_height)
{
    static const uint8_t title_rows[] = {
        0x00, 0x55, 0x00, 0x55
    };
    GRECT closer_rect;
    GRECT fuller_rect;
    WORD text_attrib[6];
    WORD previous_font = 0;
    WORD system_font = 1;
    WORD title_sep[4];
    WORD title_left;
    WORD title_right;
    WORD title_box_left = 0;
    WORD title_box_right = -1;
    WORD title_text_left;
    WORD title_text_right;
    WORD title_top;
    WORD title_bottom;
    WORD text_height;
    WORD title_y;
    WORD title_width;
    WORD available_width;
    WORD title_x;
    int has_closer;
    int has_fuller;
    int active_title;

    if (window == NULL || title_height <= 0) {
        return;
    }

    if (vqt_attributes(_aes.vdi_handle, text_attrib) != 0) {
        previous_font = text_attrib[0];
    }
    if (previous_font != 0 && previous_font != system_font) {
        (void) vst_font(_aes.vdi_handle, system_font);
    }

    text_height = _vdi_font_text_height();
    active_title = _aes_window_is_top(window);
    has_closer = _aes_window_closer_rect(window, &closer_rect);
    has_fuller = _aes_window_fuller_rect(window, &fuller_rect);
    title_top = (WORD) (outer_box[1] + 1);
    title_bottom = (WORD) (window->work.g_y - 2);
    title_left = (WORD) (outer_box[0] + 1);
    title_right = (WORD) (outer_box[2] - 1);

    title_sep[0] = outer_box[0];
    title_sep[1] = (WORD) (window->work.g_y - 1);
    title_sep[2] = outer_box[2];
    title_sep[3] = title_sep[1];
    vsl_color(_aes.vdi_handle, _aes_dark_color());
    v_pline(_aes.vdi_handle, 2, title_sep);

    if (has_closer != 0) {
        WORD close_sep[4];
        WORD close_right = (WORD) (closer_rect.g_x + closer_rect.g_w - 2);

        _aes_fill_rect(closer_rect.g_x, closer_rect.g_y,
            close_right,
            (WORD) (closer_rect.g_y + closer_rect.g_h - 1),
            _aes_light_color());
        close_sep[0] = close_right;
        close_sep[1] = closer_rect.g_y;
        close_sep[2] = close_sep[0];
        close_sep[3] = (WORD) (closer_rect.g_y + closer_rect.g_h - 1);
        vsl_color(_aes.vdi_handle, _aes_dark_color());
        v_pline(_aes.vdi_handle, 2, close_sep);
        _aes_draw_window_icon(closer_rect.g_x, closer_rect.g_y,
            close_right,
            (WORD) (closer_rect.g_y + closer_rect.g_h - 1), 5);
        title_left = (WORD) (close_sep[0] + 1);
    }

    if (has_fuller != 0) {
        WORD fuller_sep[4];
        WORD fuller_left = (WORD) (fuller_rect.g_x + 1);
        WORD fuller_right = (WORD) (fuller_rect.g_x + fuller_rect.g_w - 1);

        _aes_fill_rect(fuller_left, fuller_rect.g_y,
            fuller_right,
            (WORD) (fuller_rect.g_y + fuller_rect.g_h - 1),
            _aes_light_color());
        fuller_sep[0] = fuller_left;
        fuller_sep[1] = fuller_rect.g_y;
        fuller_sep[2] = fuller_sep[0];
        fuller_sep[3] = (WORD) (fuller_rect.g_y + fuller_rect.g_h - 1);
        vsl_color(_aes.vdi_handle, _aes_dark_color());
        v_pline(_aes.vdi_handle, 2, fuller_sep);
        _aes_draw_window_icon_offset(fuller_left, fuller_rect.g_y,
            fuller_right,
            (WORD) (fuller_rect.g_y + fuller_rect.g_h - 1), 7, 2, 0);
        title_right = (WORD) (fuller_sep[0] - 1);
    }

    if ((window->kind & NAME) != 0u && window->name[0] != '\0') {
        title_width = (WORD) _vdi_string_width(window->name);
        available_width = (WORD) (title_right - title_left + 1);
        if (available_width <= title_width) {
            title_x = title_left;
        } else {
            title_x = (WORD) (title_left +
                (available_width - title_width) / 2);
        }

        title_text_left = (WORD) (title_x - 2);
        title_text_right = (WORD) (title_x + title_width + 1);
        if (title_text_left < title_left) {
            title_text_left = title_left;
        }
        if (title_text_right > title_right) {
            title_text_right = title_right;
        }
        title_box_left = title_text_left;
        title_box_right = title_text_right;
    }

    if (title_left <= title_right && title_top <= title_bottom) {
        if (active_title != 0) {
            if (title_box_left <= title_box_right) {
                WORD left_fill_right = (WORD) (title_box_left - 1);
                WORD right_fill_left = (WORD) (title_box_right + 1);

                if (title_left <= left_fill_right) {
                    _aes_fill_pattern_rect(title_left, title_top,
                        left_fill_right, title_bottom, title_rows,
                        sizeof(title_rows));
                }
                if (right_fill_left <= title_right) {
                    _aes_fill_pattern_rect(right_fill_left, title_top,
                        title_right, title_bottom, title_rows,
                        sizeof(title_rows));
                }
            } else {
                _aes_fill_pattern_rect(title_left, title_top, title_right,
                    title_bottom, title_rows,
                    sizeof(title_rows));
            }
        } else {
            _aes_fill_rect(title_left, title_top, title_right, title_bottom,
                _aes_light_color());
        }
    }

    if (title_box_left <= title_box_right) {
        _aes_fill_rect(title_box_left, title_top, title_box_right,
            title_bottom, _aes_light_color());
        title_y = (WORD) (title_top +
            (title_bottom - title_top - text_height) / 2 +
            _vdi_font_ascent() + 1);
        _aes_draw_text(title_x, title_y, _aes_dark_color(), window->name);
    }

    if (previous_font != 0 && previous_font != system_font) {
        (void) vst_font(_aes.vdi_handle, previous_font);
    }
}

void _aes_draw_window_frame(const aes_window_t *window)
{
    static const uint8_t scrollbar_rows[] = {
        0x88, 0x22
    };
    WORD outer_box[10];
    WORD left_fill[4];
    WORD right_fill[4];
    WORD bottom_fill[4];
    WORD gutter_x = 0;
    WORD gutter_y = 0;
    WORD gutter_right = 0;
    WORD gutter_bottom = 0;
    WORD gutter_fill[4];
    WORD vertical_sep[4];
    WORD horizontal_sep[4];
    WORD shadow_bottom[4];
    WORD shadow_right[4];
    WORD work_box[10];

    if (_aes.vdi_ready == 0 || window == NULL || window->open == 0) {
        return;
    }

    outer_box[0] = window->outer.g_x;
    outer_box[1] = window->outer.g_y;
    outer_box[2] = (WORD) (window->outer.g_x + window->outer.g_w - 1);
    outer_box[3] = outer_box[1];
    outer_box[4] = outer_box[2];
    outer_box[5] = (WORD) (window->outer.g_y + window->outer.g_h - 1);
    outer_box[6] = outer_box[0];
    outer_box[7] = outer_box[5];
    outer_box[8] = outer_box[0];
    outer_box[9] = outer_box[1];

    work_box[0] = window->work.g_x;
    work_box[1] = window->work.g_y;
    work_box[2] = (WORD) (window->work.g_x + window->work.g_w - 1);
    work_box[3] = work_box[1];
    work_box[4] = work_box[2];
    work_box[5] = (WORD) (window->work.g_y + window->work.g_h - 1);
    work_box[6] = work_box[0];
    work_box[7] = work_box[5];
    work_box[8] = work_box[0];
    work_box[9] = work_box[1];

    vsl_color(_aes.vdi_handle, WHITE);
    v_pline(_aes.vdi_handle, 5, outer_box);
    shadow_bottom[0] = (WORD) (window->outer.g_x + 1);
    shadow_bottom[1] = (WORD) (window->outer.g_y + window->outer.g_h);
    shadow_bottom[2] = (WORD) (window->outer.g_x + window->outer.g_w);
    shadow_bottom[3] = shadow_bottom[1];
    shadow_right[0] = (WORD) (window->outer.g_x + window->outer.g_w);
    shadow_right[1] = (WORD) (window->outer.g_y + 1);
    shadow_right[2] = shadow_right[0];
    shadow_right[3] = (WORD) (window->outer.g_y + window->outer.g_h);
    v_pline(_aes.vdi_handle, 2, shadow_bottom);
    v_pline(_aes.vdi_handle, 2, shadow_right);

    left_fill[0] = (WORD) (window->outer.g_x + 1);
    left_fill[1] = window->work.g_y;
    left_fill[2] = (WORD) (window->work.g_x - 1);
    left_fill[3] = (WORD) (window->outer.g_y + window->outer.g_h - 2);
    if (left_fill[0] <= left_fill[2] && left_fill[1] <= left_fill[3]) {
        _aes_fill_rect(left_fill[0], left_fill[1], left_fill[2], left_fill[3],
            _aes_light_color());
    }

    right_fill[0] = (WORD) (window->work.g_x + window->work.g_w);
    right_fill[1] = window->work.g_y;
    right_fill[2] = (WORD) (window->outer.g_x + window->outer.g_w - 2);
    right_fill[3] = (WORD) (window->outer.g_y + window->outer.g_h - 2);
    if (right_fill[0] <= right_fill[2] && right_fill[1] <= right_fill[3]) {
        _aes_fill_rect(right_fill[0], right_fill[1], right_fill[2],
            right_fill[3], _aes_light_color());
    }

    bottom_fill[0] = (WORD) (window->outer.g_x + 1);
    bottom_fill[1] = (WORD) (window->work.g_y + window->work.g_h);
    bottom_fill[2] = (WORD) (window->outer.g_x + window->outer.g_w - 2);
    bottom_fill[3] = (WORD) (window->outer.g_y + window->outer.g_h - 2);
    if (bottom_fill[0] <= bottom_fill[2] && bottom_fill[1] <= bottom_fill[3]) {
        _aes_fill_rect(bottom_fill[0], bottom_fill[1], bottom_fill[2],
            bottom_fill[3], _aes_light_color());
    }

    if ((window->kind & SIZER) != 0u) {
        gutter_x = (WORD) (window->outer.g_x + window->outer.g_w -
            _aes_window_right_border(window));
        gutter_y = (WORD) (window->outer.g_y + window->outer.g_h -
            _aes_window_bottom_border(window));
        gutter_right = (WORD) (window->outer.g_x + window->outer.g_w - 2);
        gutter_bottom = (WORD) (window->outer.g_y + window->outer.g_h - 2);

        gutter_fill[0] = gutter_x;
        gutter_fill[1] = window->work.g_y;
        gutter_fill[2] = gutter_right;
        gutter_fill[3] = gutter_bottom;
        _aes_fill_rect(gutter_fill[0], gutter_fill[1], gutter_fill[2],
            gutter_fill[3], _aes_light_color());

        gutter_fill[0] = window->outer.g_x + 1;
        gutter_fill[1] = gutter_y;
        gutter_fill[2] = gutter_right;
        gutter_fill[3] = gutter_bottom;
        _aes_fill_rect(gutter_fill[0], gutter_fill[1], gutter_fill[2],
            gutter_fill[3], _aes_light_color());

        vertical_sep[0] = gutter_x;
        vertical_sep[1] = window->outer.g_y + 1;
        vertical_sep[2] = vertical_sep[0];
        vertical_sep[3] = (WORD) (window->outer.g_y + window->outer.g_h - 1);
        horizontal_sep[0] = window->outer.g_x + 1;
        horizontal_sep[1] = gutter_y;
        horizontal_sep[2] = (WORD) (window->outer.g_x + window->outer.g_w - 1);
        horizontal_sep[3] = horizontal_sep[1];
        vsl_color(_aes.vdi_handle, WHITE);
        v_pline(_aes.vdi_handle, 2, vertical_sep);
        v_pline(_aes.vdi_handle, 2, horizontal_sep);
    }

    if (window->work.g_w > 0 && window->work.g_h > 0 &&
        (window->kind & SIZER) == 0u) {
        v_pline(_aes.vdi_handle, 5, work_box);
    }
    _aes_draw_window_title(window, outer_box,
        _aes_window_title_height(window));
    if (_aes_window_has_vscroll(window) != 0) {
        GRECT track;
        GRECT slot;
        GRECT thumb;
        GRECT button;
        WORD right;
        WORD bottom;

        if (_aes_window_vtrack_rect(window, &track) != 0) {
            right = (WORD) (track.g_x + track.g_w - 1);
            bottom = (WORD) (track.g_y + track.g_h - 1);
            _aes_fill_pattern_rect(track.g_x, track.g_y, right, bottom,
                scrollbar_rows, sizeof(scrollbar_rows));
            vsl_color(_aes.vdi_handle, _aes_dark_color());
            _aes_draw_rect_edges(track.g_x, track.g_y, right, bottom,
                1, 0, 1, 1);
        }
        if (_aes_window_vup_rect(window, &button) != 0) {
            right = (WORD) (button.g_x + button.g_w - 1);
            bottom = (WORD) (button.g_y + button.g_h - 1);
            _aes_fill_rect(button.g_x, button.g_y, right, bottom,
                _aes_light_color());
            vsl_color(_aes.vdi_handle, _aes_dark_color());
            _aes_draw_rect_edges(button.g_x, button.g_y, right, bottom,
                0, 0, 1, 1);
            _aes_draw_window_icon_offset(button.g_x, button.g_y, right,
                bottom, 1, 0, -1);
        }
        if (_aes_window_vdown_rect(window, &button) != 0) {
            right = (WORD) (button.g_x + button.g_w - 1);
            bottom = (WORD) (button.g_y + button.g_h - 1);
            _aes_fill_rect(button.g_x, button.g_y, right, bottom,
                _aes_light_color());
            vsl_color(_aes.vdi_handle, _aes_dark_color());
            _aes_draw_rect_edges(button.g_x, button.g_y, right, bottom,
                1, 0, 0, 1);
            _aes_draw_window_icon(button.g_x, button.g_y, right, bottom, 2);
        }
        if (_aes_window_vthumb_rect(window, &thumb) != 0) {
            int draw_top = 1;
            int draw_bottom = 1;

            right = (WORD) (thumb.g_x + thumb.g_w - 1);
            bottom = (WORD) (thumb.g_y + thumb.g_h - 1);
            if (_aes_window_vslot_rect(window, &slot) != 0) {
                if (thumb.g_y <= slot.g_y) {
                    draw_top = 0;
                }
                if (bottom >= slot.g_y + slot.g_h - 1) {
                    draw_bottom = 0;
                }
            }
            _aes_fill_rect(thumb.g_x, thumb.g_y, right, bottom,
                _aes_light_color());
            vsl_color(_aes.vdi_handle, _aes_dark_color());
            _aes_draw_rect_edges(thumb.g_x, thumb.g_y, right, bottom,
                draw_top, 0, draw_bottom, 1);
        }
    }
    if (_aes_window_has_hscroll(window) != 0) {
        GRECT track;
        GRECT slot;
        GRECT thumb;
        GRECT button;
        WORD right;
        WORD bottom;

        if (_aes_window_htrack_rect(window, &track) != 0) {
            right = (WORD) (track.g_x + track.g_w - 1);
            bottom = (WORD) (track.g_y + track.g_h - 1);
            _aes_fill_pattern_rect(track.g_x, track.g_y, right, bottom,
                scrollbar_rows, sizeof(scrollbar_rows));
            vsl_color(_aes.vdi_handle, _aes_dark_color());
            _aes_draw_rect_edges(track.g_x, track.g_y, right, bottom,
                1, 1, 0, 1);
        }
        if (_aes_window_hleft_rect(window, &button) != 0) {
            right = (WORD) (button.g_x + button.g_w - 1);
            bottom = (WORD) (button.g_y + button.g_h - 1);
            _aes_fill_rect(button.g_x, button.g_y, right, bottom,
                _aes_light_color());
            vsl_color(_aes.vdi_handle, _aes_dark_color());
            _aes_draw_rect_edges(button.g_x, button.g_y, right, bottom,
                1, 1, 0, 0);
            _aes_draw_window_icon_offset(button.g_x, button.g_y, right,
                bottom, 4, 0, 2);
        }
        if (_aes_window_hright_rect(window, &button) != 0) {
            right = (WORD) (button.g_x + button.g_w - 1);
            bottom = (WORD) (button.g_y + button.g_h - 1);
            _aes_fill_rect(button.g_x, button.g_y, right, bottom,
                _aes_light_color());
            vsl_color(_aes.vdi_handle, _aes_dark_color());
            _aes_draw_rect_edges(button.g_x, button.g_y, right, bottom,
                1, 0, 0, 1);
            _aes_draw_window_icon_offset(button.g_x, button.g_y, right,
                bottom, 3, 0, 2);
        }
        if (_aes_window_hthumb_rect(window, &thumb) != 0) {
            int draw_left = 1;
            int draw_right = 1;

            right = (WORD) (thumb.g_x + thumb.g_w - 1);
            bottom = (WORD) (thumb.g_y + thumb.g_h - 1);
            if (_aes_window_hslot_rect(window, &slot) != 0) {
                if (thumb.g_x <= slot.g_x) {
                    draw_left = 0;
                }
                if (right >= slot.g_x + slot.g_w - 1) {
                    draw_right = 0;
                }
            }
            _aes_fill_rect(thumb.g_x, thumb.g_y, right, bottom,
                _aes_light_color());
            vsl_color(_aes.vdi_handle, _aes_dark_color());
            _aes_draw_rect_edges(thumb.g_x, thumb.g_y, right, bottom,
                1, draw_right, 0, draw_left);
        }
    }
    _aes_draw_sizer_glyph(window);
    _aes_present_after_window_draw();
}

void _aes_clear_window_frame(const aes_window_t *window)
{
    WORD rect[4];

    if (_aes.vdi_ready == 0 || window == NULL) {
        return;
    }

    rect[0] = window->outer.g_x;
    rect[1] = window->outer.g_y;
    rect[2] = (WORD) (window->outer.g_x + window->outer.g_w - 1);
    rect[3] = (WORD) (window->outer.g_y + window->outer.g_h - 1);
    vsf_color(_aes.vdi_handle, BLACK);
    v_bar(_aes.vdi_handle, rect);
    _aes_present_after_window_draw();
}

void _aes_redraw_open_windows(void)
{
    GRECT desktop;

    _aes_desktop_rect_local(&desktop);
    _aes_redraw_region(&desktop);
}

void _aes_raise_window(aes_window_t *window)
{
    if (window == NULL) {
        return;
    }

    window->z_order = _aes.next_window_z++;
}

WORD _aes_window_hit_part(const aes_window_t *window, WORD x, WORD y)
{
    GRECT rect;
    GRECT thumb;

    if (window == NULL || window->open == 0 ||
        !_aes_point_in_rect(x, y, &window->outer)) {
        return AES_WINDOW_PART_NONE;
    }

    if (_aes_window_closer_rect(window, &rect) != 0 &&
        _aes_point_in_rect(x, y, &rect)) {
        return AES_WINDOW_PART_CLOSER;
    }
    if (_aes_window_fuller_rect(window, &rect) != 0 &&
        _aes_point_in_rect(x, y, &rect)) {
        return AES_WINDOW_PART_FULLER;
    }
    if (_aes_window_sizer_rect(window, &rect) != 0 &&
        _aes_point_in_rect(x, y, &rect)) {
        return AES_WINDOW_PART_SIZER;
    }
    if (_aes_window_vup_rect(window, &rect) != 0 &&
        _aes_point_in_rect(x, y, &rect)) {
        return AES_WINDOW_PART_VUP;
    }
    if (_aes_window_vdown_rect(window, &rect) != 0 &&
        _aes_point_in_rect(x, y, &rect)) {
        return AES_WINDOW_PART_VDOWN;
    }
    if (_aes_window_hleft_rect(window, &rect) != 0 &&
        _aes_point_in_rect(x, y, &rect)) {
        return AES_WINDOW_PART_HLEFT;
    }
    if (_aes_window_hright_rect(window, &rect) != 0 &&
        _aes_point_in_rect(x, y, &rect)) {
        return AES_WINDOW_PART_HRIGHT;
    }
    if (_aes_window_vthumb_rect(window, &thumb) != 0 &&
        _aes_point_in_rect(x, y, &thumb)) {
        return AES_WINDOW_PART_VSLIDE;
    }
    if (_aes_window_hthumb_rect(window, &thumb) != 0 &&
        _aes_point_in_rect(x, y, &thumb)) {
        return AES_WINDOW_PART_HSLIDE;
    }
    if (_aes_window_vtrack_rect(window, &rect) != 0 &&
        _aes_point_in_rect(x, y, &rect)) {
        if (_aes_window_vthumb_rect(window, &thumb) != 0 && y < thumb.g_y) {
            return AES_WINDOW_PART_VPAGE_UP;
        }
        return AES_WINDOW_PART_VPAGE_DOWN;
    }
    if (_aes_window_htrack_rect(window, &rect) != 0 &&
        _aes_point_in_rect(x, y, &rect)) {
        if (_aes_window_hthumb_rect(window, &thumb) != 0 && x < thumb.g_x) {
            return AES_WINDOW_PART_HPAGE_LEFT;
        }
        return AES_WINDOW_PART_HPAGE_RIGHT;
    }
    if (_aes_window_title_rect(window, &rect) != 0 &&
        _aes_point_in_rect(x, y, &rect)) {
        return AES_WINDOW_PART_TITLE;
    }
    if (_aes_point_in_rect(x, y, &window->work)) {
        return AES_WINDOW_PART_WORK;
    }
    return AES_WINDOW_PART_NONE;
}

/*
 * Finds the parent object of one child within a GEM object tree.
 */
static WORD _aes_find_parent(OBJECT *tree, WORD object)
{
    WORD parent;
    WORD last_object;

    if (tree == NULL || object <= ROOT) {
        return NIL;
    }

    last_object = _aes_menu_last_object(tree);

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

/*
 * Computes the absolute tree-space coordinates of an object.
 */
void _aes_object_extent(OBJECT *tree, WORD object, WORD *x, WORD *y)
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
        current = _aes_find_parent(tree, current);
    }

    if (x != NULL) {
        *x = abs_x;
    }
    if (y != NULL) {
        *y = abs_y;
    }
}

/*
 * Resolves an object's specification pointer, following `INDIRECT`
 * wrappers when needed.
 */
static LONG _aes_resolve_spec(const OBJECT *obj)
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

/*
 * Draws a plain text string through the hosted VDI workstation.
 */
static void _aes_draw_text(WORD x, WORD y, WORD color, const char *text)
{
    if (text == NULL || *text == '\0') {
        _aes_draw_trace("text skip x=%d y=%d color=%d text=%p",
            x, y, color, (const void *) text);
        return;
    }

    _aes_draw_trace("text x=%d y=%d color=%d \"%s\"",
        x, y, color, text);
    vst_color(_aes.vdi_handle, color);
    v_gtext(_aes.vdi_handle, x, y, (CONST BYTE *) text);
}

/*
 * Draws one `TEDINFO`-backed text object.
 */
static void _aes_draw_ted_object(const OBJECT *tree,
                                 WORD object,
                                 const OBJECT *obj,
                                 const TEDINFO *ted,
                                 const WORD rect[4])
{
    const char *text;
    WORD text_x;
    WORD text_y;
    WORD text_width;
    WORD caret_width;
    WORD underline_y;
    WORD caret_x;
    WORD text_color;

    if (obj == NULL || ted == NULL) {
        return;
    }

    text = (const char *) (intptr_t) ted->te_ptext;
    text_x = (WORD) (rect[0] + 2);
    text_color = _aes_dark_color();
    text_width = (WORD) _vdi_string_width(text != NULL ? text : "");
    caret_width = text_width;

    if (obj->ob_type == G_FTEXT || obj->ob_type == G_FBOXTEXT) {
        text_y = (WORD) (rect[1] + _vdi_font_ascent());
        underline_y = (WORD) (rect[1] + _vdi_font_text_height() + 1);
        if (underline_y > rect[3]) {
            underline_y = rect[3];
        }
        if (tree == _aes.edit_tree && object == _aes.edit_object &&
            text != NULL) {
            WORD edit_index = _aes.edit_index;
            WORD text_length = (WORD) strlen(text);
            char prefix[256];

            if (edit_index < 0) {
                edit_index = 0;
            }
            if (edit_index > text_length) {
                edit_index = text_length;
            }
            if ((size_t) edit_index >= sizeof(prefix)) {
                edit_index = (WORD) (sizeof(prefix) - 1u);
            }
            memcpy(prefix, text, (size_t) edit_index);
            prefix[edit_index] = '\0';
            caret_width = (WORD) _vdi_string_width(prefix);
        }
        _aes_fill_rect(rect[0], rect[1], rect[2], rect[3], _aes_light_color());
        _aes_draw_hline((WORD) (rect[0] + 1), (WORD) (rect[2] - 1),
            underline_y, _aes_dark_color());
        if ((obj->ob_state & SELECTED) != 0u) {
            caret_x = (WORD) (text_x + caret_width + 1);
            if (caret_x < rect[0] + 1) {
                caret_x = (WORD) (rect[0] + 1);
            }
            if (caret_x > rect[2] - 1) {
                caret_x = (WORD) (rect[2] - 1);
            }
            _aes_draw_vline(caret_x, (WORD) (text_y - _vdi_font_ascent() + 1),
                (WORD) (underline_y - 1), _aes_dark_color());
        }
    } else if (obj->ob_type == G_BOXTEXT) {
        text_y = (WORD) (rect[1] +
            ((rect[3] - rect[1] + 1 - _vdi_font_text_height()) > 0 ?
            (rect[3] - rect[1] + 1 - _vdi_font_text_height()) / 2 : 0) +
            _vdi_font_ascent());
        _aes_fill_rect(rect[0], rect[1], rect[2], rect[3], _aes_light_color());
        _aes_draw_hline(rect[0], rect[2], rect[1], _aes_dark_color());
        _aes_draw_hline(rect[0], rect[2], rect[3], _aes_dark_color());
        _aes_draw_vline(rect[0], rect[1], rect[3], _aes_dark_color());
        _aes_draw_vline(rect[2], rect[1], rect[3], _aes_dark_color());
    } else {
        text_y = (WORD) (rect[1] +
            ((rect[3] - rect[1] + 1 - _vdi_font_text_height()) > 0 ?
            (rect[3] - rect[1] + 1 - _vdi_font_text_height()) / 2 : 0) +
            _vdi_font_ascent());
    }

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

    _aes_draw_text(text_x, text_y, text_color, text);
}

static void _aes_draw_hline(WORD x0, WORD x1, WORD y, WORD color)
{
    WORD pts[4];

    if (x0 > x1) {
        return;
    }

    pts[0] = x0;
    pts[1] = y;
    pts[2] = x1;
    pts[3] = y;
    vsl_color(_aes.vdi_handle, color);
    v_pline(_aes.vdi_handle, 2, pts);
}

static void _aes_draw_vline(WORD x, WORD y0, WORD y1, WORD color)
{
    WORD pts[4];

    if (y0 > y1) {
        return;
    }

    pts[0] = x;
    pts[1] = y0;
    pts[2] = x;
    pts[3] = y1;
    vsl_color(_aes.vdi_handle, color);
    v_pline(_aes.vdi_handle, 2, pts);
}

static void _aes_draw_dialog_frame(const WORD rect[4])
{
    WORD inset;
    WORD left;
    WORD top;
    WORD right;
    WORD bottom;

    for (inset = 0; inset < 5; ++inset) {
        WORD color = (inset == 0 || inset >= 3) ?
            _aes_dark_color() : _aes_light_color();

        left = (WORD) (rect[0] + inset);
        top = (WORD) (rect[1] + inset);
        right = (WORD) (rect[2] - inset);
        bottom = (WORD) (rect[3] - inset);
        if (left > right || top > bottom) {
            break;
        }

        _aes_draw_hline(left, right, top, color);
        _aes_draw_hline(left, right, bottom, color);
        _aes_draw_vline(left, top, bottom, color);
        _aes_draw_vline(right, top, bottom, color);
    }
}

static void _aes_draw_button_frame(const WORD rect[4], WORD dark_color,
                                   WORD light_color, int default_button)
{
    WORD inset;
    WORD border_thickness = default_button != 0 ? 2 : 1;

    (void) light_color;

    for (inset = 0; inset < border_thickness; ++inset) {
        WORD left = (WORD) (rect[0] + inset);
        WORD top = (WORD) (rect[1] + inset);
        WORD right = (WORD) (rect[2] - inset);
        WORD bottom = (WORD) (rect[3] - inset);

        if (left > right || top > bottom) {
            break;
        }

        _aes_draw_hline(left, right, top, dark_color);
        _aes_draw_hline(left, right, bottom, dark_color);
        _aes_draw_vline(left, top, bottom, dark_color);
        _aes_draw_vline(right, top, bottom, dark_color);
    }
}

/*
 * Draws one icon object with a simple framed icon box and label.
 */
static void _aes_draw_icon_object(const OBJECT *obj,
                                  const ICONBLK *icon,
                                  WORD abs_x,
                                  WORD abs_y,
                                  const WORD rect[4])
{
    const char *text;
    WORD icon_box[4];
    WORD text_x;
    WORD text_y;

    if (obj == NULL || icon == NULL) {
        return;
    }

    icon_box[0] = (WORD) (abs_x + icon->ib_xicon);
    icon_box[1] = (WORD) (abs_y + icon->ib_yicon);
    icon_box[2] = (WORD) (icon_box[0] + icon->ib_wicon - 1);
    icon_box[3] = (WORD) (icon_box[1] + icon->ib_hicon - 1);

    if ((obj->ob_state & SELECTED) != 0u) {
        vsf_color(_aes.vdi_handle, _aes_light_color());
        v_bar(_aes.vdi_handle, rect);
    }

    vsl_color(_aes.vdi_handle, _aes_dark_color());
    v_rbox(_aes.vdi_handle, icon_box);

    text = (const char *) (intptr_t) icon->ib_ptext;
    text_x = (WORD) (abs_x + icon->ib_xtext);
    text_y = (WORD) (abs_y + icon->ib_ytext + AES_CHAR_HEIGHT);
    _aes_draw_trace("icon obj=%p rect=%d,%d-%d,%d icon=%d,%d %dx%d text=%p",
        (const void *) obj, rect[0], rect[1], rect[2], rect[3],
        icon_box[0], icon_box[1], icon->ib_wicon, icon->ib_hicon,
        (const void *) text);
    _aes_draw_text(text_x, text_y, _aes_dark_color(), text);
}

/*
 * Invokes one AES user-defined draw callback.
 */
static void _aes_draw_user_object(const OBJECT *tree,
                                  WORD object,
                                  const OBJECT *obj,
                                  WORD abs_x,
                                  WORD abs_y,
                                  WORD clip[4])
{
    USERBLK *user;
    PARMBLK parm;
    aes_user_draw_t draw;

    if (tree == NULL || obj == NULL) {
        return;
    }

    user = (USERBLK *) (intptr_t) _aes_resolve_spec(obj);
    if (user == NULL || user->ab_code == 0) {
        return;
    }

    memset(&parm, 0, sizeof(parm));
    parm.pb_tree = (LONG) (intptr_t) tree;
    parm.pb_obj = object;
    parm.pb_prevstate = obj->ob_state;
    parm.pb_currstate = obj->ob_state;
    parm.pb_x = abs_x;
    parm.pb_y = abs_y;
    parm.pb_w = obj->ob_width;
    parm.pb_h = obj->ob_height;
    parm.pb_xc = clip[0];
    parm.pb_yc = clip[1];
    parm.pb_wc = (WORD) (clip[2] - clip[0] + 1);
    parm.pb_hc = (WORD) (clip[3] - clip[1] + 1);
    parm.pb_parm = user->ab_parm;

    draw = (aes_user_draw_t) (intptr_t) user->ab_code;
    (void) draw((LONG) (intptr_t) &parm);
}

/*
 * Draws one AES object node according to its type and state.
 */
static void _aes_draw_object(const OBJECT *tree,
                             WORD object,
                             WORD abs_x,
                             WORD abs_y,
                             WORD clip[4])
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
    int checked_menu_item;

    if (_aes.vdi_ready == 0 || tree == NULL || object < 0) {
        return;
    }

    obj = &tree[object];
    parent = _aes_find_parent((OBJECT *) tree, object);
    spec = _aes_resolve_spec(obj);
    checked_menu_item = (obj->ob_type == G_STRING &&
        (obj->ob_state & CHECKED) != 0u) ? 1 : 0;
    active = ((obj->ob_state & SELECTED) != 0u) ? 1 : 0;
    if (obj->ob_type == G_BUTTON && (obj->ob_state & CHECKED) != 0u) {
        active = 1;
    }
    fill_color = (active != 0) ?
        _aes_dark_color() : _aes_light_color();
    border_color = _aes_dark_color();
    inner_border_color = (active != 0) ?
        _aes_light_color() : _aes_dark_color();
    rect[0] = abs_x;
    rect[1] = abs_y;
    rect[2] = (WORD) (rect[0] + obj->ob_width - 1);
    rect[3] = (WORD) (rect[1] + obj->ob_height - 1);

    switch (obj->ob_type) {
    case G_BOX:
    case G_BOXCHAR:
        if (object == ROOT && _aes_is_dialog_root_tree(tree) != 0) {
            vsf_color(_aes.vdi_handle, _aes_light_color());
            v_bar(_aes.vdi_handle, rect);
            _aes_draw_dialog_frame(rect);
        } else if (_aes_is_menu_bar_object(tree, object) != 0) {
            vsf_color(_aes.vdi_handle, _aes_light_color());
            v_bar(_aes.vdi_handle, rect);
            _aes_draw_hline(rect[0], rect[2], rect[3], border_color);
        } else if (object == ROOT && rect[0] == 0 &&
            rect[2] >= _aes.work_out[0] &&
            rect[3] >= (WORD) (_aes.work_out[1] / 2)) {
            _aes_fill_checker_rect(rect[0], rect[1], rect[2], rect[3]);
        } else {
            vsf_color(_aes.vdi_handle, fill_color);
            v_bar(_aes.vdi_handle, rect);
        }
        if (object != ROOT) {
            if (_aes_is_menu_bar_object(tree, object) == 0) {
                if (_aes_is_dialog_frame_object(tree, object, parent) != 0) {
                    _aes_draw_dialog_frame(rect);
                } else {
                    _aes_draw_hline(rect[0], rect[2], rect[1], border_color);
                    _aes_draw_hline(rect[0], rect[2], rect[3], border_color);
                    _aes_draw_vline(rect[0], rect[1], rect[3], border_color);
                    _aes_draw_vline(rect[2], rect[1], rect[3], border_color);
                }
            }
        }
        break;
    case G_IBOX:
        if (_aes_is_dialog_frame_object(tree, object, parent) != 0) {
            vsf_color(_aes.vdi_handle, _aes_light_color());
            v_bar(_aes.vdi_handle, rect);
            _aes_draw_dialog_frame(rect);
        }
        break;
    case G_BUTTON:
        vsf_color(_aes.vdi_handle, fill_color);
        v_bar(_aes.vdi_handle, rect);
        _aes_draw_button_frame(rect, border_color, inner_border_color,
            (obj->ob_flags & DEFAULT) != 0u ? 1 : 0);
        break;
    case G_TEXT:
    case G_BOXTEXT:
    case G_FTEXT:
    case G_FBOXTEXT:
        _aes_draw_ted_object(tree, object, obj,
            (const TEDINFO *) (intptr_t) spec, rect);
        return;
    case G_ICON:
        _aes_draw_icon_object(obj, (const ICONBLK *) (intptr_t) spec,
            abs_x, abs_y, rect);
        return;
    case G_USERDEF:
        _aes_draw_user_object(tree, object, obj, abs_x, abs_y, clip);
        return;
    default:
        break;
    }

    _aes_draw_trace("obj type=%u rect=%d,%d-%d,%d spec=%p flags=%u state=%u",
        obj->ob_type, rect[0], rect[1], rect[2], rect[3],
        (const void *) (intptr_t) spec, obj->ob_flags, obj->ob_state);

    if (obj->ob_type == G_TITLE) {
        if (rect[1] + 1 <= rect[3] - 1) {
            _aes_fill_rect(rect[0], (WORD) (rect[1] + 1), rect[2],
                (WORD) (rect[3] - 1), _aes_light_color());
        }
    } else if (obj->ob_type == G_STRING && tree == _aes.menu_tree &&
        parent != NIL &&
        (tree[parent].ob_type == G_BOX || tree[parent].ob_type == G_IBOX) &&
        _aes_is_menu_bar_object(tree, parent) == 0) {
        WORD fill_right = (WORD) (rect[2] - 1);

        if (fill_right < rect[0]) {
            fill_right = rect[0];
        }
        _aes_fill_rect(rect[0], rect[1], fill_right, rect[3],
            (active != 0) ? _aes_dark_color() : _aes_light_color());
    } else if ((obj->ob_type == G_STRING || obj->ob_type == G_TITLE) &&
        active != 0) {
        vsf_color(_aes.vdi_handle, fill_color);
        v_bar(_aes.vdi_handle, rect);
    }

    text_width = 0;
    text_height = _vdi_font_text_height();
    text_ascent = _vdi_font_ascent();
    text_x = (WORD) (rect[0] + 2);
    if (obj->ob_type == G_TITLE) {
        text_y = (WORD) (rect[1] + 3 + text_ascent);
    } else {
        text_y = (WORD) (rect[1] +
            ((obj->ob_height - text_height) > 0 ?
            (obj->ob_height - text_height) / 2 : 0) + text_ascent);
    }
    if (obj->ob_type == G_TITLE) {
        text_color = _aes_dark_color();
    } else {
        text_color = (active != 0) ?
            _aes_light_color() : _aes_dark_color();
    }
    if ((obj->ob_type == G_STRING || obj->ob_type == G_TITLE ||
         obj->ob_type == G_BUTTON) && spec != 0) {
        const char *text = (const char *) (intptr_t) spec;
        int has_shortcut = 0;

        if (obj->ob_type == G_STRING && _aes_menu_is_separator_text(text)) {
            WORD dash_x = (WORD) (rect[0] + 2);
            WORD dash_y = text_y;
            WORD dash_width = _vdi_char_cell_width('-');
            WORD dash_count;
            char dash_buf[128];
            WORD i;

            if (dash_width <= 0) {
                dash_width = AES_CHAR_WIDTH;
            }
            dash_count = (WORD) ((rect[2] - rect[0] - 3) / dash_width);
            if (dash_count < 1) {
                dash_count = 1;
            }
            if ((size_t) dash_count >= sizeof(dash_buf)) {
                dash_count = (WORD) (sizeof(dash_buf) - 1u);
            }
            for (i = 0; i < dash_count; ++i) {
                dash_buf[i] = '-';
            }
            dash_buf[dash_count] = '\0';
            _aes_draw_text((WORD) (dash_x + 1), (WORD) (dash_y + 1),
                _aes_light_color(), dash_buf);
            _aes_draw_text(dash_x, dash_y, _aes_dark_color(), dash_buf);
            return;
        }

        if (obj->ob_type == G_STRING) {
            has_shortcut = _aes_menu_split_shortcut(text, shortcut_label,
                sizeof(shortcut_label), shortcut_text,
                sizeof(shortcut_text));
            if (has_shortcut != 0) {
                text = shortcut_label;
            }
        }

        text_width = (WORD) _vdi_string_width(text);
        if ((obj->ob_type == G_BUTTON || obj->ob_type == G_TITLE) &&
            text_width <= obj->ob_width) {
            text_x = (WORD) (rect[0] + (obj->ob_width - text_width) / 2);
        }
        if (checked_menu_item != 0) {
            _aes_draw_text((WORD) (rect[0] + 2), text_y, text_color, "\010");
        }
        _aes_draw_text(text_x, text_y, text_color, text);
        if (has_shortcut != 0 && shortcut_text[0] != '\0') {
            WORD shortcut_width = (WORD) _vdi_string_width(shortcut_text);
            WORD shortcut_x = (WORD) (rect[2] - shortcut_width - 2);

            if (shortcut_x > text_x) {
                _aes_draw_text(shortcut_x, text_y, text_color,
                    shortcut_text);
            }
        }
        if (obj->ob_type == G_TITLE && active != 0 &&
            rect[1] + 1 <= rect[3] - 1) {
            _aes_invert_rect(rect[0], (WORD) (rect[1] + 1), rect[2],
                (WORD) (rect[3] - 1));
        }
    }
}

/*
 * Recursively draws an object subtree while respecting `HIDETREE`.
 */
void _aes_draw_tree_recursive(const OBJECT *tree,
                              WORD object,
                              WORD parent_x,
                              WORD parent_y,
                              WORD depth,
                              WORD clip[4])
{
    WORD abs_x;
    WORD abs_y;
    WORD child_clip[4];
    WORD clip_on = 0;

    if (tree == NULL || object < 0) {
        return;
    }
    if ((tree[object].ob_flags & HIDETREE) != 0u) {
        return;
    }

    abs_x = (WORD) (parent_x + tree[object].ob_x);
    abs_y = (WORD) (parent_y + tree[object].ob_y);
    if (clip != NULL) {
        vs_clip(_aes.vdi_handle, 1, clip);
        clip_on = 1;
    }
    _aes_draw_object(tree, object, abs_x, abs_y, clip);
    if (clip_on != 0) {
        vs_clip(_aes.vdi_handle, 0, clip);
    }

    if (depth == 0 || tree[object].ob_head == NIL) {
        return;
    }

    {
        WORD child = tree[object].ob_head;

        memcpy(child_clip, clip, sizeof(child_clip));
        if (object == ROOT && _aes_is_dialog_root_tree(tree) != 0) {
            WORD inner_clip[4];

            inner_clip[0] = (WORD) (abs_x + 5);
            inner_clip[1] = (WORD) (abs_y + 5);
            inner_clip[2] = (WORD) (abs_x + tree[object].ob_width - 7);
            inner_clip[3] = (WORD) (abs_y + tree[object].ob_height - 7);
            if (inner_clip[0] <= inner_clip[2] &&
                inner_clip[1] <= inner_clip[3]) {
                child_clip[0] = _aes_max_word(child_clip[0], inner_clip[0]);
                child_clip[1] = _aes_max_word(child_clip[1], inner_clip[1]);
                child_clip[2] = _aes_min_word(child_clip[2], inner_clip[2]);
                child_clip[3] = _aes_min_word(child_clip[3], inner_clip[3]);
            }
        }

        while (child != NIL) {
            WORD next = tree[child].ob_next;
            WORD *next_clip = child_clip;

            if (object == ROOT && _aes_is_dialog_frame_object(tree, child,
                    object) != 0) {
                next_clip = clip;
            }

            _aes_draw_tree_recursive(tree, child, abs_x, abs_y,
                (depth > 0) ? (WORD) (depth - 1) : depth, next_clip);
            if (child == tree[object].ob_tail || next == object ||
                next == NIL) {
                break;
            }
            child = next;
        }
    }
}

/*
 * Recursively hit-tests an object subtree and returns the deepest hit.
 */
WORD _aes_find_in_subtree(OBJECT *tree,
                          WORD object,
                          WORD parent_x,
                          WORD parent_y,
                          WORD depth,
                          WORD mx,
                          WORD my)
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
            WORD child_hit = _aes_find_in_subtree(tree, child, abs_x, abs_y,
                (depth > 0) ? (WORD) (depth - 1) : depth, mx, my);

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

/*
 * Loads an entire file into a freshly allocated memory buffer.
 */
int _aes_load_file(const char *filename, void **data_out, size_t *size_out)
{
    int fd;
    int32_t read_size;
    size_t capacity = 4096u;
    size_t used = 0;
    char *buffer;

    if (filename == NULL || data_out == NULL || size_out == NULL) {
        return 0;
    }

    fd = gem_os_open_read(filename);
    if (fd < 0) {
        return 0;
    }

    buffer = gem_os_alloc(capacity);
    if (buffer == NULL) {
        (void) gem_os_close(fd);
        return 0;
    }

    FOREVER {
        if (used == capacity) {
            size_t new_capacity = capacity * 2u;
            char *new_buffer = gem_os_alloc(new_capacity);

            if (new_buffer == NULL) {
                gem_os_free(buffer);
                (void) gem_os_close(fd);
                return 0;
            }
            memcpy(new_buffer, buffer, used);
            gem_os_free(buffer);
            buffer = new_buffer;
            capacity = new_capacity;
        }

        read_size = gem_os_read(fd, buffer + used,
            (uint32_t) (capacity - used));
        if (read_size < 0) {
            gem_os_free(buffer);
            (void) gem_os_close(fd);
            return 0;
        }
        if (read_size == 0) {
            break;
        }
        used += (size_t) read_size;
    }

    (void) gem_os_close(fd);
    *data_out = buffer;
    *size_out = used;
    return 1;
}

/*
 * Converts one ASCII path or filename to a lowercase copy.
 */
static void _aes_ascii_lower(const char *source,
                             char *target,
                             size_t target_size)
{
    size_t i;

    if (target == NULL || target_size == 0u) {
        return;
    }

    if (source == NULL) {
        target[0] = '\0';
        return;
    }

    for (i = 0; source[i] != '\0' && i + 1u < target_size; ++i) {
        char ch = source[i];

        if (ch >= 'A' && ch <= 'Z') {
            ch = (char) (ch - 'A' + 'a');
        }
        target[i] = ch;
    }
    target[i] = '\0';
}

/*
 * Searches the hosted desktop resource locations for a readable file
 * and returns the first resolved path.
 */
int _aes_try_resolve_path(const char *filename,
                          char *resolved,
                          size_t resolved_size)
{
    static const char *search_dirs[] = {
        "",
        "bin/",
        "src/desktop/",
        "src/desktop/resource/",
        "src/desktop/icons/"
    };
    char lowercase[260];
    size_t i;

    if (filename == NULL || resolved == NULL || resolved_size == 0u) {
        return 0;
    }

    _aes_ascii_lower(filename, lowercase, sizeof(lowercase));

    for (i = 0; i < sizeof(search_dirs) / sizeof(search_dirs[0]); ++i) {
        const char *dir = search_dirs[i];
        int rc;
        int fd;

        rc = snprintf(resolved, resolved_size, "%s%s", dir, filename);
        if (rc > 0 && (size_t) rc < resolved_size) {
            fd = gem_os_open_read(resolved);
            if (fd >= 0) {
                (void) gem_os_close(fd);
                return 1;
            }
        }

        rc = snprintf(resolved, resolved_size, "%s%s", dir, lowercase);
        if (rc > 0 && (size_t) rc < resolved_size) {
            fd = gem_os_open_read(resolved);

            if (fd >= 0) {
                (void) gem_os_close(fd);
                return 1;
            }
        }
    }

    return 0;
}
