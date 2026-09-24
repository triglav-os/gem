/*
 * Walks FAT12 directory trees: short-name and attribute formatting, entry
 * decoding, the fixed root region and cluster-chained subdirectories,
 * with visited-cluster, depth and entry budgets against hostile images.
 *
 * MIT License (see: LICENSE)
 * Copyright (C) 2026 tomaz stih
 */

#include "fat12_private.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int fat12_walk_directory_region(const fat12_image_t *image,
                                       const uint8_t *dir_data, size_t dir_size,
                                       const char *prefix,
                                       fat12_walk_fn callback, void *context,
                                       fat12_walk_state_t *walk,
                                       char *error_text, size_t error_size);
static int fat12_walk_cluster_directory(const fat12_image_t *image,
                                        uint16_t first_cluster,
                                        const char *prefix,
                                        fat12_walk_fn callback, void *context,
                                        fat12_walk_state_t *walk,
                                        char *error_text, size_t error_size);

int fat12_walk_root(const fat12_image_t *image, fat12_walk_fn callback,
                    void *context, char *error_text, size_t error_size)
{
    uint8_t *root_dir;
    size_t root_size;
    int ok;
    fat12_walk_state_t walk = {0};

    if (image == NULL || callback == NULL) {
        fat12_set_error(error_text, error_size, "invalid FAT12 walk arguments");
        return 0;
    }

    root_size = (size_t)image->bpb.root_dir_sector_count *
                (size_t)image->bpb.bytes_per_sector;
    root_dir = malloc(root_size);
    if (root_dir == NULL) {
        fat12_set_error(error_text, error_size, "out of memory");
        return 0;
    }
    ok = fat12_read_bytes(image,
                          image->bpb.root_dir_sector *
                              (uint32_t)image->bpb.bytes_per_sector,
                          root_dir, root_size);
    if (!ok) {
        free(root_dir);
        fat12_set_error(error_text, error_size,
                        "failed to read FAT12 root directory");
        return 0;
    }

    ok = fat12_walk_directory_region(image, root_dir, root_size, "", callback,
                                     context, &walk, error_text, error_size);
    free(root_dir);
    return ok;
}

void fat12_format_attr(uint8_t attr, char *attr_out, size_t attr_size)
{
    char buffer[8];
    size_t index = 0;

    if (attr_out == NULL || attr_size == 0u) {
        return;
    }

    if ((attr & FAT12_ATTR_DIRECTORY) != 0u) {
        buffer[index++] = 'D';
    }
    if ((attr & FAT12_ATTR_VOLUME) != 0u) {
        buffer[index++] = 'V';
    }
    if ((attr & FAT12_ATTR_READ_ONLY) != 0u) {
        buffer[index++] = 'R';
    }
    if ((attr & FAT12_ATTR_HIDDEN) != 0u) {
        buffer[index++] = 'H';
    }
    if ((attr & FAT12_ATTR_SYSTEM) != 0u) {
        buffer[index++] = 'S';
    }
    if ((attr & FAT12_ATTR_ARCHIVE) != 0u) {
        buffer[index++] = 'A';
    }
    if (index == 0u) {
        buffer[index++] = '-';
    }
    buffer[index] = '\0';
    snprintf(attr_out, attr_size, "%s", buffer);
}

static void fat12_trim_spaces(char *text)
{
    size_t length;

    if (text == NULL) {
        return;
    }
    length = strlen(text);
    while (length > 0u && text[length - 1u] == ' ') {
        text[length - 1u] = '\0';
        --length;
    }
}

static void fat12_format_name(const uint8_t entry[32], char *name_out,
                              size_t name_size)
{
    char base[9];
    char ext[4];

    if (name_out == NULL || name_size == 0u) {
        return;
    }

    memcpy(base, entry, 8u);
    if ((unsigned char)base[0] == 0x05u)
        base[0] = (char)0xe5u;
    memcpy(ext, entry + 8, 3u);
    base[8] = '\0';
    ext[3] = '\0';
    fat12_trim_spaces(base);
    fat12_trim_spaces(ext);

    if (ext[0] != '\0') {
        snprintf(name_out, name_size, "%s.%s", base, ext);
    } else {
        snprintf(name_out, name_size, "%s", base);
    }
}

static int fat12_is_dot_entry(const fat12_dirent_t *entry)
{
    return entry != NULL &&
           (strcmp(entry->name, ".") == 0 || strcmp(entry->name, "..") == 0);
}

static int fat12_read_entry(const uint8_t entry_bytes[32],
                            fat12_dirent_t *entry)
{
    if (entry_bytes == NULL || entry == NULL) {
        return 0;
    }
    if (entry_bytes[0] == 0x00u || entry_bytes[0] == 0xe5u ||
        entry_bytes[11] == fat12_attr_long_name) {
        return 0;
    }

    memset(entry, 0, sizeof(*entry));
    fat12_format_name(entry_bytes, entry->name, sizeof(entry->name));
    entry->attr = entry_bytes[11];
    entry->first_cluster = fat12_read_le16(entry_bytes + 26);
    entry->size = fat12_read_le32(entry_bytes + 28);
    return 1;
}

static int fat12_walk_directory_region(const fat12_image_t *image,
                                       const uint8_t *dir_data, size_t dir_size,
                                       const char *prefix,
                                       fat12_walk_fn callback, void *context,
                                       fat12_walk_state_t *walk,
                                       char *error_text, size_t error_size)
{
    size_t offset;

    for (offset = 0; offset + 32u <= dir_size; offset += 32u) {
        const uint8_t *entry_bytes = dir_data + offset;
        fat12_dirent_t entry;
        char path[256];

        if (entry_bytes[0] == 0x00u) {
            break;
        }
        if (!fat12_read_entry(entry_bytes, &entry)) {
            continue;
        }

        if (fat12_is_dot_entry(&entry))
            continue;
        if (++walk->entries > 65536u || !entry.name[0]) {
            fat12_set_error(error_text, error_size,
                            "invalid FAT12 directory size");
            return 0;
        }
        for (size_t i = 0; entry.name[i]; ++i) {
            unsigned char ch = (unsigned char)entry.name[i];
            if (ch < 32u || ch == '/' || ch == '\\' || ch == ':') {
                fat12_set_error(error_text, error_size,
                                "unsafe FAT12 filename");
                return 0;
            }
        }
        int length =
            prefix && *prefix
                ? snprintf(path, sizeof(path), "%s/%s", prefix, entry.name)
                : snprintf(path, sizeof(path), "%s", entry.name);
        if (length < 0 || (size_t)length >= sizeof(path)) {
            fat12_set_error(error_text, error_size, "FAT12 path exceeds limit");
            return 0;
        }
        if (!callback(&entry, path, context)) {
            walk->stopped = 1;
            return 1;
        }
        if ((entry.attr & FAT12_ATTR_DIRECTORY) != 0u &&
            !fat12_is_dot_entry(&entry) && entry.first_cluster >= 2u) {
            if (walk->depth >= 32u) {
                fat12_set_error(error_text, error_size,
                                "FAT12 nesting exceeds limit");
                return 0;
            }
            ++walk->depth;
            int ok = fat12_walk_cluster_directory(image, entry.first_cluster,
                                                  path, callback, context, walk,
                                                  error_text, error_size);
            --walk->depth;
            if (!ok)
                return 0;
            if (walk->stopped)
                return 1;
        }
    }

    return 1;
}

static int fat12_walk_cluster_directory(const fat12_image_t *image,
                                        uint16_t first_cluster,
                                        const char *prefix,
                                        fat12_walk_fn callback, void *context,
                                        fat12_walk_state_t *walk,
                                        char *error_text, size_t error_size)
{
    uint16_t cluster = first_cluster;
    uint16_t guard = 0;
    uint8_t *dir_data;

    dir_data = malloc(image->bpb.cluster_size);
    if (dir_data == NULL) {
        fat12_set_error(error_text, error_size, "out of memory");
        return 0;
    }

    while (cluster >= 2u && cluster < fat12_cluster_end_min) {
        if (walk->visited[cluster]) {
            free(dir_data);
            fat12_set_error(error_text, error_size, "FAT12 directory cycle");
            return 0;
        }
        walk->visited[cluster] = 1;
        if (!fat12_read_bytes(image, fat12_cluster_offset(image, cluster),
                              dir_data, image->bpb.cluster_size)) {
            free(dir_data);
            fat12_set_error(error_text, error_size,
                            "failed to read FAT12 directory cluster");
            return 0;
        }
        if (!fat12_walk_directory_region(
                image, dir_data, image->bpb.cluster_size, prefix, callback,
                context, walk, error_text, error_size)) {
            free(dir_data);
            return 0;
        }

        if (walk->stopped)
            break;
        cluster = fat12_next_cluster(image, cluster);
        if (cluster == fat12_cluster_bad) {
            free(dir_data);
            fat12_set_error(error_text, error_size,
                            "invalid FAT12 cluster chain");
            return 0;
        }
        if (++guard > 4096u) {
            free(dir_data);
            fat12_set_error(error_text, error_size,
                            "FAT12 cluster loop detected");
            return 0;
        }
    }

    free(dir_data);
    return 1;
}
