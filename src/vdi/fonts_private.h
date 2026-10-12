/*
 * Declares the loaded-font record and the accessors shared by the hosted
 * VDI font loader and its glyph renderer. Private to libvdi.
 *
 * MIT License (see: LICENSE)
 * Copyright (C) 2026 tomaz stih
 */

#ifndef GEM_PRIVATE_FONTS_H
#define GEM_PRIVATE_FONTS_H

#include "vdi_internal.h"

#include "vdi_state.h"

enum {
    vdi_max_fonts = 16,
    vdi_font_id_system = 1,
    vdi_font_id_small_internal = 2,
    vdi_font_id_ibm = 3,
    vdi_font_id_small = 5,
    /* Private selectors used by Stout's translated Atari TEDINFOs. */
    vdi_font_id_ibm_compat = -3,
    vdi_font_id_small_compat = -5
};

typedef struct vdi_font {
    WORD font_id;
    char name[33];
    char file_name[64];
    WORD first_ade;
    WORD last_ade;
    WORD top;
    WORD ascent;
    WORD half;
    WORD descent;
    WORD bottom;
    WORD max_char_width;
    WORD max_cell_width;
    WORD form_width;
    WORD form_height;
    uint32_t data_offset;
    uint32_t off_offset;
    uint8_t *data;
    size_t data_size;
    WORD uniform_width;
    int present;
    int resident;
} vdi_font_t;

/* Read a little-endian 16-bit value from a font file image. */
uint16_t vdi_font_read_le16(const uint8_t *bytes);
/* The font selected for text output, or NULL before any font loads. */
vdi_font_t *vdi_current_font(void);
/* Non-zero while Stout's compact compatibility selector is active. */
int vdi_small_font_metrics_active(void);

#endif
