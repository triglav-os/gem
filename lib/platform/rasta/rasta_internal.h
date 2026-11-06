/*
 * Declares the environment-derived settings and key translation shared
 * by the Rasta raster and input backends. Private to the rasta library.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (C) 2026 tomaz stih
 */

#ifndef GEM_PLATFORM_RASTA_INTERNAL_H
#define GEM_PLATFORM_RASTA_INTERNAL_H

#include <stdint.h>

/* GEM keyboard modifier bits reported with key events. */
enum {
    rasta_mod_rshift = 0x0001,
    rasta_mod_lshift = 0x0002,
    rasta_mod_ctrl = 0x0004,
    rasta_mod_alt = 0x0008
};

/* Shared framebuffer file path (GEM_RASTA_FRAMEBUFFER, RASTA_FRAMEBUFFER). */
const char *rasta_framebuffer_path(void);
/* Viewer host address, default loopback. */
const char *rasta_host(void);
/* Viewer input port, default 5000. */
uint16_t rasta_port(void);
/* Requested window scale, default 1. */
uint16_t rasta_scale(void);
/* "on" or "off" for the viewer cursor. */
const char *rasta_cursor_mode(void);
/* "on" or "off" for inverse display polarity. */
const char *rasta_inverse_mode(void);
/* GEM modifier bit for a modifier usage code, or 0. */
uint16_t rasta_modifier_mask(uint16_t key);
/* Compose the GEM key word (scan << 8 | ascii) for a usage code. */
uint16_t rasta_key_to_gem(uint16_t key, uint16_t mods);

#endif
