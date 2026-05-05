/*
 * Builds a temporary executable scaffold for the Resource Construction
 * Set while the imported historical sources are being ported to the new
 * Linux GEM runtime. The program exercises the current VDI layer and
 * presents a short banner so the target links as a real application.
 *
 * MIT License (see: LICENSE)
 * Copyright (C) 2026 tomaz stih
 */

#include <gem/portab.h>
#include <gem/vdi.h>

#include "platform/os.h"

int main(void)
{
    WORD work_in[11] = { 0 };
    WORD work_out[57] = { 0 };
    VDI_HANDLE handle = 0;
    WORD frame[4] = { 24, 24, 360, 120 };

    v_opnvwk(work_in, &handle, work_out);
    if (handle == 0) {
        return 1;
    }

    v_clrwk(handle);
    vsl_color(handle, 1);
    v_rbox(handle, frame);
    vst_color(handle, 1);
    v_gtext(handle, 40, 60, (CONST BYTE *) "RCS scaffold");
    v_gtext(handle, 40, 76, (CONST BYTE *) "Historical sources not ported yet");
    gem_os_sleep_ms(10u);
    v_clsvwk(handle);
    return 0;
}
