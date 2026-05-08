/*
 * Declares private helpers shared by the minimal GEM VDI implementation.
 * These helpers manage workstation state, screen pixel access, simple
 * font rendering, and MFDB copy support behind the public VDI API.
 *
 * MIT License (see: LICENSE)
 * Copyright (C) 2026 tomaz stih
 */

#ifndef GEM_PRIVATE_VDI_H
#define GEM_PRIVATE_VDI_H

#include "gem/vdi.h"

#include "platform/raster.h"

#include <stddef.h>
#include <stdint.h>

enum {
    _vdi_handle_screen = 1,
    _vdi_default_width = 992,
    _vdi_default_height = 1400,
    _vdi_standard_cursor_count = 8
};

typedef struct vdi_state {
    gem_raster_surface_t *surface;
    WORD width;
    WORD height;
    WORD line_color;
    WORD fill_color;
    WORD text_color;
    WORD mouse_x;
    WORD mouse_y;
    WORD mouse_status;
    WORD cursor_hidden;
    WORD cursor_drawn;
    WORD cursor_x;
    WORD cursor_y;
    WORD update_depth;
    WORD present_pending;
    MFORM mouse_form;
    MFORM standard_mouse_forms[_vdi_standard_cursor_count];
    int open;
} vdi_state_t;

typedef struct vdi_rect {
    WORD x0;
    WORD y0;
    WORD x1;
    WORD y1;
} vdi_rect_t;

extern vdi_state_t _vdi;

WORD _vdi_parse_env_word(const char *name, WORD fallback);
WORD _vdi_load_standard_mouse_forms(void);
int _vdi_load_fonts(void);
WORD _vdi_load_external_fonts(void);
WORD _vdi_unload_external_fonts(void);
void _vdi_unload_fonts(void);
WORD _vdi_set_mouse_form(const MFORM *form);
WORD _vdi_select_system_mouse_form(WORD selector);
WORD _vdi_select_font(WORD font_id);
WORD _vdi_default_font_id(void);
WORD _vdi_font_count(void);
WORD _vdi_font_id_for_element(WORD element_num);
const char *_vdi_font_name(WORD element_num);
WORD _vdi_font_cell_width(void);
WORD _vdi_font_text_height(void);
WORD _vdi_font_ascent(void);
WORD _vdi_font_first_ade(void);
WORD _vdi_font_last_ade(void);
WORD _vdi_char_cell_width(char ch);
WORD _vdi_string_width(const char *string);
WORD _vdi_text_background_mode(void);
WORD _vdi_write_mode(void);
void _vdi_pump_events(void);
void _vdi_set_mouse_state(WORD x, WORD y, WORD status);
void _vdi_prepare_screen_write(void);
void _vdi_begin_update(void);
void _vdi_end_update(void);
void _vdi_present_screen(void);
void _vdi_get_active_clip_rect(vdi_rect_t *rect);
int _vdi_intersect_rects(const vdi_rect_t *left,
    const vdi_rect_t *right,
    vdi_rect_t *out);
int _vdi_point_visible(WORD x, WORD y);
void _vdi_plot_pixel(WORD x, WORD y, WORD color);
void _vdi_set_screen_pixel(WORD x, WORD y, WORD color);
WORD _vdi_get_screen_pixel(WORD x, WORD y);
void _vdi_clear_screen(WORD color);
int _vdi_clip_line_segment(WORD *x0, WORD *y0, WORD *x1, WORD *y1);
void _vdi_draw_screen_hline(WORD y, WORD x0, WORD x1, WORD color);
void _vdi_draw_screen_hline_direct(WORD y, WORD left, WORD right, WORD color);
void _vdi_draw_line_segment(WORD x0, WORD y0, WORD x1, WORD y1, WORD color);
void _vdi_fill_rect(WORD x0, WORD y0, WORD x1, WORD y1, WORD color);
void _vdi_draw_glyph(WORD x, WORD y, char ch, WORD color);
char _vdi_scancode_to_ascii(uint16_t key);
int _vdi_uses_screen(const MFDB *mfdb);
WORD _vdi_mfdb_get_pixel(const MFDB *mfdb, WORD x, WORD y);
void _vdi_mfdb_set_pixel(MFDB *mfdb, WORD x, WORD y, WORD color);
void _vdi_mfdb_draw_hline(MFDB *mfdb, WORD y, WORD x0, WORD x1, WORD color);
void _vdi_fill_work_out(WORD work_out[57]);

#endif
