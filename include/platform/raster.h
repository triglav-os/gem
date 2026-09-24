/*
 * Declares the raster output abstraction used by GEM to manage a
 * framebuffer-like drawing surface, present finished frames, and update
 * palette entries on indexed targets.
 *
 * MIT License (see: LICENSE)
 * Copyright (C) 2026 tomaz stih
 */

#ifndef GEM_RASTER_H
#define GEM_RASTER_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum gem_raster_format {
    GEM_RASTER_XRGB8888 = 1,
    GEM_RASTER_ARGB8888 = 2,
    GEM_RASTER_RGB565 = 3,
    GEM_RASTER_INDEX8 = 4,
    GEM_RASTER_MONO1 = 5
} gem_raster_format_t;

typedef struct gem_raster_surface {
    uint16_t width;
    uint16_t height;
    uint16_t pitch;
    gem_raster_format_t format;
    void *pixels;
} gem_raster_surface_t;

/*
 * Initializes the raster backend for the requested surface geometry and
 * pixel format.
 * Returns non-zero on success and zero on failure.
 */
int gem_raster_init(uint16_t width, uint16_t height,
                    gem_raster_format_t format);

/*
 * Revalidates the framebuffer mapping after backend-side reconfiguration.
 * Returns non-zero on success and zero on failure.
 */
int gem_raster_resync(void);

/*
 * Shuts down the raster backend and releases any owned surfaces.
 */
void gem_raster_shutdown(void);

/*
 * Returns the current drawing surface managed by the raster backend. A
 * backend may use a shadow format for drawing and convert it at present
 * time; the returned pointer remains owned by the backend.
 */
gem_raster_surface_t *gem_raster_surface(void);

/*
 * Presents the current surface contents to the display output.
 */
void gem_raster_present(void);

/*
 * Blanks the drawing surface and the published output. Used on shutdown so
 * the viewer shows a cleared screen instead of the last rendered frame.
 */
void gem_raster_clear(void);

/*
 * Presents only the rectangle (x, y, width, height) in surface coordinates.
 * Used for cheap mouse-cursor updates; out-of-range edges are clamped.
 */
void gem_raster_present_rect(int x, int y, int width, int height);

/*
 * Keeps the software GEM pointer above presenter-only color overlays. The
 * mask uses the standard 16-row GEM cursor form and refers to pixels already
 * drawn in the monochrome shadow surface. Pass visible=0 to disable it.
 */
void gem_raster_cursor_update(int x, int y, const uint16_t mask[16],
                              int visible);

/*
 * Presenter-only BGRA overlays.  GEM/VDI never draw into these buffers: the
 * server copies application pixels here and the selected backend composites
 * them over its monochrome shadow at present time.
 */
int gem_raster_overlay_create(uint32_t id, int x, int y, uint16_t width,
                              uint16_t height);
/* Resizes and repositions a presenter overlay. */
int gem_raster_overlay_resize(uint32_t id, int x, int y, uint16_t width,
                              uint16_t height);
/* Copies a BGRA rectangle into an existing presenter overlay. */
int gem_raster_overlay_update(uint32_t id, int x, int y, uint16_t width,
                              uint16_t height, const uint8_t *bgra,
                              uint16_t pitch);
/* Destroys one presenter overlay by id. */
void gem_raster_overlay_destroy(uint32_t id);
/* Changes overlay visibility without destroying its pixels. */
void gem_raster_overlay_set_visible(uint32_t id, int visible);
/* Sets screen rectangles that temporarily occlude an overlay. */
void gem_raster_overlay_set_occlusion(uint32_t id, const int *rects,
                                      unsigned count);
/* Destroys every presenter overlay owned by the backend. */
void gem_raster_overlay_clear(void);

/*
 * Updates one palette entry for indexed-color raster formats.
 * `index` selects the entry and `r`, `g`, and `b` provide its color.
 */
void gem_raster_set_palette(uint8_t index, uint8_t r, uint8_t g, uint8_t b);

#ifdef __cplusplus
}
#endif

#endif
