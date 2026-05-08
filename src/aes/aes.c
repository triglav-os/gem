/*
 * Exposes the hosted AES public entry points used by the Linux GEM
 * port. The private runtime machinery lives in `_aes.c`, while this
 * file keeps the exported AES API readable and easier to debug.
 *
 * MIT License (see: LICENSE)
 * Copyright (C) 2026 tomaz stih
 */

#include "_aes.h"
#include "alert_icons.h"

#include "platform/os.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum {
    AES_ALERT_MAX_LINES = 5,
    AES_ALERT_MAX_BUTTONS = 3,
    AES_ALERT_TEXT_LEN = 80
};

typedef struct aes_alert {
    WORD icon;
    WORD default_button;
    WORD line_count;
    WORD button_count;
    char lines[AES_ALERT_MAX_LINES][AES_ALERT_TEXT_LEN];
    char buttons[AES_ALERT_MAX_BUTTONS][AES_ALERT_TEXT_LEN];
    GRECT outer;
    GRECT button_rects[AES_ALERT_MAX_BUTTONS];
} aes_alert_t;

WORD contrl[12] __attribute__((weak));
WORD intin[128] __attribute__((weak));
WORD ptsin[256] __attribute__((weak));
WORD intout[128] __attribute__((weak));
WORD ptsout[256] __attribute__((weak));

WORD global[15];

static void _aes_desktop_rect(GRECT *rect);
static void _aes_queue_window_message(const aes_window_t *window,
    WORD message, WORD w4, WORD w5, WORD w6, WORD w7);
static WORD _aes_dequeue_message(WORD msg[8]);
static void _aes_draw_drag_outline(const GRECT *rect);
static void _aes_union_rects(const GRECT *left, const GRECT *right, GRECT *out);
static void _aes_redraw_scrollbar_delta(const aes_window_t *before,
    const aes_window_t *after, WORD field);
static void _aes_clamp_dragged_window_position(const aes_window_t *window,
    const GRECT *desktop, WORD *x, WORD *y);
static void _aes_toggle_window_iconified(aes_window_t *window);
static WORD _aes_build_visible_rects(WORD handle, GRECT out[], WORD max_rects);
static WORD _aes_track_window_interaction(const gem_hid_event_t *first_evt,
    WORD flags, WORD mepbuff[8], WORD *pmx, WORD *pmy, WORD *pmb, WORD *pks);
static int _aes_resource_contains_ptr(const void *ptr);
static LONG _aes_resource_fix_offset(LONG value);
static void _aes_resource_fix_tedinfo(TEDINFO *ted);
static void _aes_resource_fix_iconblk(ICONBLK *icon);
static void _aes_resource_fix_bitblk(BITBLK *bitblk);
static int _aes_alert_parse_group(const char **cursor,
    char *out, size_t out_size);
static void _aes_alert_split_items(const char *text,
    char items[][AES_ALERT_TEXT_LEN], WORD *count, WORD max_items);
static int _aes_parse_alert(const char *str, WORD defbtn, aes_alert_t *alert);
static void _aes_alert_compute_layout(aes_alert_t *alert);
static void _aes_alert_draw_text(WORD x, WORD y, const char *text);
static void _aes_alert_draw_frame(const GRECT *rect);
static void _aes_alert_draw_button(const GRECT *rect,
    const char *label, int is_default);
static const aes_alert_icon_asset_t *_aes_alert_icon_asset(WORD icon);
static int _aes_alert_icon_bit(const UWORD *bits,
    const aes_alert_icon_asset_t *asset, WORD x, WORD y);
static void _aes_alert_draw_icon(const aes_alert_icon_asset_t *asset,
    WORD x, WORD y);
static void _aes_draw_alert(const aes_alert_t *alert);
static WORD _aes_run_alert(aes_alert_t *alert);
extern WORD _vdi_select_system_mouse_form(WORD selector);

static void _aes_desktop_rect(GRECT *rect)
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

static int _aes_alert_parse_group(const char **cursor,
    char *out, size_t out_size)
{
    const char *scan;
    size_t len = 0;

    if (cursor == NULL || *cursor == NULL || out == NULL || out_size == 0u) {
        return 0;
    }

    scan = *cursor;
    while (*scan != '\0' && *scan != '[') {
        ++scan;
    }
    if (*scan != '[') {
        return 0;
    }
    ++scan;
    while (*scan != '\0' && *scan != ']') {
        if (len + 1u < out_size) {
            out[len++] = *scan;
        }
        ++scan;
    }
    if (*scan != ']') {
        return 0;
    }

    out[len] = '\0';
    *cursor = scan + 1;
    return 1;
}

static void _aes_alert_split_items(const char *text,
    char items[][AES_ALERT_TEXT_LEN], WORD *count, WORD max_items)
{
    WORD item = 0;
    size_t len = 0;

    if (count == NULL) {
        return;
    }
    *count = 0;
    if (text == NULL || items == NULL || max_items <= 0) {
        return;
    }

    memset(items, 0, (size_t) max_items * AES_ALERT_TEXT_LEN);
    while (*text != '\0' && item < max_items) {
        if (*text == '|') {
            items[item][len] = '\0';
            ++item;
            len = 0;
            ++text;
            continue;
        }
        if (len + 1u < AES_ALERT_TEXT_LEN) {
            items[item][len++] = *text;
        }
        ++text;
    }
    if (item < max_items) {
        items[item][len] = '\0';
        *count = (WORD) (item + 1);
    }
}

static int _aes_parse_alert(const char *str, WORD defbtn, aes_alert_t *alert)
{
    char icon_text[AES_ALERT_TEXT_LEN];
    char line_text[AES_ALERT_TEXT_LEN];
    char button_text[AES_ALERT_TEXT_LEN];
    const char *cursor = str;

    if (alert == NULL) {
        return 0;
    }

    memset(alert, 0, sizeof(*alert));
    alert->default_button = (defbtn > 0) ? defbtn : 1;
    if (str == NULL) {
        strcpy(alert->lines[0], "Alert");
        strcpy(alert->buttons[0], "OK");
        alert->line_count = 1;
        alert->button_count = 1;
        return 1;
    }

    if (_aes_alert_parse_group(&cursor, icon_text, sizeof(icon_text)) == 0 ||
        _aes_alert_parse_group(&cursor, line_text, sizeof(line_text)) == 0 ||
        _aes_alert_parse_group(&cursor, button_text,
            sizeof(button_text)) == 0) {
        strncpy(alert->lines[0], str, AES_ALERT_TEXT_LEN - 1u);
        strcpy(alert->buttons[0], "OK");
        alert->line_count = 1;
        alert->button_count = 1;
        return 1;
    }

    alert->icon = (WORD) atoi(icon_text);
    _aes_alert_split_items(line_text, alert->lines, &alert->line_count,
        AES_ALERT_MAX_LINES);
    _aes_alert_split_items(button_text, alert->buttons, &alert->button_count,
        AES_ALERT_MAX_BUTTONS);
    if (alert->line_count <= 0) {
        strcpy(alert->lines[0], "Alert");
        alert->line_count = 1;
    }
    if (alert->button_count <= 0) {
        strcpy(alert->buttons[0], "OK");
        alert->button_count = 1;
    }
    if (alert->default_button > alert->button_count) {
        alert->default_button = alert->button_count;
    }
    return 1;
}

static void _aes_alert_compute_layout(aes_alert_t *alert)
{
    GRECT desktop;
    WORD text_h;
    WORD text_w = 0;
    WORD line_gap = 4;
    WORD icon_w = 0;
    WORD icon_h = 0;
    WORD button_h;
    WORD button_gap = 8;
    WORD button_total = 0;
    WORD content_w;
    WORD content_h;
    WORD x;
    WORD i;

    if (alert == NULL) {
        return;
    }

    text_h = _vdi_font_text_height();
    if (text_h <= 0) {
        text_h = AES_CHAR_HEIGHT;
    }
    button_h = (WORD) (text_h + 12);
    if (_aes_alert_icon_asset(alert->icon) != NULL) {
        icon_w = 32;
        icon_h = 32;
    }

    for (i = 0; i < alert->line_count; ++i) {
        WORD width = (WORD) _vdi_string_width(alert->lines[i]);

        if (width > text_w) {
            text_w = width;
        }
    }

    for (i = 0; i < alert->button_count; ++i) {
        WORD width = (WORD) (_vdi_string_width(alert->buttons[i]) + 18);

        if (width < 48) {
            width = 48;
        }
        alert->button_rects[i].g_w = width;
        alert->button_rects[i].g_h = button_h;
        button_total = (WORD) (button_total + width);
        if (i + 1 < alert->button_count) {
            button_total = (WORD) (button_total + button_gap);
        }
    }

    content_w = (WORD) (text_w + icon_w + ((icon_w > 0) ? 12 : 0));
    if (button_total > content_w) {
        content_w = button_total;
    }
    content_h = (WORD) (alert->line_count * text_h +
        (alert->line_count - 1) * line_gap);
    if (icon_h > content_h) {
        content_h = icon_h;
    }

    _aes_desktop_rect(&desktop);
    alert->outer.g_w = (WORD) (content_w + 32);
    alert->outer.g_h = (WORD) (content_h + button_h + 40);
    alert->outer.g_x = (WORD) (desktop.g_x +
        (desktop.g_w - alert->outer.g_w) / 2);
    alert->outer.g_y = (WORD) (desktop.g_y +
        (desktop.g_h - alert->outer.g_h) / 2);

    x = (WORD) (alert->outer.g_x +
        (alert->outer.g_w - button_total) / 2);
    for (i = 0; i < alert->button_count; ++i) {
        alert->button_rects[i].g_x = x;
        alert->button_rects[i].g_y = (WORD) (alert->outer.g_y +
            alert->outer.g_h - button_h - 12);
        x = (WORD) (x + alert->button_rects[i].g_w + button_gap);
    }
}

static void _aes_alert_draw_text(WORD x, WORD y, const char *text)
{
    vst_color(_aes.vdi_handle, WHITE);
    v_gtext(_aes.vdi_handle, x, y, (CONST BYTE *) text);
}

static void _aes_alert_draw_frame(const GRECT *rect)
{
    WORD fill[4];
    WORD border[10];

    if (rect == NULL) {
        return;
    }

    fill[0] = rect->g_x;
    fill[1] = rect->g_y;
    fill[2] = (WORD) (rect->g_x + rect->g_w - 1);
    fill[3] = (WORD) (rect->g_y + rect->g_h - 1);
    vsf_color(_aes.vdi_handle, BLACK);
    vr_recfl(_aes.vdi_handle, fill);

    border[0] = fill[0];
    border[1] = fill[1];
    border[2] = fill[2];
    border[3] = fill[1];
    border[4] = fill[2];
    border[5] = fill[3];
    border[6] = fill[0];
    border[7] = fill[3];
    border[8] = fill[0];
    border[9] = fill[1];
    vsl_color(_aes.vdi_handle, WHITE);
    v_pline(_aes.vdi_handle, 5, border);
}

static void _aes_alert_draw_button(const GRECT *rect,
    const char *label, int is_default)
{
    WORD fill[4];
    WORD border[10];
    WORD inner[10];
    WORD text_x;
    WORD text_y;
    WORD width;

    if (rect == NULL || label == NULL) {
        return;
    }

    fill[0] = rect->g_x;
    fill[1] = rect->g_y;
    fill[2] = (WORD) (rect->g_x + rect->g_w - 1);
    fill[3] = (WORD) (rect->g_y + rect->g_h - 1);
    vsf_color(_aes.vdi_handle, BLACK);
    vr_recfl(_aes.vdi_handle, fill);

    border[0] = fill[0];
    border[1] = fill[1];
    border[2] = fill[2];
    border[3] = fill[1];
    border[4] = fill[2];
    border[5] = fill[3];
    border[6] = fill[0];
    border[7] = fill[3];
    border[8] = fill[0];
    border[9] = fill[1];
    vsl_color(_aes.vdi_handle, WHITE);
    v_pline(_aes.vdi_handle, 5, border);

    if (is_default != 0 && rect->g_w > 4 && rect->g_h > 4) {
        inner[0] = (WORD) (fill[0] + 2);
        inner[1] = (WORD) (fill[1] + 2);
        inner[2] = (WORD) (fill[2] - 2);
        inner[3] = inner[1];
        inner[4] = inner[2];
        inner[5] = (WORD) (fill[3] - 2);
        inner[6] = inner[0];
        inner[7] = inner[5];
        inner[8] = inner[0];
        inner[9] = inner[1];
        v_pline(_aes.vdi_handle, 5, inner);
    }

    width = (WORD) _vdi_string_width(label);
    text_x = (WORD) (rect->g_x + (rect->g_w - width) / 2);
    text_y = (WORD) (rect->g_y +
        (rect->g_h - _vdi_font_text_height()) / 2 + _vdi_font_ascent());
    _aes_alert_draw_text(text_x, text_y, label);
}

static const aes_alert_icon_asset_t *_aes_alert_icon_asset(WORD icon)
{
    switch (icon) {
    case 1:
        return &aes_alert_exclamation_icon_asset;
    case 2:
        return &aes_alert_question_icon_asset;
    case 3:
        return &aes_alert_stop_icon_asset;
    default:
        return NULL;
    }
}

static int _aes_alert_icon_bit(const UWORD *bits,
    const aes_alert_icon_asset_t *asset, WORD x, WORD y)
{
    size_t row_words;
    size_t index;
    UWORD word;

    if (bits == NULL || asset == NULL || x < 0 || y < 0 ||
        x >= asset->width || y >= asset->height) {
        return 0;
    }

    row_words = (size_t) ((asset->width + 15) / 16);
    index = (size_t) y * row_words + (size_t) x / 16u;
    word = bits[index];
    return (word & (WORD) (0x8000u >> (x % 16))) != 0;
}

static void _aes_alert_draw_icon(const aes_alert_icon_asset_t *asset,
    WORD x, WORD y)
{
    WORD row;

    if (asset == NULL) {
        return;
    }

    for (row = 0; row < asset->height; ++row) {
        WORD col = 0;

        while (col < asset->width) {
            WORD start;
            WORD rect[4];

            while (col < asset->width &&
                _aes_alert_icon_bit(asset->mask_bits, asset, col, row) == 0) {
                ++col;
            }
            if (col >= asset->width) {
                break;
            }
            start = col;
            while (col < asset->width &&
                _aes_alert_icon_bit(asset->mask_bits, asset, col, row) != 0) {
                ++col;
            }
            rect[0] = (WORD) (x + start);
            rect[1] = (WORD) (y + row);
            rect[2] = (WORD) (x + col - 1);
            rect[3] = rect[1];
            vsf_color(_aes.vdi_handle, BLACK);
            v_bar(_aes.vdi_handle, rect);
        }
    }

    for (row = 0; row < asset->height; ++row) {
        WORD col = 0;

        while (col < asset->width) {
            WORD start;
            WORD rect[4];

            while (col < asset->width &&
                _aes_alert_icon_bit(asset->data_bits, asset, col, row) == 0) {
                ++col;
            }
            if (col >= asset->width) {
                break;
            }
            start = col;
            while (col < asset->width &&
                _aes_alert_icon_bit(asset->data_bits, asset, col, row) != 0) {
                ++col;
            }
            rect[0] = (WORD) (x + start);
            rect[1] = (WORD) (y + row);
            rect[2] = (WORD) (x + col - 1);
            rect[3] = rect[1];
            vsf_color(_aes.vdi_handle, WHITE);
            v_bar(_aes.vdi_handle, rect);
        }
    }
}

static void _aes_draw_alert(const aes_alert_t *alert)
{
    WORD clip[4];
    WORD text_x;
    WORD text_y;
    WORD text_h;
    WORD line_gap = 4;
    WORD i;

    if (alert == NULL || _aes_ensure_vdi() == 0) {
        return;
    }

    clip[0] = alert->outer.g_x;
    clip[1] = alert->outer.g_y;
    clip[2] = (WORD) (alert->outer.g_x + alert->outer.g_w - 1);
    clip[3] = (WORD) (alert->outer.g_y + alert->outer.g_h - 1);

    wind_update(BEG_UPDATE);
    v_hide_c(_aes.vdi_handle);
    vs_clip(_aes.vdi_handle, 1, clip);
    (void) vswr_mode(_aes.vdi_handle, MD_REPLACE);

    _aes_alert_draw_frame(&alert->outer);
    text_x = (WORD) (alert->outer.g_x + 12);
    if (_aes_alert_icon_asset(alert->icon) != NULL) {
        _aes_alert_draw_icon(_aes_alert_icon_asset(alert->icon),
            text_x, (WORD) (alert->outer.g_y + 12));
        text_x = (WORD) (text_x + 44);
    }

    text_h = _vdi_font_text_height();
    for (i = 0; i < alert->line_count; ++i) {
        text_y = (WORD) (alert->outer.g_y + 12 +
            i * (text_h + line_gap) + _vdi_font_ascent());
        _aes_alert_draw_text(text_x, text_y, alert->lines[i]);
    }
    for (i = 0; i < alert->button_count; ++i) {
        _aes_alert_draw_button(&alert->button_rects[i], alert->buttons[i],
            i + 1 == alert->default_button);
    }

    vs_clip(_aes.vdi_handle, 0, clip);
    v_show_c(_aes.vdi_handle, 0);
    wind_update(END_UPDATE);
}

static WORD _aes_run_alert(aes_alert_t *alert)
{
    gem_hid_event_t evt;
    uint8_t *saved_pixels = NULL;
    WORD i;

    if (alert == NULL) {
        return 1;
    }

    _vdi_begin_update();
    v_hide_c(_aes.vdi_handle);
    (void) _aes_save_region_pixels(&alert->outer, &saved_pixels);
    v_show_c(_aes.vdi_handle, 0);
    _vdi_end_update();
    _aes_draw_alert(alert);

    for (;;) {
        if (gem_hid_poll(&evt) == 0) {
            gem_os_sleep_ms(1u);
            continue;
        }

        if (evt.type == GEM_HID_QUIT) {
            break;
        }
        if (evt.type == GEM_HID_MOUSE_MOVE) {
            _aes_store_mouse_state(&evt);
        }
        if (evt.type == GEM_HID_KEY && (evt.flags & 1u) != 0u) {
            WORD ch = (WORD) (evt.key & 0xffu);

            if (ch == 13 || ch == '\n' || ch == ' ') {
                break;
            }
            if (ch >= '1' && ch < '1' + alert->button_count) {
                wind_update(BEG_UPDATE);
                v_hide_c(_aes.vdi_handle);
                _aes_restore_region_pixels(&alert->outer, saved_pixels);
                v_show_c(_aes.vdi_handle, 0);
                wind_update(END_UPDATE);
                if (saved_pixels != NULL) {
                    gem_os_free(saved_pixels);
                }
                return (WORD) (ch - '0');
            }
            if (ch == 27) {
                break;
            }
        }
        if (evt.type == GEM_HID_MOUSE_BUTTON &&
            evt.button == GEM_HID_BUTTON_LEFT &&
            (evt.flags & GEM_HID_BUTTON_LEFT) != 0u) {
            for (i = 0; i < alert->button_count; ++i) {
                if (_aes_point_in_rect((WORD) evt.x, (WORD) evt.y,
                        &alert->button_rects[i]) != 0) {
                    wind_update(BEG_UPDATE);
                    v_hide_c(_aes.vdi_handle);
                    _aes_restore_region_pixels(&alert->outer, saved_pixels);
                    v_show_c(_aes.vdi_handle, 0);
                    wind_update(END_UPDATE);
                    if (saved_pixels != NULL) {
                        gem_os_free(saved_pixels);
                    }
                    return (WORD) (i + 1);
                }
            }
        }
    }

    wind_update(BEG_UPDATE);
    v_hide_c(_aes.vdi_handle);
    _aes_restore_region_pixels(&alert->outer, saved_pixels);
    v_show_c(_aes.vdi_handle, 0);
    wind_update(END_UPDATE);
    if (saved_pixels != NULL) {
        gem_os_free(saved_pixels);
    }
    return alert->default_button;
}


static aes_window_t *_aes_find_top_window(void)
{
    aes_window_t *best = NULL;
    size_t i;

    for (i = 0; i < AES_MAX_WINDOWS; ++i) {
        aes_window_t *window = &_aes.windows[i];

        if (window->used == 0 || window->open == 0) {
            continue;
        }
        if (best == NULL || window->z_order > best->z_order) {
            best = window;
        }
    }

    return best;
}

static void _aes_queue_window_message(const aes_window_t *window,
    WORD message, WORD w4, WORD w5, WORD w6, WORD w7)
{
    WORD msg[8];

    if (window == NULL || window->owner == 0) {
        return;
    }

    msg[0] = message;
    msg[1] = _aes.current_app_id;
    msg[2] = 0;
    msg[3] = window->handle;
    msg[4] = w4;
    msg[5] = w5;
    msg[6] = w6;
    msg[7] = w7;
    (void) appl_write(window->owner, 8, msg);
}

static WORD _aes_dequeue_message(WORD msg[8])
{
    if (msg == NULL) {
        return 0;
    }

    return appl_read(_aes.current_app_id, 8, msg);
}

static void _aes_draw_drag_outline(const GRECT *rect)
{
    WORD box[10];
    WORD previous_mode;

    if (rect == NULL || rect->g_w <= 0 || rect->g_h <= 0 ||
        _aes_ensure_vdi() == 0) {
        return;
    }

    box[0] = rect->g_x;
    box[1] = rect->g_y;
    box[2] = (WORD) (rect->g_x + rect->g_w - 1);
    box[3] = box[1];
    box[4] = box[2];
    box[5] = (WORD) (rect->g_y + rect->g_h - 1);
    box[6] = box[0];
    box[7] = box[5];
    box[8] = box[0];
    box[9] = box[1];

    previous_mode = _vdi_write_mode();
    (void) vswr_mode(_aes.vdi_handle, MD_XOR);
    vsl_color(_aes.vdi_handle, WHITE);
    v_pline(_aes.vdi_handle, 5, box);
    (void) vswr_mode(_aes.vdi_handle, previous_mode);
}

static void _aes_union_rects(const GRECT *left, const GRECT *right, GRECT *out)
{
    WORD x0;
    WORD y0;
    WORD x1;
    WORD y1;

    if (left == NULL || right == NULL || out == NULL) {
        return;
    }

    x0 = _aes_min_word(left->g_x, right->g_x);
    y0 = _aes_min_word(left->g_y, right->g_y);
    x1 = _aes_max_word((WORD) (left->g_x + left->g_w - 1),
        (WORD) (right->g_x + right->g_w - 1));
    y1 = _aes_max_word((WORD) (left->g_y + left->g_h - 1),
        (WORD) (right->g_y + right->g_h - 1));
    _aes_set_rect(out, x0, y0, (WORD) (x1 - x0 + 1), (WORD) (y1 - y0 + 1));
}

static void _aes_redraw_scrollbar_delta(const aes_window_t *before,
    const aes_window_t *after, WORD field)
{
    GRECT old_rect;
    GRECT new_rect;
    GRECT dirty;
    int have_old = 0;
    int have_new = 0;

    if (before == NULL || after == NULL || after->open == 0) {
        return;
    }

    switch (field) {
    case WF_VSLIDE:
    case WF_VSLSIZ:
        have_old = _aes_window_vthumb_rect(before, &old_rect);
        have_new = _aes_window_vthumb_rect(after, &new_rect);
        break;
    case WF_HSLIDE:
    case WF_HSLSIZ:
        have_old = _aes_window_hthumb_rect(before, &old_rect);
        have_new = _aes_window_hthumb_rect(after, &new_rect);
        break;
    default:
        break;
    }

    if (have_old != 0 && have_new != 0) {
        if (old_rect.g_x == new_rect.g_x &&
            old_rect.g_y == new_rect.g_y &&
            old_rect.g_w == new_rect.g_w &&
            old_rect.g_h == new_rect.g_h) {
            return;
        }
        _aes_union_rects(&old_rect, &new_rect, &dirty);
        _aes_redraw_region(&dirty);
    } else if (have_old != 0) {
        _aes_redraw_region(&old_rect);
    } else if (have_new != 0) {
        _aes_redraw_region(&new_rect);
    }
}

static void _aes_clamp_dragged_window_position(const aes_window_t *window,
    const GRECT *desktop, WORD *x, WORD *y)
{
    WORD title_height;
    WORD visible_width;
    WORD min_x;
    WORD max_x;
    WORD min_y;
    WORD max_y;

    if (window == NULL || desktop == NULL || x == NULL || y == NULL) {
        return;
    }

    /*
     * Permit windows to move partially off-screen while keeping a small
     * reachable strip of the title area visible so the user can always
     * drag the window back.
     */
    title_height = (WORD) (window->work.g_y - window->outer.g_y);
    if (title_height <= 0) {
        title_height = _aes_chrome_height();
    }

    visible_width = 32;
    if (visible_width > window->outer.g_w) {
        visible_width = window->outer.g_w;
    }
    if (visible_width <= 0) {
        visible_width = 1;
    }

    min_x = (WORD) (desktop->g_x - window->outer.g_w + visible_width);
    max_x = (WORD) (desktop->g_x + desktop->g_w - visible_width);
    min_y = _aes_menu_bar_height();
    max_y = (WORD) (desktop->g_y + desktop->g_h - title_height);

    *x = _aes_max_word(min_x, _aes_min_word(*x, max_x));
    *y = _aes_max_word(min_y, _aes_min_word(*y, max_y));
}

static void _aes_toggle_window_iconified(aes_window_t *window)
{
    GRECT previous_outer;
    WORD title_height;
    WORD bottom_border;

    if (window == NULL) {
        return;
    }

    previous_outer = window->outer;
    if (window->iconified != 0) {
        window->outer = window->restored_outer;
        window->iconified = 0;
    } else {
        window->restored_outer = window->outer;
        title_height = (WORD) (window->work.g_y - window->outer.g_y);
        if (title_height <= 0) {
            title_height = _aes_chrome_height();
        }
        bottom_border = (WORD) (window->outer.g_h -
            (window->work.g_y - window->outer.g_y) - window->work.g_h);
        if (bottom_border < 1) {
            bottom_border = 1;
        }
        window->outer.g_h = (WORD) (title_height + bottom_border);
        window->iconified = 1;
    }

    window->previous_outer = previous_outer;
    _aes_compute_work(window);
    if (window->open != 0) {
        _aes_redraw_window_change(&previous_outer, &window->outer);
    }
}


static WORD _aes_build_visible_rects(WORD handle, GRECT out[], WORD max_rects)
{
    GRECT pending[64];
    GRECT next_pending[64];
    GRECT base;
    WORD pending_count = 0;
    size_t i;

    if (out == NULL || max_rects <= 0) {
        return 0;
    }

    if (handle == 0) {
        _aes_desktop_rect(&base);
    } else {
        aes_window_t *window = _aes_find_window(handle);

        if (window == NULL || window->open == 0 || window->work.g_w <= 0 ||
            window->work.g_h <= 0) {
            return 0;
        }
        base = window->work;
    }

    if (base.g_w <= 0 || base.g_h <= 0) {
        return 0;
    }

    pending[0] = base;
    pending_count = 1;

    for (i = 0; i < AES_MAX_WINDOWS && pending_count > 0; ++i) {
        const aes_window_t *cover = &_aes.windows[i];
        WORD next_count = 0;
        WORD j;

        if (cover->used == 0 || cover->open == 0) {
            continue;
        }
        if (handle != 0 &&
            (cover->handle == handle ||
            cover->z_order <= _aes_find_window(handle)->z_order)) {
            continue;
        }

        for (j = 0; j < pending_count; ++j) {
            GRECT fragments[4];
            WORD fragment_count = _aes_subtract_rect(&pending[j],
                &cover->outer, fragments);
            WORD k;

            for (k = 0; k < fragment_count && next_count < max_rects; ++k) {
                next_pending[next_count++] = fragments[k];
            }
        }

        memcpy(pending, next_pending, (size_t) next_count * sizeof(GRECT));
        pending_count = next_count;
    }

    memcpy(out, pending, (size_t) pending_count * sizeof(GRECT));
    return pending_count;
}

static WORD _aes_track_window_interaction(const gem_hid_event_t *first_evt,
    WORD flags, WORD mepbuff[8], WORD *pmx, WORD *pmy, WORD *pmb, WORD *pks)
{
    aes_window_t *window;
    GRECT desktop;
    WORD handle;
    WORD part;
    WORD start_x;
    WORD start_y;
    WORD raised;

    if (first_evt == NULL || first_evt->type != GEM_HID_MOUSE_BUTTON ||
        ((first_evt->button != GEM_HID_BUTTON_LEFT &&
        first_evt->button != GEM_HID_BUTTON_RIGHT)) ||
        ((first_evt->button == GEM_HID_BUTTON_LEFT &&
        (first_evt->flags & GEM_HID_BUTTON_LEFT) == 0u) ||
        (first_evt->button == GEM_HID_BUTTON_RIGHT &&
        (first_evt->flags & GEM_HID_BUTTON_RIGHT) == 0u))) {
        return 0;
    }

    handle = wind_find((WORD) first_evt->x, (WORD) first_evt->y);
    if (handle == 0) {
        return 0;
    }

    window = _aes_find_window(handle);
    if (window == NULL) {
        return 0;
    }

    raised = (_aes_window_is_top(window) == 0) ? 1 : 0;
    if (raised != 0) {
        aes_window_t *previous_top = _aes_find_top_window();
        _aes_raise_window(window);
        _aes_redraw_window_title_states(previous_top, window);
    }

    part = _aes_window_hit_part(window, (WORD) first_evt->x, (WORD) first_evt->y);
    start_x = (WORD) first_evt->x;
    start_y = (WORD) first_evt->y;

    if (part == AES_WINDOW_PART_CLOSER) {
        gem_hid_event_t evt;

        if (_aes_point_in_rect(start_x, start_y, &window->outer) == 0) {
            return 0;
        }

        if (_aes_window_hit_part(window, start_x, start_y) != AES_WINDOW_PART_CLOSER) {
            return 0;
        }

        FOREVER {
            if (gem_hid_poll(&evt) == 0) {
                gem_os_sleep_ms(1u);
                continue;
            }
            if (evt.type == GEM_HID_MOUSE_MOVE ||
                evt.type == GEM_HID_MOUSE_BUTTON) {
                _aes_store_mouse_state(&evt);
                graf_mkstate(pmx, pmy, pmb, pks);
            }
            if (evt.type == GEM_HID_MOUSE_BUTTON &&
                evt.button == GEM_HID_BUTTON_LEFT &&
                (evt.flags & GEM_HID_BUTTON_LEFT) == 0u) {
                if (_aes_window_hit_part(window, (WORD) evt.x,
                    (WORD) evt.y) == AES_WINDOW_PART_CLOSER) {
                    _aes_queue_window_message(window, WM_CLOSED, 0, 0, 0, 0);
                    if ((flags & MU_MESAG) != 0u && mepbuff != NULL &&
                        _aes_dequeue_message(mepbuff) != 0) {
                        return MU_MESAG;
                    }
                }
                return 0;
            }
        }
    }

    if (part == AES_WINDOW_PART_FULLER) {
        if (first_evt->button == GEM_HID_BUTTON_RIGHT) {
            _aes_toggle_window_iconified(window);
            return 0;
        }
        _aes_queue_window_message(window, WM_FULLED, 0, 0, 0, 0);
        if ((flags & MU_MESAG) != 0u && mepbuff != NULL &&
            _aes_dequeue_message(mepbuff) != 0) {
            return MU_MESAG;
        }
        return 0;
    }

    if (part == AES_WINDOW_PART_VUP || part == AES_WINDOW_PART_VDOWN ||
        part == AES_WINDOW_PART_HLEFT || part == AES_WINDOW_PART_HRIGHT ||
        part == AES_WINDOW_PART_VPAGE_UP ||
        part == AES_WINDOW_PART_VPAGE_DOWN ||
        part == AES_WINDOW_PART_HPAGE_LEFT ||
        part == AES_WINDOW_PART_HPAGE_RIGHT) {
        WORD arrow_code = WA_UPLINE;

        switch (part) {
        case AES_WINDOW_PART_VUP:
            arrow_code = WA_UPLINE;
            break;
        case AES_WINDOW_PART_VDOWN:
            arrow_code = WA_DNLINE;
            break;
        case AES_WINDOW_PART_HLEFT:
            arrow_code = WA_LFLINE;
            break;
        case AES_WINDOW_PART_HRIGHT:
            arrow_code = WA_RTLINE;
            break;
        case AES_WINDOW_PART_VPAGE_UP:
            arrow_code = WA_UPPAGE;
            break;
        case AES_WINDOW_PART_VPAGE_DOWN:
            arrow_code = WA_DNPAGE;
            break;
        case AES_WINDOW_PART_HPAGE_LEFT:
            arrow_code = WA_LFPAGE;
            break;
        case AES_WINDOW_PART_HPAGE_RIGHT:
            arrow_code = WA_RTPAGE;
            break;
        default:
            break;
        }

        _aes_queue_window_message(window, WM_ARROWED, arrow_code, 0, 0, 0);
        if ((flags & MU_MESAG) != 0u && mepbuff != NULL &&
            _aes_dequeue_message(mepbuff) != 0) {
            return MU_MESAG;
        }
        return 0;
    }

    if (part == AES_WINDOW_PART_VSLIDE || part == AES_WINDOW_PART_HSLIDE) {
        gem_hid_event_t evt;
        GRECT slot;
        GRECT thumb;
        GRECT drag_rect;
        GRECT last_rect;
        WORD press_offset;
        int drag_drawn = 0;

        if (part == AES_WINDOW_PART_VSLIDE) {
            if (_aes_window_vslot_rect(window, &slot) == 0 ||
                _aes_window_vthumb_rect(window, &thumb) == 0) {
                return 0;
            }
            press_offset = (WORD) (start_y - thumb.g_y);
        } else {
            if (_aes_window_hslot_rect(window, &slot) == 0 ||
                _aes_window_hthumb_rect(window, &thumb) == 0) {
                return 0;
            }
            press_offset = (WORD) (start_x - thumb.g_x);
        }

        drag_rect = thumb;
        last_rect = thumb;
        _vdi_begin_update();
        v_hide_c(_aes.vdi_handle);
        _aes_draw_drag_outline(&drag_rect);
        v_show_c(_aes.vdi_handle, 1);
        _vdi_end_update();
        drag_drawn = 1;

        FOREVER {
            if (gem_hid_poll(&evt) == 0) {
                gem_os_sleep_ms(1u);
                continue;
            }
            if (evt.type == GEM_HID_MOUSE_MOVE ||
                evt.type == GEM_HID_MOUSE_BUTTON) {
                _vdi_begin_update();
                v_hide_c(_aes.vdi_handle);
                _aes_store_mouse_state(&evt);
                if (evt.type == GEM_HID_MOUSE_MOVE) {
                    if (part == AES_WINDOW_PART_VSLIDE) {
                        WORD limit = (WORD) (slot.g_h - thumb.g_h);
                        WORD pos = (WORD) (evt.y - slot.g_y - press_offset);

                        if (limit > 0) {
                            pos = _aes_max_word(0, _aes_min_word(pos, limit));
                        } else {
                            pos = 0;
                        }
                        if (drag_drawn != 0) {
                            _aes_draw_drag_outline(&last_rect);
                        }
                        _aes_set_rect(&drag_rect, thumb.g_x,
                            (WORD) (slot.g_y + pos), thumb.g_w, thumb.g_h);
                    } else {
                        WORD limit = (WORD) (slot.g_w - thumb.g_w);
                        WORD pos = (WORD) (evt.x - slot.g_x - press_offset);

                        if (limit > 0) {
                            pos = _aes_max_word(0, _aes_min_word(pos, limit));
                        } else {
                            pos = 0;
                        }
                        if (drag_drawn != 0) {
                            _aes_draw_drag_outline(&last_rect);
                        }
                        _aes_set_rect(&drag_rect, (WORD) (slot.g_x + pos),
                            thumb.g_y, thumb.g_w, thumb.g_h);
                    }
                    _aes_draw_drag_outline(&drag_rect);
                    last_rect = drag_rect;
                    drag_drawn = 1;
                }
                v_show_c(_aes.vdi_handle, 1);
                _vdi_end_update();
                graf_mkstate(pmx, pmy, pmb, pks);
            }
            if (evt.type == GEM_HID_MOUSE_BUTTON &&
                evt.button == GEM_HID_BUTTON_LEFT &&
                (evt.flags & GEM_HID_BUTTON_LEFT) == 0u) {
                _vdi_begin_update();
                v_hide_c(_aes.vdi_handle);
                _aes_store_mouse_state(&evt);
                if (drag_drawn != 0) {
                    _aes_draw_drag_outline(&last_rect);
                }
                v_show_c(_aes.vdi_handle, 1);
                _vdi_end_update();
                break;
            }
        }

        if (part == AES_WINDOW_PART_VSLIDE) {
            WORD limit = (WORD) (slot.g_h - thumb.g_h);
            WORD pos = (WORD) (drag_rect.g_y - slot.g_y);
            WORD slider = 0;

            if (limit > 0) {
                pos = _aes_max_word(0, _aes_min_word(pos, limit));
                slider = (WORD) ((1000L * pos) / limit);
            }
            _aes_queue_window_message(window, WM_VSLID, slider, 0, 0, 0);
        } else {
            WORD limit = (WORD) (slot.g_w - thumb.g_w);
            WORD pos = (WORD) (drag_rect.g_x - slot.g_x);
            WORD slider = 0;

            if (limit > 0) {
                pos = _aes_max_word(0, _aes_min_word(pos, limit));
                slider = (WORD) ((1000L * pos) / limit);
            }
            _aes_queue_window_message(window, WM_HSLID, slider, 0, 0, 0);
        }

        if ((flags & MU_MESAG) != 0u && mepbuff != NULL &&
            _aes_dequeue_message(mepbuff) != 0) {
            return MU_MESAG;
        }
        return 0;
    }

    if (part == AES_WINDOW_PART_TITLE && (window->kind & MOVER) != 0u) {
        gem_hid_event_t evt;
        GRECT original = window->outer;
        GRECT drag_rect = original;
        GRECT last_rect = original;
        int drag_drawn = 0;

        _aes_desktop_rect(&desktop);
        _vdi_begin_update();
        v_hide_c(_aes.vdi_handle);
        _aes_draw_drag_outline(&drag_rect);
        v_show_c(_aes.vdi_handle, 1);
        _vdi_end_update();
        drag_drawn = 1;

        FOREVER {
            if (gem_hid_poll(&evt) == 0) {
                gem_os_sleep_ms(1u);
                continue;
            }
            if (evt.type == GEM_HID_MOUSE_MOVE ||
                evt.type == GEM_HID_MOUSE_BUTTON) {
                WORD new_x;
                WORD new_y;

                _vdi_begin_update();
                v_hide_c(_aes.vdi_handle);
                _aes_store_mouse_state(&evt);

                new_x = (WORD) (original.g_x + evt.x - start_x);
                new_y = (WORD) (original.g_y + evt.y - start_y);
                _aes_clamp_dragged_window_position(window, &desktop,
                    &new_x, &new_y);
                if (new_x != drag_rect.g_x || new_y != drag_rect.g_y) {
                    if (drag_drawn != 0) {
                        _aes_draw_drag_outline(&last_rect);
                    }
                    _aes_set_rect(&drag_rect, new_x, new_y, original.g_w,
                        original.g_h);
                    _aes_draw_drag_outline(&drag_rect);
                    last_rect = drag_rect;
                    drag_drawn = 1;
                }
                v_show_c(_aes.vdi_handle, 1);
                _vdi_end_update();
                graf_mkstate(pmx, pmy, pmb, pks);
            }
            if (evt.type == GEM_HID_MOUSE_BUTTON &&
                evt.button == GEM_HID_BUTTON_LEFT &&
                (evt.flags & GEM_HID_BUTTON_LEFT) == 0u) {
                _vdi_begin_update();
                v_hide_c(_aes.vdi_handle);
                _aes_store_mouse_state(&evt);
                if (drag_drawn != 0) {
                    _aes_draw_drag_outline(&last_rect);
                }
                v_show_c(_aes.vdi_handle, 1);
                _vdi_end_update();
                if (window->iconified != 0) {
                    window->restored_outer.g_x = drag_rect.g_x;
                    window->restored_outer.g_y = drag_rect.g_y;
                }
                _aes_queue_window_message(window, WM_MOVED, drag_rect.g_x,
                    drag_rect.g_y, drag_rect.g_w, drag_rect.g_h);
                if ((flags & MU_MESAG) != 0u && mepbuff != NULL &&
                    _aes_dequeue_message(mepbuff) != 0) {
                    return MU_MESAG;
                }
                return 0;
            }
        }
    }

    if (part == AES_WINDOW_PART_SIZER && (window->kind & SIZER) != 0u) {
        gem_hid_event_t evt;
        GRECT original = window->outer;
        GRECT drag_rect = original;
        GRECT last_rect = original;
        int drag_drawn = 0;

        _aes_desktop_rect(&desktop);
        _vdi_begin_update();
        v_hide_c(_aes.vdi_handle);
        _aes_draw_drag_outline(&drag_rect);
        v_show_c(_aes.vdi_handle, 1);
        _vdi_end_update();
        drag_drawn = 1;

        FOREVER {
            if (gem_hid_poll(&evt) == 0) {
                gem_os_sleep_ms(1u);
                continue;
            }
            if (evt.type == GEM_HID_MOUSE_MOVE ||
                evt.type == GEM_HID_MOUSE_BUTTON) {
                WORD new_w;
                WORD new_h;

                _vdi_begin_update();
                v_hide_c(_aes.vdi_handle);
                _aes_store_mouse_state(&evt);
                new_w = (WORD) (original.g_w + evt.x - start_x);
                new_h = (WORD) (original.g_h + evt.y - start_y);
                new_w = _aes_max_word(96, _aes_min_word(new_w,
                    (WORD) (desktop.g_x + desktop.g_w - original.g_x)));
                new_h = _aes_max_word(64, _aes_min_word(new_h,
                    (WORD) (desktop.g_y + desktop.g_h - original.g_y)));
                if (new_w != drag_rect.g_w || new_h != drag_rect.g_h) {
                    if (drag_drawn != 0) {
                        _aes_draw_drag_outline(&last_rect);
                    }
                    _aes_set_rect(&drag_rect, original.g_x, original.g_y,
                        new_w, new_h);
                    _aes_draw_drag_outline(&drag_rect);
                    last_rect = drag_rect;
                    drag_drawn = 1;
                }
                v_show_c(_aes.vdi_handle, 1);
                _vdi_end_update();
                graf_mkstate(pmx, pmy, pmb, pks);
            }
            if (evt.type == GEM_HID_MOUSE_BUTTON &&
                evt.button == GEM_HID_BUTTON_LEFT &&
                (evt.flags & GEM_HID_BUTTON_LEFT) == 0u) {
                _vdi_begin_update();
                v_hide_c(_aes.vdi_handle);
                _aes_store_mouse_state(&evt);
                if (drag_drawn != 0) {
                    _aes_draw_drag_outline(&last_rect);
                }
                v_show_c(_aes.vdi_handle, 1);
                _vdi_end_update();
                _aes_queue_window_message(window, WM_SIZED, drag_rect.g_x,
                    drag_rect.g_y, drag_rect.g_w, drag_rect.g_h);
                if ((flags & MU_MESAG) != 0u && mepbuff != NULL &&
                    _aes_dequeue_message(mepbuff) != 0) {
                    return MU_MESAG;
                }
                return 0;
            }
        }
    }

    if (raised != 0) {
        _aes_queue_window_message(window, WM_TOPPED, 0, 0, 0, 0);
        if ((flags & MU_MESAG) != 0u && mepbuff != NULL &&
            _aes_dequeue_message(mepbuff) != 0) {
            return MU_MESAG;
        }
    }
    return 0;
}

static int _aes_resource_contains_ptr(const void *ptr)
{
    uintptr_t base;
    uintptr_t end;
    uintptr_t value;

    if (_aes.resource_data == NULL || ptr == NULL) {
        return 0;
    }

    base = (uintptr_t) _aes.resource_data;
    end = base + _aes.resource_size;
    value = (uintptr_t) ptr;
    return value >= base && value < end;
}

static LONG _aes_resource_fix_offset(LONG value)
{
    uintptr_t offset;

    if (_aes.resource_data == NULL || value <= 0) {
        return value;
    }

    offset = (uintptr_t) value;
    if (offset >= _aes.resource_size) {
        return value;
    }

    return (LONG) ((uintptr_t) _aes.resource_data + offset);
}

static void _aes_resource_fix_tedinfo(TEDINFO *ted)
{
    if (ted == NULL || !_aes_resource_contains_ptr(ted)) {
        return;
    }

    ted->te_ptext = _aes_resource_fix_offset(ted->te_ptext);
    ted->te_ptmplt = _aes_resource_fix_offset(ted->te_ptmplt);
    ted->te_pvalid = _aes_resource_fix_offset(ted->te_pvalid);
}

static void _aes_resource_fix_iconblk(ICONBLK *icon)
{
    if (icon == NULL || !_aes_resource_contains_ptr(icon)) {
        return;
    }

    icon->ib_pmask = _aes_resource_fix_offset(icon->ib_pmask);
    icon->ib_pdata = _aes_resource_fix_offset(icon->ib_pdata);
    icon->ib_ptext = _aes_resource_fix_offset(icon->ib_ptext);
}

static void _aes_resource_fix_bitblk(BITBLK *bitblk)
{
    if (bitblk == NULL || !_aes_resource_contains_ptr(bitblk)) {
        return;
    }

    bitblk->bi_pdata = _aes_resource_fix_offset(bitblk->bi_pdata);
}

/*
 * Allocates one hosted AES application slot for the current caller and
 * lazily initializes the shared runtime state on first use.
 */
WORD appl_init(void)
{
    size_t i;

    if (_aes.initialized == 0) {
        _aes_reset_state();
        _aes.initialized = 1;
    }

    for (i = 0; i < AES_MAX_APPS; ++i) {
        if (_aes.apps[i].used == 0) {
            _aes.apps[i].used = 1;
            _aes.apps[i].id = _aes.next_app_id++;
            strcpy(_aes.apps[i].name, "APP");
            _aes.current_app_id = _aes.apps[i].id;
            global[2] = _aes.current_app_id;
            _aes_trace("appl_init return id=%d", _aes.current_app_id);
            return _aes.current_app_id;
        }
    }
    _aes_trace("appl_init failed");
    return 0;
}

/*
 * Releases the current AES application slot and closes the hosted VDI
 * workstation when the last active application exits.
 */
WORD appl_exit(void)
{
    aes_app_t *app = _aes_find_app_by_id(_aes.current_app_id);

    if (app != NULL) {
        memset(app, 0, sizeof(*app));
    }
    if (_aes.vdi_ready != 0 && _aes.vdi_handle != 0) {
        v_clsvwk(_aes.vdi_handle);
        _aes.vdi_handle = 0;
        _aes.vdi_ready = 0;
    }
    _aes.current_app_id = 0;
    global[2] = 0;
    return 1;
}

/*
 * Looks up an AES application id by its registered short name.
 */
WORD appl_find(char *name)
{
    size_t i;

    if (name == NULL) {
        return 0;
    }

    for (i = 0; i < AES_MAX_APPS; ++i) {
        if (_aes.apps[i].used != 0 &&
            strncmp(_aes.apps[i].name, name, 8) == 0) {
            return _aes.apps[i].id;
        }
    }
    return 0;
}

/*
 * Stores the desktop boot-volume bitmasks reported by the legacy
 * desktop shell.
 */
WORD appl_bvset(WORD bvdisk, WORD bvhard)
{
    _aes.desktop_bvdisk = bvdisk;
    _aes.desktop_bvhard = bvhard;
    return 1;
}

/*
 * Queues one 8-word AES message for another hosted application.
 */
WORD appl_write(WORD ap_wid, WORD ap_wlength, void *ap_wpbuff)
{
    size_t i;
    WORD *words = (WORD *) ap_wpbuff;

    if (ap_wpbuff == NULL || ap_wlength < 8) {
        return 0;
    }

    for (i = 0; i < AES_MAX_MESSAGES; ++i) {
        if (_aes.messages[i].used == 0) {
            _aes.messages[i].used = 1;
            _aes.messages[i].dest = ap_wid;
            memcpy(_aes.messages[i].data, words,
                sizeof(_aes.messages[i].data));
            return 1;
        }
    }
    return 0;
}

/*
 * Dequeues the next pending AES message for one hosted application id.
 */
WORD appl_read(WORD ap_rwid, WORD ap_rlength, void *ap_rpbuff)
{
    size_t i;

    if (ap_rpbuff == NULL || ap_rlength < 8) {
        return 0;
    }

    for (i = 0; i < AES_MAX_MESSAGES; ++i) {
        if (_aes.messages[i].used != 0 &&
            _aes.messages[i].dest == ap_rwid) {
            memcpy(ap_rpbuff, _aes.messages[i].data,
                sizeof(_aes.messages[i].data));
            _aes.messages[i].used = 0;
            return 1;
        }
    }
    return 0;
}

/*
 * Accepts a tape-play request and reports success without emulating the
 * historical playback stream.
 */
WORD appl_tplay(void *pbuff, WORD length, WORD scale)
{
    (void) pbuff;
    (void) length;
    (void) scale;
    return 1;
}

/*
 * Accepts a tape-record request and clears the caller's buffer.
 */
WORD appl_trecord(void *pbuff, WORD length)
{
    if (pbuff != NULL && length > 0) {
        memset(pbuff, 0, (size_t) length);
    }
    return 1;
}

/*
 * Yields briefly to the host so polling loops can stay responsive.
 */
WORD appl_yield(void)
{
    gem_os_sleep_ms(1u);
    return 1;
}

/*
 * Blocks until the next key-down event and returns its hosted key code.
 */
WORD evnt_keybd(void)
{
    gem_hid_event_t evt;

    FOREVER {
        if (gem_hid_poll(&evt) != 0) {
            if (evt.type == GEM_HID_MOUSE_MOVE ||
                evt.type == GEM_HID_MOUSE_BUTTON) {
                _aes_store_mouse_state(&evt);
                continue;
            }
            if (evt.type == GEM_HID_KEY && (evt.flags & 1u) != 0u) {
                return (WORD) evt.key;
            }
        }
        gem_os_sleep_ms(1u);
    }
}

/*
 * Samples the current mouse-button state and compares it against the
 * requested mask and state.
 */
WORD evnt_button(WORD clicks,
                 UWORD mask,
                 UWORD state,
                 WORD *pmx,
                 WORD *pmy,
                 WORD *pmb,
                 WORD *pks)
{
    WORD buttons;

    (void) clicks;

    graf_mkstate(pmx, pmy, &buttons, pks);
    if (pmb != NULL) {
        *pmb = buttons;
    }
    return (((UWORD) buttons & mask) == state) ? 1 : 0;
}

/*
 * Tests whether the current mouse position is inside or outside one
 * rectangle and returns the result according to the AES flag contract.
 */
WORD evnt_mouse(WORD flags,
                WORD x,
                WORD y,
                WORD w,
                WORD h,
                WORD *pmx,
                WORD *pmy,
                WORD *pmb,
                WORD *pks)
{
    WORD mx;
    WORD my;
    WORD mb;
    WORD ks;
    GRECT rect;
    int inside;

    graf_mkstate(&mx, &my, &mb, &ks);
    _aes_set_rect(&rect, x, y, w, h);
    inside = _aes_point_in_rect(mx, my, &rect);
    if (pmx != NULL) {
        *pmx = mx;
    }
    if (pmy != NULL) {
        *pmy = my;
    }
    if (pmb != NULL) {
        *pmb = mb;
    }
    if (pks != NULL) {
        *pks = ks;
    }
    return (flags != 0) ? inside : !inside;
}

/*
 * Fetches the next queued AES message for the current application.
 */
WORD evnt_mesag(WORD msg[8])
{
    FOREVER {
        gem_hid_event_t evt;

        if (_aes_dequeue_message(msg) != 0) {
            return 1;
        }

        if (gem_hid_poll(&evt) != 0) {
            if (_aes.menu_visible != 0 && _aes.menu_tree != NULL &&
                _aes_menu_event(_aes.menu_tree, &evt, msg) != 0) {
                return 1;
            }

            if (_aes_track_window_interaction(&evt, MU_MESAG, msg,
                NULL, NULL, NULL, NULL) == MU_MESAG) {
                return 1;
            }

            if (evt.type == GEM_HID_MOUSE_MOVE ||
                evt.type == GEM_HID_MOUSE_BUTTON) {
                _aes_store_mouse_state(&evt);
            }
        }

        gem_os_sleep_ms(1u);
    }
}

/*
 * Sleeps for the requested 32-bit AES timer duration.
 */
WORD evnt_timer(WORD count_low, WORD count_high)
{
    uint32_t duration = (uint32_t) (uint16_t) count_low |
        ((uint32_t) (uint16_t) count_high << 16);

    gem_os_sleep_ms(duration);
    return 1;
}

/*
 * Waits for the next hosted event that matches the requested AES event
 * mask, including menu interactions synthesized as `MU_MESAG`.
 */
WORD evnt_multi(UWORD flags,
                UWORD bclk,
                UWORD bmsk,
                UWORD bst,
                UWORD m1flags,
                WORD m1x,
                WORD m1y,
                WORD m1w,
                WORD m1h,
                UWORD m2flags,
                WORD m2x,
                WORD m2y,
                WORD m2w,
                WORD m2h,
                WORD mepbuff[8],
                UWORD tlc,
                UWORD thc,
                WORD *pmx,
                WORD *pmy,
                WORD *pmb,
                WORD *pks,
                WORD *pkr,
                WORD *pbr)
{
    uint32_t timeout = (uint32_t) tlc | ((uint32_t) thc << 16);
    uint32_t start = gem_os_ticks_ms();

    (void) bclk;

    FOREVER {
        gem_hid_event_t evt;
        WORD mx = 0;
        WORD my = 0;
        WORD mb = 0;
        WORD ks = 0;

        if ((flags & MU_MESAG) != 0u && mepbuff != NULL &&
            _aes_dequeue_message(mepbuff) != 0) {
            return MU_MESAG;
        }

        if (gem_hid_poll(&evt) != 0) {
            if ((flags & MU_MESAG) != 0u && mepbuff != NULL &&
                _aes.menu_visible != 0 && _aes.menu_tree != NULL &&
                _aes_menu_event(_aes.menu_tree, &evt, mepbuff) != 0) {
                graf_mkstate(pmx, pmy, pmb, pks);
                return MU_MESAG;
            }

            if ((flags & MU_MESAG) != 0u && mepbuff != NULL) {
                WORD window_event = _aes_track_window_interaction(&evt, flags,
                    mepbuff, pmx, pmy, pmb, pks);

                if (window_event != 0) {
                    return window_event;
                }
            }

            if (evt.type == GEM_HID_MOUSE_MOVE ||
                evt.type == GEM_HID_MOUSE_BUTTON) {
                _aes_store_mouse_state(&evt);
                graf_mkstate(&mx, &my, &mb, &ks);
                if (pmx != NULL) {
                    *pmx = mx;
                }
                if (pmy != NULL) {
                    *pmy = my;
                }
                if (pmb != NULL) {
                    *pmb = mb;
                }
                if (pks != NULL) {
                    *pks = ks;
                }

                if ((flags & MU_BUTTON) != 0u &&
                    evt.type == GEM_HID_MOUSE_BUTTON &&
                    (((UWORD) mb & bmsk) == bst)) {
                    if (pbr != NULL) {
                        *pbr = 1;
                    }
                    return MU_BUTTON;
                }
                if ((flags & MU_M1) != 0u &&
                    evnt_mouse((WORD) m1flags, m1x, m1y, m1w, m1h,
                        pmx, pmy, pmb, pks) != 0) {
                    return MU_M1;
                }
                if ((flags & MU_M2) != 0u &&
                    evnt_mouse((WORD) m2flags, m2x, m2y, m2w, m2h,
                        pmx, pmy, pmb, pks) != 0) {
                    return MU_M2;
                }
                continue;
            }

            if ((flags & MU_KEYBD) != 0u &&
                evt.type == GEM_HID_KEY &&
                (evt.flags & 1u) != 0u) {
                if (pkr != NULL) {
                    *pkr = (WORD) evt.key;
                }
                graf_mkstate(pmx, pmy, pmb, pks);
                return MU_KEYBD;
            }
        }

        if ((flags & MU_TIMER) != 0u) {
            uint32_t now = gem_os_ticks_ms();

            if ((uint32_t) (now - start) >= timeout) {
                graf_mkstate(pmx, pmy, pmb, pks);
                return MU_TIMER;
            }
        }

        gem_os_sleep_ms(1u);
    }
}

/*
 * Gets or sets the desktop double-click preference value.
 */
WORD evnt_dclick(WORD clicks, WORD setget)
{
    if (setget != 0) {
        _aes.dclick_rate = clicks;
    }
    return _aes.dclick_rate;
}

/*
 * Registers or unregisters the active menu tree with the hosted AES.
 */
WORD menu_bar(OBJECT *tree, WORD show)
{
    _aes.menu_tree = tree;
    _aes.menu_visible = (show != 0) ? 1 : 0;
    if (show != 0 && tree != NULL) {
        _aes_menu_prepare_tree(tree);
        _aes_menu_hide_popups(tree);
        _aes_menu_redraw_tree(tree);
    } else {
        _aes_menu_clear_saved_region();
    }
    _aes_trace("menu_bar tree=%p show=%d", (void *) tree, show);
    return 1;
}

/*
 * Sets or clears the checkmark state on one menu item.
 */
WORD menu_icheck(OBJECT *tree, WORD item, WORD check)
{
    if (tree == NULL || item < 0) {
        return 0;
    }
    if (check != 0) {
        tree[item].ob_state |= CHECKED;
    } else {
        tree[item].ob_state &= (UWORD) ~CHECKED;
    }
    return 1;
}

/*
 * Enables or disables one menu item.
 */
WORD menu_ienable(OBJECT *tree, WORD item, WORD enable)
{
    if (tree == NULL || item < 0) {
        return 0;
    }
    if (enable != 0) {
        tree[item].ob_state &= (UWORD) ~DISABLED;
    } else {
        tree[item].ob_state |= DISABLED;
    }
    return 1;
}

/*
 * Sets or clears the selected state on one menu title.
 */
WORD menu_tnormal(OBJECT *tree, WORD title, WORD normal)
{
    if (tree == NULL || title < 0) {
        return 0;
    }
    if (normal != 0) {
        tree[title].ob_state &= (UWORD) ~SELECTED;
    } else {
        tree[title].ob_state |= SELECTED;
    }
    return 1;
}

/*
 * Replaces one menu item's text pointer.
 */
WORD menu_text(OBJECT *tree, WORD item, char *text)
{
    if (tree == NULL || item < 0) {
        return 0;
    }
    tree[item].ob_spec = (LONG) (intptr_t) text;
    return 1;
}

/*
 * Stores a short display name for one hosted AES application.
 */
WORD menu_register(WORD apid, char *name)
{
    aes_app_t *app = _aes_find_app_by_id(apid);

    if (app == NULL || name == NULL) {
        return 0;
    }
    strncpy(app->name, name, sizeof(app->name) - 1u);
    app->name[sizeof(app->name) - 1u] = '\0';
    return apid;
}

/*
 * Reports whether a registered menu owner id still exists.
 */
WORD menu_unregister(WORD mid)
{
    return (_aes_find_app_by_id(mid) != NULL) ? 1 : 0;
}

/*
 * Gets or sets the hosted "menu click" preference.
 */
WORD menu_click(WORD click, WORD setit)
{
    if (setit != 0) {
        _aes.menu_click = click;
    }
    return _aes.menu_click;
}

/*
 * Appends one child object to a parent's child list.
 */
WORD objc_add(OBJECT *tree, WORD parent, WORD child)
{
    WORD last;

    if (tree == NULL || parent < 0 || child < 0) {
        return 0;
    }

    if (tree[parent].ob_head == NIL) {
        tree[parent].ob_head = child;
    } else {
        last = tree[parent].ob_tail;
        tree[last].ob_next = child;
    }
    tree[parent].ob_tail = child;
    tree[child].ob_next = parent;
    return 1;
}

/*
 * Removes one object from its parent's child list.
 */
WORD objc_delete(OBJECT *tree, WORD object)
{
    size_t i;

    if (tree == NULL || object < 0) {
        return 0;
    }

    for (i = 0; i < 1024u; ++i) {
        if (tree[i].ob_head == object) {
            tree[i].ob_head = tree[object].ob_next;
            if (tree[i].ob_tail == object) {
                tree[i].ob_tail = NIL;
            }
            break;
        }
        if (tree[i].ob_head != NIL) {
            WORD node = tree[i].ob_head;

            while (node != NIL && node != (WORD) i) {
                if (tree[node].ob_next == object) {
                    tree[node].ob_next = tree[object].ob_next;
                    if (tree[i].ob_tail == object) {
                        tree[i].ob_tail = node;
                    }
                    break;
                }
                node = tree[node].ob_next;
            }
        }
    }
    tree[object].ob_next = NIL;
    return 1;
}

/*
 * Draws an object subtree clipped to the requested rectangle.
 */
WORD objc_draw(OBJECT *tree,
               WORD startob,
               WORD depth,
               WORD xc,
               WORD yc,
               WORD wc,
               WORD hc)
{
    WORD clip[4];
    WORD origin_x = 0;
    WORD origin_y = 0;
    WORD text_attrib[10];
    WORD previous_font = 0;
    WORD restore_font = 0;

    (void) depth;

    if (tree == NULL || startob < 0 || _aes_ensure_vdi() == 0) {
        _aes_trace("objc_draw skipped tree=%p start=%d vdi=%d",
            (void *) tree, startob, _aes.vdi_ready);
        return 0;
    }

    _aes_trace("objc_draw tree=%p start=%d depth=%d clip=%d,%d %dx%d type=%u",
        (void *) tree, startob, depth, xc, yc, wc, hc,
        (unsigned) tree[startob].ob_type);

    if (vqt_attributes(_aes.vdi_handle, text_attrib) != 0) {
        previous_font = text_attrib[0];
        restore_font = 1;
    }
    (void) vst_font(_aes.vdi_handle, 1);

    clip[0] = xc;
    clip[1] = yc;
    clip[2] = (WORD) (xc + wc - 1);
    clip[3] = (WORD) (yc + hc - 1);
    _aes_object_extent(tree, startob, &origin_x, &origin_y);
    vs_clip(_aes.vdi_handle, 1, clip);
    _aes_draw_tree_recursive(tree, startob,
        (WORD) (origin_x - tree[startob].ob_x),
        (WORD) (origin_y - tree[startob].ob_y), depth, clip);
    vs_clip(_aes.vdi_handle, 0, clip);
    if (restore_font != 0) {
        (void) vst_font(_aes.vdi_handle, previous_font);
    }
    return 1;
}

/*
 * Returns the deepest object hit within one subtree.
 */
WORD objc_find(OBJECT *tree, WORD startob, WORD depth, WORD mx, WORD my)
{
    if (tree == NULL || startob < 0) {
        return NIL;
    }

    return _aes_find_in_subtree(tree, startob, 0, 0, depth, mx, my);
}

/*
 * Computes the absolute coordinates of one object.
 */
WORD objc_offset(OBJECT *tree, WORD object, WORD *x, WORD *y)
{
    if (tree == NULL || object < 0) {
        return 0;
    }

    _aes_object_extent(tree, object, x, y);
    return 1;
}

/*
 * Accepts an object-reordering request but currently leaves the object
 * list unchanged.
 */
WORD objc_order(OBJECT *tree, WORD object, WORD newpos)
{
    (void) tree;
    (void) object;
    (void) newpos;
    return 1;
}

/*
 * Appends one printable character into a hosted editable text field.
 */
WORD objc_edit(OBJECT *tree, WORD object, WORD charidx, WORD *idx, WORD kind)
{
    TEDINFO *ted;
    char *text;
    size_t length;

    if (tree == NULL || object < 0 || idx == NULL) {
        return 0;
    }
    if (kind == EDSTART || kind == EDINIT) {
        _aes.edit_tree = tree;
        _aes.edit_object = object;
        _aes.edit_index = *idx;
        return 1;
    }
    if (kind == EDEND) {
        if (_aes.edit_tree == tree && _aes.edit_object == object) {
            _aes.edit_tree = NULL;
            _aes.edit_object = NIL;
            _aes.edit_index = 0;
        }
        return 1;
    }
    if (kind != EDCHAR || tree[object].ob_spec == 0) {
        return 1;
    }

    ted = (TEDINFO *) (intptr_t) tree[object].ob_spec;
    text = (char *) (intptr_t) ted->te_ptext;
    if (text == NULL) {
        return 0;
    }

    length = strlen(text);
    if (*idx < ted->te_txtlen - 1 && charidx >= 32 && charidx <= 126) {
        if ((size_t) *idx > length) {
            *idx = (WORD) length;
        }
        if (length < (size_t) (ted->te_txtlen - 1)) {
            memmove(&text[*idx + 1], &text[*idx],
                length - (size_t) *idx + 1u);
            text[*idx] = (char) charidx;
            ++(*idx);
        } else if ((size_t) *idx < length) {
            memmove(&text[*idx + 1], &text[*idx],
                length - (size_t) *idx);
            text[*idx] = (char) charidx;
            text[ted->te_txtlen - 1] = '\0';
            ++(*idx);
        } else {
            text[*idx] = (char) charidx;
            text[*idx + 1] = '\0';
            ++(*idx);
        }
    }
    _aes.edit_tree = tree;
    _aes.edit_object = object;
    _aes.edit_index = *idx;
    return 1;
}

/*
 * Changes one object's state and optionally redraws it.
 */
WORD objc_change(OBJECT *tree,
                 WORD object,
                 WORD depth,
                 WORD xc,
                 WORD yc,
                 WORD wc,
                 WORD hc,
                 WORD newstate,
                 WORD redraw)
{
    if (tree == NULL || object < 0) {
        return 0;
    }

    (void) depth;
    tree[object].ob_state = (UWORD) newstate;
    if (redraw != 0) {
        return objc_draw(tree, object, 0, xc, yc, wc, hc);
    }
    return 1;
}

/*
 * Starts a form interaction and currently returns the starting object.
 */
WORD form_do(OBJECT *tree, WORD startob)
{
    if (tree == NULL) {
        return 0;
    }

    return startob;
}

/*
 * Accepts a form-dial animation request without animating on the host.
 */
WORD form_dial(WORD flag,
               WORD x1,
               WORD y1,
               WORD w1,
               WORD h1,
               WORD x2,
               WORD y2,
               WORD w2,
               WORD h2)
{
    GRECT dirty;
    GRECT desktop;

    (void) flag;
    (void) x1;
    (void) y1;
    (void) w1;
    (void) h1;
    (void) x2;
    (void) y2;
    (void) w2;
    (void) h2;

    if (flag == FMD_START || flag == FMD_GROW) {
        _aes_desktop_rect(&desktop);
        if (desktop.g_w > 0 && desktop.g_h > 0) {
            _aes_redraw_region(&desktop);
        }
    }

    if (flag == FMD_FINISH || flag == FMD_SHRINK) {
        _aes_set_rect(&dirty, x2, y2, w2, h2);
        _aes_redraw_region(&dirty);
    }

    return 1;
}

/*
 * Displays a simple hosted alert box parsed from the classic AES alert
 * string syntax and returns the selected button number.
 */
WORD form_alert(WORD defbtn, char *str)
{
    aes_alert_t alert;

    if (_aes_parse_alert(str, defbtn, &alert) == 0) {
        return (defbtn > 0) ? defbtn : 1;
    }
    _aes_alert_compute_layout(&alert);
    return _aes_run_alert(&alert);
}

/*
 * Converts an AES form-error request into a simple alert return value.
 */
WORD form_error(WORD errnum)
{
    switch (errnum) {
    case 2:
        return form_alert(1, "[3][ File not found ][ OK ]");
    case 8:
        return form_alert(1, "[3][ Insufficient memory ][ OK ]");
    default:
        return form_alert(1, "[1][ AES form error ][ OK ]");
    }
}

/*
 * Centers one form on the hosted desktop work area.
 */
WORD form_center(OBJECT *tree, WORD *cx, WORD *cy, WORD *cw, WORD *ch)
{
    WORD width = 200;
    WORD height = 100;

    if (tree != NULL) {
        width = tree[ROOT].ob_width;
        height = tree[ROOT].ob_height;
    }
    if (_aes_ensure_vdi() != 0) {
        if (cx != NULL) {
            *cx = (WORD) ((_aes.work_out[0] + 1 - width) / 2);
        }
        if (cy != NULL) {
            *cy = (WORD) ((_aes.work_out[1] + 1 - height) / 2);
        }
    } else {
        if (cx != NULL) {
            *cx = 0;
        }
        if (cy != NULL) {
            *cy = 0;
        }
    }
    if (cw != NULL) {
        *cw = width;
    }
    if (ch != NULL) {
        *ch = height;
    }
    return 1;
}

/*
 * Passes one key event through the hosted form keyboard helper.
 */
WORD form_keybd(OBJECT *tree,
                WORD object,
                WORD next,
                WORD thechar,
                WORD *newobj,
                WORD *newchar)
{
    (void) tree;
    if (newobj != NULL) {
        *newobj = next;
    }
    if (newchar != NULL) {
        *newchar = thechar;
    }
    return object;
}

/*
 * Toggles the selected state on a form button object.
 */
WORD form_button(OBJECT *tree, WORD object, WORD clicks, WORD *newobj)
{
    (void) clicks;

    if (tree == NULL || object < 0) {
        return 0;
    }
    tree[object].ob_state ^= SELECTED;
    if (newobj != NULL) {
        *newobj = object;
    }
    return 1;
}

/*
 * Accepts a rubber-box request and reports the requested minimum size.
 */
WORD graf_rubbox(WORD xorigin,
                 WORD yorigin,
                 WORD wmin,
                 WORD hmin,
                 WORD *pwend,
                 WORD *phend)
{
    if (pwend != NULL) {
        *pwend = wmin;
    }
    if (phend != NULL) {
        *phend = hmin;
    }
    return (xorigin | yorigin) == (xorigin | yorigin);
}

/*
 * Constrains a dragged box inside a bounding rectangle.
 */
WORD graf_dragbox(WORD w,
                  WORD h,
                  WORD sx,
                  WORD sy,
                  WORD xc,
                  WORD yc,
                  WORD wc,
                  WORD hc,
                  WORD *pdx,
                  WORD *pdy)
{
    if (pdx != NULL) {
        *pdx = _aes_max_word(xc, _aes_min_word(sx, (WORD) (xc + wc - w)));
    }
    if (pdy != NULL) {
        *pdy = _aes_max_word(yc, _aes_min_word(sy, (WORD) (yc + hc - h)));
    }
    return 1;
}

/*
 * Accepts a grow-box copy request without animating it.
 */
WORD graf_mbox(WORD w, WORD h, WORD srcx, WORD srcy, WORD dstx, WORD dsty)
{
    (void) w;
    (void) h;
    (void) srcx;
    (void) srcy;
    (void) dstx;
    (void) dsty;
    return 1;
}

/*
 * Accepts a grow-box animation request without animating it.
 */
WORD graf_growbox(WORD x1,
                  WORD y1,
                  WORD w1,
                  WORD h1,
                  WORD x2,
                  WORD y2,
                  WORD w2,
                  WORD h2)
{
    (void) x1;
    (void) y1;
    (void) w1;
    (void) h1;
    (void) x2;
    (void) y2;
    (void) w2;
    (void) h2;
    return 1;
}

/*
 * Reuses the grow-box stub for shrink-box requests.
 */
WORD graf_shrinkbox(WORD x1,
                    WORD y1,
                    WORD w1,
                    WORD h1,
                    WORD x2,
                    WORD y2,
                    WORD w2,
                    WORD h2)
{
    return graf_growbox(x1, y1, w1, h1, x2, y2, w2, h2);
}

/*
 * Simulates a watch-box transition by storing the requested in/out
 * states sequentially.
 */
WORD graf_watchbox(OBJECT *tree, WORD object, UWORD in_state, UWORD out_state)
{
    if (tree == NULL || object < 0) {
        return 0;
    }
    tree[object].ob_state = in_state;
    tree[object].ob_state = out_state;
    return 1;
}

/*
 * Returns a scrollbar slider position scaled into the historical
 * 0..1000 AES range.
 */
WORD graf_slidebox(OBJECT *tree, WORD parent, WORD object, WORD orientation)
{
    OBJECT *parent_obj;
    OBJECT *child_obj;

    (void) orientation;

    if (tree == NULL || parent < 0 || object < 0) {
        return 0;
    }

    parent_obj = &tree[parent];
    child_obj = &tree[object];
    if (parent_obj->ob_height == 0) {
        return 0;
    }
    return (WORD) ((1000L * child_obj->ob_y) /
        _aes_max_word(parent_obj->ob_height, 1));
}

/*
 * Returns the hosted character and box metrics together with the VDI
 * handle.
 */
WORD graf_handle(WORD *charw, WORD *charh, WORD *boxw, WORD *boxh)
{
    if (_aes_ensure_vdi() == 0) {
        return 0;
    }
    if (charw != NULL) {
        *charw = AES_CHAR_WIDTH;
    }
    if (charh != NULL) {
        *charh = AES_CHAR_HEIGHT;
    }
    if (boxw != NULL) {
        *boxw = 8;
    }
    if (boxh != NULL) {
        *boxh = _aes_menu_chrome_height();
    }
    return _aes.vdi_handle;
}

/*
 * Shows, hides, or replaces the hosted mouse form.
 */
WORD graf_mouse(WORD mode, void *form)
{
    if (_aes_ensure_vdi() == 0) {
        return 0;
    }

    if (mode == M_OFF) {
        v_hide_c(_aes.vdi_handle);
    } else if (mode == M_ON) {
        v_show_c(_aes.vdi_handle, 1);
    } else if (mode == USER_DEF && form != NULL) {
        (void) vsc_form(_aes.vdi_handle, (MFORM *) form);
    } else if (mode >= ARROW && mode <= OUTLN_CROSS) {
        (void) _vdi_select_system_mouse_form(mode);
    }
    return 1;
}

/*
 * Samples the current hosted mouse position and button state.
 */
VOID graf_mkstate(WORD *pmx, WORD *pmy, WORD *pmb, WORD *pks)
{
    WORD status = 0;
    WORD x = 0;
    WORD y = 0;

    if (_aes_ensure_vdi() != 0) {
        vq_mouse(_aes.vdi_handle, &status, &x, &y);
    }
    if (pmx != NULL) {
        *pmx = x;
    }
    if (pmy != NULL) {
        *pmy = y;
    }
    if (pmb != NULL) {
        *pmb = status;
    }
    if (pks != NULL) {
        *pks = 0;
    }
}

/*
 * Copies the hosted scrap path into the caller's buffer.
 */
WORD scrp_read(char *pscrap)
{
    if (pscrap == NULL) {
        return 0;
    }
    strcpy(pscrap, _aes.scrap_path);
    return (_aes.scrap_path[0] != '\0') ? 1 : 0;
}

/*
 * Replaces the hosted scrap path string.
 */
WORD scrp_write(char *pscrap)
{
    if (pscrap == NULL) {
        return 0;
    }
    strncpy(_aes.scrap_path, pscrap, sizeof(_aes.scrap_path) - 1u);
    _aes.scrap_path[sizeof(_aes.scrap_path) - 1u] = '\0';
    return 1;
}

/*
 * Clears the hosted scrap path string.
 */
WORD scrp_clear(void)
{
    _aes.scrap_path[0] = '\0';
    return 1;
}

/*
 * Initializes the hosted file-selector path and selection strings.
 */
WORD fsel_input(char *pipath, char *pisel, WORD *pbutton)
{
    if (pbutton != NULL) {
        *pbutton = 0;
    }
    if (pipath != NULL && pipath[0] == '\0') {
        strcpy(pipath, "./");
    }
    if (pisel != NULL && pisel[0] == '\0') {
        strcpy(pisel, "*.*");
    }
    return 1;
}

/*
 * Allocates one hosted window slot and initializes its outer and work
 * rectangles.
 */
WORD wind_create(UWORD kind, WORD x, WORD y, WORD w, WORD h)
{
    size_t i;

    for (i = 0; i < AES_MAX_WINDOWS; ++i) {
        if (_aes.windows[i].used == 0) {
            _aes.windows[i].used = 1;
            _aes.windows[i].handle = (WORD) (i + 1);
            _aes.windows[i].owner = _aes.current_app_id;
            _aes.windows[i].z_order = _aes.next_window_z++;
            _aes.windows[i].kind = kind;
            _aes_set_rect(&_aes.windows[i].outer, x, y, w, h);
            _aes.windows[i].previous_outer = _aes.windows[i].outer;
            _aes.windows[i].restored_outer = _aes.windows[i].outer;
            _aes.windows[i].hslide = 0;
            _aes.windows[i].vslide = 0;
            _aes.windows[i].hslsize = 1000;
            _aes.windows[i].vslsize = 1000;
            _aes_compute_work(&_aes.windows[i]);
            return _aes.windows[i].handle;
        }
    }
    return 0;
}

/*
 * Opens one hosted window and updates its geometry.
 */
WORD wind_open(WORD handle, WORD x, WORD y, WORD w, WORD h)
{
    aes_window_t *window = _aes_find_window(handle);
    aes_window_t *previous_top = NULL;
    GRECT previous_outer;
    GRECT desktop;
    size_t i;
    int had_open_windows = 0;
    int was_open = 0;

    if (window == NULL) {
        return 0;
    }
    was_open = (window->open != 0);
    previous_top = _aes_find_top_window();
    for (i = 0; i < AES_MAX_WINDOWS; ++i) {
        if (_aes.windows[i].used != 0 && _aes.windows[i].open != 0 &&
            _aes.windows[i].handle != handle) {
            had_open_windows = 1;
            break;
        }
    }
    if (was_open != 0) {
        previous_outer = window->outer;
    } else {
        _aes_set_rect(&previous_outer, 0, 0, 0, 0);
        _aes_raise_window(window);
    }
    _aes_set_rect(&window->outer, x, y, w, h);
    window->restored_outer = window->outer;
    window->iconified = 0;
    window->previous_outer = window->outer;
    _aes_compute_work(window);
    window->open = 1;
    if (had_open_windows == 0) {
        _aes_desktop_rect(&desktop);
        if (desktop.g_w > 0 && desktop.g_h > 0) {
            _aes_redraw_region(&desktop);
        }
    }
    _aes_redraw_window_change(&previous_outer, &window->outer);
    if (was_open == 0) {
        _aes_redraw_window_title_states(previous_top, window);
    }
    return 1;
}

/*
 * Marks one hosted window as closed.
 */
WORD wind_close(WORD handle)
{
    aes_window_t *window = _aes_find_window(handle);
    GRECT previous_outer;

    if (window == NULL) {
        return 0;
    }
    previous_outer = window->outer;
    window->open = 0;
    _aes_redraw_window_change(&previous_outer, NULL);
    return 1;
}

/*
 * Releases one hosted window slot entirely.
 */
WORD wind_delete(WORD handle)
{
    aes_window_t *window = _aes_find_window(handle);

    if (window == NULL) {
        return 0;
    }
    memset(window, 0, sizeof(*window));
    return 1;
}

/*
 * Returns desktop or window geometry according to the requested
 * `wind_get()` field.
 */
WORD wind_get(WORD handle, WORD field, WORD *w1, WORD *w2, WORD *w3, WORD *w4)
{
    aes_window_t *window = _aes_find_window(handle);
    const GRECT *rect;
    GRECT rect_value;

    if (handle == 0) {
        WORD box_height = 0;
        WORD menu_height;

        _aes_trace("wind_get desktop field=%d", field);
        if (_aes_ensure_vdi() == 0) {
            return 0;
        }
        _aes_trace("wind_get desktop ptrs %p %p %p %p",
            (void *) w1, (void *) w2, (void *) w3, (void *) w4);

        rect_value.g_x = 0;
        rect_value.g_y = 0;
        rect_value.g_w = (WORD) (_aes.work_out[0] + 1);
        rect_value.g_h = (WORD) (_aes.work_out[1] + 1);

        if (_aes.menu_visible != 0 && _aes.menu_tree != NULL) {
            (void) graf_handle(NULL, NULL, NULL, &box_height);
            menu_height = (box_height > 0) ? box_height :
                _aes_menu_chrome_height();
            if (menu_height <= 0) {
                menu_height = _aes_menu_chrome_height();
            }
        } else {
            menu_height = 0;
        }

        rect_value.g_y = menu_height;
        rect_value.g_h = (WORD) _aes_max_word((WORD) 0,
            (WORD) (rect_value.g_h - menu_height));

        if (field == WF_FIRSTXYWH) {
            _aes.enum_handle = 0;
            _aes.enum_index = 0;
            _aes.enum_count = _aes_build_visible_rects(0, _aes.enum_rects, 64);
            if (_aes.enum_count > 0) {
                rect_value = _aes.enum_rects[0];
                _aes.enum_stage = 1;
            } else {
                _aes.enum_stage = 0;
                _aes_set_rect(&rect_value, 0, 0, 0, 0);
            }
        } else if (field == WF_NEXTXYWH) {
            if (_aes.enum_handle == 0 &&
                _aes.enum_stage != 0 &&
                _aes.enum_index + 1 < _aes.enum_count) {
                ++_aes.enum_index;
                rect_value = _aes.enum_rects[_aes.enum_index];
            } else {
                _aes.enum_stage = 0;
                _aes.enum_count = 0;
                _aes.enum_index = 0;
                _aes_set_rect(&rect_value, 0, 0, 0, 0);
            }
        }

        if (w1 != NULL) {
            *w1 = rect_value.g_x;
        }
        if (w2 != NULL) {
            *w2 = rect_value.g_y;
        }
        if (w3 != NULL) {
            *w3 = rect_value.g_w;
        }
        if (w4 != NULL) {
            *w4 = rect_value.g_h;
        }
        return 1;
    }

    if (window == NULL) {
        return 0;
    }

    if (field == WF_FIRSTXYWH) {
        _aes.enum_handle = handle;
        _aes.enum_index = 0;
        _aes.enum_count = _aes_build_visible_rects(handle, _aes.enum_rects, 64);
        if (_aes.enum_count > 0) {
            rect = &_aes.enum_rects[0];
            _aes.enum_stage = 1;
        } else {
            rect = &rect_value;
            _aes.enum_stage = 0;
            _aes_set_rect(&rect_value, 0, 0, 0, 0);
        }
    } else if (field == WF_NEXTXYWH) {
        if (_aes.enum_handle == handle &&
            _aes.enum_stage != 0 &&
            _aes.enum_index + 1 < _aes.enum_count) {
            ++_aes.enum_index;
            rect = &_aes.enum_rects[_aes.enum_index];
        } else {
            rect = &rect_value;
            _aes.enum_stage = 0;
            _aes.enum_count = 0;
            _aes.enum_index = 0;
            _aes_set_rect(&rect_value, 0, 0, 0, 0);
        }
    } else if (field == WF_WXYWH) {
        rect = &window->outer;
    } else if (field == WF_CXYWH) {
        rect = &window->work;
    } else if (field == WF_PXYWH) {
        rect = &window->previous_outer;
    } else if (field == WF_FXYWH) {
        rect_value.g_x = 0;
        rect_value.g_y = _aes_menu_bar_height();
        rect_value.g_w = (WORD) (_aes.work_out[0] + 1);
        rect_value.g_h = (WORD) _aes_max_word((WORD) 0,
            (WORD) (_aes.work_out[1] + 1 - rect_value.g_y));
        rect = &rect_value;
    } else if (field == WF_HSLIDE) {
        rect_value.g_x = window->hslide;
        rect_value.g_y = 0;
        rect_value.g_w = 0;
        rect_value.g_h = 0;
        rect = &rect_value;
    } else if (field == WF_VSLIDE) {
        rect_value.g_x = window->vslide;
        rect_value.g_y = 0;
        rect_value.g_w = 0;
        rect_value.g_h = 0;
        rect = &rect_value;
    } else if (field == WF_HSLSIZ) {
        rect_value.g_x = window->hslsize;
        rect_value.g_y = 0;
        rect_value.g_w = 0;
        rect_value.g_h = 0;
        rect = &rect_value;
    } else if (field == WF_VSLSIZ) {
        rect_value.g_x = window->vslsize;
        rect_value.g_y = 0;
        rect_value.g_w = 0;
        rect_value.g_h = 0;
        rect = &rect_value;
    } else {
        rect = &window->work;
    }

    if (w1 != NULL) {
        *w1 = rect->g_x;
    }
    if (w2 != NULL) {
        *w2 = rect->g_y;
    }
    if (w3 != NULL) {
        *w3 = rect->g_w;
    }
    if (w4 != NULL) {
        *w4 = rect->g_h;
    }
    return 1;
}

/*
 * Updates one hosted window's text fields or geometry.
 */
WORD wind_set(WORD handle, WORD field, WORD w1, WORD w2, WORD w3, WORD w4)
{
    aes_window_t *window = _aes_find_window(handle);
    aes_window_t before;

    if (window == NULL) {
        return 0;
    }

    before = *window;

    switch (field) {
    case WF_NAME:
        return _aes_wind_set_text(handle, field,
            (const char *) (intptr_t) w1);
    case WF_INFO:
        return _aes_wind_set_text(handle, field,
            (const char *) (intptr_t) w1);
    case WF_TOP:
    {
        aes_window_t *previous_top = _aes_find_top_window();
        _aes_raise_window(window);
        _aes_redraw_window_change(&window->outer, &window->outer);
        _aes_redraw_window_title_states(previous_top, window);
        break;
    }
    case WF_WXYWH:
    case WF_CXYWH:
        window->previous_outer = window->outer;
        _aes_set_rect(&window->outer, w1, w2, w3, w4);
        if (window->iconified != 0) {
            window->restored_outer.g_x = w1;
            window->restored_outer.g_y = w2;
            window->restored_outer.g_w = w3;
        } else {
            window->restored_outer = window->outer;
            window->iconified = 0;
        }
        _aes_compute_work(window);
        if (window->open != 0) {
            _aes_redraw_window_change(&window->previous_outer, &window->outer);
        }
        break;
    case WF_HSLIDE:
    {
        WORD value = _aes_max_word(0, _aes_min_word(w1, 1000));
        if (window->hslide == value) {
            break;
        }
        window->hslide = value;
        if (window->open != 0) {
            _aes_redraw_scrollbar_delta(&before, window, field);
        }
        break;
    }
    case WF_VSLIDE:
    {
        WORD value = _aes_max_word(0, _aes_min_word(w1, 1000));
        if (window->vslide == value) {
            break;
        }
        window->vslide = value;
        if (window->open != 0) {
            _aes_redraw_scrollbar_delta(&before, window, field);
        }
        break;
    }
    case WF_HSLSIZ:
    {
        WORD value = _aes_max_word(0, _aes_min_word(w1, 1000));
        if (window->hslsize == value) {
            break;
        }
        window->hslsize = value;
        if (window->open != 0) {
            _aes_redraw_scrollbar_delta(&before, window, field);
        }
        break;
    }
    case WF_VSLSIZ:
    {
        WORD value = _aes_max_word(0, _aes_min_word(w1, 1000));
        if (window->vslsize == value) {
            break;
        }
        window->vslsize = value;
        if (window->open != 0) {
            _aes_redraw_scrollbar_delta(&before, window, field);
        }
        break;
    }
    default:
        break;
    }
    return 1;
}

/*
 * Updates one hosted window string field using a full host pointer.
 */
WORD wind_set_str(WORD handle, WORD field, const char *text)
{
    return _aes_wind_set_text(handle, field, text);
}

/*
 * Returns the topmost open hosted window covering one screen point.
 */
WORD wind_find(WORD x, WORD y)
{
    size_t i;
    aes_window_t *best = NULL;

    for (i = 0; i < AES_MAX_WINDOWS; ++i) {
        aes_window_t *window = &_aes.windows[i];

        if (window->used != 0 && window->open != 0 &&
            _aes_point_in_rect(x, y, &window->outer)) {
            if (best == NULL || window->z_order > best->z_order) {
                best = window;
            }
        }
    }
    return (best != NULL) ? best->handle : 0;
}

/*
 * Starts or ends a deferred hosted redraw batch.
 */
WORD wind_update(WORD flag)
{
    if (flag == BEG_UPDATE) {
        ++_aes.update_depth;
        _vdi_begin_update();
    } else if (flag == END_UPDATE && _aes.update_depth > 0) {
        --_aes.update_depth;
        _vdi_end_update();
    }
    return 1;
}

/*
 * Converts between border and work rectangles using the hosted window
 * decoration thickness.
 */
WORD wind_calc(WORD type,
               UWORD kind,
               WORD inx,
               WORD iny,
               WORD inw,
               WORD inh,
               WORD *outx,
               WORD *outy,
               WORD *outw,
               WORD *outh)
{
    aes_window_t temp_window;
    WORD left_border;
    WORD right_border;
    WORD bottom_border;
    WORD title_height;

    memset(&temp_window, 0, sizeof(temp_window));
    temp_window.kind = kind;
    _aes_set_rect(&temp_window.outer, inx, iny, inw, inh);
    _aes_compute_work(&temp_window);
    left_border = (WORD) (temp_window.work.g_x - temp_window.outer.g_x);
    title_height = (WORD) (temp_window.work.g_y - temp_window.outer.g_y);
    right_border = (WORD) ((temp_window.outer.g_x + temp_window.outer.g_w) -
        (temp_window.work.g_x + temp_window.work.g_w));
    bottom_border = (WORD) ((temp_window.outer.g_y + temp_window.outer.g_h) -
        (temp_window.work.g_y + temp_window.work.g_h));

    if (type == WC_BORDER) {
        if (outx != NULL) {
            *outx = (WORD) (inx - left_border);
        }
        if (outy != NULL) {
            *outy = (WORD) (iny - title_height);
        }
        if (outw != NULL) {
            *outw = (WORD) (inw + left_border + right_border);
        }
        if (outh != NULL) {
            *outh = (WORD) (inh + title_height + bottom_border);
        }
    } else {
        if (outx != NULL) {
            *outx = (WORD) (inx + left_border);
        }
        if (outy != NULL) {
            *outy = (WORD) (iny + title_height);
        }
        if (outw != NULL) {
            *outw = (WORD) (inw - left_border - right_border);
        }
        if (outh != NULL) {
            *outh = (WORD) (inh - title_height - bottom_border);
        }
    }
    return 1;
}

/*
 * Loads one AES resource file either from the built-in provider or from
 * a hosted file path.
 */
WORD rsrc_load(char *filename)
{
    char resolved[260];

    if (filename == NULL) {
        return 0;
    }
    if (_aes.resource_data != NULL) {
        gem_os_free(_aes.resource_data);
        _aes.resource_data = NULL;
        _aes.resource_size = 0;
    }
    _aes.resource_is_builtin = 0;
    if (gem_builtin_rsrc_load(filename) != 0) {
        _aes.resource_is_builtin = 1;
        return 1;
    }
    if (_aes_try_resolve_path(filename, resolved, sizeof(resolved))) {
        return _aes_load_file(resolved, &_aes.resource_data,
            &_aes.resource_size);
    }
    if (_aes_load_file(filename, &_aes.resource_data,
        &_aes.resource_size) != 0) {
        return 1;
    }
    return 0;
}

/*
 * Releases the currently loaded AES resource image.
 */
WORD rsrc_free(void)
{
    if (_aes.resource_data != NULL) {
        gem_os_free(_aes.resource_data);
        _aes.resource_data = NULL;
        _aes.resource_size = 0;
    }
    if (_aes.resource_is_builtin != 0) {
        gem_builtin_rsrc_free();
        _aes.resource_is_builtin = 0;
    }
    return 1;
}

/*
 * Returns a typed address within the currently loaded AES resource
 * image.
 */
WORD rsrc_gaddr(WORD type, WORD index, void **addr)
{
    RSHDR *header;
    uint8_t *base;

    if (addr == NULL) {
        return 0;
    }
    if (_aes.resource_is_builtin != 0) {
        return gem_builtin_rsrc_gaddr(type, index, addr);
    }
    if (_aes.resource_data == NULL) {
        return 0;
    }

    base = (uint8_t *) _aes.resource_data;
    header = (RSHDR *) _aes.resource_data;
    switch (type) {
    case R_TREE: {
        WORD *trindex = (WORD *) (base + header->rsh_trindex);
        *addr = base + trindex[index];
        return 1;
    }
    case R_OBJECT:
        *addr = base + header->rsh_object + index * sizeof(OBJECT);
        return 1;
    case R_TEDINFO:
        *addr = base + header->rsh_tedinfo + index * sizeof(TEDINFO);
        _aes_resource_fix_tedinfo((TEDINFO *) *addr);
        return 1;
    case R_ICONBLK:
        *addr = base + header->rsh_iconblk + index * sizeof(ICONBLK);
        _aes_resource_fix_iconblk((ICONBLK *) *addr);
        return 1;
    case R_BITBLK:
        *addr = base + header->rsh_bitblk + index * sizeof(BITBLK);
        _aes_resource_fix_bitblk((BITBLK *) *addr);
        return 1;
    case R_STRING: {
        LONG *strings = (LONG *) (base + header->rsh_string);
        *addr = base + strings[index];
        return 1;
    }
    default:
        return 0;
    }
}

/*
 * Accepts a resource-address override request when any resource image
 * is currently loaded.
 */
WORD rsrc_saddr(WORD type, WORD index, void *addr)
{
    (void) type;
    (void) index;
    (void) addr;
    return (_aes.resource_data != NULL || _aes.resource_is_builtin != 0) ?
        1 : 0;
}

/*
 * Fixes one loaded resource object by relocating any file-relative
 * pointer fields into host pointers.
 */
WORD rsrc_obfix(OBJECT *tree, WORD object)
{
    LONG spec;

    if (tree == NULL || object < 0) {
        return 0;
    }
    if (!_aes_resource_contains_ptr(tree)) {
        return 1;
    }

    spec = tree[object].ob_spec;
    switch (tree[object].ob_type) {
    case G_STRING:
    case G_TITLE:
    case G_BUTTON:
        tree[object].ob_spec = _aes_resource_fix_offset(spec);
        break;
    case G_TEXT:
    case G_BOXTEXT:
    case G_FTEXT:
    case G_FBOXTEXT:
        spec = _aes_resource_fix_offset(spec);
        tree[object].ob_spec = spec;
        _aes_resource_fix_tedinfo((TEDINFO *) (intptr_t) spec);
        break;
    case G_ICON:
        spec = _aes_resource_fix_offset(spec);
        tree[object].ob_spec = spec;
        _aes_resource_fix_iconblk((ICONBLK *) (intptr_t) spec);
        break;
    default:
        break;
    }
    return 1;
}

/*
 * Returns the last shell command and tail remembered by the hosted AES.
 */
WORD shel_read(char *cmd, char *tail)
{
    if (cmd != NULL) {
        strcpy(cmd, _aes.shell_cmd);
    }
    if (tail != NULL) {
        strcpy(tail, _aes.shell_tail);
    }
    return 1;
}

/*
 * Stores the next shell command and tail requested by the caller.
 */
WORD shel_write(WORD doex, WORD isgr, WORD iscr, char *cmd, char *tail)
{
    (void) doex;
    (void) isgr;
    (void) iscr;

    if (cmd != NULL) {
        strncpy(_aes.shell_cmd, cmd, sizeof(_aes.shell_cmd) - 1u);
        _aes.shell_cmd[sizeof(_aes.shell_cmd) - 1u] = '\0';
    }
    if (tail != NULL) {
        strncpy(_aes.shell_tail, tail, sizeof(_aes.shell_tail) - 1u);
        _aes.shell_tail[sizeof(_aes.shell_tail) - 1u] = '\0';
    }
    return 1;
}

/*
 * Returns the in-memory `DESKTOP.INF` image, loading it on demand the
 * first time the shell asks for it.
 */
WORD shel_get(char *buf, WORD length)
{
    if (_aes.shell_buf_len == 0) {
        void *data = NULL;
        size_t size = 0u;
        char resolved[260];

        if (_aes_try_resolve_path("DESKTOP.INF", resolved, sizeof(resolved)) &&
            _aes_load_file(resolved, &data, &size)) {
            if (size >= sizeof(_aes.shell_buf)) {
                size = sizeof(_aes.shell_buf) - 1u;
            }
            memcpy(_aes.shell_buf, data, size);
            _aes.shell_buf[size] = '\0';
            _aes.shell_buf_len = (WORD) size;
            gem_os_free(data);
        }
    }

    if (buf == NULL || length <= 0) {
        return 0;
    }

    if ((size_t) length > sizeof(_aes.shell_buf)) {
        length = (WORD) sizeof(_aes.shell_buf);
    }
    memcpy(buf, _aes.shell_buf, (size_t) length);
    return 1;
}

/*
 * Replaces the in-memory `DESKTOP.INF` image and mirrors it to
 * `bin/desktop.inf`.
 */
WORD shel_put(char *buf, WORD length)
{
    int fd;

    if (buf == NULL || length < 0) {
        return 0;
    }

    if ((size_t) length >= sizeof(_aes.shell_buf)) {
        length = (WORD) (sizeof(_aes.shell_buf) - 1u);
    }
    memcpy(_aes.shell_buf, buf, (size_t) length);
    _aes.shell_buf[length] = '\0';
    _aes.shell_buf_len = length;

    fd = gem_os_open_write("bin/desktop.inf");
    if (fd >= 0) {
        (void) gem_os_write(fd, _aes.shell_buf,
            (uint32_t) _aes.shell_buf_len);
        (void) gem_os_close(fd);
    }

    return 1;
}

/*
 * Resolves one shell path against the hosted desktop search locations.
 */
WORD shel_find(char *path)
{
    char resolved[260];

    if (path == NULL) {
        return 0;
    }
    if (_aes_try_resolve_path(path, resolved, sizeof(resolved))) {
        strcpy(path, resolved);
        return 1;
    }
    return 0;
}

/*
 * Returns one process environment variable through the historical AES
 * shell interface.
 */
WORD shel_envrn(char **env, char *var)
{
    char *value;

    if (env == NULL || var == NULL) {
        return 0;
    }

    value = getenv(var);
    if (value == NULL) {
        return 0;
    }
    *env = value;
    return 1;
}

/*
 * Returns the hosted shell default command and working directory.
 */
WORD shel_rdef(char *lpcmd, char *lpdir)
{
    if (lpcmd != NULL) {
        strcpy(lpcmd, _aes.shell_cmd);
    }
    if (lpdir != NULL) {
        strcpy(lpdir, _aes.shell_dir);
    }
    return 1;
}

/*
 * Stores the hosted shell default command and working directory.
 */
WORD shel_wdef(char *lpcmd, char *lpdir)
{
    if (lpcmd != NULL) {
        strncpy(_aes.shell_cmd, lpcmd, sizeof(_aes.shell_cmd) - 1u);
        _aes.shell_cmd[sizeof(_aes.shell_cmd) - 1u] = '\0';
    }
    if (lpdir != NULL) {
        strncpy(_aes.shell_dir, lpdir, sizeof(_aes.shell_dir) - 1u);
        _aes.shell_dir[sizeof(_aes.shell_dir) - 1u] = '\0';
    }
    return 1;
}
