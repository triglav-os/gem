/*
 * Declares Gemscape's CEF off-screen browser integration.
 *
 * MIT License (see: LICENSE)
 * Copyright (C) 2026 tomaz stih
 */

#ifndef GEMSCAPE_CEF_BROWSER_H
#define GEMSCAPE_CEF_BROWSER_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

int gemscape_cef_start(int argc, char **argv, uint16_t width, uint16_t height,
                       const char *url);
int gemscape_cef_execute_process(int argc, char **argv);
void gemscape_cef_pump(void);
void gemscape_cef_resize(uint16_t width, uint16_t height);
int gemscape_cef_take_frame(uint16_t *width, uint16_t *height,
                            const uint8_t **pixels, uint32_t *size);
void gemscape_cef_load_url(const char *url);
void gemscape_cef_mouse_move(int x, int y, int leave);
void gemscape_cef_mouse_click(int x, int y, int button, int mouse_up);
void gemscape_cef_mouse_wheel(int x, int y, int delta_y);
void gemscape_cef_focus(int focused);
void gemscape_cef_key(uint16_t key, uint16_t modifiers);
int gemscape_cef_get_scroll(int *x, int *y, int *viewport_width,
                            int *viewport_height, int *content_width,
                            int *content_height);
void gemscape_cef_scroll_to(int x, int y);
void gemscape_cef_back(void);
void gemscape_cef_forward(void);
void gemscape_cef_reload(void);
void gemscape_cef_stop(void);

#ifdef __cplusplus
}
#endif

#endif
