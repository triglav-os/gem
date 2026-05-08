/*
 * Implements the remaining miscellaneous GEM VDI compatibility entry
 * points such as palette selection, metafile stubs, and simple pixel
 * queries for the hosted port.
 *
 * MIT License (see: LICENSE)
 * Copyright (C) 2026 tomaz stih
 */

#include "gem/vdi.h"

#include "_internal.h"
#include "_state.h"

#include <stdio.h>
#include <string.h>

WORD v_bit_image(WORD handle, BYTE *filename, WORD aspect, WORD x_scale,
    WORD y_scale, WORD h_align, WORD v_align, WORD xy[4])
{
    FILE *stream;

    (void) aspect;
    (void) x_scale;
    (void) y_scale;
    (void) h_align;
    (void) v_align;
    (void) xy;

    if (!_vdi_valid_handle(handle) || filename == NULL) {
        return 0;
    }

    stream = fopen((const char *) filename, "rb");
    if (stream == NULL) {
        return 0;
    }
    fclose(stream);
    return 1;
}

WORD vs_palette(WORD handle, WORD palette)
{
    if (!_vdi_valid_handle(handle)) {
        return 0;
    }

    return palette;
}

WORD v_meta_extents(WORD handle, WORD min_x, WORD min_y, WORD max_x,
    WORD max_y)
{
    (void) min_x;
    (void) min_y;
    (void) max_x;
    (void) max_y;
    return _vdi_valid_handle(handle);
}

WORD v_write_meta(WORD handle, WORD num_intin, WORD *intin_values,
    WORD num_ptsin, WORD *ptsin_values)
{
    (void) num_intin;
    (void) intin_values;
    (void) num_ptsin;
    (void) ptsin_values;
    return _vdi_valid_handle(handle);
}

WORD vm_filename(WORD handle, BYTE *filename)
{
    size_t length;

    if (!_vdi_valid_handle(handle) || filename == NULL) {
        return 0;
    }

    length = strlen((const char *) filename);
    if (length >= sizeof(_vdi_compat.meta_filename)) {
        length = sizeof(_vdi_compat.meta_filename) - 1u;
    }
    memcpy(_vdi_compat.meta_filename, filename, length);
    _vdi_compat.meta_filename[length] = '\0';
    return 1;
}

WORD v_get_pixel(WORD handle, WORD x, WORD y, WORD *pel, WORD *index)
{
    WORD color;

    if (!_vdi_valid_handle(handle)) {
        return 0;
    }

    color = _vdi_get_screen_pixel(x, y);
    if (pel != NULL) {
        *pel = color;
    }
    if (index != NULL) {
        *index = color;
    }
    return 1;
}

WORD v_escape(WORD handle, WORD function, WORD count_in, WORD count_out,
    WORD *intin_values, WORD *ptsin_values)
{
    (void) function;
    (void) count_in;
    (void) count_out;
    (void) intin_values;
    (void) ptsin_values;
    return _vdi_valid_handle(handle);
}
