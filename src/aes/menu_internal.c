/*
 * Implements the hosted AES menu tree model shared by layout, tracking and
 * drawing: container and child navigation, popup hiding, whole-tree
 * redraw, application menu switching, subtree bounds and the saved screen
 * regions restored when a popup closes.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (C) 2026 tomaz stih
 */

#include "menu_private.h"
#include "system_menu.h"

#include "../vdi/vdi_internal.h"

#include "platform/os.h"

#include <string.h>

static WORD aes_menu_last_reachable_object(const OBJECT *tree, WORD object,
                                           uint8_t visited[256]);
static void aes_menu_expand_saved_rect(OBJECT *tree, WORD object, GRECT *rect);
static void aes_menu_free_saved_pixels(void);
static void aes_menu_restore_saved_region(void);
static int aes_menu_save_region(const GRECT *rect);

static int aes_menu_redraw_in_progress;

WORD aes_menu_popup_container(OBJECT *tree)
{
    if (tree == NULL) {
        return NIL;
    }

    if (aes_state.menu_popup_root_direct != 0) {
        return ROOT;
    }

    return tree[ROOT].ob_tail;
}

WORD aes_menu_first_popup_child(OBJECT *tree, WORD popup_parent)
{
    WORD child;

    if (tree == NULL || popup_parent == NIL) {
        return NIL;
    }

    child = tree[popup_parent].ob_head;
    if (child == NIL) {
        return NIL;
    }

    if (popup_parent == ROOT && aes_state.menu_popup_root_direct != 0) {
        child = tree[child].ob_next;
        if (child == ROOT) {
            return NIL;
        }
    }

    return child;
}

WORD aes_menu_last_object(OBJECT *tree)
{
    uint8_t visited[256] = {0};

    if (tree == NULL) {
        return ROOT;
    }

    return aes_menu_last_reachable_object(tree, ROOT, visited);
}

static WORD aes_menu_last_reachable_object(const OBJECT *tree, WORD object,
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

    while (child != NIL && child != object) {
        WORD branch_last = aes_menu_last_reachable_object(tree, child, visited);

        if (branch_last > last) {
            last = branch_last;
        }
        if (child == tree[object].ob_tail || tree[child].ob_next == object ||
            tree[child].ob_next == ROOT || tree[child].ob_next == NIL) {
            break;
        }
        child = tree[child].ob_next;
    }

    return last;
}

WORD aes_menu_title_container(OBJECT *tree)
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

static WORD aes_menu_nth_child(OBJECT *tree, WORD parent, WORD index)
{
    WORD child;
    WORD current_index = 0;

    if (tree == NULL || parent == NIL) {
        return NIL;
    }

    child = aes_menu_first_popup_child(tree, parent);
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

static WORD aes_menu_index_for_title(OBJECT *tree, WORD title)
{
    WORD parent = aes_menu_title_container(tree);
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

WORD aes_menu_popup_for_title(OBJECT *tree, WORD title)
{
    WORD popup_parent = aes_menu_popup_container(tree);
    WORD title_index = aes_menu_index_for_title(tree, title);

    if (title_index < 0 || popup_parent == NIL) {
        return NIL;
    }

    return aes_menu_nth_child(tree, popup_parent, title_index);
}

void aes_menu_hide_popups(OBJECT *tree)
{
    WORD popup_parent;
    WORD child;

    if (tree == NULL) {
        return;
    }

    popup_parent = aes_menu_popup_container(tree);
    if (popup_parent == NIL) {
        return;
    }

    child = aes_menu_first_popup_child(tree, popup_parent);
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

void aes_menu_redraw_tree(OBJECT *tree)
{
    WORD bar;
    WORD popup_parent;
    WORD popup;
    WORD visible_popup_count = 0;
    GRECT redraw_rect;

    if (tree == NULL || aes_ensure_vdi() == 0) {
        return;
    }

    if (aes_menu_redraw_in_progress != 0) {
        return;
    }

    aes_menu_redraw_in_progress = 1;

    vdi_begin_update();
    v_hide_c(aes_state.vdi_handle);

    aes_menu_restore_saved_region();

    bar = tree[ROOT].ob_head;
    popup_parent = aes_menu_popup_container(tree);
    popup = aes_menu_first_popup_child(tree, popup_parent);
    aes_menu_free_saved_pixels();

    if (bar != NIL && aes_menu_subtree_rect(tree, bar, &redraw_rect) != 0) {
        aes_menu_expand_saved_rect(tree, bar, &redraw_rect);
        (void)aes_menu_save_region(&redraw_rect);
    }

    while (popup != NIL) {
        WORD next = tree[popup].ob_next;

        if ((tree[popup].ob_flags & HIDETREE) == 0u &&
            aes_menu_subtree_rect(tree, popup, &redraw_rect) != 0) {
            ++visible_popup_count;
            aes_menu_expand_saved_rect(tree, popup, &redraw_rect);
            (void)aes_menu_save_region(&redraw_rect);
        }

        if (popup == tree[popup_parent].ob_tail || next == popup_parent ||
            next == NIL || next == ROOT) {
            break;
        }
        popup = next;
    }

    if (bar != NIL && aes_menu_subtree_rect(tree, bar, &redraw_rect) != 0) {
        WORD fill[4];
        GRECT seam;

        fill[0] = redraw_rect.g_x;
        fill[1] = redraw_rect.g_y;
        fill[2] = (WORD)(redraw_rect.g_x + redraw_rect.g_w - 1);
        fill[3] = (WORD)(redraw_rect.g_y + redraw_rect.g_h - 1);
        vsf_color(aes_state.vdi_handle, aes_light_color());
        v_bar(aes_state.vdi_handle, fill);
        objc_draw(tree, bar, MAX_DEPTH, redraw_rect.g_x, redraw_rect.g_y,
                  redraw_rect.g_w, redraw_rect.g_h);

        /*
         * Repaint the first row below the menu bar from the underlying
         * desktop/windows before popups are drawn. This clears any stale
         * seam pixels left from previous popup geometry changes while
         * still allowing the active popup to paint over its own span.
         */
        aes_set_rect(&seam, redraw_rect.g_x,
                     (WORD)(redraw_rect.g_y + redraw_rect.g_h), redraw_rect.g_w,
                     1);
        aes_redraw_region(&seam);
    }

    popup = aes_menu_first_popup_child(tree, popup_parent);
    while (popup != NIL) {
        WORD next = tree[popup].ob_next;

        if ((tree[popup].ob_flags & HIDETREE) == 0u &&
            aes_menu_subtree_rect(tree, popup, &redraw_rect) != 0) {
            WORD fill[4];

            fill[0] = redraw_rect.g_x;
            fill[1] = redraw_rect.g_y;
            fill[2] = (WORD)(redraw_rect.g_x + redraw_rect.g_w - 1);
            fill[3] = (WORD)(redraw_rect.g_y + redraw_rect.g_h - 1);
            vsf_color(aes_state.vdi_handle, aes_light_color());
            v_bar(aes_state.vdi_handle, fill);
            objc_draw(tree, popup, MAX_DEPTH, redraw_rect.g_x, redraw_rect.g_y,
                      redraw_rect.g_w, redraw_rect.g_h);
        }

        if (popup == tree[popup_parent].ob_tail || next == popup_parent ||
            next == NIL || next == ROOT) {
            break;
        }
        popup = next;
    }

    if (visible_popup_count == 0 && bar != NIL &&
        aes_menu_subtree_rect(tree, bar, &redraw_rect) != 0) {
        GRECT repair;

        repair.g_x = 0;
        repair.g_y = (WORD)(redraw_rect.g_y + redraw_rect.g_h - 1);
        repair.g_w = (WORD)(aes_state.work_out[0] + 1);
        repair.g_h = 2;
        if (repair.g_y < 0) {
            repair.g_y = 0;
        }
        if (repair.g_y <= aes_state.work_out[1]) {
            if (repair.g_y + repair.g_h - 1 > aes_state.work_out[1]) {
                repair.g_h = (WORD)(aes_state.work_out[1] - repair.g_y + 1);
            }
            if (repair.g_h > 0) {
                aes_redraw_region(&repair);
            }
        }
    }

    aes_system_menu_draw();
    v_show_c(aes_state.vdi_handle, 1);
    vdi_end_update();
    aes_menu_redraw_in_progress = 0;
}

void aes_menu_clear_saved_region(void)
{
    vdi_begin_update();
    v_hide_c(aes_state.vdi_handle);
    aes_menu_restore_saved_region();
    v_show_c(aes_state.vdi_handle, 1);
    vdi_end_update();
}

void aes_menu_switch_to_app(WORD app_id)
{
    aes_app_t *app = aes_find_app_by_id(app_id);
    OBJECT *tree;
    WORD visible;

    /* The bar reflects the active application only: one without a menu
     * shows an empty strip, never another application's titles. */
    aes_state.active_app_id = (app != NULL) ? app_id : 0;
    tree = (app != NULL && app->menu_visible != 0) ? app->menu_tree : NULL;
    visible = (tree != NULL) ? 1 : 0;

    if (tree == aes_state.menu_tree && visible == aes_state.menu_visible) {
        return;
    }

    if (aes_state.menu_visible != 0) {
        aes_menu_clear_saved_region();
    }

    aes_state.menu_tree = tree;
    aes_state.menu_visible = visible;
    aes_state.menu_owner_app_id = (tree != NULL) ? app_id : 0;

    if (tree == NULL) {
        aes_menu_draw_empty_bar();
    } else {
        /*
         * menu_bar() already normalized this tree's linkage once when
         * the owning app installed it. aes_menu_prepare_tree() is not
         * idempotent (its re-link guard only holds before ROOT.ob_tail
         * gets rewritten to the popup root), so calling it again here
         * on every focus switch corrupts popup lookups.
         */
        aes_menu_hide_popups(tree);
        aes_menu_redraw_tree(tree);
    }
}

void aes_menu_draw_empty_bar(void)
{
    WORD height = aes_menu_bar_height();
    WORD width;
    WORD fill[4];

    if (height <= 1 || aes_ensure_vdi() == 0) {
        return;
    }

    width = (WORD)(aes_state.work_out[0] + 1);
    fill[0] = 0;
    fill[1] = 0;
    fill[2] = (WORD)(width - 1);
    fill[3] = (WORD)(height - 1);
    vdi_begin_update();
    v_hide_c(aes_state.vdi_handle);
    vs_clip(aes_state.vdi_handle, 1, fill);
    vsf_color(aes_state.vdi_handle, aes_light_color());
    fill[3] = (WORD)(height - 2);
    v_bar(aes_state.vdi_handle, fill);
    /* The rule under the strip stays where the menu bar draws it. */
    fill[1] = fill[3] = (WORD)(height - 1);
    vsf_color(aes_state.vdi_handle, aes_dark_color());
    v_bar(aes_state.vdi_handle, fill);
    vsf_color(aes_state.vdi_handle, aes_light_color());
    fill[1] = 0;
    vs_clip(aes_state.vdi_handle, 0, fill);
    aes_system_menu_draw();
    v_show_c(aes_state.vdi_handle, 1);
    vdi_end_update();
}

int aes_menu_subtree_rect(OBJECT *tree, WORD object, GRECT *rect)
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

    last_object = aes_menu_last_object(tree);
    for (i = object; i <= last_object; ++i) {
        WORD abs_x;
        WORD abs_y;
        WORD right;
        WORD bottom;

        if ((tree[i].ob_flags & HIDETREE) != 0u) {
            continue;
        }
        if (i != object && aes_find_parent(tree, i) != object) {
            continue;
        }

        aes_object_extent(tree, i, &abs_x, &abs_y);
        right = (WORD)(abs_x + tree[i].ob_width - 1);
        bottom = (WORD)(abs_y + tree[i].ob_height - 1);

        if (!found) {
            min_x = abs_x;
            min_y = abs_y;
            max_x = right;
            max_y = bottom;
            found = 1;
        } else {
            min_x = aes_min_word(min_x, abs_x);
            min_y = aes_min_word(min_y, abs_y);
            max_x = aes_max_word(max_x, right);
            max_y = aes_max_word(max_y, bottom);
        }
    }

    if (!found) {
        return 0;
    }

    aes_set_rect(rect, min_x, min_y, (WORD)(max_x - min_x + 1),
                 (WORD)(max_y - min_y + 1));
    return 1;
}

static void aes_menu_expand_saved_rect(OBJECT *tree, WORD object, GRECT *rect)
{
    if (tree == NULL || rect == NULL || rect->g_w <= 0 || rect->g_h <= 0) {
        return;
    }

    if (object == tree[ROOT].ob_head) {
        rect->g_h = (WORD)(rect->g_h + 1);
    }
}

static void aes_menu_free_saved_pixels(void)
{
    WORD i;

    for (i = 0; i < AES_MAX_MENU_SAVED_REGIONS; ++i) {
        if (aes_state.menu_saved_pixels[i] != NULL) {
            gem_os_free(aes_state.menu_saved_pixels[i]);
            aes_state.menu_saved_pixels[i] = NULL;
        }
        aes_set_rect(&aes_state.menu_saved_rects[i], 0, 0, 0, 0);
    }
    aes_state.menu_saved_count = 0;
}

static void aes_menu_restore_saved_region(void)
{
    WORD i;

    if (aes_state.menu_saved_count <= 0) {
        aes_menu_free_saved_pixels();
        return;
    }

    for (i = 0; i < aes_state.menu_saved_count; ++i) {
        GRECT rect = aes_state.menu_saved_rects[i];
        uint8_t *pixels = aes_state.menu_saved_pixels[i];

        if (pixels == NULL || rect.g_w <= 0 || rect.g_h <= 0) {
            continue;
        }

        aes_restore_region_pixels(&rect, pixels);
    }

    aes_menu_free_saved_pixels();
}

static int aes_menu_save_region(const GRECT *rect)
{
    WORD x;
    WORD y;
    size_t count;
    size_t index = 0;
    uint8_t *pixels;

    if (rect == NULL || rect->g_w <= 0 || rect->g_h <= 0 ||
        aes_state.menu_saved_count >= AES_MAX_MENU_SAVED_REGIONS) {
        return 0;
    }

    count = (size_t)rect->g_w * (size_t)rect->g_h;
    pixels = (uint8_t *)gem_os_alloc(count);
    if (pixels == NULL) {
        return 0;
    }

    for (y = 0; y < rect->g_h; ++y) {
        for (x = 0; x < rect->g_w; ++x) {
            pixels[index++] = (uint8_t)vdi_get_screen_pixel(
                (WORD)(rect->g_x + x), (WORD)(rect->g_y + y));
        }
    }

    aes_state.menu_saved_pixels[aes_state.menu_saved_count] = pixels;
    aes_state.menu_saved_rects[aes_state.menu_saved_count] = *rect;
    ++aes_state.menu_saved_count;
    return 1;
}
