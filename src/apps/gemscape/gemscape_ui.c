/*
 * Implements the first hosted GEM Gemscape window shell. This initial
 * version focuses on a browser-like frame with a toolbar band, address
 * band, content pane, and status bar sized from native GEM metrics so
 * it fits visually with titles, menu rows, and scroll controls.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (C) 2026 tomaz stih
 */

#include <gem.h>
#include <gem/gemctl.h>
#include "sample_resources.h"

#include "gemscape.h"
#include "cef_browser.h"
#include "browser_settings.h"
#include <ctype.h>
#include <signal.h>
#include <gem/os.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static volatile sig_atomic_t gemscape_quit_requested;

static void gemscape_signal(int signal_number)
{
    (void)signal_number;
    gemscape_quit_requested = 1;
}

enum {
    gemscape_work_width = 560,
    gemscape_work_height = 360,
    gemscape_margin = 6,
    gemscape_inner_gap = 4,
    gemscape_toolbar_icon_count = 4
};

enum {
    gemscape_paper = BLACK,
    gemscape_ink = WHITE,
    gemscape_content_paper = BLACK
};

typedef struct gemscape_state {
    WORD handle;
    WORD vdi_handle;
    WORD char_w;
    WORD char_h;
    WORD box_w;
    WORD box_h;
    BITBLK toolbar_icons[gemscape_toolbar_icon_count];
    int toolbar_icons_loaded;
    GRECT normal_rect;
    int full_open;
    WORD last_mouse_x;
    WORD last_mouse_y;
    int have_mouse_position;
    int browser_focused;
    char address_text[2048];
    gemscape_preferences_t preferences;
    gemscape_settings_dialog_t settings;
    unsigned int browser_revision;
    int mouse_buttons;
    int browser_buttons;
    int address_drag;
    size_t address_anchor;
    WORD drag_x;
    uint32_t last_click_tick;
    WORD last_click_x;
    WORD last_click_y;
    int click_count;
    int browser_hover;
    char status_text[256];
    gemctl_text_edit_t address_edit;
#if defined(GEMSCAPE_OVERLAY) && GEMSCAPE_OVERLAY
    gem_overlay_id_t overlay;
    int overlay_upload_pending;
    uint32_t overlay_resize_tick;
    int cef_started;
    int scroll_metrics_valid;
    WORD vslide;
    WORD vslsize;
    WORD hslide;
    WORD hslsize;
#endif
} gemscape_state_t;

static void gemscape_layout(const gemscape_state_t *state, const GRECT *work,
                            GRECT *address, GRECT *content, GRECT *status);

static void gemscape_free_toolbar_icons(gemscape_state_t *state)
{
    WORD ii;

    if (state == NULL) {
        return;
    }

    for (ii = 0; ii < gemscape_toolbar_icon_count; ++ii) {
        sample_free_bitblk(&state->toolbar_icons[ii]);
    }
    state->toolbar_icons_loaded = 0;
}

static int gemscape_load_toolbar_icons(gemscape_state_t *state)
{
    if (state == NULL) {
        return 0;
    }

    state->toolbar_icons_loaded =
        sample_load_bitblks("gemscape_toolbar.rsc", "gemscape_toolbar.rsc",
                            state->toolbar_icons, gemscape_toolbar_icon_count);
    return state->toolbar_icons_loaded;
}

static void gemscape_intersect_rect(const GRECT *a, const GRECT *b, GRECT *out)
{
    WORD x0;
    WORD y0;
    WORD x1;
    WORD y1;

    if (a == NULL || b == NULL || out == NULL) {
        return;
    }

    x0 = (a->g_x > b->g_x) ? a->g_x : b->g_x;
    y0 = (a->g_y > b->g_y) ? a->g_y : b->g_y;
    x1 = (WORD)(((a->g_x + a->g_w - 1) < (b->g_x + b->g_w - 1))
                    ? (a->g_x + a->g_w - 1)
                    : (b->g_x + b->g_w - 1));
    y1 = (WORD)(((a->g_y + a->g_h - 1) < (b->g_y + b->g_h - 1))
                    ? (a->g_y + a->g_h - 1)
                    : (b->g_y + b->g_h - 1));

    if (x1 < x0 || y1 < y0) {
        out->g_x = 0;
        out->g_y = 0;
        out->g_w = 0;
        out->g_h = 0;
        return;
    }

    out->g_x = x0;
    out->g_y = y0;
    out->g_w = (WORD)(x1 - x0 + 1);
    out->g_h = (WORD)(y1 - y0 + 1);
}

static void gemscape_draw_toolbar_icon(const gemscape_state_t *state,
                                       WORD slot_x, WORD slot_y, WORD slot_size,
                                       WORD icon_index)
{
    const BITBLK *bitblk;
    GRECT slot;

    if (state == NULL || icon_index < 0 ||
        icon_index >= gemscape_toolbar_icon_count ||
        state->toolbar_icons_loaded == 0 || slot_size <= 0) {
        return;
    }

    bitblk = &state->toolbar_icons[icon_index];
    if (bitblk->bi_pdata == 0 || bitblk->bi_wb <= 0 || bitblk->bi_hl <= 0) {
        return;
    }

    slot.g_x = slot_x;
    slot.g_y = slot_y;
    slot.g_w = slot_size;
    slot.g_h = slot_size;
#if defined(GEMSCAPE_CEF) && GEMSCAPE_CEF
    const gemscape_browser_info_t *info = gemscape_cef_info();
    if (icon_index == 2 && info->loading) {
        WORD pxy[4] = {(WORD)(slot_x + 6), (WORD)(slot_y + 6),
                       (WORD)(slot_x + slot_size - 7),
                       (WORD)(slot_y + slot_size - 7)};
        vsl_color(state->vdi_handle, WHITE);
        vsl_type(state->vdi_handle, 1);
        vsl_width(state->vdi_handle, 1);
        v_pline(state->vdi_handle, 2, pxy);
        WORD swap = pxy[1];
        pxy[1] = pxy[3];
        pxy[3] = swap;
        v_pline(state->vdi_handle, 2, pxy);
        return;
    }
    if ((icon_index == 0 && !info->can_back) ||
        (icon_index == 1 && !info->can_forward)) {
        uint8_t pixels[256];
        size_t size = (size_t)bitblk->bi_wb * (size_t)bitblk->bi_hl;
        if (size <= sizeof(pixels)) {
            BITBLK disabled = *bitblk;
            memcpy(pixels, (const void *)(intptr_t)bitblk->bi_pdata, size);
            for (size_t i = 0; i < size; ++i)
                pixels[i] &= (i / (size_t)bitblk->bi_wb) % 2 ? 0x55 : 0xaa;
            disabled.bi_pdata = (intptr_t)pixels;
            gemctl_toolbar_bitmap(state->vdi_handle, &disabled, &slot);
            return;
        }
    }
#endif
    gemctl_toolbar_bitmap(state->vdi_handle, bitblk, &slot);
}

static void gemscape_draw_content(const gemscape_state_t *state,
                                  const GRECT *rect)
{
    (void)state;
    (void)rect;
}

#if defined(GEMSCAPE_OVERLAY) && GEMSCAPE_OVERLAY
static void gemscape_update_overlay(gemscape_state_t *state, const GRECT *work,
                                    const GRECT *content)
{
    size_t bytes;
    uint8_t *pixels;

    if (state == NULL || work == NULL || content == NULL || work->g_w <= 0 ||
        work->g_h <= 0 || content->g_w <= 0 || content->g_h <= 0 ||
        state->overlay == 0)
        return;
#if defined(GEMSCAPE_CEF) && GEMSCAPE_CEF
    if (state->cef_started != 0) {
        int received = 0;
        const uint8_t *frame;
        uint16_t frame_width;
        uint16_t frame_height;
        uint32_t frame_size;

        gemscape_cef_pump();
        if (gemscape_cef_take_frame(&frame_width, &frame_height, &frame,
                                    &frame_size) &&
            frame_width == (uint16_t)content->g_w &&
            frame_height == (uint16_t)content->g_h &&
            frame_size == (uint32_t)content->g_w * content->g_h * 4u) {
            (void)gem_overlay_update(
                state->overlay, (uint16_t)(content->g_x - work->g_x),
                (uint16_t)(content->g_y - work->g_y), frame_width, frame_height,
                frame, (uint16_t)(frame_width * 4u));
            received = 1;
        }
        /* CEF paints asynchronously. Keep polling until its first frame and
         * thereafter let navigation/invalidations deliver new frames. */
        state->overlay_upload_pending = received == 0 ? 1 : 0;
        return;
    }
#endif
    bytes = (size_t)content->g_w * content->g_h * 4u;
    pixels = malloc(bytes);
    if (pixels == NULL)
        return;
    for (WORD y = 0; y < content->g_h; ++y) {
        for (WORD x = 0; x < content->g_w; ++x) {
            uint8_t *pixel = pixels + ((size_t)y * content->g_w + x) * 4u;
            pixel[0] = (uint8_t)(32u + ((unsigned)x * 160u) /
                                           (unsigned)(content->g_w > 1
                                                          ? content->g_w - 1
                                                          : 1));
            pixel[1] = (uint8_t)(40u + ((unsigned)y * 160u) /
                                           (unsigned)(content->g_h > 1
                                                          ? content->g_h - 1
                                                          : 1));
            pixel[2] =
                (uint8_t)(180u + ((unsigned)(x + y) * 60u) /
                                     (unsigned)(content->g_w + content->g_h));
            pixel[3] = 255u;
        }
    }
    (void)gem_overlay_update(
        state->overlay, (uint16_t)(content->g_x - work->g_x),
        (uint16_t)(content->g_y - work->g_y), (uint16_t)content->g_w,
        (uint16_t)content->g_h, pixels, (uint16_t)(content->g_w * 4));
    free(pixels);
    state->overlay_upload_pending = 0;
}

static void gemscape_refresh_overlay(gemscape_state_t *state)
{
    GRECT work;
    GRECT address;
    GRECT content;
    GRECT status;

    if (state == NULL || state->overlay == 0 ||
        gem_os_ticks_ms() - state->overlay_resize_tick < 120u)
        return;
    if (wind_get(state->handle, WF_WORKXYWH, &work.g_x, &work.g_y, &work.g_w,
                 &work.g_h) == 0)
        return;
    gemscape_layout(state, &work, &address, &content, &status);
    gemscape_update_overlay(state, &work, &content);
}

static WORD gemscape_slider_size(int viewport, int content)
{
    LONG size;

    if (viewport <= 0 || content <= viewport)
        return 1000;
    size = (LONG)viewport * 1000L / content;
    return (WORD)(size < 1L ? 1L : size);
}

static WORD gemscape_slider_position(int offset, int viewport, int content)
{
    int limit = content - viewport;

    if (offset <= 0 || limit <= 0)
        return 0;
    if (offset >= limit)
        return 1000;
    return (WORD)((LONG)offset * 1000L / limit);
}

static void gemscape_update_scrollbars(gemscape_state_t *state)
{
#if defined(GEMSCAPE_CEF) && GEMSCAPE_CEF
    int x;
    int y;
    int viewport_width;
    int viewport_height;
    int content_width;
    int content_height;
    WORD vslide;
    WORD vslsize;
    WORD hslide;
    WORD hslsize;

    if (state == NULL || state->cef_started == 0 ||
        gemscape_cef_get_scroll(&x, &y, &viewport_width, &viewport_height,
                                &content_width, &content_height) == 0)
        return;
    vslide = gemscape_slider_position(y, viewport_height, content_height);
    vslsize = gemscape_slider_size(viewport_height, content_height);
    hslide = gemscape_slider_position(x, viewport_width, content_width);
    hslsize = gemscape_slider_size(viewport_width, content_width);
    if (state->scroll_metrics_valid == 0 || state->vslide != vslide) {
        (void)wind_set(state->handle, WF_VSLIDE, vslide, 0, 0, 0);
        state->vslide = vslide;
    }
    if (state->scroll_metrics_valid == 0 || state->vslsize != vslsize) {
        (void)wind_set(state->handle, WF_VSLSIZ, vslsize, 0, 0, 0);
        state->vslsize = vslsize;
    }
    if (state->scroll_metrics_valid == 0 || state->hslide != hslide) {
        (void)wind_set(state->handle, WF_HSLIDE, hslide, 0, 0, 0);
        state->hslide = hslide;
    }
    if (state->scroll_metrics_valid == 0 || state->hslsize != hslsize) {
        (void)wind_set(state->handle, WF_HSLSIZ, hslsize, 0, 0, 0);
        state->hslsize = hslsize;
    }
    state->scroll_metrics_valid = 1;
#else
    (void)state;
#endif
}

static void gemscape_scroll_browser(gemscape_state_t *state, WORD message,
                                    WORD value, const GRECT *browser)
{
#if defined(GEMSCAPE_CEF) && GEMSCAPE_CEF
    int x = 0;
    int y = 0;
    int viewport_width = browser != NULL ? browser->g_w : 0;
    int viewport_height = browser != NULL ? browser->g_h : 0;
    int content_width = viewport_width;
    int content_height = viewport_height;
    int x_limit;
    int y_limit;

    if (state == NULL || state->cef_started == 0)
        return;
    (void)gemscape_cef_get_scroll(&x, &y, &viewport_width, &viewport_height,
                                  &content_width, &content_height);
    x_limit =
        content_width > viewport_width ? content_width - viewport_width : 0;
    y_limit =
        content_height > viewport_height ? content_height - viewport_height : 0;
    if (message == WM_VSLID) {
        y = (int)((LONG)value * y_limit / 1000L);
    } else if (message == WM_HSLID) {
        x = (int)((LONG)value * x_limit / 1000L);
    } else {
        switch (value) {
            case WA_UPLINE:
                y -= 40;
                break;
            case WA_DNLINE:
                y += 40;
                break;
            case WA_UPPAGE:
                y -= viewport_height;
                break;
            case WA_DNPAGE:
                y += viewport_height;
                break;
            case WA_LFLINE:
                x -= 40;
                break;
            case WA_RTLINE:
                x += 40;
                break;
            case WA_LFPAGE:
                x -= viewport_width;
                break;
            case WA_RTPAGE:
                x += viewport_width;
                break;
            default:
                break;
        }
    }
    if (x < 0)
        x = 0;
    if (y < 0)
        y = 0;
    if (x > x_limit)
        x = x_limit;
    if (y > y_limit)
        y = y_limit;
    gemscape_cef_scroll_to(x, y);
    state->overlay_upload_pending = 1;
#else
    (void)state;
    (void)message;
    (void)value;
    (void)browser;
#endif
}
#endif

static void gemscape_layout(const gemscape_state_t *state, const GRECT *work,
                            GRECT *address, GRECT *content, GRECT *status)
{
    gemctl_chrome_t chrome;

    if (state == NULL || work == NULL || address == NULL || content == NULL ||
        status == NULL) {
        return;
    }

    gemctl_chrome_layout(work, state->char_h, state->box_h, &chrome);
    *address = chrome.toolbar;
    *content = chrome.content;
    *status = chrome.status;
}

static void gemscape_draw_frame(gemscape_state_t *state, const GRECT *dirty)
{
    GRECT work;
    GRECT visible;
    GRECT clip;
    GRECT address;
    GRECT content;
    GRECT status;
    GRECT address_text;
    WORD clip_xy[4];
    WORD button_size;
    WORD separator_x;
    WORD right_button_left;
    int i;

    if (state == NULL) {
        return;
    }

    if (wind_get(state->handle, WF_WORKXYWH, &work.g_x, &work.g_y, &work.g_w,
                 &work.g_h) == 0) {
        return;
    }

    gemscape_layout(state, &work, &address, &content, &status);

    wind_update(BEG_UPDATE);
    wind_get(state->handle, WF_FIRSTXYWH, &visible.g_x, &visible.g_y,
             &visible.g_w, &visible.g_h);

    while (visible.g_w > 0 && visible.g_h > 0) {
        clip = visible;
        if (dirty != NULL) {
            gemscape_intersect_rect(&clip, dirty, &clip);
        }

        if (clip.g_w > 0 && clip.g_h > 0) {
            clip_xy[0] = clip.g_x;
            clip_xy[1] = clip.g_y;
            clip_xy[2] = (WORD)(clip.g_x + clip.g_w - 1);
            clip_xy[3] = (WORD)(clip.g_y + clip.g_h - 1);
            vs_clip(state->vdi_handle, 1, clip_xy);

            gemctl_toolbar_draw(state->vdi_handle, &address);
            button_size = address.g_h;
            for (i = 1; i <= 3; ++i) {
                separator_x = (WORD)(address.g_x + i * button_size - 1);
                if (separator_x < (WORD)(address.g_x + address.g_w)) {
                    gemctl_toolbar_separator(
                        state->vdi_handle, separator_x, address.g_y,
                        (WORD)(address.g_y + address.g_h - 1));
                }
            }
            for (i = 0; i < 3; ++i) {
                gemscape_draw_toolbar_icon(
                    state, (WORD)(address.g_x + i * button_size), address.g_y,
                    button_size, (WORD)i);
            }
            right_button_left = (WORD)(address.g_x + address.g_w - button_size);
            if (right_button_left > address.g_x) {
                gemctl_toolbar_separator(state->vdi_handle, right_button_left,
                                         address.g_y,
                                         (WORD)(address.g_y + address.g_h - 1));
                gemscape_draw_toolbar_icon(state, right_button_left,
                                           address.g_y, button_size, 3);
            }
            address_text = address;
            address_text.g_x = (WORD)(address.g_x + 3 * button_size);
            address_text.g_w = (WORD)(right_button_left - address_text.g_x);
            if (address_text.g_w < 0) {
                address_text.g_w = 0;
            }
            gemctl_text_edit_draw(&state->address_edit, state->vdi_handle,
                                  &address_text, &clip);
            gemscape_draw_content(state, &content);
            gemctl_status_draw(state->vdi_handle, &status, state->status_text);

            vs_clip(state->vdi_handle, 0, clip_xy);
        }

        wind_get(state->handle, WF_NEXTXYWH, &visible.g_x, &visible.g_y,
                 &visible.g_w, &visible.g_h);
    }

    wind_update(END_UPDATE);
#if defined(GEMSCAPE_OVERLAY) && GEMSCAPE_OVERLAY
    if (state->overlay_upload_pending != 0)
        gemscape_refresh_overlay(state);
#endif
}

static void gemscape_toggle_full(gemscape_state_t *state)
{
    GRECT full;
    GRECT current;

    if (state == NULL) {
        return;
    }

    if (wind_get(0, WF_WORKXYWH, &full.g_x, &full.g_y, &full.g_w, &full.g_h) ==
        0) {
        return;
    }
    if (wind_get(state->handle, WF_WXYWH, &current.g_x, &current.g_y,
                 &current.g_w, &current.g_h) == 0) {
        return;
    }

    if (state->full_open == 0) {
        state->normal_rect = current;
        (void)wind_set(state->handle, WF_WXYWH, full.g_x, full.g_y, full.g_w,
                       full.g_h);
        state->full_open = 1;
    } else {
        (void)wind_set(state->handle, WF_WXYWH, state->normal_rect.g_x,
                       state->normal_rect.g_y, state->normal_rect.g_w,
                       state->normal_rect.g_h);
        state->full_open = 0;
    }
}

static void gemscape_address_rect(const GRECT *bar, GRECT *field)
{
    *field = *bar;
    field->g_x = (WORD)(bar->g_x + 3 * bar->g_h);
    field->g_w = (WORD)(bar->g_w - 4 * bar->g_h);
}

#if defined(GEMSCAPE_CEF) && GEMSCAPE_CEF
static void gemscape_sync_browser(gemscape_state_t *state)
{
#if defined(GEMSCAPE_CEF) && GEMSCAPE_CEF
    const gemscape_browser_info_t *info = gemscape_cef_info();
    char title[300];
    if (info->revision == state->browser_revision)
        return;
    state->browser_revision = info->revision;
    if (!gemctl_text_edit_active(&state->address_edit) && info->url[0])
        snprintf(state->address_text, sizeof(state->address_text), "%s",
                 info->url);
    snprintf(state->status_text, sizeof(state->status_text), "%s",
             info->status);
    snprintf(title, sizeof(title), "Gemscape - %.240s",
             info->title[0] ? info->title : "Browser");
    wind_set_str(state->handle, WF_NAME, title);
    gemscape_draw_frame(state, NULL);
    if (state->settings.handle > 0 && state->settings.tab == 1) {
        snprintf(state->settings.message, sizeof(state->settings.message),
                 "%.120s", info->status);
        gemscape_settings_draw(&state->settings);
    }
#else
    (void)state;
#endif
}

#endif

static void gemscape_settings_action(gemscape_state_t *state, int action)
{
#if defined(GEMSCAPE_CEF) && GEMSCAPE_CEF
    if (action == gemscape_settings_saved) {
        gemscape_cef_zoom(state->preferences.zoom);
        snprintf(state->status_text, sizeof(state->status_text),
                 "Settings saved. Other changes apply next launch.");
        gemscape_draw_frame(state, NULL);
    } else if (action >= gemscape_settings_cookies &&
               action <= gemscape_settings_site) {
        char *questions[] = {"[2][Clear all browser cookies?|This signs you "
                             "out of websites.][Cancel|Clear]",
                             "[2][Clear the HTTP browser cache?][Cancel|Clear]",
                             "[2][Clear cookies and stored data|for the "
                             "current website?][Cancel|Clear]"};
        if (form_alert(1, questions[action - gemscape_settings_cookies]) == 2)
            gemscape_cef_clear_data(action - gemscape_settings_cookies);
    }
#else
    (void)state;
    (void)action;
#endif
}

static void gemscape_navigate(gemscape_state_t *state)
{
    char url[sizeof(state->address_text)];
    const char *text = state->address_text;
    size_t length;
    while (*text == ' ')
        ++text;
    length = strlen(text);
    while (length > 0 && text[length - 1] == ' ')
        --length;
    if (length == 0)
        return;
    for (size_t i = 0; i < length; ++i) {
        if ((unsigned char)text[i] <= 32u) {
            snprintf(state->status_text, sizeof(state->status_text),
                     "Address contains a space or control character.");
            return;
        }
    }
    int written;
    if (strstr(text, "://") != NULL || strncmp(text, "about:", 6) == 0) {
        if (strncmp(text, "http://", 7) && strncmp(text, "https://", 8) &&
            strncmp(text, "file://", 7) && strcmp(text, "about:blank")) {
            snprintf(state->status_text, sizeof(state->status_text),
                     "Unsupported address scheme.");
            return;
        }
        written = snprintf(url, sizeof(url), "%.*s", (int)length, text);
    } else
        written = snprintf(url, sizeof(url), "%s%.*s",
                           strncmp(text, "localhost", 9) == 0 ||
                                   strncmp(text, "127.", 4) == 0
                               ? "http://"
                               : "https://",
                           (int)length, text);
    if (written < 0 || (size_t)written >= sizeof(url))
        return;
    snprintf(state->address_text, sizeof(state->address_text), "%s", url);
    gemctl_text_edit_blur(&state->address_edit);
    state->browser_focused = 1;
#if defined(GEMSCAPE_CEF) && GEMSCAPE_CEF
    if (state->cef_started) {
        gemscape_cef_load_url(url);
        gemscape_cef_focus(1);
    }
#endif
}

static gemctl_text_edit_result_t gemscape_address_key(gemscape_state_t *state,
                                                      WORD key, WORD modifiers)
{
#if defined(GEMSCAPE_CEF) && GEMSCAPE_CEF
    if (modifiers & K_CTRL) {
        unsigned int scan = (uint16_t)key >> 8;
        gemctl_text_edit_t *edit = &state->address_edit;
        if (scan == 6u || scan == 27u) {
            char text[sizeof(state->address_text)];
            size_t first =
                edit->cursor < edit->anchor ? edit->cursor : edit->anchor;
            size_t last =
                edit->cursor > edit->anchor ? edit->cursor : edit->anchor;
            if (last > first && last <= strlen(edit->text)) {
                memcpy(text, edit->text + first, last - first);
                text[last - first] = '\0';
                gemscape_cef_set_clipboard(text);
                if (scan == 27u)
                    return gemctl_text_edit_insert(edit, "");
            }
            return GEMCTL_TEXT_EDIT_IGNORED;
        }
        if (scan == 25u)
            return gemctl_text_edit_insert(edit, gemscape_cef_clipboard());
    }
#endif
    return gemctl_text_edit_key_mod(&state->address_edit, (uint16_t)key,
                                    (uint16_t)modifiers);
}

static void gemscape_resize_browser(gemscape_state_t *state)
{
#if defined(GEMSCAPE_OVERLAY) && GEMSCAPE_OVERLAY
    GRECT work, bar, content, status;
    wind_get(state->handle, WF_WORKXYWH, &work.g_x, &work.g_y, &work.g_w,
             &work.g_h);
    gemscape_layout(state, &work, &bar, &content, &status);
    if (work.g_w < 1 || work.g_h < 1 || content.g_w < 1 || content.g_h < 1)
        return;
    gem_overlay_resize(state->overlay, (uint16_t)work.g_w, (uint16_t)work.g_h);
#if defined(GEMSCAPE_CEF) && GEMSCAPE_CEF
    if (state->cef_started)
        gemscape_cef_resize((uint16_t)content.g_w, (uint16_t)content.g_h);
#endif
    state->overlay_resize_tick = gem_os_ticks_ms();
    state->overlay_upload_pending = 1;
#else
    (void)state;
#endif
}

static void gemscape_pointer(gemscape_state_t *state, WORD x, WORD y,
                             WORD buttons, WORD modifiers)
{
    GRECT work, bar, content, status, field;
    int pressed = buttons & ~state->mouse_buttons;
    int released = state->mouse_buttons & ~buttons;
    WORD owner = wind_find(x, y);
    wind_get(state->handle, WF_WORKXYWH, &work.g_x, &work.g_y, &work.g_w,
             &work.g_h);
    gemscape_layout(state, &work, &bar, &content, &status);
    gemscape_address_rect(&bar, &field);
    int content_hit = owner == state->handle && x >= content.g_x &&
                      x < content.g_x + content.g_w && y >= content.g_y &&
                      y < content.g_y + content.g_h;
    if (pressed & 1) {
        uint32_t now = gem_os_ticks_ms();
        if (now - state->last_click_tick < 400 &&
            abs(x - state->last_click_x) < 5 &&
            abs(y - state->last_click_y) < 5)
            state->click_count = state->click_count == 2 ? 1 : 2;
        else
            state->click_count = 1;
        state->last_click_tick = now;
        state->last_click_x = x;
        state->last_click_y = y;
        if (state->settings.handle > 0 && owner == state->settings.handle) {
            gemscape_settings_action(
                state, gemscape_settings_click(&state->settings, x, y,
                                               &state->preferences));
        } else if (owner == state->handle && y >= bar.g_y &&
                   y < bar.g_y + bar.g_h) {
            state->browser_focused = 0;
#if defined(GEMSCAPE_CEF) && GEMSCAPE_CEF
            gemscape_cef_focus(0);
#endif
            if (x >= bar.g_x + bar.g_w - bar.g_h) {
                gemctl_text_edit_blur(&state->address_edit);
                gemscape_settings_open(&state->settings, state->vdi_handle,
                                       &state->preferences);
            } else if (x < field.g_x) {
                int slot = (x - bar.g_x) / bar.g_h;
                gemctl_text_edit_blur(&state->address_edit);
#if defined(GEMSCAPE_CEF) && GEMSCAPE_CEF
                if (slot == 0)
                    gemscape_cef_back();
                if (slot == 1)
                    gemscape_cef_forward();
                if (slot == 2) {
                    if (gemscape_cef_info()->loading)
                        gemscape_cef_cancel_load();
                    else
                        gemscape_cef_reload();
                }
#else
                (void)slot;
#endif
            } else {
                gemctl_text_edit_t position = state->address_edit;
                gemctl_text_edit_click(&position, state->vdi_handle, &field, x,
                                       0);
                state->address_anchor = position.cursor;
                state->drag_x = x;
                state->address_drag = 1;
                if (!gemctl_text_edit_active(&state->address_edit) ||
                    state->click_count == 2)
                    gemctl_text_edit_focus(&state->address_edit, 1);
                else
                    gemctl_text_edit_click(
                        &state->address_edit, state->vdi_handle, &field, x,
                        (modifiers & (K_LSHIFT | K_RSHIFT)) != 0);
            }
            gemscape_draw_frame(state, NULL);
        } else if (owner == state->handle) {
            gemctl_text_edit_blur(&state->address_edit);
            state->browser_focused = content_hit;
            gemscape_draw_frame(state, NULL);
        }
    }
    if (state->address_drag && (buttons & 1) && abs(x - state->drag_x) > 3) {
        state->address_edit.anchor = state->address_anchor;
        gemctl_text_edit_click(&state->address_edit, state->vdi_handle, &field,
                               x, 1);
        gemscape_draw_frame(state, NULL);
    }
    if (released & 1)
        state->address_drag = 0;
#if defined(GEMSCAPE_CEF) && GEMSCAPE_CEF
    if (state->cef_started) {
        for (int button = 1; button <= 2; button *= 2) {
            if ((pressed & button) && content_hit) {
                state->browser_buttons |= button;
                state->browser_focused = 1;
                gemscape_cef_mouse_click(
                    x - content.g_x, y - content.g_y, button, 0,
                    button == 1 ? state->click_count : 1, (uint16_t)modifiers);
            }
            if ((released & button) && (state->browser_buttons & button)) {
                gemscape_cef_mouse_click(
                    x - content.g_x, y - content.g_y, button, 1,
                    button == 1 ? state->click_count : 1, (uint16_t)modifiers);
                state->browser_buttons &= ~button;
            }
        }
        if (content_hit || state->browser_buttons || state->browser_hover) {
            gemscape_cef_mouse_move(x - content.g_x, y - content.g_y,
                                    !content_hit && !state->browser_buttons,
                                    (uint16_t)modifiers);
            state->browser_hover = content_hit;
        }
    }
#endif
    state->mouse_buttons = buttons;
}

int gemscape_main(int argc, char **argv)
{
    gemscape_state_t state;
    int exit_status = 0;
    WORD appl_id;
    WORD event_flags;
    WORD mouse_x;
    WORD mouse_y;
    WORD mouse_buttons;
    WORD key_state;
    WORD key_code;
    WORD button_return;
    WORD msg[8];
    WORD outer_x;
    WORD outer_y;
    WORD outer_w;
    WORD outer_h;
    UWORD window_kind;

    (void)argc;
    (void)argv;

#if defined(GEMSCAPE_CEF) && GEMSCAPE_CEF
    {
        int cef_process = gemscape_cef_execute_process(argc, argv);

        if (cef_process >= 0)
            return cef_process;
    }
#endif

    signal(SIGTERM, gemscape_signal);
    signal(SIGINT, gemscape_signal);

    if (!gem_os_init()) {
        fprintf(stderr, "gem_os_init() failed\n");
        return 1;
    }

    appl_id = appl_init();
    if (appl_id < 0) {
        gem_os_shutdown();
        return 1;
    }

    memset(&state, 0, sizeof(state));
    gemscape_preferences_load(&state.preferences);
    snprintf(state.status_text, sizeof(state.status_text), "Ready.");
    {
        const char *initial_url = gem_os_getenv_ref("GEMSCAPE_URL");

        if (initial_url == NULL || initial_url[0] == '\0')
            initial_url = state.preferences.homepage;
        strncpy(state.address_text, initial_url,
                sizeof(state.address_text) - 1u);
    }
    gemctl_text_edit_init(&state.address_edit, state.address_text,
                          sizeof(state.address_text));
    state.vdi_handle =
        graf_handle(&state.char_w, &state.char_h, &state.box_w, &state.box_h);
    if (state.vdi_handle == 0) {
        appl_exit();
        gem_os_shutdown();
        return 1;
    }

    (void)gemscape_load_toolbar_icons(&state);

    window_kind = (UWORD)(NAME | CLOSER | FULLER | MOVER | SIZER | UPARROW |
                          DNARROW | VSLIDE | LFARROW | RTARROW | HSLIDE);

    state.handle = wind_create(window_kind, 0, 0, 640, 400);
    if (state.handle <= 0) {
        appl_exit();
        gem_os_shutdown();
        return 1;
    }

    (void)wind_set_str(state.handle, WF_NAME, "Gemscape");
    (void)wind_set(state.handle, WF_VSLSIZ, 1000, 0, 0, 0);
    (void)wind_set(state.handle, WF_VSLIDE, 0, 0, 0, 0);
    (void)wind_set(state.handle, WF_HSLSIZ, 1000, 0, 0, 0);
    (void)wind_set(state.handle, WF_HSLIDE, 0, 0, 0, 0);
    (void)wind_calc(WC_BORDER, window_kind, 72, 48, gemscape_work_width,
                    gemscape_work_height, &outer_x, &outer_y, &outer_w,
                    &outer_h);
    {
        GRECT desktop;
        wind_get(0, WF_WORKXYWH, &desktop.g_x, &desktop.g_y, &desktop.g_w,
                 &desktop.g_h);
        if (outer_x < desktop.g_x)
            outer_x = desktop.g_x;
        if (outer_y < desktop.g_y)
            outer_y = desktop.g_y;
        if (outer_x + outer_w > desktop.g_x + desktop.g_w)
            outer_w = (WORD)(desktop.g_x + desktop.g_w - outer_x);
        if (outer_y + outer_h > desktop.g_y + desktop.g_h)
            outer_h = (WORD)(desktop.g_y + desktop.g_h - outer_y);
    }
    state.normal_rect.g_x = outer_x;
    state.normal_rect.g_y = outer_y;
    state.normal_rect.g_w = outer_w;
    state.normal_rect.g_h = outer_h;
    (void)wind_open(state.handle, outer_x, outer_y, outer_w, outer_h);
#if defined(GEMSCAPE_OVERLAY) && GEMSCAPE_OVERLAY
    {
        GRECT work;
        GRECT address;
        GRECT content;
        GRECT status;

        if (wind_get(state.handle, WF_WORKXYWH, &work.g_x, &work.g_y, &work.g_w,
                     &work.g_h) != 0) {
            gemscape_layout(&state, &work, &address, &content, &status);
            state.overlay = gem_overlay_create(state.handle, (uint16_t)work.g_w,
                                               (uint16_t)work.g_h);
            state.overlay_upload_pending = 1;
            state.overlay_resize_tick = 0;
#if defined(GEMSCAPE_CEF) && GEMSCAPE_CEF
            state.cef_started = gemscape_cef_start(
                argc, argv, (uint16_t)content.g_w, (uint16_t)content.g_h,
                state.address_text, &state.preferences);
#endif
        }
    }
#endif
#if defined(GEMSCAPE_CEF) && GEMSCAPE_CEF
    if (!state.cef_started) {
        form_alert(1, "[1][Browser startup failed.|Profile may already be in "
                      "use.|See the browser profile log.][OK]");
        gemscape_quit_requested = 1;
        exit_status = 1;
    }
#endif
    (void)graf_mouse(M_ON, NULL);

    while (!gemscape_quit_requested) {
        event_flags =
            evnt_multi((UWORD)(MU_MESAG | MU_BUTTON | MU_KEYBD | MU_TIMER), 1,
                       1, (state.mouse_buttons & 1) ? 0 : 1, 0, 0, 0, 0, 0, 0,
                       0, 0, 0, 0, msg, 20, 0, &mouse_x, &mouse_y,
                       &mouse_buttons, &key_state, &key_code, &button_return);

#if defined(GEMSCAPE_OVERLAY) && GEMSCAPE_OVERLAY
        if ((event_flags & MU_TIMER) != 0 && state.cef_started != 0) {
            gemscape_refresh_overlay(&state);
            gemscape_update_scrollbars(&state);
            gemscape_sync_browser(&state);
        }
#endif

        if ((event_flags & MU_MESAG) != 0 && state.settings.handle > 0 &&
            msg[3] == state.settings.handle)
            gemscape_settings_message(&state.settings, msg);

        if ((event_flags & MU_MESAG) != 0 && msg[3] == state.handle) {
            if (msg[0] == WM_CLOSED) {
                break;
            } else if (msg[0] == WM_REDRAW) {
                GRECT redraw;

                redraw.g_x = msg[4];
                redraw.g_y = msg[5];
                redraw.g_w = msg[6];
                redraw.g_h = msg[7];
                gemscape_draw_frame(&state, &redraw);
            } else if (msg[0] == WM_MOVED || msg[0] == WM_SIZED) {
                (void)wind_set(state.handle, WF_WXYWH, msg[4], msg[5], msg[6],
                               msg[7]);
                state.normal_rect.g_x = msg[4];
                state.normal_rect.g_y = msg[5];
                state.normal_rect.g_w = msg[6];
                state.normal_rect.g_h = msg[7];
                state.full_open = 0;
#if defined(GEMSCAPE_OVERLAY) && GEMSCAPE_OVERLAY
                if (state.overlay != 0) {
                    GRECT work;
                    GRECT address;
                    GRECT content;
                    GRECT status;

                    if (wind_get(state.handle, WF_WORKXYWH, &work.g_x,
                                 &work.g_y, &work.g_w, &work.g_h) != 0) {
                        gemscape_layout(&state, &work, &address, &content,
                                        &status);
                        if (msg[0] == WM_MOVED) {
                            (void)gem_overlay_map(state.overlay, state.handle);
                        } else {
                            (void)gem_overlay_resize(state.overlay,
                                                     (uint16_t)work.g_w,
                                                     (uint16_t)work.g_h);
#if defined(GEMSCAPE_CEF) && GEMSCAPE_CEF
                            if (state.cef_started != 0 && content.g_w > 0 &&
                                content.g_h > 0)
                                gemscape_cef_resize((uint16_t)content.g_w,
                                                    (uint16_t)content.g_h);
#endif
                            state.overlay_upload_pending = 1;
                            state.overlay_resize_tick = gem_os_ticks_ms();
                        }
                    }
                }
#endif
            } else if (msg[0] == WM_FULLED) {
                gemscape_toggle_full(&state);
                gemscape_resize_browser(&state);
            } else if (msg[0] == WM_TOPPED) {
                (void)wind_set(state.handle, WF_TOP, 0, 0, 0, 0);
            } else if (msg[0] == WM_ARROWED || msg[0] == WM_VSLID ||
                       msg[0] == WM_HSLID) {
#if defined(GEMSCAPE_CEF) && GEMSCAPE_CEF
                if (state.cef_started != 0) {
                    GRECT work;
                    GRECT address;
                    GRECT browser;
                    GRECT status;
                    (void)wind_get(state.handle, WF_WORKXYWH, &work.g_x,
                                   &work.g_y, &work.g_w, &work.g_h);
                    gemscape_layout(&state, &work, &address, &browser, &status);
                    gemscape_scroll_browser(&state, msg[0], msg[4], &browser);
                }
#endif
            }
        }

        if ((event_flags & (MU_TIMER | MU_BUTTON)) != 0)
            gemscape_pointer(&state, mouse_x, mouse_y, mouse_buttons,
                             key_state);

        if ((event_flags & MU_KEYBD) != 0) {
            WORD top = 0, unused = 0;
            wind_get(0, WF_TOP, &top, &unused, &unused, &unused);
            if (state.settings.handle > 0 && top == state.settings.handle) {
                gemscape_settings_action(
                    &state,
                    gemscape_settings_key(&state.settings, key_code, key_state,
                                          &state.preferences));
            } else if (top == state.handle) {
                if ((key_state & K_CTRL) &&
                    ((key_code >> 8) == 15 || (key_code & 255) == 12)) {
                    state.browser_focused = 0;
#if defined(GEMSCAPE_CEF) && GEMSCAPE_CEF
                    gemscape_cef_focus(0);
#endif
                    gemctl_text_edit_focus(&state.address_edit, 1);
                    gemscape_draw_frame(&state, NULL);
                } else if (gemctl_text_edit_active(&state.address_edit)) {
                    gemctl_text_edit_result_t result =
                        gemscape_address_key(&state, key_code, key_state);
                    if (result == GEMCTL_TEXT_EDIT_ACCEPT) {
                        gemscape_navigate(&state);
                    } else if (result == GEMCTL_TEXT_EDIT_CANCEL) {
                        gemctl_text_edit_blur(&state.address_edit);
                        state.browser_focused = 1;
#if defined(GEMSCAPE_CEF) && GEMSCAPE_CEF
                        gemscape_cef_focus(1);
                        snprintf(state.address_text, sizeof(state.address_text),
                                 "%s", gemscape_cef_info()->url);
#endif
                    }
                    gemscape_draw_frame(&state, NULL);
#if defined(GEMSCAPE_CEF) && GEMSCAPE_CEF
                } else if (state.cef_started) {
                    if ((key_code & 255) == 27) {
                        gemscape_cef_cancel_load();
                        if (state.browser_focused)
                            gemscape_cef_key((uint16_t)key_code,
                                             (uint16_t)key_state);
                    } else if ((key_code >> 8) == 62 ||
                               ((key_state & K_CTRL) && (key_code >> 8) == 21))
                        gemscape_cef_reload();
                    else if ((key_state & K_ALT) && (key_code >> 8) == 80)
                        gemscape_cef_back();
                    else if ((key_state & K_ALT) && (key_code >> 8) == 79)
                        gemscape_cef_forward();
                    else if ((key_state & K_ALT) && (key_code >> 8) == 74)
                        gemscape_cef_load_url(state.preferences.homepage);
                    else if (state.browser_focused)
                        gemscape_cef_key((uint16_t)key_code,
                                         (uint16_t)key_state);
#endif
                }
            }
        }
    }

    gemscape_settings_close(&state.settings);
    if (state.handle > 0) {
        (void)wind_close(state.handle);
        (void)wind_delete(state.handle);
    }
#if defined(GEMSCAPE_OVERLAY) && GEMSCAPE_OVERLAY
    if (state.overlay != 0)
        (void)gem_overlay_destroy(state.overlay);
#if defined(GEMSCAPE_CEF) && GEMSCAPE_CEF
    if (state.cef_started != 0)
        gemscape_cef_stop();
#endif
#endif
    gemscape_free_toolbar_icons(&state);
    appl_exit();
    gem_os_shutdown();
    return exit_status;
}
