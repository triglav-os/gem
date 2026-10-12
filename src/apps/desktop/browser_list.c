/*
 * Draws the desktop browser's four-column list view and formats file
 * modification dates, compact byte sizes and file types for display. Column
 * headings also map pointer positions to the matching sort mode.
 *
 * MIT License (see: LICENSE)
 * Copyright (C) 2026 tomaz stih
 */

#include "desktop.h"

#include <inttypes.h>
#include <stdio.h>
#include <string.h>

enum {
    DESKTOP_LIST_DATE_W = 88,
    DESKTOP_LIST_SIZE_W = 72,
    DESKTOP_LIST_TYPE_W = 64,
    DESKTOP_LIST_TEXT_PAD = 8,
    DESKTOP_LIST_CHAR_W = 8
};

typedef struct desktop_browser_columns {
    WORD name_x;
    WORD date_x;
    WORD size_x;
    WORD type_x;
} desktop_browser_columns_t;

WORD desktop_browser_band_height(void)
{
    WORD height = (WORD)(g_desktop.box_height - 2);
    WORD minimum = (WORD)(g_desktop.char_height + 4);

    if (height < minimum) {
        height = minimum;
    }
    return (height > 0) ? height : DESKTOP_BROWSER_ROW_H;
}

/* Center text vertically using the currently selected font's metrics. */
static WORD desktop_browser_text_baseline(WORD y, WORD height,
                                          const char *text)
{
    WORD distances[5] = {0};
    WORD extent[8] = {0};
    WORD text_height;
    WORD ascent;
    WORD top;

    (void)vqt_fontinfo(g_desktop.vdi_handle, NULL, NULL, distances, NULL, NULL);
    (void)vqt_extent(g_desktop.vdi_handle, (char *)text, extent);
    text_height = (WORD)(extent[5] - extent[1] + 1);
    ascent = (WORD)(distances[1] + 2);
    top = (WORD)(y + (height - text_height) / 2);
    return (WORD)(top + ascent);
}

static void desktop_browser_list_columns(
    const desktop_browser_window_t *browser, desktop_browser_columns_t *columns)
{
    WORD right;

    right = (WORD)(browser->work.g_x + browser->work.g_w);
    columns->name_x = (WORD)(browser->work.g_x + DESKTOP_LIST_TEXT_PAD);
    columns->type_x = (WORD)(right - DESKTOP_LIST_TYPE_W);
    columns->size_x = (WORD)(columns->type_x - DESKTOP_LIST_SIZE_W);
    columns->date_x = (WORD)(columns->size_x - DESKTOP_LIST_DATE_W);

    if (columns->date_x < columns->name_x + 32) {
        columns->date_x = (WORD)(columns->name_x + 32);
        columns->size_x = (WORD)(columns->date_x + DESKTOP_LIST_DATE_W);
        columns->type_x = (WORD)(columns->size_x + DESKTOP_LIST_SIZE_W);
    }
}

static void desktop_browser_list_date(const desktop_browser_entry_t *entry,
                                      char *text, size_t text_size)
{
    text[0] = '\0';
    if (entry->is_parent != 0 || entry->mtime <= 0) {
        return;
    }

    if (gem_os_format_timestamp(entry->mtime, "%Y-%m-%d", text,
                                text_size) == 0) {
        text[0] = '\0';
    }
}

static void desktop_browser_format_size(uint64_t bytes, char *text,
                                        size_t text_size)
{
    static const char *const units[] = {"B", "KB", "MB", "GB",
                                        "TB", "PB", "EB"};
    uint64_t value = bytes;
    size_t unit = 0u;

    while (value >= 1024u && unit + 1u < sizeof(units) / sizeof(units[0])) {
        value = value / 1024u + ((value % 1024u) >= 512u ? 1u : 0u);
        ++unit;
    }
    (void)snprintf(text, text_size, "%" PRIu64 " %s", value, units[unit]);
}

static void desktop_browser_list_size(const desktop_browser_entry_t *entry,
                                      char *text, size_t text_size)
{
    text[0] = '\0';
    if (entry->is_parent != 0 || entry->is_dir != 0) {
        return;
    }
    desktop_browser_format_size(entry->size_bytes, text, text_size);
}

static void desktop_browser_list_type(const desktop_browser_entry_t *entry,
                                      char *text, size_t text_size)
{
    const char *dot;

    if (entry->is_parent != 0) {
        (void)snprintf(text, text_size, "Parent");
        return;
    }
    if (entry->is_dir != 0) {
        (void)snprintf(text, text_size, "Folder");
        return;
    }
    if (entry->is_executable != 0) {
        (void)snprintf(text, text_size, "Program");
        return;
    }

    dot = strrchr(entry->name, '.');
    if (dot != NULL && dot != entry->name && dot[1] != '\0') {
        (void)snprintf(text, text_size, "%.*s", (int)text_size - 1, dot + 1);
    } else {
        (void)snprintf(text, text_size, "File");
    }
}

void desktop_browser_list_draw_header(const desktop_browser_window_t *browser,
                                      WORD clip_y0, WORD clip_y1)
{
    desktop_browser_columns_t columns;
    WORD fill[4];
    WORD separator[4];
    WORD bottom;
    WORD baseline;
    WORD band_height;

    if (browser == NULL) {
        return;
    }
    band_height = desktop_browser_band_height();
    bottom = (WORD)(browser->work.g_y + band_height - 1);
    if (bottom < clip_y0 || browser->work.g_y > clip_y1) {
        return;
    }

    desktop_browser_list_columns(browser, &columns);
    fill[0] = browser->work.g_x;
    fill[1] = browser->work.g_y;
    fill[2] = (WORD)(browser->work.g_x + browser->work.g_w - 1);
    fill[3] = bottom;
    vsf_color(g_desktop.vdi_handle, BLACK);
    vr_recfl(g_desktop.vdi_handle, fill);
    (void)vst_font(g_desktop.vdi_handle, DESKTOP_SYSTEM_FONT);
    vst_color(g_desktop.vdi_handle, WHITE);
    baseline = desktop_browser_text_baseline(browser->work.g_y, band_height,
                                             "Name");
    v_gtext(g_desktop.vdi_handle, columns.name_x, baseline,
            (const BYTE *)"Name");
    v_gtext(g_desktop.vdi_handle, columns.date_x, baseline,
            (const BYTE *)"Date");
    v_gtext(g_desktop.vdi_handle, columns.size_x, baseline,
            (const BYTE *)"Size");
    v_gtext(g_desktop.vdi_handle, columns.type_x, baseline,
            (const BYTE *)"Type");

    separator[0] = browser->work.g_x;
    separator[1] = bottom;
    separator[2] = (WORD)(browser->work.g_x + browser->work.g_w - 1);
    separator[3] = bottom;
    vsl_color(g_desktop.vdi_handle, WHITE);
    v_pline(g_desktop.vdi_handle, 2, separator);
}

void desktop_browser_list_draw_row(const desktop_browser_window_t *browser,
                                   const desktop_browser_entry_t *entry,
                                   WORD row_y)
{
    desktop_browser_columns_t columns;
    char name[64];
    char date[16];
    char size[24];
    char type[16];
    int name_chars;
    WORD baseline;

    if (browser == NULL || entry == NULL) {
        return;
    }

    desktop_browser_list_columns(browser, &columns);
    name_chars = (columns.date_x - columns.name_x) / DESKTOP_LIST_CHAR_W - 2;
    if (name_chars < 0) {
        name_chars = 0;
    }
    if ((size_t)name_chars > sizeof(name) - 3u) {
        name_chars = (int)sizeof(name) - 3;
    }
    if (entry->is_dir != 0 && entry->is_parent == 0) {
        (void)snprintf(name, sizeof(name), "%s%.*s",
                       desktop_browser_entry_prefix(browser, entry),
                       name_chars, entry->name);
    } else {
        (void)snprintf(name, sizeof(name), "%s %.*s",
                       desktop_browser_entry_prefix(browser, entry),
                       name_chars, entry->name);
    }
    desktop_browser_list_date(entry, date, sizeof(date));
    desktop_browser_list_size(entry, size, sizeof(size));
    desktop_browser_list_type(entry, type, sizeof(type));

    baseline = desktop_browser_text_baseline(row_y, DESKTOP_BROWSER_ROW_H,
                                             name);
    v_gtext(g_desktop.vdi_handle, columns.name_x, baseline,
            (const BYTE *)name);
    v_gtext(g_desktop.vdi_handle, columns.date_x, baseline,
            (const BYTE *)date);
    v_gtext(g_desktop.vdi_handle, columns.size_x, baseline,
            (const BYTE *)size);
    v_gtext(g_desktop.vdi_handle, columns.type_x, baseline,
            (const BYTE *)type);
}

WORD desktop_browser_list_sort_at(const desktop_browser_window_t *browser,
                                  WORD x)
{
    desktop_browser_columns_t columns;

    if (browser == NULL || x < browser->work.g_x ||
        x >= browser->work.g_x + browser->work.g_w) {
        return NIL;
    }

    desktop_browser_list_columns(browser, &columns);
    if (x < columns.date_x) {
        return DESKTOP_SORT_NAME;
    }
    if (x < columns.size_x) {
        return DESKTOP_SORT_DATE;
    }
    if (x < columns.type_x) {
        return DESKTOP_SORT_SIZE;
    }
    return DESKTOP_SORT_TYPE;
}

void desktop_browser_draw_status(const desktop_browser_window_t *browser,
                                 WORD clip_y0, WORD clip_y1)
{
    char total_text[16];
    char status[96];
    uint64_t total_bytes = 0u;
    WORD files = 0;
    WORD folders = 0;
    WORD band_height;
    WORD top;
    WORD bottom;
    WORD fill[4];
    WORD separator[4];
    WORD baseline;
    WORD index;

    if (browser == NULL) {
        return;
    }

    band_height = desktop_browser_band_height();
    top = (WORD)(browser->work.g_y + browser->work.g_h - band_height);
    bottom = (WORD)(browser->work.g_y + browser->work.g_h - 1);
    if (bottom < clip_y0 || top > clip_y1) {
        return;
    }

    for (index = 0; index < browser->entry_count; ++index) {
        const desktop_browser_entry_t *entry = &browser->entries[index];

        if (entry->is_parent != 0) {
            continue;
        }
        if (entry->is_dir != 0) {
            ++folders;
        } else {
            ++files;
            if (UINT64_MAX - total_bytes < entry->size_bytes) {
                total_bytes = UINT64_MAX;
            } else {
                total_bytes += entry->size_bytes;
            }
        }
    }

    desktop_browser_format_size(total_bytes, total_text, sizeof(total_text));
    (void)snprintf(status, sizeof(status), "%d file%s, %d folder%s, %s total",
                   files, files == 1 ? "" : "s", folders,
                   folders == 1 ? "" : "s", total_text);

    fill[0] = browser->work.g_x;
    fill[1] = top;
    fill[2] = (WORD)(browser->work.g_x + browser->work.g_w - 1);
    fill[3] = bottom;
    vsf_color(g_desktop.vdi_handle, BLACK);
    v_bar(g_desktop.vdi_handle, fill);
    separator[0] = browser->work.g_x;
    separator[1] = top;
    separator[2] = fill[2];
    separator[3] = top;
    (void)vsl_type(g_desktop.vdi_handle, 1);
    (void)vsl_width(g_desktop.vdi_handle, 1);
    vsl_color(g_desktop.vdi_handle, WHITE);
    v_pline(g_desktop.vdi_handle, 2, separator);

    (void)vst_font(g_desktop.vdi_handle, DESKTOP_SYSTEM_FONT);
    vst_color(g_desktop.vdi_handle, WHITE);
    baseline = desktop_browser_text_baseline(top, band_height, status);
    v_gtext(g_desktop.vdi_handle, (WORD)(browser->work.g_x + 6), baseline,
            (const BYTE *)status);
}
