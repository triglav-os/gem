/*
 * Implements the private hosted AES core state, tracing,
 * shared geometry helpers, and file/path utility routines.
 *
 * MIT License (see: LICENSE)
 * Copyright (C) 2026 tomaz stih
 */

#include "_aes.h"

#include "../vdi/_internal.h"

#include "platform/os.h"

#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

aes_state_t _aes;

__attribute__((weak)) int gem_builtin_rsrc_load(const char *filename);
__attribute__((weak)) int gem_builtin_rsrc_gaddr(WORD type,
                                                 WORD index,
                                                 void **addr);
__attribute__((weak)) void gem_builtin_rsrc_free(void);
extern WORD _vdi_select_system_mouse_form(WORD selector);
static void _aes_write_trace(const char *env_var, const char *logfile,
                             const char *fmt, va_list ap);
void _aes_trace(const char *fmt, ...);
void _aes_store_mouse_state(const gem_hid_event_t *evt);
void _aes_store_key_state(const gem_hid_event_t *evt);
static void _aes_update_hover_mouse_cursor(WORD x, WORD y);
WORD _aes_chrome_height(void);
WORD _aes_menu_chrome_height(void);
WORD _aes_menu_bar_height(void);
WORD _aes_min_word(WORD left, WORD right);
WORD _aes_max_word(WORD left, WORD right);
void _aes_set_rect(GRECT *rect, WORD x, WORD y, WORD w, WORD h);
int _aes_point_in_rect(WORD x, WORD y, const GRECT *rect);
void _aes_reset_state(void);
int _aes_ensure_vdi(void);
aes_app_t *_aes_find_app_by_id(WORD id);
aes_window_t *_aes_find_window(WORD handle);
void _aes_desktop_rect(GRECT *rect);
int _aes_rects_intersect(const GRECT *left, const GRECT *right);
int _aes_intersect_rects(const GRECT *left, const GRECT *right, GRECT *out);
WORD _aes_subtract_rect(const GRECT *source, const GRECT *cover, GRECT out[4]);
WORD _aes_find_parent(OBJECT *tree, WORD object);
LONG _aes_resolve_spec(const OBJECT *obj);
int _aes_menu_is_separator_text(const char *text);
int _aes_menu_split_shortcut(const char *text,
                             char *label,
                             size_t label_size,
                             char *shortcut,
                             size_t shortcut_size);
WORD _aes_light_color(void);
WORD _aes_dark_color(void);
int _aes_save_region_pixels(const GRECT *rect, uint8_t **pixels_out);
void _aes_restore_region_pixels(const GRECT *rect, uint8_t *pixels);
int _aes_load_file(const char *filename, void **data_out, size_t *size_out);
static void _aes_ascii_lower(const char *source,
                             char *target,
                             size_t target_size);
int _aes_try_resolve_path(const char *filename,
                          char *resolved,
                          size_t resolved_size);

__attribute__((weak)) int gem_builtin_rsrc_load(const char *filename)
{
    (void) filename;
    return 0;
}

__attribute__((weak)) int gem_builtin_rsrc_gaddr(WORD type,
                                                 WORD index,
                                                 void **addr)
{
    (void) type;
    (void) index;
    (void) addr;
    return 0;
}

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

void _aes_store_mouse_state(const gem_hid_event_t *evt)
{
    if (evt == NULL) {
        return;
    }

    _vdi_set_mouse_state((WORD) evt->x, (WORD) evt->y, (WORD) evt->flags);
    _aes_update_hover_mouse_cursor((WORD) evt->x, (WORD) evt->y);
}

void _aes_store_key_state(const gem_hid_event_t *evt)
{
    if (evt == NULL || evt->type != GEM_HID_KEY) {
        return;
    }

    _aes.key_state = (WORD) evt->mod;
}

static void _aes_update_hover_mouse_cursor(WORD x, WORD y)
{
    OBJECT *tree = NULL;
    WORD desired = _aes.mouse_base_cursor;

    if (_aes.mouse_cursor_hidden != 0 || _aes.vdi_ready == 0) {
        return;
    }

    if (desired < ARROW || desired > OUTLN_CROSS) {
        return;
    }

    if (desired == ARROW && _aes.menu_visible == 0) {
        if (_aes.edit_tree != NULL) {
            tree = _aes.edit_tree;
        } else if (_aes.hover_tree != NULL) {
            tree = _aes.hover_tree;
        }

        if (tree != NULL) {
            WORD hit = objc_find(tree, ROOT, MAX_DEPTH, x, y);

            if (hit != NIL &&
                (tree[hit].ob_type == G_FTEXT ||
                tree[hit].ob_type == G_FBOXTEXT)) {
                desired = TEXT_CRSR;
            }
        }
    }

    if (_aes.mouse_applied_cursor != desired) {
        (void) _vdi_select_system_mouse_form(desired);
        _aes.mouse_applied_cursor = desired;
    }
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

WORD _aes_min_word(WORD left, WORD right)
{
    return (left < right) ? left : right;
}

WORD _aes_max_word(WORD left, WORD right)
{
    return (left > right) ? left : right;
}

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

int _aes_point_in_rect(WORD x, WORD y, const GRECT *rect)
{
    if (rect == NULL) {
        return 0;
    }

    return x >= rect->g_x && y >= rect->g_y &&
        x < rect->g_x + rect->g_w &&
        y < rect->g_y + rect->g_h;
}

void _aes_reset_state(void)
{
    memset(&_aes, 0, sizeof(_aes));
    _aes.next_app_id = 1;
    _aes.next_window_z = 1u;
    _aes.next_message_seq = 1u;
    _aes.dclick_rate = 3;
    _aes.menu_click = 1;
    _aes.edit_object = NIL;
    _aes.mouse_base_cursor = ARROW;
    _aes.mouse_applied_cursor = ARROW;
    _aes.key_state = 0;
    global[3] = 0x1100;
    global[4] = 0;
}

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
    _aes.mouse_cursor_hidden = 0;
    _aes.mouse_applied_cursor = ARROW;
    _aes_trace("ensure_vdi opened handle=%d size=%dx%d",
        _aes.vdi_handle, _aes.work_out[0] + 1, _aes.work_out[1] + 1);
    return 1;
}

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

void _aes_desktop_rect(GRECT *rect)
{
    if (rect == NULL) {
        return;
    }

    if (_aes_ensure_vdi() == 0) {
        _aes_set_rect(rect, 0, 0, 0, 0);
        return;
    }

    _aes_set_rect(rect, 0, 0, (WORD) (_aes.work_out[0] + 1),
        (WORD) (_aes.work_out[1] + 1));
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

WORD _aes_find_parent(OBJECT *tree, WORD object)
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

LONG _aes_resolve_spec(const OBJECT *obj)
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

int _aes_menu_is_separator_text(const char *text)
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

static void _aes_rtrim_ascii_whitespace(char *text)
{
    size_t length;

    if (text == NULL) {
        return;
    }

    length = strlen(text);
    while (length > 0u &&
        (text[length - 1u] == ' ' || text[length - 1u] == '\t')) {
        --length;
    }
    text[length] = '\0';
}

int _aes_menu_split_shortcut(const char *text,
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
        _aes_rtrim_ascii_whitespace(label);
        return 0;
    }

    left_len = (size_t) (tab - text);
    if (left_len >= label_size) {
        left_len = label_size - 1u;
    }
    memcpy(label, text, left_len);
    label[left_len] = '\0';
    _aes_rtrim_ascii_whitespace(label);

    ++tab;
    right_len = strlen(tab);
    if (right_len >= shortcut_size) {
        right_len = shortcut_size - 1u;
    }
    memcpy(shortcut, tab, right_len);
    shortcut[right_len] = '\0';
    _aes_rtrim_ascii_whitespace(shortcut);
    return 1;
}

WORD _aes_light_color(void)
{
    return BLACK;
}

WORD _aes_dark_color(void)
{
    return (_aes_light_color() == BLACK) ? WHITE : BLACK;
}

int _aes_save_region_pixels(const GRECT *rect, uint8_t **pixels_out)
{
    WORD x;
    WORD y;
    size_t count;
    size_t index = 0;
    uint8_t *pixels;

    if (rect == NULL || pixels_out == NULL || rect->g_w <= 0 || rect->g_h <= 0) {
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

    *pixels_out = pixels;
    return 1;
}

void _aes_restore_region_pixels(const GRECT *rect, uint8_t *pixels)
{
    WORD x;
    WORD y;
    size_t index = 0;

    if (rect == NULL || pixels == NULL || rect->g_w <= 0 || rect->g_h <= 0) {
        return;
    }

    for (y = 0; y < rect->g_h; ++y) {
        for (x = 0; x < rect->g_w; ++x) {
            _vdi_set_screen_pixel((WORD) (rect->g_x + x),
                (WORD) (rect->g_y + y), (WORD) pixels[index++]);
        }
    }
}

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
