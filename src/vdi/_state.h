/*
 * Declares shared private state and helper APIs for the hosted GEM VDI
 * compatibility layer. This header lets the public VDI entry points be
 * split across smaller modules while they continue to share one
 * workstation state block and one set of geometry helpers.
 *
 * MIT License (see: LICENSE)
 * Copyright (C) 2026 tomaz stih
 */

#ifndef GEM_PRIVATE_VDI_STATE_H
#define GEM_PRIVATE_VDI_STATE_H

#include "_internal.h"

enum {
    VDI_COMPAT_COLORS = 16,
    VDI_INPUT_DEVICES = 8,
    VDI_META_NAME_LEN = 260,
    VDI_POLYGON_MAX_POINTS = 256
};

#define VDI_PI 3.14159265358979323846

typedef struct vdi_compat_state {
    WORD clip_enabled;
    WORD clip[4];
    WORD output_window[4];
    WORD write_mode;
    WORD line_style;
    WORD line_width;
    WORD line_beg_style;
    WORD line_end_style;
    WORD line_pattern;
    WORD marker_type;
    WORD marker_height;
    WORD marker_color;
    WORD text_font;
    WORD text_height;
    WORD text_rotation;
    WORD text_alignment_h;
    WORD text_alignment_v;
    WORD text_background;
    WORD text_effects;
    WORD fill_interior;
    WORD fill_style;
    WORD fill_perimeter;
    WORD fill_pattern[16];
    WORD input_mode[VDI_INPUT_DEVICES];
    WORD cursor_row;
    WORD cursor_column;
    WORD reverse_video;
    WORD palette[VDI_COMPAT_COLORS][3];
    VOID (*timv)(void);
    VOID (*butv)(void);
    VOID (*motv)(void);
    VOID (*curv)(void);
    BYTE meta_filename[VDI_META_NAME_LEN];
} vdi_compat_state_t;

extern vdi_compat_state_t _vdi_compat;
extern int _vdi_compat_initialized;

WORD _vdi_valid_handle(WORD handle);
WORD _vdi_color_to_pixel(WORD color_index);
WORD _vdi_min_word(WORD left, WORD right);
WORD _vdi_max_word(WORD left, WORD right);

void _vdi_prepare_defaults(void);
void _vdi_reset_workstation_state(void);

WORD _vdi_text_background_mode(void);
WORD _vdi_write_mode(void);

WORD _vdi_string_length(const char *string);
void _vdi_set_extent_for_string(const char *string, WORD extent[8]);
void _vdi_store_rgb(WORD index, const WORD rgb[3]);
void _vdi_copy_rgb(WORD index, WORD rgb[3]);

void _vdi_rect_from_xy(const WORD xy[4], WORD *x0, WORD *y0, WORD *x1,
    WORD *y1);
void _vdi_draw_line(WORD x0, WORD y0, WORD x1, WORD y1, WORD color);
void _vdi_fill_box(WORD x0, WORD y0, WORD x1, WORD y1, WORD color);
void _vdi_outline_box(WORD x0, WORD y0, WORD x1, WORD y1, WORD color);
void _vdi_fill_polygon_points(const WORD *points, WORD count, WORD color);
WORD _vdi_build_arc_points(WORD cx, WORD cy, WORD xrad, WORD yrad,
    WORD start, WORD end, WORD *points, WORD close_polygon);
void _vdi_draw_polyline_points(const WORD *points, WORD count, WORD color,
    WORD closed);

#endif
