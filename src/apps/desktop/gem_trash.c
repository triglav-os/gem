/*
 * Implements freedesktop-compatible home and per-volume Trash stores for the
 * GEMix desktop. Linux environment, mount, metadata and file operations are
 * performed only through gem_os wrappers.
 *
 * MIT License (see: LICENSE)
 * Copyright (C) 2026 tomaz stih
 */

#include "gem_trash.h"

#include <stdio.h>
#include <string.h>

enum {
    GEM_TRASH_MAX_STORES = 32,
    GEM_TRASH_INFO_MAX = GEM_OS_PATH_MAX * 4,
    GEM_TRASH_DATE_MAX = 31
};

typedef struct gem_trash_store_list {
    char paths[GEM_TRASH_MAX_STORES][GEM_OS_PATH_MAX];
    size_t count;
} gem_trash_store_list_t;

static gem_os_mutex_t gem_trash_mutex;
static int gem_trash_mutex_ready;

static void gem_trash_known_stores(gem_trash_store_list_t *stores);
static int gem_trash_store_for_source(const char *source, char *root,
                                      size_t root_size);

static int gem_trash_lock(void)
{
    if (gem_trash_mutex_ready == 0) {
        if (gem_os_mutex_init(&gem_trash_mutex) == 0) {
            return 0;
        }
        gem_trash_mutex_ready = 1;
    }
    gem_os_mutex_lock(&gem_trash_mutex);
    return 1;
}

static int gem_trash_copy(char *dst, size_t dst_size, const char *src)
{
    size_t length;

    if (dst == NULL || dst_size == 0u || src == NULL) {
        return 0;
    }
    length = strlen(src);
    if (length >= dst_size) {
        return 0;
    }
    memcpy(dst, src, length + 1u);
    return 1;
}

static int gem_trash_path_has_prefix(const char *root, const char *path)
{
    size_t length = strlen(root);

    return strncmp(root, path, length) == 0 &&
           (length == 1u || path[length] == '\0' || path[length] == '/');
}

static int gem_trash_path_is_store(const char *path)
{
    gem_trash_store_list_t stores;
    size_t index;

    gem_trash_known_stores(&stores);
    for (index = 0u; index < stores.count; ++index) {
        if (gem_trash_path_has_prefix(stores.paths[index], path) != 0) {
            return 1;
        }
    }
    return 0;
}

static int gem_trash_join(const char *left, const char *right, char *path,
                          size_t path_size)
{
    int length;

    if (left == NULL || right == NULL || path == NULL || path_size == 0u) {
        return 0;
    }
    length = snprintf(path, path_size, strcmp(left, "/") == 0 ? "/%s" :
                                                                "%s/%s",
                      left, right);
    return length > 0 && (size_t)length < path_size;
}

static int gem_trash_home_root(char *root, size_t root_size)
{
    char base[GEM_OS_PATH_MAX];

    if (gem_os_user_data_dir(base, sizeof(base)) == 0) {
        return 0;
    }
    return gem_trash_join(base, "Trash", root, root_size);
}

static int gem_trash_store_dirs(const char *root, char *files,
                                size_t files_size, char *info,
                                size_t info_size)
{
    return gem_trash_join(root, "files", files, files_size) != 0 &&
           gem_trash_join(root, "info", info, info_size) != 0;
}

static int gem_trash_prepare_store(const char *root)
{
    char files[GEM_OS_PATH_MAX];
    char info[GEM_OS_PATH_MAX];

    return gem_trash_store_dirs(root, files, sizeof(files), info,
                                sizeof(info)) != 0 &&
           gem_os_mkdir_p(root) != 0 && gem_os_mkdir_p(files) != 0 &&
           gem_os_mkdir_p(info) != 0;
}

static int gem_trash_prepare_store_for_source(const char *source, char *root,
                                              size_t root_size)
{
    char home_root[GEM_OS_PATH_MAX];

    if (gem_trash_store_for_source(source, root, root_size) != 0 &&
        gem_trash_path_has_prefix(source, root) == 0 &&
        gem_trash_prepare_store(root) != 0) {
        return 1;
    }
    return gem_trash_home_root(home_root, sizeof(home_root)) != 0 &&
           gem_trash_path_has_prefix(source, home_root) == 0 &&
           gem_trash_prepare_store(home_root) != 0 &&
           gem_trash_copy(root, root_size, home_root) != 0;
}

static int gem_trash_store_add(gem_trash_store_list_t *stores,
                               const char *path)
{
    size_t index;

    for (index = 0u; index < stores->count; ++index) {
        if (strcmp(stores->paths[index], path) == 0) {
            return 1;
        }
    }
    if (stores->count >= GEM_TRASH_MAX_STORES ||
        gem_trash_copy(stores->paths[stores->count], GEM_OS_PATH_MAX, path) ==
            0) {
        return 0;
    }
    ++stores->count;
    return 1;
}

static void gem_trash_known_stores(gem_trash_store_list_t *stores)
{
    gem_os_mount_iter_t mounts = {0};
    char mount_path[GEM_OS_PATH_MAX];
    char root[GEM_OS_PATH_MAX];
    char leaf[48];

    memset(stores, 0, sizeof(*stores));
    if (gem_trash_home_root(root, sizeof(root)) != 0) {
        (void)gem_trash_store_add(stores, root);
    }
    (void)snprintf(leaf, sizeof(leaf), ".Trash-%lu",
                   (unsigned long)gem_os_getuid());
    if (gem_os_mount_iter_open(&mounts) == 0) {
        return;
    }
    while (gem_os_mount_iter_read(&mounts, mount_path, sizeof(mount_path)) !=
           0) {
        if (gem_trash_join(mount_path, leaf, root, sizeof(root)) != 0 &&
            gem_os_path_exists(root) != 0) {
            (void)gem_trash_store_add(stores, root);
        }
    }
    gem_os_mount_iter_close(&mounts);
}

static int gem_trash_store_for_source(const char *source, char *root,
                                      size_t root_size)
{
    char home_root[GEM_OS_PATH_MAX];
    char home_parent[GEM_OS_PATH_MAX];
    char mount[GEM_OS_PATH_MAX];
    char leaf[48];
    char *slash;

    if (gem_trash_home_root(home_root, sizeof(home_root)) == 0 ||
        gem_trash_copy(home_parent, sizeof(home_parent), home_root) == 0) {
        return 0;
    }
    slash = strrchr(home_parent, '/');
    if (slash == NULL || slash == home_parent) {
        return 0;
    }
    *slash = '\0';
    if (gem_os_mkdir_p(home_parent) == 0) {
        return 0;
    }
    if (gem_os_same_device(source, home_parent) != 0) {
        return gem_trash_copy(root, root_size, home_root);
    }
    if (gem_os_mount_for_path(source, mount, sizeof(mount)) == 0) {
        return 0;
    }
    (void)snprintf(leaf, sizeof(leaf), ".Trash-%lu",
                   (unsigned long)gem_os_getuid());
    return gem_trash_join(mount, leaf, root, root_size);
}

static int gem_trash_hex_value(char value)
{
    if (value >= '0' && value <= '9') {
        return value - '0';
    }
    if (value >= 'A' && value <= 'F') {
        return value - 'A' + 10;
    }
    if (value >= 'a' && value <= 'f') {
        return value - 'a' + 10;
    }
    return -1;
}

int gem_trash_encode_path(const char *path, char *encoded, size_t encoded_size)
{
    static const char hex[] = "0123456789ABCDEF";
    size_t input = 0u;
    size_t output = 0u;

    if (path == NULL || path[0] != '/' || encoded == NULL ||
        encoded_size == 0u) {
        return GEM_TRASH_INVALID;
    }
    while (path[input] != '\0') {
        unsigned char value = (unsigned char)path[input++];
        int unreserved =
            (value >= 'A' && value <= 'Z') ||
            (value >= 'a' && value <= 'z') ||
            (value >= '0' && value <= '9') || value == '-' || value == '_' ||
            value == '.' || value == '~';

        if (unreserved != 0) {
            if (output + 1u >= encoded_size) {
                return GEM_TRASH_FULL;
            }
            encoded[output++] = (char)value;
        } else {
            if (output + 3u >= encoded_size) {
                return GEM_TRASH_FULL;
            }
            encoded[output++] = '%';
            encoded[output++] = hex[value >> 4u];
            encoded[output++] = hex[value & 15u];
        }
    }
    encoded[output] = '\0';
    return GEM_TRASH_OK;
}

int gem_trash_decode_path(const char *encoded, char *path, size_t path_size)
{
    size_t input = 0u;
    size_t output = 0u;

    if (encoded == NULL || path == NULL || path_size == 0u) {
        return GEM_TRASH_INVALID;
    }
    while (encoded[input] != '\0') {
        unsigned char value = (unsigned char)encoded[input++];

        if (value == '%') {
            if (encoded[input] == '\0' || encoded[input + 1u] == '\0') {
                return GEM_TRASH_INVALID;
            }
            int high = gem_trash_hex_value(encoded[input]);
            int low = gem_trash_hex_value(encoded[input + 1u]);

            if (high < 0 || low < 0) {
                return GEM_TRASH_INVALID;
            }
            value = (unsigned char)((high << 4) | low);
            input += 2u;
            if (value == 0u) {
                return GEM_TRASH_INVALID;
            }
        }
        if (output + 1u >= path_size) {
            return GEM_TRASH_FULL;
        }
        path[output++] = (char)value;
    }
    path[output] = '\0';
    return path[0] == '/' ? GEM_TRASH_OK : GEM_TRASH_INVALID;
}

static int gem_trash_unique_paths(const char *root, const char *base,
                                  gem_trash_item_t *item)
{
    char files[GEM_OS_PATH_MAX];
    char info[GEM_OS_PATH_MAX];
    unsigned int suffix;

    if (gem_trash_store_dirs(root, files, sizeof(files), info, sizeof(info)) ==
        0) {
        return GEM_TRASH_FULL;
    }
    for (suffix = 0u; suffix < 100000u; ++suffix) {
        char unique[GEM_OS_PATH_MAX];
        char info_name[GEM_OS_PATH_MAX];
        int length;

        length = suffix == 0u
                     ? snprintf(unique, sizeof(unique), "%s", base)
                     : snprintf(unique, sizeof(unique), "%s.%u", base,
                                suffix);
        if (length <= 0 || (size_t)length >= sizeof(unique)) {
            return GEM_TRASH_FULL;
        }
        length = snprintf(info_name, sizeof(info_name), "%s.trashinfo", unique);
        if (length <= 0 || (size_t)length >= sizeof(info_name) ||
            gem_trash_join(files, unique, item->payload_path,
                           sizeof(item->payload_path)) == 0 ||
            gem_trash_join(info, info_name, item->info_path,
                           sizeof(item->info_path)) == 0) {
            return GEM_TRASH_FULL;
        }
        if (gem_os_path_exists(item->payload_path) == 0 &&
            gem_os_path_exists(item->info_path) == 0) {
            (void)gem_trash_copy(item->name, sizeof(item->name), unique);
            return GEM_TRASH_OK;
        }
    }
    return GEM_TRASH_FULL;
}

static int gem_trash_put_locked(const char *source_path,
                                gem_trash_item_t *moved_item)
{
    char source[GEM_OS_PATH_MAX];
    char root[GEM_OS_PATH_MAX];
    char encoded[GEM_TRASH_INFO_MAX];
    char record[GEM_TRASH_INFO_MAX];
    const char *base;
    gem_trash_item_t item;
    gem_os_file_info_t info;
    int result;
    int length;

    if (source_path == NULL ||
        gem_os_resolve_entry_under(source_path, "/", source,
                                   sizeof(source)) == 0 ||
        strcmp(source, "/") == 0 || gem_trash_path_is_store(source) != 0 ||
        gem_os_access(source, GEM_OS_ACCESS_READ) == 0 ||
        gem_os_lstat_path(source, &info) == 0) {
        return GEM_TRASH_INVALID;
    }
    base = strrchr(source, '/');
    if (base == NULL || base[1] == '\0') {
        return GEM_TRASH_INVALID;
    }
    ++base;
    if (gem_trash_prepare_store_for_source(source, root, sizeof(root)) == 0) {
        return GEM_TRASH_IO_ERROR;
    }

    memset(&item, 0, sizeof(item));
    result = gem_trash_unique_paths(root, base, &item);
    if (result != GEM_TRASH_OK ||
        gem_trash_encode_path(source, encoded, sizeof(encoded)) !=
            GEM_TRASH_OK ||
        gem_os_local_timestamp(item.deletion_date,
                               sizeof(item.deletion_date)) == 0) {
        return result != GEM_TRASH_OK ? result : GEM_TRASH_IO_ERROR;
    }
    length = snprintf(record, sizeof(record),
                      "[Trash Info]\nPath=%s\nDeletionDate=%s\n", encoded,
                      item.deletion_date);
    if (length <= 0 || (size_t)length >= sizeof(record) ||
        gem_os_write_file_atomic(item.info_path, record, (size_t)length) == 0) {
        return GEM_TRASH_IO_ERROR;
    }
    if (gem_os_move_path(source, item.payload_path) == 0) {
        (void)gem_os_unlink(item.info_path);
        return GEM_TRASH_IO_ERROR;
    }
    (void)gem_trash_copy(item.original_path, sizeof(item.original_path),
                         source);
    (void)gem_trash_copy(item.store_root, sizeof(item.store_root), root);
    item.size_bytes = info.size_bytes;
    item.mtime = (int64_t)(info.mtime_ms / 1000u);
    item.is_directory = info.is_directory;
    item.is_executable = info.is_executable;
    if (moved_item != NULL) {
        *moved_item = item;
    }
    return GEM_TRASH_OK;
}

int gem_trash_put(const char *source_path, gem_trash_item_t *moved_item)
{
    int result;

    if (gem_trash_lock() == 0) {
        return GEM_TRASH_IO_ERROR;
    }
    result = gem_trash_put_locked(source_path, moved_item);
    gem_os_mutex_unlock(&gem_trash_mutex);
    return result;
}

static int gem_trash_parse_info(const char *text, char *original,
                                size_t original_size, char *date,
                                size_t date_size)
{
    const char *path_line;
    const char *date_line;
    const char *end;
    char encoded[GEM_TRASH_INFO_MAX];
    size_t length;

    if (text == NULL || strncmp(text, "[Trash Info]\n", 13u) != 0) {
        return 0;
    }
    path_line = strstr(text + 13u, "Path=");
    date_line = strstr(text + 13u, "DeletionDate=");
    if (path_line == NULL || date_line == NULL) {
        return 0;
    }
    path_line += 5;
    end = strchr(path_line, '\n');
    if (end == NULL || end > date_line) {
        return 0;
    }
    length = (size_t)(end - path_line);
    if (length == 0u || length >= sizeof(encoded)) {
        return 0;
    }
    memcpy(encoded, path_line, length);
    encoded[length] = '\0';
    date_line += 13;
    end = strchr(date_line, '\n');
    length = end != NULL ? (size_t)(end - date_line) : strlen(date_line);
    if (length == 0u || length >= date_size || length > GEM_TRASH_DATE_MAX) {
        return 0;
    }
    memcpy(date, date_line, length);
    date[length] = '\0';
    return gem_trash_decode_path(encoded, original, original_size) ==
           GEM_TRASH_OK;
}

static int gem_trash_list_store(const char *root,
                                gem_trash_list_callback_t callback,
                                void *context, size_t *count)
{
    char files[GEM_OS_PATH_MAX];
    char info_dir[GEM_OS_PATH_MAX];
    gem_os_dir_t directory = {0};
    gem_os_dirent_t entry;

    if (gem_trash_store_dirs(root, files, sizeof(files), info_dir,
                             sizeof(info_dir)) == 0 ||
        gem_os_dir_open(info_dir, &directory) == 0) {
        return GEM_TRASH_OK;
    }
    while (gem_os_dir_read(&directory, &entry) != 0) {
        static const char suffix[] = ".trashinfo";
        char record[GEM_TRASH_INFO_MAX];
        gem_trash_item_t item;
        gem_os_file_info_t payload_info;
        size_t name_length = strlen(entry.name);
        size_t base_length;

        if (name_length <= sizeof(suffix) - 1u ||
            strcmp(entry.name + name_length - (sizeof(suffix) - 1u), suffix) !=
                0) {
            continue;
        }
        memset(&item, 0, sizeof(item));
        base_length = name_length - (sizeof(suffix) - 1u);
        if (base_length >= sizeof(item.name)) {
            continue;
        }
        memcpy(item.name, entry.name, base_length);
        item.name[base_length] = '\0';
        if (gem_trash_join(info_dir, entry.name, item.info_path,
                           sizeof(item.info_path)) == 0 ||
            gem_trash_join(files, item.name, item.payload_path,
                           sizeof(item.payload_path)) == 0 ||
            gem_os_read_file(item.info_path, record, sizeof(record), NULL) ==
                0 ||
            gem_trash_parse_info(record, item.original_path,
                                 sizeof(item.original_path), item.deletion_date,
                                 sizeof(item.deletion_date)) == 0 ||
            gem_os_lstat_path(item.payload_path, &payload_info) == 0 ||
            gem_trash_copy(item.store_root, sizeof(item.store_root), root) ==
                0) {
            continue;
        }
        {
            const char *original_name = strrchr(item.original_path, '/');

            if (original_name != NULL && original_name[1] != '\0') {
                (void)gem_trash_copy(item.name, sizeof(item.name),
                                     original_name + 1);
            }
        }
        item.size_bytes = payload_info.size_bytes;
        item.mtime = (int64_t)(payload_info.mtime_ms / 1000u);
        item.is_directory = payload_info.is_directory;
        item.is_executable = payload_info.is_executable;
        ++*count;
        if (callback != NULL && callback(&item, context) == 0) {
            gem_os_dir_close(&directory);
            return GEM_TRASH_FULL;
        }
    }
    gem_os_dir_close(&directory);
    return GEM_TRASH_OK;
}

int gem_trash_list(gem_trash_list_callback_t callback, void *context,
                   size_t *item_count)
{
    gem_trash_store_list_t stores;
    size_t count = 0u;
    size_t index;
    int result = GEM_TRASH_OK;

    gem_trash_known_stores(&stores);
    for (index = 0u; index < stores.count; ++index) {
        result = gem_trash_list_store(stores.paths[index], callback, context,
                                      &count);
        if (result != GEM_TRASH_OK) {
            break;
        }
    }
    if (item_count != NULL) {
        *item_count = count;
    }
    return result;
}

int gem_trash_resolve_directory(const gem_trash_item_t *item, const char *path,
                                char *resolved, size_t resolved_size)
{
    char files_root[GEM_OS_PATH_MAX];
    char info_root[GEM_OS_PATH_MAX];
    gem_os_file_info_t info;

    if (item == NULL || path == NULL || resolved == NULL ||
        gem_trash_store_dirs(item->store_root, files_root, sizeof(files_root),
                             info_root, sizeof(info_root)) == 0 ||
        gem_os_resolve_under(path, files_root, resolved, resolved_size) == 0 ||
        gem_os_lstat_path(resolved, &info) == 0 || info.is_directory == 0 ||
        info.is_symlink != 0) {
        return GEM_TRASH_INVALID;
    }
    return GEM_TRASH_OK;
}

static int gem_trash_parent_path(const char *path, char *parent,
                                 size_t parent_size)
{
    char *slash;

    if (gem_trash_copy(parent, parent_size, path) == 0) {
        return 0;
    }
    slash = strrchr(parent, '/');
    if (slash == NULL) {
        return 0;
    }
    if (slash == parent) {
        slash[1] = '\0';
    } else {
        *slash = '\0';
    }
    return 1;
}

static int gem_trash_validate_item(const gem_trash_item_t *item,
                                   char *payload, size_t payload_size,
                                   char *info, size_t info_size)
{
    char files_root[GEM_OS_PATH_MAX];
    char info_root[GEM_OS_PATH_MAX];

    return item != NULL && item->original_path[0] == '/' &&
           gem_trash_store_dirs(item->store_root, files_root,
                                sizeof(files_root), info_root,
                                sizeof(info_root)) != 0 &&
           gem_os_resolve_entry_under(item->payload_path, files_root, payload,
                                      payload_size) != 0 &&
           gem_os_resolve_entry_under(item->info_path, info_root, info,
                                      info_size) != 0;
}

static int gem_trash_restore_to(const gem_trash_item_t *item,
                                const char *destination)
{
    char payload[GEM_OS_PATH_MAX];
    char info[GEM_OS_PATH_MAX];
    char parent[GEM_OS_PATH_MAX];
    if (gem_trash_validate_item(item, payload, sizeof(payload), info,
                                sizeof(info)) == 0) {
        return GEM_TRASH_INVALID;
    }
    if (destination == NULL || destination[0] != '/' ||
        gem_os_path_exists(destination) != 0) {
        return GEM_TRASH_EXISTS;
    }
    if (gem_trash_parent_path(destination, parent, sizeof(parent)) == 0 ||
        gem_os_mkdir_p(parent) == 0 ||
        gem_os_rename(payload, destination) == 0) {
        return GEM_TRASH_IO_ERROR;
    }
    if (gem_os_unlink(info) == 0) {
        return GEM_TRASH_IO_ERROR;
    }
    return GEM_TRASH_OK;
}

int gem_trash_restore(const gem_trash_item_t *item)
{
    return item != NULL ? gem_trash_restore_to(item, item->original_path)
                        : GEM_TRASH_INVALID;
}

int gem_trash_restore_renamed(const gem_trash_item_t *item)
{
    char destination[GEM_OS_PATH_MAX];
    unsigned int suffix;

    if (item == NULL || item->original_path[0] != '/') {
        return GEM_TRASH_INVALID;
    }
    for (suffix = 1u; suffix < 100000u; ++suffix) {
        int length = snprintf(destination, sizeof(destination), "%s.%u",
                              item->original_path, suffix);

        if (length <= 0 || (size_t)length >= sizeof(destination)) {
            return GEM_TRASH_FULL;
        }
        if (gem_os_path_exists(destination) == 0) {
            return gem_trash_restore_to(item, destination);
        }
    }
    return GEM_TRASH_FULL;
}

int gem_trash_purge(const gem_trash_item_t *item)
{
    char payload[GEM_OS_PATH_MAX];
    char info[GEM_OS_PATH_MAX];
    char files_root[GEM_OS_PATH_MAX];
    char info_root[GEM_OS_PATH_MAX];

    if (gem_trash_validate_item(item, payload, sizeof(payload), info,
                                sizeof(info)) == 0 ||
        gem_trash_store_dirs(item->store_root, files_root, sizeof(files_root),
                             info_root, sizeof(info_root)) == 0) {
        return GEM_TRASH_INVALID;
    }
    if (gem_os_remove_tree_under(payload, files_root) == 0) {
        return GEM_TRASH_IO_ERROR;
    }
    return gem_os_unlink(info) != 0 ? GEM_TRASH_OK : GEM_TRASH_IO_ERROR;
}

typedef struct gem_trash_empty_context {
    size_t removed;
    int result;
} gem_trash_empty_context_t;

static int gem_trash_empty_item(const gem_trash_item_t *item, void *context)
{
    gem_trash_empty_context_t *empty = (gem_trash_empty_context_t *)context;

    empty->result = gem_trash_purge(item);
    if (empty->result == GEM_TRASH_OK) {
        ++empty->removed;
        return 1;
    }
    return 0;
}

int gem_trash_empty(size_t *removed_count)
{
    gem_trash_empty_context_t context = {0u, GEM_TRASH_OK};
    int result = gem_trash_list(gem_trash_empty_item, &context, NULL);

    if (removed_count != NULL) {
        *removed_count = context.removed;
    }
    if (context.result != GEM_TRASH_OK) {
        return context.result;
    }
    return result == GEM_TRASH_FULL ? GEM_TRASH_IO_ERROR : result;
}
