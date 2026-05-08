/*
 * Implements GEM VDI text, font, and attribute entry points for the
 * hosted port. This keeps text shaping, font selection, and attribute
 * queries separate from raster and primitive drawing code.
 *
 * MIT License (see: LICENSE)
 * Copyright (C) 2026 tomaz stih
 */

#include "gem/vdi.h"

#include "_internal.h"
#include "_state.h"

#include <string.h>

VOID v_justified(WORD handle, WORD x, WORD y, char *string, WORD length,
    WORD wordspace, WORD charspace)
{
    BYTE buffer[256];
    size_t copy_len;

    (void) wordspace;
    (void) charspace;

    if (!_vdi_valid_handle(handle) || string == NULL) {
        return;
    }

    copy_len = strlen(string);
    if (length >= 0 && (size_t) length < copy_len) {
        copy_len = (size_t) length;
    }
    if (copy_len >= sizeof(buffer)) {
        copy_len = sizeof(buffer) - 1u;
    }
    memcpy(buffer, string, copy_len);
    buffer[copy_len] = '\0';
    v_gtext(handle, x, y, buffer);
}

WORD vst_height(WORD handle, WORD height, WORD *charw, WORD *charh,
    WORD *cellw, WORD *cellh)
{
    if (!_vdi_valid_handle(handle)) {
        return 0;
    }

    _vdi_compat.text_height = (height > 0) ? height : _vdi_font_text_height();
    if (charw != NULL) {
        *charw = _vdi_font_cell_width();
    }
    if (charh != NULL) {
        *charh = _vdi_compat.text_height;
    }
    if (cellw != NULL) {
        *cellw = _vdi_font_cell_width();
    }
    if (cellh != NULL) {
        *cellh = _vdi_compat.text_height;
    }
    return _vdi_compat.text_height;
}

WORD vst_rotation(WORD handle, WORD angle)
{
    if (!_vdi_valid_handle(handle)) {
        return 0;
    }

    _vdi_compat.text_rotation = angle;
    return angle;
}

WORD vs_color(WORD handle, WORD index, WORD rgb[3])
{
    if (!_vdi_valid_handle(handle)) {
        return 0;
    }

    _vdi_store_rgb(index, rgb);
    return 1;
}

WORD vsl_type(WORD handle, WORD style)
{
    if (!_vdi_valid_handle(handle)) {
        return 0;
    }

    _vdi_compat.line_style = style;
    return style;
}

WORD vsl_width(WORD handle, WORD width)
{
    if (!_vdi_valid_handle(handle)) {
        return 0;
    }

    _vdi_compat.line_width = (width > 0) ? width : 1;
    return _vdi_compat.line_width;
}

WORD vsm_type(WORD handle, WORD symbol)
{
    if (!_vdi_valid_handle(handle)) {
        return 0;
    }

    _vdi_compat.marker_type = symbol;
    return symbol;
}

WORD vsm_height(WORD handle, WORD height)
{
    if (!_vdi_valid_handle(handle)) {
        return 0;
    }

    _vdi_compat.marker_height = (height > 0) ? height :
        _vdi_font_text_height();
    return _vdi_compat.marker_height;
}

WORD vsm_color(WORD handle, WORD color_index)
{
    if (!_vdi_valid_handle(handle)) {
        return 0;
    }

    _vdi_compat.marker_color = _vdi_color_to_pixel(color_index);
    return color_index;
}

WORD vst_font(WORD handle, WORD font)
{
    WORD selected;

    if (!_vdi_valid_handle(handle)) {
        return 0;
    }

    selected = _vdi_select_font(font);
    if (selected == 0) {
        return 0;
    }
    _vdi_compat.text_font = font;
    return font;
}

WORD vsf_interior(WORD handle, WORD style)
{
    if (!_vdi_valid_handle(handle)) {
        return 0;
    }

    _vdi_compat.fill_interior = style;
    return style;
}

WORD vsf_style(WORD handle, WORD style)
{
    if (!_vdi_valid_handle(handle)) {
        return 0;
    }

    _vdi_compat.fill_style = style;
    return style;
}

WORD vq_color(WORD handle, WORD index, WORD setflag, WORD rgb[])
{
    (void) setflag;

    if (!_vdi_valid_handle(handle)) {
        return 0;
    }

    _vdi_copy_rgb(index, rgb);
    return 1;
}

WORD vswr_mode(WORD handle, WORD mode)
{
    if (!_vdi_valid_handle(handle)) {
        return 0;
    }

    _vdi_compat.write_mode = mode;
    return mode;
}

WORD vql_attributes(WORD handle, WORD attrib[])
{
    if (!_vdi_valid_handle(handle) || attrib == NULL) {
        return 0;
    }

    attrib[0] = _vdi_compat.line_style;
    attrib[1] = _vdi.line_color;
    attrib[2] = _vdi_compat.write_mode;
    attrib[3] = _vdi_compat.line_width;
    return 1;
}

WORD vqm_attributes(WORD handle, WORD attrib[])
{
    if (!_vdi_valid_handle(handle) || attrib == NULL) {
        return 0;
    }

    attrib[0] = _vdi_compat.marker_type;
    attrib[1] = _vdi_compat.marker_color;
    attrib[2] = _vdi_compat.write_mode;
    attrib[3] = _vdi_compat.marker_height;
    return 1;
}

WORD vqf_attributes(WORD handle, WORD attrib[])
{
    if (!_vdi_valid_handle(handle) || attrib == NULL) {
        return 0;
    }

    attrib[0] = _vdi_compat.fill_interior;
    attrib[1] = _vdi.fill_color;
    attrib[2] = _vdi_compat.fill_style;
    attrib[3] = _vdi_compat.fill_perimeter;
    return 1;
}

WORD vqt_attributes(WORD handle, WORD attrib[])
{
    if (!_vdi_valid_handle(handle) || attrib == NULL) {
        return 0;
    }

    attrib[0] = _vdi_compat.text_font;
    attrib[1] = _vdi.text_color;
    attrib[2] = _vdi_compat.text_rotation;
    attrib[3] = _vdi_compat.text_height;
    attrib[4] = _vdi_compat.text_alignment_h;
    attrib[5] = _vdi_compat.text_alignment_v;
    return 1;
}

WORD vst_alignment(WORD handle, WORD horiz, WORD vert)
{
    if (!_vdi_valid_handle(handle)) {
        return 0;
    }

    _vdi_compat.text_alignment_h = horiz;
    _vdi_compat.text_alignment_v = vert;
    return 1;
}

WORD vq_extnd(WORD handle, WORD owflag, WORD work_out[])
{
    (void) owflag;

    if (!_vdi_valid_handle(handle) || work_out == NULL) {
        return 0;
    }

    _vdi_fill_work_out(work_out);
    return 1;
}

WORD vsf_perimeter(WORD handle, WORD per_vis)
{
    if (!_vdi_valid_handle(handle)) {
        return 0;
    }

    _vdi_compat.fill_perimeter = per_vis;
    return per_vis;
}

WORD vst_background(WORD handle, WORD mode)
{
    if (!_vdi_valid_handle(handle)) {
        return 0;
    }

    _vdi_compat.text_background = mode;
    return mode;
}

WORD vst_effects(WORD handle, WORD effect)
{
    if (!_vdi_valid_handle(handle)) {
        return 0;
    }

    _vdi_compat.text_effects = effect;
    return effect;
}

WORD vst_point(WORD handle, WORD point, WORD *charw, WORD *charh,
    WORD *cellw, WORD *cellh)
{
    return vst_height(handle, point, charw, charh, cellw, cellh);
}

WORD vsl_ends(WORD handle, WORD begstyle, WORD endstyle)
{
    if (!_vdi_valid_handle(handle)) {
        return 0;
    }

    _vdi_compat.line_beg_style = begstyle;
    _vdi_compat.line_end_style = endstyle;
    return 1;
}

WORD vsl_end_style(WORD handle, WORD begstyle, WORD endstyle)
{
    return vsl_ends(handle, begstyle, endstyle);
}

WORD vsf_udpat(WORD handle, WORD *fill_pat, WORD planes)
{
    size_t count = sizeof(_vdi_compat.fill_pattern) /
        sizeof(_vdi_compat.fill_pattern[0]);

    (void) planes;

    if (!_vdi_valid_handle(handle) || fill_pat == NULL) {
        return 0;
    }

    memcpy(_vdi_compat.fill_pattern, fill_pat, sizeof(WORD) * count);
    return 1;
}

WORD vsl_udsty(WORD handle, WORD pattern)
{
    if (!_vdi_valid_handle(handle)) {
        return 0;
    }

    _vdi_compat.line_pattern = pattern;
    return pattern;
}

WORD vqt_extent(WORD handle, char *string, WORD extent[8])
{
    if (!_vdi_valid_handle(handle) || extent == NULL) {
        return 0;
    }

    _vdi_set_extent_for_string(string, extent);
    return 1;
}

WORD vqt_width(WORD handle, WORD character, WORD *cell_width,
    WORD *left_delta, WORD *right_delta)
{
    WORD width;

    if (!_vdi_valid_handle(handle)) {
        return 0;
    }

    width = _vdi_char_cell_width((char) character);

    if (cell_width != NULL) {
        *cell_width = width;
    }
    if (left_delta != NULL) {
        *left_delta = 0;
    }
    if (right_delta != NULL) {
        *right_delta = 0;
    }
    return width;
}

WORD vst_load_fonts(WORD handle, WORD select)
{
    if (!_vdi_valid_handle(handle)) {
        return 0;
    }

    (void) select;
    return _vdi_load_external_fonts();
}

WORD vst_unload_fonts(WORD handle, WORD select)
{
    if (!_vdi_valid_handle(handle)) {
        return 0;
    }

    (void) select;
    _vdi_compat.text_font = _vdi_default_font_id();
    return _vdi_unload_external_fonts();
}

WORD vqt_name(WORD handle, WORD element_num, BYTE *name)
{
    const char *font_name;
    WORD font_id;

    if (!_vdi_valid_handle(handle) || name == NULL) {
        return 0;
    }

    font_id = _vdi_font_id_for_element(element_num);
    font_name = _vdi_font_name(element_num);
    if (font_id == 0 || font_name == NULL) {
        return 0;
    }
    strcpy((char *) name, font_name);
    return font_id;
}

WORD vqt_fontinfo(WORD handle, WORD *min_ade, WORD *max_ade,
    WORD distances[], WORD *max_width, WORD effects[])
{
    WORD text_height = _vdi_font_text_height();

    if (!_vdi_valid_handle(handle)) {
        return 0;
    }

    if (min_ade != NULL) {
        *min_ade = _vdi_font_first_ade();
    }
    if (max_ade != NULL) {
        *max_ade = _vdi_font_last_ade();
    }
    if (distances != NULL) {
        distances[0] = 0;
        distances[1] = (WORD) (_vdi_font_ascent() - 2);
        distances[2] = 2;
        distances[3] = text_height;
        distances[4] = text_height;
    }
    if (max_width != NULL) {
        *max_width = _vdi_font_cell_width();
    }
    if (effects != NULL) {
        effects[0] = 0;
        effects[1] = 0;
        effects[2] = 0;
    }
    return 1;
}

WORD vq_chcells(WORD handle, WORD *rows, WORD *columns)
{
    if (!_vdi_valid_handle(handle)) {
        return 0;
    }

    if (rows != NULL) {
        *rows = (WORD) (_vdi.height / _vdi_font_text_height());
    }
    if (columns != NULL) {
        *columns = (WORD) (_vdi.width / _vdi_font_cell_width());
    }
    return 1;
}
