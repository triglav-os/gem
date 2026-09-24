/*
 * Translates the private DESKTOP.INF compatibility model to and from the
 * public desktop.cfg schema. The configuration file remains authoritative.
 *
 * MIT License (see: LICENSE)
 * Copyright (C) 2026 tomaz stih
 */

#include "desktop_inf.h"

#include "gem/gemconf.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum { DESKTOP_INF_LOCATION_MAX = 64 };

typedef struct desktop_inf_location {
    char path[128];
    char label[DESKTOP_INF_TEXT_MAX];
} desktop_inf_location_t;

static void desktop_inf_copy(char *target, size_t size, const char *source)
{
    if (source == NULL)
        source = "";
    strncpy(target, source, size - 1u);
    target[size - 1u] = '\0';
}

static int desktop_inf_number(const gemconf_t *config, const char *section,
                              const char *key, int fallback, int minimum,
                              int maximum)
{
    long value = gemconf_get_long(config, section, key, fallback);

    if (value < minimum || value > maximum)
        return fallback;
    return (int)value;
}

static int desktop_inf_set_number(gemconf_t *config, const char *section,
                                  const char *key, int value)
{
    char text[24];

    (void)snprintf(text, sizeof(text), "%d", value);
    return gemconf_set(config, section, key, text);
}

static int desktop_inf_set_bool(gemconf_t *config, const char *section,
                                const char *key, int value)
{
    return gemconf_set(config, section, key, value != 0 ? "true" : "false");
}

static int desktop_inf_section_index(char *section, size_t size,
                                     const char *prefix, size_t index)
{
    int length = snprintf(section, size, "%s.%03u", prefix, (unsigned)index);

    return length > 0 && (size_t)length < size;
}

static int desktop_inf_section_present(const gemconf_t *config,
                                       const char *section)
{
    size_t index;

    for (index = 0u; index < gemconf_count(config); ++index) {
        const char *entry = gemconf_entry_section(config, index);

        if (entry != NULL && strcmp(entry, section) == 0)
            return 1;
    }
    return 0;
}

static int desktop_inf_compat_section(const char *section)
{
    static const char *prefixes[] = {"window.", "location.", "association.",
                                     "accessory.", "legacy."};
    size_t index;

    if (strcmp(section, "trash") == 0)
        return 1;
    for (index = 0u; index < sizeof(prefixes) / sizeof(prefixes[0]); ++index)
        if (strncmp(section, prefixes[index], strlen(prefixes[index])) == 0)
            return 1;
    return 0;
}

static void desktop_inf_remove_compat(gemconf_t *config)
{
    static const char *desktop_keys[] = {
        "desktop_inf",      "view",         "sort",
        "confirm_delete",   "confirm_copy", "confirm_overwrite",
        "double_click",     "menu_click",   "date_format",
        "time_format",      "sound",        "environment_reserved_second",
        "environment_extra"};
    size_t index = gemconf_count(config);

    while (index != 0u) {
        const char *section;
        const char *key;

        --index;
        section = gemconf_entry_section(config, index);
        key = gemconf_entry_key(config, index);
        if (section != NULL && key != NULL &&
            desktop_inf_compat_section(section))
            (void)gemconf_remove(config, section, key);
    }
    for (index = 0u; index < sizeof(desktop_keys) / sizeof(desktop_keys[0]);
         ++index)
        (void)gemconf_remove(config, "desktop", desktop_keys[index]);
}

static size_t desktop_inf_locations(const gemconf_t *config,
                                    desktop_inf_location_t *locations)
{
    size_t index;
    size_t count = 0u;

    for (index = 0u;
         index < gemconf_count(config) && count < DESKTOP_INF_LOCATION_MAX;
         ++index) {
        const char *section = gemconf_entry_section(config, index);

        if (section != NULL && strcmp(section, "locations") == 0) {
            desktop_inf_copy(locations[count].path,
                             sizeof(locations[count].path),
                             gemconf_entry_key(config, index));
            desktop_inf_copy(locations[count].label,
                             sizeof(locations[count].label),
                             gemconf_entry_value(config, index));
            ++count;
        }
    }
    return count;
}

static int desktop_inf_drive_default(size_t index)
{
    if (index < 24u)
        return 'C' + (int)index;
    if (index < 26u)
        return 'A' + (int)(index - 24u);
    return 0;
}

static int desktop_inf_image_icon(const char *path)
{
    const char *dot = strrchr(path, '.');

    if (dot == NULL)
        return 0;
    return (tolower((unsigned char)dot[1]) == 's' &&
            tolower((unsigned char)dot[2]) == 't' && dot[3] == '\0') ||
                   (tolower((unsigned char)dot[1]) == 'm' &&
                    tolower((unsigned char)dot[2]) == 's' &&
                    tolower((unsigned char)dot[3]) == 'a' && dot[4] == '\0')
               ? 1
               : 0;
}

static int desktop_inf_valid_extra(const char *value)
{
    size_t index;
    size_t length = strlen(value);

    if ((length & 1u) != 0u)
        return 0;
    for (index = 0u; index < length; ++index)
        if (!isxdigit((unsigned char)value[index]))
            return 0;
    return 1;
}

static void desktop_inf_read_preferences(const gemconf_t *config,
                                         desktop_inf_model_t *model)
{
    desktop_inf_preferences_t *p = &model->preferences;
    const char *view = gemconf_get(config, "desktop", "view", "icons");
    const char *sort = gemconf_get(config, "desktop", "sort", "name");

    p->present = 1;
    p->icon_view = strcmp(view, "text") != 0;
    p->sort = strcmp(sort, "type") == 0   ? 1
              : strcmp(sort, "size") == 0 ? 2
              : strcmp(sort, "date") == 0 ? 3
                                          : 0;
    p->confirm_delete =
        gemconf_get_bool(config, "desktop", "confirm_delete", 1);
    p->confirm_copy = gemconf_get_bool(config, "desktop", "confirm_copy", 1);
    p->double_click =
        desktop_inf_number(config, "desktop", "double_click", 2, 0, 7);
    p->confirm_overwrite =
        gemconf_get_bool(config, "desktop", "confirm_overwrite", 1);
    p->menu_click = gemconf_get_bool(config, "desktop", "menu_click", 0);
    p->month_day_date =
        strcmp(gemconf_get(config, "desktop", "date_format", "month_day"),
               "day_month") != 0;
    p->twelve_hour_time =
        strcmp(gemconf_get(config, "desktop", "time_format", "twelve_hour"),
               "twenty_four_hour") != 0;
    p->sound = gemconf_get_bool(config, "desktop", "sound", 1);
    p->reserved_second = (unsigned char)desktop_inf_number(
        config, "desktop", "environment_reserved_second", 0, 0, 255);
    p->reserved_second &= 0xe0u;
    {
        const char *extra =
            gemconf_get(config, "desktop", "environment_extra", "");

        desktop_inf_copy(p->extra_hex, sizeof(p->extra_hex),
                         desktop_inf_valid_extra(extra) ? extra : "");
    }
}

static void desktop_inf_read_windows(const gemconf_t *config,
                                     desktop_inf_model_t *model)
{
    size_t index;

    for (index = 0u; index < DESKTOP_INF_MAX_WINDOWS; ++index) {
        char section[32];
        desktop_inf_window_t *window;

        (void)desktop_inf_section_index(section, sizeof(section), "window",
                                        index);
        if (!desktop_inf_section_present(config, section))
            continue;
        window = &model->windows[model->window_count++];
        window->horizontal_scroll = desktop_inf_number(
            config, section, "horizontal_scroll", 0, -1, 255);
        window->vertical_scroll =
            desktop_inf_number(config, section, "vertical_scroll", 0, -1, 255);
        window->x = desktop_inf_number(config, section, "x", 0, -1, 255);
        window->y = desktop_inf_number(config, section, "y", 0, -1, 255);
        window->width =
            desktop_inf_number(config, section, "width", 76, -1, 255);
        window->height =
            desktop_inf_number(config, section, "height", 10, -1, 255);
        window->object =
            desktop_inf_number(config, section, "object", 0, -1, 255);
        desktop_inf_copy(window->path, sizeof(window->path),
                         gemconf_get(config, section, "path", ""));
    }
}

static int desktop_inf_location_metadata(const gemconf_t *config,
                                         const char *path, char *section,
                                         size_t section_size)
{
    size_t index;

    for (index = 0u; index < DESKTOP_INF_MAX_ITEMS; ++index) {
        const char *candidate;

        (void)desktop_inf_section_index(section, section_size, "location",
                                        index);
        candidate = gemconf_get(config, section, "path", NULL);
        if (candidate != NULL && strcmp(candidate, path) == 0)
            return 1;
    }
    section[0] = '\0';
    return 0;
}

static void desktop_inf_read_locations(const gemconf_t *config,
                                       desktop_inf_model_t *model)
{
    desktop_inf_location_t locations[DESKTOP_INF_LOCATION_MAX];
    size_t count = desktop_inf_locations(config, locations);
    size_t index;

    for (index = 0u; index < count && model->item_count < DESKTOP_INF_MAX_ITEMS;
         ++index) {
        desktop_inf_item_t *item = &model->items[model->item_count++];
        char section[32];
        int has_metadata = desktop_inf_location_metadata(
            config, locations[index].path, section, sizeof(section));

        item->code = 'M';
        item->x = has_metadata ? desktop_inf_number(config, section, "x",
                                                    (int)index, -1, 255)
                               : (int)index;
        item->y = has_metadata
                      ? desktop_inf_number(config, section, "y", 0, -1, 255)
                      : 0;
        item->application_icon =
            has_metadata
                ? desktop_inf_number(
                      config, section, "application_icon",
                      desktop_inf_image_icon(locations[index].path), -1, 255)
                : desktop_inf_image_icon(locations[index].path);
        item->document_icon =
            has_metadata ? desktop_inf_number(config, section, "document_icon",
                                              -1, -1, 255)
                         : -1;
        item->drive = has_metadata
                          ? gemconf_get(config, section, "drive", " ")[0]
                          : (char)desktop_inf_drive_default(index);
        desktop_inf_copy(item->name, sizeof(item->name),
                         locations[index].label);
    }
    for (index = 0u; index < DESKTOP_INF_MAX_ITEMS &&
                     model->item_count < DESKTOP_INF_MAX_ITEMS;
         ++index) {
        char section[32];
        const char *path;
        const char *label;
        size_t location_index;
        int mapped = 0;
        desktop_inf_item_t *item;

        (void)desktop_inf_section_index(section, sizeof(section), "location",
                                        index);
        if (!desktop_inf_section_present(config, section))
            continue;
        path = gemconf_get(config, section, "path", NULL);
        for (location_index = 0u; path != NULL && location_index < count;
             ++location_index)
            if (strcmp(path, locations[location_index].path) == 0)
                mapped = 1;
        if (mapped != 0)
            continue;
        item = &model->items[model->item_count++];
        item->code = 'M';
        item->x = desktop_inf_number(config, section, "x", (int)index, -1, 255);
        item->y = desktop_inf_number(config, section, "y", 0, -1, 255);
        item->application_icon =
            desktop_inf_number(config, section, "application_icon", 0, -1, 255);
        item->document_icon =
            desktop_inf_number(config, section, "document_icon", -1, -1, 255);
        item->drive = gemconf_get(config, section, "drive", " ")[0];
        label = gemconf_get(config, section, "label", "");
        desktop_inf_copy(item->name, sizeof(item->name), label);
    }
    if (gemconf_get_bool(config, "desktop", "show_trash", 0) != 0 &&
        model->item_count < DESKTOP_INF_MAX_ITEMS) {
        desktop_inf_item_t *item = &model->items[model->item_count++];

        item->code = 'T';
        item->x = desktop_inf_number(config, "trash", "x", 7, -1, 255);
        item->y = desktop_inf_number(config, "trash", "y", 8, -1, 255);
        item->application_icon =
            desktop_inf_number(config, "trash", "application_icon", 3, -1, 255);
        item->document_icon =
            desktop_inf_number(config, "trash", "document_icon", -1, -1, 255);
        desktop_inf_copy(item->name, sizeof(item->name),
                         gemconf_get(config, "trash", "label", "Wastebasket"));
    }
}

static char desktop_inf_kind_code(const char *kind, int full_memory)
{
    char code = strcmp(kind, "dos") == 0             ? 'F'
                : strcmp(kind, "dos_parameter") == 0 ? 'P'
                : strcmp(kind, "directory") == 0     ? 'D'
                                                     : 'G';

    if (full_memory != 0 && (code == 'F' || code == 'P'))
        code = (char)tolower((unsigned char)code);
    return code;
}

static void desktop_inf_read_associations(const gemconf_t *config,
                                          desktop_inf_model_t *model)
{
    size_t index;

    for (index = 0u; index < DESKTOP_INF_MAX_ITEMS &&
                     model->item_count < DESKTOP_INF_MAX_ITEMS;
         ++index) {
        char section[32];
        const char *executable;
        desktop_inf_item_t *item;

        (void)desktop_inf_section_index(section, sizeof(section), "association",
                                        index);
        executable = gemconf_get(config, section, "executable", NULL);
        if (executable == NULL)
            continue;
        item = &model->items[model->item_count++];
        item->code = desktop_inf_kind_code(
            gemconf_get(config, section, "kind", "gem"),
            gemconf_get_bool(config, section, "full_memory", 0));
        item->application_icon = desktop_inf_number(
            config, section, "application_icon", -1, -1, 255);
        item->document_icon =
            desktop_inf_number(config, section, "document_icon", -1, -1, 255);
        item->memory_kib =
            desktop_inf_number(config, section, "memory_kib", 0, 0, 65535);
        desktop_inf_copy(item->name, sizeof(item->name), executable);
        desktop_inf_copy(item->patterns, sizeof(item->patterns),
                         gemconf_get(config, section, "patterns", ""));
    }
}

static void desktop_inf_read_accessories(const gemconf_t *config,
                                         desktop_inf_model_t *model)
{
    size_t index;

    for (index = 0u; index < DESKTOP_INF_MAX_ACCESSORIES; ++index) {
        char section[32];
        const char *name;
        desktop_inf_accessory_t *accessory;

        (void)desktop_inf_section_index(section, sizeof(section), "accessory",
                                        index);
        name = gemconf_get(config, section, "name", NULL);
        if (name == NULL)
            continue;
        accessory = &model->accessories[model->accessory_count++];
        accessory->swap =
            desktop_inf_number(config, section, "swap", 0, -1, 255);
        desktop_inf_copy(accessory->name, sizeof(accessory->name), name);
    }
}

static int desktop_inf_hex_decode(const char *text, char *output,
                                  size_t output_size)
{
    static const char digits[] = "0123456789abcdef";
    size_t length = strlen(text);
    size_t index;

    if ((length & 1u) != 0u || length / 2u >= output_size)
        return 0;
    for (index = 0u; index < length; index += 2u) {
        const char *high = strchr(digits, tolower((unsigned char)text[index]));
        const char *low =
            strchr(digits, tolower((unsigned char)text[index + 1u]));

        if (high == NULL || low == NULL)
            return 0;
        output[index / 2u] = (char)(((high - digits) << 4) | (low - digits));
    }
    output[length / 2u] = '\0';
    return 1;
}

static void desktop_inf_read_unknown(const gemconf_t *config,
                                     desktop_inf_model_t *model)
{
    size_t index;

    for (index = 0u; index < DESKTOP_INF_MAX_UNKNOWN; ++index) {
        char section[32];
        const char *record;

        (void)desktop_inf_section_index(section, sizeof(section), "legacy",
                                        index);
        record = gemconf_get(config, section, "record_hex", NULL);
        if (record != NULL &&
            desktop_inf_hex_decode(record, model->unknown[model->unknown_count],
                                   sizeof(model->unknown[0])))
            ++model->unknown_count;
    }
}

static int desktop_inf_model_from_config(const gemconf_t *config,
                                         desktop_inf_model_t *model)
{
    memset(model, 0, sizeof(*model));
    desktop_inf_read_preferences(config, model);
    desktop_inf_read_windows(config, model);
    desktop_inf_read_accessories(config, model);
    desktop_inf_read_locations(config, model);
    desktop_inf_read_associations(config, model);
    desktop_inf_read_unknown(config, model);
    return 1;
}

static int desktop_inf_write_preferences(gemconf_t *config,
                                         const desktop_inf_model_t *model)
{
    static const char *sorts[] = {"name", "type", "size", "date"};
    const desktop_inf_preferences_t *p = &model->preferences;

    return gemconf_set(config, "desktop", "desktop_inf", "dri-gem-3") &&
           gemconf_set(config, "desktop", "view",
                       p->icon_view ? "icons" : "text") &&
           gemconf_set(config, "desktop", "sort", sorts[p->sort & 3]) &&
           desktop_inf_set_bool(config, "desktop", "confirm_delete",
                                p->confirm_delete) &&
           desktop_inf_set_bool(config, "desktop", "confirm_copy",
                                p->confirm_copy) &&
           desktop_inf_set_number(config, "desktop", "double_click",
                                  p->double_click) &&
           desktop_inf_set_bool(config, "desktop", "confirm_overwrite",
                                p->confirm_overwrite) &&
           desktop_inf_set_bool(config, "desktop", "menu_click",
                                p->menu_click) &&
           gemconf_set(config, "desktop", "date_format",
                       p->month_day_date ? "month_day" : "day_month") &&
           gemconf_set(config, "desktop", "time_format",
                       p->twelve_hour_time ? "twelve_hour"
                                           : "twenty_four_hour") &&
           desktop_inf_set_bool(config, "desktop", "sound", p->sound) &&
           desktop_inf_set_number(config, "desktop",
                                  "environment_reserved_second",
                                  p->reserved_second) &&
           gemconf_set(config, "desktop", "environment_extra", p->extra_hex);
}

static int desktop_inf_write_windows(gemconf_t *config,
                                     const desktop_inf_model_t *model)
{
    size_t index;

    for (index = 0u; index < model->window_count; ++index) {
        char section[32];
        const desktop_inf_window_t *window = &model->windows[index];

        (void)desktop_inf_section_index(section, sizeof(section), "window",
                                        index);
        if (!desktop_inf_set_number(config, section, "horizontal_scroll",
                                    window->horizontal_scroll) ||
            !desktop_inf_set_number(config, section, "vertical_scroll",
                                    window->vertical_scroll) ||
            !desktop_inf_set_number(config, section, "x", window->x) ||
            !desktop_inf_set_number(config, section, "y", window->y) ||
            !desktop_inf_set_number(config, section, "width", window->width) ||
            !desktop_inf_set_number(config, section, "height",
                                    window->height) ||
            !desktop_inf_set_number(config, section, "object",
                                    window->object) ||
            !gemconf_set(config, section, "path", window->path))
            return 0;
    }
    return 1;
}

static int desktop_inf_write_location(gemconf_t *config, const char *section,
                                      const desktop_inf_item_t *item,
                                      const char *path)
{
    char drive[2] = {item->drive != '\0' ? item->drive : ' ', '\0'};

    return (path == NULL || gemconf_set(config, section, "path", path)) &&
           gemconf_set(config, section, "drive", drive) &&
           desktop_inf_set_number(config, section, "x", item->x) &&
           desktop_inf_set_number(config, section, "y", item->y) &&
           desktop_inf_set_number(config, section, "application_icon",
                                  item->application_icon) &&
           desktop_inf_set_number(config, section, "document_icon",
                                  item->document_icon) &&
           gemconf_set(config, section, "label", item->name);
}

static const char *desktop_inf_code_kind(char code)
{
    code = (char)toupper((unsigned char)code);
    return code == 'F'   ? "dos"
           : code == 'P' ? "dos_parameter"
           : code == 'D' ? "directory"
                         : "gem";
}

static int desktop_inf_write_items(gemconf_t *config,
                                   const desktop_inf_model_t *model,
                                   const desktop_inf_location_t *locations,
                                   size_t location_count)
{
    size_t item_index;
    size_t location_index = 0u;
    size_t association_index = 0u;
    int has_trash = 0;

    for (item_index = 0u; item_index < model->item_count; ++item_index) {
        const desktop_inf_item_t *item = &model->items[item_index];
        char section[32];

        if (item->code == 'M') {
            const char *path = location_index < location_count
                                   ? locations[location_index].path
                                   : NULL;

            (void)desktop_inf_section_index(section, sizeof(section),
                                            "location", location_index);
            if (path != NULL &&
                !gemconf_set(config, "locations", path, item->name))
                return 0;
            if (!desktop_inf_write_location(config, section, item, path))
                return 0;
            ++location_index;
        } else if (item->code == 'T' && has_trash == 0) {
            has_trash = 1;
            if (!desktop_inf_set_number(config, "trash", "x", item->x) ||
                !desktop_inf_set_number(config, "trash", "y", item->y) ||
                !desktop_inf_set_number(config, "trash", "application_icon",
                                        item->application_icon) ||
                !desktop_inf_set_number(config, "trash", "document_icon",
                                        item->document_icon) ||
                !gemconf_set(config, "trash", "label", item->name))
                return 0;
        } else if (item->code != 'T') {
            (void)desktop_inf_section_index(section, sizeof(section),
                                            "association", association_index++);
            if (!gemconf_set(config, section, "kind",
                             desktop_inf_code_kind(item->code)) ||
                !gemconf_set(config, section, "executable", item->name) ||
                !gemconf_set(config, section, "patterns", item->patterns) ||
                !desktop_inf_set_number(config, section, "application_icon",
                                        item->application_icon) ||
                !desktop_inf_set_number(config, section, "document_icon",
                                        item->document_icon) ||
                !desktop_inf_set_bool(config, section, "full_memory",
                                      islower((unsigned char)item->code)) ||
                !desktop_inf_set_number(config, section, "memory_kib",
                                        item->memory_kib))
                return 0;
        }
    }
    while (location_index < location_count) {
        if (!gemconf_remove(config, "locations",
                            locations[location_index].path)) {
            return 0;
        }
        ++location_index;
    }
    return desktop_inf_set_bool(config, "desktop", "show_trash", has_trash);
}

static int desktop_inf_write_accessories(gemconf_t *config,
                                         const desktop_inf_model_t *model)
{
    size_t index;

    for (index = 0u; index < model->accessory_count; ++index) {
        char section[32];

        (void)desktop_inf_section_index(section, sizeof(section), "accessory",
                                        index);
        if (!desktop_inf_set_number(config, section, "swap",
                                    model->accessories[index].swap) ||
            !gemconf_set(config, section, "name",
                         model->accessories[index].name))
            return 0;
    }
    return 1;
}

static int desktop_inf_hex_encode(const char *input, char *output,
                                  size_t output_size)
{
    static const char digits[] = "0123456789ABCDEF";
    size_t length = strlen(input);
    size_t index;

    if (length * 2u + 1u > output_size)
        return 0;
    for (index = 0u; index < length; ++index) {
        unsigned char value = (unsigned char)input[index];

        output[index * 2u] = digits[value >> 4];
        output[index * 2u + 1u] = digits[value & 15u];
    }
    output[length * 2u] = '\0';
    return 1;
}

static int desktop_inf_write_unknown(gemconf_t *config,
                                     const desktop_inf_model_t *model)
{
    size_t index;

    for (index = 0u; index < model->unknown_count; ++index) {
        char section[32];
        char encoded[DESKTOP_INF_TEXT_MAX * 2];

        (void)desktop_inf_section_index(section, sizeof(section), "legacy",
                                        index);
        if (!desktop_inf_hex_encode(model->unknown[index], encoded,
                                    sizeof(encoded)) ||
            !gemconf_set(config, section, "record_hex", encoded))
            return 0;
    }
    return 1;
}

int desktop_inf_config_get(unsigned char *data, size_t capacity,
                           size_t *size_out)
{
    gemconf_t config;
    desktop_inf_model_t *model;
    int result;

    if (data == NULL || size_out == NULL || capacity > DESKTOP_INF_DATA_MAX)
        return 0;
    model = calloc(1u, sizeof(*model));
    if (model == NULL)
        return 0;
    gemconf_init(&config);
    result = gemconf_load(&config, "desktop") &&
             desktop_inf_model_from_config(&config, model) &&
             desktop_inf_format(model, data, capacity, size_out);
    gemconf_clear(&config);
    free(model);
    return result;
}

int desktop_inf_config_put(const unsigned char *data, size_t size)
{
    gemconf_t config;
    desktop_inf_model_t *model;
    desktop_inf_location_t locations[DESKTOP_INF_LOCATION_MAX];
    size_t location_count;
    int result;

    model = calloc(1u, sizeof(*model));
    if (model == NULL)
        return 0;
    gemconf_init(&config);
    result = desktop_inf_parse(data, size, model) &&
             gemconf_load(&config, "desktop");
    location_count = result ? desktop_inf_locations(&config, locations) : 0u;
    if (result) {
        desktop_inf_remove_compat(&config);
        result = desktop_inf_write_preferences(&config, model) &&
                 desktop_inf_write_windows(&config, model) &&
                 desktop_inf_write_items(&config, model, locations,
                                         location_count) &&
                 desktop_inf_write_accessories(&config, model) &&
                 desktop_inf_write_unknown(&config, model) &&
                 gemconf_save(&config);
    }
    gemconf_clear(&config);
    free(model);
    return result;
}
