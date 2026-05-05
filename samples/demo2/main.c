/*
 * Lists every VDI font currently available and renders a native-size
 * specimen for each one. The demo loads the runtime font directory,
 * queries the font list in order, and shows each sample in a compact
 * two-column sheet on the 640x400 monochrome screen.
 *
 * MIT License (see: LICENSE)
 * Copyright (C) 2026 tomaz stih
 */

#include <gem/portab.h>
#include <gem/vdi.h>

#include <stdio.h>
#include <string.h>

enum {
    demo2_columns = 2,
    demo2_margin = 12,
    demo2_gutter = 8,
    demo2_title_height = 16,
    demo2_name_height = 12,
    demo2_sample_height = 18,
    demo2_small_font = 5
};

static void draw_font_cell(VDI_HANDLE handle,
                           WORD left,
                           WORD top,
                           WORD right,
                           WORD bottom,
                           WORD font_id,
                           const BYTE *font_name)
{
    WORD clip[4];
    char label[96];

    clip[0] = (WORD) (left + 2);
    clip[1] = (WORD) (top + 2);
    clip[2] = (WORD) (right - 2);
    clip[3] = (WORD) (bottom - 2);

    vsl_color(handle, 0);
    v_rbox(handle, clip);

    vst_color(handle, 0);
    (void) vst_background(handle, 1);
    vst_font(handle, demo2_small_font);
    snprintf(label, sizeof(label), "ID %d", font_id);
    v_gtext(handle, (WORD) (left + 6), (WORD) (top + demo2_name_height),
        (CONST BYTE *) label);
    v_gtext(handle, (WORD) (left + 52), (WORD) (top + demo2_name_height),
        font_name);

    (void) vst_background(handle, 0);
    vst_font(handle, font_id);
    v_output_window(handle, clip);
    v_gtext(handle, (WORD) (left + 6), (WORD) (top + demo2_name_height + 20),
        (CONST BYTE *) "The quick brown fox 0123456789");
    vs_clip(handle, 0, clip);
}

int main(void)
{
    WORD work_in[11] = { 0 };
    WORD work_out[57] = { 0 };
    VDI_HANDLE handle = 0;
    WORD width;
    WORD height;
    WORD font_count;
    WORD rows;
    WORD cell_width;
    WORD cell_height;
    WORD index;
    BYTE name[64];

    v_opnvwk(work_in, &handle, work_out);
    if (handle == 0) {
        return 1;
    }

    font_count = vst_load_fonts(handle, 0);
    width = (WORD) (work_out[0] + 1);
    height = (WORD) (work_out[1] + 1);
    rows = (WORD) ((font_count + demo2_columns - 1) / demo2_columns);
    if (rows <= 0) {
        rows = 1;
    }

    cell_width = (WORD) ((width - demo2_margin * 2 -
        demo2_gutter * (demo2_columns - 1)) / demo2_columns);
    cell_height = (WORD) ((height - demo2_margin * 2 -
        demo2_title_height - demo2_gutter * (rows - 1)) / rows);

    v_clrwk(handle);
    vst_color(handle, 0);
    vst_font(handle, demo2_small_font);
    v_gtext(handle, demo2_margin, demo2_margin + 12,
        (CONST BYTE *) "DEMO2: ALL LOADED VDI FONTS AT NATIVE SIZE");

    memset(name, 0, sizeof(name));
    for (index = 1; index <= font_count; ++index) {
        WORD font_id;
        WORD row;
        WORD column;
        WORD left;
        WORD top;
        WORD right;
        WORD bottom;

        font_id = vqt_name(handle, index, name);
        if (font_id == 0) {
            continue;
        }

        row = (WORD) ((index - 1) / demo2_columns);
        column = (WORD) ((index - 1) % demo2_columns);
        left = (WORD) (demo2_margin + column * (cell_width + demo2_gutter));
        top = (WORD) (demo2_margin + demo2_title_height +
            row * (cell_height + demo2_gutter));
        right = (WORD) (left + cell_width - 1);
        bottom = (WORD) (top + cell_height - 1);

        draw_font_cell(handle, left, top, right, bottom, font_id, name);
    }

    FOREVER {
    }

    v_clsvwk(handle);
    return 0;
}
