/*
 * Isolates clients' pens, fonts, clipping and write modes from each other
 * and from AES chrome, while retaining the one physical GEM workstation.
 *
 * MIT License (see: LICENSE)
 * Copyright (C) 2026 tomaz stih
 */
#include "drawing.h"
#include "gem/gemd.h"

void gemd_drawing_save(gemd_drawing_t *state)
{
    state->attributes = vdi_compat;
    state->line = vdi_state.line_color;
    state->fill = vdi_state.fill_color;
    state->text = vdi_state.text_color;
    state->initialized = 1;
}

void gemd_drawing_restore(const gemd_drawing_t *state)
{
    vdi_compat = state->attributes;
    vdi_state.line_color = state->line;
    vdi_state.fill_color = state->fill;
    vdi_state.text_color = state->text;
    (void)vdi_select_font(vdi_compat.text_font);
}

void gemd_drawing_init(gemd_drawing_t *state)
{
    gemd_drawing_t previous;
    gemd_drawing_save(&previous);
    vdi_reset_workstation_state();
    vdi_state.line_color = vdi_state.fill_color = vdi_state.text_color =
        vdi_color_to_pixel(BLACK);
    gemd_drawing_save(state);
    gemd_drawing_restore(&previous);
}

int gemd_vdi_request(uint16_t opcode)
{
    return opcode == GEM_RPC_VDI_EXT || opcode == GEM_RPC_BITMAP_PUT ||
           opcode == GEM_RPC_BITMAP_GET || opcode == GEM_RPC_BITMAP_COPY ||
           (opcode >= GEM_RPC_V_CLRWK && opcode <= GEM_RPC_V_GTEXT) ||
           opcode == GEM_RPC_VQT_EXTENT || opcode == GEM_RPC_VQT_FONTINFO;
}

int gemd_raster_request(uint16_t opcode)
{
    return opcode == GEM_RPC_V_CLRWK || opcode == GEM_RPC_V_PLINE ||
           opcode == GEM_RPC_V_FILLAREA || opcode == GEM_RPC_V_BAR ||
           opcode == GEM_RPC_VR_RECFL || opcode == GEM_RPC_V_GTEXT;
}
