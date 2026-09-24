/*
 * Parses and writes the bounded line-oriented DRI GEM/3 DESKTOP.INF format,
 * including FreeGEM trash and extended-environment records.
 *
 * MIT License (see: LICENSE)
 * Copyright (C) 2026 tomaz stih
 */

#include "desktop_inf.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>

typedef struct desktop_inf_writer {
    unsigned char *data;
    size_t capacity;
    size_t used;
} desktop_inf_writer_t;

static int desktop_inf_hex_value(unsigned char value)
{
    if (value >= '0' && value <= '9')
        return value - '0';
    value = (unsigned char)toupper(value);
    return value >= 'A' && value <= 'F' ? value - 'A' + 10 : -1;
}

static int desktop_inf_scan_byte(const char **cursor, const char *end,
                                 int *value)
{
    int high;
    int low;

    if (end - *cursor < 2)
        return 0;
    high = desktop_inf_hex_value((unsigned char)(*cursor)[0]);
    low = desktop_inf_hex_value((unsigned char)(*cursor)[1]);
    if (high < 0 || low < 0)
        return 0;
    *value = (high << 4) | low;
    if (*value == 255)
        *value = -1;
    *cursor += 2;
    return 1;
}

static void desktop_inf_skip_spaces(const char **cursor, const char *end)
{
    while (*cursor < end && (**cursor == ' ' || **cursor == '\t'))
        ++*cursor;
}

static int desktop_inf_scan_string(const char **cursor, const char *end,
                                   char *out, size_t out_size)
{
    const char *start;
    size_t length;

    desktop_inf_skip_spaces(cursor, end);
    start = *cursor;
    while (*cursor < end && **cursor != '@')
        ++*cursor;
    if (*cursor == end)
        return 0;
    length = (size_t)(*cursor - start);
    if (length >= out_size)
        return 0;
    memcpy(out, start, length);
    out[length] = '\0';
    ++*cursor;
    return 1;
}

static int desktop_inf_parse_environment(const char *cursor, const char *end,
                                         desktop_inf_model_t *model)
{
    desktop_inf_preferences_t *preferences = &model->preferences;
    int first;
    int second;
    size_t extra_length;
    const char *extra;

    if (!desktop_inf_scan_byte(&cursor, end, &first) || first < 0 ||
        !desktop_inf_scan_byte(&cursor, end, &second) || second < 0)
        return 0;
    extra = cursor;
    while (cursor < end && isxdigit((unsigned char)*cursor))
        ++cursor;
    extra_length = (size_t)(cursor - extra);
    desktop_inf_skip_spaces(&cursor, end);
    if (cursor != end || (extra_length & 1u) != 0u ||
        extra_length >= sizeof(preferences->extra_hex))
        return 0;
    preferences->present = 1;
    preferences->icon_view = (first & 0x80) != 0;
    preferences->sort = (first >> 5) & 3;
    preferences->confirm_delete = (first & 0x10) != 0;
    preferences->confirm_copy = (first & 0x08) != 0;
    preferences->double_click = first & 7;
    preferences->confirm_overwrite = (second & 0x10) == 0;
    preferences->menu_click = (second & 0x08) != 0;
    preferences->month_day_date = (second & 0x04) == 0;
    preferences->twelve_hour_time = (second & 0x02) == 0;
    preferences->sound = (second & 0x01) == 0;
    preferences->reserved_second = (unsigned char)(second & 0xe0);
    memcpy(preferences->extra_hex, extra, extra_length);
    preferences->extra_hex[extra_length] = '\0';
    return 1;
}

static int desktop_inf_parse_window(const char *cursor, const char *end,
                                    desktop_inf_model_t *model)
{
    desktop_inf_window_t *window;

    if (model->window_count >= DESKTOP_INF_MAX_WINDOWS)
        return 0;
    window = &model->windows[model->window_count];
    if (!desktop_inf_scan_byte(&cursor, end, &window->horizontal_scroll) ||
        !desktop_inf_scan_byte(&cursor, end, &window->vertical_scroll) ||
        !desktop_inf_scan_byte(&cursor, end, &window->x) ||
        !desktop_inf_scan_byte(&cursor, end, &window->y) ||
        !desktop_inf_scan_byte(&cursor, end, &window->width) ||
        !desktop_inf_scan_byte(&cursor, end, &window->height) ||
        !desktop_inf_scan_byte(&cursor, end, &window->object) ||
        !desktop_inf_scan_string(&cursor, end, window->path,
                                 sizeof(window->path)))
        return 0;
    desktop_inf_skip_spaces(&cursor, end);
    if (cursor != end)
        return 0;
    ++model->window_count;
    return 1;
}

static int desktop_inf_parse_accessory(const char *cursor, const char *end,
                                       desktop_inf_model_t *model)
{
    desktop_inf_accessory_t *accessory;

    if (model->accessory_count >= DESKTOP_INF_MAX_ACCESSORIES)
        return 0;
    accessory = &model->accessories[model->accessory_count];
    if (!desktop_inf_scan_byte(&cursor, end, &accessory->swap) ||
        !desktop_inf_scan_string(&cursor, end, accessory->name,
                                 sizeof(accessory->name)))
        return 0;
    desktop_inf_skip_spaces(&cursor, end);
    if (cursor != end)
        return 0;
    ++model->accessory_count;
    return 1;
}

static int desktop_inf_parse_item(char code, const char *cursor,
                                  const char *end, desktop_inf_model_t *model)
{
    desktop_inf_item_t *item;
    int desktop_item = code == 'M' || code == 'T';
    int high;
    int low;

    if (model->item_count >= DESKTOP_INF_MAX_ITEMS)
        return 0;
    item = &model->items[model->item_count];
    item->code = code;
    if (desktop_item && (!desktop_inf_scan_byte(&cursor, end, &item->x) ||
                         !desktop_inf_scan_byte(&cursor, end, &item->y)))
        return 0;
    if (!desktop_inf_scan_byte(&cursor, end, &item->application_icon) ||
        !desktop_inf_scan_byte(&cursor, end, &item->document_icon))
        return 0;
    desktop_inf_skip_spaces(&cursor, end);
    if (desktop_item) {
        if (cursor == end)
            return 0;
        item->drive = *cursor == ' ' ? '\0' : *cursor;
        ++cursor;
        desktop_inf_skip_spaces(&cursor, end);
    }
    if (!desktop_inf_scan_string(&cursor, end, item->name,
                                 sizeof(item->name)) ||
        !desktop_inf_scan_string(&cursor, end, item->patterns,
                                 sizeof(item->patterns)))
        return 0;
    desktop_inf_skip_spaces(&cursor, end);
    if (cursor < end && *cursor == 'R') {
        ++cursor;
        if (!desktop_inf_scan_byte(&cursor, end, &high) || high < 0 ||
            !desktop_inf_scan_byte(&cursor, end, &low) || low < 0)
            return 0;
        item->memory_kib = (high << 8) | low;
        desktop_inf_skip_spaces(&cursor, end);
    }
    if (cursor != end)
        return 0;
    ++model->item_count;
    return 1;
}

static int desktop_inf_parse_line(const char *line, size_t length,
                                  desktop_inf_model_t *model)
{
    const char *cursor = line;
    const char *end = line + length;
    char code;

    while (cursor < end && (*cursor == ' ' || *cursor == '\t'))
        ++cursor;
    if (cursor == end)
        return 1;
    if (end - cursor < 2 || cursor[0] != '#')
        return 0;
    code = cursor[1];
    cursor += 2;
    switch (code) {
        case 'E':
            return desktop_inf_parse_environment(cursor, end, model);
        case 'W':
            return desktop_inf_parse_window(cursor, end, model);
        case 'A':
            return desktop_inf_parse_accessory(cursor, end, model);
        case 'M':
        case 'T':
        case 'G':
        case 'F':
        case 'f':
        case 'P':
        case 'p':
        case 'D':
            return desktop_inf_parse_item(code, cursor, end, model);
        default:
            if (model->unknown_count >= DESKTOP_INF_MAX_UNKNOWN ||
                length >= sizeof(model->unknown[0]))
                return 0;
            memcpy(model->unknown[model->unknown_count], line, length);
            model->unknown[model->unknown_count][length] = '\0';
            ++model->unknown_count;
            return 1;
    }
}

int desktop_inf_parse(const unsigned char *data, size_t size,
                      desktop_inf_model_t *model)
{
    size_t start = 0u;
    size_t index;

    if (data == NULL || model == NULL || size == 0u ||
        size > DESKTOP_INF_DATA_MAX)
        return 0;
    memset(model, 0, sizeof(*model));
    for (index = 0u; index <= size; ++index) {
        int at_end = index == size || data[index] == 0u || data[index] == 0x1au;
        int at_line = !at_end && data[index] == '\n';
        size_t length;

        if (!at_end && !at_line) {
            unsigned char value = data[index];
            if (value != '\r' && value != '\t' &&
                (value < 0x20u || value > 0x7eu))
                return 0;
            continue;
        }
        length = index - start;
        if (length != 0u && data[start + length - 1u] == '\r')
            --length;
        if (length >= DESKTOP_INF_TEXT_MAX ||
            !desktop_inf_parse_line((const char *)data + start, length, model))
            return 0;
        if (at_end)
            break;
        start = index + 1u;
    }
    return model->preferences.present || model->window_count != 0u ||
           model->item_count != 0u || model->accessory_count != 0u ||
           model->unknown_count != 0u;
}

static int desktop_inf_write(desktop_inf_writer_t *writer, const char *text,
                             size_t length)
{
    if (length > writer->capacity - writer->used)
        return 0;
    memcpy(writer->data + writer->used, text, length);
    writer->used += length;
    return 1;
}

static int desktop_inf_write_text(desktop_inf_writer_t *writer,
                                  const char *text)
{
    return desktop_inf_write(writer, text, strlen(text));
}

static int desktop_inf_write_byte(desktop_inf_writer_t *writer, int value)
{
    static const char digits[] = "0123456789ABCDEF";
    char text[2];
    unsigned byte = value < 0 ? 255u : (unsigned)value & 255u;

    text[0] = digits[byte >> 4];
    text[1] = digits[byte & 15u];
    return desktop_inf_write(writer, text, sizeof(text));
}

static int desktop_inf_write_string(desktop_inf_writer_t *writer,
                                    const char *value)
{
    const char *cursor;

    for (cursor = value; *cursor != '\0'; ++cursor) {
        char output = *cursor == '@' ? '?' : *cursor;
        if (!desktop_inf_write(writer, &output, 1u))
            return 0;
    }
    return desktop_inf_write_text(writer, "@ ");
}

static int desktop_inf_format_environment(const desktop_inf_model_t *model,
                                          desktop_inf_writer_t *writer)
{
    const desktop_inf_preferences_t *p = &model->preferences;
    int first = (p->icon_view ? 0x80 : 0) | ((p->sort & 3) << 5) |
                (p->confirm_delete ? 0x10 : 0) | (p->confirm_copy ? 0x08 : 0) |
                (p->double_click & 7);
    int second = p->reserved_second | (p->confirm_overwrite ? 0 : 0x10) |
                 (p->menu_click ? 0x08 : 0) | (p->month_day_date ? 0 : 0x04) |
                 (p->twelve_hour_time ? 0 : 0x02) | (p->sound ? 0 : 1);

    return desktop_inf_write_text(writer, "#E") &&
           desktop_inf_write_byte(writer, first) &&
           desktop_inf_write_byte(writer, second) &&
           desktop_inf_write_text(writer, p->extra_hex) &&
           desktop_inf_write_text(writer, "\r\n");
}

static int desktop_inf_format_window(const desktop_inf_window_t *window,
                                     desktop_inf_writer_t *writer)
{
    return desktop_inf_write_text(writer, "#W") &&
           desktop_inf_write_byte(writer, window->horizontal_scroll) &&
           desktop_inf_write_byte(writer, window->vertical_scroll) &&
           desktop_inf_write_byte(writer, window->x) &&
           desktop_inf_write_byte(writer, window->y) &&
           desktop_inf_write_byte(writer, window->width) &&
           desktop_inf_write_byte(writer, window->height) &&
           desktop_inf_write_byte(writer, window->object) &&
           desktop_inf_write_text(writer, " ") &&
           desktop_inf_write_string(writer, window->path) &&
           desktop_inf_write_text(writer, "\r\n");
}

static int desktop_inf_format_item(const desktop_inf_item_t *item,
                                   desktop_inf_writer_t *writer)
{
    int desktop_item = item->code == 'M' || item->code == 'T';
    char prefix[2] = {'#', item->code};
    char drive[2] = {item->drive != '\0' ? item->drive : ' ', ' '};

    if (!desktop_inf_write(writer, prefix, sizeof(prefix)) ||
        (desktop_item && (!desktop_inf_write_byte(writer, item->x) ||
                          !desktop_inf_write_byte(writer, item->y))) ||
        !desktop_inf_write_byte(writer, item->application_icon) ||
        !desktop_inf_write_byte(writer, item->document_icon) ||
        !desktop_inf_write_text(writer, " ") ||
        (desktop_item && !desktop_inf_write(writer, drive, sizeof(drive))) ||
        !desktop_inf_write_string(writer, item->name) ||
        !desktop_inf_write_string(writer, item->patterns))
        return 0;
    if (item->memory_kib != 0 &&
        (!desktop_inf_write_text(writer, "R") ||
         !desktop_inf_write_byte(writer, item->memory_kib >> 8) ||
         !desktop_inf_write_byte(writer, item->memory_kib)))
        return 0;
    return desktop_inf_write_text(writer, "\r\n");
}

int desktop_inf_format(const desktop_inf_model_t *model, unsigned char *data,
                       size_t capacity, size_t *size_out)
{
    desktop_inf_writer_t writer = {data, capacity, 0u};
    size_t index;
    const unsigned char ending[2] = {0x1a, 0x00};

    if (model == NULL || data == NULL || size_out == NULL ||
        capacity > DESKTOP_INF_DATA_MAX)
        return 0;
    if (!desktop_inf_format_environment(model, &writer))
        return 0;
    for (index = 0u; index < model->window_count; ++index)
        if (!desktop_inf_format_window(&model->windows[index], &writer))
            return 0;
    for (index = 0u; index < model->accessory_count; ++index) {
        if (!desktop_inf_write_text(&writer, "#A") ||
            !desktop_inf_write_byte(&writer, model->accessories[index].swap) ||
            !desktop_inf_write_text(&writer, " ") ||
            !desktop_inf_write_string(&writer,
                                      model->accessories[index].name) ||
            !desktop_inf_write_text(&writer, "\r\n"))
            return 0;
    }
    for (index = 0u; index < model->item_count; ++index)
        if (!desktop_inf_format_item(&model->items[index], &writer))
            return 0;
    for (index = 0u; index < model->unknown_count; ++index)
        if (!desktop_inf_write_text(&writer, model->unknown[index]) ||
            !desktop_inf_write_text(&writer, "\r\n"))
            return 0;
    if (!desktop_inf_write(&writer, (const char *)ending, sizeof(ending)))
        return 0;
    *size_out = writer.used;
    return 1;
}
