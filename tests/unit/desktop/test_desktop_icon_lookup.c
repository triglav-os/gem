/*
 * Verify shared icon precedence, invalid-resource fallback, browser and Trash
 * selection, cache refresh and the lifetime of retained desktop assets.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (C) 2026 tomaz stih
 */
#include "desktop.h"

#include <stdio.h>
#include <stdlib.h>

#include <assert.h>
#include <stdint.h>
#include <string.h>

enum { PLANE_BYTES = 2 * 32 * sizeof(UWORD) };

static _Alignas(ICONBLK) unsigned char file[1024];
static size_t file_size;

/* Lay out RSHDR, one ICONBLK and its planes the way resgen does. */
static void build(void)
{
    RSHDR header;
    ICONBLK icon;
    size_t icon_at = (sizeof(header) + 7u) & ~(size_t)7u;
    size_t planes_at = icon_at + sizeof(icon);

    memset(file, 0, sizeof(file));
    memset(&header, 0, sizeof(header));
    memset(&icon, 0, sizeof(icon));
    header.rsh_iconblk = (WORD)icon_at;
    header.rsh_nib = 1;
    icon.ib_pmask = (LONG)planes_at;
    icon.ib_pdata = (LONG)(planes_at + PLANE_BYTES);
    icon.ib_wicon = 32;
    icon.ib_hicon = 32;
    for (size_t i = 0; i < PLANE_BYTES / sizeof(UWORD); ++i) {
        UWORD mask = (UWORD)(0xff00u >> (i % 8));
        UWORD data = (UWORD)(i * 0x0101u);

        memcpy(file + planes_at + i * sizeof(UWORD), &mask, sizeof(mask));
        memcpy(file + planes_at + PLANE_BYTES + i * sizeof(UWORD), &data,
               sizeof(data));
    }
    memcpy(file, &header, sizeof(header));
    memcpy(file + icon_at, &icon, sizeof(icon));
    file_size = planes_at + 2u * PLANE_BYTES;
}

static const char *available[16];
static int invalid_resource;
static const char *invalid_name;
static size_t resource_index;

/* Mock only platform file access; use real resource parsing and selection. */
int gem_os_find_resource(const char *name, char *path, size_t capacity)
{
    for (size_t i = 0; i < 16; ++i) {
        if (available[i] != NULL && strcmp(name, available[i]) == 0) {
            snprintf(path, capacity, "%s", name);
            resource_index = i;
            return 1;
        }
    }
    return 0;
}

int gem_os_read_file(const char *path, char *buffer, size_t capacity,
                     size_t *length)
{
    RSHDR header;
    ICONBLK icon;
    UWORD marker = (UWORD)(resource_index + 1);

    build();
    memcpy(&header, file, sizeof(header));
    memcpy(&icon, file + header.rsh_iconblk, sizeof(icon));
    memcpy(file + icon.ib_pdata, &marker, sizeof(marker));
    if (invalid_resource ||
        (invalid_name != NULL && strcmp(path, invalid_name) == 0)) {
        header.rsh_ntree = 1;
        memcpy(file, &header, sizeof(header));
    }
    assert(capacity >= file_size);
    memcpy(buffer, file, file_size);
    *length = file_size;
    return 1;
}

void *gem_os_alloc(size_t size)
{
    return malloc(size);
}

void gem_os_free(void *pointer)
{
    free(pointer);
}

int main(void)
{
    const desktop_icon_asset_t *defaults[] = {
        &desktop_folder_icon_asset, &desktop_disk_icon_asset,
        &desktop_doc_icon_asset, &desktop_prg_icon_asset,
        &desktop_trash_icon_asset};
    const char *names[] = {"icons/folder.rsc", "icons/disk.rsc",
                           "icons/document.rsc", "icons/program.rsc",
                           "icons/trash.rsc"};
    desktop_browser_window_t browser = {0};
    desktop_browser_entry_t entry = {0};
    desktop_custom_icon_t retained;
    const desktop_icon_asset_t *asset;
    const desktop_icon_asset_t *trash;

    /* No external icons: every category retains its built-in artwork. */
    for (size_t i = 0; i < 5; ++i) {
        asset = desktop_standard_icon(defaults[i]);
        assert(asset->data_bits == defaults[i]->data_bits);
        available[i] = names[i];
    }
    desktop_program_icons_forget();
    for (size_t i = 0; i < 5; ++i) {
        asset = desktop_standard_icon(defaults[i]);
        assert(asset->data_bits[0] == i + 1);
    }
    trash = desktop_standard_icon(&desktop_trash_icon_asset);

    /* Named entries beat type icons in both File Manager and Trash. */
    available[5] = "icons/report.rsc";
    available[6] = "icons/work.rsc";
    available[7] = "icons/filemanager.rsc";
    for (int backend = DESKTOP_BROWSER_FILES; backend <= DESKTOP_BROWSER_TRASH;
         ++backend) {
        browser.backend = (WORD)backend;
        strcpy(entry.name, "report.txt");
        entry.is_dir = entry.is_app = entry.is_parent = 0;
        assert(desktop_browser_entry_asset(&browser, &entry)->data_bits[0] ==
               6);
        strcpy(entry.name, "unknown.txt");
        assert(desktop_browser_entry_asset(&browser, &entry)->data_bits[0] ==
               3);
        entry.is_app = 1;
        assert(desktop_browser_entry_asset(&browser, &entry)->data_bits[0] ==
               4);
        strcpy(entry.name, "work");
        entry.is_dir = 1;
        assert(desktop_browser_entry_asset(&browser, &entry)->data_bits[0] ==
               7);
        entry.is_parent = 1;
        assert(desktop_browser_entry_asset(&browser, &entry)->data_bits[0] ==
               1);
    }
    assert(
        desktop_icon_copy("filemanager", &desktop_folder_icon_asset, &retained)
            ->data_bits[0] == 8);
    assert(desktop_named_icon("missing", &desktop_folder_icon_asset)
               ->data_bits[0] == 1);

    /* Retained launcher and common icons survive cache replacement. */
    for (int i = 0; i < 100; ++i) {
        char name[32];
        snprintf(name, sizeof(name), "missing%d", i);
        (void)desktop_program_icon(name);
    }
    assert(retained.asset.data_bits[0] == 8);
    assert(trash->data_bits[0] == 5);

    /* Invalid per-entry artwork still falls back to a valid type icon. */
    invalid_name = "icons/report.rsc";
    desktop_program_icons_forget();
    assert(desktop_named_icon("report.txt", &desktop_doc_icon_asset)
               ->data_bits[0] == 3);
    invalid_name = NULL;

    /* Corrupt resources are rejected, including every common category. */
    invalid_resource = 1;
    desktop_program_icons_forget();
    for (size_t i = 0; i < 5; ++i)
        assert(desktop_named_icon("report.txt", defaults[i])->data_bits ==
               defaults[i]->data_bits);
    assert(trash->data_bits == desktop_trash_icon_asset.data_bits);
    assert(retained.asset.data_bits[0] == 8);

    /* Missing named icons still use a valid common icon after refresh. */
    invalid_resource = 0;
    available[5] = NULL;
    desktop_program_icons_forget();
    assert(desktop_named_icon("report.txt", &desktop_doc_icon_asset)
               ->data_bits[0] == 3);
    available[5] = "icons/report.rsc";
    desktop_program_icons_forget();
    assert(desktop_named_icon("report.txt", &desktop_doc_icon_asset)
               ->data_bits[0] == 6);
    assert(desktop_program_icon("../.hidden") == NULL);
    assert(desktop_program_icon(NULL) == NULL);
    return 0;
}
