/*
 * Implements the classic AES alert box: parsing the [icon][lines][buttons]
 * string, laying out the panel, painting its frame, icon and buttons, and
 * running the modal interaction behind form_alert and form_error.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (C) 2026 tomaz stih
 */

#include "aes_internal.h"
#include "alert_icons.h"
#include "window_track.h"

#include "platform/os.h"

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

static const aes_alert_icon_asset_t *aes_alert_icon_asset(WORD icon);

enum {
    AES_ALERT_MAX_LINES = 5,
    AES_ALERT_MAX_BUTTONS = 3,
    AES_ALERT_TEXT_LEN = 80
};

typedef struct aes_alert {
    WORD window;
    GRECT cover;
    WORD icon;
    WORD default_button;
    WORD line_count;
    WORD button_count;
    char lines[AES_ALERT_MAX_LINES][AES_ALERT_TEXT_LEN];
    char buttons[AES_ALERT_MAX_BUTTONS][AES_ALERT_TEXT_LEN];
    GRECT outer;
    GRECT button_rects[AES_ALERT_MAX_BUTTONS];
} aes_alert_t;

static int aes_alert_parse_group(const char **cursor, char *out,
                                 size_t out_size)
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

static void aes_alert_split_items(const char *text,
                                  char items[][AES_ALERT_TEXT_LEN], WORD *count,
                                  WORD max_items)
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

    memset(items, 0, (size_t)max_items * AES_ALERT_TEXT_LEN);
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
        *count = (WORD)(item + 1);
    }
}

static int aes_parse_alert(const char *str, WORD defbtn, aes_alert_t *alert)
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

    if (aes_alert_parse_group(&cursor, icon_text, sizeof(icon_text)) == 0 ||
        aes_alert_parse_group(&cursor, line_text, sizeof(line_text)) == 0 ||
        aes_alert_parse_group(&cursor, button_text, sizeof(button_text)) == 0) {
        strncpy(alert->lines[0], str, AES_ALERT_TEXT_LEN - 1u);
        strcpy(alert->buttons[0], "OK");
        alert->line_count = 1;
        alert->button_count = 1;
        return 1;
    }

    alert->icon = (WORD)atoi(icon_text);
    aes_alert_split_items(line_text, alert->lines, &alert->line_count,
                          AES_ALERT_MAX_LINES);
    aes_alert_split_items(button_text, alert->buttons, &alert->button_count,
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

static void aes_alert_compute_layout(aes_alert_t *alert)
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

    text_h = vdi_font_text_height();
    if (text_h <= 0) {
        text_h = AES_CHAR_HEIGHT;
    }
    button_h = (WORD)(text_h + 12);
    if (aes_alert_icon_asset(alert->icon) != NULL) {
        icon_w = 32;
        icon_h = 32;
    }

    for (i = 0; i < alert->line_count; ++i) {
        WORD width = (WORD)vdi_string_width(alert->lines[i]);

        if (width > text_w) {
            text_w = width;
        }
    }

    for (i = 0; i < alert->button_count; ++i) {
        WORD width = (WORD)(vdi_string_width(alert->buttons[i]) + 18);

        if (width < 48) {
            width = 48;
        }
        alert->button_rects[i].g_w = width;
        alert->button_rects[i].g_h = button_h;
        button_total = (WORD)(button_total + width);
        if (i + 1 < alert->button_count) {
            button_total = (WORD)(button_total + button_gap);
        }
    }

    content_w = (WORD)(text_w + icon_w + ((icon_w > 0) ? 12 : 0));
    if (button_total > content_w) {
        content_w = button_total;
    }
    content_h =
        (WORD)(alert->line_count * text_h + (alert->line_count - 1) * line_gap);
    if (icon_h > content_h) {
        content_h = icon_h;
    }

    aes_dialog_parent_rect(&desktop);
    alert->outer.g_w = (WORD)(content_w + 32);
    alert->outer.g_h = (WORD)(content_h + button_h + 40);
    alert->outer.g_x =
        (WORD)(desktop.g_x + (desktop.g_w - alert->outer.g_w) / 2);
    alert->outer.g_y =
        (WORD)(desktop.g_y + (desktop.g_h - alert->outer.g_h) / 2);

    x = (WORD)(alert->outer.g_x + (alert->outer.g_w - button_total) / 2);
    for (i = 0; i < alert->button_count; ++i) {
        alert->button_rects[i].g_x = x;
        alert->button_rects[i].g_y =
            (WORD)(alert->outer.g_y + alert->outer.g_h - button_h - 12);
        x = (WORD)(x + alert->button_rects[i].g_w + button_gap);
    }
}

static void aes_alert_draw_text(WORD x, WORD y, const char *text)
{
    vst_color(aes_state.vdi_handle, WHITE);
    v_gtext(aes_state.vdi_handle, x, y, (CONST BYTE *)text);
}

static void aes_alert_draw_frame(const GRECT *rect, int classic_frame)
{
    WORD fill[4];
    WORD border[10];

    if (rect == NULL) {
        return;
    }

    fill[0] = rect->g_x;
    fill[1] = rect->g_y;
    fill[2] = (WORD)(rect->g_x + rect->g_w - 1);
    fill[3] = (WORD)(rect->g_y + rect->g_h - 1);
    vsf_color(aes_state.vdi_handle, BLACK);
    vr_recfl(aes_state.vdi_handle, fill);

    /* Titled windows already supply their standard enclosure. */
    if (!classic_frame)
        return;

    /* Classic 11010 enclosure for the windowless fallback. */
    for (WORD inset = 0; inset < 5; ++inset) {
        WORD left = (WORD)(fill[0] + inset);
        WORD top = (WORD)(fill[1] + inset);
        WORD right = (WORD)(fill[2] - inset);
        WORD bottom = (WORD)(fill[3] - inset);
        if (left > right || top > bottom)
            break;
        border[0] = left;
        border[1] = top;
        border[2] = right;
        border[3] = top;
        border[4] = right;
        border[5] = bottom;
        border[6] = left;
        border[7] = bottom;
        border[8] = left;
        border[9] = top;
        vsl_color(aes_state.vdi_handle,
                  inset == 2 || inset == 4 ? BLACK : WHITE);
        v_pline(aes_state.vdi_handle, 5, border);
    }
}

static void aes_alert_draw_button(const GRECT *rect, const char *label,
                                  int is_default)
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
    fill[2] = (WORD)(rect->g_x + rect->g_w - 1);
    fill[3] = (WORD)(rect->g_y + rect->g_h - 1);
    vsf_color(aes_state.vdi_handle, BLACK);
    vr_recfl(aes_state.vdi_handle, fill);

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
    vsl_color(aes_state.vdi_handle, WHITE);
    v_pline(aes_state.vdi_handle, 5, border);

    if (is_default != 0 && rect->g_w > 4 && rect->g_h > 4) {
        inner[0] = (WORD)(fill[0] + 2);
        inner[1] = (WORD)(fill[1] + 2);
        inner[2] = (WORD)(fill[2] - 2);
        inner[3] = inner[1];
        inner[4] = inner[2];
        inner[5] = (WORD)(fill[3] - 2);
        inner[6] = inner[0];
        inner[7] = inner[5];
        inner[8] = inner[0];
        inner[9] = inner[1];
        v_pline(aes_state.vdi_handle, 5, inner);
    }

    width = (WORD)vdi_string_width(label);
    text_x = (WORD)(rect->g_x + (rect->g_w - width) / 2);
    text_y = (WORD)(rect->g_y + (rect->g_h - vdi_font_text_height()) / 2 +
                    vdi_font_ascent());
    aes_alert_draw_text(text_x, text_y, label);
}

static const aes_alert_icon_asset_t *aes_alert_icon_asset(WORD icon)
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

static int aes_alert_icon_bit(const UWORD *bits,
                              const aes_alert_icon_asset_t *asset, WORD x,
                              WORD y)
{
    size_t row_words;
    size_t index;
    UWORD word;

    if (bits == NULL || asset == NULL || x < 0 || y < 0 || x >= asset->width ||
        y >= asset->height) {
        return 0;
    }

    row_words = (size_t)((asset->width + 15) / 16);
    index = (size_t)y * row_words + (size_t)x / 16u;
    word = bits[index];
    return (word & (WORD)(0x8000u >> (x % 16))) != 0;
}

static void aes_alert_draw_icon(const aes_alert_icon_asset_t *asset, WORD x,
                                WORD y)
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
                   aes_alert_icon_bit(asset->mask_bits, asset, col, row) == 0) {
                ++col;
            }
            if (col >= asset->width) {
                break;
            }
            start = col;
            while (col < asset->width &&
                   aes_alert_icon_bit(asset->mask_bits, asset, col, row) != 0) {
                ++col;
            }
            rect[0] = (WORD)(x + start);
            rect[1] = (WORD)(y + row);
            rect[2] = (WORD)(x + col - 1);
            rect[3] = rect[1];
            vsf_color(aes_state.vdi_handle, BLACK);
            v_bar(aes_state.vdi_handle, rect);
        }
    }

    for (row = 0; row < asset->height; ++row) {
        WORD col = 0;

        while (col < asset->width) {
            WORD start;
            WORD rect[4];

            while (col < asset->width &&
                   aes_alert_icon_bit(asset->data_bits, asset, col, row) == 0) {
                ++col;
            }
            if (col >= asset->width) {
                break;
            }
            start = col;
            while (col < asset->width &&
                   aes_alert_icon_bit(asset->data_bits, asset, col, row) != 0) {
                ++col;
            }
            rect[0] = (WORD)(x + start);
            rect[1] = (WORD)(y + row);
            rect[2] = (WORD)(x + col - 1);
            rect[3] = rect[1];
            vsf_color(aes_state.vdi_handle, WHITE);
            v_bar(aes_state.vdi_handle, rect);
        }
    }
}

static void aes_draw_alert(const aes_alert_t *alert)
{
    WORD clip[4];
    WORD text_x;
    WORD text_y;
    WORD text_h;
    WORD line_gap = 4;
    WORD i;

    if (alert == NULL || aes_ensure_vdi() == 0) {
        return;
    }

    clip[0] = alert->outer.g_x;
    clip[1] = alert->outer.g_y;
    clip[2] = (WORD)(alert->outer.g_x + alert->outer.g_w - 1);
    clip[3] = (WORD)(alert->outer.g_y + alert->outer.g_h - 1);

    wind_update(BEG_UPDATE);
    v_hide_c(aes_state.vdi_handle);
    vs_clip(aes_state.vdi_handle, 1, clip);
    (void)vswr_mode(aes_state.vdi_handle, MD_REPLACE);

    aes_alert_draw_frame(&alert->outer, alert->window <= 0);
    text_x = (WORD)(alert->outer.g_x + 12);
    if (aes_alert_icon_asset(alert->icon) != NULL) {
        aes_alert_draw_icon(aes_alert_icon_asset(alert->icon), text_x,
                            (WORD)(alert->outer.g_y + 12));
        text_x = (WORD)(text_x + 44);
    }

    text_h = vdi_font_text_height();
    for (i = 0; i < alert->line_count; ++i) {
        text_y = (WORD)(alert->outer.g_y + 12 + i * (text_h + line_gap) +
                        vdi_font_ascent());
        aes_alert_draw_text(text_x, text_y, alert->lines[i]);
    }
    for (i = 0; i < alert->button_count; ++i) {
        aes_alert_draw_button(&alert->button_rects[i], alert->buttons[i],
                              i + 1 == alert->default_button);
    }

    vs_clip(aes_state.vdi_handle, 0, clip);
    v_show_c(aes_state.vdi_handle, 0);
    wind_update(END_UPDATE);
}

static void aes_alert_wait_button_release(void)
{
    gem_hid_event_t evt;

    FOREVER
    {
        if (aes_wait_hook && !aes_wait_hook()) {
            return;
        }
        if (gem_hid_poll(&evt) == 0) {
            gem_os_sleep_ms(1u);
            continue;
        }
        if (evt.type == GEM_HID_QUIT) {
            return;
        }
        if (evt.type == GEM_HID_MOUSE_MOVE ||
            evt.type == GEM_HID_MOUSE_BUTTON) {
            aes_store_mouse_state(&evt);
        }
        if (evt.type == GEM_HID_MOUSE_BUTTON &&
            evt.button == GEM_HID_BUTTON_LEFT &&
            (evt.flags & GEM_HID_BUTTON_LEFT) == 0u) {
            return;
        }
    }
}

static WORD aes_run_alert(aes_alert_t *alert)
{
    gem_hid_event_t evt;
    uint8_t *saved_pixels = NULL;
    WORD i;

    if (alert == NULL) {
        return 1;
    }

    vdi_begin_update();
    v_hide_c(aes_state.vdi_handle);
    if (alert->window <= 0)
        (void)aes_save_region_pixels(&alert->outer, &saved_pixels);
    v_show_c(aes_state.vdi_handle, 0);
    vdi_end_update();
    aes_draw_alert(alert);

    for (;;) {
        if (aes_wait_hook && !aes_wait_hook()) {
            alert->default_button = 0;
            break;
        }
        if (gem_hid_poll(&evt) == 0) {
            gem_os_sleep_ms(1u);
            continue;
        }

        if (evt.type == GEM_HID_QUIT) {
            break;
        }
        if (evt.type == GEM_HID_MOUSE_MOVE ||
            evt.type == GEM_HID_MOUSE_BUTTON) {
            aes_store_mouse_state(&evt);
        }
        if (evt.type == GEM_HID_KEY && (evt.flags & 1u) != 0u) {
            WORD ch = (WORD)(evt.key & 0xffu);

            if (ch == 13 || ch == '\n' || ch == ' ') {
                break;
            }
            if (ch >= '1' && ch < '1' + alert->button_count) {
                wind_update(BEG_UPDATE);
                v_hide_c(aes_state.vdi_handle);
                if (saved_pixels)
                    aes_restore_region_pixels(&alert->outer, saved_pixels);
                v_show_c(aes_state.vdi_handle, 0);
                wind_update(END_UPDATE);
                if (saved_pixels != NULL) {
                    gem_os_free(saved_pixels);
                }
                return (WORD)(ch - '0');
            }
            if (ch == 27) {
                break;
            }
        }
        if (evt.type == GEM_HID_MOUSE_BUTTON &&
            evt.button == GEM_HID_BUTTON_LEFT &&
            (evt.flags & GEM_HID_BUTTON_LEFT) != 0u) {
            aes_window_t *window = aes_find_window(alert->window);
            if (window && evt.y >= window->outer.g_y &&
                evt.y < window->work.g_y && evt.x >= window->outer.g_x &&
                evt.x < window->outer.g_x + window->outer.g_w) {
                if (evt.x < window->outer.g_x + aes_chrome_height())
                    break;
                WORD mx = evt.x, my = evt.y, buttons = 1, modifiers = 0;
                aes_track_t track = {.window = window,
                                     .first_evt = &evt,
                                     .flags = 0,
                                     .pmx = &mx,
                                     .pmy = &my,
                                     .pmb = &buttons,
                                     .pks = &modifiers,
                                     .start_x = evt.x,
                                     .start_y = evt.y};
                aes_begin_interaction_lock();
                aes_track_move(&track);
                WORD dx = window->work.g_x - alert->outer.g_x;
                WORD dy = window->work.g_y - alert->outer.g_y;
                alert->outer = window->work;
                alert->cover = window->outer;
                for (WORD j = 0; j < alert->button_count; ++j) {
                    alert->button_rects[j].g_x += dx;
                    alert->button_rects[j].g_y += dy;
                }
                aes_notify_window_topology_changed();
                aes_draw_alert(alert);
                continue;
            }
            for (i = 0; i < alert->button_count; ++i) {
                if (aes_point_in_rect((WORD)evt.x, (WORD)evt.y,
                                      &alert->button_rects[i]) != 0) {
                    aes_alert_wait_button_release();
                    wind_update(BEG_UPDATE);
                    v_hide_c(aes_state.vdi_handle);
                    if (saved_pixels)
                        aes_restore_region_pixels(&alert->outer, saved_pixels);
                    v_show_c(aes_state.vdi_handle, 0);
                    wind_update(END_UPDATE);
                    if (saved_pixels != NULL) {
                        gem_os_free(saved_pixels);
                    }
                    return (WORD)(i + 1);
                }
            }
        }
    }

    wind_update(BEG_UPDATE);
    v_hide_c(aes_state.vdi_handle);
    if (saved_pixels)
        aes_restore_region_pixels(&alert->outer, saved_pixels);
    v_show_c(aes_state.vdi_handle, 0);
    wind_update(END_UPDATE);
    if (saved_pixels != NULL) {
        gem_os_free(saved_pixels);
    }
    return alert->default_button;
}

WORD form_alert(WORD defbtn, char *str)
{
    aes_alert_t alert;
    WORD result;
    const GRECT *previous_cover = aes_modal_cover;

    if (aes_parse_alert(str, defbtn, &alert) == 0) {
        return (defbtn > 0) ? defbtn : 1;
    }
    aes_alert_compute_layout(&alert);
    GRECT border;
    wind_calc(WC_BORDER, NAME | MOVER | CLOSER, alert.outer.g_x,
              alert.outer.g_y, alert.outer.g_w, alert.outer.g_h, &border.g_x,
              &border.g_y, &border.g_w, &border.g_h);
    GRECT parent, screen;
    aes_dialog_parent_rect(&parent);
    aes_desktop_rect(&screen);
    WORD x = parent.g_x + (parent.g_w - border.g_w) / 2;
    WORD y = parent.g_y + (parent.g_h - border.g_h) / 2;
    if (x + border.g_w > screen.g_x + screen.g_w)
        x = screen.g_x + screen.g_w - border.g_w;
    if (y + border.g_h > screen.g_y + screen.g_h)
        y = screen.g_y + screen.g_h - border.g_h;
    if (x < screen.g_x)
        x = screen.g_x;
    if (y < screen.g_y)
        y = screen.g_y;
    WORD dx = x - border.g_x, dy = y - border.g_y;
    border.g_x = x;
    border.g_y = y;
    alert.outer.g_x += dx;
    alert.outer.g_y += dy;
    for (WORD i = 0; i < alert.button_count; ++i) {
        alert.button_rects[i].g_x += dx;
        alert.button_rects[i].g_y += dy;
    }
    alert.window = wind_create(NAME | MOVER | CLOSER, border.g_x, border.g_y,
                               border.g_w, border.g_h);
    if (alert.window > 0) {
        wind_set_str(alert.window, WF_NAME, "Message");
        wind_open(alert.window, border.g_x, border.g_y, border.g_w, border.g_h);
    }
    alert.cover = alert.window > 0 ? border : alert.outer;
    aes_modal_cover = alert.window > 0 ? previous_cover : &alert.cover;
    aes_notify_window_topology_changed();
    result = aes_run_alert(&alert);
    aes_modal_cover = previous_cover;
    if (alert.window > 0) {
        wind_close(alert.window);
        wind_delete(alert.window);
    }
    aes_notify_window_topology_changed();
    if (aes_wait_hook)
        aes_redraw_region(&alert.outer);
    return result;
}

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
