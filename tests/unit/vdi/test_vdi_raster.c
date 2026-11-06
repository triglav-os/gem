/*
 * VDI unit tests for raster output: polylines, bars and markers, clipping,
 * filled areas and cell arrays, arcs and ellipses, contour fills and raster
 * copies, write modes, and the WORD-limit coordinate and scroll cases.
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

int test_polyline_bar_and_marker(void)
{
    VDI_HANDLE handle = open_handle();
    WORD line_points[] = {2, 2, 20, 12, 4, 18, 31, 6};
    WORD patterned_line[] = {2, 24, 33, 24};
    WORD marker_points[] = {10, 10, 16, 14, 22, 8};
    WORD bar_xy[] = {30, 4, 42, 18};
    test_bitmap_t reference;
    test_bitmap_t optimized;

    ASSERT_TRUE(handle == 1);
    test_bitmap_init(&reference, TEST_VDI_WIDTH, TEST_VDI_HEIGHT);
    test_bitmap_init(&optimized, TEST_VDI_WIDTH, TEST_VDI_HEIGHT);
    vsl_color(handle, 0);
    vsf_color(handle, 0);
    vsm_color(handle, 0);
    clear_screen_and_counter(handle);

    test_reference_pline(&reference, NULL, 4, line_points, 1);
    test_optimized_pline(&optimized, NULL, 4, line_points, 1);
    ASSERT_TRUE(test_bitmap_equal(&reference, &optimized));
    v_pline(handle, 4, line_points);
    ASSERT_TRUE(assert_surface_matches(&reference));
    ASSERT_TRUE(test_host_present_count() == 1);

    clear_screen_and_counter(handle);
    ASSERT_TRUE(vsl_type(handle, 7) == 7);
    ASSERT_TRUE(vsl_udsty(handle, (WORD)0xaaaau) == (WORD)0xaaaau);
    v_pline(handle, 2, patterned_line);
    snapshot_surface_bitmap(&optimized);
    for (WORD x = patterned_line[0]; x <= patterned_line[2]; ++x) {
        ASSERT_TRUE(test_bitmap_get_pixel(&optimized, x, patterned_line[1]) ==
                    (uint8_t)(((x - patterned_line[0]) & 1) == 0));
    }
    ASSERT_TRUE(vsl_type(handle, 1) == 1);

    clear_screen_and_counter(handle);
    test_bitmap_clear(&reference, 0);
    test_bitmap_clear(&optimized, 0);
    test_reference_bar(&reference, NULL, bar_xy, 1);
    test_optimized_bar(&optimized, NULL, bar_xy, 1);
    ASSERT_TRUE(test_bitmap_equal(&reference, &optimized));
    v_bar(handle, bar_xy);
    ASSERT_TRUE(assert_surface_matches(&reference));

    clear_screen_and_counter(handle);
    test_bitmap_clear(&reference, 0);
    test_reference_pmarker(&reference, NULL, 3, marker_points, 1);
    v_pmarker(handle, 3, marker_points);
    ASSERT_TRUE(assert_surface_matches(&reference));

    test_bitmap_free(&reference);
    test_bitmap_free(&optimized);
    v_clsvwk(handle);
    return 1;
}

int test_clipping_and_output_window(void)
{
    VDI_HANDLE handle = open_handle();
    WORD clip_xy[] = {8, 8, 20, 20};
    WORD bar_xy[] = {0, 0, 30, 30};
    WORD line_xy[] = {0, 12, 40, 12};
    test_bitmap_t reference;
    test_clip_rect_t clip = {1, {8, 8, 20, 20}};

    ASSERT_TRUE(handle == 1);
    test_bitmap_init(&reference, TEST_VDI_WIDTH, TEST_VDI_HEIGHT);
    vsl_color(handle, 0);
    vsf_color(handle, 0);
    clear_screen_and_counter(handle);

    vs_clip(handle, 1, clip_xy);
    test_reference_bar(&reference, &clip, bar_xy, 1);
    v_bar(handle, bar_xy);
    ASSERT_TRUE(assert_surface_matches(&reference));

    clear_screen_and_counter(handle);
    test_bitmap_clear(&reference, 0);
    test_reference_pline(&reference, &clip, 2, line_xy, 1);
    v_pline(handle, 2, line_xy);
    ASSERT_TRUE(assert_surface_matches(&reference));

    clear_screen_and_counter(handle);
    test_bitmap_clear(&reference, 0);
    v_output_window(handle, clip_xy);
    test_reference_bar(&reference, &clip, bar_xy, 1);
    v_bar(handle, bar_xy);
    ASSERT_TRUE(assert_surface_matches(&reference));

    test_bitmap_free(&reference);
    v_clsvwk(handle);
    return 1;
}

int test_fillarea_and_cellarray(void)
{
    VDI_HANDLE handle = open_handle();
    WORD polygon[] = {10, 10, 30, 12, 36, 24, 18, 30, 8, 18};
    WORD cell_xy[] = {40, 8, 63, 23};
    WORD colors[] = {1, 0, 1, 0, 1, 1};
    test_bitmap_t reference;
    test_bitmap_t optimized;

    ASSERT_TRUE(handle == 1);
    test_bitmap_init(&reference, TEST_VDI_WIDTH, TEST_VDI_HEIGHT);
    test_bitmap_init(&optimized, TEST_VDI_WIDTH, TEST_VDI_HEIGHT);
    vsl_color(handle, 0);
    vsf_color(handle, 0);
    vsf_perimeter(handle, 0);

    clear_screen_and_counter(handle);
    test_reference_fillarea(&reference, NULL, 5, polygon, 1, 1, 0);
    test_optimized_fillarea(&optimized, NULL, 5, polygon, 1);
    v_fillarea(handle, 5, polygon);
    ASSERT_TRUE(assert_surface_matches(&reference));
    ASSERT_TRUE(memchr(optimized.pixels, 1,
                       (size_t)optimized.pitch * (size_t)optimized.height) !=
                NULL);

    clear_screen_and_counter(handle);
    test_bitmap_clear(&reference, 0);
    test_reference_cellarray(&reference, NULL, cell_xy, 3, 2, colors);
    ASSERT_TRUE(v_cellarray(handle, cell_xy, 3, 3, 2, 1, colors) == 1);
    ASSERT_TRUE(assert_surface_matches(&reference));

    test_bitmap_free(&reference);
    test_bitmap_free(&optimized);
    v_clsvwk(handle);
    return 1;
}

int test_circle_arc_ellipse_family(void)
{
    VDI_HANDLE handle = open_handle();
    test_bitmap_t reference;
    test_bitmap_t optimized;
    WORD rbox_xy[] = {50, 8, 76, 28};

    ASSERT_TRUE(handle == 1);
    test_bitmap_init(&reference, TEST_VDI_WIDTH, TEST_VDI_HEIGHT);
    test_bitmap_init(&optimized, TEST_VDI_WIDTH, TEST_VDI_HEIGHT);
    vsl_color(handle, 0);
    vsf_color(handle, 0);

    clear_screen_and_counter(handle);
    test_reference_circle(&reference, NULL, 16, 16, 8, 1);
    test_optimized_circle(&optimized, NULL, 16, 16, 8, 1);
    ASSERT_TRUE(test_bitmap_equal(&reference, &optimized));
    v_circle(handle, 16, 16, 8);
    ASSERT_TRUE(assert_surface_matches(&reference));

    clear_screen_and_counter(handle);
    test_bitmap_clear(&reference, 0);
    test_reference_arc(&reference, NULL, 18, 18, 10, 10, 0, 900, 1);
    v_arc(handle, 18, 18, 10, 0, 900);
    ASSERT_TRUE(assert_surface_matches(&reference));

    clear_screen_and_counter(handle);
    test_bitmap_clear(&reference, 0);
    test_reference_pieslice(&reference, NULL, 20, 20, 10, 10, 0, 900, 1, 1);
    v_pieslice(handle, 20, 20, 10, 0, 900);
    ASSERT_TRUE(assert_surface_matches(&reference));

    clear_screen_and_counter(handle);
    test_bitmap_clear(&reference, 0);
    test_reference_arc(&reference, NULL, 20, 40, 14, 8, 0, 3599, 1);
    v_ellipse(handle, 20, 40, 14, 8);
    ASSERT_TRUE(assert_surface_matches(&reference));

    clear_screen_and_counter(handle);
    test_bitmap_clear(&reference, 0);
    test_reference_arc(&reference, NULL, 20, 40, 14, 8, 900, 1800, 1);
    v_ellarc(handle, 20, 40, 14, 8, 900, 1800);
    ASSERT_TRUE(assert_surface_matches(&reference));

    clear_screen_and_counter(handle);
    test_bitmap_clear(&reference, 0);
    test_reference_pieslice(&reference, NULL, 20, 40, 14, 8, 1800, 2700, 1, 1);
    v_ellpie(handle, 20, 40, 14, 8, 1800, 2700);
    ASSERT_TRUE(assert_surface_matches(&reference));

    clear_screen_and_counter(handle);
    test_bitmap_clear(&reference, 0);
    test_reference_rbox(&reference, NULL, rbox_xy, 1, 1, 0, 1);
    v_rbox(handle, rbox_xy);
    ASSERT_TRUE(assert_surface_matches(&reference));

    clear_screen_and_counter(handle);
    test_bitmap_clear(&reference, 0);
    test_reference_rbox(&reference, NULL, rbox_xy, 1, 1, 1, 1);
    v_rfbox(handle, rbox_xy);
    ASSERT_TRUE(assert_surface_matches(&reference));

    test_bitmap_free(&reference);
    test_bitmap_free(&optimized);
    v_clsvwk(handle);
    return 1;
}

int test_contourfill_and_blits(void)
{
    VDI_HANDLE handle = open_handle();
    WORD frame_xy[] = {10, 10, 30, 24};
    WORD copy_xy[] = {10, 10, 30, 24, 40, 12, 60, 26};
    WORD copy_clip_xy[] = {45, 14, 55, 20};
    WORD transparent_xy[] = {0, 0, 15, 15, 8, 8, 23, 23};
    WORD colors[2] = {0, 1};
    packed_mfdb_t source;
    packed_mfdb_t target;
    test_bitmap_t reference;
    test_bitmap_t source_bitmap;
    test_bitmap_t target_bitmap;
    test_clip_rect_t copy_clip = {1, {45, 14, 55, 20}};

    ASSERT_TRUE(handle == 1);
    test_bitmap_init(&reference, TEST_VDI_WIDTH, TEST_VDI_HEIGHT);
    vsl_color(handle, 0);
    vsf_color(handle, 0);

    clear_screen_and_counter(handle);
    v_rbox(handle, frame_xy);
    test_reference_rbox(&reference, NULL, frame_xy, 1, 1, 0, 1);
    test_reference_contourfill(&reference, NULL, 20, 18, 1);
    v_contourfill(handle, 20, 18, 0);
    ASSERT_TRUE(assert_surface_matches(&reference));

    clear_screen_and_counter(handle);
    test_bitmap_clear(&reference, 0);
    test_reference_bar(&reference, NULL, frame_xy, 1);
    v_bar(handle, frame_xy);
    test_reference_vro_cpyfm(&reference, &reference, copy_xy);
    vro_cpyfm(handle, 1, copy_xy, NULL, NULL);
    ASSERT_TRUE(assert_surface_matches(&reference));

    clear_screen_and_counter(handle);
    test_bitmap_clear(&reference, 0);
    test_reference_bar(&reference, NULL, frame_xy, 1);
    v_bar(handle, frame_xy);
    vs_clip(handle, 1, copy_clip_xy);
    test_reference_vro_cpyfm_clipped(&reference, &reference, copy_xy,
                                     &copy_clip);
    vro_cpyfm(handle, 1, copy_xy, NULL, NULL);
    ASSERT_TRUE(assert_surface_matches(&reference));
    vs_clip(handle, 0, copy_clip_xy);

    packed_mfdb_init(&source, 16, 16);
    packed_mfdb_init(&target, 16, 16);
    packed_mfdb_set_pixel(&source, 2, 2, 1);
    packed_mfdb_set_pixel(&source, 4, 4, 1);
    packed_mfdb_set_pixel(&source, 8, 8, 1);
    packed_mfdb_to_bitmap(&source, &source_bitmap);
    packed_mfdb_to_bitmap(&target, &target_bitmap);
    test_reference_vrt_cpyfm(&target_bitmap, &source_bitmap, transparent_xy,
                             test_vdi_color_to_pixel(colors[1]));
    vrt_cpyfm(handle, 1, transparent_xy, &source.mfdb, &target.mfdb, colors);
    {
        test_bitmap_t actual_target;
        WORD y;

        packed_mfdb_to_bitmap(&target, &actual_target);
        if (!test_bitmap_equal(&target_bitmap, &actual_target)) {
            for (y = 0; y < target_bitmap.height; ++y) {
                WORD x;

                for (x = 0; x < target_bitmap.width; ++x) {
                    uint8_t expected =
                        test_bitmap_get_pixel(&target_bitmap, x, y);
                    uint8_t actual =
                        test_bitmap_get_pixel(&actual_target, x, y);

                    if (expected != actual) {
                        fprintf(stderr,
                                "mfdb transparent mismatch at %d,%d "
                                "expected=%u actual=%u\n",
                                x, y, (unsigned int)expected,
                                (unsigned int)actual);
                        break;
                    }
                }
            }
            ASSERT_TRUE(0);
        }
        test_bitmap_free(&actual_target);
    }
    packed_mfdb_free(&source);
    packed_mfdb_free(&target);

    test_bitmap_free(&reference);
    test_bitmap_free(&source_bitmap);
    test_bitmap_free(&target_bitmap);
    v_clsvwk(handle);
    return 1;
}

int test_vrt_cpyfm_glyph_bitmap(void)
{
    VDI_HANDLE handle = open_handle();
    packed_mfdb_t source;
    packed_mfdb_t target;
    test_bitmap_t source_bitmap;
    test_bitmap_t expected_target;
    WORD pxy[] = {0, 0, 12, 15, 20, 10, 32, 25};
    WORD colors[2] = {0, 1};
    WORD y;
    static const char *rows[] = {
        "....####.....", "....####.....", "....####.....", "....####.....",
        "....####.....", "....####.....", "....####.....", "....####.....",
        "....####.....", "....####.....", "....####.....", "....####.....",
        "....####.....", "....####.....", "....####.....", "....####....."};

    ASSERT_TRUE(handle == 1);
    packed_mfdb_init(&source, 13, 16);
    packed_mfdb_init(&target, TEST_VDI_WIDTH, TEST_VDI_HEIGHT);
    for (y = 0; y < 16; ++y) {
        WORD x;

        for (x = 0; x < 13; ++x) {
            if (rows[y][x] == '#') {
                packed_mfdb_set_pixel(&source, x, y, 1);
            }
        }
    }

    packed_mfdb_to_bitmap(&source, &source_bitmap);
    test_bitmap_init(&expected_target, TEST_VDI_WIDTH, TEST_VDI_HEIGHT);
    test_reference_vrt_cpyfm(&expected_target, &source_bitmap, pxy,
                             test_vdi_color_to_pixel(colors[1]));

    clear_screen_and_counter(handle);
    vrt_cpyfm(handle, 1, pxy, &source.mfdb, NULL, colors);
    ASSERT_TRUE(assert_surface_matches(&expected_target));

    vrt_cpyfm(handle, 1, pxy, &source.mfdb, &target.mfdb, colors);
    {
        test_bitmap_t actual_target;

        packed_mfdb_to_bitmap(&target, &actual_target);
        ASSERT_TRUE(test_bitmap_equal(&expected_target, &actual_target));
        test_bitmap_free(&actual_target);
    }

    packed_mfdb_free(&source);
    packed_mfdb_free(&target);
    test_bitmap_free(&source_bitmap);
    test_bitmap_free(&expected_target);
    v_clsvwk(handle);
    return 1;
}

int test_write_modes(void)
{
    VDI_HANDLE handle = open_handle();
    WORD box_xy[] = {8, 8, 24, 20};
    WORD copy_xy[] = {0, 0, 7, 7, 0, 0, 7, 7};
    packed_mfdb_t source;
    packed_mfdb_t target;

    ASSERT_TRUE(handle == 1);
    vsl_color(handle, 0);
    vsf_color(handle, 0);

    clear_screen_and_counter(handle);
    ASSERT_TRUE(vswr_mode(handle, 1) == 1);
    v_bar(handle, box_xy);
    ASSERT_TRUE(vswr_mode(handle, 3) == 3);
    v_bar(handle, box_xy);

    {
        WORD pel = 0;
        WORD index = 0;

        ASSERT_TRUE(v_get_pixel(handle, 12, 12, &pel, &index) == 1);
        ASSERT_TRUE(pel == 0 && index == 0);
    }

    ASSERT_TRUE(vswr_mode(handle, 1) == 1);
    v_bar(handle, box_xy);
    {
        WORD pel = 0;
        WORD index = 0;

        ASSERT_TRUE(v_get_pixel(handle, 12, 12, &pel, &index) == 1);
        ASSERT_TRUE(pel == 1 && index == 1);
    }
    ASSERT_TRUE(vswr_mode(handle, 4) == 4);
    v_bar(handle, box_xy);
    {
        WORD pel = 0;
        WORD index = 0;

        ASSERT_TRUE(v_get_pixel(handle, 12, 12, &pel, &index) == 1);
        ASSERT_TRUE(pel == 0 && index == 0);
    }

    packed_mfdb_init(&source, 8, 8);
    packed_mfdb_init(&target, 8, 8);
    packed_mfdb_set_pixel(&source, 1, 1, 1);
    packed_mfdb_set_pixel(&source, 2, 2, 1);
    packed_mfdb_set_pixel(&target, 1, 1, 1);
    packed_mfdb_set_pixel(&target, 3, 3, 1);
    vro_cpyfm(handle, 6, copy_xy, &source.mfdb, &target.mfdb);
    ASSERT_TRUE(packed_mfdb_get_pixel(&target, 1, 1) == 0);
    ASSERT_TRUE(packed_mfdb_get_pixel(&target, 2, 2) == 1);
    ASSERT_TRUE(packed_mfdb_get_pixel(&target, 3, 3) == 1);
    packed_mfdb_free(&source);
    packed_mfdb_free(&target);

    v_clsvwk(handle);
    return 1;
}

/*
 * Corners at the WORD limits used to wrap the copy width to zero (a
 * division by zero) and text near the right edge of the coordinate space
 * wrapped past the clip test into the row buffer. Both are reachable from
 * an RPC client, so they must render harmlessly. The screen scroll cases
 * cover the byte-copy fast path: a shift by whole bytes, an overlapping
 * downward move and unaligned edges each have to match the pixel model.
 */
int test_extreme_coordinates_and_screen_scroll(void)
{
    VDI_HANDLE handle = open_handle();
    WORD origin_xy[] = {0, 0, 0, 0};
    WORD span_xy[] = {0, 0, 0, 0, -32768, 0, 32767, 0};
    WORD tall_xy[] = {0, 0, 0, 0, 0, -32768, 0, 32767};
    WORD bar_xy[] = {8, 10, 15, 12};
    WORD shift_xy[] = {8, 10, 15, 12, 24, 10, 31, 12};
    WORD row_xy[] = {0, 20, 95, 20};
    WORD scroll_xy[] = {0, 20, 95, 29, 0, 22, 95, 31};
    WORD byte_xy[] = {8, 40, 15, 40};
    WORD edge_xy[] = {9, 40, 14, 40, 25, 40, 30, 40};
    test_bitmap_t reference;
    WORD x;

    ASSERT_TRUE(handle == 1);
    test_bitmap_init(&reference, TEST_VDI_WIDTH, TEST_VDI_HEIGHT);
    vsl_color(handle, 0);
    vsf_color(handle, 0);
    vst_color(handle, 0);
    ASSERT_TRUE(vswr_mode(handle, 1) == 1);

    clear_screen_and_counter(handle);
    v_bar(handle, origin_xy);
    test_reference_bar(&reference, NULL, origin_xy, 1);
    vro_cpyfm(handle, 1, span_xy, NULL, NULL);
    vro_cpyfm(handle, 1, tall_xy, NULL, NULL);
    for (x = 0; x < TEST_VDI_WIDTH; ++x) {
        test_bitmap_set_pixel(&reference, x, 0, 1);
    }
    for (x = 0; x < TEST_VDI_HEIGHT; ++x) {
        test_bitmap_set_pixel(&reference, 0, x, 1);
    }
    ASSERT_TRUE(assert_surface_matches(&reference));

    clear_screen_and_counter(handle);
    test_bitmap_clear(&reference, 0);
    v_gtext(handle, 32760, 20, (BYTE *)"MMMM");
    v_gtext(handle, 32767, 20, (BYTE *)"MMMM");
    v_gtext(handle, -32768, 20, (BYTE *)"MMMM");
    v_gtext(handle, -20, 20, (BYTE *)"");
    ASSERT_TRUE(assert_surface_matches(&reference));

    clear_screen_and_counter(handle);
    test_bitmap_clear(&reference, 0);
    v_bar(handle, bar_xy);
    test_reference_bar(&reference, NULL, bar_xy, 1);
    vro_cpyfm(handle, 1, shift_xy, NULL, NULL);
    reference_screen_copy(&reference, shift_xy);
    ASSERT_TRUE(assert_surface_matches(&reference));

    clear_screen_and_counter(handle);
    test_bitmap_clear(&reference, 0);
    v_bar(handle, row_xy);
    test_reference_bar(&reference, NULL, row_xy, 1);
    vro_cpyfm(handle, 1, scroll_xy, NULL, NULL);
    reference_screen_copy(&reference, scroll_xy);
    ASSERT_TRUE(assert_surface_matches(&reference));

    clear_screen_and_counter(handle);
    test_bitmap_clear(&reference, 0);
    v_bar(handle, byte_xy);
    test_reference_bar(&reference, NULL, byte_xy, 1);
    vro_cpyfm(handle, 1, edge_xy, NULL, NULL);
    reference_screen_copy(&reference, edge_xy);
    ASSERT_TRUE(assert_surface_matches(&reference));

    test_bitmap_free(&reference);
    v_clsvwk(handle);
    return 1;
}
