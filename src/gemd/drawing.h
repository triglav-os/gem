/*
 * Private per-connection VDI attributes, independent of shared screen state.
 *
 * MIT License (see: LICENSE)
 * Copyright (C) 2026 tomaz stih
 */
#ifndef GEMD_DRAWING_H
#define GEMD_DRAWING_H
#include "../vdi/vdi_state.h"
#include "../gem/gem_protocol.h"

typedef struct gemd_drawing {
    vdi_compat_state_t attributes;
    WORD line, fill, text;
    int initialized;
} gemd_drawing_t;

/* Save/restore attributes, never cursor, damage, framebuffer or update state.
 */
void gemd_drawing_save(gemd_drawing_t *state);
void gemd_drawing_restore(const gemd_drawing_t *state);
/* Initialize independent default attributes without resetting other clients. */
void gemd_drawing_init(gemd_drawing_t *state);
/* Identify attribute/query calls and raster-writing calls respectively. */
int gemd_vdi_request(uint16_t opcode);
int gemd_raster_request(uint16_t opcode);
#endif
