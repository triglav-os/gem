/*
 * Isolates clients' pens, fonts, clipping and write modes from each other
 * and from AES chrome, while retaining the one physical GEM workstation.
 *
 * MIT License (see: LICENSE)
 * Copyright (C) 2026 tomaz stih
 */
#include "drawing.h"

void gemd_drawing_save(gemd_drawing_t *state)
{
    state->attributes = _vdi_compat;
    state->line = _vdi.line_color;
    state->fill = _vdi.fill_color;
    state->text = _vdi.text_color;
    state->initialized = 1;
}

void gemd_drawing_restore(const gemd_drawing_t *state)
{
    _vdi_compat = state->attributes;
    _vdi.line_color = state->line;
    _vdi.fill_color = state->fill;
    _vdi.text_color = state->text;
    (void) _vdi_select_font(_vdi_compat.text_font);
}

void gemd_drawing_init(gemd_drawing_t *state)
{
    gemd_drawing_t previous;
    gemd_drawing_save(&previous);
    _vdi_reset_workstation_state();
    _vdi.line_color = _vdi.fill_color = _vdi.text_color = _vdi_color_to_pixel(BLACK);
    gemd_drawing_save(state);
    gemd_drawing_restore(&previous);
}

int gemd_vdi_request(uint16_t opcode)
{
    return (opcode >= GEM_RPC_V_CLRWK && opcode <= GEM_RPC_V_GTEXT) ||
        opcode == GEM_RPC_VQT_EXTENT || opcode == GEM_RPC_VQT_FONTINFO;
}

int gemd_raster_request(uint16_t opcode)
{
    return opcode == GEM_RPC_V_CLRWK || opcode == GEM_RPC_V_PLINE ||
        opcode == GEM_RPC_V_FILLAREA || opcode == GEM_RPC_V_BAR ||
        opcode == GEM_RPC_VR_RECFL || opcode == GEM_RPC_V_GTEXT;
}
