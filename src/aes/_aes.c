/*
 * Implements the private hosted AES helper layer, including runtime
 * state management, menu tracking, object drawing, hit testing, and
 * resource-file utility routines.
 *
 * MIT License (see: LICENSE)
 * Copyright (C) 2026 tomaz stih
 */

#include "_aes.h"

#include "platform/os.h"

#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

aes_state_t _aes;

static WORD _aes_find_parent(OBJECT *tree, WORD object);
static WORD _aes_window_left_border(const aes_window_t *window);
static WORD _aes_window_right_border(const aes_window_t *window);
static WORD _aes_window_bottom_border(const aes_window_t *window);
static WORD _aes_window_title_height(const aes_window_t *window);
static WORD _aes_light_color(void);
static WORD _aes_dark_color(void);
static int _aes_window_title_rect(const aes_window_t *window, GRECT *rect);
static int _aes_window_closer_rect(const aes_window_t *window, GRECT *rect);
static int _aes_window_fuller_rect(const aes_window_t *window, GRECT *rect);
static int _aes_window_sizer_rect(const aes_window_t *window, GRECT *rect);
static void _aes_draw_text(WORD x, WORD y, WORD color, const char *text);
static void _aes_fill_rect(WORD x0, WORD y0, WORD x1, WORD y1, WORD color);
static void _aes_fill_checker_rect(WORD x0, WORD y0, WORD x1, WORD y1);
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
static int _aes_rects_intersect(const GRECT *left, const GRECT *right);
static int _aes_intersect_rects(const GRECT *left, const GRECT *right,
    GRECT *out);
static WORD _aes_subtract_rect(const GRECT *source, const GRECT *cover,
    GRECT out[4]);
static void _aes_expand_window_damage_rect(const GRECT *src, GRECT *out);
static void _aes_redraw_region(const GRECT *dirty);
static void _aes_desktop_rect_local(GRECT *rect);

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

/*
 * Appends one formatted AES runtime trace line when `GEM_TRACE_AES` is
 * enabled in the environment.
 */
void _aes_trace(const char *fmt, ...)
{
    const char *trace = getenv("GEM_TRACE_AES");
    FILE *fp;
    va_list ap;

    if (trace == NULL || trace[0] == '\0') {
        return;
    }

    fp = fopen("/tmp/gem_aes_trace.log", "a");
    if (fp == NULL) {
        return;
    }

    va_start(ap, fmt);
    vfprintf(fp, fmt, ap);
    va_end(ap);
    fputc('\n', fp);
    fclose(fp);
}

/*
 * Appends one formatted object-drawing trace line when
 * `GEM_TRACE_DRAW` is enabled in the environment.
 */
static void _aes_draw_trace(const char *fmt, ...)
{
    const char *trace = getenv("GEM_TRACE_DRAW");
    FILE *fp;
    va_list ap;

    if (trace == NULL || trace[0] == '\0') {
        return;
    }

    fp = fopen("/tmp/gem_draw_trace.log", "a");
    if (fp == NULL) {
        return;
    }

    va_start(ap, fmt);
    vfprintf(fp, fmt, ap);
    va_end(ap);
    fputc('\n', fp);
    fclose(fp);
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

/*
 * Returns the menu popup-container object under the current menu tree.
 */
static WORD _aes_menu_popup_container(OBJECT *tree)
{
    if (tree == NULL) {
        return NIL;
    }

    return tree[ROOT].ob_tail;
}

/*
 * Returns the menu-title container object under the current menu tree.
 */
static WORD _aes_menu_title_container(OBJECT *tree)
{
    WORD root_child;

    if (tree == NULL) {
        return NIL;
    }

    root_child = tree[ROOT].ob_head;
    if (root_child == NIL) {
        return NIL;
    }
    return tree[root_child].ob_head;
}

/*
 * Returns the `index`th child object under `parent`.
 */
static WORD _aes_menu_nth_child(OBJECT *tree, WORD parent, WORD index)
{
    WORD child;
    WORD current_index = 0;

    if (tree == NULL || parent == NIL || tree[parent].ob_head == NIL) {
        return NIL;
    }

    child = tree[parent].ob_head;
    while (child != NIL) {
        WORD next = tree[child].ob_next;

        if (current_index == index) {
            return child;
        }
        ++current_index;
        if (child == tree[parent].ob_tail || next == parent || next == NIL) {
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

    child = tree[popup_parent].ob_head;
    while (child != NIL) {
        WORD next = tree[child].ob_next;

        tree[child].ob_flags |= HIDETREE;
        if (child == tree[popup_parent].ob_tail || next == popup_parent ||
            next == NIL) {
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
 * Redraws the full menu tree after a title or popup state change.
 */
static void _aes_menu_redraw(OBJECT *tree)
{
    WORD width;
    WORD height;
    WORD rect[4];

    if (tree == NULL || _aes_ensure_vdi() == 0) {
        return;
    }

    width = tree[ROOT].ob_width;
    height = tree[ROOT].ob_height;
    rect[0] = tree[ROOT].ob_x;
    rect[1] = tree[ROOT].ob_y;
    rect[2] = (WORD) (rect[0] + width - 1);
    rect[3] = (WORD) (rect[1] + height - 1);
    vsf_color(_aes.vdi_handle, _aes_light_color());
    v_bar(_aes.vdi_handle, rect);
    objc_draw(tree, ROOT, MAX_DEPTH, 0, 0, width, height);
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
    _aes_menu_redraw(tree);
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

                if (hover != NIL && hover != popup &&
                    tree[hover].ob_type == G_STRING &&
                    (tree[hover].ob_state & DISABLED) == 0u) {
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
                _aes_menu_redraw(tree);
            }
            continue;
        }

        if (evt.type == GEM_HID_MOUSE_BUTTON &&
            (evt.flags & GEM_HID_BUTTON_LEFT) == 0u) {
            WORD release_title = _aes_menu_hit_title(tree,
                (WORD) evt.x, (WORD) evt.y);
            WORD release_hit = objc_find(tree, popup, MAX_DEPTH,
                (WORD) evt.x, (WORD) evt.y);

            if (release_hit != NIL && release_hit != popup &&
                tree[release_hit].ob_type == G_STRING &&
                (tree[release_hit].ob_state & DISABLED) == 0u) {
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
            _aes_menu_set_title_selected(tree, title, 0);
            _aes_menu_redraw(tree);

            if (item == NIL) {
                return 0;
            }

            memset(mepbuff, 0, sizeof(WORD) * 8u);
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
    return (window != NULL && window->kind != 0u) ? 2 : 0;
}

static WORD _aes_window_right_border(const aes_window_t *window)
{
    if (window != NULL && (window->kind & SIZER) != 0u) {
        return (WORD) (_aes_window_title_height(window) - 1);
    }
    return _aes_window_left_border(window);
}

static WORD _aes_window_bottom_border(const aes_window_t *window)
{
    if (window != NULL && (window->kind & SIZER) != 0u) {
        return (WORD) (_aes_window_title_height(window) - 1);
    }
    return (window != NULL && window->kind != 0u) ? 2 : 0;
}

static WORD _aes_window_title_height(const aes_window_t *window)
{
    WORD text_height;

    if (window == NULL || window->kind == 0u) {
        return 0;
    }
    if ((window->kind & (NAME | CLOSER | FULLER | MOVER)) == 0u) {
        return 2;
    }

    text_height = _aes.vdi_ready ? _vdi_font_text_height() : AES_CHAR_HEIGHT;
    return (WORD) (text_height + 10);
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
        side,
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
    if (rect == NULL) {
        return;
    }

    if (_aes.vdi_ready == 0) {
        _aes_set_rect(rect, 0, 0, 0, 0);
        return;
    }

    _aes_set_rect(rect, 0, 0, (WORD) (_aes.work_out[0] + 1),
        (WORD) (_aes.work_out[1] + 1));
}

static int _aes_rects_intersect(const GRECT *left, const GRECT *right)
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

static int _aes_intersect_rects(const GRECT *left, const GRECT *right,
                                GRECT *out)
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

static WORD _aes_subtract_rect(const GRECT *source, const GRECT *cover,
                               GRECT out[4])
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

static void _aes_redraw_region(const GRECT *dirty)
{
    uint32_t target_z;
    WORD clip[4];
    GRECT desktop;

    if (dirty == NULL || dirty->g_w <= 0 || dirty->g_h <= 0 ||
        _aes.vdi_ready == 0) {
        return;
    }

    _aes_desktop_rect_local(&desktop);
    if (_aes_rects_intersect(&desktop, dirty) == 0) {
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

static void _aes_fill_checker_rect(WORD x0, WORD y0, WORD x1, WORD y1)
{
    WORD x;
    WORD y;
    WORD dark_pixel = (_aes_dark_color() == WHITE) ? 1 : 0;

    if (x0 > x1 || y0 > y1) {
        return;
    }

    _aes_fill_rect(x0, y0, x1, y1, _aes_light_color());
    for (y = y0; y <= y1; ++y) {
        for (x = (WORD) (x0 + ((x0 + y) & 1)); x <= x1;
             x = (WORD) (x + 2)) {
            _vdi_plot_pixel(x, y, dark_pixel);
        }
    }
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
    text_x = (WORD) (x0 + ((x1 - x0 + 1) - text_width) / 2);
    text_y = (WORD) (y0 + ((y1 - y0 + 1) - text_height) / 2 +
        _vdi_font_ascent());
    _aes_draw_text(text_x, text_y, _aes_dark_color(), text);

    if (previous_font != 0 && previous_font != system_font) {
        (void) vst_font(_aes.vdi_handle, previous_font);
    }
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
        title_left = (WORD) (close_sep[0] + 2);
    }

    if (has_fuller != 0) {
        WORD fuller_sep[4];
        WORD fuller_left = (WORD) (fuller_rect.g_x + 1);

        _aes_fill_rect(fuller_left, fuller_rect.g_y,
            (WORD) (fuller_rect.g_x + fuller_rect.g_w - 1),
            (WORD) (fuller_rect.g_y + fuller_rect.g_h - 1),
            _aes_light_color());
        fuller_sep[0] = fuller_left;
        fuller_sep[1] = fuller_rect.g_y;
        fuller_sep[2] = fuller_sep[0];
        fuller_sep[3] = (WORD) (fuller_rect.g_y + fuller_rect.g_h - 1);
        vsl_color(_aes.vdi_handle, _aes_dark_color());
        v_pline(_aes.vdi_handle, 2, fuller_sep);
        _aes_draw_window_icon_offset(fuller_left, fuller_rect.g_y,
            (WORD) (fuller_rect.g_x + fuller_rect.g_w - 1),
            (WORD) (fuller_rect.g_y + fuller_rect.g_h - 1), 7, 1, 0);
        title_right = (WORD) (fuller_sep[0] - 2);
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
        if (title_box_left <= title_box_right) {
            WORD left_fill_right = (WORD) (title_box_left - 2);
            WORD right_fill_left = (WORD) (title_box_right + 2);

            if (title_left <= left_fill_right) {
                _aes_fill_checker_rect(title_left, title_top,
                    left_fill_right, title_bottom);
            }
            if (right_fill_left <= title_right) {
                _aes_fill_checker_rect(right_fill_left, title_top,
                    title_right, title_bottom);
            }
        } else {
            _aes_fill_checker_rect(title_left, title_top, title_right,
                title_bottom);
        }
    }

    if (title_box_left <= title_box_right) {
        _aes_fill_rect(title_box_left, title_top, title_box_right,
            title_bottom, _aes_light_color());
        title_y = (WORD) (title_top +
            (title_bottom - title_top - text_height) / 2 +
            _vdi_font_ascent());
        _aes_draw_text(title_x, title_y, _aes_dark_color(), window->name);
    }

    if (previous_font != 0 && previous_font != system_font) {
        (void) vst_font(_aes.vdi_handle, previous_font);
    }
}

void _aes_draw_window_frame(const aes_window_t *window)
{
    WORD outer_box[10];
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

    if (tree == NULL || object <= ROOT) {
        return NIL;
    }

    for (parent = ROOT; parent < 1024; ++parent) {
        WORD child;

        if (parent == object || tree[parent].ob_head == NIL) {
            if ((tree[parent].ob_flags & LASTOB) != 0u && parent > object) {
                break;
            }
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
        if ((tree[parent].ob_flags & LASTOB) != 0u && parent > object) {
            break;
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
static void _aes_draw_ted_object(const OBJECT *obj,
                                 const TEDINFO *ted,
                                 const WORD rect[4])
{
    const char *text;
    WORD box[4];
    WORD text_x;
    WORD text_y;
    WORD fill_color;
    WORD border_color;
    WORD text_color;

    if (obj == NULL || ted == NULL) {
        return;
    }

    text = (const char *) (intptr_t) ted->te_ptext;
    text_x = (WORD) (rect[0] + 2);
    text_y = (WORD) (rect[1] + AES_CHAR_HEIGHT);
    text_color = ((obj->ob_state & SELECTED) != 0u) ?
        _aes_light_color() : _aes_dark_color();
    fill_color = ((obj->ob_state & SELECTED) != 0u) ?
        _aes_dark_color() : _aes_light_color();
    border_color = _aes_dark_color();

    if (obj->ob_type == G_BOXTEXT || obj->ob_type == G_FBOXTEXT) {
        memcpy(box, rect, sizeof(box));
        vsf_color(_aes.vdi_handle, fill_color);
        v_bar(_aes.vdi_handle, box);
        vsl_color(_aes.vdi_handle, border_color);
        v_rbox(_aes.vdi_handle, box);
    }

    _aes_draw_text(text_x, text_y, text_color, text);
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
    const OBJECT *obj;
    LONG spec;
    WORD rect[4];
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

    if (_aes.vdi_ready == 0 || tree == NULL || object < 0) {
        return;
    }

    obj = &tree[object];
    spec = _aes_resolve_spec(obj);
    active = ((obj->ob_state & (SELECTED | CHECKED)) != 0u) ? 1 : 0;
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
        if (object == ROOT && rect[0] == 0 && rect[2] >= _aes.work_out[0] &&
            rect[3] >= (WORD) (_aes.work_out[1] / 2)) {
            _aes_fill_checker_rect(rect[0], rect[1], rect[2], rect[3]);
        } else {
            vsf_color(_aes.vdi_handle, fill_color);
            v_bar(_aes.vdi_handle, rect);
        }
        if (object != ROOT) {
            vsl_color(_aes.vdi_handle, border_color);
            v_rbox(_aes.vdi_handle, rect);
        }
        break;
    case G_IBOX:
        break;
    case G_BUTTON:
        vsf_color(_aes.vdi_handle, fill_color);
        v_bar(_aes.vdi_handle, rect);
        vsl_color(_aes.vdi_handle, border_color);
        v_rbox(_aes.vdi_handle, rect);
        if ((obj->ob_flags & DEFAULT) != 0u &&
            rect[0] + 2 <= rect[2] - 2 &&
            rect[1] + 2 <= rect[3] - 2) {
            WORD inner_rect[4];

            inner_rect[0] = (WORD) (rect[0] + 1);
            inner_rect[1] = (WORD) (rect[1] + 1);
            inner_rect[2] = (WORD) (rect[2] - 1);
            inner_rect[3] = (WORD) (rect[3] - 1);
            vsl_color(_aes.vdi_handle, inner_border_color);
            v_rbox(_aes.vdi_handle, inner_rect);
        }
        break;
    case G_TEXT:
    case G_BOXTEXT:
    case G_FTEXT:
    case G_FBOXTEXT:
        _aes_draw_ted_object(obj, (const TEDINFO *) (intptr_t) spec, rect);
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

    if ((obj->ob_type == G_STRING || obj->ob_type == G_TITLE) &&
        active != 0) {
        vsf_color(_aes.vdi_handle, fill_color);
        v_bar(_aes.vdi_handle, rect);
    }

    text_width = 0;
    text_height = _vdi_font_text_height();
    text_ascent = _vdi_font_ascent();
    text_x = (WORD) (rect[0] + 2);
    text_y = (WORD) (rect[1] +
        ((obj->ob_height - text_height) > 0 ?
        (obj->ob_height - text_height) / 2 : 0) + text_ascent);
    text_color = (active != 0) ?
        _aes_light_color() : _aes_dark_color();
    if ((obj->ob_type == G_STRING || obj->ob_type == G_TITLE ||
         obj->ob_type == G_BUTTON) && spec != 0) {
        text_width = (WORD) _vdi_string_width((const char *) (intptr_t) spec);
        if ((obj->ob_type == G_BUTTON || obj->ob_type == G_TITLE) &&
            text_width < obj->ob_width) {
            text_x = (WORD) (rect[0] + (obj->ob_width - text_width) / 2);
        }
        _aes_draw_text(text_x, text_y, text_color,
            (const char *) (intptr_t) spec);
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

    if (tree == NULL || object < 0) {
        return;
    }
    if ((tree[object].ob_flags & HIDETREE) != 0u) {
        return;
    }

    abs_x = (WORD) (parent_x + tree[object].ob_x);
    abs_y = (WORD) (parent_y + tree[object].ob_y);
    _aes_draw_object(tree, object, abs_x, abs_y, clip);

    if (depth == 0 || tree[object].ob_head == NIL) {
        return;
    }

    {
        WORD child = tree[object].ob_head;

        while (child != NIL) {
            WORD next = tree[child].ob_next;

            _aes_draw_tree_recursive(tree, child, abs_x, abs_y,
                (depth > 0) ? (WORD) (depth - 1) : depth, clip);
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
