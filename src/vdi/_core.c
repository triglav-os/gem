/*
 * Implements the private hosted GEM VDI core state that remains shared
 * across the split helper modules. This file owns the global VDI
 * workstation state, update/present flow, environment parsing, and the
 * standard `work_out` capability block.
 *
 * MIT License (see: LICENSE)
 * Copyright (C) 2026 tomaz stih
 */

#include "_internal.h"

#include "_state.h"

#include "platform/raster.h"

#include <stdlib.h>
#include <string.h>

vdi_state_t _vdi;

WORD _vdi_parse_env_word(const char *name, WORD fallback)
{
    const char *value = getenv(name);
    char *end = NULL;
    long parsed;

    if (value == NULL || value[0] == '\0') {
        return fallback;
    }

    parsed = strtol(value, &end, 10);
    if (end == value || *end != '\0' || parsed <= 0 || parsed > 32767L) {
        return fallback;
    }

    return (WORD) parsed;
}

void _vdi_pump_events(void)
{
}

void _vdi_begin_update(void)
{
    ++_vdi.update_depth;
}

void _vdi_end_update(void)
{
    if (_vdi.update_depth <= 0) {
        return;
    }

    --_vdi.update_depth;
    if (_vdi.update_depth == 0 && _vdi.present_pending != 0) {
        _vdi.present_pending = 0;
        _vdi_present_screen();
    }
    _vdi_pump_events();
}

void _vdi_fill_work_out(WORD work_out[57])
{
    WORD cell_width = _vdi_font_cell_width();
    WORD text_height = _vdi_font_text_height();

    memset(work_out, 0, sizeof(WORD) * 57u);
    work_out[0] = (WORD) (_vdi.width - 1);
    work_out[1] = (WORD) (_vdi.height - 1);
    work_out[2] = 1;
    work_out[3] = 1;
    work_out[4] = 1;
    work_out[5] = 1;
    work_out[6] = 1;
    work_out[7] = 1;
    work_out[8] = 1;
    work_out[9] = 1;
    work_out[10] = 1;
    work_out[11] = 1;
    work_out[12] = 1;
    work_out[13] = 2;
    work_out[35] = 1;
    work_out[39] = 1;
    work_out[40] = 1;
    work_out[41] = 1;
    work_out[42] = 1;
    work_out[43] = 1;
    work_out[44] = 1;
    work_out[45] = cell_width;
    work_out[46] = text_height;
    work_out[47] = cell_width;
    work_out[48] = text_height;
    work_out[49] = 1;
    work_out[51] = 1;
    work_out[54] = 1;
    work_out[56] = 1;
}
