/*
 * Implements reusable non-standard GEM control behavior.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (C) 2026 tomaz stih
 */

#include <gem/gemctl.h>

#include <string.h>

enum { gemctl_band_gap = 4, gemctl_text_margin = 6 };

static void gemctl_fill(VDI_HANDLE handle, const GRECT *rect)
{
    WORD pxy[4];

    if (rect == NULL || rect->g_w <= 0 || rect->g_h <= 0)
        return;
    pxy[0] = rect->g_x;
    pxy[1] = rect->g_y;
    pxy[2] = (WORD)(rect->g_x + rect->g_w - 1);
    pxy[3] = (WORD)(rect->g_y + rect->g_h - 1);
    (void)vswr_mode(handle, MD_REPLACE);
    (void)vsf_interior(handle, FIS_SOLID);
    (void)vsf_style(handle, 8);
    (void)vsf_perimeter(handle, 0);
    vsf_color(handle, BLACK);
    v_bar(handle, pxy);
}

static void gemctl_line(VDI_HANDLE handle, WORD x0, WORD y0, WORD x1, WORD y1)
{
    WORD pxy[4] = {x0, y0, x1, y1};

    (void)vsl_type(handle, 1);
    (void)vsl_width(handle, 1);
    vsl_color(handle, WHITE);
    v_pline(handle, 2, pxy);
}

void gemctl_chrome_layout(const GRECT *work, WORD char_height, WORD box_height,
                          gemctl_chrome_t *chrome)
{
    WORD band_height;

    if (work == NULL || chrome == NULL)
        return;
    band_height = (WORD)(box_height - 2);
    if (band_height < char_height + gemctl_band_gap)
        band_height = (WORD)(char_height + gemctl_band_gap);
    if (band_height < 12)
        band_height = 12;
    if (band_height * 2 > work->g_h)
        band_height = (WORD)(work->g_h / 2);
    chrome->band_height = band_height;
    chrome->toolbar = *work;
    chrome->toolbar.g_h = band_height;
    chrome->status = *work;
    chrome->status.g_y = (WORD)(work->g_y + work->g_h - band_height);
    chrome->status.g_h = band_height;
    chrome->content = *work;
    chrome->content.g_y = (WORD)(work->g_y + band_height);
    chrome->content.g_h = (WORD)(work->g_h - band_height * 2);
    if (chrome->content.g_h < 0)
        chrome->content.g_h = 0;
}

void gemctl_toolbar_draw(VDI_HANDLE handle, const GRECT *rect)
{
    gemctl_fill(handle, rect);
    if (rect != NULL && rect->g_w > 0 && rect->g_h > 0)
        gemctl_line(handle, rect->g_x, (WORD)(rect->g_y + rect->g_h - 1),
                    (WORD)(rect->g_x + rect->g_w - 1),
                    (WORD)(rect->g_y + rect->g_h - 1));
}

void gemctl_status_draw(VDI_HANDLE handle, const GRECT *rect, const char *text)
{
    WORD distances[5] = {0};
    WORD extent[8];
    WORD y;

    gemctl_fill(handle, rect);
    if (rect == NULL || rect->g_w <= 0 || rect->g_h <= 0)
        return;
    gemctl_line(handle, rect->g_x, rect->g_y, (WORD)(rect->g_x + rect->g_w - 1),
                rect->g_y);
    if (text == NULL)
        return;
    (void)vqt_fontinfo(handle, NULL, NULL, distances, NULL, NULL);
    (void)vqt_extent(handle, (char *)text, extent);
    y = (WORD)(rect->g_y + (rect->g_h - (extent[5] - extent[1] + 1)) / 2 +
               distances[1] + 2);
    vst_color(handle, WHITE);
    v_gtext(handle, (WORD)(rect->g_x + gemctl_text_margin), y,
            (const BYTE *)text);
}

void gemctl_toolbar_separator(VDI_HANDLE handle, WORD x, WORD y0, WORD y1)
{
    if (y1 >= y0)
        gemctl_line(handle, x, y0, x, y1);
}

void gemctl_toolbar_bitmap(VDI_HANDLE handle, const BITBLK *bitmap,
                           const GRECT *slot)
{
    MFDB source;
    WORD pxy[8];
    WORD colors[2] = {BLACK, WHITE};
    WORD width;
    WORD x;
    WORD y;

    if (bitmap == NULL || slot == NULL || bitmap->bi_pdata == 0 ||
        bitmap->bi_wb <= 0 || bitmap->bi_hl <= 0)
        return;
    width = (WORD)(bitmap->bi_wb * 8);
    x = (WORD)(slot->g_x + (slot->g_w - width) / 2);
    y = (WORD)(slot->g_y + (slot->g_h - bitmap->bi_hl) / 2);
    memset(&source, 0, sizeof(source));
    source.fd_addr = (VOID *)(intptr_t)bitmap->bi_pdata;
    source.fd_w = width;
    source.fd_h = bitmap->bi_hl;
    source.fd_wdwidth = (WORD)(bitmap->bi_wb / 2);
    source.fd_stand = 1;
    source.fd_nplanes = 1;
    pxy[0] = pxy[1] = 0;
    pxy[2] = (WORD)(width - 1);
    pxy[3] = (WORD)(bitmap->bi_hl - 1);
    pxy[4] = x;
    pxy[5] = y;
    pxy[6] = (WORD)(x + width - 1);
    pxy[7] = (WORD)(y + bitmap->bi_hl - 1);
    vrt_cpyfm(handle, MD_REPLACE, pxy, &source, NULL, colors);
}

void gemctl_toolbar_zoom_icon(VDI_HANDLE handle, const GRECT *slot, int zoom_in)
{
    WORD cx;
    WORD cy;
    WORD radius;

    if (slot == NULL || slot->g_w <= 0 || slot->g_h <= 0)
        return;
    radius = (WORD)((slot->g_w < slot->g_h ? slot->g_w : slot->g_h) / 5);
    if (radius < 3)
        radius = 3;
    cx = (WORD)(slot->g_x + slot->g_w / 2 - 2);
    cy = (WORD)(slot->g_y + slot->g_h / 2 - 2);
    (void)vsl_type(handle, 1);
    (void)vsl_width(handle, 1);
    vsl_color(handle, WHITE);
    v_circle(handle, cx, cy, radius);
    gemctl_line(handle, (WORD)(cx + radius - 1), (WORD)(cy + radius - 1),
                (WORD)(cx + radius + 3), (WORD)(cy + radius + 3));
    gemctl_line(handle, (WORD)(cx - radius + 2), cy, (WORD)(cx + radius - 2),
                cy);
    if (zoom_in != 0)
        gemctl_line(handle, cx, (WORD)(cy - radius + 2), cx,
                    (WORD)(cy + radius - 2));
}

static WORD gemctl_character_width(VDI_HANDLE handle, char character)
{
    WORD extent[8];
    char text[2] = {character, '\0'};
    vqt_extent(handle, text, extent);
    return (WORD)(extent[2] - extent[0] + 1);
}

void gemctl_text_edit_click(gemctl_text_edit_t *control, VDI_HANDLE handle,
                            const GRECT *rect, WORD x, int extend)
{
    size_t position, length;
    WORD current;
    if (control == NULL || control->text == NULL || rect == NULL)
        return;
    length = strlen(control->text);
    if (control->offset > length)
        control->offset = length;
    if (extend && x < rect->g_x + gemctl_text_margin && control->offset > 0)
        --control->offset;
    position = control->offset;
    current = (WORD)(rect->g_x + gemctl_text_margin);
    while (position < length) {
        WORD width = gemctl_character_width(handle, control->text[position]);
        if (x < current + width / 2)
            break;
        current = (WORD)(current + width);
        ++position;
    }
    gemctl_text_edit_position(control, position, extend);
}

void gemctl_text_edit_draw(gemctl_text_edit_t *control, VDI_HANDLE handle,
                           const GRECT *rect, const GRECT *clip)
{
    WORD bounds[4], restore[4], distances[5] = {0};
    WORD x, width, baseline;
    size_t length, first, last;
    if (control == NULL || control->text == NULL || rect == NULL ||
        clip == NULL || rect->g_w < 2)
        return;
    bounds[0] = rect->g_x > clip->g_x ? rect->g_x : clip->g_x;
    bounds[1] = rect->g_y > clip->g_y ? rect->g_y : clip->g_y;
    bounds[2] = rect->g_x + rect->g_w < clip->g_x + clip->g_w
                    ? rect->g_x + rect->g_w - 1
                    : clip->g_x + clip->g_w - 1;
    bounds[3] = rect->g_y + rect->g_h < clip->g_y + clip->g_h
                    ? rect->g_y + rect->g_h - 1
                    : clip->g_y + clip->g_h - 1;
    if (bounds[0] > bounds[2] || bounds[1] > bounds[3])
        return;
    vs_clip(handle, 1, bounds);
    length = strlen(control->text);
    if (!control->active)
        control->offset = 0;
    if (control->offset > control->cursor)
        control->offset = control->cursor;
    if (control->offset > length)
        control->offset = length;
    if (control->active) {
        width = 0;
        for (size_t i = control->offset; i < control->cursor && i < length; ++i)
            width = (WORD)(width +
                           gemctl_character_width(handle, control->text[i]));
        while (width >= rect->g_w - 2 * gemctl_text_margin &&
               control->offset < control->cursor) {
            width = (WORD)(width - gemctl_character_width(
                                       handle, control->text[control->offset]));
            ++control->offset;
        }
    }
    gemctl_fill(handle, rect);
    vqt_fontinfo(handle, NULL, NULL, distances, NULL, NULL);
    baseline =
        (WORD)(rect->g_y + (rect->g_h - (distances[0] + distances[4] + 1)) / 2 +
               distances[1] + 2);
    first =
        control->cursor < control->anchor ? control->cursor : control->anchor;
    last =
        control->cursor > control->anchor ? control->cursor : control->anchor;
    x = (WORD)(rect->g_x + gemctl_text_margin);
    for (size_t i = control->offset;
         i <= length && x < rect->g_x + rect->g_w - 2; ++i) {
        char text[2] = {control->text[i], '\0'};
        int selected = control->active && i >= first && i < last;
        if (i == control->cursor && control->active && first == last)
            gemctl_line(handle, x, rect->g_y + 2, x, rect->g_y + rect->g_h - 3);
        if (i == length)
            break;
        width = gemctl_character_width(handle, text[0]);
        if (selected) {
            WORD pxy[4] = {x, (WORD)(rect->g_y + 1), (WORD)(x + width - 1),
                           (WORD)(rect->g_y + rect->g_h - 2)};
            vsf_color(handle, WHITE);
            v_bar(handle, pxy);
        }
        vst_color(handle, selected ? BLACK : WHITE);
        v_gtext(handle, x, baseline, (const BYTE *)text);
        x = (WORD)(x + width);
    }
    restore[0] = clip->g_x;
    restore[1] = clip->g_y;
    restore[2] = (WORD)(clip->g_x + clip->g_w - 1);
    restore[3] = (WORD)(clip->g_y + clip->g_h - 1);
    vs_clip(handle, 1, restore);
}
