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
    vdi_handle_screen = 1,
    vdi_default_width = 992,
    vdi_default_height = 1400,
    vdi_standard_cursor_count = 8
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
    /* Accumulated damage for partial FB presents (avoids full-screen blit). */
    WORD dirty_valid;
    WORD dirty_x0;
    WORD dirty_y0;
    WORD dirty_x1;
    WORD dirty_y1;
    MFORM mouse_form;
    MFORM standard_mouse_forms[vdi_standard_cursor_count];
    int open;
} vdi_state_t;

typedef struct vdi_rect {
    WORD x0;
    WORD y0;
    WORD x1;
    WORD y1;
} vdi_rect_t;

extern vdi_state_t vdi_state;

WORD vdi_parse_env_word(const char *name, WORD fallback);
WORD vdi_load_standard_mouse_forms(void);
int vdi_load_fonts(void);
/* Validate bitmap and glyph offsets before trusting an on-disk GEM font. */
int vdi_font_bitmap_valid(const uint8_t *data, size_t size, WORD first,
                          WORD last, WORD width, WORD height,
                          uint32_t bitmap_offset, uint32_t table_offset);
WORD vdi_load_external_fonts(void);
WORD vdi_unload_external_fonts(void);
void vdi_unload_fonts(void);
WORD vdi_set_mouse_form(const MFORM *form);
WORD vdi_select_system_mouse_form(WORD selector);
WORD vdi_select_font(WORD font_id);
WORD vdi_default_font_id(void);
WORD vdi_font_count(void);
WORD vdi_font_id_for_element(WORD element_num);
const char *vdi_font_name(WORD element_num);
WORD vdi_font_cell_width(void);
WORD vdi_font_text_height(void);
WORD vdi_font_ascent(void);
WORD vdi_font_first_ade(void);
WORD vdi_font_last_ade(void);
WORD vdi_char_cell_width(char ch);
WORD vdi_string_width(const char *string);
WORD vdi_text_background_mode(void);
WORD vdi_write_mode(void);
void vdi_pump_events(void);
void vdi_set_mouse_state(WORD x, WORD y, WORD status);
void vdi_prepare_screen_write(void);
void vdi_begin_update(void);
void vdi_end_update(void);
void vdi_end_update_no_present(void);
void vdi_present_screen(void);
void vdi_mark_dirty(WORD x0, WORD y0, WORD x1, WORD y1);
void vdi_flush_rect(WORD x, WORD y, WORD w, WORD h);
void vdi_flush_display(void);
void vdi_get_active_clip_rect(vdi_rect_t *rect);
int vdi_intersect_rects(const vdi_rect_t *left, const vdi_rect_t *right,
                        vdi_rect_t *out);
int vdi_point_visible(WORD x, WORD y);
void vdi_plot_pixel(WORD x, WORD y, WORD color);
void vdi_set_screen_pixel(WORD x, WORD y, WORD color);
WORD vdi_get_screen_pixel(WORD x, WORD y);
void vdi_clear_screen(WORD color);
int vdi_clip_line_segment(WORD *x0, WORD *y0, WORD *x1, WORD *y1);
void vdi_draw_screen_hline(WORD y, WORD x0, WORD x1, WORD color);
void vdi_draw_screen_hline_direct(WORD y, WORD left, WORD right, WORD color);
void vdi_draw_line_segment(WORD x0, WORD y0, WORD x1, WORD y1, WORD color);
void vdi_fill_rect(WORD x0, WORD y0, WORD x1, WORD y1, WORD color);
void vdi_draw_glyph(WORD x, WORD y, char ch, WORD color);
char vdi_scancode_to_ascii(uint16_t key);
int vdi_uses_screen(const MFDB *mfdb);
WORD vdi_mfdb_get_pixel(const MFDB *mfdb, WORD x, WORD y);
void vdi_mfdb_set_pixel(MFDB *mfdb, WORD x, WORD y, WORD color);
void vdi_mfdb_draw_hline(MFDB *mfdb, WORD y, WORD x0, WORD x1, WORD color);
void vdi_fill_work_out(WORD work_out[57]);

#endif
