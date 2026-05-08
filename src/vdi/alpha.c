/*
 * Implements GEM VDI alpha-cursor compatibility entry points. These
 * routines emulate the text-console style API on top of the hosted
 * workstation without mixing it into the main drawing modules.
 *
 * MIT License (see: LICENSE)
 * Copyright (C) 2026 tomaz stih
 */

#include "gem/vdi.h"

#include "_internal.h"
#include "_state.h"

#include <string.h>

WORD v_exit_cur(WORD handle)
{
    return _vdi_valid_handle(handle);
}

WORD v_enter_cur(WORD handle)
{
    return _vdi_valid_handle(handle);
}

WORD v_curup(WORD handle)
{
    if (!_vdi_valid_handle(handle)) {
        return 0;
    }

    if (_vdi_compat.cursor_row > 0) {
        --_vdi_compat.cursor_row;
    }
    return 1;
}

WORD v_curdown(WORD handle)
{
    if (!_vdi_valid_handle(handle)) {
        return 0;
    }

    ++_vdi_compat.cursor_row;
    return 1;
}

WORD v_curright(WORD handle)
{
    if (!_vdi_valid_handle(handle)) {
        return 0;
    }

    ++_vdi_compat.cursor_column;
    return 1;
}

WORD v_curleft(WORD handle)
{
    if (!_vdi_valid_handle(handle)) {
        return 0;
    }

    if (_vdi_compat.cursor_column > 0) {
        --_vdi_compat.cursor_column;
    }
    return 1;
}

WORD v_curhome(WORD handle)
{
    if (!_vdi_valid_handle(handle)) {
        return 0;
    }

    _vdi_compat.cursor_row = 0;
    _vdi_compat.cursor_column = 0;
    return 1;
}

WORD v_eeos(WORD handle)
{
    if (!_vdi_valid_handle(handle)) {
        return 0;
    }

    v_clrwk(handle);
    return 1;
}

WORD v_eeol(WORD handle)
{
    WORD y0;
    WORD y1;
    WORD x0;
    WORD x1;

    if (!_vdi_valid_handle(handle)) {
        return 0;
    }

    x0 = (WORD) (_vdi_compat.cursor_column * _vdi_font_cell_width());
    x1 = (WORD) (_vdi.width - 1);
    y0 = (WORD) (_vdi_compat.cursor_row * _vdi_font_text_height());
    y1 = (WORD) (y0 + _vdi_font_text_height() - 1);
    _vdi_fill_box(x0, y0, x1, y1, 0);
    _vdi_present_screen();
    return 1;
}

WORD vs_curaddress(WORD handle, WORD row, WORD column)
{
    if (!_vdi_valid_handle(handle)) {
        return 0;
    }

    _vdi_compat.cursor_row = (row > 0) ? row - 1 : 0;
    _vdi_compat.cursor_column = (column > 0) ? column - 1 : 0;
    return 1;
}

WORD v_curtext(WORD handle, BYTE *string)
{
    WORD x;
    WORD y;

    if (!_vdi_valid_handle(handle) || string == NULL) {
        return 0;
    }

    x = (WORD) (_vdi_compat.cursor_column * _vdi_font_cell_width());
    y = (WORD) ((_vdi_compat.cursor_row + 1) * _vdi_font_text_height() - 1);
    v_gtext(handle, x, y, string);
    _vdi_compat.cursor_column = (WORD) (_vdi_compat.cursor_column +
        _vdi_string_length((const char *) string));
    return 1;
}

WORD v_rvon(WORD handle)
{
    if (!_vdi_valid_handle(handle)) {
        return 0;
    }

    _vdi_compat.reverse_video = 1;
    return 1;
}

WORD v_rvoff(WORD handle)
{
    if (!_vdi_valid_handle(handle)) {
        return 0;
    }

    _vdi_compat.reverse_video = 0;
    return 1;
}

WORD vq_curaddress(WORD handle, WORD *row, WORD *column)
{
    if (!_vdi_valid_handle(handle)) {
        return 0;
    }

    if (row != NULL) {
        *row = (WORD) (_vdi_compat.cursor_row + 1);
    }
    if (column != NULL) {
        *column = (WORD) (_vdi_compat.cursor_column + 1);
    }
    return 1;
}

WORD vq_tabstatus(WORD handle)
{
    return _vdi_valid_handle(handle);
}

WORD v_hardcopy(WORD handle)
{
    return _vdi_valid_handle(handle);
}

WORD v_dspcur(WORD handle, WORD x, WORD y)
{
    if (!_vdi_valid_handle(handle)) {
        return 0;
    }

    _vdi_compat.cursor_column = (WORD) (x / _vdi_font_cell_width());
    _vdi_compat.cursor_row = (WORD) (y / _vdi_font_text_height());
    return 1;
}

WORD v_rmcur(WORD handle)
{
    return _vdi_valid_handle(handle);
}

WORD v_form_adv(WORD handle)
{
    return _vdi_valid_handle(handle);
}

WORD v_output_window(WORD handle, WORD xy[4])
{
    if (!_vdi_valid_handle(handle) || xy == NULL) {
        return 0;
    }

    memcpy(_vdi_compat.output_window, xy, sizeof(_vdi_compat.output_window));
    memcpy(_vdi_compat.clip, xy, sizeof(_vdi_compat.clip));
    _vdi_compat.clip_enabled = 1;
    return 1;
}

WORD v_clear_disp_list(WORD handle)
{
    return _vdi_valid_handle(handle);
}
