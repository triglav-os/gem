/*
 * Implements the GEM raster abstraction for the rasta emulator. The
 * backend stages VDI drawing in a packed shadow surface and publishes
 * completed updates to rasta's mapped 32-bit BGRA framebuffer. VDI continues
 * to draw into a packed monochrome shadow; the viewer never observes an
 * intermediate clear while a window is being repainted.
 *
 * MIT License (see: LICENSE)
 * Copyright (C) 2026 tomaz stih
 */

#define _POSIX_C_SOURCE 200809L

#include "rasta_internal.h"

#include <stdio.h>

#include "platform/raster.h"

#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

enum {
    rasta_bits_per_pixel = 32,
    rasta_bytes_per_pixel = rasta_bits_per_pixel / 8,
    rasta_overlay_max = 8,
    rasta_overlay_limit = 8 * 1024 * 1024
    ,rasta_overlay_occlusion_max = 16
};

typedef struct rasta_overlay {
    uint32_t id;
    int x;
    int y;
    uint16_t width;
    uint16_t height;
    int visible;
    int occlusion[rasta_overlay_occlusion_max][4];
    unsigned occlusion_count;
    uint8_t *pixels;
} rasta_overlay_t;

static gem_raster_surface_t g_surface;
static uint8_t *g_present_pixels;
static int g_framebuffer_fd = -1;
static size_t g_framebuffer_size;
static size_t g_shadow_size;
static dev_t g_framebuffer_dev;
static ino_t g_framebuffer_ino;
static rasta_overlay_t g_overlays[rasta_overlay_max];
static unsigned g_overlay_count;
static int g_cursor_x;
static int g_cursor_y;
static uint16_t g_cursor_mask[16];
static int g_cursor_visible;

static size_t rasta_row_bytes(uint16_t width);
static size_t rasta_file_size(uint16_t width, uint16_t height);
static int ensure_framebuffer_target(void);
static void publish_rect(int x, int y, int width, int height);

static int overlay_occluded(const rasta_overlay_t *overlay, int x, int y)
{
    for (unsigned i = 0; i < overlay->occlusion_count; ++i) {
        const int *r = overlay->occlusion[i];
        if (x >= r[0] && y >= r[1] && x < r[0] + r[2] &&
            y < r[1] + r[3])
            return 1;
    }
    return 0;
}

static rasta_overlay_t *find_overlay(uint32_t id)
{
    for (size_t i = 0; i < rasta_overlay_max; ++i)
        if (g_overlays[i].pixels != NULL && g_overlays[i].id == id)
            return &g_overlays[i];
    return NULL;
}

static void blend_overlay_pixel(uint8_t *dst, const uint8_t *src)
{
    unsigned alpha = src[3];

    if (alpha == 0u)
        return;
    if (alpha == 255u) {
        memcpy(dst, src, 4u);
        return;
    }
    for (unsigned channel = 0; channel < 3u; ++channel)
        dst[channel] = (uint8_t)((src[channel] * alpha +
                                  dst[channel] * (255u - alpha) + 127u) /
                                 255u);
    dst[3] = 255u;
}

static int record_framebuffer_identity(void)
{
    struct stat st;

    if (g_framebuffer_fd < 0) {
        errno = EBADF;
        return 0;
    }

    if (fstat(g_framebuffer_fd, &st) != 0) {
        return 0;
    }

    g_framebuffer_dev = st.st_dev;
    g_framebuffer_ino = st.st_ino;
    return 1;
}

static int map_framebuffer(size_t size)
{
    void *mapping;

    if (g_present_pixels != NULL) {
        if (munmap(g_present_pixels, g_framebuffer_size) != 0) {
            return 0;
        }
        g_present_pixels = NULL;
    }

    mapping = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED,
                   g_framebuffer_fd, 0);
    if (mapping == MAP_FAILED) {
        return 0;
    }

    g_present_pixels = mapping;
    if (g_surface.pixels == NULL) {
        g_surface.pixels = calloc(g_shadow_size, 1);
        if (g_surface.pixels == NULL) {
            munmap(mapping, size);
            g_present_pixels = NULL;
            return 0;
        }
    }
    g_framebuffer_size = size;
    /* A new viewer mapping needs the whole frame, even on a partial present. */
    publish_rect(0, 0, (int)g_surface.width, (int)g_surface.height);
    return 1;
}

/*
 * Rasta may recreate the framebuffer file when the subscriber requests a
 * new geometry. Refresh the target file descriptor before we present so
 * GEM does not keep writing into an orphaned file after rasta switches
 * to a new inode.
 */
static int reopen_framebuffer_target(size_t size)
{
    const char *path = rasta_framebuffer_path();
    struct stat target;

    if (g_framebuffer_fd >= 0) {
        close(g_framebuffer_fd);
        g_framebuffer_fd = -1;
    }

    g_framebuffer_fd = open(
        path, O_RDWR | O_CREAT | O_NOFOLLOW | O_NONBLOCK | O_CLOEXEC, 0600);
    if (g_framebuffer_fd < 0) {
        return 0;
    }

    /* Validate the opened inode before truncating it, not the path before
     * open. In particular, reject symlinks, devices, FIFOs and hard links. */
    if (fstat(g_framebuffer_fd, &target) != 0 || !S_ISREG(target.st_mode) ||
        target.st_uid != geteuid() || target.st_nlink != 1 ||
        fchmod(g_framebuffer_fd, 0600) != 0 ||
        ftruncate(g_framebuffer_fd, (off_t)size) != 0) {
        close(g_framebuffer_fd);
        g_framebuffer_fd = -1;
        return 0;
    }

    if (!record_framebuffer_identity()) {
        close(g_framebuffer_fd);
        g_framebuffer_fd = -1;
        return 0;
    }
    if (!map_framebuffer(size)) {
        close(g_framebuffer_fd);
        g_framebuffer_fd = -1;
        return 0;
    }

    return 1;
}

static size_t rasta_row_bytes(uint16_t width)
{
    return ((size_t)width + 7u) / 8u;
}

static size_t rasta_file_size(uint16_t width, uint16_t height)
{
    return (size_t)width * rasta_bytes_per_pixel * (size_t)height;
}

static int open_framebuffer(uint16_t width, uint16_t height)
{
    size_t size = rasta_file_size(width, height);

    return reopen_framebuffer_target(size);
}

static void close_framebuffer(void)
{
    if (g_present_pixels != NULL)
        munmap(g_present_pixels, g_framebuffer_size);
    g_present_pixels = NULL;
    free(g_surface.pixels);
    g_surface.pixels = NULL;
    if (g_framebuffer_fd >= 0) {
        close(g_framebuffer_fd);
        g_framebuffer_fd = -1;
    }

    g_framebuffer_size = 0u;
    g_shadow_size = 0u;
    g_framebuffer_dev = 0;
    g_framebuffer_ino = 0;
}

static void clear_overlays(void)
{
    for (size_t i = 0; i < rasta_overlay_max; ++i) {
        free(g_overlays[i].pixels);
        memset(&g_overlays[i], 0, sizeof(g_overlays[i]));
    }
    g_overlay_count = 0u;
}

static int ensure_framebuffer_target(void)
{
    const char *path = rasta_framebuffer_path();
    struct stat path_st;
    struct stat fd_st;
    size_t expected_size;

    if (g_surface.width == 0u || g_surface.height == 0u) {
        errno = EINVAL;
        return 0;
    }

    expected_size = rasta_file_size(g_surface.width, g_surface.height);
    if (g_framebuffer_fd < 0) {
        return reopen_framebuffer_target(expected_size);
    }

    if (stat(path, &path_st) != 0) {
        return reopen_framebuffer_target(expected_size);
    }

    if (fstat(g_framebuffer_fd, &fd_st) != 0) {
        return reopen_framebuffer_target(expected_size);
    }

    if (path_st.st_dev != fd_st.st_dev || path_st.st_ino != fd_st.st_ino ||
        fd_st.st_dev != g_framebuffer_dev ||
        fd_st.st_ino != g_framebuffer_ino) {
        return reopen_framebuffer_target(expected_size);
    }

    if ((size_t)fd_st.st_size != expected_size) {
        if (ftruncate(g_framebuffer_fd, (off_t)expected_size) != 0) {
            return 0;
        }
        if (!record_framebuffer_identity()) {
            return 0;
        }
        publish_rect(0, 0, (int)g_surface.width, (int)g_surface.height);
    }

    return 1;
}

int gem_raster_init(uint16_t width, uint16_t height, gem_raster_format_t format)
{
    size_t pitch;

    if (width == 0u || height == 0u || format != GEM_RASTER_MONO1) {
        errno = EINVAL;
        return 0;
    }

    pitch = rasta_row_bytes(width);
    if (pitch > 0xffffu) {
        errno = EOVERFLOW;
        return 0;
    }

    close_framebuffer();
    clear_overlays();
    g_cursor_visible = 0;
    memset(&g_surface, 0, sizeof(g_surface));
    g_surface.width = width;
    g_surface.height = height;
    g_surface.pitch = (uint16_t)pitch;
    g_surface.format = format;
    g_surface.pixels = NULL;
    g_shadow_size = pitch * (size_t)height;
    return open_framebuffer(width, height);
}

int gem_raster_resync(void)
{
    if (g_surface.width == 0u || g_surface.height == 0u) {
        errno = EINVAL;
        return 0;
    }

    if (g_framebuffer_fd < 0 &&
        !open_framebuffer(g_surface.width, g_surface.height)) {
        return 0;
    }

    return ensure_framebuffer_target();
}

void gem_raster_shutdown(void)
{
    close_framebuffer();
    clear_overlays();
    g_cursor_visible = 0;
    memset(&g_surface, 0, sizeof(g_surface));
}

gem_raster_surface_t *gem_raster_surface(void)
{
    if (g_surface.pixels == NULL) {
        return NULL;
    }

    return &g_surface;
}

void gem_raster_present(void)
{
    if (g_surface.pixels == NULL) {
        return;
    }

    if (!ensure_framebuffer_target()) {
        return;
    }
    publish_rect(0, 0, (int)g_surface.width, (int)g_surface.height);
}

void gem_raster_clear(void)
{
    if (g_framebuffer_size == 0u) {
        return;
    }
    if (g_surface.pixels != NULL)
        memset(g_surface.pixels, 0, g_shadow_size);
    if (g_present_pixels != NULL)
        publish_rect(0, 0, (int)g_surface.width, (int)g_surface.height);
}

/* Expands the monochrome shadow into Rasta's opaque BGRA8888 format. */
static void publish_rect(int x, int y, int width, int height)
{
    int row;
    int64_t right = (int64_t)x + width;
    int64_t bottom = (int64_t)y + height;
    size_t source_offset;
    uint8_t *destination;
    if (!g_surface.pixels || width <= 0 || height <= 0 ||
        !ensure_framebuffer_target())
        return;
    if (x < 0)
        x = 0;
    if (y < 0)
        y = 0;
    if (right > g_surface.width)
        right = g_surface.width;
    if (bottom > g_surface.height)
        bottom = g_surface.height;
    if (right <= x || bottom <= y)
        return;
    for (row = y; row < bottom; ++row) {
        const uint8_t *source = (const uint8_t *)g_surface.pixels +
                                (size_t)row * g_surface.pitch;
        destination = g_present_pixels +
                      ((size_t)row * g_surface.width + (size_t)x) *
                          rasta_bytes_per_pixel;
        source_offset = (size_t)x / 8u;
        for (int column = x; column < right; ++column) {
            uint8_t value = (source[source_offset] &
                             (uint8_t)(0x80u >> ((unsigned)column & 7u)))
                                ? 0u
                                : 255u;

            destination[0] = value;
            destination[1] = value;
            destination[2] = value;
            destination[3] = 255u;

            for (size_t i = 0; g_overlay_count != 0u && i < rasta_overlay_max;
                 ++i) {
                const rasta_overlay_t *overlay = &g_overlays[i];
                int ox = column - overlay->x;
                int oy = row - overlay->y;

                if (overlay->pixels == NULL || overlay->visible == 0 ||
                    overlay_occluded(overlay, column, row) != 0 ||
                    ox < 0 || oy < 0 ||
                    ox >= overlay->width || oy >= overlay->height)
                    continue;
                blend_overlay_pixel(destination,
                                    overlay->pixels +
                                        ((size_t)oy * overlay->width +
                                         (size_t)ox) * 4u);
            }
            if (g_cursor_visible != 0 && column >= g_cursor_x &&
                column < g_cursor_x + 16 && row >= g_cursor_y &&
                row < g_cursor_y + 16 &&
                (g_cursor_mask[row - g_cursor_y] &
                 (uint16_t)(0x8000u >> (column - g_cursor_x))) != 0u) {
                destination[0] = value;
                destination[1] = value;
                destination[2] = value;
                destination[3] = 255u;
            }
            destination += rasta_bytes_per_pixel;
            if (((unsigned)column & 7u) == 7u)
                ++source_offset;
        }
    }
}

void gem_raster_cursor_update(int x, int y, const uint16_t mask[16],
                              int visible)
{
    g_cursor_x = x;
    g_cursor_y = y;
    if (mask != NULL)
        memcpy(g_cursor_mask, mask, sizeof(g_cursor_mask));
    else
        memset(g_cursor_mask, 0, sizeof(g_cursor_mask));
    g_cursor_visible = visible != 0;
}

int gem_raster_overlay_create(uint32_t id, int x, int y, uint16_t width,
                              uint16_t height)
{
    rasta_overlay_t *overlay = find_overlay(id);
    int old_x = 0;
    int old_y = 0;
    uint16_t old_width = 0;
    uint16_t old_height = 0;
    size_t size = (size_t)width * height * 4u;

    if (width == 0u || height == 0u || size > rasta_overlay_limit)
        return 0;
    if (overlay == NULL) {
        for (size_t i = 0; i < rasta_overlay_max; ++i) {
            if (g_overlays[i].pixels == NULL) {
                overlay = &g_overlays[i];
                overlay->pixels = calloc(1u, size);
                if (overlay->pixels == NULL)
                    return 0;
                overlay->id = id;
                overlay->visible = 1;
                ++g_overlay_count;
                break;
            }
        }
    } else if (overlay->width != width || overlay->height != height) {
        old_x = overlay->x;
        old_y = overlay->y;
        old_width = overlay->width;
        old_height = overlay->height;
        uint8_t *pixels = realloc(overlay->pixels, size);
        if (pixels == NULL)
            return 0;
        overlay->pixels = pixels;
        memset(overlay->pixels, 0, size);
    }
    if (overlay == NULL)
        return 0;
    if (old_width == 0u &&
        (overlay->x != x || overlay->y != y)) {
        old_x = overlay->x;
        old_y = overlay->y;
        old_width = overlay->width;
        old_height = overlay->height;
    }
    overlay->x = x;
    overlay->y = y;
    overlay->width = width;
    overlay->height = height;
    if (old_width != 0u && old_height != 0u)
        publish_rect(old_x, old_y, old_width, old_height);
    return 1;
}

int gem_raster_overlay_resize(uint32_t id, int x, int y, uint16_t width,
                              uint16_t height)
{
    return gem_raster_overlay_create(id, x, y, width, height);
}

int gem_raster_overlay_update(uint32_t id, int x, int y, uint16_t width,
                              uint16_t height, const uint8_t *bgra,
                              uint16_t pitch)
{
    rasta_overlay_t *overlay = find_overlay(id);

    if (overlay == NULL || bgra == NULL || width == 0u || height == 0u ||
        x < 0 || y < 0 || x + width > overlay->width ||
        y + height > overlay->height || pitch < width * 4u)
        return 0;
    for (uint16_t row = 0; row < height; ++row)
        memcpy(overlay->pixels +
                   ((size_t)(y + row) * overlay->width + (size_t)x) * 4u,
               bgra + (size_t)row * pitch, (size_t)width * 4u);
    gem_raster_present_rect(overlay->x + x, overlay->y + y, width, height);
    return 1;
}

void gem_raster_overlay_destroy(uint32_t id)
{
    rasta_overlay_t *overlay = find_overlay(id);

    if (overlay != NULL) {
        if (overlay->visible != 0)
            publish_rect(overlay->x, overlay->y, overlay->width,
                         overlay->height);
        free(overlay->pixels);
        memset(overlay, 0, sizeof(*overlay));
        if (g_overlay_count != 0u)
            --g_overlay_count;
    }
}

void gem_raster_overlay_set_visible(uint32_t id, int visible)
{
    rasta_overlay_t *overlay = find_overlay(id);

    if (overlay == NULL || overlay->visible == (visible != 0))
        return;
    if (overlay->visible != 0)
        publish_rect(overlay->x, overlay->y, overlay->width, overlay->height);
    overlay->visible = (visible != 0);
    if (overlay->visible != 0)
        publish_rect(overlay->x, overlay->y, overlay->width, overlay->height);
}

void gem_raster_overlay_set_occlusion(uint32_t id, const int *rects,
                                      unsigned count)
{
    rasta_overlay_t *overlay = find_overlay(id);
    unsigned limited = count > rasta_overlay_occlusion_max
                           ? rasta_overlay_occlusion_max
                           : count;
    if (overlay == NULL)
        return;
    publish_rect(overlay->x, overlay->y, overlay->width, overlay->height);
    overlay->occlusion_count = limited;
    if (limited != 0u && rects != NULL)
        memcpy(overlay->occlusion, rects, (size_t)limited * 4u * sizeof(int));
    publish_rect(overlay->x, overlay->y, overlay->width, overlay->height);
}

void gem_raster_overlay_clear(void)
{
    clear_overlays();
}

void gem_raster_present_rect(int x, int y, int width, int height)
{
    publish_rect(x, y, width, height);
}

void gem_raster_set_palette(uint8_t index, uint8_t r, uint8_t g, uint8_t b)
{
    (void)index;
    (void)r;
    (void)g;
    (void)b;
}
