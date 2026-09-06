/*
 * Exercises VDI attributes, inquiries and large bidirectional MFDB transfers
 * against a real gemd. Pixel assertions catch successful but ineffective RPCs.
 *
 * MIT License (see: LICENSE)
 * Copyright (C) 2026 tomaz stih
 */
#include <gem.h>
#include <assert.h>
#include <stdlib.h>
#include <string.h>
static void callback(void) {}
int main(void)
{
    WORD in[11] = {0}, out[57] = {0}, handle = 0;
    WORD values[57] = {0}, rgb[3] = {100, 200, 300}, got[3] = {0};
    WORD cw, ch, fw, fh, a, b, c, index;
    BYTE name[33] = {0};
    v_opnvwk(in, &handle, out);
    assert(handle && out[0] >= 639 && out[1] >= 399);
    assert(vq_extnd(handle, 0, values));
    assert(!memcmp(out, values, sizeof(out)));
    assert(vs_color(handle, 3, rgb));
    assert(vq_color(handle, 3, 0, got));
    assert(!memcmp(rgb, got, sizeof(rgb)));
    assert(vsl_type(handle, 3) == 3);
    assert(vsl_width(handle, 5) == 5);
    assert(vql_attributes(handle, values));
    assert(values[0] == 3 && values[3] == 5);
    assert(vsm_type(handle, 4) == 4);
    assert(vsm_height(handle, 12) == 12);
    assert(vsm_color(handle, 0) == 0);
    assert(vqm_attributes(handle, values));
    assert(values[0] == 4 && values[3] == 12);
    assert(vst_height(handle, 16, &cw, &ch, &fw, &fh) > 0);
    assert(cw > 0 && ch > 0 && fw > 0 && fh > 0);
    assert(vst_rotation(handle, 900) == 900);
    assert(vst_alignment(handle, 1, 2));
    assert(vqt_attributes(handle, values));
    assert(values[2] == 900);
    assert(vst_load_fonts(handle, 0) > 0);
    assert(vqt_name(handle, 1, name) > 0 && name[0]);
    assert(vqt_width(handle, 'A', &a, &b, &c));
    assert(vqin_mode(handle, 1, &a));
    assert(vsin_mode(handle, 1, 2) == 2);
    assert(vqin_mode(handle, 1, &a) && a == 2);
    assert(vrq_valuator(handle, 42, &a, &b) && a == 42);
    assert(vsm_valuator(handle, 17, &a, &b) && a == 17);
    assert(vrq_choice(handle, 3, &a) && a == 3);
    assert(vsm_choice(handle, &a));
    assert(vrq_locator(handle, 0, 0, &a, &b, &c));
    assert(vsm_locator(handle, 0, 0, &a, &b, &c));
    VOID (*old)(void) = NULL;
    assert(vex_timv(handle, callback, &old, &a) && !old && a == 1);
    assert(vex_timv(handle, NULL, &old, &a) && old == callback);
    assert(vex_butv(handle, callback, &old));
    assert(vex_motv(handle, callback, &old));
    assert(vex_curv(handle, callback, &old));
    assert(vs_curaddress(handle, 2, 3));
    assert(vq_curaddress(handle, &a, &b));
    assert(a == 2 && b == 3);
    v_clrwk(handle);
    vsf_color(handle, 0);
    WORD rect[4] = {40, 40, 100, 100};
    v_bar(handle, rect);
    assert(v_get_pixel(handle, 50, 50, &a, &index) && a == 1);
    assert(v_get_pixel(handle, 20, 20, &a, &index) && a == 0);
    size_t size = 32000;
    UWORD *source = malloc(size), *destination = calloc(1, size);
    assert(source && destination);
    for (size_t i = 0; i < size / 2; ++i)
        source[i] = (UWORD)(i * 31u);
    MFDB src = {.fd_addr = source,
                .fd_w = 640,
                .fd_h = 400,
                .fd_wdwidth = 40,
                .fd_nplanes = 1};
    MFDB dst = src;
    dst.fd_addr = destination;
    WORD xy[8] = {0, 0, 639, 399, 0, 0, 639, 399};
    vro_cpyfm(handle, 3, xy, &src, &dst);
    assert(!memcmp(source, destination, size));
    memset(destination, 0, size);
    assert(vr_trnfm(handle, &src, &dst));
    assert(!memcmp(source, destination, size));
    WORD small[8] = {0, 0, 15, 15, 120, 120, 135, 135};
    for (unsigned i = 0; i < 16; ++i)
        source[i * 40] = 0xffff;
    WORD colors[2] = {1, 0};
    vrt_cpyfm(handle, 1, small, &src, NULL, colors);
    assert(v_get_pixel(handle, 125, 125, &a, &index) && a == 1);
    WORD cellrect[4] = {200, 200, 219, 219}, cells[4] = {1, 0, 0, 1};
    assert(v_cellarray(handle, cellrect, 2, 2, 2, 1, cells));
    assert(v_get_pixel(handle, 205, 205, &a, &index) && a == 1);
    assert(v_get_pixel(handle, 215, 205, &a, &index) && a == 0);
    free(source);
    free(destination);
    v_clsvwk(handle);
    (void)appl_exit();
    return 0;
}
