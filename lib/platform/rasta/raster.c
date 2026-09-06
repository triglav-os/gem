/*
 * Implements the GEM raster abstraction for the rasta emulator. The
 * backend stages VDI drawing in a packed shadow surface and publishes
 * completed updates to rasta's mapped framebuffer. The viewer never
 * observes an intermediate clear while a window is being repainted.
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

enum { rasta_bits_per_pixel = 1 };

static gem_raster_surface_t g_surface;
static uint8_t *g_present_pixels;
static int g_framebuffer_fd = -1;
static size_t g_framebuffer_size;
static dev_t g_framebuffer_dev;
static ino_t g_framebuffer_ino;

static const char *rasta_framebuffer_path(void);
static size_t rasta_row_bytes(uint16_t width);
static size_t rasta_file_size(uint16_t width, uint16_t height);
static int ensure_framebuffer_target(void);

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
        g_surface.pixels = calloc(size, 1);
        if (g_surface.pixels == NULL) {
            munmap(mapping, size);
            g_present_pixels = NULL;
            return 0;
        }
    }
    g_framebuffer_size = size;
    /* A new viewer mapping needs the whole frame, even on a partial present. */
    memcpy(g_present_pixels, g_surface.pixels, size);
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

static size_t rasta_row_bytes(uint16_t width)
{
    return ((size_t)width + 7u) / 8u;
}

static size_t rasta_file_size(uint16_t width, uint16_t height)
{
    return rasta_row_bytes(width) * (size_t)height;
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
    g_framebuffer_dev = 0;
    g_framebuffer_ino = 0;
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
        memcpy(g_present_pixels, g_surface.pixels, expected_size);
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
    memset(&g_surface, 0, sizeof(g_surface));
    g_surface.width = width;
    g_surface.height = height;
    g_surface.pitch = (uint16_t)pitch;
    g_surface.format = format;
    g_surface.pixels = NULL;
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
    memcpy(g_present_pixels, g_surface.pixels, g_framebuffer_size);
}

void gem_raster_present_rect(int x, int y, int width, int height)
{
    int row;
    int64_t right = (int64_t)x + width;
    int64_t bottom = (int64_t)y + height;
    size_t first, last;
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
    first = (size_t)x / 8u;
    last = ((size_t)right + 7u) / 8u;
    for (row = y; row < bottom; ++row) {
        size_t offset = (size_t)row * rasta_row_bytes(g_surface.width) + first;
        memcpy(g_present_pixels + offset, (uint8_t *)g_surface.pixels + offset,
               last - first);
    }
}

void gem_raster_set_palette(uint8_t index, uint8_t r, uint8_t g, uint8_t b)
{
    (void)index;
    (void)r;
    (void)g;
    (void)b;
}
