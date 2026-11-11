/*
 * Resolves shared Desktop, File Manager and Trash icon resources by entry
 * name, file extension (icons/.txt.rsc) and type, with bounded caching
 * and stable storage for built-in icon replacements.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (C) 2026 tomaz stih
 */

#include "desktop_icon_lookup.h"

#include <gem/os.h>
#include <stdio.h>
#include <string.h>

static void desktop_standard_icons_refresh(void);

enum {
    /* Longest entry name (without extension) that may own an icon. */
    DESKTOP_ICON_NAME_MAX = 64,
    /* Entry icons remembered between browser reloads. */
    DESKTOP_ICON_CACHE_SLOTS = 64
};

/* One remembered lookup; `found` is zero for names without an icon. */
typedef struct desktop_icon_cache_slot {
    char name[DESKTOP_ICON_NAME_MAX];
    WORD used;
    WORD found;
    desktop_custom_icon_t icon;
} desktop_icon_cache_slot_t;

static desktop_icon_cache_slot_t desktop_icon_cache[DESKTOP_ICON_CACHE_SLOTS];
static size_t desktop_icon_cache_next;

/*
 * Copies the icon name of an entry (its leaf name with any extension
 * removed) into `name`. Hidden, empty and overlong names have no icon.
 */
static int desktop_program_icon_name(const char *program, char *name,
                                     size_t size)
{
    const char *leaf;
    const char *dot;
    size_t stem;

    if (program == NULL || name == NULL || size == 0u)
        return 0;
    leaf = strrchr(program, '/');
    leaf = leaf != NULL ? leaf + 1 : program;
    dot = strrchr(leaf, '.');
    stem = dot != NULL ? (size_t)(dot - leaf) : strlen(leaf);
    if (stem == 0u || leaf[0] == '.' || stem >= size)
        return 0;
    memcpy(name, leaf, stem);
    name[stem] = '\0';
    return 1;
}

/*
 * Loads `icons/<name>.rsc` from the GEM resource search path (see
 * gem_os_find_resource) into icon. Returns nonzero when an icon-only
 * resource with a valid ICONBLK was read.
 */
static int desktop_load_program_icon(const char *name,
                                     desktop_custom_icon_t *icon)
{
    char relative[sizeof("icons/") + DESKTOP_ICON_NAME_MAX + sizeof(".rsc")];
    char path[GEM_OS_PATH_MAX];
    unsigned char *bytes;
    size_t length = 0;
    int ok = 0;

    if (strlen(name) >= DESKTOP_ICON_NAME_MAX)
        return 0;
    snprintf(relative, sizeof(relative), "icons/%s.rsc", name);
    if (!gem_os_find_resource(relative, path, sizeof(path)))
        return 0;
    bytes = gem_os_alloc(DESKTOP_RSC_ICON_MAX_FILE + 1u);
    if (bytes == NULL)
        return 0;
    if (gem_os_read_file(path, (char *)bytes, DESKTOP_RSC_ICON_MAX_FILE + 1u,
                         &length) &&
        desktop_icon_rsc_decode(bytes, length, &icon->bitmap)) {
        icon->asset.mask_bits = icon->bitmap.mask;
        icon->asset.data_bits = icon->bitmap.data;
        icon->asset.width = icon->bitmap.width;
        icon->asset.height = icon->bitmap.height;
        ok = 1;
    }
    gem_os_free(bytes);
    return ok;
}

/*
 * Returns the cached or freshly loaded icon for a validated lookup key: an
 * entry stem or a lowercase extension key such as ".txt". Keys starting with
 * '.' never collide with stems, which cannot start with one.
 */
static const desktop_icon_asset_t *desktop_cached_icon(const char *name)
{
    desktop_icon_cache_slot_t *slot;
    size_t i;

    for (i = 0; i < DESKTOP_ICON_CACHE_SLOTS; ++i) {
        slot = &desktop_icon_cache[i];
        if (slot->used != 0 && strcmp(slot->name, name) == 0)
            return slot->found != 0 ? &slot->icon.asset : NULL;
    }
    /* Round-robin replacement; callers use the asset immediately. */
    slot = &desktop_icon_cache[desktop_icon_cache_next];
    desktop_icon_cache_next =
        (desktop_icon_cache_next + 1u) % DESKTOP_ICON_CACHE_SLOTS;
    memset(slot, 0, sizeof(*slot));
    memcpy(slot->name, name, strlen(name) + 1u);
    slot->used = 1;
    slot->found = (WORD)desktop_load_program_icon(name, &slot->icon);
    return slot->found != 0 ? &slot->icon.asset : NULL;
}

const desktop_icon_asset_t *desktop_program_icon(const char *program)
{
    char name[DESKTOP_ICON_NAME_MAX];

    if (!desktop_program_icon_name(program, name, sizeof(name)))
        return NULL;
    return desktop_cached_icon(name);
}

const desktop_icon_asset_t *desktop_extension_icon(const char *file)
{
    char key[DESKTOP_ICON_NAME_MAX];
    const char *leaf;
    const char *dot;

    if (file == NULL)
        return NULL;
    leaf = strrchr(file, '/');
    leaf = leaf != NULL ? leaf + 1 : file;
    if (leaf[0] == '\0')
        return NULL;
    /* A leading dot marks a hidden file, not an extension. Try the longest
     * extension first, so "a.tar.gz" uses ".tar.gz" before ".gz". */
    for (dot = strchr(leaf + 1, '.'); dot != NULL;
         dot = strchr(dot + 1, '.')) {
        size_t length = strlen(dot);
        size_t i;
        int usable = length > 1u && length < sizeof(key);
        const desktop_icon_asset_t *asset;

        for (i = 0; usable && i < length; ++i) {
            unsigned char ch = (unsigned char)dot[i];

            /* Extension keys are file names; reject separators and empty
             * components such as "a..txt" or a trailing dot. */
            if (ch < 32u || ch == '\\' ||
                (ch == '.' && (i + 1u == length || dot[i + 1u] == '.'))) {
                usable = 0;
                break;
            }
            key[i] = (char)(ch >= 'A' && ch <= 'Z' ? ch - 'A' + 'a' : ch);
        }
        if (!usable)
            continue;
        key[length] = '\0';
        asset = desktop_cached_icon(key);
        if (asset != NULL)
            return asset;
    }
    return NULL;
}

void desktop_program_icons_forget(void)
{
    memset(desktop_icon_cache, 0, sizeof(desktop_icon_cache));
    desktop_icon_cache_next = 0;
    desktop_standard_icons_refresh();
}

/* Common icons have stable addresses: desktop objects retain their assets. */
static struct {
    const char *name;
    const desktop_icon_asset_t *fallback;
    int loaded;
    desktop_custom_icon_t icon;
} desktop_standard_icons[] = {
    {.name = "folder", .fallback = &desktop_folder_icon_asset},
    {.name = "disk", .fallback = &desktop_disk_icon_asset},
    {.name = "document", .fallback = &desktop_doc_icon_asset},
    {.name = "program", .fallback = &desktop_prg_icon_asset},
    {.name = "trash", .fallback = &desktop_trash_icon_asset}};

static void desktop_standard_icon_load(size_t i)
{
    if (!desktop_load_program_icon(desktop_standard_icons[i].name,
                                   &desktop_standard_icons[i].icon))
        desktop_standard_icons[i].icon.asset =
            *desktop_standard_icons[i].fallback;
    desktop_standard_icons[i].loaded = 1;
}

static void desktop_standard_icons_refresh(void)
{
    for (size_t i = 0;
         i < sizeof(desktop_standard_icons) / sizeof(desktop_standard_icons[0]);
         ++i) {
        if (desktop_standard_icons[i].loaded)
            desktop_standard_icon_load(i);
    }
}

const desktop_icon_asset_t *
desktop_standard_icon(const desktop_icon_asset_t *fallback)
{
    for (size_t i = 0;
         i < sizeof(desktop_standard_icons) / sizeof(desktop_standard_icons[0]);
         ++i) {
        if (desktop_standard_icons[i].fallback == fallback) {
            if (!desktop_standard_icons[i].loaded)
                desktop_standard_icon_load(i);
            return &desktop_standard_icons[i].icon.asset;
        }
    }
    return fallback;
}

const desktop_icon_asset_t *
desktop_named_icon(const char *name, const desktop_icon_asset_t *fallback)
{
    const desktop_icon_asset_t *own = desktop_program_icon(name);

    if (own != NULL)
        return own;
    /* Extension icons describe files, never folders, disks or Trash. */
    if (fallback == &desktop_doc_icon_asset ||
        fallback == &desktop_prg_icon_asset) {
        own = desktop_extension_icon(name);
        if (own != NULL)
            return own;
    }
    return desktop_standard_icon(fallback);
}

const desktop_icon_asset_t *
desktop_icon_copy(const char *name, const desktop_icon_asset_t *fallback,
                  desktop_custom_icon_t *storage)
{
    const desktop_icon_asset_t *asset = desktop_named_icon(name, fallback);
    size_t bytes = ((size_t)asset->width + 15u) / 16u * (size_t)asset->height *
                   sizeof(UWORD);

    memcpy(storage->bitmap.mask, asset->mask_bits, bytes);
    memcpy(storage->bitmap.data, asset->data_bits, bytes);
    storage->asset = *asset;
    storage->asset.mask_bits = storage->bitmap.mask;
    storage->asset.data_bits = storage->bitmap.data;
    return &storage->asset;
}
