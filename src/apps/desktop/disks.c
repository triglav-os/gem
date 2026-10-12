/*
 * Discovers the disks shown on the desktop: filters mount types and
 * paths, derives labels and free space, and builds the disk, folder and
 * sorted icon list.
 *
 * MIT License (see: LICENSE)
 * Copyright (C) 2026 tomaz stih
 */

#include "desktop.h"

#include <stdio.h>
#include <string.h>
#include <strings.h>

static void desktop_copy_text(char *target, size_t target_size,
                              const char *source)
{
    size_t length;

    if (target == NULL || target_size == 0u) {
        return;
    }
    if (source == NULL) {
        target[0] = '\0';
        return;
    }
    length = strlen(source);
    if (length >= target_size) {
        length = target_size - 1u;
    }
    memmove(target, source, length);
    target[length] = '\0';
}

static int desktop_path_space(const char *path, uint64_t *total_bytes,
                              uint64_t *avail_bytes)
{
    return gem_os_space(path, total_bytes, avail_bytes);
}

static void desktop_label_from_path(const char *path, char *label,
                                    size_t label_size)
{
    const char *base;
    size_t i;
    int new_word = 1;

    if (label_size == 0u) {
        return;
    }

    if (strcmp(path, "/") == 0) {
        desktop_copy_text(label, label_size, "ROOT");
        return;
    }

    base = strrchr(path, '/');
    if (base != NULL && base[1] != '\0') {
        ++base;
    } else {
        base = path;
    }

    desktop_copy_text(label, label_size, base);
    for (i = 0; label[i] != '\0'; ++i) {
        if (label[i] == ' ' || label[i] == '_' || label[i] == '-') {
            new_word = 1;
            continue;
        }
        if (new_word != 0 && label[i] >= 'a' && label[i] <= 'z') {
            label[i] = (char)(label[i] - ('a' - 'A'));
        } else if (new_word == 0 && label[i] >= 'A' && label[i] <= 'Z') {
            label[i] = (char)(label[i] + ('a' - 'A'));
        }
        new_word = 0;
    }
}

static int desktop_has_path(const char *path, int is_folder)
{
    WORD i;

    for (i = 0; i < g_desktop.icon_count; ++i) {
        if (g_desktop.icons[i].is_folder == is_folder &&
            strcmp(g_desktop.icons[i].path, path) == 0) {
            return 1;
        }
    }
    return 0;
}

static void desktop_add_folder_icon(const char *path, const char *label)
{
    desktop_icon_entry_t *entry;

    if (g_desktop.icon_count >= DESKTOP_MAX_ICONS || path == NULL ||
        desktop_has_path(path, 1) != 0) {
        return;
    }

    entry = &g_desktop.icons[g_desktop.icon_count];
    memset(entry, 0, sizeof(*entry));
    desktop_copy_text(entry->path, sizeof(entry->path), path);
    if (label != NULL && label[0] != '\0') {
        desktop_copy_text(entry->label, sizeof(entry->label), label);
    } else {
        desktop_label_from_path(path, entry->label, sizeof(entry->label));
    }
    entry->asset = &desktop_folder_icon_asset;
    entry->draw_info.asset = entry->asset;
    entry->draw_info.label = entry->label;
    entry->userblk.ab_code = (LONG)(intptr_t)desktop_user_draw;
    entry->userblk.ab_parm = (LONG)(intptr_t)&entry->draw_info;
    entry->object_id = (WORD)(DESKTOP_ICON_BASE + g_desktop.icon_count);
    entry->is_folder = 1;
    if (desktop_path_space(path, &entry->total_bytes, &entry->avail_bytes) ==
        0) {
        entry->total_bytes = 0;
        entry->avail_bytes = 0;
    }
    ++g_desktop.icon_count;
}

static void desktop_mark_disk_icon(const char *path)
{
    WORD i;
    for (i = 0; i < g_desktop.icon_count; ++i) {
        if (strcmp(g_desktop.icons[i].path, path) == 0) {
            g_desktop.icons[i].asset = &desktop_disk_icon_asset;
            g_desktop.icons[i].draw_info.asset = g_desktop.icons[i].asset;
            return;
        }
    }
}

static void desktop_add_configured_locations(void)
{
#ifndef GEM_DESKTOP_HOSTED
    gem_rpc_location_list_t locations;
    uint32_t i;

    if (!gem_location_list(&locations)) {
        return;
    }
    g_desktop.show_hidden =
        (GEM_RPC_LOCATION_LIST_OPTIONS(locations) &
         GEM_RPC_DESKTOP_SHOW_HIDDEN) != 0u;
    for (i = 0; i < locations.count &&
                g_desktop.icon_count < DESKTOP_MAX_ICONS;
         ++i) {
        gem_os_file_info_t info;

        if (locations.entries[i].type == GEM_RPC_LOCATION_DISK_IMAGE ||
            (gem_os_stat_path(locations.entries[i].path, &info) != 0 &&
             info.is_directory != 0)) {
            desktop_add_folder_icon(locations.entries[i].path,
                                    locations.entries[i].label);
            if (locations.entries[i].type == GEM_RPC_LOCATION_DISK_IMAGE)
                desktop_mark_disk_icon(locations.entries[i].path);
        }
    }
#else
    size_t i;
    g_desktop.show_hidden =
        gemconf_get_bool(&g_desktop.config, "desktop", "show_hidden", 0) != 0;
    WORD maximum =
        gemconf_get_bool(&g_desktop.config, "desktop", "show_trash", 1)
            ? (WORD)(DESKTOP_MAX_ICONS - 1)
            : DESKTOP_MAX_ICONS;

    for (i = 0u;
         i < gemconf_count(&g_desktop.config) && g_desktop.icon_count < maximum;
         ++i) {
        const char *section = gemconf_entry_section(&g_desktop.config, i);
        const char *path = gemconf_entry_key(&g_desktop.config, i);
        const char *label = gemconf_entry_value(&g_desktop.config, i);
        gem_os_file_info_t info;

        if (section != NULL && strcmp(section, "locations") == 0 &&
            path != NULL && path[0] == '/' &&
            ((gem_os_stat_path(path, &info) != 0 && info.is_directory != 0) ||
             (strrchr(path, '.') != NULL &&
              (strcasecmp(strrchr(path, '.'), ".st") == 0 ||
               strcasecmp(strrchr(path, '.'), ".msa") == 0)))) {
            desktop_add_folder_icon(path, label);
            if (strrchr(path, '.') != NULL &&
                (strcasecmp(strrchr(path, '.'), ".st") == 0 ||
                 strcasecmp(strrchr(path, '.'), ".msa") == 0))
                desktop_mark_disk_icon(path);
        }
    }
#endif
}

void desktop_probe_disks(void)
{
    g_desktop.icon_count = 0;
    desktop_add_configured_locations();
}

void desktop_publish_location_selection(void)
{
#ifndef GEM_DESKTOP_HOSTED
    const desktop_icon_entry_t *selected =
        desktop_find_icon(g_desktop.selected_icon);

    (void)gem_location_select(selected != NULL && selected->is_folder != 0 &&
                                       selected->is_trash == 0
                                   ? selected->path
                                   : "");
#endif
}

void desktop_refresh_locations(void)
{
    WORD old_show_hidden = g_desktop.show_hidden;
    WORD index;

    (void)gemconf_load(&g_desktop.config, "desktop");
    g_desktop.selected_icon = NIL;
    desktop_publish_location_selection();
    desktop_probe_disks();
    desktop_sort_icons_by_label();
    desktop_install_trash_icon();
    desktop_refresh_icon_bindings();
    desktop_layout_icons();
    if (old_show_hidden != g_desktop.show_hidden) {
        for (index = 0; index < DESKTOP_BROWSER_MAX_WINDOWS; ++index) {
            desktop_browser_window_t *browser = &g_desktop.browsers[index];

            if (browser->used == 0 || desktop_browser_reload(browser) == 0) {
                continue;
            }
            wind_set_str(browser->handle, WF_NAME, browser->title);
            desktop_browser_sync_work(browser);
            desktop_browser_redraw(browser, NULL);
        }
        desktop_update_desk_menu_labels();
    }
    desktop_redraw(NULL);
}

#ifdef GEM_DESKTOP_HOSTED
static void desktop_location_label(const char *path, char *label,
                                   size_t label_size)
{
    const char *leaf;

    if (strcmp(path, "/") == 0) {
        desktop_copy_text(label, label_size, "Root");
        return;
    }
    leaf = strrchr(path, '/');
    leaf = leaf != NULL && leaf[1] != '\0' ? leaf + 1 : path;
    desktop_copy_text(label, label_size, leaf);
}
#endif

void desktop_choose_location(void)
{
    char path[GEM_OS_PATH_MAX];
    char label[24];
    WORD button = 0;

    if (gem_os_getcwd(path, sizeof(path)) == 0) {
        strcpy(path, "/");
    }
    {
        char name[GEM_OS_PATH_MAX] = "";
        WORD old_options = fsel_options(0, 0);
        UWORD options = g_desktop.show_hidden != 0 ? FSEL_SHOW_HIDDEN : 0u;

        (void)fsel_options(options, 1);
        if (fsel_location(path, name, &button) == 0 || button == 0) {
            (void)fsel_options((UWORD)old_options, 1);
            desktop_set_status("Locations unchanged.");
            desktop_redraw(NULL);
            return;
        }
        (void)fsel_options((UWORD)old_options, 1);
        if (name[0] != '\0') {
            char selected[GEM_OS_PATH_MAX];
            if (snprintf(selected, sizeof(selected), "%s/%s", path, name) <= 0 ||
                strlen(selected) >= sizeof(path)) {
                return;
            }
            strcpy(path, selected);
        }
    }
#ifndef GEM_DESKTOP_HOSTED
    (void)label;
    if (gem_location_add(path) <= 0) {
        form_alert(1, "[3][ Unable to save the location. ][ OK ]");
        desktop_set_status("Unable to save location.");
        desktop_redraw(NULL);
        return;
    }
#else
    desktop_location_label(path, label, sizeof(label));
    if (gemconf_set(&g_desktop.config, "locations", path, label) == 0 ||
        gemconf_save(&g_desktop.config) == 0) {
        form_alert(1, "[3][ Unable to save the location. ][ OK ]");
        desktop_set_status("Unable to save location.");
        desktop_redraw(NULL);
        return;
    }
#endif

    desktop_refresh_locations();
    desktop_set_status("Location attached.");
    desktop_redraw(NULL);
}

void desktop_delete_selected_location(void)
{
    const desktop_icon_entry_t *selected =
        desktop_find_icon(g_desktop.selected_icon);
    char alert[192];

    if (selected == NULL || selected->is_folder == 0 ||
        selected->is_trash != 0) {
        desktop_set_status("Select a configured location first.");
        desktop_redraw(NULL);
        return;
    }
    (void)snprintf(alert, sizeof(alert),
                   "[2][ Are you sure you want to remove \"%.*s\"? ]["
                   " Remove | Cancel ]",
                   (int)sizeof(selected->label) - 1, selected->label);
#ifdef GEM_DESKTOP_HOSTED
    if (form_alert(2, alert) != 1) {
        desktop_set_status("Location removal cancelled.");
        desktop_redraw(NULL);
        return;
    }
    if (gemconf_remove(&g_desktop.config, "locations", selected->path) == 0 ||
        gemconf_save(&g_desktop.config) == 0) {
        desktop_set_status("Unable to save location removal.");
        desktop_redraw(NULL);
        return;
    }
#else
    (void)alert;
    {
        int result = gem_location_remove(selected->path);

        if (result == 2) {
            desktop_set_status("Location removal cancelled.");
            desktop_redraw(NULL);
            return;
        }
        if (result <= 0) {
            desktop_set_status("Unable to save location removal.");
            desktop_redraw(NULL);
            return;
        }
    }
#endif

    desktop_refresh_locations();
    desktop_set_status("Location removed.");
    desktop_redraw(NULL);
}

void desktop_sort_icons_by_label(void)
{
    WORD i;
    WORD j;

    if (g_desktop.icon_count < 2) {
        desktop_refresh_icon_bindings();
        return;
    }

    for (i = 0; i < g_desktop.icon_count - 1; ++i) {
        for (j = (WORD)(i + 1); j < g_desktop.icon_count; ++j) {
            int swap_needed = 0;

            if (g_desktop.icons[i].is_trash != 0) {
                swap_needed = 1;
            } else if (g_desktop.icons[j].is_trash == 0 &&
                       strcmp(g_desktop.icons[i].label,
                              g_desktop.icons[j].label) > 0) {
                swap_needed = 1;
            }
            if (swap_needed != 0) {
                desktop_icon_entry_t swap = g_desktop.icons[i];

                g_desktop.icons[i] = g_desktop.icons[j];
                g_desktop.icons[j] = swap;
            }
        }
    }

    desktop_refresh_icon_bindings();
}
