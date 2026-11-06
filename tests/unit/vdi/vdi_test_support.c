/*
 * Shared harness for the VDI unit tests: opening the memory-backed
 * workstation, snapshotting and comparing the surface against reference
 * bitmaps, and packed monochrome MFDB fixtures.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (C) 2026 tomaz stih
 */

#define _POSIX_C_SOURCE 200809L

#include "vdi_test.h"
#include "vdi_internal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

uint8_t test_vdi_color_to_pixel(WORD color_index)
{
    return (uint8_t)((color_index == 0) ? 1u : 0u);
}

VDI_HANDLE open_handle(void)
{
    WORD work_in[11] = {0};
    WORD work_out[57] = {0};
    VDI_HANDLE handle = 0;

    setenv("GEM_VDI_WIDTH", "96", 1);
    setenv("GEM_VDI_HEIGHT", "64", 1);
    v_opnvwk(work_in, &handle, work_out);
    return handle;
}

void snapshot_surface_bitmap(test_bitmap_t *bitmap)
{
    gem_raster_surface_t *surface = test_host_surface();
    const uint8_t *packed = surface->pixels;
    WORD y;

    if (bitmap->pixels == NULL || bitmap->width != (WORD)surface->width ||
        bitmap->height != (WORD)surface->height ||
        bitmap->pitch != (WORD)surface->width) {
        test_bitmap_free(bitmap);
        test_bitmap_init(bitmap, (WORD)surface->width, (WORD)surface->height);
    } else {
        test_bitmap_clear(bitmap, 0);
    }

    for (y = 0; y < (WORD)surface->height; ++y) {
        WORD x;

        for (x = 0; x < (WORD)surface->width; ++x) {
            uint8_t value =
                (packed[(size_t)y * surface->pitch + (size_t)x / 8u] &
                 (uint8_t)(1u << (7u - ((unsigned int)x & 7u)))) != 0u
                    ? 1u
                    : 0u;

            test_bitmap_set_pixel(bitmap, x, y, value);
        }
    }
}

void clear_screen_and_counter(VDI_HANDLE handle)
{
    test_host_reset_present_count();
    v_clrwk(handle);
    test_host_reset_present_count();
}

int assert_surface_matches(const test_bitmap_t *expected)
{
    test_bitmap_t actual;
    WORD y;

    memset(&actual, 0, sizeof(actual));
    snapshot_surface_bitmap(&actual);
    if (test_bitmap_equal(expected, &actual)) {
        test_bitmap_free(&actual);
        return 1;
    }

    for (y = 0; y < expected->height; ++y) {
        WORD x;

        for (x = 0; x < expected->width; ++x) {
            uint8_t lhs = test_bitmap_get_pixel(expected, x, y);
            uint8_t rhs = test_bitmap_get_pixel(&actual, x, y);

            if (lhs != rhs) {
                fprintf(stderr,
                        "framebuffer mismatch at %d,%d expected=%u actual=%u\n",
                        x, y, (unsigned int)lhs, (unsigned int)rhs);
                test_bitmap_free(&actual);
                return 0;
            }
        }
    }
    test_bitmap_free(&actual);
    return 0;
}

void packed_mfdb_init(packed_mfdb_t *packed, WORD width, WORD height)
{
    size_t bytes;

    packed->row_bytes = (WORD)(((width + 15) / 16) * 2);
    bytes = (size_t)packed->row_bytes * (size_t)height;
    packed->bytes = calloc(bytes, 1u);
    memset(&packed->mfdb, 0, sizeof(packed->mfdb));
    packed->mfdb.fd_addr = packed->bytes;
    packed->mfdb.fd_w = width;
    packed->mfdb.fd_h = height;
    packed->mfdb.fd_wdwidth = (WORD)(packed->row_bytes / 2);
    packed->mfdb.fd_stand = 0;
    packed->mfdb.fd_nplanes = 1;
}

void packed_mfdb_free(packed_mfdb_t *packed)
{
    free(packed->bytes);
    packed->bytes = NULL;
}

void packed_mfdb_set_pixel(packed_mfdb_t *packed, WORD x, WORD y, WORD value)
{
    size_t offset;
    UWORD *row;
    UWORD mask;

    if (x < 0 || y < 0 || x >= packed->mfdb.fd_w || y >= packed->mfdb.fd_h) {
        return;
    }

    offset = (size_t)y * (size_t)packed->mfdb.fd_wdwidth + (size_t)x / 16u;
    row = (UWORD *)packed->bytes;
    mask = (UWORD)(0x8000u >> ((unsigned int)x & 15u));
    if (value != 0) {
        row[offset] |= mask;
    } else {
        row[offset] &= (UWORD)~mask;
    }
}

uint8_t packed_mfdb_get_pixel(const packed_mfdb_t *packed, WORD x, WORD y)
{
    size_t offset;
    const UWORD *row;
    UWORD mask;

    if (x < 0 || y < 0 || x >= packed->mfdb.fd_w || y >= packed->mfdb.fd_h) {
        return 0;
    }

    offset = (size_t)y * (size_t)packed->mfdb.fd_wdwidth + (size_t)x / 16u;
    row = (const UWORD *)packed->bytes;
    mask = (UWORD)(0x8000u >> ((unsigned int)x & 15u));
    return (row[offset] & mask) != 0u ? 1u : 0u;
}

void packed_mfdb_to_bitmap(const packed_mfdb_t *packed, test_bitmap_t *bitmap)
{
    WORD y;

    test_bitmap_init(bitmap, packed->mfdb.fd_w, packed->mfdb.fd_h);
    for (y = 0; y < packed->mfdb.fd_h; ++y) {
        WORD x;

        for (x = 0; x < packed->mfdb.fd_w; ++x) {
            test_bitmap_set_pixel(bitmap, x, y,
                                  packed_mfdb_get_pixel(packed, x, y));
        }
    }
}

/* Apply a screen-to-screen copy to the reference through a snapshot, so an
 * overlapping destination is judged against the source as it was. */
void reference_screen_copy(test_bitmap_t *reference, const WORD pxy[8])
{
    test_bitmap_t snapshot;
    WORD y;

    test_bitmap_init(&snapshot, reference->width, reference->height);
    for (y = 0; y < reference->height; ++y) {
        WORD x;

        for (x = 0; x < reference->width; ++x) {
            test_bitmap_set_pixel(&snapshot, x, y,
                                  test_bitmap_get_pixel(reference, x, y));
        }
    }
    test_reference_vro_cpyfm(reference, &snapshot, pxy);
    test_bitmap_free(&snapshot);
}
