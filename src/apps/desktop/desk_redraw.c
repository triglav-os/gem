/*
 * Repaints the desktop background and icons for damaged regions and for
 * windows that moved, resized or closed, and keeps the status line.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (C) 2026 tomaz stih
 */

#define _POSIX_C_SOURCE 200809L

#include "desktop.h"

#include <stdio.h>
#include <string.h>

void desktop_set_status(const char *text)
{
    if (text == NULL) {
        g_desktop.status_text[0] = '\0';
    } else {
        strncpy(g_desktop.status_text, text,
                sizeof(g_desktop.status_text) - 1u);
        g_desktop.status_text[sizeof(g_desktop.status_text) - 1u] = '\0';
    }
}

static void desktop_draw_background_pattern(const GRECT *dirty)
{
    WORD fill[4];
    WORD x0;
    WORD y0;
    WORD x1;
    WORD y1;

    x0 = g_desktop.work.g_x;
    y0 = g_desktop.work.g_y;
    x1 = (WORD)(g_desktop.work.g_x + g_desktop.work.g_w - 1);
    y1 = (WORD)(g_desktop.work.g_y + g_desktop.work.g_h - 1);

    if (dirty != NULL) {
        WORD dirty_x1 = (WORD)(dirty->g_x + dirty->g_w - 1);
        WORD dirty_y1 = (WORD)(dirty->g_y + dirty->g_h - 1);

        if (x0 < dirty->g_x) {
            x0 = dirty->g_x;
        }
        if (y0 < dirty->g_y) {
            y0 = dirty->g_y;
        }
        if (x1 > dirty_x1) {
            x1 = dirty_x1;
        }
        if (y1 > dirty_y1) {
            y1 = dirty_y1;
        }
    }

    if (x0 > x1 || y0 > y1) {
        return;
    }

    fill[0] = x0;
    fill[1] = y0;
    fill[2] = x1;
    fill[3] = y1;

    (void)vsf_interior(g_desktop.vdi_handle, FIS_PATTERN);
    (void)vsf_style(g_desktop.vdi_handle, 2);
    /* Match the AES checker phase when damaged background is restored. */
    vsf_color(g_desktop.vdi_handle, WHITE);
    v_bar(g_desktop.vdi_handle, fill);
    (void)vsf_interior(g_desktop.vdi_handle, FIS_SOLID);
    (void)vsf_style(g_desktop.vdi_handle, 1);
}

void desktop_redraw(const GRECT *dirty)
{
    GRECT visible;

    wind_update(BEG_UPDATE);
    (void)vswr_mode(g_desktop.vdi_handle, MD_REPLACE);
    wind_get(0, WF_FIRSTXYWH, &visible.g_x, &visible.g_y, &visible.g_w,
             &visible.g_h);
    while (visible.g_w > 0 && visible.g_h > 0) {
        WORD x0 = visible.g_x;
        WORD y0 = visible.g_y;
        WORD x1 = (WORD)(visible.g_x + visible.g_w - 1);
        WORD y1 = (WORD)(visible.g_y + visible.g_h - 1);
        WORD clip_xy[4];
        GRECT clipped_dirty;

        if (dirty != NULL) {
            WORD dirty_x1 = (WORD)(dirty->g_x + dirty->g_w - 1);
            WORD dirty_y1 = (WORD)(dirty->g_y + dirty->g_h - 1);

            if (x0 < dirty->g_x) {
                x0 = dirty->g_x;
            }
            if (y0 < dirty->g_y) {
                y0 = dirty->g_y;
            }
            if (x1 > dirty_x1) {
                x1 = dirty_x1;
            }
            if (y1 > dirty_y1) {
                y1 = dirty_y1;
            }
        }

        if (x0 <= x1 && y0 <= y1) {
            clip_xy[0] = x0;
            clip_xy[1] = y0;
            clip_xy[2] = x1;
            clip_xy[3] = y1;
            clipped_dirty.g_x = x0;
            clipped_dirty.g_y = y0;
            clipped_dirty.g_w = (WORD)(x1 - x0 + 1);
            clipped_dirty.g_h = (WORD)(y1 - y0 + 1);
            vs_clip(g_desktop.vdi_handle, 1, clip_xy);
            desktop_draw_background_pattern(&clipped_dirty);
            objc_draw(g_desktop.desktop_tree, ROOT, MAX_DEPTH, x0, y0,
                      clipped_dirty.g_w, clipped_dirty.g_h);
            vs_clip(g_desktop.vdi_handle, 0, clip_xy);
        }

        wind_get(0, WF_NEXTXYWH, &visible.g_x, &visible.g_y, &visible.g_w,
                 &visible.g_h);
    }
    v_updwk(g_desktop.vdi_handle);
    wind_update(END_UPDATE);
}

void desktop_redraw_window_change(const GRECT *before, const GRECT *after)
{
    GRECT fragments[4];
    WORD count = 0;
    WORD before_right;
    WORD before_bottom;
    WORD overlap_x0;
    WORD overlap_y0;
    WORD overlap_x1;
    WORD overlap_y1;
    WORD after_right;
    WORD after_bottom;
    WORD i;

    if (before == NULL || before->g_w <= 0 || before->g_h <= 0) {
        return;
    }

    if (after == NULL || after->g_w <= 0 || after->g_h <= 0) {
        desktop_redraw(before);
        return;
    }

    before_right = (WORD)(before->g_x + before->g_w - 1);
    before_bottom = (WORD)(before->g_y + before->g_h - 1);
    after_right = (WORD)(after->g_x + after->g_w - 1);
    after_bottom = (WORD)(after->g_y + after->g_h - 1);
    overlap_x0 = (before->g_x > after->g_x) ? before->g_x : after->g_x;
    overlap_y0 = (before->g_y > after->g_y) ? before->g_y : after->g_y;
    overlap_x1 = (before_right < after_right) ? before_right : after_right;
    overlap_y1 = (before_bottom < after_bottom) ? before_bottom : after_bottom;

    if (overlap_x0 > overlap_x1 || overlap_y0 > overlap_y1) {
        desktop_redraw(before);
        return;
    }

    if (before->g_y < overlap_y0) {
        fragments[count].g_x = before->g_x;
        fragments[count].g_y = before->g_y;
        fragments[count].g_w = before->g_w;
        fragments[count].g_h = (WORD)(overlap_y0 - before->g_y);
        ++count;
    }
    if (overlap_y1 < before_bottom) {
        fragments[count].g_x = before->g_x;
        fragments[count].g_y = (WORD)(overlap_y1 + 1);
        fragments[count].g_w = before->g_w;
        fragments[count].g_h = (WORD)(before_bottom - overlap_y1);
        ++count;
    }
    if (before->g_x < overlap_x0) {
        fragments[count].g_x = before->g_x;
        fragments[count].g_y = overlap_y0;
        fragments[count].g_w = (WORD)(overlap_x0 - before->g_x);
        fragments[count].g_h = (WORD)(overlap_y1 - overlap_y0 + 1);
        ++count;
    }
    if (overlap_x1 < before_right) {
        fragments[count].g_x = (WORD)(overlap_x1 + 1);
        fragments[count].g_y = overlap_y0;
        fragments[count].g_w = (WORD)(before_right - overlap_x1);
        fragments[count].g_h = (WORD)(overlap_y1 - overlap_y0 + 1);
        ++count;
    }

    for (i = 0; i < count; ++i) {
        if (fragments[i].g_w > 0 && fragments[i].g_h > 0) {
            desktop_redraw(&fragments[i]);
        }
    }
}
