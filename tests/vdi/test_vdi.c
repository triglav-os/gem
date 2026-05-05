/*
 * Exercises the hosted VDI implementation against memory-backed
 * reference renderers and public-state queries. The goal is to catch
 * framebuffer regressions before they surface as higher-level AES bugs.
 *
 * MIT License (see: LICENSE)
 * Copyright (C) 2026 tomaz stih
 */

#define _POSIX_C_SOURCE 200809L

#include "vdi_test.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define ASSERT_TRUE(expr) \
    do { \
        if (!(expr)) { \
            fprintf(stderr, "assertion failed: %s at %s:%d\n", #expr, \
                __FILE__, __LINE__); \
            return 0; \
        } \
    } while (0)

typedef struct packed_mfdb {
    MFDB mfdb;
    uint8_t *bytes;
    WORD row_bytes;
} packed_mfdb_t;

static VDI_HANDLE open_handle(void)
{
    WORD work_in[11] = { 0 };
    WORD work_out[57] = { 0 };
    VDI_HANDLE handle = 0;

    setenv("GEM_VDI_WIDTH", "96", 1);
    setenv("GEM_VDI_HEIGHT", "64", 1);
    v_opnvwk(work_in, &handle, work_out);
    return handle;
}

static void snapshot_surface_bitmap(test_bitmap_t *bitmap)
{
    gem_raster_surface_t *surface = test_host_surface();
    const uint8_t *packed = surface->pixels;
    WORD y;

    if (bitmap->pixels == NULL || bitmap->width != (WORD) surface->width ||
        bitmap->height != (WORD) surface->height ||
        bitmap->pitch != (WORD) surface->width) {
        test_bitmap_free(bitmap);
        test_bitmap_init(bitmap, (WORD) surface->width, (WORD) surface->height);
    } else {
        test_bitmap_clear(bitmap, 0);
    }

    for (y = 0; y < (WORD) surface->height; ++y) {
        WORD x;

        for (x = 0; x < (WORD) surface->width; ++x) {
            uint8_t value =
                (packed[(size_t) y * surface->pitch + (size_t) x / 8u] &
                (uint8_t) (1u << (7u - ((unsigned int) x & 7u)))) != 0u ?
                1u : 0u;

            test_bitmap_set_pixel(bitmap, x, y, value);
        }
    }
}

static void clear_screen_and_counter(VDI_HANDLE handle)
{
    test_host_reset_present_count();
    v_clrwk(handle);
    test_host_reset_present_count();
}

static int assert_surface_matches(const test_bitmap_t *expected)
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
                fprintf(stderr, "framebuffer mismatch at %d,%d expected=%u actual=%u\n",
                    x, y, (unsigned int) lhs, (unsigned int) rhs);
                test_bitmap_free(&actual);
                return 0;
            }
        }
    }
    test_bitmap_free(&actual);
    return 0;
}

static void packed_mfdb_init(packed_mfdb_t *packed, WORD width, WORD height)
{
    size_t bytes;

    packed->row_bytes = (WORD) (((width + 15) / 16) * 2);
    bytes = (size_t) packed->row_bytes * (size_t) height;
    packed->bytes = calloc(bytes, 1u);
    memset(&packed->mfdb, 0, sizeof(packed->mfdb));
    packed->mfdb.fd_addr = packed->bytes;
    packed->mfdb.fd_w = width;
    packed->mfdb.fd_h = height;
    packed->mfdb.fd_wdwidth = (WORD) (packed->row_bytes / 2);
    packed->mfdb.fd_stand = 0;
    packed->mfdb.fd_nplanes = 1;
}

static void packed_mfdb_free(packed_mfdb_t *packed)
{
    free(packed->bytes);
    packed->bytes = NULL;
}

static void packed_mfdb_set_pixel(packed_mfdb_t *packed, WORD x, WORD y, WORD value)
{
    size_t offset;
    uint8_t mask;

    if (x < 0 || y < 0 || x >= packed->mfdb.fd_w || y >= packed->mfdb.fd_h) {
        return;
    }

    offset = (size_t) y * (size_t) packed->row_bytes + (size_t) x / 8u;
    mask = (uint8_t) (1u << (7 - ((unsigned int) x & 7u)));
    if (value != 0) {
        packed->bytes[offset] |= mask;
    } else {
        packed->bytes[offset] &= (uint8_t) ~mask;
    }
}

static uint8_t packed_mfdb_get_pixel(const packed_mfdb_t *packed, WORD x, WORD y)
{
    size_t offset;
    uint8_t mask;

    if (x < 0 || y < 0 || x >= packed->mfdb.fd_w || y >= packed->mfdb.fd_h) {
        return 0;
    }

    offset = (size_t) y * (size_t) packed->row_bytes + (size_t) x / 8u;
    mask = (uint8_t) (1u << (7 - ((unsigned int) x & 7u)));
    return (packed->bytes[offset] & mask) != 0u ? 1u : 0u;
}

static void packed_mfdb_to_bitmap(const packed_mfdb_t *packed, test_bitmap_t *bitmap)
{
    WORD y;

    test_bitmap_init(bitmap, packed->mfdb.fd_w, packed->mfdb.fd_h);
    for (y = 0; y < packed->mfdb.fd_h; ++y) {
        WORD x;

        for (x = 0; x < packed->mfdb.fd_w; ++x) {
            test_bitmap_set_pixel(bitmap, x, y, packed_mfdb_get_pixel(packed, x, y));
        }
    }
}

static int test_open_close(void)
{
    WORD work_in[11] = { 0 };
    WORD work_out[57] = { 0 };
    VDI_HANDLE handle = 0;

    setenv("GEM_VDI_WIDTH", "96", 1);
    setenv("GEM_VDI_HEIGHT", "64", 1);
    v_opnvwk(work_in, &handle, work_out);
    ASSERT_TRUE(handle == 1);
    ASSERT_TRUE(work_out[0] == TEST_VDI_WIDTH - 1);
    ASSERT_TRUE(work_out[1] == TEST_VDI_HEIGHT - 1);
    v_clsvwk(handle);
    return 1;
}

static int test_polyline_bar_and_marker(void)
{
    VDI_HANDLE handle = open_handle();
    WORD line_points[] = { 2, 2, 20, 12, 4, 18, 31, 6 };
    WORD marker_points[] = { 10, 10, 16, 14, 22, 8 };
    WORD bar_xy[] = { 30, 4, 42, 18 };
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

static int test_clipping_and_output_window(void)
{
    VDI_HANDLE handle = open_handle();
    WORD clip_xy[] = { 8, 8, 20, 20 };
    WORD bar_xy[] = { 0, 0, 30, 30 };
    WORD line_xy[] = { 0, 12, 40, 12 };
    test_bitmap_t reference;
    test_clip_rect_t clip = { 1, { 8, 8, 20, 20 } };

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

static int test_fillarea_and_cellarray(void)
{
    VDI_HANDLE handle = open_handle();
    WORD polygon[] = { 10, 10, 30, 12, 36, 24, 18, 30, 8, 18 };
    WORD cell_xy[] = { 40, 8, 63, 23 };
    WORD colors[] = { 1, 0, 1, 0, 1, 1 };
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
        (size_t) optimized.pitch * (size_t) optimized.height) != NULL);

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

static int test_circle_arc_ellipse_family(void)
{
    VDI_HANDLE handle = open_handle();
    test_bitmap_t reference;
    test_bitmap_t optimized;
    WORD rbox_xy[] = { 50, 8, 76, 28 };

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

static int test_contourfill_and_blits(void)
{
    VDI_HANDLE handle = open_handle();
    WORD frame_xy[] = { 10, 10, 30, 24 };
    WORD copy_xy[] = { 10, 10, 30, 24, 40, 12, 60, 26 };
    WORD copy_clip_xy[] = { 45, 14, 55, 20 };
    WORD transparent_xy[] = { 0, 0, 15, 15, 8, 8, 23, 23 };
    WORD colors[2] = { 0, 1 };
    packed_mfdb_t source;
    packed_mfdb_t target;
    test_bitmap_t reference;
    test_bitmap_t source_bitmap;
    test_bitmap_t target_bitmap;
    test_clip_rect_t copy_clip = { 1, { 45, 14, 55, 20 } };

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
    test_reference_vrt_cpyfm(&target_bitmap, &source_bitmap, transparent_xy, 1);
    vrt_cpyfm(handle, 1, transparent_xy, &source.mfdb, &target.mfdb, colors);
    {
        test_bitmap_t actual_target;

        packed_mfdb_to_bitmap(&target, &actual_target);
        ASSERT_TRUE(test_bitmap_equal(&target_bitmap, &actual_target));
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

static int test_write_modes(void)
{
    VDI_HANDLE handle = open_handle();
    WORD box_xy[] = { 8, 8, 24, 20 };
    WORD copy_xy[] = { 0, 0, 7, 7, 0, 0, 7, 7 };
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

static int test_attribute_and_query_functions(void)
{
    VDI_HANDLE handle = open_handle();
    WORD rgb[3] = { 100, 200, 300 };
    WORD out_rgb[3] = { 0 };
    WORD attrib[57] = { 0 };
    WORD extent[8] = { 0 };
    WORD charw = 0;
    WORD charh = 0;
    WORD cellw = 0;
    WORD cellh = 0;
    WORD rows = 0;
    WORD columns = 0;
    WORD status = 0;
    WORD font_count = 0;
    BYTE name[32] = { 0 };
    WORD distances[5] = { 0 };
    WORD effects[3] = { 0 };
    MFORM form;
    WORD pattern = (WORD) 0xaaaa;

    ASSERT_TRUE(handle == 1);
    memset(&form, 0, sizeof(form));
    vsl_color(handle, 0);
    vsf_color(handle, 0);
    vst_color(handle, 0);

    ASSERT_TRUE(vst_height(handle, 11, &charw, &charh, &cellw, &cellh) == 11);
    ASSERT_TRUE(charw > 0 && charh == 11);
    ASSERT_TRUE(vst_rotation(handle, 900) == 900);
    ASSERT_TRUE(vst_font(handle, 3) == 3);
    ASSERT_TRUE(vst_alignment(handle, 1, 2) == 1);
    ASSERT_TRUE(vst_background(handle, 1) == 1);
    ASSERT_TRUE(vst_effects(handle, 2) == 2);
    ASSERT_TRUE(vst_point(handle, 9, &charw, &charh, &cellw, &cellh) == 9);
    font_count = vst_load_fonts(handle, 0);
    ASSERT_TRUE(font_count >= 2);

    ASSERT_TRUE(vsl_type(handle, 3) == 3);
    ASSERT_TRUE(vsl_width(handle, 2) == 2);
    ASSERT_TRUE(vsl_ends(handle, 1, 2) == 1);
    ASSERT_TRUE(vsl_end_style(handle, 2, 3) == 1);
    ASSERT_TRUE(vsl_udsty(handle, pattern) == pattern);

    ASSERT_TRUE(vsm_type(handle, 4) == 4);
    ASSERT_TRUE(vsm_height(handle, 7) == 7);
    ASSERT_TRUE(vsm_color(handle, 1) == 1);

    ASSERT_TRUE(vsf_interior(handle, 2) == 2);
    ASSERT_TRUE(vsf_style(handle, 3) == 3);
    ASSERT_TRUE(vsf_perimeter(handle, 1) == 1);
    ASSERT_TRUE(vsf_udpat(handle, attrib, 1) == 1);

    ASSERT_TRUE(vswr_mode(handle, 1) == 1);
    ASSERT_TRUE(vsin_mode(handle, 0, 1) == 1);
    ASSERT_TRUE(vqin_mode(handle, 0, &status) == 1);
    ASSERT_TRUE(status == 1);

    ASSERT_TRUE(vs_color(handle, 2, rgb) == 1);
    ASSERT_TRUE(vq_color(handle, 2, 0, out_rgb) == 1);
    ASSERT_TRUE(out_rgb[0] == 100 && out_rgb[1] == 200 && out_rgb[2] == 300);

    ASSERT_TRUE(vql_attributes(handle, attrib) == 1);
    ASSERT_TRUE(attrib[0] == 3 && attrib[3] == 2);
    ASSERT_TRUE(vqm_attributes(handle, attrib) == 1);
    ASSERT_TRUE(attrib[0] == 4 && attrib[3] == 7);
    ASSERT_TRUE(vqf_attributes(handle, attrib) == 1);
    ASSERT_TRUE(attrib[0] == 2 && attrib[2] == 3);
    ASSERT_TRUE(vqt_attributes(handle, attrib) == 1);
    ASSERT_TRUE(attrib[0] == 3 && attrib[2] == 900);

    ASSERT_TRUE(vq_extnd(handle, 0, attrib) == 1);
    ASSERT_TRUE(attrib[0] == TEST_VDI_WIDTH - 1);
    ASSERT_TRUE(vqt_extent(handle, "TEST", extent) == 1);
    ASSERT_TRUE(extent[2] >= 0);
    ASSERT_TRUE(vqt_width(handle, 'A', &charw, NULL, NULL) > 0);
    ASSERT_TRUE(vq_chcells(handle, &rows, &columns) == 1);
    ASSERT_TRUE(rows > 0 && columns > 0);
    ASSERT_TRUE(vq_key_s(handle, &status) == 1 && status == 0);
    ASSERT_TRUE(vqt_name(handle, 1, name) == 1);
    ASSERT_TRUE(name[0] != '\0');
    ASSERT_TRUE(vqt_name(handle, 2, name) == 2);
    ASSERT_TRUE(name[0] != '\0');
    ASSERT_TRUE(vqt_name(handle, font_count, name) != 0);
    ASSERT_TRUE(name[0] != '\0');
    ASSERT_TRUE(vqt_fontinfo(handle, NULL, NULL, distances, &charw, effects) == 1);
    ASSERT_TRUE(charw > 0);
    ASSERT_TRUE(vst_unload_fonts(handle, 0) == 0);
    ASSERT_TRUE(vqt_name(handle, 1, name) == 1);
    ASSERT_TRUE(vqt_name(handle, 2, name) == 2);
    ASSERT_TRUE(vqt_name(handle, 3, name) == 0);

    ASSERT_TRUE(vsc_form(handle, &form) == 1);
    ASSERT_TRUE(vex_timv(handle, NULL, NULL, &status) == 1);
    ASSERT_TRUE(vex_butv(handle, NULL, NULL) == 1);
    ASSERT_TRUE(vex_motv(handle, NULL, NULL) == 1);
    ASSERT_TRUE(vex_curv(handle, NULL, NULL) == 1);
    ASSERT_TRUE(vm_filename(handle, (BYTE *) "meta.out") == 1);
    ASSERT_TRUE(vs_palette(handle, 1) == 1);
    ASSERT_TRUE(v_meta_extents(handle, 0, 0, 10, 10) == 1);
    ASSERT_TRUE(v_write_meta(handle, 0, NULL, 0, NULL) == 1);
    ASSERT_TRUE(v_escape(handle, 0, 0, 0, NULL, NULL) == 1);
    ASSERT_TRUE(v_hardcopy(handle) == 1);
    ASSERT_TRUE(vq_tabstatus(handle) == 1);
    ASSERT_TRUE(v_form_adv(handle) == 1);
    ASSERT_TRUE(v_clear_disp_list(handle) == 1);
    ASSERT_TRUE(v_exit_cur(handle) == 1);
    ASSERT_TRUE(v_enter_cur(handle) == 1);
    ASSERT_TRUE(v_rmcur(handle) == 1);

    v_clsvwk(handle);
    return 1;
}

static int test_cursor_and_text_helpers(void)
{
    VDI_HANDLE handle = open_handle();
    test_bitmap_t after;
    WORD row = 0;
    WORD column = 0;
    WORD pel = 0;
    WORD index = 0;

    ASSERT_TRUE(handle == 1);
    memset(&after, 0, sizeof(after));
    vsl_color(handle, 0);
    vsf_color(handle, 0);
    vst_color(handle, 0);

    clear_screen_and_counter(handle);
    v_gtext(handle, 4, 12, (BYTE *) "AB");
    snapshot_surface_bitmap(&after);
    ASSERT_TRUE(memchr(after.pixels, 1, (size_t) after.pitch * after.height) != NULL);

    clear_screen_and_counter(handle);
    v_justified(handle, 4, 12, "ABCD", 2, 0, 0);
    snapshot_surface_bitmap(&after);
    ASSERT_TRUE(memchr(after.pixels, 1, (size_t) after.pitch * after.height) != NULL);

    clear_screen_and_counter(handle);
    vs_curaddress(handle, 2, 3);
    ASSERT_TRUE(vq_curaddress(handle, &row, &column) == 1);
    ASSERT_TRUE(row == 2 && column == 3);
    ASSERT_TRUE(v_curtext(handle, (BYTE *) "HI") == 1);
    ASSERT_TRUE(v_get_pixel(handle, 12, 8, &pel, &index) == 1);
    ASSERT_TRUE(pel == index);
    ASSERT_TRUE(v_curdown(handle) == 1);
    ASSERT_TRUE(v_curright(handle) == 1);
    ASSERT_TRUE(v_curleft(handle) == 1);
    ASSERT_TRUE(v_curup(handle) == 1);
    ASSERT_TRUE(v_curhome(handle) == 1);
    ASSERT_TRUE(v_dspcur(handle, 12, 16) == 1);
    ASSERT_TRUE(v_eeol(handle) == 1);
    ASSERT_TRUE(v_eeos(handle) == 1);
    vq_mouse(handle, &row, &column, &pel);
    ASSERT_TRUE(row == 0 && column == 0 && pel == 0);
    v_hide_c(handle);
    v_show_c(handle, 0);

    test_bitmap_free(&after);
    v_clsvwk(handle);
    return 1;
}

int main(void)
{
    if (!test_open_close()) {
        return 1;
    }
    if (!test_polyline_bar_and_marker()) {
        return 1;
    }
    if (!test_clipping_and_output_window()) {
        return 1;
    }
    if (!test_fillarea_and_cellarray()) {
        return 1;
    }
    if (!test_circle_arc_ellipse_family()) {
        return 1;
    }
    if (!test_contourfill_and_blits()) {
        return 1;
    }
    if (!test_write_modes()) {
        return 1;
    }
    if (!test_attribute_and_query_functions()) {
        return 1;
    }
    if (!test_cursor_and_text_helpers()) {
        return 1;
    }

    puts("test_vdi: ok");
    return 0;
}
