/*
 * Declares Gemscape's CEF off-screen browser integration.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (C) 2026 tomaz stih
 */

#ifndef GEMSCAPE_CEF_BROWSER_H
#define GEMSCAPE_CEF_BROWSER_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

struct gemscape_preferences;

typedef struct gemscape_browser_info {
    char url[2048];
    char title[256];
    char status[256];
    unsigned int revision;
    int loading;
    int can_back;
    int can_forward;
    int clearing;
} gemscape_browser_info_t;

/* Initializes the browser and its configured persistent or memory profile. */
int gemscape_cef_start(int argc, char **argv, uint16_t width, uint16_t height,
                       const char *url,
                       const struct gemscape_preferences *preferences);
/* Reason for the latest gemscape_cef_start failure, formatted for an alert
 * body with '|' line breaks. Valid until the next start attempt. */
const char *gemscape_cef_start_error(void);
/* Runs CEF subprocesses; returns -1 when this is the main application. */
int gemscape_cef_execute_process(int argc, char **argv);
/* Services Chromium and periodically requests CSS layout metrics. */
void gemscape_cef_pump(void);
/* Resizes the off-screen viewport and schedules a new paint. */
void gemscape_cef_resize(uint16_t width, uint16_t height);
/* Returns the newest composited BGRA frame, valid until the next take/stop. */
int gemscape_cef_take_frame(uint16_t *width, uint16_t *height,
                            const uint8_t **pixels, uint32_t *size);
/* Navigates the main frame to a caller-validated address. */
void gemscape_cef_load_url(const char *url);
/* Sends pointer motion in viewport pixels, including leave and drag state. */
void gemscape_cef_mouse_move(int x, int y, int leave, uint16_t modifiers);
/* Sends a real left/right button transition and its click count. */
void gemscape_cef_mouse_click(int x, int y, int button, int mouse_up,
                              int clicks, uint16_t modifiers);
/* Sends a vertical wheel delta at viewport coordinates. */
void gemscape_cef_mouse_wheel(int x, int y, int delta_y);
/* Transfers keyboard focus to or from the web view. */
void gemscape_cef_focus(int focused);
/* Translates GEM USB/ASCII keys and modifiers into CEF keyboard events. */
void gemscape_cef_key(uint16_t key, uint16_t modifiers);
/* Returns the browser-session plain-text clipboard. */
const char *gemscape_cef_clipboard(void);
/* Replaces the browser-session plain-text clipboard with a bounded copy. */
void gemscape_cef_set_clipboard(const char *text);
/* Returns the last valid CSS scroll/viewport/document metrics, or zero. */
int gemscape_cef_get_scroll(int *x, int *y, int *viewport_width,
                            int *viewport_height, int *content_width,
                            int *content_height);
/* Scrolls the main document to absolute CSS coordinates. */
void gemscape_cef_scroll_to(int x, int y);
/* Navigates backwards if a history entry exists. */
void gemscape_cef_back(void);
/* Navigates forwards if a history entry exists. */
void gemscape_cef_forward(void);
/* Reloads the current page. */
void gemscape_cef_reload(void);
/* Returns browser-owned navigation/loading state for the GEM chrome. */
const gemscape_browser_info_t *gemscape_cef_info(void);
/* Applies page zoom immediately, expressed as a percentage. */
void gemscape_cef_zoom(int percent);
/* Cancels the current page load without shutting down CEF. */
void gemscape_cef_cancel_load(void);
/* Clears cookies (0), HTTP cache (1), or current-origin storage (2)
 * asynchronously. */
int gemscape_cef_clear_data(int kind);
/* Closes CEF, flushes profile data and releases owned objects and locks. */
void gemscape_cef_stop(void);

#ifdef __cplusplus
}
#endif

#endif
