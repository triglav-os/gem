/*
 * Connects File Manager selections and confirmation alerts to the GEM Trash
 * backend, then refreshes affected browser windows after each operation.
 *
 * MIT License (see: LICENSE)
 * Copyright (C) 2026 tomaz stih
 */

#include "desktop.h"

#include <stdio.h>
#include <string.h>

static desktop_browser_window_t *desktop_active_browser(void)
{
    return desktop_find_browser_by_handle(g_desktop.active_browser_handle);
}

static desktop_browser_entry_t *desktop_selected_entry(
    desktop_browser_window_t *browser)
{
    if (browser == NULL || browser->selected_index < 0 ||
        browser->selected_index >= browser->entry_count) {
        return NULL;
    }
    return &browser->entries[browser->selected_index];
}

static void desktop_refresh_browser(desktop_browser_window_t *browser)
{
    if (browser != NULL && desktop_browser_reload(browser) != 0) {
        wind_set_str(browser->handle, WF_NAME, browser->title);
        desktop_browser_sync_work(browser);
        desktop_browser_redraw(browser, NULL);
    }
    desktop_update_desk_menu_labels();
    desktop_redraw(NULL);
}

void desktop_refresh_trash_availability(void)
{
    size_t item_count = 0u;

    if (gem_trash_list(NULL, NULL, &item_count) == GEM_TRASH_OK) {
        g_desktop.trash_nonempty = item_count > 0u;
    }
}

static void desktop_refresh_trash_windows(void)
{
    WORD index;

    desktop_refresh_trash_availability();
    for (index = 0; index < DESKTOP_BROWSER_MAX_WINDOWS; ++index) {
        desktop_browser_window_t *browser = &g_desktop.browsers[index];

        if (browser->used == 0 ||
            browser->backend != DESKTOP_BROWSER_TRASH) {
            continue;
        }
        (void)snprintf(browser->path, sizeof(browser->path), "trash://");
        desktop_refresh_browser(browser);
    }
}

static int desktop_parent_directory(const char *path, char *parent,
                                    size_t parent_size)
{
    char *slash;

    if (path == NULL || path[0] != '/' ||
        snprintf(parent, parent_size, "%s", path) <= 0) {
        return 0;
    }
    slash = strrchr(parent, '/');
    if (slash == NULL) {
        return 0;
    }
    if (slash == parent) {
        parent[1] = '\0';
    } else {
        *slash = '\0';
    }
    return 1;
}

/* Notify every in-process File Manager displaying the changed directory. */
void desktop_refresh_file_windows_for_path(const char *path)
{
    char parent[GEM_OS_PATH_MAX];
    WORD index;

    if (desktop_parent_directory(path, parent, sizeof(parent)) == 0) {
        return;
    }
    for (index = 0; index < DESKTOP_BROWSER_MAX_WINDOWS; ++index) {
        desktop_browser_window_t *browser = &g_desktop.browsers[index];

        if (browser->used == 0 ||
            browser->backend != DESKTOP_BROWSER_FILES ||
            strcmp(browser->path, parent) != 0) {
            continue;
        }
        desktop_refresh_browser(browser);
    }
}

static void desktop_trash_filesystem_selection(
    desktop_browser_window_t *browser, WORD primary_index)
{
    char alert[GEM_OS_PATH_MAX + 64];
    char changed_path[GEM_OS_PATH_MAX];
    WORD index;
    WORD item_count = 0;
    WORD moved = 0;
    desktop_browser_entry_t *primary = &browser->entries[primary_index];

    changed_path[0] = '\0';
    for (index = 0; index < browser->entry_count; ++index) {
        if (browser->entries[index].selected != 0 &&
            browser->entries[index].is_parent == 0) ++item_count;
    }
    if (item_count == 0) return;

    if (item_count == 1) {
        (void)snprintf(alert, sizeof(alert),
                       "[2][ Move %.*s to Trash? ][ Trash | Cancel ]", 360,
                       primary->name);
    } else {
        (void)snprintf(
            alert, sizeof(alert),
            "[2][ Move %d selected items|to Trash? ][ Trash | Cancel ]",
            item_count);
    }
    if (form_alert(2, alert) != 1) {
        desktop_set_status("Move to Trash cancelled.");
        desktop_redraw(NULL);
        return;
    }
    for (index = 0; index < browser->entry_count; ++index) {
        desktop_browser_entry_t *entry = &browser->entries[index];

        if (entry->selected == 0 || entry->is_parent != 0) continue;
        if (changed_path[0] == '\0') {
            (void)snprintf(changed_path, sizeof(changed_path), "%s",
                           entry->path);
        }
        if (gem_trash_put(entry->path, NULL) != GEM_TRASH_OK) break;
        ++moved;
    }
    if (moved != item_count) {
        (void)snprintf(alert, sizeof(alert),
                       "[3][ Cannot move %.*s to Trash. ][ OK ]", 360,
                       browser->entries[index].name);
        (void)form_alert(1, alert);
        desktop_set_status("Unable to move item to Trash.");
    } else if (item_count == 1) {
        (void)snprintf(alert, sizeof(alert), "Moved %.*s to Trash.", 450,
                       primary->name);
        desktop_set_status(alert);
    } else {
        (void)snprintf(alert, sizeof(alert), "Moved %d items to Trash.",
                       item_count);
        desktop_set_status(alert);
    }
    if (moved > 0) {
        desktop_refresh_file_windows_for_path(changed_path);
        desktop_refresh_trash_windows();
    }
    desktop_redraw(NULL);
}

void desktop_trash_browser_entry(desktop_browser_window_t *browser, WORD index)
{
    if (browser == NULL || browser->backend != DESKTOP_BROWSER_FILES ||
        index < 0 || index >= browser->entry_count ||
        browser->entries[index].is_parent != 0) {
        return;
    }
    desktop_trash_filesystem_selection(browser, index);
}

static void desktop_purge_trash_entry(desktop_browser_entry_t *entry)
{
    char alert[GEM_OS_PATH_MAX + 96];

    (void)snprintf(
        alert, sizeof(alert),
        "[3][ Permanently delete %.*s?|This cannot be undone. ][ Delete | Cancel ]",
        350, entry->name);
    if (form_alert(2, alert) != 1) {
        desktop_set_status("Permanent delete cancelled.");
        desktop_redraw(NULL);
        return;
    }
    if (gem_trash_purge(&entry->trash_item) != GEM_TRASH_OK) {
        desktop_set_status("Unable to permanently delete item.");
    } else {
        (void)snprintf(alert, sizeof(alert), "Permanently deleted %.*s.", 450,
                       entry->name);
        desktop_set_status(alert);
    }
    desktop_refresh_trash_windows();
}

void desktop_delete_active_selection(void)
{
    desktop_browser_window_t *browser = desktop_active_browser();
    desktop_browser_entry_t *entry = desktop_selected_entry(browser);

    if (entry == NULL || entry->is_parent != 0) {
        desktop_set_status("Select an item first.");
        desktop_redraw(NULL);
        return;
    }
    if (browser->backend == DESKTOP_BROWSER_FILES) {
        desktop_trash_filesystem_selection(browser, browser->selected_index);
    } else if (strcmp(browser->path, "trash://") == 0 &&
               entry->is_trash_root_item != 0) {
        desktop_purge_trash_entry(entry);
    } else {
        desktop_set_status("Return to Trash root to delete this item.");
        desktop_redraw(NULL);
    }
}

void desktop_restore_active_selection(void)
{
    desktop_browser_window_t *browser = desktop_active_browser();
    desktop_browser_entry_t *entry = desktop_selected_entry(browser);
    char restored_path[GEM_OS_PATH_MAX];
    int result;

    if (browser == NULL || browser->backend != DESKTOP_BROWSER_TRASH ||
        strcmp(browser->path, "trash://") != 0 || entry == NULL ||
        entry->is_trash_root_item == 0) {
        desktop_set_status("Select an item at the Trash root first.");
        desktop_redraw(NULL);
        return;
    }
    (void)snprintf(restored_path, sizeof(restored_path), "%s",
                   entry->trash_item.original_path);
    result = gem_trash_restore(&entry->trash_item);
    if (result == GEM_TRASH_EXISTS) {
        char alert[GEM_OS_PATH_MAX + 80];
        WORD choice;

        (void)snprintf(alert, sizeof(alert),
                       "[2][ %.*s already exists. ][ Skip | Rename | Cancel ]",
                       360, entry->name);
        choice = form_alert(3, alert);
        if (choice == 2) {
            result = gem_trash_restore_renamed(&entry->trash_item);
        } else {
            desktop_set_status(choice == 1 ? "Restore skipped."
                                           : "Restore cancelled.");
        }
        if (choice != 2) {
            desktop_redraw(NULL);
            return;
        }
        if (result == GEM_TRASH_OK) {
            desktop_set_status("Item restored under a new name.");
        } else {
            desktop_set_status("Unable to restore Trash item.");
        }
    } else if (result != GEM_TRASH_OK) {
        desktop_set_status("Unable to restore Trash item.");
    } else {
        desktop_set_status("Item restored to its original location.");
    }
    if (result == GEM_TRASH_OK) {
        desktop_refresh_file_windows_for_path(restored_path);
    }
    desktop_refresh_trash_windows();
}

void desktop_empty_trash(void)
{
    size_t count = 0u;
    char status[96];

    if (form_alert(2,
                   "[3][ Permanently empty Trash?|This cannot be undone. ]"
                   "[ Empty | Cancel ]") != 1) {
        desktop_set_status("Empty Trash cancelled.");
        desktop_redraw(NULL);
        return;
    }
    if (gem_trash_empty(&count) != GEM_TRASH_OK) {
        desktop_set_status("Unable to empty Trash completely.");
    } else {
        (void)snprintf(status, sizeof(status), "Trash emptied: %lu item%s.",
                       (unsigned long)count, count == 1u ? "" : "s");
        desktop_set_status(status);
    }
    desktop_refresh_trash_windows();
}
