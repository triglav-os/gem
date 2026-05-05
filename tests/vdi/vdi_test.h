/*
 * Declares shared helpers for the hosted VDI regression tests. The test
 * harness uses a memory-backed raster surface so framebuffer results can
 * be compared byte-for-byte against reference renderers.
 *
 * MIT License (see: LICENSE)
 * Copyright (C) 2026 tomaz stih
 */

#ifndef GEM_TEST_VDI_H
#define GEM_TEST_VDI_H

#include "gem/vdi.h"
#include "platform/raster.h"

#include <stddef.h>
#include <stdint.h>

enum {
    TEST_VDI_WIDTH = 96,
    TEST_VDI_HEIGHT = 64,
    TEST_VDI_PITCH = TEST_VDI_WIDTH
};

typedef struct test_bitmap {
    WORD width;
    WORD height;
    WORD pitch;
    uint8_t *pixels;
} test_bitmap_t;

typedef struct test_clip_rect {
    WORD enabled;
    WORD xy[4];
} test_clip_rect_t;

void test_bitmap_init(test_bitmap_t *bitmap, WORD width, WORD height);
void test_bitmap_free(test_bitmap_t *bitmap);
void test_bitmap_clear(test_bitmap_t *bitmap, uint8_t value);
uint8_t test_bitmap_get_pixel(const test_bitmap_t *bitmap, WORD x, WORD y);
void test_bitmap_set_pixel(test_bitmap_t *bitmap, WORD x, WORD y, uint8_t value);
int test_bitmap_equal(const test_bitmap_t *left, const test_bitmap_t *right);

gem_raster_surface_t *test_host_surface(void);
int test_host_present_count(void);
void test_host_reset_present_count(void);

void test_reference_pline(test_bitmap_t *bitmap,
                          const test_clip_rect_t *clip,
                          WORD count,
                          const WORD *pxy,
                          uint8_t color);
void test_reference_pmarker(test_bitmap_t *bitmap,
                            const test_clip_rect_t *clip,
                            WORD count,
                            const WORD *xy,
                            uint8_t color);
void test_reference_bar(test_bitmap_t *bitmap,
                        const test_clip_rect_t *clip,
                        const WORD xy[4],
                        uint8_t color);
void test_reference_fillarea(test_bitmap_t *bitmap,
                             const test_clip_rect_t *clip,
                             WORD count,
                             const WORD *xy,
                             uint8_t fill_color,
                             uint8_t border_color,
                             WORD draw_perimeter);
void test_reference_cellarray(test_bitmap_t *bitmap,
                              const test_clip_rect_t *clip,
                              const WORD xy[4],
                              WORD columns,
                              WORD rows,
                              const WORD *colors);
void test_reference_circle(test_bitmap_t *bitmap,
                           const test_clip_rect_t *clip,
                           WORD x,
                           WORD y,
                           WORD radius,
                           uint8_t color);
void test_reference_arc(test_bitmap_t *bitmap,
                        const test_clip_rect_t *clip,
                        WORD x,
                        WORD y,
                        WORD xrad,
                        WORD yrad,
                        WORD begang,
                        WORD endang,
                        uint8_t color);
void test_reference_pieslice(test_bitmap_t *bitmap,
                             const test_clip_rect_t *clip,
                             WORD x,
                             WORD y,
                             WORD xrad,
                             WORD yrad,
                             WORD begang,
                             WORD endang,
                             uint8_t fill_color,
                             uint8_t line_color);
void test_reference_rbox(test_bitmap_t *bitmap,
                         const test_clip_rect_t *clip,
                         const WORD xy[4],
                         uint8_t line_color,
                         uint8_t fill_color,
                         WORD filled,
                         WORD draw_perimeter);
void test_reference_contourfill(test_bitmap_t *bitmap,
                                const test_clip_rect_t *clip,
                                WORD x,
                                WORD y,
                                uint8_t color);
void test_reference_vro_cpyfm(test_bitmap_t *dst,
                              test_bitmap_t *src,
                              const WORD pxy[8]);
void test_reference_vro_cpyfm_clipped(test_bitmap_t *dst,
                                      test_bitmap_t *src,
                                      const WORD pxy[8],
                                      const test_clip_rect_t *clip);
void test_reference_vrt_cpyfm(test_bitmap_t *dst,
                              test_bitmap_t *src,
                              const WORD pxy[8],
                              uint8_t foreground);
void test_reference_vrt_cpyfm_clipped(test_bitmap_t *dst,
                                      test_bitmap_t *src,
                                      const WORD pxy[8],
                                      uint8_t foreground,
                                      const test_clip_rect_t *clip);

void test_optimized_pline(test_bitmap_t *bitmap,
                          const test_clip_rect_t *clip,
                          WORD count,
                          const WORD *pxy,
                          uint8_t color);
void test_optimized_fillarea(test_bitmap_t *bitmap,
                             const test_clip_rect_t *clip,
                             WORD count,
                             const WORD *xy,
                             uint8_t color);
void test_optimized_circle(test_bitmap_t *bitmap,
                           const test_clip_rect_t *clip,
                           WORD x,
                           WORD y,
                           WORD radius,
                           uint8_t color);
void test_optimized_bar(test_bitmap_t *bitmap,
                        const test_clip_rect_t *clip,
                        const WORD xy[4],
                        uint8_t color);

#endif
