/*
 * Implements the core hosted GEM VDI workstation entry points and the
 * shared compatibility state used by the split VDI modules. This file
 * keeps workstation lifecycle, clipping, and common helper routines in
 * one place while the larger primitive, raster, and text surfaces live
 * in smaller translation units.
 *
 * MIT License (see: LICENSE)
 * Copyright (C) 2026 tomaz stih
 */

#include "gem/vdi.h"

#include "_internal.h"
#include "_state.h"

#include "platform/hid.h"
#include "platform/os.h"
#include "platform/raster.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

vdi_compat_state_t _vdi_compat;
int _vdi_compat_initialized;

WORD _vdi_color_to_pixel(WORD color_index)
{
    return (color_index == 0) ? 1 : 0;
}

VOID v_opnvwk(WORD work_in[11], VDI_HANDLE *handle, WORD work_out[57])
{
    WORD width = _vdi_parse_env_word("GEM_VDI_WIDTH", _vdi_default_width);
    WORD height = _vdi_parse_env_word("GEM_VDI_HEIGHT", _vdi_default_height);

    (void) work_in;

    if (handle == NULL || work_out == NULL) {
        return;
    }

    _vdi_prepare_defaults();
    if (!_vdi.open) {
        if (!_vdi_load_fonts()) {
            *handle = 0;
            return;
        }
        if (!gem_os_init()) {
            _vdi_unload_fonts();
            *handle = 0;
            return;
        }
        if (!gem_raster_init((uint16_t) width, (uint16_t) height,
            GEM_RASTER_MONO1)) {
            gem_os_shutdown();
            _vdi_unload_fonts();
            *handle = 0;
            return;
        }
        (void) gem_hid_init();
        gem_os_sleep_ms(20u);
        (void) gem_raster_resync();
        gem_os_sleep_ms(20u);
        (void) gem_raster_resync();

        memset(&_vdi, 0, sizeof(_vdi));
        _vdi.surface = gem_raster_surface();
        _vdi.width = width;
        _vdi.height = height;
        _vdi.line_color = _vdi_color_to_pixel(1);
        _vdi.fill_color = _vdi_color_to_pixel(1);
        _vdi.text_color = _vdi_color_to_pixel(1);
        _vdi.cursor_hidden = 1;
        _vdi.open = 1;
        (void) _vdi_load_standard_mouse_forms();
        gem_raster_set_palette(0u, 0u, 0u, 0u);
        gem_raster_set_palette(1u, 255u, 255u, 255u);
        _vdi_clear_screen(0);
        _vdi_present_screen();
    }

    _vdi_reset_workstation_state();
    *handle = _vdi_handle_screen;
    _vdi_fill_work_out(work_out);
}

VOID v_clsvwk(VDI_HANDLE handle)
{
    if (handle != _vdi_handle_screen || !_vdi.open) {
        return;
    }

    gem_hid_shutdown();
    gem_raster_shutdown();
    gem_os_shutdown();
    _vdi_unload_fonts();
    memset(&_vdi, 0, sizeof(_vdi));
}

VOID v_clrwk(VDI_HANDLE handle)
{
    if (handle != _vdi_handle_screen || !_vdi.open) {
        return;
    }

    _vdi_clear_screen(0);
    _vdi_present_screen();
}

VOID v_pline(VDI_HANDLE handle, WORD count, CONST WORD *pxy)
{
    WORD i;

    if (handle != _vdi_handle_screen || !_vdi.open || pxy == NULL ||
        count < 2) {
        return;
    }

    for (i = 0; i < (WORD) (count - 1); ++i) {
        WORD x0 = pxy[i * 2];
        WORD y0 = pxy[i * 2 + 1];
        WORD x1 = pxy[i * 2 + 2];
        WORD y1 = pxy[i * 2 + 3];

        _vdi_draw_line_segment(x0, y0, x1, y1, _vdi.line_color);
    }

    _vdi_present_screen();
}

VOID v_bar(VDI_HANDLE handle, CONST WORD pxy[4])
{
    if (handle != _vdi_handle_screen || !_vdi.open || pxy == NULL) {
        return;
    }

    _vdi_fill_rect(pxy[0], pxy[1], pxy[2], pxy[3], _vdi.fill_color);
    _vdi_present_screen();
}

VOID v_gtext(VDI_HANDLE handle, WORD x, WORD y, CONST BYTE *text)
{
    WORD pen_x = x;
    WORD top_y = (WORD) (y - _vdi_font_ascent());
    const char *cursor = (const char *) text;

    if (handle != _vdi_handle_screen || !_vdi.open || text == NULL) {
        return;
    }

    while (*cursor != '\0') {
        _vdi_draw_glyph(pen_x, top_y, *cursor, _vdi.text_color);
        pen_x = (WORD) (pen_x + _vdi_char_cell_width(*cursor));
        ++cursor;
    }

    _vdi_present_screen();
}

VOID vst_color(VDI_HANDLE handle, WORD color_index)
{
    if (handle == _vdi_handle_screen && _vdi.open) {
        _vdi.text_color = _vdi_color_to_pixel(color_index);
    }
}

VOID vsf_color(VDI_HANDLE handle, WORD color_index)
{
    if (handle == _vdi_handle_screen && _vdi.open) {
        _vdi.fill_color = _vdi_color_to_pixel(color_index);
    }
}

VOID vsl_color(VDI_HANDLE handle, WORD color_index)
{
    if (handle == _vdi_handle_screen && _vdi.open) {
        _vdi.line_color = _vdi_color_to_pixel(color_index);
    }
}

WORD _vdi_valid_handle(WORD handle)
{
    return _vdi.open != 0 && handle == _vdi_handle_screen;
}

WORD _vdi_min_word(WORD left, WORD right)
{
    return (left < right) ? left : right;
}

WORD _vdi_max_word(WORD left, WORD right)
{
    return (left > right) ? left : right;
}

void _vdi_rect_from_xy(const WORD xy[4], WORD *x0, WORD *y0, WORD *x1,
    WORD *y1)
{
    *x0 = _vdi_min_word(xy[0], xy[2]);
    *y0 = _vdi_min_word(xy[1], xy[3]);
    *x1 = _vdi_max_word(xy[0], xy[2]);
    *y1 = _vdi_max_word(xy[1], xy[3]);
}

void _vdi_get_active_clip_rect(vdi_rect_t *rect)
{
    vdi_rect_t screen;
    vdi_rect_t requested;

    if (rect == NULL) {
        return;
    }

    screen.x0 = 0;
    screen.y0 = 0;
    screen.x1 = (WORD) (_vdi.width - 1);
    screen.y1 = (WORD) (_vdi.height - 1);
    *rect = screen;
    if (_vdi_compat.clip_enabled == 0) {
        return;
    }

    _vdi_rect_from_xy(_vdi_compat.clip, &requested.x0, &requested.y0,
        &requested.x1, &requested.y1);
    if (!_vdi_intersect_rects(&screen, &requested, rect)) {
        *rect = screen;
        rect->x1 = -1;
        rect->y1 = -1;
    }
}

int _vdi_intersect_rects(const vdi_rect_t *left, const vdi_rect_t *right,
    vdi_rect_t *out)
{
    if (left == NULL || right == NULL || out == NULL) {
        return 0;
    }

    out->x0 = _vdi_max_word(left->x0, right->x0);
    out->y0 = _vdi_max_word(left->y0, right->y0);
    out->x1 = _vdi_min_word(left->x1, right->x1);
    out->y1 = _vdi_min_word(left->y1, right->y1);
    return out->x0 <= out->x1 && out->y0 <= out->y1;
}

int _vdi_point_visible(WORD x, WORD y)
{
    vdi_rect_t clip;

    if (!_vdi.open) {
        return 0;
    }

    _vdi_get_active_clip_rect(&clip);
    return x >= clip.x0 && x <= clip.x1 && y >= clip.y0 && y <= clip.y1;
}

void _vdi_plot_pixel(WORD x, WORD y, WORD color)
{
    if (_vdi_point_visible(x, y)) {
        _vdi_set_screen_pixel(x, y, color);
    }
}

void _vdi_draw_line(WORD x0, WORD y0, WORD x1, WORD y1, WORD color)
{
    _vdi_draw_line_segment(x0, y0, x1, y1, color);
}

void _vdi_fill_box(WORD x0, WORD y0, WORD x1, WORD y1, WORD color)
{
    _vdi_fill_rect(x0, y0, x1, y1, color);
}

void _vdi_outline_box(WORD x0, WORD y0, WORD x1, WORD y1, WORD color)
{
    _vdi_draw_line(x0, y0, x1, y0, color);
    _vdi_draw_line(x1, y0, x1, y1, color);
    _vdi_draw_line(x1, y1, x0, y1, color);
    _vdi_draw_line(x0, y1, x0, y0, color);
}

void _vdi_prepare_defaults(void)
{
    if (_vdi_compat_initialized != 0) {
        return;
    }

    memset(&_vdi_compat, 0, sizeof(_vdi_compat));
    _vdi_compat.write_mode = 1;
    _vdi_compat.line_style = 1;
    _vdi_compat.line_width = 1;
    _vdi_compat.line_beg_style = 0;
    _vdi_compat.line_end_style = 0;
    _vdi_compat.marker_type = 3;
    _vdi_compat.marker_height = _vdi_font_text_height();
    _vdi_compat.marker_color = 1;
    _vdi_compat.text_font = 3;
    _vdi_compat.text_height = _vdi_font_text_height();
    _vdi_compat.fill_interior = 1;
    _vdi_compat.fill_style = 1;
    _vdi_compat.fill_perimeter = 1;
    _vdi_compat.palette[0][0] = 0;
    _vdi_compat.palette[0][1] = 0;
    _vdi_compat.palette[0][2] = 0;
    _vdi_compat.palette[1][0] = 1000;
    _vdi_compat.palette[1][1] = 1000;
    _vdi_compat.palette[1][2] = 1000;
    _vdi_compat_initialized = 1;
}

void _vdi_reset_workstation_state(void)
{
    _vdi_compat.clip[0] = 0;
    _vdi_compat.clip[1] = 0;
    _vdi_compat.clip[2] = (WORD) (_vdi.width - 1);
    _vdi_compat.clip[3] = (WORD) (_vdi.height - 1);
    _vdi_compat.output_window[0] = _vdi_compat.clip[0];
    _vdi_compat.output_window[1] = _vdi_compat.clip[1];
    _vdi_compat.output_window[2] = _vdi_compat.clip[2];
    _vdi_compat.output_window[3] = _vdi_compat.clip[3];
    _vdi_compat.clip_enabled = 0;
}

WORD _vdi_string_length(const char *string)
{
    size_t length;

    if (string == NULL) {
        return 0;
    }

    length = strlen(string);
    if (length > 32767u) {
        length = 32767u;
    }
    return (WORD) length;
}

void _vdi_set_extent_for_string(const char *string, WORD extent[8])
{
    WORD width = _vdi_string_width(string);
    WORD text_height = _vdi_font_text_height();

    extent[0] = 0;
    extent[1] = 0;
    extent[2] = (WORD) (width - ((width > 0) ? 1 : 0));
    extent[3] = 0;
    extent[4] = (WORD) (width - ((width > 0) ? 1 : 0));
    extent[5] = (WORD) (text_height - 1);
    extent[6] = 0;
    extent[7] = (WORD) (text_height - 1);
}

void _vdi_store_rgb(WORD index, const WORD rgb[3])
{
    if (index < 0 || index >= VDI_COMPAT_COLORS || rgb == NULL) {
        return;
    }

    _vdi_compat.palette[index][0] = rgb[0];
    _vdi_compat.palette[index][1] = rgb[1];
    _vdi_compat.palette[index][2] = rgb[2];
}

void _vdi_copy_rgb(WORD index, WORD rgb[3])
{
    if (rgb == NULL) {
        return;
    }
    if (index < 0 || index >= VDI_COMPAT_COLORS) {
        rgb[0] = 0;
        rgb[1] = 0;
        rgb[2] = 0;
        return;
    }

    rgb[0] = _vdi_compat.palette[index][0];
    rgb[1] = _vdi_compat.palette[index][1];
    rgb[2] = _vdi_compat.palette[index][2];
}

WORD v_opnwk(WORD work_in[], WORD *handle, WORD work_out[])
{
    VDI_HANDLE wk_handle = 0;

    _vdi_prepare_defaults();
    v_opnvwk(work_in, &wk_handle, work_out);
    if (handle != NULL) {
        *handle = wk_handle;
    }
    return wk_handle;
}

WORD v_opnrwk(WORD work_in[], WORD *handle, WORD work_out[])
{
    return v_opnwk(work_in, handle, work_out);
}

WORD v_clswk(WORD handle)
{
    if (!_vdi_valid_handle(handle)) {
        return 0;
    }

    v_clsvwk(handle);
    return 1;
}

WORD _vdi_text_background_mode(void)
{
    return _vdi_compat.text_background;
}

WORD _vdi_write_mode(void)
{
    return _vdi_compat.write_mode;
}

WORD v_updwk(WORD handle)
{
    if (!_vdi_valid_handle(handle)) {
        return 0;
    }

    _vdi_present_screen();
    return 1;
}

VOID vs_clip(WORD handle, WORD clip_flag, WORD xy[4])
{
    if (!_vdi_valid_handle(handle) || xy == NULL) {
        _vdi_compat.clip_enabled = 0;
        return;
    }

    _vdi_compat.clip_enabled = (clip_flag != 0) ? 1 : 0;
    memcpy(_vdi_compat.clip, xy, sizeof(_vdi_compat.clip));
}
