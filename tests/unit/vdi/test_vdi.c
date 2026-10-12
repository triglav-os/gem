/*
 * Runs the hosted VDI unit tests: workstation lifetime, attribute and
 * query functions, cursor and text helpers, hostile font files, and the
 * raster cases in test_vdi_raster.c, against memory-backed reference
 * renderers so framebuffer regressions surface before AES bugs do.
 *
 * MIT License (see: LICENSE)
 * Copyright (C) 2026 tomaz stih
 */

#define _POSIX_C_SOURCE 200809L

#include "vdi_test.h"
#include "vdi_internal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static int test_open_close(void)
{
    WORD work_in[11] = {0};
    WORD work_out[57] = {0};
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

static int test_attribute_and_query_functions(void)
{
    VDI_HANDLE handle = open_handle();
    WORD rgb[3] = {100, 200, 300};
    WORD out_rgb[3] = {0};
    WORD attrib[57] = {0};
    WORD extent[8] = {0};
    WORD charw = 0;
    WORD charh = 0;
    WORD cellw = 0;
    WORD cellh = 0;
    WORD rows = 0;
    WORD columns = 0;
    WORD status = 0;
    WORD font_count = 0;
    BYTE name[32] = {0};
    WORD distances[5] = {0};
    WORD effects[3] = {0};
    MFORM form;
    WORD pattern = (WORD)0xaaaa;

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
    ASSERT_TRUE(vst_font(handle, 5) == 5);
    ASSERT_TRUE(vdi_font_cell_width() == 8);
    ASSERT_TRUE(vdi_font_text_height() == 16);
    ASSERT_TRUE(vdi_string_width("ABC") == 24);
    ASSERT_TRUE(vst_font(handle, -5) == -5);
    ASSERT_TRUE(vdi_font_cell_width() == 6);
    ASSERT_TRUE(vdi_font_text_height() == 6);
    ASSERT_TRUE(vdi_string_width("ABC") == 18);
    ASSERT_TRUE(vst_font(handle, -3) == -3);
    ASSERT_TRUE(vdi_font_cell_width() == 8);
    ASSERT_TRUE(vdi_font_text_height() == 16);
    ASSERT_TRUE(vst_font(handle, 3) == 3);
    ASSERT_TRUE(vdi_font_cell_width() == 8);
    ASSERT_TRUE(vdi_font_text_height() == 16);
    ASSERT_TRUE(vqt_fontinfo(handle, NULL, NULL, distances, &charw, effects) ==
                1);
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
    ASSERT_TRUE(vm_filename(handle, (BYTE *)"meta.out") == 1);
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
    v_gtext(handle, 4, 12, (BYTE *)"AB");
    snapshot_surface_bitmap(&after);
    ASSERT_TRUE(memchr(after.pixels, 1, (size_t)after.pitch * after.height) !=
                NULL);

    clear_screen_and_counter(handle);
    v_justified(handle, 4, 12, "ABCD", 2, 0, 0);
    snapshot_surface_bitmap(&after);
    ASSERT_TRUE(memchr(after.pixels, 1, (size_t)after.pitch * after.height) !=
                NULL);

    clear_screen_and_counter(handle);
    vs_curaddress(handle, 2, 3);
    ASSERT_TRUE(vq_curaddress(handle, &row, &column) == 1);
    ASSERT_TRUE(row == 2 && column == 3);
    ASSERT_TRUE(v_curtext(handle, (BYTE *)"HI") == 1);
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

static int test_hostile_fonts(void)
{
    uint8_t font[100] = {0};
    char long_path[700];
    char directory[] = "/tmp/gem-font-test.XXXXXXXX";
    char fonts[128], path[160];
    FILE *file;
    const char *configured = getenv("GEM_RESOURCE_DIR");
    char saved[4096];
    ASSERT_TRUE(!configured || strlen(configured) < sizeof(saved));
    if (configured)
        strcpy(saved, configured);
    /* Two glyphs, offsets 0/4/8, in one eight-bit scanline. */
    font[92] = 4;
    font[94] = 8;
    ASSERT_TRUE(
        vdi_font_bitmap_valid(font, sizeof(font), 65, 66, 1, 1, 88, 90));
    font[94] = 9;
    ASSERT_TRUE(
        !vdi_font_bitmap_valid(font, sizeof(font), 65, 66, 1, 1, 88, 90));
    font[94] = 3;
    ASSERT_TRUE(
        !vdi_font_bitmap_valid(font, sizeof(font), 65, 66, 1, 1, 88, 90));
    ASSERT_TRUE(
        !vdi_font_bitmap_valid(font, sizeof(font), 66, 65, 1, 1, 88, 90));
    ASSERT_TRUE(
        !vdi_font_bitmap_valid(font, sizeof(font), 65, 66, -1, 1, 88, 90));
    ASSERT_TRUE(!vdi_font_bitmap_valid(font, sizeof(font), 65, 66, 1, 1,
                                       UINT32_MAX, 90));
    ASSERT_TRUE(!vdi_font_bitmap_valid(font, 94, 65, 66, 1, 1, 88, 90));
    memset(long_path, 'x', sizeof(long_path) - 1);
    long_path[sizeof(long_path) - 1] = '\0';
    vdi_unload_fonts();
    ASSERT_TRUE(setenv("GEM_RESOURCE_DIR", long_path, 1) == 0);
    ASSERT_TRUE(!vdi_load_fonts());
    ASSERT_TRUE(mkdtemp(directory));
    snprintf(fonts, sizeof(fonts), "%s/fonts", directory);
    snprintf(path, sizeof(path), "%s/AtariSTHigh.fnt", fonts);
    ASSERT_TRUE(mkdir(fonts, 0700) == 0);
    font[36] = 65;
    font[38] = 66;
    font[80] = 1;
    font[82] = 1;
    file = fopen(path, "wb");
    ASSERT_TRUE(file && fwrite(font, 1, sizeof(font), file) == sizeof(font));
    fclose(file);
    ASSERT_TRUE(setenv("GEM_RESOURCE_DIR", directory, 1) == 0);
    ASSERT_TRUE(!vdi_load_fonts());
    ASSERT_TRUE(unlink(path) == 0 && rmdir(fonts) == 0 &&
                rmdir(directory) == 0);
    if (configured) {
        setenv("GEM_RESOURCE_DIR", saved, 1);
    } else
        unsetenv("GEM_RESOURCE_DIR");
    ASSERT_TRUE(vdi_load_fonts());
    vdi_unload_fonts();
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
    if (!test_extreme_coordinates_and_screen_scroll()) {
        return 1;
    }
    if (!test_attribute_and_query_functions()) {
        return 1;
    }
    if (!test_cursor_and_text_helpers()) {
        return 1;
    }
    if (!test_vrt_cpyfm_glyph_bitmap()) {
        return 1;
    }
    if (!test_hostile_fonts())
        return 1;

    puts("test_vdi: ok");
    return 0;
}
