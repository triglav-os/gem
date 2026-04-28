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

#include <stdint.h>

enum {
    _vdi_handle_screen = 1,
    _vdi_default_width = 640,
    _vdi_default_height = 400,
    _vdi_text_width = 6,
    _vdi_text_height = 8
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
    int open;
} vdi_state_t;

extern vdi_state_t _vdi;

WORD _vdi_parse_env_word(const char *name, WORD fallback);
void _vdi_pump_events(void);
void _vdi_present_screen(void);
void _vdi_set_screen_pixel(WORD x, WORD y, WORD color);
WORD _vdi_get_screen_pixel(WORD x, WORD y);
void _vdi_clear_screen(WORD color);
void _vdi_draw_line_segment(WORD x0, WORD y0, WORD x1, WORD y1, WORD color);
void _vdi_fill_rect(WORD x0, WORD y0, WORD x1, WORD y1, WORD color);
void _vdi_draw_glyph(WORD x, WORD y, char ch, WORD color);
char _vdi_scancode_to_ascii(uint16_t key);
int _vdi_uses_screen(const MFDB *mfdb);
WORD _vdi_mfdb_get_pixel(const MFDB *mfdb, WORD x, WORD y);
void _vdi_mfdb_set_pixel(MFDB *mfdb, WORD x, WORD y, WORD color);
void _vdi_fill_work_out(WORD work_out[57]);

#endif
