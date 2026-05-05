/*
 * Implements the memory-backed host services used by the VDI tests.
 * These stubs satisfy the raster, input, and OS abstractions without
 * touching the real hosted desktop runtime.
 *
 * MIT License (see: LICENSE)
 * Copyright (C) 2026 tomaz stih
 */

#include "vdi_test.h"

#include "platform/hid.h"
#include "platform/os.h"

#include <stdlib.h>
#include <string.h>

static gem_raster_surface_t g_surface;
static int g_present_count;

void test_bitmap_init(test_bitmap_t *bitmap, WORD width, WORD height)
{
    size_t size;

    bitmap->width = width;
    bitmap->height = height;
    bitmap->pitch = width;
    size = (size_t) width * (size_t) height;
    bitmap->pixels = calloc(size, 1u);
}

void test_bitmap_free(test_bitmap_t *bitmap)
{
    free(bitmap->pixels);
    bitmap->pixels = NULL;
}

void test_bitmap_clear(test_bitmap_t *bitmap, uint8_t value)
{
    memset(bitmap->pixels, value,
        (size_t) bitmap->pitch * (size_t) bitmap->height);
}

uint8_t test_bitmap_get_pixel(const test_bitmap_t *bitmap, WORD x, WORD y)
{
    if (bitmap == NULL || bitmap->pixels == NULL ||
        x < 0 || y < 0 || x >= bitmap->width || y >= bitmap->height) {
        return 0;
    }

    return bitmap->pixels[(size_t) y * (size_t) bitmap->pitch + (size_t) x];
}

void test_bitmap_set_pixel(test_bitmap_t *bitmap, WORD x, WORD y, uint8_t value)
{
    if (bitmap == NULL || bitmap->pixels == NULL ||
        x < 0 || y < 0 || x >= bitmap->width || y >= bitmap->height) {
        return;
    }

    bitmap->pixels[(size_t) y * (size_t) bitmap->pitch + (size_t) x] =
        (uint8_t) ((value != 0u) ? 1u : 0u);
}

int test_bitmap_equal(const test_bitmap_t *left, const test_bitmap_t *right)
{
    size_t size;

    if (left == NULL || right == NULL ||
        left->width != right->width ||
        left->height != right->height ||
        left->pitch != right->pitch) {
        return 0;
    }

    size = (size_t) left->pitch * (size_t) left->height;
    return memcmp(left->pixels, right->pixels, size) == 0;
}

gem_raster_surface_t *test_host_surface(void)
{
    return &g_surface;
}

int test_host_present_count(void)
{
    return g_present_count;
}

void test_host_reset_present_count(void)
{
    g_present_count = 0;
}

int gem_os_init(void)
{
    return 1;
}

void gem_os_shutdown(void)
{
}

void *gem_os_alloc(size_t size)
{
    return calloc(1u, size);
}

void gem_os_free(void *ptr)
{
    free(ptr);
}

uint32_t gem_os_ticks_ms(void)
{
    return 0;
}

void gem_os_sleep_ms(uint32_t ms)
{
    (void) ms;
}

int gem_hid_init(void)
{
    return 1;
}

void gem_hid_shutdown(void)
{
}

int gem_hid_poll(gem_hid_event_t *evt)
{
    (void) evt;
    return 0;
}

int gem_raster_init(uint16_t width, uint16_t height, gem_raster_format_t format)
{
    size_t pitch = ((size_t) width + 7u) / 8u;
    size_t size = pitch * (size_t) height;

    if (format != GEM_RASTER_MONO1) {
        return 0;
    }

    free(g_surface.pixels);
    memset(&g_surface, 0, sizeof(g_surface));
    g_surface.width = width;
    g_surface.height = height;
    g_surface.pitch = (uint16_t) pitch;
    g_surface.format = format;
    g_surface.pixels = calloc(size, 1u);
    return (g_surface.pixels != NULL) ? 1 : 0;
}

int gem_raster_resync(void)
{
    return 1;
}

void gem_raster_shutdown(void)
{
    free(g_surface.pixels);
    memset(&g_surface, 0, sizeof(g_surface));
}

gem_raster_surface_t *gem_raster_surface(void)
{
    return &g_surface;
}

void gem_raster_present(void)
{
    ++g_present_count;
}

void gem_raster_set_palette(uint8_t index,
                            uint8_t r,
                            uint8_t g,
                            uint8_t b)
{
    (void) index;
    (void) r;
    (void) g;
    (void) b;
}
