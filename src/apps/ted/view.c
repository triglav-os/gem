/*
 * Draws Ted's clipped text viewport, selection, caret and scroll metrics.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (C) 2026 tomaz stih
 */
#include "ted.h"
#include <stdio.h>
#include <string.h>

void ted_layout(ted_state_t *s)
{
    wind_get(s->window, WF_WORKXYWH, &s->work.g_x, &s->work.g_y, &s->work.g_w,
             &s->work.g_h);
    gemctl_chrome_layout(&s->work, s->char_h, s->box_h, &s->chrome);
    s->body = s->chrome.content;
}
size_t ted_column(ted_state_t *s, size_t start, size_t end)
{
    size_t col = 0;
    for (size_t p = start; p < end; p = ted_document_step(&s->doc, p, 1))
        col += s->doc.text[p] == '\t'   ? 4 - col % 4
               : s->doc.text[p] == '\r' ? 0
                                        : 1;
    return col;
}
size_t ted_offset(ted_state_t *s, size_t line, size_t column)
{
    size_t p = ted_document_line(&s->doc, line), col = 0;
    while (p < s->doc.length && s->doc.text[p] != '\n' && col < column) {
        col += s->doc.text[p] == '\t'   ? 4 - col % 4
               : s->doc.text[p] == '\r' ? 0
                                        : 1;
        p = ted_document_step(&s->doc, p, 1);
    }
    return p;
}
void ted_follow(ted_state_t *s)
{
    size_t line, column;
    ted_document_position(&s->doc, s->doc.cursor, &line, &column);
    column = ted_column(s, ted_document_line(&s->doc, line), s->doc.cursor);
    size_t rows = s->body.g_h / s->ch, cols = s->body.g_w / s->cw;
    if (!rows)
        rows = 1;
    if (!cols)
        cols = 1;
    if (line < s->top)
        s->top = line;
    if (line >= s->top + rows)
        s->top = line - rows + 1;
    if (column < s->left)
        s->left = column;
    if (column >= s->left + cols)
        s->left = column - cols + 1;
}
void ted_draw(ted_state_t *s)
{
    ted_layout(s);
    char title[GEM_OS_PATH_MAX + 32], status[128];
    size_t line, column, total, unused;
    ted_document_position(&s->doc, s->doc.cursor, &line, &column);
    ted_document_position(&s->doc, s->doc.length, &total, &unused);
    ++total;
    const char *leaf = strrchr(s->path, '/');
    if (!leaf)
        leaf = s->path;
    else
        ++leaf;
    snprintf(title, sizeof(title), "%s%s - Ted", s->doc.dirty ? "* " : "",
             s->path[0] ? leaf : "Untitled");
    wind_set_str(s->window, WF_NAME, title);
    snprintf(status, sizeof(status), "Line %zu  Column %zu   %zu bytes%s",
             line + 1, column + 1, s->doc.length,
             s->doc.dirty ? "  Modified" : "");
    WORD rows = s->body.g_h / s->ch;
    if (rows < 1)
        rows = 1;
    size_t maximum = total > (size_t)rows ? total - rows : 0;
    if (s->top > maximum)
        s->top = maximum;
    wind_set(s->window, WF_VSLSIZ,
             total > (size_t)rows ? (WORD)(1000 * rows / total) : 1000, 0, 0,
             0);
    wind_set(s->window, WF_VSLIDE,
             total > (size_t)rows ? (WORD)(1000 * s->top / (total - rows)) : 0,
             0, 0, 0);
    GRECT clip;
    wind_update(BEG_UPDATE);
    wind_get(s->window, WF_FIRSTXYWH, &clip.g_x, &clip.g_y, &clip.g_w,
             &clip.g_h);
    while (clip.g_w > 0 && clip.g_h > 0) {
        WORD xy[4] = {clip.g_x, clip.g_y, clip.g_x + clip.g_w - 1,
                      clip.g_y + clip.g_h - 1};
        vs_clip(s->vdi, 1, xy);
        vswr_mode(s->vdi, MD_REPLACE);
        vsf_interior(s->vdi, FIS_SOLID);
        vsf_color(s->vdi, BLACK);
        v_bar(s->vdi, xy);
        vst_color(s->vdi, WHITE);
        ted_toolbar_draw(s);
        const GRECT *band = &s->chrome.status;
        const char *hint = ted_toolbar_hint(s->hover_action);
        gemctl_status_draw(s->vdi, band, hint ? hint : status);
        WORD bodyclip[4] = {
            xy[0], xy[1] > s->body.g_y ? xy[1] : s->body.g_y, xy[2],
            xy[3] < band->g_y - 1 ? xy[3] : (WORD)(band->g_y - 1)};
        if (bodyclip[1] <= bodyclip[3]) {
            vs_clip(s->vdi, 1, bodyclip);
            size_t pos = ted_document_line(&s->doc, s->top), col = 0;
            WORD y = s->body.g_y;
            size_t first =
                s->doc.cursor < s->doc.anchor ? s->doc.cursor : s->doc.anchor;
            size_t last =
                s->doc.cursor > s->doc.anchor ? s->doc.cursor : s->doc.anchor;
            for (;;) {
                if (y + s->ch > s->body.g_y + s->body.g_h)
                    break;
                int visible = col + 4 >= s->left &&
                              col <= s->left + (size_t)(s->body.g_w / s->cw);
                WORD x = visible ? s->body.g_x + 4 +
                                       ((int)col - (int)s->left) * s->cw
                                 : 0;
                if (visible && pos == s->doc.cursor && first == last) {
                    WORD caret[4] = {x, y + 1, x, y + s->ch - 2};
                    vsl_color(s->vdi, WHITE);
                    vsl_type(s->vdi, 1);
                    vsl_width(s->vdi, 1);
                    v_pline(s->vdi, 2, caret);
                }
                if (pos >= s->doc.length)
                    break;
                unsigned char c = s->doc.text[pos];
                size_t cells = c == '\t' ? 4 - col % 4 : c == '\r' ? 0 : 1;
                int selected = pos >= first && pos < last;
                if (visible && selected) {
                    WORD box[4] = {x, y, x + (WORD)(cells * s->cw) - 1,
                                   y + s->ch - 1};
                    vsf_color(s->vdi, WHITE);
                    v_bar(s->vdi, box);
                }
                char glyph[2] = {c >= 32 && c < 127 ? (char)c : '?', 0};
                if (visible && c != '\n' && c != '\r' && c != '\t') {
                    vst_color(s->vdi, selected ? BLACK : WHITE);
                    v_gtext(s->vdi, x, y + s->ch - 3, (const BYTE *)glyph);
                }
                pos = ted_document_step(&s->doc, pos, 1);
                if (c == '\n') {
                    y += s->ch;
                    col = 0;
                } else
                    col += cells;
            }
        }
        vs_clip(s->vdi, 0, xy);
        wind_get(s->window, WF_NEXTXYWH, &clip.g_x, &clip.g_y, &clip.g_w,
                 &clip.g_h);
    }
    wind_update(END_UPDATE);
}
