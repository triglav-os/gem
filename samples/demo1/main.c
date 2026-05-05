/*
 * Draws a monochrome broadcast-style VDI test card that exercises most
 * visible workstation operations in one scene. The demo stresses
 * clipping, line work, fills, arcs, ellipses, text, markers, contour
 * fill, cell arrays, and MFDB blits on the direct framebuffer path.
 *
 * MIT License (see: LICENSE)
 * Copyright (C) 2026 tomaz stih
 */

#include <gem/portab.h>
#include <gem/vdi.h>

#include <stddef.h>
#include <stdlib.h>
#include <string.h>

typedef struct packed_mfdb {
    MFDB mfdb;
    BYTE *bytes;
    WORD row_bytes;
} packed_mfdb_t;

static WORD min_word(WORD left, WORD right)
{
    return (left < right) ? left : right;
}

static void mfdb_init(packed_mfdb_t *packed, WORD width, WORD height)
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

static void mfdb_free(packed_mfdb_t *packed)
{
    free(packed->bytes);
    packed->bytes = NULL;
}

static void mfdb_set_pixel(packed_mfdb_t *packed, WORD x, WORD y, WORD value)
{
    size_t offset;
    BYTE mask;

    if (x < 0 || y < 0 || x >= packed->mfdb.fd_w || y >= packed->mfdb.fd_h) {
        return;
    }

    offset = (size_t) y * (size_t) packed->row_bytes + (size_t) x / 8u;
    mask = (BYTE) (1u << (7u - ((unsigned int) x & 7u)));
    if (value != 0) {
        packed->bytes[offset] |= mask;
    } else {
        packed->bytes[offset] &= (BYTE) ~mask;
    }
}

static void fill_logo_pattern(packed_mfdb_t *packed)
{
    WORD x;
    WORD y;

    for (y = 0; y < packed->mfdb.fd_h; ++y) {
        for (x = 0; x < packed->mfdb.fd_w; ++x) {
            WORD on = 0;

            if (x == 0 || y == 0 || x == packed->mfdb.fd_w - 1 ||
                y == packed->mfdb.fd_h - 1) {
                on = 1;
            }
            if (x == packed->mfdb.fd_w / 2 || y == packed->mfdb.fd_h / 2) {
                on = 1;
            }
            if ((x > 4 && x < packed->mfdb.fd_w - 5) &&
                (y > 4 && y < packed->mfdb.fd_h - 5) &&
                ((x + y) % 6 == 0 || (x - y) % 6 == 0)) {
                on = 1;
            }
            mfdb_set_pixel(packed, x, y, on);
        }
    }
}

static void draw_frame(VDI_HANDLE handle, WORD width, WORD height)
{
    WORD outer[] = { 4, 4, (WORD) (width - 5), (WORD) (height - 5) };
    WORD inner[] = { 12, 12, (WORD) (width - 13), (WORD) (height - 13) };
    WORD center_lines[] = {
        (WORD) (width / 2), 12, (WORD) (width / 2), (WORD) (height - 13),
        12, (WORD) (height / 2), (WORD) (width - 13), (WORD) (height / 2)
    };

    vsl_color(handle, 1);
    v_rbox(handle, outer);
    v_rbox(handle, inner);
    v_pline(handle, 2, center_lines);
    v_pline(handle, 2, center_lines + 4);
}

static void draw_resolution_boxes(VDI_HANDLE handle, WORD width)
{
    WORD cells[] = { 18, 18, (WORD) (width - 19), 56 };
    WORD pattern[24];
    WORD index;

    for (index = 0; index < 24; ++index) {
        pattern[index] = (index & 1) == 0 ? 0 : 1;
    }

    (void) v_cellarray(handle, cells, 6, 12, 2, 1, pattern);
}

static void draw_concentric_targets(VDI_HANDLE handle, WORD width, WORD height)
{
    WORD cx = (WORD) (width / 2);
    WORD cy = (WORD) (height / 2);
    WORD radius;

    vsl_color(handle, 1);
    for (radius = 28; radius <= min_word((WORD) (width / 3),
            (WORD) (height / 3)); radius = (WORD) (radius + 28)) {
        v_circle(handle, cx, cy, radius);
    }

    v_arc(handle, cx, cy, (WORD) (height / 3), 0, 900);
    v_arc(handle, cx, cy, (WORD) (height / 3), 1800, 2700);
    v_ellipse(handle, cx, cy, (WORD) (width / 5), (WORD) (height / 7));
    v_ellarc(handle, cx, cy, (WORD) (width / 4), (WORD) (height / 5),
        450, 1350);
}

static void draw_corner_pies(VDI_HANDLE handle, WORD width, WORD height)
{
    WORD filled_box[] = { (WORD) (width - 142), 24, (WORD) (width - 94), 54 };

    vsf_color(handle, 1);
    vsl_color(handle, 1);
    v_pieslice(handle, 48, 96, 24, 0, 900);
    v_pieslice(handle, (WORD) (width - 49), 96, 24, 900, 1800);
    v_pieslice(handle, 48, (WORD) (height - 97), 24, 2700, 3600);
    v_pieslice(handle, (WORD) (width - 49), (WORD) (height - 97), 24,
        1800, 2700);
    v_pie(handle, (WORD) (width - 64), 84, 18, 0, 2700);
    v_ellpie(handle, (WORD) (width / 2), 84, 52, 24, 0, 1800);
    v_rfbox(handle, filled_box);
}

static void draw_clip_window_test(VDI_HANDLE handle, WORD width, WORD height)
{
    WORD clip[] = {
        (WORD) (width / 2 - 72), (WORD) (height - 104),
        (WORD) (width / 2 + 72), (WORD) (height - 36)
    };
    WORD lines[] = {
        (WORD) (clip[0] - 40), clip[1], (WORD) (clip[2] + 40), clip[3],
        (WORD) (clip[0] - 40), clip[3], (WORD) (clip[2] + 40), clip[1]
    };
    WORD box[] = {
        (WORD) (clip[0] - 18), (WORD) (clip[1] + 12),
        (WORD) (clip[2] + 18), (WORD) (clip[3] - 12)
    };

    v_output_window(handle, clip);
    vsf_color(handle, 1);
    v_bar(handle, box);
    v_pline(handle, 2, lines);
    v_pline(handle, 2, lines + 4);
    vst_color(handle, 1);
    v_gtext(handle, (WORD) (clip[0] + 12), (WORD) (clip[1] + 26),
        (CONST BYTE *) "CLIPPED WINDOW");
    vs_clip(handle, 0, clip);
}

static void draw_polygon_and_fill(VDI_HANDLE handle, WORD width, WORD height)
{
    WORD poly[] = {
        (WORD) (width / 2 - 120), (WORD) (height / 2 + 30),
        (WORD) (width / 2 - 60), (WORD) (height / 2 - 44),
        (WORD) (width / 2 + 12), (WORD) (height / 2 - 18),
        (WORD) (width / 2 + 68), (WORD) (height / 2 + 46),
        (WORD) (width / 2 - 12), (WORD) (height / 2 + 88)
    };
    WORD contour_box[] = {
        (WORD) (width - 182), (WORD) (height - 112),
        (WORD) (width - 48), (WORD) (height - 38)
    };

    vsf_perimeter(handle, 1);
    vsf_color(handle, 1);
    v_fillarea(handle, 5, poly);
    v_rbox(handle, contour_box);
    v_contourfill(handle, (WORD) (width - 116), (WORD) (height - 76), 1);
}

static void draw_marker_strip(VDI_HANDLE handle, WORD width)
{
    WORD markers[18];
    WORD i;

    for (i = 0; i < 9; ++i) {
        markers[i * 2] = (WORD) (24 + i * ((width - 48) / 8));
        markers[i * 2 + 1] = 70;
    }

    v_pmarker(handle, 9, markers);
}

static void draw_mfdb_blt_tests(VDI_HANDLE handle, WORD width, WORD height)
{
    packed_mfdb_t logo;
    packed_mfdb_t mask;
    WORD copy_xy[8];
    WORD mask_xy[8];
    WORD colors[] = { 0, 1 };

    mfdb_init(&logo, 48, 48);
    mfdb_init(&mask, 48, 48);
    fill_logo_pattern(&logo);
    fill_logo_pattern(&mask);

    copy_xy[0] = 0;
    copy_xy[1] = 0;
    copy_xy[2] = 47;
    copy_xy[3] = 47;
    copy_xy[4] = 24;
    copy_xy[5] = (WORD) (height - 84);
    copy_xy[6] = 95;
    copy_xy[7] = (WORD) (height - 13);
    vro_cpyfm(handle, 1, copy_xy, &logo.mfdb, NULL);

    mask_xy[0] = 0;
    mask_xy[1] = 0;
    mask_xy[2] = 47;
    mask_xy[3] = 47;
    mask_xy[4] = (WORD) (width - 96);
    mask_xy[5] = (WORD) (height - 84);
    mask_xy[6] = (WORD) (width - 25);
    mask_xy[7] = (WORD) (height - 13);
    vrt_cpyfm(handle, 1, mask_xy, &mask.mfdb, NULL, colors);

    mfdb_free(&logo);
    mfdb_free(&mask);
}

static void draw_labels(VDI_HANDLE handle, WORD width, WORD height)
{
    char line[64];
    WORD extent[8];
    WORD char_width;
    WORD left = 0;

    vst_color(handle, 1);
    vst_font(handle, 3);
    v_gtext(handle, 24, 104, (CONST BYTE *) "GEM VDI DIRECT FRAMEBUFFER");
    vst_font(handle, 2);
    v_justified(handle, 24, (WORD) (height - 18),
        "LINES FILL TEXT BLIT CLIP ARC PIE ELLIPSE MARKER", 48, 0, 0);

    vst_font(handle, 3);
    (void) vqt_extent(handle, "MONO TV TEST CARD", extent);
    left = (WORD) ((width - (extent[2] - extent[0] + 1)) / 2);
    v_gtext(handle, left, (WORD) (height / 2 - 112),
        (CONST BYTE *) "MONO TV TEST CARD");

    vst_font(handle, 2);
    if (vq_chcells(handle, &char_width, &left) == 1) {
        (void) char_width;
    }

    (void) vqt_width(handle, 'M', &char_width, NULL, NULL);
    strcpy(line, "CHAR WIDTH ");
    line[11] = (char) ('0' + (char) (char_width / 10));
    line[12] = (char) ('0' + (char) (char_width % 10));
    line[13] = '\0';
    v_gtext(handle, 24, (WORD) (height - 96), (CONST BYTE *) line);
}

static void exercise_state_queries(VDI_HANDLE handle, WORD width, WORD height)
{
    WORD status = 0;
    WORD row = 2;
    WORD column = 3;
    WORD pel = 0;
    WORD index = 0;
    WORD clip[] = { 0, 0, (WORD) (width - 1), (WORD) (height - 1) };

    (void) vst_height(handle, 8, NULL, NULL, NULL, NULL);
    (void) vst_alignment(handle, 0, 0);
    (void) vst_rotation(handle, 0);
    (void) vsm_type(handle, 3);
    (void) vsm_height(handle, 8);
    (void) vswr_mode(handle, 1);
    (void) v_hide_c(handle);
    v_show_c(handle, 1);
    vs_curaddress(handle, row, column);
    (void) vq_curaddress(handle, &row, &column);
    (void) v_get_pixel(handle, (WORD) (width / 2), (WORD) (height / 2),
        &pel, &index);
    (void) vq_mouse(handle, &status, &row, &column);
    (void) vq_key_s(handle, &status);
    (void) v_output_window(handle, clip);
}

int main(void)
{
    WORD work_in[11] = { 0 };
    WORD work_out[57] = { 0 };
    VDI_HANDLE handle = 0;
    WORD width;
    WORD height;

    v_opnvwk(work_in, &handle, work_out);
    if (handle == 0) {
        return 1;
    }
    (void) vst_load_fonts(handle, 0);

    width = (WORD) (work_out[0] + 1);
    height = (WORD) (work_out[1] + 1);
    v_clrwk(handle);

    draw_frame(handle, width, height);
    draw_resolution_boxes(handle, width);
    draw_marker_strip(handle, width);
    draw_concentric_targets(handle, width, height);
    draw_corner_pies(handle, width, height);
    draw_polygon_and_fill(handle, width, height);
    draw_clip_window_test(handle, width, height);
    draw_mfdb_blt_tests(handle, width, height);
    draw_labels(handle, width, height);
    exercise_state_queries(handle, width, height);

    FOREVER {
    }

    v_clsvwk(handle);
    return 0;
}
