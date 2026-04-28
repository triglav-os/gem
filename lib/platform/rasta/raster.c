/*
 * Implements the GEM raster abstraction for the rasta emulator. The
 * backend owns a simple 1 byte per pixel monochrome staging surface,
 * maps the emulator framebuffer file, and packs that surface into
 * rasta's 1-bit framebuffer layout.
 *
 * MIT License (see: LICENSE)
 * Copyright (C) 2026 tomaz stih
 */

#define _POSIX_C_SOURCE 200809L

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
    rasta_bits_per_pixel = 1
};

static gem_raster_surface_t g_surface;
static int g_framebuffer_fd = -1;
static uint8_t *g_framebuffer = NULL;
static size_t g_framebuffer_size;

static const char *rasta_framebuffer_path(void)
{
    const char *value = getenv("GEM_RASTA_FRAMEBUFFER");

    if (value != NULL && value[0] != '\0') {
        return value;
    }

    value = getenv("RASTA_FRAMEBUFFER");
    if (value != NULL && value[0] != '\0') {
        return value;
    }

    return "/tmp/rasta.fb";
}

static size_t rasta_file_size(uint16_t width, uint16_t height)
{
    size_t bits = (size_t) width * (size_t) height *
        (size_t) rasta_bits_per_pixel;

    return (bits + 7u) / 8u;
}

static int open_framebuffer(uint16_t width, uint16_t height)
{
    const char *path = rasta_framebuffer_path();
    size_t size = rasta_file_size(width, height);

    g_framebuffer_fd = open(path, O_RDWR | O_CREAT, 0644);
    if (g_framebuffer_fd < 0) {
        return 0;
    }

    if (ftruncate(g_framebuffer_fd, (off_t) size) != 0) {
        close(g_framebuffer_fd);
        g_framebuffer_fd = -1;
        return 0;
    }

    g_framebuffer = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED,
        g_framebuffer_fd, 0);
    if (g_framebuffer == MAP_FAILED) {
        close(g_framebuffer_fd);
        g_framebuffer_fd = -1;
        g_framebuffer = NULL;
        return 0;
    }

    g_framebuffer_size = size;
    return 1;
}

static void close_framebuffer(void)
{
    if (g_framebuffer != NULL) {
        munmap(g_framebuffer, g_framebuffer_size);
        g_framebuffer = NULL;
    }
    if (g_framebuffer_fd >= 0) {
        close(g_framebuffer_fd);
        g_framebuffer_fd = -1;
    }

    g_framebuffer_size = 0u;
}

/*
 * Packs one quantized RGB pixel into the bitstream expected by rasta.
 * Pixels are densely packed and written most-significant-bit first.
 */
static void set_rasta_pixel(uint16_t x, uint16_t y, uint8_t value)
{
    size_t pixel_index = (size_t) y * (size_t) g_surface.width + (size_t) x;
    size_t start_bit = pixel_index * (size_t) rasta_bits_per_pixel;
    unsigned int bit;

    for (bit = 0u; bit < rasta_bits_per_pixel; ++bit) {
        size_t absolute_bit = start_bit + (size_t) bit;
        size_t byte_index = absolute_bit / 8u;
        size_t bit_index = 7u - (absolute_bit % 8u);
        uint8_t mask = (uint8_t) (1u << bit_index);
        uint8_t source_bit = (uint8_t)
            ((value >> (rasta_bits_per_pixel - bit - 1u)) & 1u);

        if (byte_index >= g_framebuffer_size) {
            return;
        }

        if (source_bit != 0u) {
            g_framebuffer[byte_index] |= mask;
        } else {
            g_framebuffer[byte_index] &= (uint8_t) ~mask;
        }
    }
}

int gem_raster_init(uint16_t width, uint16_t height,
    gem_raster_format_t format)
{
    size_t bytes;
    size_t pitch;

    if (width == 0u || height == 0u || format != GEM_RASTER_INDEX8) {
        errno = EINVAL;
        return 0;
    }

    pitch = (size_t) width;
    bytes = pitch * (size_t) height;
    if (pitch > 0xffffu) {
        errno = EOVERFLOW;
        return 0;
    }

    memset(&g_surface, 0, sizeof(g_surface));
    g_surface.width = width;
    g_surface.height = height;
    g_surface.pitch = (uint16_t) pitch;
    g_surface.format = format;
    g_surface.pixels = calloc(1u, bytes);
    if (g_surface.pixels == NULL) {
        return 0;
    }

    if (!open_framebuffer(width, height)) {
        free(g_surface.pixels);
        g_surface.pixels = NULL;
        memset(&g_surface, 0, sizeof(g_surface));
        return 0;
    }

    memset(g_framebuffer, 0, g_framebuffer_size);
    return 1;
}

void gem_raster_shutdown(void)
{
    close_framebuffer();

    free(g_surface.pixels);
    g_surface.pixels = NULL;
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
    uint16_t y;

    if (g_surface.pixels == NULL || g_framebuffer == NULL) {
        return;
    }

    for (y = 0u; y < g_surface.height; ++y) {
        uint8_t *row = (uint8_t *) g_surface.pixels +
            (size_t) y * g_surface.pitch;
        uint16_t x;

        for (x = 0u; x < g_surface.width; ++x) {
            set_rasta_pixel(x, y, (uint8_t) (row[x] != 0u));
        }
    }

    msync(g_framebuffer, g_framebuffer_size, MS_ASYNC);
}

void gem_raster_set_palette(uint8_t index, uint8_t r, uint8_t g, uint8_t b)
{
    (void) index;
    (void) r;
    (void) g;
    (void) b;
}
