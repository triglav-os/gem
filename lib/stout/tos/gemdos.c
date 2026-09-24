/*
 * Implements the Stout guest's supported Atari GEMDOS trap services.
 *
 * MIT License (see: LICENSE)
 * Copyright (C) 2026 tomaz stih
 */

#include "tos.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <time.h>

enum {
    gemdos_ok = 0,
    gemdos_error = -1,
    gemdos_bad_handle = -6,
    gemdos_no_more_files = -49,
    gemdos_path_not_found = -34,
    gemdos_access_denied = -36,
    gemdos_out_of_memory = -39,
    gemdos_not_implemented = -32
};

static void gemdos_result(uint32_t sp, uint32_t advance, int32_t result)
{
    (void)sp;
    (void)advance;
    stout_set_reg(M68K_REG_D0, (uint32_t)result);
}

static void gemdos_pop_component(char *path)
{
    char *slash = strrchr(path, '/');

    if (slash != NULL)
        *slash = '\0';
    else
        path[0] = '\0';
}

static int gemdos_append_component(char *path, size_t size,
                                   const char *component, size_t length)
{
    size_t used = strlen(path);
    size_t index;

    if (length == 0u || (length == 1u && component[0] == '.'))
        return 1;
    if (length == 2u && component[0] == '.' && component[1] == '.') {
        gemdos_pop_component(path);
        return 1;
    }
    for (index = 0u; index < length; ++index) {
        unsigned char value = (unsigned char)component[index];

        if (value < 32u || value == ':')
            return 0;
    }
    if (used != 0u && used + 1u < size)
        path[used++] = '/';
    if (used + length >= size)
        return 0;
    memcpy(path + used, component, length);
    path[used + length] = '\0';
    return 1;
}

static int gemdos_guest_path(uint32_t address, char *path, size_t size,
                             int *disk_path)
{
    char guest[GEM_OS_PATH_MAX];
    char relative[GEM_OS_PATH_MAX];
    const char *cursor;
    uint16_t drive = g_stout.current_drive;
    int rooted;
    int output_length;

    if (disk_path != NULL)
        *disk_path = 0;
    if (path == NULL || size == 0u)
        return 0;
    stout_read_string(address, guest, sizeof(guest));
    if (isalpha((unsigned char)guest[0]) && guest[1] == ':') {
        drive = (uint16_t)(toupper((unsigned char)guest[0]) - 'A');
        cursor = guest + 2u;
    } else {
        cursor = guest;
    }
    if (drive != 0u && drive != 2u)
        return 0;
    if (drive == 0u && g_stout.disk_mounted == 0)
        return 0;
    if (drive == 2u && g_stout.host_drive_root[0] == '\0')
        return 0;
    rooted = *cursor == '/' || *cursor == '\\';
    relative[0] = '\0';
    if (!rooted) {
        const char *current = drive == 0u ? g_stout.disk_current_path
                                          : g_stout.host_current_path;

        if (strlen(current) >= sizeof(relative))
            return 0;
        strcpy(relative, current);
    }
    while (*cursor != '\0') {
        const char *component;
        size_t length;

        while (*cursor == '/' || *cursor == '\\')
            ++cursor;
        if (*cursor == '\0')
            break;
        component = cursor;
        while (*cursor != '\0' && *cursor != '/' && *cursor != '\\')
            ++cursor;
        length = (size_t)(cursor - component);
        if (!gemdos_append_component(relative, sizeof(relative), component,
                                     length))
            return 0;
    }
    if (drive == 0u) {
        if (strlen(relative) >= size)
            return 0;
        strcpy(path, relative);
        if (disk_path != NULL)
            *disk_path = 1;
        return 1;
    }
    if (relative[0] == '\0') {
        if (strlen(g_stout.host_drive_root) >= size)
            return 0;
        strcpy(path, g_stout.host_drive_root);
        return 1;
    }
    output_length =
        snprintf(path, size, "%s/%s", g_stout.host_drive_root, relative);
    return output_length > 0 && (size_t)output_length < size;
}

static int gemdos_host_path(const char *path, char *resolved, size_t size,
                            int allow_missing)
{
    gem_os_file_info_t info;
    char parent[GEM_OS_PATH_MAX];
    char canonical[GEM_OS_PATH_MAX];
    char *slash;
    const char *name;
    int length;

    if (gem_os_lstat_path(path, &info) != 0) {
        if (info.is_symlink != 0)
            return 0;
        return gem_os_resolve_under(path, g_stout.host_drive_root, resolved,
                                    size);
    }
    if (!allow_missing || strlen(path) >= sizeof(parent))
        return 0;
    strcpy(parent, path);
    slash = strrchr(parent, '/');
    if (slash == NULL || slash[1] == '\0')
        return 0;
    name = slash + 1u;
    if (slash == parent)
        slash[1] = '\0';
    else
        *slash = '\0';
    if (gem_os_resolve_under(parent, g_stout.host_drive_root, canonical,
                             sizeof(canonical)) == 0)
        return 0;
    length = snprintf(resolved, size, "%s/%s", canonical, name);
    return length > 0 && (size_t)length < size;
}

int stout_gemdos_read_file(uint32_t path_address, uint8_t **data_out,
                           size_t *size_out)
{
    char path[GEM_OS_PATH_MAX];
    char resolved[GEM_OS_PATH_MAX];
    gem_os_file_info_t info;
    uint8_t *data;
    size_t used = 0u;
    int disk_path;
    int fd;

    if (data_out == NULL || size_out == NULL ||
        !gemdos_guest_path(path_address, path, sizeof(path), &disk_path))
        return 0;
    *data_out = NULL;
    *size_out = 0u;
    if (g_stout.trace_traps != 0)
        fprintf(stderr, "stout: GEMDOS read path='%s' drive=%c\n", path,
                disk_path != 0 ? 'A' : 'C');
    if (disk_path != 0)
        return stout_disk_read_path(&g_stout.disk, path, data_out, size_out);
    if (!gemdos_host_path(path, resolved, sizeof(resolved), 0) ||
        gem_os_stat_path(resolved, &info) == 0 || info.is_directory != 0 ||
        info.size_bytes > SIZE_MAX)
        return 0;
    data = malloc((size_t)info.size_bytes != 0u ? (size_t)info.size_bytes : 1u);
    if (data == NULL)
        return 0;
    fd = gem_os_open_read(resolved);
    if (fd < 0) {
        free(data);
        return 0;
    }
    while (used < (size_t)info.size_bytes) {
        int32_t count = gem_os_read(fd, data + used,
                                    (uint32_t)((size_t)info.size_bytes - used));

        if (count <= 0) {
            (void)gem_os_close(fd);
            free(data);
            return 0;
        }
        used += (size_t)count;
    }
    if (gem_os_close(fd) != 0) {
        free(data);
        return 0;
    }
    *data_out = data;
    *size_out = used;
    return 1;
}

/* Create only missing directory components beneath the already-canonical
 * host drive root.  Checking every component before and after creation keeps
 * an existing symlink from redirecting an extractor outside that root. */
static int gemdos_create_host_parents(const char *path)
{
    char parent[GEM_OS_PATH_MAX];
    char canonical[GEM_OS_PATH_MAX];
    char *cursor;
    char *slash;
    size_t root_length = strlen(g_stout.host_drive_root);

    if (path == NULL || strlen(path) >= sizeof(parent) ||
        strncmp(path, g_stout.host_drive_root, root_length) != 0 ||
        (strcmp(g_stout.host_drive_root, "/") != 0 && path[root_length] != '/'))
        return 0;
    strcpy(parent, path);
    slash = strrchr(parent, '/');
    if (slash == NULL)
        return 0;
    *slash = '\0';
    if (strcmp(parent, g_stout.host_drive_root) == 0)
        return 1;

    cursor = parent + (root_length == 1u ? 0u : root_length);
    if (*cursor == '/')
        ++cursor;
    for (;;) {
        gem_os_file_info_t info;
        char saved;

        while (*cursor != '/' && *cursor != '\0')
            ++cursor;
        saved = *cursor;
        *cursor = '\0';
        if (gem_os_lstat_path(parent, &info) != 0) {
            if (info.is_symlink != 0 || info.is_directory == 0)
                return 0;
        } else if (gem_os_mkdir(parent) == 0) {
            return 0;
        }
        if (gem_os_resolve_under(parent, g_stout.host_drive_root, canonical,
                                 sizeof(canonical)) == 0)
            return 0;
        *cursor = saved;
        if (saved == '\0')
            break;
        ++cursor;
    }
    return 1;
}

static int gemdos_alloc_handle(void)
{
    int handle;

    for (handle = 6; handle < STOUT_GEMDOS_HANDLES; ++handle) {
        if (!g_stout.gemdos_handles[handle].used) {
            memset(&g_stout.gemdos_handles[handle], 0,
                   sizeof(g_stout.gemdos_handles[handle]));
            g_stout.gemdos_handles[handle].used = 1;
            g_stout.gemdos_handles[handle].host_fd = -1;
            return handle;
        }
    }
    return -1;
}

static int gemdos_handle_valid(uint16_t handle)
{
    return handle >= 6u && handle < STOUT_GEMDOS_HANDLES &&
           g_stout.gemdos_handles[handle].used;
}

static int gemdos_close_handle(uint16_t handle)
{
    stout_gemdos_handle_t *file = &g_stout.gemdos_handles[handle];
    int okay = 1;

    if (file->disk_image != 0 && file->writable != 0 && file->dirty != 0)
        okay = stout_disk_write_path(&g_stout.disk, file->path, file->data,
                                     file->size, file->attributes);
    if (okay != 0 && file->disk_image != 0 && file->metadata_dirty != 0)
        okay = stout_disk_set_datetime(&g_stout.disk, file->path, file->time,
                                       file->date);
    if (file->host_fd >= 0)
        (void)gem_os_close(file->host_fd);
    free(file->data);
    memset(file, 0, sizeof(*file));
    return okay;
}

void stout_gemdos_shutdown(void)
{
    uint16_t handle;

    if (g_stout.dta.active && g_stout.dta.disk_image == 0) {
        gem_os_dir_close(&g_stout.dta.directory);
    }
    g_stout.dta.active = 0;
    for (handle = 6u; handle < STOUT_GEMDOS_HANDLES; ++handle) {
        if (g_stout.gemdos_handles[handle].used)
            (void)gemdos_close_handle(handle);
    }
}

static int gemdos_match(const char *name, const char *pattern)
{
    while (*pattern != '\0') {
        if (*pattern == '*') {
            ++pattern;
            if (*pattern == '\0')
                return 1;
            while (*name != '\0') {
                if (gemdos_match(name, pattern))
                    return 1;
                ++name;
            }
            return gemdos_match(name, pattern);
        }
        if (*name == '\0' ||
            (*pattern != '?' && tolower((unsigned char)*name) !=
                                    tolower((unsigned char)*pattern))) {
            return 0;
        }
        ++name;
        ++pattern;
    }
    return *name == '\0';
}

static void gemdos_write_dta(const gem_os_dirent_t *entry)
{
    uint32_t address = g_stout.dta.address;
    size_t length;

    if (!stout_valid_range(address, 44u))
        return;
    memset(g_stout.ram + address, 0, 44u);
    g_stout.ram[address + 21u] =
        entry->info.is_directory ? 0x10u
                                 : (uint8_t)(entry->info.is_hidden ? 2u : 0u);
    stout_write_u32(address + 26u, (uint32_t)entry->info.size_bytes);
    length = strlen(entry->name);
    if (length > 13u)
        length = 13u;
    memcpy(g_stout.ram + address + 30u, entry->name, length);
}

static int gemdos_next_match(void)
{
    gem_os_dirent_t entry;

    if (!g_stout.dta.active)
        return 0;
    while (gem_os_dir_read(&g_stout.dta.directory, &entry)) {
        if (gemdos_match(entry.name, g_stout.dta.pattern)) {
            gemdos_write_dta(&entry);
            return 1;
        }
    }
    gem_os_dir_close(&g_stout.dta.directory);
    g_stout.dta.active = 0;
    return 0;
}

typedef struct gemdos_disk_search_context {
    size_t wanted_index;
    size_t matching_index;
    const char *directory;
    const char *pattern;
    uint16_t attributes;
    fat12_dirent_t entry;
    int found;
} gemdos_disk_search_context_t;

static int gemdos_disk_search_callback(const fat12_dirent_t *entry,
                                       const char *path, void *context)
{
    gemdos_disk_search_context_t *search = context;
    const char *name = path;
    size_t directory_length = strlen(search->directory);

    if (directory_length != 0u) {
        if (strncasecmp(path, search->directory, directory_length) != 0 ||
            path[directory_length] != '/')
            return 1;
        name = path + directory_length + 1u;
    }
    if (strchr(name, '/') != NULL ||
        (entry->attr & FAT12_ATTR_VOLUME) != 0u ||
        ((entry->attr & FAT12_ATTR_DIRECTORY) != 0u &&
         (search->attributes & FAT12_ATTR_DIRECTORY) == 0u) ||
        ((entry->attr & FAT12_ATTR_HIDDEN) != 0u &&
         (search->attributes & FAT12_ATTR_HIDDEN) == 0u) ||
        !gemdos_match(name, search->pattern))
        return 1;
    if (search->matching_index++ != search->wanted_index)
        return 1;
    search->entry = *entry;
    search->found = 1;
    return 0;
}

static int gemdos_next_disk_match(void)
{
    const fat12_image_t *fat = stout_disk_fat12(&g_stout.disk);
    gemdos_disk_search_context_t search;
    gem_os_dirent_t output;
    char error_text[FAT12_ERROR_LENGTH];

    if (fat == NULL)
        return 0;
    memset(&search, 0, sizeof(search));
    search.wanted_index = g_stout.dta.disk_match_index;
    search.directory = g_stout.dta.disk_directory;
    search.pattern = g_stout.dta.pattern;
    search.attributes = g_stout.dta.attributes;
    if (!fat12_walk_root(fat, gemdos_disk_search_callback, &search, error_text,
                         sizeof(error_text)) ||
        search.found == 0) {
        g_stout.dta.active = 0;
        return 0;
    }
    memset(&output, 0, sizeof(output));
    snprintf(output.name, sizeof(output.name), "%s", search.entry.name);
    output.info.size_bytes = search.entry.size;
    output.info.is_directory =
        (search.entry.attr & FAT12_ATTR_DIRECTORY) != 0u;
    output.info.is_hidden = (search.entry.attr & FAT12_ATTR_HIDDEN) != 0u;
    output.info.is_read_only =
        (search.entry.attr & FAT12_ATTR_READ_ONLY) != 0u;
    gemdos_write_dta(&output);
    ++g_stout.dta.disk_match_index;
    return 1;
}

static int gemdos_find_first(uint32_t path_address, uint16_t attribute)
{
    char path[GEM_OS_PATH_MAX];
    char pattern[128];
    char resolved[GEM_OS_PATH_MAX];
    char *slash;
    int disk_path;

    if (g_stout.dta.active && g_stout.dta.disk_image == 0) {
        gem_os_dir_close(&g_stout.dta.directory);
    }
    g_stout.dta.active = 0;
    if (!gemdos_guest_path(path_address, path, sizeof(path), &disk_path))
        return 0;
    slash = strrchr(path, '/');
    if (slash != NULL) {
        if (strlen(slash + 1u) >= sizeof(pattern))
            return 0;
        strcpy(pattern, slash + 1u);
        *slash = '\0';
    } else {
        if (strlen(path) >= sizeof(pattern))
            return 0;
        strcpy(pattern, path);
        path[0] = '\0';
    }
    if (disk_path != 0) {
        if (strlen(path) >= sizeof(g_stout.dta.disk_directory) ||
            !stout_disk_directory_exists(&g_stout.disk, path))
            return 0;
        strcpy(g_stout.dta.disk_directory, path);
        strcpy(g_stout.dta.pattern, pattern);
        g_stout.dta.active = 1;
        g_stout.dta.disk_image = 1;
        g_stout.dta.address = g_stout.dta_address;
        g_stout.dta.disk_match_index = 0u;
        g_stout.dta.attributes = attribute;
        return gemdos_next_disk_match();
    }
    if (path[0] == '\0')
        strcpy(path, g_stout.host_drive_root);
    if (!gemdos_host_path(path, resolved, sizeof(resolved), 0) ||
        !gem_os_dir_open(resolved, &g_stout.dta.directory))
        return 0;
    g_stout.dta.active = 1;
    g_stout.dta.disk_image = 0;
    g_stout.dta.address = g_stout.dta_address;
    (void)attribute;
    strcpy(g_stout.dta.pattern, pattern);
    return gemdos_next_match();
}

int stout_handle_gemdos(void)
{
    uint32_t sp = stout_get_reg(M68K_REG_SP);
    uint16_t function;

    if (!stout_valid_range(sp, 2u))
        return 0;
    function = stout_read_u16(sp);
    switch (function) {
        case 0:
            stout_stop_with_code(0);
            gemdos_result(sp, 2u, 0);
            return 1;
        case 1: {
            uint8_t character = 0;
            (void)stout_console_input(&character);
            gemdos_result(sp, 2u, character);
            return 1;
        }
        case 2:
            stout_console_output((uint8_t)stout_read_u16(sp + 2u));
            gemdos_result(sp, 4u, 0);
            return 1;
        case 3:
        case 7: {
            uint8_t character = 0;
            (void)stout_console_input(&character);
            gemdos_result(sp, 2u, character);
            return 1;
        }
        case 4:
        case 5:
            stout_console_output((uint8_t)stout_read_u16(sp + 2u));
            gemdos_result(sp, 4u, 0);
            return 1;
        case 6: {
            uint8_t character = (uint8_t)stout_read_u16(sp + 2u);
            if (character == 0xffu)
                (void)stout_console_input(&character);
            else
                stout_console_output(character);
            gemdos_result(sp, 4u, character);
            return 1;
        }
        case 8:
            gemdos_result(sp, 2u, stout_console_peek() ? 0xffu : 0u);
            return 1;
        case 9: {
            char text[512];
            size_t index;
            stout_read_string(stout_read_u32(sp + 2u), text, sizeof(text));
            for (index = 0u; text[index] != '$' && text[index] != '\0'; ++index)
                stout_console_output((uint8_t)text[index]);
            gemdos_result(sp, 6u, 0);
            return 1;
        }
        case 10: {
            uint32_t address = stout_read_u32(sp + 2u);
            uint8_t max;
            uint8_t count = 0;
            uint8_t character;
            if (!stout_valid_range(address, 2u))
                return 0;
            max = g_stout.ram[address];
            while (count < max && stout_console_input(&character) &&
                   character != '\r' && character != '\n') {
                g_stout.ram[address + 2u + count++] = character;
                stout_console_output(character);
            }
            g_stout.ram[address + 1u] = count;
            gemdos_result(sp, 6u, 0);
            return 1;
        }
        case 11:
            gemdos_result(sp, 2u, stout_console_peek() ? -1 : 0);
            return 1;
        case 16:
        case 17:
        case 18:
        case 19:
            gemdos_result(sp, 2u, 1);
            return 1;
        case 14: {
            uint16_t drive = stout_read_u16(sp + 2u);
            uint32_t map = 1u << 2;

            if (g_stout.disk_mounted != 0)
                map |= 1u;
            if (drive == 2u || (drive == 0u && g_stout.disk_mounted != 0))
                g_stout.current_drive = drive;
            gemdos_result(sp, 4u, (int32_t)map);
        }
            return 1;
        case 25:
            gemdos_result(sp, 2u, g_stout.current_drive);
            return 1;
        case 32: { /* Super */
            uint32_t stack = stout_read_u32(sp + 2u);
            int32_t result;

            if (stack == 1u || stack == 0xffffffffu) {
                result = g_stout.supervisor_mode != 0 ? 1 : 0;
            } else if (g_stout.supervisor_mode == 0) {
                g_stout.supervisor_mode = 1;
                stout_set_reg(M68K_REG_SR, stout_get_reg(M68K_REG_SR) |
                                               0x2000u);
                result = (int32_t)STOUT_SUPERVISOR_STACK_TOP;
            } else {
                g_stout.supervisor_mode = 0;
                stout_set_reg(M68K_REG_SR, stout_get_reg(M68K_REG_SR) &
                                               ~0x2000u);
                result = 0;
            }
            if (g_stout.trace_traps != 0)
                fprintf(stderr,
                        "stout: GEMDOS Super stack=0x%08x mode=%s "
                        "result=0x%08x\n",
                        stack,
                        g_stout.supervisor_mode != 0 ? "supervisor" : "user",
                        (uint32_t)result);
            gemdos_result(sp, 6u, result);
            return 1;
        }
        case 26:
            g_stout.dta_address = stout_read_u32(sp + 2u);
            gemdos_result(sp, 6u, 0);
            return 1;
        case 42:   /* Tgetdate */
        case 44: { /* Tgettime */
            time_t now = time(NULL);
            struct tm *clock = localtime(&now);
            uint16_t value = 0u;

            if (clock != NULL) {
                if (function == 42u) {
                    int year = clock->tm_year + 1900;

                    if (year < 1980)
                        year = 1980;
                    if (year > 2107)
                        year = 2107;
                    value =
                        (uint16_t)(((year - 1980) << 9) |
                                   ((clock->tm_mon + 1) << 5) | clock->tm_mday);
                } else {
                    value =
                        (uint16_t)((clock->tm_hour << 11) |
                                   (clock->tm_min << 5) | (clock->tm_sec / 2));
                }
            }
            gemdos_result(sp, 2u, value);
            return 1;
        }
        case 47: /* Fgetdta */
            gemdos_result(sp, 2u, (int32_t)g_stout.dta_address);
            return 1;
        case 48: /* Sversion */
            gemdos_result(sp, 2u, 0x0130);
            return 1;
        case 54: { /* Dfree */
            uint32_t address = stout_read_u32(sp + 2u);
            uint16_t drive = stout_read_u16(sp + 6u);
            const fat12_image_t *fat = stout_disk_fat12(&g_stout.disk);
            uint32_t clusters = 32767u;
            uint32_t sectors = 2u;
            uint32_t bytes = 512u;

            if (!stout_valid_range(address, 16u))
                return 0;
            if ((drive == 1u || (drive == 0u && g_stout.current_drive == 0u)) &&
                fat != NULL) {
                clusters = (fat->bpb.total_sectors - fat->bpb.data_sector) /
                           fat->bpb.sectors_per_cluster;
                stout_write_u32(address,
                                stout_disk_free_clusters(&g_stout.disk));
                sectors = fat->bpb.sectors_per_cluster;
                bytes = fat->bpb.bytes_per_sector;
            } else {
                stout_write_u32(address, clusters);
            }
            stout_write_u32(address + 4u, clusters);
            stout_write_u32(address + 8u, bytes);
            stout_write_u32(address + 12u, sectors);
            gemdos_result(sp, 8u, 0);
            return 1;
        }
        case 57: { /* Dcreate */
            char path[GEM_OS_PATH_MAX];
            char resolved[GEM_OS_PATH_MAX];
            int disk_path;
            int okay = gemdos_guest_path(stout_read_u32(sp + 2u), path,
                                         sizeof(path), &disk_path);
            int result;

            if (okay != 0 && disk_path != 0)
                okay = stout_disk_create_directory(&g_stout.disk, path);
            else if (okay != 0)
                okay = gemdos_host_path(path, resolved, sizeof(resolved), 1) &&
                       gem_os_mkdir(resolved);
            result = okay != 0 ? gemdos_ok : gemdos_error;
            gemdos_result(sp, 6u, result);
            return 1;
        }
        case 58: { /* Ddelete */
            char path[GEM_OS_PATH_MAX];
            char resolved[GEM_OS_PATH_MAX];
            int disk_path;
            int result =
                gemdos_guest_path(stout_read_u32(sp + 2u), path, sizeof(path),
                                  &disk_path) &&
                        !disk_path &&
                        gemdos_host_path(path, resolved, sizeof(resolved), 0) &&
                        strcmp(resolved, g_stout.host_drive_root) != 0 &&
                        gem_os_rmdir(resolved)
                    ? 0
                    : gemdos_error;
            gemdos_result(sp, 6u, result);
            return 1;
        }
        case 59: { /* Dsetpath */
            char path[GEM_OS_PATH_MAX];
            char resolved[GEM_OS_PATH_MAX];
            gem_os_file_info_t info;
            int disk_path;
            int okay = gemdos_guest_path(stout_read_u32(sp + 2u), path,
                                         sizeof(path), &disk_path);

            if (okay != 0 && disk_path != 0) {
                okay = stout_disk_directory_exists(&g_stout.disk, path);
            } else if (okay != 0) {
                okay = gemdos_host_path(path, resolved, sizeof(resolved), 0) &&
                       gem_os_stat_path(resolved, &info) && info.is_directory;
            }
            if (okay != 0 && disk_path != 0) {
                snprintf(g_stout.disk_current_path,
                         sizeof(g_stout.disk_current_path), "%s", path);
            } else if (okay != 0) {
                const char *relative =
                    resolved + strlen(g_stout.host_drive_root);

                while (*relative == '/')
                    ++relative;
                snprintf(g_stout.host_current_path,
                         sizeof(g_stout.host_current_path), "%s", relative);
            }
            gemdos_result(sp, 6u,
                          okay != 0 ? gemdos_ok : gemdos_path_not_found);
            return 1;
        }
        case 60: { /* Fcreate */
            char path[GEM_OS_PATH_MAX];
            char resolved[GEM_OS_PATH_MAX];
            int disk_path = 0;
            int handle = gemdos_alloc_handle();
            int fd = -1;

            uint8_t attributes = (uint8_t)stout_read_u16(sp + 6u);

            if (handle >= 0 && gemdos_guest_path(stout_read_u32(sp + 2u), path,
                                                 sizeof(path), &disk_path)) {
                if (g_stout.trace_traps != 0)
                    fprintf(stderr,
                            "stout: GEMDOS Fcreate path='%s' drive=%c "
                            "attributes=0x%02x\n",
                            path, disk_path != 0 ? 'A' : 'C', attributes);
                if (disk_path != 0) {
                    if (stout_disk_write_path(&g_stout.disk, path, NULL, 0u,
                                              attributes)) {
                        stout_gemdos_handle_t *file =
                            &g_stout.gemdos_handles[handle];

                        file->disk_image = 1;
                        file->writable = 1;
                        file->attributes = attributes;
                        snprintf(file->path, sizeof(file->path), "%s", path);
                        gemdos_result(sp, 8u, handle);
                        return 1;
                    }
                } else if (gemdos_create_host_parents(path) &&
                           gemdos_host_path(path, resolved, sizeof(resolved),
                                            1)) {
                    fd = gem_os_open_write(resolved);
                }
            }
            if (handle < 0 || fd < 0) {
                if (handle >= 0)
                    gemdos_close_handle((uint16_t)handle);
                gemdos_result(sp, 8u,
                              disk_path != 0 ? gemdos_access_denied
                                             : gemdos_error);
                return 1;
            }
            g_stout.gemdos_handles[handle].host_fd = fd;
            g_stout.gemdos_handles[handle].writable = 1;
            gemdos_result(sp, 8u, handle);
            return 1;
        }
        case 61: { /* Fopen */
            char path[GEM_OS_PATH_MAX];
            char resolved[GEM_OS_PATH_MAX];
            int disk_path;
            uint16_t mode = stout_read_u16(sp + 6u);
            int handle = gemdos_alloc_handle();
            if (handle < 0 || !gemdos_guest_path(stout_read_u32(sp + 2u), path,
                                                 sizeof(path), &disk_path)) {
                if (handle >= 0)
                    gemdos_close_handle((uint16_t)handle);
                gemdos_result(sp, 8u, gemdos_error);
                return 1;
            }
            if (disk_path) {
                stout_gemdos_handle_t *file = &g_stout.gemdos_handles[handle];

                if (mode > 2u || (mode != 0u && g_stout.disk.writable == 0) ||
                    !stout_disk_read_path(&g_stout.disk, path, &file->data,
                                          &file->size)) {
                    gemdos_close_handle((uint16_t)handle);
                    gemdos_result(sp, 8u, gemdos_error);
                    return 1;
                }
                file->capacity = file->size;
                file->disk_image = 1;
                file->writable = mode != 0u;
                snprintf(file->path, sizeof(file->path), "%s", path);
            } else {
                int fd = mode <= 2u && gemdos_host_path(path, resolved,
                                                        sizeof(resolved), 0)
                             ? gem_os_open_existing(resolved, mode)
                             : -1;
                if (fd < 0) {
                    gemdos_close_handle((uint16_t)handle);
                    gemdos_result(sp, 8u, gemdos_error);
                    return 1;
                }
                g_stout.gemdos_handles[handle].host_fd = fd;
                g_stout.gemdos_handles[handle].writable = mode != 0u;
            }
            gemdos_result(sp, 8u, handle);
            return 1;
        }
        case 62: {
            uint16_t handle = stout_read_u16(sp + 2u);
            if (!gemdos_handle_valid(handle))
                gemdos_result(sp, 4u, gemdos_bad_handle);
            else
                gemdos_result(sp, 4u,
                              gemdos_close_handle(handle) ? gemdos_ok
                                                          : gemdos_error);
            return 1;
        }
        case 63:   /* Fread */
        case 64: { /* Fwrite */
            uint16_t handle = stout_read_u16(sp + 2u);
            uint32_t count = stout_read_u32(sp + 4u);
            uint32_t address = stout_read_u32(sp + 8u);
            uint32_t done = 0u;
            if (!stout_valid_range(address, (size_t)count))
                return 0;
            if (handle == 0u && function == 63u) {
                while (done < count) {
                    uint8_t character;
                    if (!stout_console_input(&character))
                        break;
                    g_stout.ram[address + done++] = character;
                }
            } else if ((handle == 1u || handle == 2u) && function == 64u) {
                while (done < count)
                    stout_console_output(g_stout.ram[address + done++]);
            } else if (gemdos_handle_valid(handle)) {
                stout_gemdos_handle_t *file = &g_stout.gemdos_handles[handle];
                if (file->disk_image) {
                    if (function == 63u) {
                        size_t available = file->position < file->size
                                               ? file->size - file->position
                                               : 0u;
                        if (available < count)
                            count = (uint32_t)available;
                        memcpy(g_stout.ram + address,
                               file->data + file->position, count);
                        file->position += count;
                        done = count;
                    } else if (file->writable != 0) {
                        size_t required = file->position + count;

                        if (required < file->position ||
                            required > UINT32_MAX) {
                            done = (uint32_t)gemdos_error;
                        } else {
                            if (required > file->capacity) {
                                size_t capacity = file->capacity != 0u
                                                      ? file->capacity
                                                      : 512u;
                                uint8_t *replacement;

                                while (capacity < required &&
                                       capacity <= UINT32_MAX / 2u)
                                    capacity *= 2u;
                                if (capacity < required)
                                    capacity = required;
                                replacement = realloc(file->data, capacity);
                                if (replacement == NULL) {
                                    done = (uint32_t)gemdos_out_of_memory;
                                    goto write_done;
                                }
                                file->data = replacement;
                                file->capacity = capacity;
                            }
                            if (file->position > file->size)
                                memset(file->data + file->size, 0,
                                       file->position - file->size);
                            memcpy(file->data + file->position,
                                   g_stout.ram + address, count);
                            file->position += count;
                            if (file->position > file->size)
                                file->size = file->position;
                            file->dirty = 1;
                            done = count;
                        }
                    } else {
                        done = (uint32_t)gemdos_access_denied;
                    }
                } else {
                    int32_t result;

                    if (function == 64u && file->writable == 0) {
                        result = gemdos_access_denied;
                    } else {
                        result =
                            function == 63u
                                ? gem_os_read(file->host_fd,
                                              g_stout.ram + address, count)
                                : gem_os_write(file->host_fd,
                                               g_stout.ram + address, count);
                        if (result < 0)
                            result = gemdos_error;
                    }
                    done = (uint32_t)result;
                }
            } else {
                done = (uint32_t)gemdos_bad_handle;
            }
        write_done:
            if (g_stout.trace_traps != 0)
                fprintf(stderr,
                        "stout: GEMDOS F%s handle=%u requested=%u "
                        "address=0x%08x result=%d\n",
                        function == 63u ? "read" : "write", handle,
                        (unsigned)stout_read_u32(sp + 4u), address,
                        (int32_t)done);
            gemdos_result(sp, 12u, (int32_t)done);
            return 1;
        }
        case 65: { /* Fdelete */
            char path[GEM_OS_PATH_MAX];
            char resolved[GEM_OS_PATH_MAX];
            int disk_path;
            int result;

            if (!gemdos_guest_path(stout_read_u32(sp + 2u), path, sizeof(path),
                                   &disk_path)) {
                result = gemdos_error;
            } else if (disk_path != 0) {
                result = stout_disk_delete_path(&g_stout.disk, path)
                             ? gemdos_ok
                             : gemdos_error;
            } else {
                result =
                    gemdos_host_path(path, resolved, sizeof(resolved), 0) &&
                            gem_os_unlink(resolved)
                        ? gemdos_ok
                        : gemdos_error;
            }
            gemdos_result(sp, 6u, result);
            return 1;
        }
        case 66: { /* Fseek */
            int32_t offset = (int32_t)stout_read_u32(sp + 2u);
            uint16_t handle = stout_read_u16(sp + 6u);
            uint16_t mode = stout_read_u16(sp + 8u);
            int64_t position = -1;
            if (gemdos_handle_valid(handle)) {
                stout_gemdos_handle_t *file = &g_stout.gemdos_handles[handle];
                if (file->disk_image) {
                    size_t base = mode == 0u   ? 0u
                                  : mode == 1u ? file->position
                                               : file->size;
                    int64_t target = (int64_t)base + offset;

                    if (mode <= 2u && target >= 0 &&
                        (file->writable != 0 ||
                         (uint64_t)target <= file->size) &&
                        (uint64_t)target <= UINT32_MAX)
                        position = (int64_t)(file->position = (size_t)target);
                } else
                    position = gem_os_seek(file->host_fd, offset, mode);
            }
            gemdos_result(sp, 10u, (int32_t)position);
            return 1;
        }
        case 71: { /* Dgetpath */
            uint32_t address = stout_read_u32(sp + 2u);
            uint16_t requested = stout_read_u16(sp + 6u);
            uint16_t drive = requested == 0u ? g_stout.current_drive
                                             : (uint16_t)(requested - 1u);
            const char *current = drive == 0u ? g_stout.disk_current_path
                                               : g_stout.host_current_path;
            size_t length = strlen(current);

            if ((drive != 0u && drive != 2u) ||
                !stout_valid_range(address, length + 2u))
                return 0;
            g_stout.ram[address] = '\\';
            memcpy(g_stout.ram + address + 1u, current, length + 1u);
            gemdos_result(sp, 8u, 0);
            return 1;
        }
        case 72: { /* Malloc */
            uint32_t size = stout_read_u32(sp + 2u);
            uint32_t result = g_stout.heap_next;
            if (size == 0xffffffffu)
                result = STOUT_TPA_TOP - result;
            else if (result > STOUT_TPA_TOP || size > STOUT_TPA_TOP - result)
                result = (uint32_t)gemdos_out_of_memory;
            else
                g_stout.heap_next = (result + size + 3u) & ~3u;
            if (g_stout.trace_traps != 0)
                fprintf(stderr,
                        "stout: GEMDOS Malloc requested=%u result=0x%08x "
                        "next=0x%08x\n",
                        size, result, g_stout.heap_next);
            gemdos_result(sp, 6u, (int32_t)result);
            return 1;
        }
        case 67: /* Fattrib: report a normal file and ignore read-only changes.
                  */
            gemdos_result(sp, 10u, 0);
            return 1;
        case 69: /* Fdup: duplicate console handles. */
            gemdos_result(sp, 4u,
                          stout_read_u16(sp + 2u) <= 2u
                              ? stout_read_u16(sp + 2u)
                              : gemdos_bad_handle);
            return 1;
        case 70: /* Fforce: console redirection is handled by the terminal. */
            gemdos_result(sp, 6u, 0);
            return 1;
        case 73:
            gemdos_result(sp, function == 73u ? 6u : 10u, 0);
            return 1;
        case 74: {
            uint32_t block = stout_read_u32(sp + 4u);
            uint32_t size = stout_read_u32(sp + 8u);
            uint32_t minimum = g_stout.load_address +
                               (uint32_t)g_stout.program.image_size +
                               g_stout.program.header.bss_size;
            uint32_t end = block + size;
            int okay = block == g_stout.basepage_address && end >= block &&
                       end >= minimum && end <= STOUT_TPA_TOP;

            if (okay != 0)
                g_stout.heap_next = (end + 3u) & ~3u;
            if (g_stout.trace_traps != 0)
                fprintf(stderr,
                        "stout: GEMDOS Mshrink zero=%u block=0x%08x "
                        "size=%u result=%d\n",
                        stout_read_u16(sp + 2u), block, size,
                        okay != 0 ? gemdos_ok : gemdos_error);
            gemdos_result(sp, 10u, okay != 0 ? gemdos_ok : gemdos_error);
        }
            return 1;
        case 75: /* Pexec: a nested process needs a separate Stout instance. */
            gemdos_result(sp, 16u, gemdos_not_implemented);
            return 1;
        case 76:
            stout_stop_with_code((int)(int16_t)stout_read_u16(sp + 2u));
            gemdos_result(sp, 4u, 0);
            return 1;
        case 49:
            stout_stop_with_code(0);
            gemdos_result(sp, 10u, 0);
            return 1;
        case 78:
            gemdos_result(sp, 8u,
                          gemdos_find_first(stout_read_u32(sp + 2u),
                                            stout_read_u16(sp + 6u))
                              ? 0
                              : gemdos_no_more_files);
            return 1;
        case 79:
            gemdos_result(sp, 2u,
                          (g_stout.dta.active && g_stout.dta.disk_image != 0
                               ? gemdos_next_disk_match()
                               : gemdos_next_match())
                              ? 0
                              : gemdos_no_more_files);
            return 1;
        case 86: { /* Frename */
            char old_path[GEM_OS_PATH_MAX];
            char new_path[GEM_OS_PATH_MAX];
            char old_resolved[GEM_OS_PATH_MAX];
            char new_resolved[GEM_OS_PATH_MAX];
            int old_disk;
            int new_disk;
            int result;

            if (!gemdos_guest_path(stout_read_u32(sp + 4u), old_path,
                                   sizeof(old_path), &old_disk) ||
                !gemdos_guest_path(stout_read_u32(sp + 8u), new_path,
                                   sizeof(new_path), &new_disk) ||
                old_disk != new_disk) {
                result = gemdos_error;
            } else if (old_disk != 0) {
                result =
                    stout_disk_rename_path(&g_stout.disk, old_path, new_path)
                        ? gemdos_ok
                        : gemdos_error;
            } else {
                result = gemdos_host_path(old_path, old_resolved,
                                          sizeof(old_resolved), 0) &&
                                 gemdos_host_path(new_path, new_resolved,
                                                  sizeof(new_resolved), 1) &&
                                 gem_os_rename(old_resolved, new_resolved)
                             ? gemdos_ok
                             : gemdos_error;
            }
            gemdos_result(sp, 12u, result);
            return 1;
        }
        case 87: { /* Fdatime */
            uint32_t address = stout_read_u32(sp + 2u);
            uint16_t handle = stout_read_u16(sp + 6u);
            uint16_t write = stout_read_u16(sp + 8u);
            stout_gemdos_handle_t *file;
            int okay = 1;

            if (!stout_valid_range(address, 4u) ||
                !gemdos_handle_valid(handle)) {
                gemdos_result(sp, 10u, gemdos_bad_handle);
                return 1;
            }
            file = &g_stout.gemdos_handles[handle];
            if (write != 0u) {
                file->time = stout_read_u16(address);
                file->date = stout_read_u16(address + 2u);
                file->metadata_dirty = file->disk_image;
            } else if (file->disk_image != 0) {
                if (file->metadata_dirty == 0)
                    okay = stout_disk_get_datetime(&g_stout.disk, file->path,
                                                   &file->time, &file->date);
                if (okay != 0) {
                    stout_write_u16(address, file->time);
                    stout_write_u16(address + 2u, file->date);
                }
            } else {
                stout_write_u16(address, 0u);
                stout_write_u16(address + 2u, 0u);
            }
            gemdos_result(sp, 10u, okay != 0 ? gemdos_ok : gemdos_error);
            return 1;
        }
        default:
            fprintf(stderr, "stout: unsupported GEMDOS opcode %u\n", function);
            gemdos_result(sp, 2u, gemdos_not_implemented);
            return 1;
    }
}
