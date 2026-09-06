/*
 * Implements the private hosted AES core state, tracing,
 * shared geometry helpers, and file/path utility routines.
 *
 * MIT License (see: LICENSE)
 * Copyright (C) 2026 tomaz stih
 */

#include "aes_internal.h"

#include "../vdi/vdi_internal.h"

#include "platform/os.h"

#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

aes_state_t aes_state;
int (*aes_wait_hook)(void);
int aes_external_input;
const GRECT *aes_modal_cover;

__attribute__((weak)) int gem_builtin_rsrc_load(const char *filename);
__attribute__((weak)) int gem_builtin_rsrc_gaddr(WORD type, WORD index,
                                                 void **addr);
__attribute__((weak)) void gem_builtin_rsrc_free(void);
extern WORD vdi_select_system_mouse_form(WORD selector);
static void aes_write_trace(const char *env_var, const char *fmt, va_list ap);
void aes_trace(const char *fmt, ...);
void aes_store_mouse_state(const gem_hid_event_t *evt);
void aes_store_key_state(const gem_hid_event_t *evt);
static void aes_update_hover_mouse_cursor(WORD x, WORD y);
WORD aes_chrome_height(void);
WORD aes_menu_chrome_height(void);
WORD aes_menu_bar_height(void);
WORD aes_min_word(WORD left, WORD right);
WORD aes_max_word(WORD left, WORD right);
void aes_set_rect(GRECT *rect, WORD x, WORD y, WORD w, WORD h);
int aes_point_in_rect(WORD x, WORD y, const GRECT *rect);
void aes_reset_state(void);
int aes_ensure_vdi(void);
aes_app_t *aes_find_app_by_id(WORD id);
aes_window_t *aes_find_window(WORD handle);
void aes_desktop_rect(GRECT *rect);
int aes_rects_intersect(const GRECT *left, const GRECT *right);
int aes_intersect_rects(const GRECT *left, const GRECT *right, GRECT *out);
WORD aes_subtract_rect(const GRECT *source, const GRECT *cover, GRECT out[4]);
WORD aes_find_parent(OBJECT *tree, WORD object);
LONG aes_resolve_spec(const OBJECT *obj);
int aes_menu_is_separator_text(const char *text);
int aes_menu_split_shortcut(const char *text, char *label, size_t label_size,
                            char *shortcut, size_t shortcut_size);
WORD aes_light_color(void);
WORD aes_dark_color(void);
int aes_save_region_pixels(const GRECT *rect, uint8_t **pixels_out);
void aes_restore_region_pixels(const GRECT *rect, uint8_t *pixels);
int aes_load_file(const char *filename, void **data_out, size_t *size_out);
static void aes_ascii_lower(const char *source, char *target,
                            size_t target_size);
int aes_try_resolve_path(const char *filename, char *resolved,
                         size_t resolved_size);

__attribute__((weak)) int gem_builtin_rsrc_load(const char *filename)
{
    (void)filename;
    return 0;
}

__attribute__((weak)) int gem_builtin_rsrc_gaddr(WORD type, WORD index,
                                                 void **addr)
{
    (void)type;
    (void)index;
    (void)addr;
    return 0;
}

__attribute__((weak)) void gem_builtin_rsrc_free(void) {}

static void aes_write_trace(const char *env_var, const char *fmt, va_list ap)
{
    const char *trace = getenv(env_var);

    if (trace == NULL || trace[0] == '\0') {
        return;
    }
    vfprintf(stderr, fmt, ap);
    fputc('\n', stderr);
}

void aes_trace(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    aes_write_trace("GEM_TRACE_AES", fmt, ap);
    va_end(ap);
}

void aes_store_mouse_state(const gem_hid_event_t *evt)
{
    if (evt == NULL) {
        return;
    }

    vdi_set_mouse_state((WORD)evt->x, (WORD)evt->y, (WORD)evt->flags);
    aes_update_hover_mouse_cursor((WORD)evt->x, (WORD)evt->y);
}

void aes_store_key_state(const gem_hid_event_t *evt)
{
    if (evt == NULL || evt->type != GEM_HID_KEY) {
        return;
    }

    aes_state.key_state = (WORD)evt->mod;
}

static void aes_update_hover_mouse_cursor(WORD x, WORD y)
{
    OBJECT *tree = NULL;
    WORD desired = aes_state.mouse_base_cursor;

    if (aes_state.mouse_cursor_hidden != 0 || aes_state.vdi_ready == 0) {
        return;
    }

    if (desired < ARROW || desired > OUTLN_CROSS) {
        return;
    }

    if (desired == ARROW && aes_state.menu_visible == 0) {
        if (aes_state.edit_tree != NULL) {
            tree = aes_state.edit_tree;
        } else if (aes_state.hover_tree != NULL) {
            tree = aes_state.hover_tree;
        }

        if (tree != NULL) {
            WORD hit = objc_find(tree, ROOT, MAX_DEPTH, x, y);

            if (hit != NIL && (tree[hit].ob_type == G_FTEXT ||
                               tree[hit].ob_type == G_FBOXTEXT)) {
                desired = TEXT_CRSR;
            }
        }
    }

    if (aes_state.mouse_applied_cursor != desired) {
        (void)vdi_select_system_mouse_form(desired);
        aes_state.mouse_applied_cursor = desired;
    }
}

WORD aes_chrome_height(void)
{
    WORD text_height;

    text_height =
        aes_state.vdi_ready ? vdi_font_text_height() : AES_CHAR_HEIGHT;
    return (WORD)(text_height + 4);
}

WORD aes_menu_chrome_height(void)
{
    WORD text_height;

    text_height =
        aes_state.vdi_ready ? vdi_font_text_height() : AES_CHAR_HEIGHT;
    return (WORD)(text_height + 6);
}

WORD aes_menu_bar_height(void)
{
    WORD box_height = 0;

    if (aes_state.menu_visible == 0 || aes_state.menu_tree == NULL) {
        return 0;
    }

    (void)graf_handle(NULL, NULL, NULL, &box_height);
    if (box_height <= 0) {
        box_height = aes_menu_chrome_height();
    }
    return box_height;
}

WORD aes_min_word(WORD left, WORD right)
{
    return (left < right) ? left : right;
}

WORD aes_max_word(WORD left, WORD right)
{
    return (left > right) ? left : right;
}

void aes_set_rect(GRECT *rect, WORD x, WORD y, WORD w, WORD h)
{
    if (rect == NULL) {
        return;
    }

    rect->g_x = x;
    rect->g_y = y;
    rect->g_w = w;
    rect->g_h = h;
}

int aes_point_in_rect(WORD x, WORD y, const GRECT *rect)
{
    if (rect == NULL) {
        return 0;
    }

    return x >= rect->g_x && y >= rect->g_y && x < rect->g_x + rect->g_w &&
           y < rect->g_y + rect->g_h;
}

void aes_reset_state(void)
{
    memset(&aes_state, 0, sizeof(aes_state));
    aes_state.next_app_id = 1;
    aes_state.next_window_z = 1u;
    aes_state.next_message_seq = 1u;
    aes_state.dclick_rate = 3;
    aes_state.menu_click = 1;
    aes_state.edit_object = NIL;
    aes_state.mouse_base_cursor = ARROW;
    aes_state.mouse_applied_cursor = ARROW;
    aes_state.key_state = 0;
    global[3] = 0x1100;
    global[4] = 0;
}

int aes_ensure_vdi(void)
{
    if (aes_state.vdi_ready != 0 && aes_state.vdi_handle != 0) {
        aes_trace("ensure_vdi already handle=%d", aes_state.vdi_handle);
        return 1;
    }

    aes_trace("ensure_vdi opening");
    memset(aes_state.work_in, 0, sizeof(aes_state.work_in));
    memset(aes_state.work_out, 0, sizeof(aes_state.work_out));
    v_opnvwk(aes_state.work_in, &aes_state.vdi_handle, aes_state.work_out);
    if (aes_state.vdi_handle == 0) {
        aes_trace("ensure_vdi failed");
        return 0;
    }

    aes_state.vdi_ready = 1;
    /* Paint the desktop once when AES first acquires its workstation.
     * Later window redraws repair only their damaged regions, so they
     * cannot initialize the untouched background around the first window.
     * Keep this in AES: a VDI-only application owns its entire surface.
     */
    aes_redraw_open_windows();

    /*
     * AES applications expect the default arrow cursor to be visible
     * once the hosted workstation exists. Demos that only use basic
     * window messages never call graf_mouse(M_ON) themselves.
     */
    v_show_c(aes_state.vdi_handle, 1);
    aes_state.mouse_cursor_hidden = 0;
    aes_state.mouse_applied_cursor = ARROW;
    aes_trace("ensure_vdi opened handle=%d size=%dx%d", aes_state.vdi_handle,
              aes_state.work_out[0] + 1, aes_state.work_out[1] + 1);
    return 1;
}

aes_app_t *aes_find_app_by_id(WORD id)
{
    size_t i;

    for (i = 0; i < AES_MAX_APPS; ++i) {
        if (aes_state.apps[i].used != 0 && aes_state.apps[i].id == id) {
            return &aes_state.apps[i];
        }
    }
    return NULL;
}

aes_window_t *aes_find_window(WORD handle)
{
    size_t i;

    for (i = 0; i < AES_MAX_WINDOWS; ++i) {
        if (aes_state.windows[i].used != 0 &&
            aes_state.windows[i].handle == handle) {
            return &aes_state.windows[i];
        }
    }
    return NULL;
}

void aes_desktop_rect(GRECT *rect)
{
    if (rect == NULL) {
        return;
    }

    if (aes_ensure_vdi() == 0) {
        aes_set_rect(rect, 0, 0, 0, 0);
        return;
    }

    aes_set_rect(rect, 0, 0, (WORD)(aes_state.work_out[0] + 1),
                 (WORD)(aes_state.work_out[1] + 1));
}

int aes_rects_intersect(const GRECT *left, const GRECT *right)
{
    WORD left_right;
    WORD left_bottom;
    WORD right_right;
    WORD right_bottom;

    if (left == NULL || right == NULL || left->g_w <= 0 || left->g_h <= 0 ||
        right->g_w <= 0 || right->g_h <= 0) {
        return 0;
    }

    left_right = (WORD)(left->g_x + left->g_w - 1);
    left_bottom = (WORD)(left->g_y + left->g_h - 1);
    right_right = (WORD)(right->g_x + right->g_w - 1);
    right_bottom = (WORD)(right->g_y + right->g_h - 1);

    if (left_right < right->g_x || right_right < left->g_x ||
        left_bottom < right->g_y || right_bottom < left->g_y) {
        return 0;
    }
    return 1;
}

int aes_intersect_rects(const GRECT *left, const GRECT *right, GRECT *out)
{
    WORD left_right;
    WORD left_bottom;
    WORD right_right;
    WORD right_bottom;
    WORD x0;
    WORD y0;
    WORD x1;
    WORD y1;

    if (out == NULL || aes_rects_intersect(left, right) == 0) {
        return 0;
    }

    left_right = (WORD)(left->g_x + left->g_w - 1);
    left_bottom = (WORD)(left->g_y + left->g_h - 1);
    right_right = (WORD)(right->g_x + right->g_w - 1);
    right_bottom = (WORD)(right->g_y + right->g_h - 1);

    x0 = aes_max_word(left->g_x, right->g_x);
    y0 = aes_max_word(left->g_y, right->g_y);
    x1 = aes_min_word(left_right, right_right);
    y1 = aes_min_word(left_bottom, right_bottom);
    aes_set_rect(out, x0, y0, (WORD)(x1 - x0 + 1), (WORD)(y1 - y0 + 1));
    return 1;
}

WORD aes_subtract_rect(const GRECT *source, const GRECT *cover, GRECT out[4])
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
        aes_intersect_rects(source, cover, &overlap) == 0) {
        out[0] = *source;
        return 1;
    }

    source_right = (WORD)(source->g_x + source->g_w - 1);
    source_bottom = (WORD)(source->g_y + source->g_h - 1);
    overlap_right = (WORD)(overlap.g_x + overlap.g_w - 1);
    overlap_bottom = (WORD)(overlap.g_y + overlap.g_h - 1);

    if (source->g_y < overlap.g_y) {
        aes_set_rect(&out[count++], source->g_x, source->g_y, source->g_w,
                     (WORD)(overlap.g_y - source->g_y));
    }
    if (overlap_bottom < source_bottom) {
        aes_set_rect(&out[count++], source->g_x, (WORD)(overlap_bottom + 1),
                     source->g_w, (WORD)(source_bottom - overlap_bottom));
    }
    if (source->g_x < overlap.g_x) {
        aes_set_rect(&out[count++], source->g_x, overlap.g_y,
                     (WORD)(overlap.g_x - source->g_x), overlap.g_h);
    }
    if (overlap_right < source_right) {
        aes_set_rect(&out[count++], (WORD)(overlap_right + 1), overlap.g_y,
                     (WORD)(source_right - overlap_right), overlap.g_h);
    }

    return count;
}

WORD aes_find_parent(OBJECT *tree, WORD object)
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

LONG aes_resolve_spec(const OBJECT *obj)
{
    LONG spec;

    if (obj == NULL) {
        return 0;
    }

    spec = obj->ob_spec;
    if ((obj->ob_flags & INDIRECT) != 0u && spec != 0) {
        spec = *(LONG *)(intptr_t)spec;
    }
    return spec;
}

int aes_menu_is_separator_text(const char *text)
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

static void aes_rtrim_ascii_whitespace(char *text)
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

int aes_menu_split_shortcut(const char *text, char *label, size_t label_size,
                            char *shortcut, size_t shortcut_size)
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
    if (text == NULL || label == NULL || shortcut == NULL || label_size == 0u ||
        shortcut_size == 0u) {
        return 0;
    }

    tab = strchr(text, '\t');
    if (tab == NULL) {
        strncpy(label, text, label_size - 1u);
        label[label_size - 1u] = '\0';
        aes_rtrim_ascii_whitespace(label);
        return 0;
    }

    left_len = (size_t)(tab - text);
    if (left_len >= label_size) {
        left_len = label_size - 1u;
    }
    memcpy(label, text, left_len);
    label[left_len] = '\0';
    aes_rtrim_ascii_whitespace(label);

    ++tab;
    right_len = strlen(tab);
    if (right_len >= shortcut_size) {
        right_len = shortcut_size - 1u;
    }
    memcpy(shortcut, tab, right_len);
    shortcut[right_len] = '\0';
    aes_rtrim_ascii_whitespace(shortcut);
    return 1;
}

WORD aes_light_color(void)
{
    /*
     * Unchanged from the rasta-proven model: with
     * vdi_color_to_pixel(WHITE)→1 / BLACK→0 and set-bit = black ink
     * on the display, BLACK as the "light" fill paints white paper.
     */
    return BLACK;
}

WORD aes_dark_color(void)
{
    return (aes_light_color() == BLACK) ? WHITE : BLACK;
}

int aes_save_region_pixels(const GRECT *rect, uint8_t **pixels_out)
{
    WORD x;
    WORD y;
    size_t count;
    size_t index = 0;
    uint8_t *pixels;

    if (rect == NULL || pixels_out == NULL || rect->g_w <= 0 ||
        rect->g_h <= 0) {
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

    *pixels_out = pixels;
    return 1;
}

void aes_restore_region_pixels(const GRECT *rect, uint8_t *pixels)
{
    WORD x;
    WORD y;
    size_t index = 0;

    if (rect == NULL || pixels == NULL || rect->g_w <= 0 || rect->g_h <= 0) {
        return;
    }

    for (y = 0; y < rect->g_h; ++y) {
        for (x = 0; x < rect->g_w; ++x) {
            vdi_set_screen_pixel((WORD)(rect->g_x + x), (WORD)(rect->g_y + y),
                                 (WORD)pixels[index++]);
        }
    }
    vdi_mark_dirty(rect->g_x, rect->g_y, (WORD)(rect->g_x + rect->g_w - 1),
                   (WORD)(rect->g_y + rect->g_h - 1));
    vdi_present_screen();
}

int aes_load_file(const char *filename, void **data_out, size_t *size_out)
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
        (void)gem_os_close(fd);
        return 0;
    }

    FOREVER
    {
        if (used == capacity) {
            size_t new_capacity = capacity * 2u;
            char *new_buffer = gem_os_alloc(new_capacity);

            if (new_buffer == NULL) {
                gem_os_free(buffer);
                (void)gem_os_close(fd);
                return 0;
            }
            memcpy(new_buffer, buffer, used);
            gem_os_free(buffer);
            buffer = new_buffer;
            capacity = new_capacity;
        }

        read_size = gem_os_read(fd, buffer + used, (uint32_t)(capacity - used));
        if (read_size < 0) {
            gem_os_free(buffer);
            (void)gem_os_close(fd);
            return 0;
        }
        if (read_size == 0) {
            break;
        }
        used += (size_t)read_size;
    }

    (void)gem_os_close(fd);
    *data_out = buffer;
    *size_out = used;
    return 1;
}

static void aes_ascii_lower(const char *source, char *target,
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
            ch = (char)(ch - 'A' + 'a');
        }
        target[i] = ch;
    }
    target[i] = '\0';
}

int aes_try_resolve_path(const char *filename, char *resolved,
                         size_t resolved_size)
{
    static const char *search_dirs[] = {"", "bin/resources/"};
    const char *resource_dir;
    char lowercase[260];
    size_t i;

    if (filename == NULL || resolved == NULL || resolved_size == 0u) {
        return 0;
    }

    aes_ascii_lower(filename, lowercase, sizeof(lowercase));
    resource_dir = getenv("GEM_RESOURCE_DIR");
    if (resource_dir != NULL && resource_dir[0] != '\0') {
        int rc =
            snprintf(resolved, resolved_size, "%s/%s", resource_dir, filename);
        int fd;

        if (rc > 0 && (size_t)rc < resolved_size) {
            fd = gem_os_open_read(resolved);
            if (fd >= 0) {
                (void)gem_os_close(fd);
                return 1;
            }
        }
        rc =
            snprintf(resolved, resolved_size, "%s/%s", resource_dir, lowercase);
        if (rc > 0 && (size_t)rc < resolved_size) {
            fd = gem_os_open_read(resolved);
            if (fd >= 0) {
                (void)gem_os_close(fd);
                return 1;
            }
        }
    }

    for (i = 0; i < sizeof(search_dirs) / sizeof(search_dirs[0]); ++i) {
        const char *dir = search_dirs[i];
        int rc;
        int fd;

        rc = snprintf(resolved, resolved_size, "%s%s", dir, filename);
        if (rc > 0 && (size_t)rc < resolved_size) {
            fd = gem_os_open_read(resolved);
            if (fd >= 0) {
                (void)gem_os_close(fd);
                return 1;
            }
        }

        rc = snprintf(resolved, resolved_size, "%s%s", dir, lowercase);
        if (rc > 0 && (size_t)rc < resolved_size) {
            fd = gem_os_open_read(resolved);

            if (fd >= 0) {
                (void)gem_os_close(fd);
                return 1;
            }
        }
    }

    return 0;
}
