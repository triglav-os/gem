/*
 * Presenter-only BGRA overlay API. Overlay pixels are copied to gemd and
 * composited by the display backend; they are not part of GEM/VDI memory.
 *
 * MIT License (see: LICENSE)
 * Copyright (C) 2026 tomaz stih
 */
#ifndef GEM_OVERLAY_H
#define GEM_OVERLAY_H

#include "gem/aes.h"

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef uint32_t gem_overlay_id_t;

/* Creates a presenter BGRA layer attached to a GEM window. */
gem_overlay_id_t gem_overlay_create(WORD window_handle, uint16_t width,
                                     uint16_t height);
/* Resizes an existing layer, clearing its copied pixels. */
int gem_overlay_resize(gem_overlay_id_t id, uint16_t width, uint16_t height);
/* Maps a layer to the current work origin of a GEM window. */
int gem_overlay_map(gem_overlay_id_t id, WORD window_handle);
/* Hides and detaches a layer without destroying its storage. */
int gem_overlay_unmap(gem_overlay_id_t id);
/* Destroys a layer and releases its server-side pixel storage. */
int gem_overlay_destroy(gem_overlay_id_t id);
/* Copies a BGRA rectangle into a layer and presents the changed region. */
int gem_overlay_update(gem_overlay_id_t id, uint16_t x, uint16_t y,
                       uint16_t width, uint16_t height,
                       const uint8_t *bgra, uint16_t pitch);

#ifdef __cplusplus
}
#endif

#endif
