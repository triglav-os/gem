/*
 * Shared File Manager commands and a snapshot clipboard. Clipboard entries
 * survive navigation and closed source windows; cut entries are consumed
 * only after successful moves. Destinations are never overwritten.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (C) 2026 tomaz stih
 */
#include "desktop.h"
#include <stdio.h>
#include <string.h>

static desktop_browser_window_t clipboard;
static int clipboard_cut;

static desktop_browser_window_t *active_browser(void)
{
    return desktop_find_browser_by_handle(g_desktop.active_browser_handle);
}

static int selection_count(const desktop_browser_window_t *browser)
{
    int count = 0;
    if (browser != NULL)
        for (int i = 0; i < browser->entry_count; ++i)
            if (browser->entries[i].selected && !browser->entries[i].is_parent)
                ++count;
    return count;
}

int desktop_file_command_enabled(WORD command)
{
    desktop_browser_window_t *browser = active_browser();
    int count = selection_count(browser);
    int files = browser != NULL && browser->backend == DESKTOP_BROWSER_FILES;
    int trash = browser != NULL && browser->backend == DESKTOP_BROWSER_TRASH &&
                strcmp(browser->path, "trash://") == 0;
    switch (command) {
        case MENU_FILE_OPEN:
            return browser != NULL && browser->selected_index >= 0;
        case MENU_FILE_DELETE:
            return count > 0 && (files || trash);
        case MENU_FILE_RESTORE:
            return count > 0 && trash;
        case MENU_FILE_EMPTY_TRASH:
            return g_desktop.trash_nonempty;
        case MENU_FILE_RENAME:
            return files && count == 1;
        case MENU_FILE_CUT:
            return count > 0 && (files || trash);
        case MENU_FILE_COPY:
            return count > 0;
        case MENU_FILE_PASTE:
            return files && clipboard.entry_count > 0;
        case MENU_FILE_SELECT_ALL:
            return browser != NULL && browser->entry_count > 0;
        default:
            return command >= MENU_FILE_NEW_FOLDER &&
                   command <= MENU_FILE_NEW_CSV && files;
    }
}

static void command_error(const char *text)
{
    char alert[240];
    snprintf(alert, sizeof(alert), "[1][ %s ][ OK ]", text);
    form_alert(1, alert);
    desktop_set_status(text);
}

static int valid_name(const char *name)
{
    if (!*name || strcmp(name, ".") == 0 || strcmp(name, "..") == 0 ||
        strlen(name) > 255)
        return 0;
    for (const unsigned char *p = (const unsigned char *)name; *p; ++p)
        if (*p < 32 || *p == 127 || *p == '/' || *p == '\\')
            return 0;
    return 1;
}

static void name_command(desktop_browser_window_t *browser, WORD command)
{
    static const char *const names[] = {"New Folder", "Untitled.txt",
                                        "Untitled.md", "Untitled.html",
                                        "Untitled.csv"};
    static const char *const templates[] = {
        "", "", "# Untitled\n\n",
        "<!doctype html>\n<html lang=\"en\">\n"
        "<head><meta charset=\"utf-8\"><title>Untitled</title></head>\n"
        "<body><h1>Untitled</h1></body>\n</html>\n",
        "name,value\n"};
    char name[256], source[GEM_OS_PATH_MAX] = "", destination[GEM_OS_PATH_MAX];
    int rename = command == MENU_FILE_RENAME;
    int kind = rename ? 0 : command - MENU_FILE_NEW_FOLDER;
    int okay;
    if (rename) {
        desktop_browser_entry_t *entry = NULL;
        for (int i = 0; i < browser->entry_count; ++i) {
            if (browser->entries[i].selected &&
                !browser->entries[i].is_parent) {
                entry = &browser->entries[i];
                break;
            }
        }
        if (entry == NULL)
            return;
        snprintf(name, sizeof(name), "%.255s", entry->name);
        snprintf(source, sizeof(source), "%s", entry->path);
    } else
        snprintf(name, sizeof(name), "%s", names[kind]);
    if (!desktop_name_dialog(rename ? "Rename" : "Create", name, sizeof(name)))
        return;
    if (!valid_name(name) ||
        !desktop_path_join(browser->path, name, destination,
                           sizeof(destination))) {
        command_error(
            "Enter a single name, without slash, dot or dot-dot paths.");
        return;
    }
    if (rename && strcmp(source, destination) == 0)
        return;
    if (gem_os_path_exists(destination)) {
        command_error("That name already exists. Choose another name.");
        return;
    }
    if (rename)
        okay = gem_os_move_path(source, destination);
    else if (kind == 0)
        okay = gem_os_mkdir(destination);
    else
        okay = gem_os_create_file(destination, templates[kind],
                                  strlen(templates[kind]));
    if (!okay)
        command_error(
            "Operation failed. Check permissions and available space.");
    else {
        desktop_set_status(rename ? "Renamed." : "Created.");
        desktop_refresh_file_windows_for_path(destination);
    }
}

static void clipboard_store(desktop_browser_window_t *browser, int cut)
{
    clipboard.entry_count = 0;
    clipboard.backend = browser->backend;
    snprintf(clipboard.disk_root, sizeof(clipboard.disk_root), "%s",
             browser->disk_root);
    for (int i = 0; i < browser->entry_count; ++i) {
        desktop_browser_entry_t *entry = &browser->entries[i];
        if (entry->selected && !entry->is_parent)
            clipboard.entries[clipboard.entry_count++] = *entry;
    }
    clipboard_cut = cut;
    desktop_set_status(cut ? "Cut selection. Choose a folder and Paste."
                           : "Copied selection. Choose a folder and Paste.");
}

static void clipboard_paste(desktop_browser_window_t *browser)
{
    char directory[GEM_OS_PATH_MAX], destination[GEM_OS_PATH_MAX], error[256];
    int completed = 0, total = clipboard.entry_count;
    snprintf(directory, sizeof(directory), "%s", browser->path);
    /* Preflight the entire batch: no partial result for a known collision. */
    for (int i = 0; i < total; ++i) {
        desktop_browser_entry_t *entry = &clipboard.entries[i];
        if (!desktop_path_join(directory, entry->name, destination,
                               sizeof(destination)) ||
            gem_os_path_exists(destination)) {
            command_error("A destination name already exists or is too "
                          "long.|Nothing was pasted.");
            return;
        }
        for (int j = 0; j < i; ++j) {
            if (strcmp(entry->name, clipboard.entries[j].name) == 0) {
                command_error("The selection contains duplicate destination "
                              "names.|Paste these items separately.");
                return;
            }
        }
    }
    if (clipboard.backend == DESKTOP_BROWSER_DISK &&
        !stout_disk_open(&clipboard.disk, clipboard.disk_root, error,
                         sizeof(error))) {
        command_error("The source disk image is no longer available.");
        return;
    }
    for (int i = 0; i < total; ++i) {
        desktop_browser_entry_t *entry = &clipboard.entries[i];
        int okay;
        desktop_path_join(directory, entry->name, destination,
                          sizeof(destination));
        if (clipboard.backend == DESKTOP_BROWSER_DISK)
            okay = desktop_disk_copy_entry(&clipboard, entry, destination,
                                           directory);
        else if (entry->is_trash_root_item)
            okay = gem_trash_export(&entry->trash_item, destination,
                                    clipboard_cut) == GEM_TRASH_OK;
        else
            okay = clipboard_cut ? gem_os_move_path(entry->path, destination)
                                 : gem_os_copy_path(entry->path, destination);
        if (!okay)
            break;
        ++completed;
        if (clipboard_cut)
            desktop_refresh_file_windows_for_path(entry->path);
    }
    if (clipboard.backend == DESKTOP_BROWSER_DISK)
        stout_disk_free(&clipboard.disk);
    if (completed) {
        desktop_refresh_file_windows_for_path(destination);
        if (clipboard_cut && clipboard.backend == DESKTOP_BROWSER_TRASH)
            desktop_refresh_trash_windows();
    }
    if (clipboard_cut && completed) {
        memmove(clipboard.entries, clipboard.entries + completed,
                (size_t)(total - completed) * sizeof(clipboard.entries[0]));
        clipboard.entry_count = (WORD)(total - completed);
    }
    if (completed != total)
        command_error("Paste stopped. Unfinished items remain on the "
                      "clipboard.|Check source, permissions and destination.");
    else
        desktop_set_status(clipboard_cut ? "Selection moved."
                                         : "Selection copied.");
}

void desktop_file_command(WORD command)
{
    desktop_browser_window_t *browser = active_browser();
    if (!desktop_file_command_enabled(command))
        return;
    switch (command) {
        case MENU_FILE_OPEN:
            desktop_browser_open_entry(browser, browser->selected_index);
            break;
        case MENU_FILE_DELETE:
            desktop_delete_active_selection();
            break;
        case MENU_FILE_RESTORE:
            desktop_restore_active_selection();
            break;
        case MENU_FILE_EMPTY_TRASH:
            desktop_empty_trash();
            break;
        case MENU_FILE_RENAME:
            name_command(browser, command);
            break;
        case MENU_FILE_CUT:
            clipboard_store(browser, 1);
            break;
        case MENU_FILE_COPY:
            clipboard_store(browser, 0);
            break;
        case MENU_FILE_PASTE:
            clipboard_paste(browser);
            break;
        case MENU_FILE_SELECT_ALL:
            browser->selected_index = NIL;
            for (int i = 0; i < browser->entry_count; ++i) {
                browser->entries[i].selected = !browser->entries[i].is_parent;
                if (browser->entries[i].selected &&
                    browser->selected_index == NIL)
                    browser->selected_index = (WORD)i;
            }
            desktop_browser_redraw(browser, NULL);
            break;
        default:
            name_command(browser, command);
            break;
    }
    desktop_update_desk_menu_labels();
    desktop_redraw(NULL);
}

int desktop_file_shortcut(WORD key, WORD modifiers)
{
    int scan = (key >> 8) & 255, character = key & 255;
    WORD command = NIL;
    if (modifiers & K_CTRL) {
        /* Hosted input supplies USB scans and control-character ASCII. */
        if (character == 3 || character == 'c' || character == 'C')
            command = MENU_FILE_COPY;
        if (character == 24 || character == 'x' || character == 'X')
            command = MENU_FILE_CUT;
        if (character == 22 || character == 'v' || character == 'V')
            command = MENU_FILE_PASTE;
        if (character == 1 || character == 'a' || character == 'A')
            command = MENU_FILE_SELECT_ALL;
        if (character == 14 || character == 'n' || character == 'N')
            command = (modifiers & (K_LSHIFT | K_RSHIFT)) ? MENU_FILE_NEW_FOLDER
                                                          : MENU_FILE_NEW_TEXT;
    } else if (scan == 59 || scan == 0x3c)
        command = MENU_FILE_RENAME;
    if (command == NIL)
        return 0;
    desktop_file_command(command);
    return 1;
}
