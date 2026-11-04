/*
 * Declares the private helpers shared by the hosted AES window and object
 * modules: geometry queries, painting primitives, chrome presentation and
 * object-part renderers. These are implementation details of libaes and are
 * not part of the public GEM interface.
 *
 * MIT License (see: LICENSE)
 * Copyright (C) 2026 tomaz stih
 */

#ifndef GEM_AES_WINDOW_INTERNAL_H
#define GEM_AES_WINDOW_INTERNAL_H

#include "aes_internal.h"

#include <stddef.h>
#include <stdint.h>

/* Left border width in pixels for the window kind. */
WORD aes_window_left_border(const aes_window_t *window);
/* Nonzero when the kind carries any vertical scroll part. */
int aes_window_has_vscroll(const aes_window_t *window);
/* Nonzero when the kind carries any horizontal scroll part. */
int aes_window_has_hscroll(const aes_window_t *window);
/* Thickness of a scroll bar strip, never below 12. */
WORD aes_window_scroll_span(const aes_window_t *window);
/* Side length of a scroll arrow button. */
WORD aes_window_scroll_button_side(const aes_window_t *window);
/* Right border width, including a vertical scroll bar. */
WORD aes_window_right_border(const aes_window_t *window);
/* Bottom border height, including a horizontal scroll bar. */
WORD aes_window_bottom_border(const aes_window_t *window);
/* Title strip height for the kind; 0 for a bare window. */
WORD aes_window_title_height(const aes_window_t *window);
/* Rectangle of the close box; 0 when the kind lacks it. */
int aes_window_closer_rect(const aes_window_t *window, GRECT *rect);
/* Rectangle of the full box; 0 when the kind lacks it. */
int aes_window_fuller_rect(const aes_window_t *window, GRECT *rect);
/* Rectangle of the draggable title area; 0 when absent. */
int aes_window_title_rect(const aes_window_t *window, GRECT *rect);
/* Rectangle of the size box; 0 when the kind lacks it. */
int aes_window_sizer_rect(const aes_window_t *window, GRECT *rect);
/* Rectangle of the up arrow; 0 when absent. */
int aes_window_vup_rect(const aes_window_t *window, GRECT *rect);
/* Rectangle of the down arrow; 0 when absent. */
int aes_window_vdown_rect(const aes_window_t *window, GRECT *rect);
/* Rectangle of the left arrow; 0 when absent. */
int aes_window_hleft_rect(const aes_window_t *window, GRECT *rect);
/* Rectangle of the right arrow; 0 when absent. */
int aes_window_hright_rect(const aes_window_t *window, GRECT *rect);
/* Flush a window frame to the display even inside an update. */
void aes_present_window_frame(const aes_window_t *window);
/* Repaint visible title/information chrome without damaging covered windows. */
void aes_redraw_window_text(const aes_window_t *window);
/* Fill an inclusive rectangle with a VDI color index. */
void aes_fill_rect(WORD x0, WORD y0, WORD x1, WORD y1, WORD color);
/* Draw selected one-pixel edges of an inclusive rectangle. */
void aes_draw_rect_edges(WORD x0, WORD y0, WORD x1, WORD y1, int draw_top,
                         int draw_right, int draw_bottom, int draw_left);
/* Invert every pixel of an inclusive rectangle. */
void aes_invert_rect(WORD x0, WORD y0, WORD x1, WORD y1);
/* Fill a rectangle with an 8-pixel-wide row pattern. */
void aes_fill_pattern_rect(WORD x0, WORD y0, WORD x1, WORD y1,
                           const uint8_t *rows, size_t row_count);
/* Fill a rectangle with the desktop checker pattern. */
void aes_fill_checker_rect(WORD x0, WORD y0, WORD x1, WORD y1);
/* Draw a horizontal line in a color index. */
void aes_draw_hline(WORD x0, WORD x1, WORD y, WORD color);
/* Draw a vertical line in a color index. */
void aes_draw_vline(WORD x, WORD y0, WORD y1, WORD color);
/* Draw a string at a text baseline in a color index. */
void aes_draw_text(WORD x, WORD y, WORD color, const char *text);
/* Draw a string, then stipple it for the DISABLED state. */
void aes_stipple_text_pixels(WORD x, WORD y, WORD foreground, WORD background,
                             const char *text);
/* Emit a GEM_TRACE_DRAW line; resolves the variable once. */
void aes_draw_trace(const char *fmt, ...);
/* Nonzero when a tree root is an application work area. */
int aes_is_window_work_root(const OBJECT *tree);
/* Nonzero when a tree root is a bordered dialog box. */
int aes_is_dialog_root_tree(const OBJECT *tree);
/* Nonzero when an object is the menu bar container. */
int aes_is_menu_bar_object(const OBJECT *tree, WORD object);
/* Nonzero when a child is a dialog frame decoration. */
int aes_is_dialog_frame_object(const OBJECT *tree, WORD object, WORD parent);
/* Draw an editable/text object from its TEDINFO. */
void aes_draw_ted_object(const OBJECT *tree, WORD object, const OBJECT *obj,
                         const TEDINFO *ted, const WORD rect[4]);
/* Draw the double frame of a dialog root. */
void aes_draw_dialog_frame(const WORD rect[4]);
/* Draw a button frame, thicker for the default button. */
void aes_draw_button_frame(const WORD rect[4], WORD dark_color,
                           WORD light_color, int default_button);
/* Draw an ICONBLK with its mask, data and caption. */
void aes_draw_icon_object(const OBJECT *obj, const ICONBLK *icon, WORD abs_x,
                          WORD abs_y, const WORD rect[4]);
/* Invoke a USERDEF callback with a filled PARMBLK. */
void aes_draw_user_object(const OBJECT *tree, WORD object, const OBJECT *obj,
                          WORD abs_x, WORD abs_y, WORD clip[4]);
/* Visible rectangles of a window (or the desktop for handle 0). */
WORD aes_build_visible_rects(WORD handle, GRECT out[], WORD max_rects);
/* Repaint only the slider thumb area that a field change moved. */
void aes_redraw_scrollbar_delta(const aes_window_t *before,
                                const aes_window_t *after, WORD field);

#endif
