/*
 * Implements reusable non-standard GEM control behavior.
 *
 * MIT License (see: LICENSE)
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

void gemctl_text_edit_init(gemctl_text_edit_t *control, char *text,
                           size_t capacity)
{
    if (control == NULL) {
        return;
    }
    control->text = text;
    control->capacity = capacity;
    control->active = 0;
    control->replace_pending = 0;
    if (text != NULL && capacity > 0u) {
        text[capacity - 1u] = '\0';
    }
}

void gemctl_text_edit_focus(gemctl_text_edit_t *control, int replace)
{
    if (control == NULL || control->text == NULL || control->capacity == 0u) {
        return;
    }
    control->active = 1;
    control->replace_pending = replace != 0;
}

void gemctl_text_edit_blur(gemctl_text_edit_t *control)
{
    if (control == NULL) {
        return;
    }
    control->active = 0;
    control->replace_pending = 0;
}

int gemctl_text_edit_active(const gemctl_text_edit_t *control)
{
    return control != NULL && control->active != 0;
}

gemctl_text_edit_result_t gemctl_text_edit_key(gemctl_text_edit_t *control,
                                               uint16_t key)
{
    unsigned char ascii = (unsigned char)(key & 0xffu);
    unsigned char scan = (unsigned char)((key >> 8) & 0xffu);
    size_t length;

    if (!gemctl_text_edit_active(control)) {
        return GEMCTL_TEXT_EDIT_IGNORED;
    }
    if (ascii == 27u) {
        return GEMCTL_TEXT_EDIT_CANCEL;
    }
    if (ascii == '\r' || ascii == '\n' ||
        (ascii == 0u && (scan == 28u || scan == 40u))) {
        return GEMCTL_TEXT_EDIT_ACCEPT;
    }
    if (ascii == '\b' || (ascii == 0u && (scan == 14u || scan == 42u))) {
        length = strlen(control->text);
        if (length > 0u) {
            control->text[length - 1u] = '\0';
            control->replace_pending = 0;
            return GEMCTL_TEXT_EDIT_CHANGED;
        }
        return GEMCTL_TEXT_EDIT_IGNORED;
    }
    if (ascii < 32u || ascii >= 127u) {
        return GEMCTL_TEXT_EDIT_IGNORED;
    }
    if (control->replace_pending != 0) {
        control->text[0] = '\0';
        control->replace_pending = 0;
    }
    length = strlen(control->text);
    if (length + 1u >= control->capacity) {
        return GEMCTL_TEXT_EDIT_IGNORED;
    }
    control->text[length] = (char)ascii;
    control->text[length + 1u] = '\0';
    return GEMCTL_TEXT_EDIT_CHANGED;
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
