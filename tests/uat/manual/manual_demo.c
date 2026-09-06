/*
 * Implements the shared VDI sample scenes derived from the Programmer's
 * Guide. The code keeps each manual example in one place so demo3 and
 * later wrappers can run a single scene without duplicating setup logic.
 *
 * MIT License (see: LICENSE)
 * Copyright (C) 2026 tomaz stih
 */

#include "manual_demo.h"

#include <gem/vdi.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum {
    manual_demo_system_font = 1,
    manual_demo_small_font = 2
};

typedef void (*manual_demo_step_fn)(VDI_HANDLE handle, WORD width, WORD height);

typedef struct manual_demo_step {
    const char *title;
    const char *subtitle;
    manual_demo_step_fn run;
} manual_demo_step_t;

typedef struct manual_demo_packed_mfdb {
    MFDB mfdb;
    BYTE *bytes;
    WORD row_bytes;
} manual_demo_packed_mfdb_t;

static void manual_demo_draw_header(VDI_HANDLE handle,
                                    WORD width,
                                    const char *title,
                                    const char *subtitle)
{
    WORD line_xy[4] = { 0, 42, width - 1, 42 };

    vst_color(handle, 0);
    (void) vst_background(handle, 0);
    (void) vst_font(handle, manual_demo_small_font);
    v_gtext(handle, 8, 20, (CONST BYTE *) title);
    if (subtitle != NULL && subtitle[0] != '\0') {
        v_gtext(handle, 8, 36, (CONST BYTE *) subtitle);
    }
    v_pline(handle, 2, line_xy);
}

static void manual_demo_reset_state(VDI_HANDLE handle, WORD width, WORD height)
{
    WORD full[4] = { 0, 0, (WORD) (width - 1), (WORD) (height - 1) };

    v_clrwk(handle);
    vs_clip(handle, 0, full);
    (void) v_output_window(handle, full);
    (void) vswr_mode(handle, 1);
    (void) vst_background(handle, 0);
    vsl_color(handle, 0);
    vsf_color(handle, 0);
    vst_color(handle, 0);
    (void) vst_font(handle, manual_demo_system_font);
}

static WORD manual_demo_scale_x(WORD width, WORD ndc)
{
    return (WORD) (((LONG) ndc * (width - 1)) / 32767L);
}

static WORD manual_demo_scale_y(WORD height, WORD ndc)
{
    return (WORD) (((LONG) ndc * (height - 40)) / 32767L + 32L);
}

static void manual_demo_mfdb_init(manual_demo_packed_mfdb_t *packed,
                                  WORD width,
                                  WORD height)
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
    packed->mfdb.fd_nplanes = 1;
}

static void manual_demo_mfdb_free(manual_demo_packed_mfdb_t *packed)
{
    free(packed->bytes);
    packed->bytes = NULL;
}

static void manual_demo_mfdb_set_pixel(manual_demo_packed_mfdb_t *packed,
                                       WORD x,
                                       WORD y,
                                       WORD on)
{
    size_t offset;
    BYTE mask;

    if (x < 0 || y < 0 || x >= packed->mfdb.fd_w || y >= packed->mfdb.fd_h) {
        return;
    }

    offset = (size_t) y * (size_t) packed->row_bytes + (size_t) x / 8u;
    mask = (BYTE) (1u << (7u - ((unsigned int) x & 7u)));
    if (on != 0) {
        packed->bytes[offset] |= mask;
    } else {
        packed->bytes[offset] &= (BYTE) ~mask;
    }
}

static void manual_demo_fill_pattern_box(manual_demo_packed_mfdb_t *packed,
                                         WORD width,
                                         WORD height)
{
    WORD y;

    for (y = 0; y < height; ++y) {
        WORD x;

        for (x = 0; x < width; ++x) {
            if (((x / 4) + (y / 4)) & 1) {
                manual_demo_mfdb_set_pixel(packed, x, y, 1);
            }
        }
    }
}

static void manual_demo_step_listing_2_1(VDI_HANDLE handle,
                                         WORD width,
                                         WORD height)
{
    WORD pxy[12] = {
        12000, 12000,
        12000, 20000,
        14000, 21000,
        16000, 20000,
        16000, 12000,
        12000, 12000
    };
    WORD index;

    for (index = 0; index < 12; index += 2) {
        pxy[index] = manual_demo_scale_x(width, pxy[index]);
        pxy[index + 1] = manual_demo_scale_y(height, pxy[index + 1]);
    }
    v_pline(handle, 6, pxy);
}

static void manual_demo_step_clipping(VDI_HANDLE handle,
                                      WORD width,
                                      WORD height)
{
    WORD clip[4] = { 60, 70, (WORD) (width / 2), (WORD) (height - 40) };
    WORD bar_xy[4] = { 20, 40, (WORD) (width - 20), (WORD) (height - 20) };
    WORD border[10] = {
        clip[0], clip[1], clip[2], clip[1], clip[2],
        clip[3], clip[0], clip[3], clip[0], clip[1]
    };

    vs_clip(handle, 1, clip);
    v_bar(handle, bar_xy);
    vs_clip(handle, 0, clip);
    v_pline(handle, 5, border);
}

static void manual_demo_step_polyline_and_markers(VDI_HANDLE handle,
                                                  WORD width,
                                                  WORD height)
{
    WORD line_xy[10] = {
        40, (WORD) (height - 50),
        100, 90,
        (WORD) (width / 2), (WORD) (height - 90),
        (WORD) (width - 100), 90,
        (WORD) (width - 40), (WORD) (height - 50)
    };
    WORD marker_xy[8] = {
        80, 110, 160, 150, (WORD) (width - 160), 150,
        (WORD) (width - 80), 110
    };

    (void) vsl_type(handle, 3);
    (void) vsl_width(handle, 1);
    (void) vsl_ends(handle, 1, 2);
    v_pline(handle, 5, line_xy);
    (void) vsm_type(handle, 4);
    (void) vsm_height(handle, 12);
    v_pmarker(handle, 4, marker_xy);
}

static void manual_demo_step_text_and_fonts(VDI_HANDLE handle,
                                            WORD width,
                                            WORD height)
{
    BYTE name[64] = { 0 };
    WORD font_id;

    (void) vst_height(handle, 16, NULL, NULL, NULL, NULL);
    (void) vst_font(handle, manual_demo_system_font);
    v_gtext(handle, 40, 90, (CONST BYTE *) "System font sample");
    (void) vst_font(handle, manual_demo_small_font);
    (void) vst_background(handle, 1);
    v_gtext(handle, 40, 120, (CONST BYTE *) "Opaque small title");
    (void) vst_background(handle, 0);
    v_gtext(handle, 40, 145, (CONST BYTE *) "Transparent small title");

    font_id = vqt_name(handle, 3, name);
    if (font_id != 0) {
        (void) vst_font(handle, font_id);
        v_gtext(handle, 40, 190, name);
        v_gtext(handle, 40, 215, (CONST BYTE *) "External font sample 012345");
    }

    (void) width;
    (void) height;
}

static void manual_demo_step_fillarea_and_bar(VDI_HANDLE handle,
                                              WORD width,
                                              WORD height)
{
    WORD polygon[10] = {
        90, 90,
        (WORD) (width / 2), 60,
        (WORD) (width - 90), 100,
        (WORD) (width - 140), (WORD) (height - 80),
        120, (WORD) (height - 100)
    };
    WORD bar_xy[4] = { 120, 110, (WORD) (width - 120), (WORD) (height - 70) };

    (void) vsf_perimeter(handle, 1);
    v_fillarea(handle, 5, polygon);
    v_bar(handle, bar_xy);
}

static void manual_demo_step_cellarray(VDI_HANDLE handle,
                                       WORD width,
                                       WORD height)
{
    WORD cell_xy[4] = { 120, 90, (WORD) (width - 120), (WORD) (height - 80) };
    WORD colors[24];
    WORD index;

    for (index = 0; index < 24; ++index) {
        colors[index] = (WORD) ((index + (index / 6)) & 1);
    }
    (void) v_cellarray(handle, cell_xy, 6, 6, 4, 1, colors);
}

static void manual_demo_step_contourfill(VDI_HANDLE handle,
                                         WORD width,
                                         WORD height)
{
    WORD frame[10] = {
        130, 90,
        (WORD) (width - 130), 90,
        (WORD) (width - 130), (WORD) (height - 90),
        130, (WORD) (height - 90),
        130, 90
    };

    v_pline(handle, 5, frame);
    v_contourfill(handle, (WORD) (width / 2), (WORD) (height / 2), 0);
}

static void manual_demo_step_arc_family(VDI_HANDLE handle,
                                        WORD width,
                                        WORD height)
{
    WORD mid_x = (WORD) (width / 2);
    WORD mid_y = (WORD) (height / 2 + 20);

    v_circle(handle, 110, mid_y, 35);
    v_arc(handle, 110, mid_y, 28, 0, 1800);
    v_pieslice(handle, mid_x, mid_y, 40, 450, 1350);
    v_ellipse(handle, (WORD) (width - 120), mid_y, 42, 24);
    v_ellarc(handle, (WORD) (width - 120), mid_y, 34, 18, 1800, 3200);
    v_ellpie(handle, mid_x, (WORD) (mid_y + 70), 55, 22, 0, 1600);
}

static void manual_demo_step_rounded_boxes(VDI_HANDLE handle,
                                           WORD width,
                                           WORD height)
{
    WORD left[4] = { 80, 90, (WORD) (width / 2 - 20), (WORD) (height - 80) };
    WORD right[4] = {
        (WORD) (width / 2 + 20), 90, (WORD) (width - 80),
        (WORD) (height - 80)
    };

    v_rbox(handle, left);
    v_rfbox(handle, right);
}

static void manual_demo_step_justified(VDI_HANDLE handle,
                                       WORD width,
                                       WORD height)
{
    (void) width;
    (void) height;
    (void) vst_font(handle, manual_demo_small_font);
    v_justified(handle, 40, 120,
        "JUSTIFIED TEXT SAMPLE FROM THE PROGRAMMER GUIDE", 46, 0, 0);
    (void) vst_font(handle, manual_demo_system_font);
    v_justified(handle, 40, 180,
        "SECOND PASS WITH THE SYSTEM FONT", 32, 0, 0);
}

static void manual_demo_step_write_modes(VDI_HANDLE handle,
                                         WORD width,
                                         WORD height)
{
    WORD box[4] = { 120, 100, (WORD) (width - 120), (WORD) (height - 80) };

    (void) vswr_mode(handle, 1);
    v_bar(handle, box);
    (void) vswr_mode(handle, 3);
    v_bar(handle, box);
    (void) vswr_mode(handle, 1);
    v_gtext(handle, 40, 80, (CONST BYTE *) "Replace then XOR");
    (void) vswr_mode(handle, 4);
    v_bar(handle, box);
    (void) vswr_mode(handle, 1);
    v_gtext(handle, 40, 220, (CONST BYTE *) "Erase clears the filled area");
}

static void manual_demo_step_blits(VDI_HANDLE handle, WORD width, WORD height)
{
    manual_demo_packed_mfdb_t src;
    manual_demo_packed_mfdb_t mask;
    WORD copy_xy[8];
    WORD transp_xy[8];
    WORD colors[2] = { 0, 1 };

    (void) height;

    manual_demo_mfdb_init(&src, 48, 32);
    manual_demo_mfdb_init(&mask, 48, 32);
    manual_demo_fill_pattern_box(&src, 48, 32);
    manual_demo_fill_pattern_box(&mask, 24, 32);

    copy_xy[0] = 0;
    copy_xy[1] = 0;
    copy_xy[2] = 47;
    copy_xy[3] = 31;
    copy_xy[4] = 80;
    copy_xy[5] = 100;
    copy_xy[6] = 127;
    copy_xy[7] = 131;
    vro_cpyfm(handle, 3, copy_xy, &src.mfdb, NULL);

    transp_xy[0] = 0;
    transp_xy[1] = 0;
    transp_xy[2] = 47;
    transp_xy[3] = 31;
    transp_xy[4] = (WORD) (width / 2);
    transp_xy[5] = 100;
    transp_xy[6] = (WORD) (width / 2 + 47);
    transp_xy[7] = 131;
    vrt_cpyfm(handle, 1, transp_xy, &mask.mfdb, NULL, colors);

    manual_demo_mfdb_free(&src);
    manual_demo_mfdb_free(&mask);
}

static void manual_demo_step_queries(VDI_HANDLE handle,
                                     WORD width,
                                     WORD height)
{
    WORD attrib[57] = { 0 };
    WORD extent[8] = { 0 };
    WORD width_q = 0;
    WORD rows = 0;
    WORD columns = 0;
    WORD min_ade = 0;
    WORD max_ade = 0;
    WORD distances[5] = { 0 };
    WORD effects[3] = { 0 };
    BYTE name[64] = { 0 };
    char line[96];

    (void) width;
    (void) height;
    (void) vq_extnd(handle, 0, attrib);
    (void) vqt_extent(handle, "QUERY", extent);
    (void) vqt_width(handle, 'Q', &width_q, NULL, NULL);
    (void) vq_chcells(handle, &rows, &columns);
    (void) vqt_name(handle, 1, name);
    (void) vqt_fontinfo(handle, &min_ade, &max_ade, distances, &width_q,
        effects);

    (void) vst_font(handle, manual_demo_small_font);
    snprintf(line, sizeof(line), "size=%dx%d chars=%dx%d", attrib[0] + 1,
        attrib[1] + 1, columns, rows);
    v_gtext(handle, 40, 80, (CONST BYTE *) line);
    snprintf(line, sizeof(line), "font1=%s width(Q)=%d", name, width_q);
    v_gtext(handle, 40, 100, (CONST BYTE *) line);
    snprintf(line, sizeof(line), "ADE range=%d..%d extent=%d x %d", min_ade,
        max_ade, extent[2] - extent[0] + 1, extent[7] - extent[1] + 1);
    v_gtext(handle, 40, 120, (CONST BYTE *) line);
}

static void manual_demo_step_cursor_helpers(VDI_HANDLE handle,
                                            WORD width,
                                            WORD height)
{
    WORD row = 0;
    WORD column = 0;

    (void) width;
    (void) height;
    vs_curaddress(handle, 3, 5);
    (void) vq_curaddress(handle, &row, &column);
    (void) vst_font(handle, manual_demo_small_font);
    v_curtext(handle, (BYTE *) "CURSOR TEXT");
    v_curdown(handle);
    v_curright(handle);
    v_curtext(handle, (BYTE *) "MOVED");
    v_dspcur(handle, 220, 180);
    v_rmcur(handle);
}

static const manual_demo_step_t g_manual_demo_steps[] = {
    { "LISTING 2-1", "Sample C program from Chapter 2",
        manual_demo_step_listing_2_1 },
    { "VS_CLIP / V_BAR", "Clipping window from the output chapter",
        manual_demo_step_clipping },
    { "V_PLINE / V_PMARKER", "Polyline and polymarker output",
        manual_demo_step_polyline_and_markers },
    { "V_GTEXT / VST_FONT", "System, small, and external font samples",
        manual_demo_step_text_and_fonts },
    { "V_FILLAREA / V_BAR", "Filled polygon and filled rectangle",
        manual_demo_step_fillarea_and_bar },
    { "V_CELLARRAY", "Indexed cell array raster write",
        manual_demo_step_cellarray },
    { "V_CONTOURFILL", "Seed fill inside a simple frame",
        manual_demo_step_contourfill },
    { "ARC / PIE / ELLIPSE", "Circular and elliptical primitive family",
        manual_demo_step_arc_family },
    { "V_RBOX / V_RFBOX", "Rounded outline and filled rounded rectangle",
        manual_demo_step_rounded_boxes },
    { "V_JUSTIFIED", "Justified text output", manual_demo_step_justified },
    { "VSWR_MODE", "Replace, XOR, and erase write modes",
        manual_demo_step_write_modes },
    { "VRO_CPYFM / VRT_CPYFM", "Opaque and transparent raster copies",
        manual_demo_step_blits },
    { "INQUIRE FUNCTIONS", "Workstation, font, and text metrics",
        manual_demo_step_queries },
    { "CURSOR HELPERS", "Cursor positioning and text helpers",
        manual_demo_step_cursor_helpers }
};

int manual_demo_run(manual_demo_scene_t scene)
{
    WORD work_in[11] = { 0 };
    WORD work_out[57] = { 0 };
    VDI_HANDLE handle = 0;
    WORD width;
    WORD height;
    size_t scene_index = (size_t) scene;

    if (scene_index >=
        sizeof(g_manual_demo_steps) / sizeof(g_manual_demo_steps[0])) {
        return 1;
    }

    v_opnvwk(work_in, &handle, work_out);
    if (handle == 0) {
        return 1;
    }

    (void) vst_load_fonts(handle, 0);
    width = (WORD) (work_out[0] + 1);
    height = (WORD) (work_out[1] + 1);

    manual_demo_reset_state(handle, width, height);
    manual_demo_draw_header(handle, width,
        g_manual_demo_steps[scene_index].title,
        g_manual_demo_steps[scene_index].subtitle);
    g_manual_demo_steps[scene_index].run(handle, width, height);

    FOREVER {
    }

    v_clsvwk(handle);
    return 0;
}
