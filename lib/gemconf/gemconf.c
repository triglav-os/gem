/*
 * Implements XDG component configuration with a deliberately small INI
 * grammar: sections, key=value pairs, blank lines and # or ; comments.
 * Files are bounded, merged in precedence order and saved atomically.
 *
 * MIT License (see: LICENSE)
 * Copyright (C) 2026 tomaz stih
 */

#include "gem/gemconf.h"

#include "platform/os.h"

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum {
    GEMCONF_FILE_MAX = 65536,
    GEMCONF_SECTION_MAX = 63,
    GEMCONF_KEY_MAX = 127,
    GEMCONF_VALUE_MAX = 1023,
    GEMCONF_SYSTEM_DIRS_MAX = 16
};

typedef struct gemconf_entry {
    char section[GEMCONF_SECTION_MAX + 1];
    char key[GEMCONF_KEY_MAX + 1];
    char value[GEMCONF_VALUE_MAX + 1];
} gemconf_entry_t;

static gemconf_entry_t *gemconf_entries(const gemconf_t *config)
{
    return config != NULL ? (gemconf_entry_t *)config->entries : NULL;
}

static char *gemconf_trim(char *text)
{
    char *end;

    while (*text != '\0' && isspace((unsigned char)*text)) {
        ++text;
    }
    end = text + strlen(text);
    while (end > text && isspace((unsigned char)end[-1])) {
        --end;
    }
    *end = '\0';
    return text;
}

static int gemconf_component_valid(const char *component)
{
    size_t i;
    size_t length;

    if (component == NULL) {
        return 0;
    }
    length = strlen(component);
    if (length == 0u || length >= sizeof(((gemconf_t *)0)->component)) {
        return 0;
    }
    for (i = 0u; i < length; ++i) {
        unsigned char value = (unsigned char)component[i];

        if (!isalnum(value) && value != '-' && value != '_') {
            return 0;
        }
    }
    return 1;
}

static int gemconf_copy(char *target, size_t size, const char *value)
{
    size_t length;

    if (target == NULL || size == 0u || value == NULL) {
        return 0;
    }
    length = strlen(value);
    if (length >= size) {
        return 0;
    }
    memcpy(target, value, length + 1u);
    return 1;
}

static int gemconf_reserve(gemconf_t *config, size_t wanted)
{
    gemconf_entry_t *entries;
    size_t capacity;

    if (config->capacity >= wanted) {
        return 1;
    }
    capacity = config->capacity != 0u ? config->capacity * 2u : 16u;
    while (capacity < wanted) {
        capacity *= 2u;
    }
    entries = realloc(config->entries, capacity * sizeof(*entries));
    if (entries == NULL) {
        return 0;
    }
    config->entries = entries;
    config->capacity = capacity;
    return 1;
}

static size_t gemconf_find(const gemconf_t *config, const char *section,
                           const char *key)
{
    gemconf_entry_t *entries = gemconf_entries(config);
    size_t i;

    if (config == NULL || section == NULL || key == NULL) {
        return (size_t)-1;
    }
    for (i = 0u; i < config->count; ++i) {
        if (strcmp(entries[i].section, section) == 0 &&
            strcmp(entries[i].key, key) == 0) {
            return i;
        }
    }
    return (size_t)-1;
}

static int gemconf_path_for(const char *base, const char *component, char *path,
                            size_t path_size)
{
    int length;

    if (base == NULL || base[0] != '/' || component == NULL || path == NULL ||
        path_size == 0u) {
        return 0;
    }
    length = snprintf(path, path_size, "%s/gem/%s.cfg", base, component);
    return length > 0 && (size_t)length < path_size;
}

static int gemconf_user_base(char *base, size_t base_size)
{
    return gem_os_user_config_dir(base, base_size);
}

static int gemconf_parse(gemconf_t *config, char *text)
{
    char section[GEMCONF_SECTION_MAX + 1] = "default";
    char *line = text;

    while (line != NULL && *line != '\0') {
        char *next = strchr(line, '\n');
        char *trimmed;

        if (next != NULL) {
            *next++ = '\0';
        }
        if (line[0] != '\0' && line[strlen(line) - 1u] == '\r') {
            line[strlen(line) - 1u] = '\0';
        }
        trimmed = gemconf_trim(line);
        if (*trimmed == '\0' || *trimmed == '#' || *trimmed == ';') {
            line = next;
            continue;
        }
        if (*trimmed == '[') {
            char *end = strrchr(trimmed + 1, ']');

            if (end != NULL) {
                *end = '\0';
                trimmed = gemconf_trim(trimmed + 1);
                if (*trimmed != '\0' &&
                    gemconf_copy(section, sizeof(section), trimmed) == 0) {
                    return 0;
                }
            }
        } else {
            char *equals = strchr(trimmed, '=');

            if (equals != NULL) {
                char *key;
                char *value;

                *equals = '\0';
                key = gemconf_trim(trimmed);
                value = gemconf_trim(equals + 1);
                if (*key != '\0' &&
                    gemconf_set(config, section, key, value) == 0) {
                    return 0;
                }
            }
        }
        line = next;
    }
    return 1;
}

static int gemconf_load_file(gemconf_t *config, const char *path)
{
    char *text;
    int result;

    text = malloc(GEMCONF_FILE_MAX + 1u);
    if (text == NULL) {
        return 0;
    }
    if (gem_os_read_file(path, text, GEMCONF_FILE_MAX + 1u, NULL) == 0) {
        result = gem_os_path_exists(path) == 0;
        free(text);
        return result;
    }
    result = gemconf_parse(config, text);
    free(text);
    return result;
}

void gemconf_init(gemconf_t *config)
{
    if (config != NULL) {
        memset(config, 0, sizeof(*config));
    }
}

void gemconf_clear(gemconf_t *config)
{
    if (config != NULL) {
        free(config->entries);
        memset(config, 0, sizeof(*config));
    }
}

int gemconf_load(gemconf_t *config, const char *component)
{
    char directories[2048];
    char *items[GEMCONF_SYSTEM_DIRS_MAX];
    size_t item_count = 0u;
    char *cursor;
    char path[GEM_OS_PATH_MAX];
    char base[GEM_OS_PATH_MAX];

    if (config == NULL || gemconf_component_valid(component) == 0) {
        return 0;
    }
    gemconf_clear(config);
    if (gemconf_copy(config->component, sizeof(config->component), component) ==
        0) {
        return 0;
    }

    if (gem_os_system_config_dirs(directories, sizeof(directories)) == 0) {
        return 0;
    }
    cursor = directories;
    while (cursor != NULL && item_count < GEMCONF_SYSTEM_DIRS_MAX) {
        char *next = strchr(cursor, ':');

        if (next != NULL) {
            *next++ = '\0';
        }
        if (cursor[0] == '/') {
            items[item_count++] = cursor;
        }
        cursor = next;
    }
    while (item_count > 0u) {
        --item_count;
        if (gemconf_path_for(items[item_count], component, path,
                             sizeof(path)) != 0 &&
            gemconf_load_file(config, path) == 0) {
            gemconf_clear(config);
            return 0;
        }
    }
    if (gemconf_user_base(base, sizeof(base)) != 0 &&
        gemconf_path_for(base, component, path, sizeof(path)) != 0 &&
        gemconf_load_file(config, path) == 0) {
        gemconf_clear(config);
        return 0;
    }
    return 1;
}

const char *gemconf_get(const gemconf_t *config, const char *section,
                        const char *key, const char *fallback)
{
    size_t index = gemconf_find(config, section, key);

    return index != (size_t)-1 ? gemconf_entries(config)[index].value
                               : fallback;
}

static int gemconf_equal_folded(const char *left, const char *right)
{
    while (*left != '\0' && *right != '\0') {
        if (tolower((unsigned char)*left) != tolower((unsigned char)*right)) {
            return 0;
        }
        ++left;
        ++right;
    }
    return *left == '\0' && *right == '\0';
}

int gemconf_get_bool(const gemconf_t *config, const char *section,
                     const char *key, int fallback)
{
    const char *value = gemconf_get(config, section, key, NULL);

    if (value == NULL) {
        return fallback != 0;
    }
    if (strcmp(value, "1") == 0 || gemconf_equal_folded(value, "true") ||
        gemconf_equal_folded(value, "yes") ||
        gemconf_equal_folded(value, "on")) {
        return 1;
    }
    if (strcmp(value, "0") == 0 || gemconf_equal_folded(value, "false") ||
        gemconf_equal_folded(value, "no") ||
        gemconf_equal_folded(value, "off")) {
        return 0;
    }
    return fallback != 0;
}

long gemconf_get_long(const gemconf_t *config, const char *section,
                      const char *key, long fallback)
{
    const char *value = gemconf_get(config, section, key, NULL);
    char *end;
    long parsed;

    if (value == NULL || *value == '\0') {
        return fallback;
    }
    errno = 0;
    parsed = strtol(value, &end, 10);
    while (*end != '\0' && isspace((unsigned char)*end)) {
        ++end;
    }
    return errno == 0 && *end == '\0' ? parsed : fallback;
}

int gemconf_set(gemconf_t *config, const char *section, const char *key,
                const char *value)
{
    gemconf_entry_t *entry;
    size_t index;

    if (config == NULL || section == NULL || key == NULL || value == NULL ||
        *section == '\0' || *key == '\0' || strchr(section, '\n') != NULL ||
        strchr(section, ']') != NULL || strchr(key, '\n') != NULL ||
        strchr(key, '=') != NULL || strchr(value, '\n') != NULL ||
        strlen(section) > GEMCONF_SECTION_MAX ||
        strlen(key) > GEMCONF_KEY_MAX || strlen(value) > GEMCONF_VALUE_MAX) {
        return 0;
    }
    index = gemconf_find(config, section, key);
    if (index == (size_t)-1) {
        if (gemconf_reserve(config, config->count + 1u) == 0) {
            return 0;
        }
        index = config->count++;
        memset(&gemconf_entries(config)[index], 0,
               sizeof(gemconf_entries(config)[index]));
    }
    entry = &gemconf_entries(config)[index];
    return gemconf_copy(entry->section, sizeof(entry->section), section) &&
           gemconf_copy(entry->key, sizeof(entry->key), key) &&
           gemconf_copy(entry->value, sizeof(entry->value), value);
}

int gemconf_remove(gemconf_t *config, const char *section, const char *key)
{
    size_t index = gemconf_find(config, section, key);

    if (index == (size_t)-1) {
        return 0;
    }
    if (index + 1u < config->count) {
        memmove(&gemconf_entries(config)[index],
                &gemconf_entries(config)[index + 1u],
                (config->count - index - 1u) * sizeof(gemconf_entry_t));
    }
    --config->count;
    return 1;
}

size_t gemconf_count(const gemconf_t *config)
{
    return config != NULL ? config->count : 0u;
}

const char *gemconf_entry_section(const gemconf_t *config, size_t index)
{
    return config != NULL && index < config->count
               ? gemconf_entries(config)[index].section
               : NULL;
}

const char *gemconf_entry_key(const gemconf_t *config, size_t index)
{
    return config != NULL && index < config->count
               ? gemconf_entries(config)[index].key
               : NULL;
}

const char *gemconf_entry_value(const gemconf_t *config, size_t index)
{
    return config != NULL && index < config->count
               ? gemconf_entries(config)[index].value
               : NULL;
}

int gemconf_user_path(const gemconf_t *config, char *path, size_t path_size)
{
    char base[GEM_OS_PATH_MAX];

    return config != NULL && config->component[0] != '\0' &&
           gemconf_user_base(base, sizeof(base)) != 0 &&
           gemconf_path_for(base, config->component, path, path_size);
}

static int gemconf_append(char *text, size_t size, size_t *used,
                          const char *value)
{
    size_t length = strlen(value);

    if (*used >= size || length >= size - *used) {
        return 0;
    }
    memcpy(text + *used, value, length);
    *used += length;
    text[*used] = '\0';
    return 1;
}

int gemconf_save(const gemconf_t *config)
{
    char base[GEM_OS_PATH_MAX];
    char directory[GEM_OS_PATH_MAX];
    char path[GEM_OS_PATH_MAX];
    char *text;
    size_t capacity;
    size_t used = 0u;
    size_t i;
    const char *last_section = NULL;
    int result = 0;
    int directory_length;

    if (config == NULL || config->component[0] == '\0' ||
        gemconf_user_base(base, sizeof(base)) == 0) {
        return 0;
    }
    directory_length = snprintf(directory, sizeof(directory), "%s/gem", base);
    if (directory_length <= 0 ||
        (size_t)directory_length >= sizeof(directory) ||
        gemconf_path_for(base, config->component, path, sizeof(path)) == 0 ||
        gem_os_mkdir_p(directory) == 0) {
        return 0;
    }
    capacity = 64u;
    for (i = 0u; i < config->count; ++i) {
        const gemconf_entry_t *entry = &gemconf_entries(config)[i];

        capacity += strlen(entry->section) + strlen(entry->key) +
                    strlen(entry->value) + 8u;
        if (capacity > GEMCONF_FILE_MAX + 1u) {
            return 0;
        }
    }
    text = malloc(capacity);
    if (text == NULL) {
        return 0;
    }
    text[0] = '\0';
    if (gemconf_append(text, capacity, &used,
                       "# GEM configuration; edit with any text editor.\n") ==
        0) {
        free(text);
        return 0;
    }
    for (i = 0u; i < config->count; ++i) {
        const gemconf_entry_t *entry = &gemconf_entries(config)[i];

        if (last_section == NULL || strcmp(last_section, entry->section) != 0) {
            if ((i != 0u && gemconf_append(text, capacity, &used, "\n") == 0) ||
                gemconf_append(text, capacity, &used, "[") == 0 ||
                gemconf_append(text, capacity, &used, entry->section) == 0 ||
                gemconf_append(text, capacity, &used, "]\n") == 0) {
                goto done;
            }
            last_section = entry->section;
        }
        if (gemconf_append(text, capacity, &used, entry->key) == 0 ||
            gemconf_append(text, capacity, &used, " = ") == 0 ||
            gemconf_append(text, capacity, &used, entry->value) == 0 ||
            gemconf_append(text, capacity, &used, "\n") == 0) {
            goto done;
        }
    }
    result = gem_os_write_file_atomic(path, text, used);

done:
    free(text);
    return result;
}
