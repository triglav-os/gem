/*
 * Implements the GEM raster abstraction for the rasta emulator. The
 * backend maps rasta's packed 1-bit framebuffer file directly and
 * exposes that mapping as the GEM raster surface. VDI therefore draws
 * into the same bytes that rasta displays, without a shadow staging
 * buffer or frame repacking step.
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

    if (g_surface.pixels != NULL) {
        if (munmap(g_surface.pixels, g_framebuffer_size) != 0) {
            return 0;
        }
        g_surface.pixels = NULL;
    }

    mapping = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED,
        g_framebuffer_fd, 0);
    if (mapping == MAP_FAILED) {
        return 0;
    }

    g_surface.pixels = mapping;
    g_framebuffer_size = size;
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

    if (g_framebuffer_fd >= 0) {
        close(g_framebuffer_fd);
        g_framebuffer_fd = -1;
    }

    g_framebuffer_fd = open(path, O_RDWR | O_CREAT, 0644);
    if (g_framebuffer_fd < 0) {
        return 0;
    }

    if (ftruncate(g_framebuffer_fd, (off_t) size) != 0) {
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
    return ((size_t) width + 7u) / 8u;
}

static size_t rasta_file_size(uint16_t width, uint16_t height)
{
    return rasta_row_bytes(width) * (size_t) height;
}

static int open_framebuffer(uint16_t width, uint16_t height)
{
    size_t size = rasta_file_size(width, height);

    return reopen_framebuffer_target(size);
}

static void close_framebuffer(void)
{
    if (g_surface.pixels != NULL) {
        munmap(g_surface.pixels, g_framebuffer_size);
        g_surface.pixels = NULL;
    }
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
        fd_st.st_dev != g_framebuffer_dev || fd_st.st_ino != g_framebuffer_ino) {
        return reopen_framebuffer_target(expected_size);
    }

    if ((size_t) fd_st.st_size != expected_size) {
        if (ftruncate(g_framebuffer_fd, (off_t) expected_size) != 0) {
            return 0;
        }
        if (!record_framebuffer_identity()) {
            return 0;
        }
    }

    return 1;
}

int gem_raster_init(uint16_t width, uint16_t height,
    gem_raster_format_t format)
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

    memset(&g_surface, 0, sizeof(g_surface));
    g_surface.width = width;
    g_surface.height = height;
    g_surface.pitch = (uint16_t) pitch;
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
    (void) msync(g_surface.pixels, g_framebuffer_size, MS_SYNC);
}

void gem_raster_set_palette(uint8_t index, uint8_t r, uint8_t g, uint8_t b)
{
    (void) index;
    (void) r;
    (void) g;
    (void) b;
}
