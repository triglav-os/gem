/*
 * Exercises the minimal GEM VDI implementation by opening a virtual
 * workstation, drawing a polyline, filling a rectangle, and rendering
 * a short text string.
 *
 * MIT License (see: LICENSE)
 * Copyright (C) 2026 tomaz stih
 */

#include <gem/portab.h>
#include <gem/vdi.h>

int main(void)
{
    WORD work_in[11] = { 0 };
    WORD work_out[57] = { 0 };
    VDI_HANDLE handle = 0;
    WORD rect[] = { 40, 40, 300, 180 };
    WORD line[] = { 20, 20, 300, 200, 500, 80 };

    v_opnvwk(work_in, &handle, work_out);
    if (handle == 0) {
        return 1;
    }

    v_clrwk(handle);

    vsl_color(handle, 1);
    v_pline(handle, 3, line);

    vsf_color(handle, 2);
    v_bar(handle, rect);

    vst_color(handle, 1);
    v_gtext(handle, 60, 220, (CONST BYTE *) "hello from vdi");

    FOREVER {
    }

    v_clsvwk(handle);
    return 0;
}
