/*
 * Implements native Linux framebuffer output for GEM. VDI draws into a
 * packed monochrome shadow surface and this backend converts it to the
 * fbdev device's native 1, 8, 16, 24, or 32-bit pixel representation.
 *
 * MIT License (see: LICENSE)
 * Copyright (C) 2026 tomaz stih
 */

#define _POSIX_C_SOURCE 200809L

#include "platform/raster.h"

#include <errno.h>
#include <fcntl.h>
#include <linux/fb.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <unistd.h>

static gem_raster_surface_t g_surface;
static struct fb_fix_screeninfo g_fix;
static struct fb_var_screeninfo g_var;
static uint8_t *g_framebuffer;
static size_t g_framebuffer_size;
static int g_framebuffer_fd = -1;

static const char *framebuffer_path(void)
{
    const char *path = getenv("GEM_LINUX_FB");

    return (path != NULL && path[0] != '\0') ? path : "/dev/fb0";
}

static uint32_t scale_channel(uint8_t value, uint32_t length)
{
    uint64_t maximum;

    if (length == 0u) {
        return 0u;
    }
    maximum = (length >= 32u) ? UINT32_MAX : ((UINT64_C(1) << length) - 1u);
    return (uint32_t) (((uint64_t) value * maximum + 127u) / 255u);
}

static uint32_t native_pixel(uint8_t value)
{
    uint32_t pixel = 0u;

    pixel |= scale_channel(value, g_var.red.length) << g_var.red.offset;
    pixel |= scale_channel(value, g_var.green.length) << g_var.green.offset;
    pixel |= scale_channel(value, g_var.blue.length) << g_var.blue.offset;
    if (g_var.transp.length != 0u) {
        pixel |= scale_channel(255u, g_var.transp.length) <<
            g_var.transp.offset;
    }
    return pixel;
}

static void write_native_pixel(uint8_t *destination, uint32_t pixel,
    uint32_t bytes_per_pixel)
{
    uint32_t byte;

    for (byte = 0u; byte < bytes_per_pixel; ++byte) {
        destination[byte] = (uint8_t) (pixel >> (byte * 8u));
    }
}

static void set_indexed_palette(void)
{
    uint16_t red[256];
    uint16_t green[256];
    uint16_t blue[256];
    struct fb_cmap map;
    unsigned int index;

    if (g_var.bits_per_pixel != 8u ||
        g_fix.visual == FB_VISUAL_TRUECOLOR ||
        g_fix.visual == FB_VISUAL_DIRECTCOLOR) {
        return;
    }

    for (index = 0u; index < 256u; ++index) {
        uint16_t value = (uint16_t) (index * 257u);

        red[index] = value;
        green[index] = value;
        blue[index] = value;
    }
    memset(&map, 0, sizeof(map));
    map.start = 0u;
    map.len = 256u;
    map.red = red;
    map.green = green;
    map.blue = blue;
    (void) ioctl(g_framebuffer_fd, FBIOPUTCMAP, &map);
}

int gem_raster_init(uint16_t width, uint16_t height,
    gem_raster_format_t format)
{
    size_t pitch;
    size_t shadow_size;

    if (width == 0u || height == 0u || format != GEM_RASTER_MONO1) {
        errno = EINVAL;
        return 0;
    }

    g_framebuffer_fd = open(framebuffer_path(), O_RDWR | O_CLOEXEC);
    if (g_framebuffer_fd < 0) {
        return 0;
    }
    if (ioctl(g_framebuffer_fd, FBIOGET_FSCREENINFO, &g_fix) != 0 ||
        ioctl(g_framebuffer_fd, FBIOGET_VSCREENINFO, &g_var) != 0) {
        gem_raster_shutdown();
        return 0;
    }
    if (g_var.bits_per_pixel != 1u && g_var.bits_per_pixel != 8u &&
        g_var.bits_per_pixel != 16u && g_var.bits_per_pixel != 24u &&
        g_var.bits_per_pixel != 32u) {
        errno = ENOTSUP;
        gem_raster_shutdown();
        return 0;
    }

    if (width > g_var.xres) {
        width = (uint16_t) g_var.xres;
    }
    if (height > g_var.yres) {
        height = (uint16_t) g_var.yres;
    }
    pitch = ((size_t) width + 7u) / 8u;
    if (pitch > UINT16_MAX) {
        errno = EOVERFLOW;
        gem_raster_shutdown();
        return 0;
    }

    shadow_size = pitch * (size_t) height;
    g_surface.pixels = calloc(1u, shadow_size);
    if (g_surface.pixels == NULL) {
        gem_raster_shutdown();
        return 0;
    }

    g_framebuffer_size = g_fix.smem_len;
    g_framebuffer = mmap(NULL, g_framebuffer_size, PROT_READ | PROT_WRITE,
        MAP_SHARED, g_framebuffer_fd, 0);
    if (g_framebuffer == MAP_FAILED) {
        g_framebuffer = NULL;
        gem_raster_shutdown();
        return 0;
    }

    g_surface.width = width;
    g_surface.height = height;
    g_surface.pitch = (uint16_t) pitch;
    g_surface.format = format;
    set_indexed_palette();
    return 1;
}

int gem_raster_resync(void)
{
    return g_surface.pixels != NULL && g_framebuffer != NULL;
}

void gem_raster_shutdown(void)
{
    free(g_surface.pixels);
    if (g_framebuffer != NULL) {
        (void) munmap(g_framebuffer, g_framebuffer_size);
    }
    if (g_framebuffer_fd >= 0) {
        (void) close(g_framebuffer_fd);
    }
    memset(&g_surface, 0, sizeof(g_surface));
    memset(&g_fix, 0, sizeof(g_fix));
    memset(&g_var, 0, sizeof(g_var));
    g_framebuffer = NULL;
    g_framebuffer_size = 0u;
    g_framebuffer_fd = -1;
}

gem_raster_surface_t *gem_raster_surface(void)
{
    return (g_surface.pixels != NULL) ? &g_surface : NULL;
}

void gem_raster_present(void)
{
    const uint8_t *source = g_surface.pixels;
    uint32_t bytes_per_pixel = (g_var.bits_per_pixel + 7u) / 8u;
    uint16_t y;

    if (source == NULL || g_framebuffer == NULL) {
        return;
    }

    for (y = 0u; y < g_surface.height; ++y) {
        uint8_t *row = g_framebuffer +
            ((size_t) y + g_var.yoffset) * g_fix.line_length;
        uint16_t x;

        if (g_var.bits_per_pixel == 1u) {
            for (x = 0u; x < g_surface.width; ++x) {
                size_t source_offset =
                    (size_t) y * g_surface.pitch + x / 8u;
                uint8_t source_mask = (uint8_t) (0x80u >> (x & 7u));
                size_t target_x = (size_t) x + g_var.xoffset;
                uint8_t target_mask =
                    (uint8_t) (0x80u >> (target_x & 7u));
                int white = (source[source_offset] & source_mask) != 0u;

                if (g_fix.visual == FB_VISUAL_MONO01) {
                    white = !white;
                }
                if (white) {
                    row[target_x / 8u] |= target_mask;
                } else {
                    row[target_x / 8u] &= (uint8_t) ~target_mask;
                }
            }
            continue;
        }
        row += (size_t) g_var.xoffset * bytes_per_pixel;
        for (x = 0u; x < g_surface.width; ++x) {
            size_t source_offset = (size_t) y * g_surface.pitch + x / 8u;
            uint8_t mask = (uint8_t) (0x80u >> (x & 7u));
            uint8_t intensity =
                ((source[source_offset] & mask) != 0u) ? 255u : 0u;
            uint32_t pixel = (g_var.bits_per_pixel == 8u) ?
                intensity : native_pixel(intensity);

            write_native_pixel(row + (size_t) x * bytes_per_pixel,
                pixel, bytes_per_pixel);
        }
    }
}

void gem_raster_set_palette(uint8_t index, uint8_t red, uint8_t green,
    uint8_t blue)
{
    (void) index;
    (void) red;
    (void) green;
    (void) blue;
}
