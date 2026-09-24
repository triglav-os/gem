/*
 * Implements the first hosted GEM Gemscape window shell. This initial
 * version focuses on a browser-like frame with a toolbar band, address
 * band, content pane, and status bar sized from native GEM metrics so
 * it fits visually with titles, menu rows, and scroll controls.
 *
 * MIT License (see: LICENSE)
 * Copyright (C) 2026 tomaz stih
 */

#include <gem.h>
#include <gem/gemctl.h>
#include "sample_resources.h"

#include "gemscape.h"
#include "cef_browser.h"
#include <gem/os.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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
    char address_text[512];
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

static int gemscape_trace_enabled(void)
{
    const char *value = gem_os_getenv_ref("GEMSCAPE_TRACE");

    return value != NULL && value[0] != '\0';
}

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

static void gemscape_draw_vline(WORD handle, WORD x, WORD y0, WORD y1)
{
    WORD pxy[4];

    if (y1 < y0) {
        return;
    }

    (void)vsl_type(handle, 1);
    (void)vsl_width(handle, 1);
    vsl_color(handle, gemscape_ink);
    pxy[0] = x;
    pxy[1] = y0;
    pxy[2] = x;
    pxy[3] = y1;
    v_pline(handle, 2, pxy);
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
    gemctl_toolbar_bitmap(state->vdi_handle, bitblk, &slot);
}

static void gemscape_draw_text(const gemscape_state_t *state, const GRECT *rect,
                               const char *text, int centered)
{
    WORD distances[5];
    WORD extent[8];
    WORD text_height;
    WORD text_width;
    WORD ascent;
    WORD top_y;
    WORD x;
    WORD y;

    if (state == NULL || rect == NULL || text == NULL) {
        return;
    }

    memset(distances, 0, sizeof(distances));
    (void)vqt_fontinfo(state->vdi_handle, NULL, NULL, distances, NULL, NULL);
    (void)vqt_extent(state->vdi_handle, (char *)text, extent);
    text_height = (WORD)(extent[5] - extent[1] + 1);
    text_width = (WORD)(extent[2] - extent[0] + 1);
    ascent = (WORD)(distances[1] + 2);

    if (centered != 0) {
        x = (WORD)(rect->g_x + (rect->g_w - text_width) / 2);
    } else {
        x = (WORD)(rect->g_x + gemscape_margin);
    }
    top_y = (WORD)(rect->g_y + (rect->g_h - text_height) / 2);
    y = (WORD)(top_y + ascent);

    vst_color(state->vdi_handle, gemscape_ink);
    v_gtext(state->vdi_handle, x, y, (const BYTE *)text);
}

static void gemscape_draw_address_caret(const gemscape_state_t *state,
                                        const GRECT *rect)
{
    WORD extent[8];
    WORD x;

    if (state == NULL || rect == NULL ||
        !gemctl_text_edit_active(&state->address_edit))
        return;
    (void)vqt_extent(state->vdi_handle, (char *)state->address_text, extent);
    x = (WORD)(rect->g_x + gemscape_margin + extent[2] - extent[0] + 1);
    if (x < rect->g_x + rect->g_w - 2)
        gemscape_draw_vline(state->vdi_handle, x, rect->g_y + 2,
                            rect->g_y + rect->g_h - 3);
}

static void gemscape_draw_content(const gemscape_state_t *state,
                                  const GRECT *rect)
{
#ifndef GEMSCAPE_OVERLAY
    GRECT note;

    if (state == NULL || rect == NULL) {
        return;
    }

    gemscape_fill_rect(state->vdi_handle, gemscape_content_paper, rect);

    note = *rect;
    gemscape_draw_text(state, &note, "Gemscape viewport", 0);
    note.g_y = (WORD)(note.g_y + rect->g_h / 10);
    gemscape_draw_text(
        state, &note, "Address controls and viewport frame under construction.",
        0);
#else
    (void)state;
    (void)rect;
#endif
}

#if defined(GEMSCAPE_OVERLAY) && GEMSCAPE_OVERLAY
static void gemscape_update_overlay(gemscape_state_t *state,
                                    const GRECT *work, const GRECT *content)
{
    size_t bytes;
    uint8_t *pixels;

    if (state == NULL || work == NULL || content == NULL || work->g_w <= 0 ||
        work->g_h <= 0 || content->g_w <= 0 || content->g_h <= 0 ||
        state->overlay == 0)
        return;
    bytes = (size_t)content->g_w * content->g_h * 4u;
    pixels = malloc(bytes);
    if (pixels == NULL)
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
                (uint16_t)(content->g_y - work->g_y), frame_width,
                frame_height, frame, (uint16_t)(frame_width * 4u));
            received = 1;
        }
        free(pixels);
        /* CEF paints asynchronously. Keep polling until its first frame and
         * thereafter let navigation/invalidations deliver new frames. */
        state->overlay_upload_pending = received == 0 ? 1 : 0;
        return;
    }
#endif
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
            pixel[2] = (uint8_t)(180u + ((unsigned)(x + y) * 60u) /
                                         (unsigned)(content->g_w + content->g_h));
            pixel[3] = 255u;
        }
    }
    (void)gem_overlay_update(
        state->overlay, (uint16_t)(content->g_x - work->g_x),
        (uint16_t)(content->g_y - work->g_y), (uint16_t)content->g_w,
        (uint16_t)content->g_h, pixels,
                             (uint16_t)(content->g_w * 4));
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
    x_limit = content_width > viewport_width ? content_width - viewport_width : 0;
    y_limit = content_height > viewport_height
                  ? content_height - viewport_height
                  : 0;
    if (message == WM_VSLID) {
        y = (int)((LONG)value * y_limit / 1000L);
    } else if (message == WM_HSLID) {
        x = (int)((LONG)value * x_limit / 1000L);
    } else {
        switch (value) {
            case WA_UPLINE: y -= 40; break;
            case WA_DNLINE: y += 40; break;
            case WA_UPPAGE: y -= viewport_height; break;
            case WA_DNPAGE: y += viewport_height; break;
            case WA_LFLINE: x -= 40; break;
            case WA_RTLINE: x += 40; break;
            case WA_LFPAGE: x -= viewport_width; break;
            case WA_RTPAGE: x += viewport_width; break;
            default: break;
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
                gemctl_toolbar_separator(
                    state->vdi_handle, right_button_left, address.g_y,
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
            gemscape_draw_text(state, &address_text, state->address_text, 0);
            gemscape_draw_address_caret(state, &address_text);
            gemscape_draw_content(state, &content);
            gemctl_status_draw(state->vdi_handle, &status,
                               "Ready. Gemscape frame initialized.");

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

int gemscape_main(int argc, char **argv)
{
    gemscape_state_t state;
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
    {
        const char *initial_url = gem_os_getenv_ref("GEMSCAPE_URL");

        if (initial_url == NULL || initial_url[0] == '\0')
            initial_url = "https://www.youtube.com/";
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

        if (wind_get(state.handle, WF_WORKXYWH, &work.g_x, &work.g_y,
                     &work.g_w, &work.g_h) != 0) {
            gemscape_layout(&state, &work, &address, &content, &status);
            state.overlay = gem_overlay_create(state.handle,
                                               (uint16_t)work.g_w,
                                               (uint16_t)work.g_h);
            state.overlay_upload_pending = 1;
            state.overlay_resize_tick = 0;
#if defined(GEMSCAPE_CEF) && GEMSCAPE_CEF
            state.cef_started = gemscape_cef_start(
                argc, argv, (uint16_t)content.g_w, (uint16_t)content.g_h,
                state.address_text);
#endif
        }
    }
#endif
    (void)graf_mouse(M_ON, NULL);

    FOREVER
    {
        event_flags =
            evnt_multi((UWORD)(MU_MESAG | MU_BUTTON | MU_KEYBD | MU_TIMER), 1,
                       1, 1, 0,
                       0, 0, 0, 0, 0, 0, 0, 0, 0, msg, 100, 0, &mouse_x,
                       &mouse_y,
                       &mouse_buttons, &key_state, &key_code, &button_return);

#if defined(GEMSCAPE_OVERLAY) && GEMSCAPE_OVERLAY
        if ((event_flags & MU_TIMER) != 0 && state.cef_started != 0) {
            gemscape_refresh_overlay(&state);
            gemscape_update_scrollbars(&state);
        }
#endif

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

        if ((event_flags & (MU_TIMER | MU_BUTTON)) != 0 &&
            (state.have_mouse_position == 0 ||
             state.last_mouse_x != mouse_x || state.last_mouse_y != mouse_y)) {
            GRECT work;
            GRECT address;
            GRECT content;
            GRECT status;

            state.last_mouse_x = mouse_x;
            state.last_mouse_y = mouse_y;
            state.have_mouse_position = 1;
            if (wind_get(state.handle, WF_WORKXYWH, &work.g_x, &work.g_y,
                         &work.g_w, &work.g_h) != 0) {
                gemscape_layout(&state, &work, &address, &content, &status);
#if defined(GEMSCAPE_CEF) && GEMSCAPE_CEF
                if (state.cef_started != 0 && mouse_x >= content.g_x &&
                    mouse_x < content.g_x + content.g_w &&
                    mouse_y >= content.g_y && mouse_y < content.g_y + content.g_h)
                    gemscape_cef_mouse_move(mouse_x - content.g_x,
                                            mouse_y - content.g_y, 0);
#endif
            }
        }

        if ((event_flags & MU_BUTTON) != 0) {
            GRECT work;
            GRECT address;
            GRECT content;
            GRECT status;

            if (wind_get(state.handle, WF_WORKXYWH, &work.g_x, &work.g_y,
                         &work.g_w, &work.g_h) != 0) {
                gemscape_layout(&state, &work, &address, &content, &status);
                if (gemscape_trace_enabled())
                    fprintf(stderr, "gemscape: click %d,%d content=%d,%d %dx%d\n",
                            mouse_x, mouse_y, content.g_x, content.g_y,
                            content.g_w, content.g_h);
                if (mouse_x >= address.g_x && mouse_x < address.g_x + address.g_w &&
                    mouse_y >= address.g_y && mouse_y < address.g_y + address.g_h) {
                    WORD button_size = address.g_h;
                    WORD slot = (WORD)((mouse_x - address.g_x) / button_size);
                    state.browser_focused = 0;
#if defined(GEMSCAPE_CEF) && GEMSCAPE_CEF
                    if (state.cef_started != 0)
                        gemscape_cef_focus(0);
#endif
                    if (slot == 0) {
#if defined(GEMSCAPE_CEF) && GEMSCAPE_CEF
                        gemscape_cef_back();
#endif
                    } else if (slot == 1) {
#if defined(GEMSCAPE_CEF) && GEMSCAPE_CEF
                        gemscape_cef_forward();
#endif
                    } else if (slot == 2) {
#if defined(GEMSCAPE_CEF) && GEMSCAPE_CEF
                        gemscape_cef_reload();
#endif
                    } else {
                        gemctl_text_edit_focus(&state.address_edit, 1);
                    }
                    gemscape_draw_frame(&state, NULL);
                } else {
                    int content_hit =
                        mouse_x >= content.g_x &&
                        mouse_x < content.g_x + content.g_w &&
                        mouse_y >= content.g_y &&
                        mouse_y < content.g_y + content.g_h;

                    gemctl_text_edit_blur(&state.address_edit);
                    state.browser_focused = content_hit;
#if defined(GEMSCAPE_CEF) && GEMSCAPE_CEF
                    if (state.cef_started != 0) {
                        gemscape_cef_focus(content_hit);
                    }
                    if (state.cef_started != 0 && content_hit != 0) {
                        gemscape_cef_mouse_click(
                            mouse_x - content.g_x, mouse_y - content.g_y,
                            1, 0);
                        gemscape_cef_mouse_click(
                            mouse_x - content.g_x, mouse_y - content.g_y,
                            1, 1);
                    }
#endif
                }
            }
        }

        if ((event_flags & MU_KEYBD) != 0) {
            if (gemctl_text_edit_active(&state.address_edit)) {
                gemctl_text_edit_result_t result = gemctl_text_edit_key(
                    &state.address_edit, (uint16_t)key_code);

                if (result == GEMCTL_TEXT_EDIT_CHANGED) {
                    gemscape_draw_frame(&state, NULL);
                } else if (result == GEMCTL_TEXT_EDIT_ACCEPT) {
                    gemctl_text_edit_blur(&state.address_edit);
#if defined(GEMSCAPE_CEF) && GEMSCAPE_CEF
                    if (state.cef_started != 0)
                        gemscape_cef_load_url(state.address_text);
#endif
                    gemscape_draw_frame(&state, NULL);
                } else if (result == GEMCTL_TEXT_EDIT_CANCEL) {
                    gemctl_text_edit_blur(&state.address_edit);
                    gemscape_draw_frame(&state, NULL);
                }
#if defined(GEMSCAPE_CEF) && GEMSCAPE_CEF
            } else if (state.browser_focused != 0 &&
                       state.cef_started != 0) {
                gemscape_cef_key((uint16_t)key_code, (uint16_t)key_state);
#endif
            } else if ((key_code & 0x00ff) == 27) {
                break;
            }
        }
    }

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
    return 0;
}
