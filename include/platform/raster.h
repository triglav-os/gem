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
    GEM_RASTER_RGB565   = 3,
    GEM_RASTER_INDEX8   = 4,
    GEM_RASTER_MONO1    = 5
} gem_raster_format_t;

typedef struct gem_raster_surface {
    uint16_t            width;
    uint16_t            height;
    uint16_t            pitch;
    gem_raster_format_t format;
    void               *pixels;
} gem_raster_surface_t;

/*
 * Initializes the raster backend for the requested surface geometry and
 * pixel format.
 * Returns non-zero on success and zero on failure.
 */
int  gem_raster_init(uint16_t width, uint16_t height,
                     gem_raster_format_t format);

/*
 * Revalidates the framebuffer mapping after backend-side reconfiguration.
 * Returns non-zero on success and zero on failure.
 */
int  gem_raster_resync(void);

/*
 * Shuts down the raster backend and releases any owned surfaces.
 */
void gem_raster_shutdown(void);

/*
 * Returns the current drawing surface managed by the raster backend.
 * The returned pointer remains owned by the backend.
 */
gem_raster_surface_t *gem_raster_surface(void);

/*
 * Presents the current surface contents to the display output.
 */
void gem_raster_present(void);

/*
 * Updates one palette entry for indexed-color raster formats.
 * `index` selects the entry and `r`, `g`, and `b` provide its color.
 */
void gem_raster_set_palette(uint8_t index,
                            uint8_t r, uint8_t g, uint8_t b);

#ifdef __cplusplus
}
#endif

#endif
