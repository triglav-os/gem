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

enum { linux_overlay_max = 8, linux_overlay_limit = 8 * 1024 * 1024 };
enum { linux_overlay_occlusion_max = 16 };
typedef struct linux_overlay {
    uint32_t id;
    int x;
    int y;
    uint16_t width;
    uint16_t height;
    int visible;
    int occlusion[linux_overlay_occlusion_max][4];
    unsigned occlusion_count;
    uint8_t *pixels;
} linux_overlay_t;
static linux_overlay_t g_overlays[linux_overlay_max];
static unsigned g_overlay_count;
static int g_cursor_x;
static int g_cursor_y;
static uint16_t g_cursor_mask[16];
static int g_cursor_visible;

static linux_overlay_t *find_overlay(uint32_t id)
{
    for (size_t i = 0; i < linux_overlay_max; ++i)
        if (g_overlays[i].pixels != NULL && g_overlays[i].id == id)
            return &g_overlays[i];
    return NULL;
}

static int overlay_active(void)
{
    return g_overlay_count != 0u;
}

static int overlay_occluded(const linux_overlay_t *overlay, int x, int y)
{
    for (unsigned i = 0; i < overlay->occlusion_count; ++i) {
        const int *r = overlay->occlusion[i];
        if (x >= r[0] && y >= r[1] && x < r[0] + r[2] &&
            y < r[1] + r[3])
            return 1;
    }
    return 0;
}

static int overlay_color(int x, int y, uint8_t *red, uint8_t *green,
                         uint8_t *blue)
{
    if (g_overlay_count == 0u)
        return 0;
    for (size_t i = linux_overlay_max; i-- > 0;) {
        const linux_overlay_t *overlay = &g_overlays[i];
        int ox = x - overlay->x;
        int oy = y - overlay->y;
        const uint8_t *pixel;
        unsigned alpha;

        if (overlay->pixels == NULL || overlay->visible == 0 || ox < 0 || oy < 0 ||
            ox >= overlay->width || oy >= overlay->height)
            continue;
        if (overlay_occluded(overlay, x, y) != 0)
            continue;
        pixel = overlay->pixels +
                ((size_t)oy * overlay->width + (size_t)ox) * 4u;
        alpha = pixel[3];
        if (alpha == 0u)
            continue;
        if (alpha == 255u) {
            *blue = pixel[0];
            *green = pixel[1];
            *red = pixel[2];
            return 1;
        }
        *blue = (uint8_t)((pixel[0] * alpha + *blue * (255u - alpha) +
                           127u) /
                          255u);
        *green = (uint8_t)((pixel[1] * alpha + *green * (255u - alpha) +
                            127u) /
                           255u);
        *red = (uint8_t)((pixel[2] * alpha + *red * (255u - alpha) + 127u) /
                         255u);
        return 1;
    }
    return 0;
}

static void clear_overlays(void)
{
    for (size_t i = 0; i < linux_overlay_max; ++i) {
        free(g_overlays[i].pixels);
        memset(&g_overlays[i], 0, sizeof(g_overlays[i]));
    }
    g_overlay_count = 0u;
}

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
    return (uint32_t)(((uint64_t)value * maximum + 127u) / 255u);
}

static uint32_t native_pixel(uint8_t value)
{
    uint32_t pixel = 0u;

    pixel |= scale_channel(value, g_var.red.length) << g_var.red.offset;
    pixel |= scale_channel(value, g_var.green.length) << g_var.green.offset;
    pixel |= scale_channel(value, g_var.blue.length) << g_var.blue.offset;
    if (g_var.transp.length != 0u) {
        pixel |= scale_channel(255u, g_var.transp.length)
                 << g_var.transp.offset;
    }
    return pixel;
}

static void write_native_pixel(uint8_t *destination, uint32_t pixel,
                               uint32_t bytes_per_pixel)
{
    uint32_t byte;

    for (byte = 0u; byte < bytes_per_pixel; ++byte) {
        destination[byte] = (uint8_t)(pixel >> (byte * 8u));
    }
}

static void set_indexed_palette(void)
{
    uint16_t red[256];
    uint16_t green[256];
    uint16_t blue[256];
    struct fb_cmap map;
    unsigned int index;

    if (g_var.bits_per_pixel != 8u || g_fix.visual == FB_VISUAL_TRUECOLOR ||
        g_fix.visual == FB_VISUAL_DIRECTCOLOR) {
        return;
    }

    for (index = 0u; index < 256u; ++index) {
        uint16_t value = (uint16_t)(index * 257u);

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
    (void)ioctl(g_framebuffer_fd, FBIOPUTCMAP, &map);
}

int gem_raster_init(uint16_t width, uint16_t height, gem_raster_format_t format)
{
    size_t pitch;
    size_t shadow_size;

    /* Zero width/height is the documented "fill the framebuffer" request
     * and is resolved once the device geometry is known below. */
    if (format != GEM_RASTER_MONO1) {
        errno = EINVAL;
        return 0;
    }

    clear_overlays();
    g_cursor_visible = 0;
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

    /* width/height 0 (or oversized) → use the full visible framebuffer. */
    if (width == 0u || width > g_var.xres) {
        width = (uint16_t)g_var.xres;
    }
    if (height == 0u || height > g_var.yres) {
        height = (uint16_t)g_var.yres;
    }
    if (width == 0u || height == 0u) {
        errno = ENOTSUP;
        gem_raster_shutdown();
        return 0;
    }
    pitch = ((size_t)width + 7u) / 8u;
    if (pitch > UINT16_MAX) {
        errno = EOVERFLOW;
        gem_raster_shutdown();
        return 0;
    }

    shadow_size = pitch * (size_t)height;
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
    g_surface.pitch = (uint16_t)pitch;
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
    clear_overlays();
    g_cursor_visible = 0;
    free(g_surface.pixels);
    if (g_framebuffer != NULL) {
        (void)munmap(g_framebuffer, g_framebuffer_size);
    }
    if (g_framebuffer_fd >= 0) {
        (void)close(g_framebuffer_fd);
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

/*
 * Match the rasta mono buffer interpretation at the FB boundary only.
 * GEM/VDI still use the same shadow packing as rasta (set bit = black
 * ink on the host display). Do not "fix" polarity in AES/VDI.
 *
 * set bit → black ink, clear → white paper.
 */
static uint8_t shadow_bit_to_intensity(int bit_set)
{
    return bit_set ? 0u : 255u;
}

/*
 * Fast path for 32bpp (QEMU stdvga / common DRM fbdev): expand whole
 * mono shadow bytes into eight native pixels with 32-bit stores instead
 * of one bit → one pixel per loop iteration.
 */
static void present_rect_xrgb32(const uint8_t *source, int x0, int y0, int x1,
                                int y1)
{
    const uint32_t black = native_pixel(0u);
    const uint32_t white = native_pixel(255u);
    const size_t fb_pitch = g_fix.line_length;
    const size_t mono_pitch = g_surface.pitch;
    const size_t x_off = (size_t)g_var.xoffset;
    int row_y;

    for (row_y = y0; row_y <= y1; ++row_y) {
        uint32_t *dst =
            (uint32_t *)(g_framebuffer +
                         ((size_t)row_y + g_var.yoffset) * fb_pitch) +
            x_off;
        const uint8_t *src_row = source + (size_t)row_y * mono_pitch;
        int col_x = x0;

        /* Leading partial mono byte. */
        while (col_x <= x1 && ((unsigned int)col_x & 7u) != 0u) {
            uint8_t bits = src_row[col_x / 8];
            int bit_set = (bits & (uint8_t)(0x80u >> (col_x & 7))) != 0u;

            dst[col_x] = bit_set ? black : white;
            ++col_x;
        }

        /* Full mono bytes → 8 pixels. */
        while (col_x + 7 <= x1) {
            uint8_t bits = src_row[col_x / 8];
            uint32_t *p = dst + col_x;

            p[0] = (bits & 0x80u) ? black : white;
            p[1] = (bits & 0x40u) ? black : white;
            p[2] = (bits & 0x20u) ? black : white;
            p[3] = (bits & 0x10u) ? black : white;
            p[4] = (bits & 0x08u) ? black : white;
            p[5] = (bits & 0x04u) ? black : white;
            p[6] = (bits & 0x02u) ? black : white;
            p[7] = (bits & 0x01u) ? black : white;
            col_x += 8;
        }

        /* Trailing partial mono byte. */
        while (col_x <= x1) {
            uint8_t bits = src_row[col_x / 8];
            int bit_set = (bits & (uint8_t)(0x80u >> (col_x & 7))) != 0u;

            dst[col_x] = bit_set ? black : white;
            ++col_x;
        }
    }
}

void gem_raster_present_rect(int x, int y, int width, int height)
{
    const uint8_t *source = g_surface.pixels;
    uint32_t bytes_per_pixel = (g_var.bits_per_pixel + 7u) / 8u;
    int x0;
    int y0;
    int64_t x1;
    int64_t y1;
    int row_y;

    if (source == NULL || g_framebuffer == NULL || width <= 0 || height <= 0) {
        return;
    }

    x0 = x;
    y0 = y;
    x1 = (int64_t)x + width - 1;
    y1 = (int64_t)y + height - 1;
    if (x0 < 0) {
        x0 = 0;
    }
    if (y0 < 0) {
        y0 = 0;
    }
    if (x1 >= (int)g_surface.width) {
        x1 = (int)g_surface.width - 1;
    }
    if (y1 >= (int)g_surface.height) {
        y1 = (int)g_surface.height - 1;
    }
    if (x0 > x1 || y0 > y1) {
        return;
    }

    /* QEMU std / most DRM fbdev: 32bpp, 4-byte aligned line length. */
    if (!overlay_active() && g_var.bits_per_pixel == 32u &&
        (g_fix.line_length % 4u) == 0u &&
        ((uintptr_t)g_framebuffer % 4u) == 0u) {
        present_rect_xrgb32(source, x0, y0, x1, y1);
        return;
    }

    for (row_y = y0; row_y <= y1; ++row_y) {
        uint8_t *row =
            g_framebuffer + ((size_t)row_y + g_var.yoffset) * g_fix.line_length;
        int col_x;

        if (g_var.bits_per_pixel == 1u) {
            for (col_x = x0; col_x <= x1; ++col_x) {
                size_t source_offset =
                    (size_t)row_y * g_surface.pitch + (size_t)col_x / 8u;
                uint8_t source_mask =
                    (uint8_t)(0x80u >> ((unsigned int)col_x & 7u));
                size_t target_x = (size_t)col_x + g_var.xoffset;
                uint8_t target_mask = (uint8_t)(0x80u >> (target_x & 7u));
                int bit_set = (source[source_offset] & source_mask) != 0u;
                int white = shadow_bit_to_intensity(bit_set) != 0u;

                if (g_fix.visual == FB_VISUAL_MONO01) {
                    white = !white;
                }
                if (white) {
                    row[target_x / 8u] |= target_mask;
                } else {
                    row[target_x / 8u] &= (uint8_t)~target_mask;
                }
            }
            continue;
        }

        row += (size_t)g_var.xoffset * bytes_per_pixel;
        for (col_x = x0; col_x <= x1; ++col_x) {
            size_t source_offset =
                (size_t)row_y * g_surface.pitch + (size_t)col_x / 8u;
            uint8_t mask = (uint8_t)(0x80u >> ((unsigned int)col_x & 7u));
            int bit_set = (source[source_offset] & mask) != 0u;
                uint8_t intensity = shadow_bit_to_intensity(bit_set);
                uint8_t red = intensity;
                uint8_t green = intensity;
                uint8_t blue = intensity;
                (void)overlay_color(col_x, row_y, &red, &green, &blue);
                if (g_cursor_visible != 0 && col_x >= g_cursor_x &&
                    col_x < g_cursor_x + 16 && row_y >= g_cursor_y &&
                    row_y < g_cursor_y + 16 &&
                    (g_cursor_mask[row_y - g_cursor_y] &
                     (uint16_t)(0x8000u >> (col_x - g_cursor_x))) != 0u)
                    red = green = blue = intensity;
                uint32_t pixel;

                if (g_var.bits_per_pixel == 8u) {
                    pixel = ((unsigned)red * 77u +
                             (unsigned)green * 150u +
                             (unsigned)blue * 29u + 128u) / 256u;
                } else {
                    pixel = scale_channel(red, g_var.red.length)
                                << g_var.red.offset;
                    pixel |= scale_channel(green, g_var.green.length)
                             << g_var.green.offset;
                    pixel |= scale_channel(blue, g_var.blue.length)
                             << g_var.blue.offset;
                    if (g_var.transp.length != 0u)
                        pixel |= scale_channel(255u, g_var.transp.length)
                                 << g_var.transp.offset;
                }

            write_native_pixel(row + (size_t)col_x * bytes_per_pixel, pixel,
                               bytes_per_pixel);
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
    linux_overlay_t *overlay = find_overlay(id);
    size_t size = (size_t)width * height * 4u;

    if (width == 0u || height == 0u || size > linux_overlay_limit)
        return 0;
    if (overlay == NULL) {
        for (size_t i = 0; i < linux_overlay_max; ++i) {
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
        uint8_t *pixels = realloc(overlay->pixels, size);
        if (pixels == NULL)
            return 0;
        overlay->pixels = pixels;
        memset(overlay->pixels, 0, size);
    }
    if (overlay == NULL)
        return 0;
    overlay->x = x;
    overlay->y = y;
    overlay->width = width;
    overlay->height = height;
    return 1;
}

void gem_raster_overlay_set_visible(uint32_t id, int visible)
{
    linux_overlay_t *overlay = find_overlay(id);

    if (overlay != NULL)
        overlay->visible = (visible != 0);
}

void gem_raster_overlay_set_occlusion(uint32_t id, const int *rects,
                                      unsigned count)
{
    linux_overlay_t *overlay = find_overlay(id);
    unsigned limited = count > linux_overlay_occlusion_max
                           ? linux_overlay_occlusion_max
                           : count;
    if (overlay == NULL)
        return;
    overlay->occlusion_count = limited;
    if (limited != 0u && rects != NULL)
        memcpy(overlay->occlusion, rects, (size_t)limited * 4u * sizeof(int));
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
    linux_overlay_t *overlay = find_overlay(id);

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
    linux_overlay_t *overlay = find_overlay(id);

    if (overlay != NULL) {
        free(overlay->pixels);
        memset(overlay, 0, sizeof(*overlay));
        if (g_overlay_count != 0u)
            --g_overlay_count;
    }
}

void gem_raster_overlay_clear(void)
{
    clear_overlays();
}

void gem_raster_present(void)
{
    if (g_surface.pixels == NULL) {
        return;
    }
    gem_raster_present_rect(0, 0, (int)g_surface.width, (int)g_surface.height);
}

void gem_raster_clear(void)
{
    if (g_surface.pixels != NULL && g_framebuffer_size != 0u) {
        memset(g_surface.pixels, 0, g_framebuffer_size);
    }
    if (g_framebuffer != NULL && g_framebuffer_size != 0u) {
        memset(g_framebuffer, 0, g_framebuffer_size);
    }
}

void gem_raster_set_palette(uint8_t index, uint8_t red, uint8_t green,
                            uint8_t blue)
{
    (void)index;
    (void)red;
    (void)green;
    (void)blue;
}
