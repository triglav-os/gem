/*
 * Implements the private hosted GEM VDI core state that remains shared
 * across the split helper modules. This file owns the global VDI
 * workstation state, update/present flow, environment parsing, and the
 * standard `work_out` capability block.
 *
 * MIT License (see: LICENSE)
 * Copyright (C) 2026 tomaz stih
 */

#include "vdi_internal.h"

#include "vdi_state.h"

#include "platform/raster.h"

#include <stdlib.h>
#include <string.h>

vdi_state_t vdi_state;

WORD vdi_parse_env_word(const char *name, WORD fallback)
{
    const char *value = getenv(name);
    char *end = NULL;
    long parsed;

    if (value == NULL || value[0] == '\0') {
        return fallback;
    }

    parsed = strtol(value, &end, 10);
    if (end == value || *end != '\0' || parsed <= 0 || parsed > 32767L) {
        return fallback;
    }

    return (WORD)parsed;
}

void vdi_pump_events(void) {}

void vdi_mark_dirty(WORD x0, WORD y0, WORD x1, WORD y1)
{
    WORD t;

    if (!vdi_state.open) {
        return;
    }
    if (x0 > x1) {
        t = x0;
        x0 = x1;
        x1 = t;
    }
    if (y0 > y1) {
        t = y0;
        y0 = y1;
        y1 = t;
    }
    if (x0 < 0) {
        x0 = 0;
    }
    if (y0 < 0) {
        y0 = 0;
    }
    if (x1 >= vdi_state.width) {
        x1 = (WORD)(vdi_state.width - 1);
    }
    if (y1 >= vdi_state.height) {
        y1 = (WORD)(vdi_state.height - 1);
    }
    if (x0 > x1 || y0 > y1) {
        return;
    }
    if (vdi_state.dirty_valid == 0) {
        vdi_state.dirty_x0 = x0;
        vdi_state.dirty_y0 = y0;
        vdi_state.dirty_x1 = x1;
        vdi_state.dirty_y1 = y1;
        vdi_state.dirty_valid = 1;
        return;
    }
    if (x0 < vdi_state.dirty_x0) {
        vdi_state.dirty_x0 = x0;
    }
    if (y0 < vdi_state.dirty_y0) {
        vdi_state.dirty_y0 = y0;
    }
    if (x1 > vdi_state.dirty_x1) {
        vdi_state.dirty_x1 = x1;
    }
    if (y1 > vdi_state.dirty_y1) {
        vdi_state.dirty_y1 = y1;
    }
}

void vdi_begin_update(void)
{
    ++vdi_state.update_depth;
}

void vdi_end_update(void)
{
    if (vdi_state.update_depth <= 0) {
        return;
    }

    --vdi_state.update_depth;
    if (vdi_state.update_depth == 0 && vdi_state.present_pending != 0) {
        vdi_state.present_pending = 0;
        vdi_present_screen();
    }
    vdi_pump_events();
}

void vdi_end_update_no_present(void)
{
    if (vdi_state.update_depth <= 0) {
        return;
    }
    --vdi_state.update_depth;
    /* A partial inner flush must not cancel the outer batch's pending
     * presentation (for example, restoring a popup then drawing its seam).
     */
    if (vdi_state.update_depth == 0) {
        vdi_state.present_pending = 0;
    }
}

void vdi_flush_rect(WORD x, WORD y, WORD w, WORD h)
{
    /*
     * Push a rectangle to the physical display even while wind_update /
     * begin_update has deferred normal presents. Used for XOR rubber-band
     * feedback (window drag, scrollers) and clipped region redraws.
     */
    if (!vdi_state.open || w <= 0 || h <= 0) {
        return;
    }
    gem_raster_present_rect((int)x, (int)y, (int)w, (int)h);
}

void vdi_flush_display(void)
{
    if (!vdi_state.open) {
        return;
    }
    vdi_state.present_pending = 0;
    vdi_state.dirty_valid = 0;
    gem_raster_present();
}

void vdi_fill_work_out(WORD work_out[57])
{
    WORD cell_width = vdi_font_cell_width();
    WORD text_height = vdi_font_text_height();

    memset(work_out, 0, sizeof(WORD) * 57u);
    work_out[0] = (WORD)(vdi_state.width - 1);
    work_out[1] = (WORD)(vdi_state.height - 1);
    work_out[2] = 1;
    work_out[3] = 1;
    work_out[4] = 1;
    work_out[5] = 1;
    work_out[6] = 1;
    work_out[7] = 1;
    work_out[8] = 1;
    work_out[9] = 1;
    work_out[10] = 1;
    work_out[11] = 1;
    work_out[12] = 1;
    work_out[13] = 2;
    work_out[35] = 1;
    work_out[39] = 1;
    work_out[40] = 1;
    work_out[41] = 1;
    work_out[42] = 1;
    work_out[43] = 1;
    work_out[44] = 1;
    work_out[45] = cell_width;
    work_out[46] = text_height;
    work_out[47] = cell_width;
    work_out[48] = text_height;
    work_out[49] = 1;
    work_out[51] = 1;
    work_out[54] = 1;
    work_out[56] = 1;
}
