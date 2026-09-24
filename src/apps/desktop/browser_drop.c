/*
 * Dispatches the shared File Manager drag completion path to Trash, mounted
 * images or normal filesystem Copy/Move handling. Destination collisions
 * remain untouched and affected open views refresh after success.
 *
 * MIT License (see: LICENSE)
 * Copyright (C) 2026 tomaz stih
 */

#include "desktop.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>

typedef struct desktop_disk_copy_context {
    const fat12_image_t *image;
    const char *source;
    const char *destination;
    size_t source_length;
    int okay;
} desktop_disk_copy_context_t;

static int desktop_disk_relative_path(const desktop_browser_window_t *browser,
                                      const char *path, const char **relative)
{
    size_t root_length;

    if (browser == NULL || path == NULL || relative == NULL) return 0;
    root_length = strlen(browser->disk_root);
    if (root_length == 0u || strncmp(path, browser->disk_root, root_length) != 0 ||
        path[root_length] != '/') return 0;
    *relative = path + root_length + 1u;
    return **relative != '\0';
}

static int desktop_disk_copy_walk(const fat12_dirent_t *entry,
                                  const char *path, void *opaque)
{
    desktop_disk_copy_context_t *context = opaque;
    const char *tail;
    char destination[GEM_OS_PATH_MAX];
    uint8_t *data = NULL;
    size_t size = 0u;
    char error_text[FAT12_ERROR_LENGTH];

    if (context->okay == 0 || entry == NULL || path == NULL) return 0;
    if (strncasecmp(path, context->source, context->source_length) != 0) {
        return 1;
    }
    if (path[context->source_length] == '\0') return 1;
    if (path[context->source_length] != '/') return 1;
    tail = path + context->source_length + 1u;
    if (desktop_path_join(context->destination, tail, destination,
                          sizeof(destination)) == 0) {
        context->okay = 0;
        return 0;
    }
    if ((entry->attr & FAT12_ATTR_VOLUME) != 0u) return 1;
    if ((entry->attr & FAT12_ATTR_DIRECTORY) != 0u) {
        context->okay = gem_os_mkdir_p(destination);
        return context->okay;
    }
    if (fat12_read_file_alloc(context->image, entry, &data, &size, error_text,
                              sizeof(error_text)) == 0) {
        context->okay = 0;
        return 0;
    }
    context->okay = gem_os_write_file_atomic(destination, data, size);
    free(data);
    return context->okay;
}

static int desktop_disk_copy_entry(desktop_browser_window_t *source,
                                   const desktop_browser_entry_t *entry,
                                   const char *destination,
                                   const char *destination_parent)
{
    const fat12_image_t *image;
    const char *relative;

    if (desktop_disk_relative_path(source, entry->path, &relative) == 0) {
        return 0;
    }
    if (entry->is_dir == 0) {
        uint8_t *data = NULL;
        size_t size = 0u;
        int okay = stout_disk_read_path(&source->disk, relative, &data, &size) &&
                   gem_os_write_file_atomic(destination, data, size);

        free(data);
        if (okay == 0) (void)gem_os_unlink(destination);
        return okay;
    }

    image = stout_disk_fat12(&source->disk);
    if (image != NULL && gem_os_mkdir(destination) != 0) {
        desktop_disk_copy_context_t context;
        char error_text[FAT12_ERROR_LENGTH];
        int walked;

        context.image = image;
        context.source = relative;
        context.destination = destination;
        context.source_length = strlen(relative);
        context.okay = 1;
        walked = fat12_walk_root(image, desktop_disk_copy_walk, &context,
                                 error_text, sizeof(error_text));
        if (walked != 0 && context.okay != 0) return 1;
    }
    (void)gem_os_remove_tree_under(destination, destination_parent);
    return 0;
}

void desktop_browser_drop_entry(desktop_browser_window_t *source,
                                WORD source_index,
                                desktop_browser_window_t *target,
                                WORD target_index, int move)
{
    const desktop_browser_entry_t *entry;
    char source_path[GEM_OS_PATH_MAX];
    char first_source[GEM_OS_PATH_MAX];
    char destination_directory[GEM_OS_PATH_MAX];
    char destination[GEM_OS_PATH_MAX];
    char last_destination[GEM_OS_PATH_MAX];
    char name[GEM_OS_PATH_MAX];
    char alert[GEM_OS_PATH_MAX + 160];
    WORD index;
    WORD item_count = 0;
    WORD completed = 0;
    int okay;

    if (source == NULL || target == NULL || source_index < 0 ||
        source_index >= source->entry_count ||
        source->entries[source_index].is_parent != 0) return;
    if (source->backend == DESKTOP_BROWSER_DISK &&
        target->backend == DESKTOP_BROWSER_TRASH) {
        desktop_set_status("Cannot move files from a mounted image to Trash.");
        desktop_redraw(NULL);
        return;
    }
    if (target->backend == DESKTOP_BROWSER_TRASH) {
        desktop_trash_browser_entry(source, source_index);
        return;
    }
    if (target->backend == DESKTOP_BROWSER_DISK) {
        desktop_set_status("Cannot drop files into a mounted image.");
        desktop_redraw(NULL);
        return;
    }
    if ((source->backend != DESKTOP_BROWSER_FILES &&
         source->backend != DESKTOP_BROWSER_DISK) ||
        target->backend != DESKTOP_BROWSER_FILES) return;
    if (source->backend == DESKTOP_BROWSER_DISK && move != 0) {
        desktop_set_status("Mounted-image files can be copied but not moved.");
        desktop_redraw(NULL);
        return;
    }
    if (target_index >= 0 && target_index < target->entry_count &&
        target->entries[target_index].is_dir != 0) {
        if (target == source && target->entries[target_index].selected != 0) {
            desktop_set_status("Cannot drop a selection into itself.");
            desktop_redraw(NULL);
            return;
        }
        (void)snprintf(destination_directory,
                       sizeof(destination_directory), "%s",
                       target->entries[target_index].path);
    } else {
        (void)snprintf(destination_directory,
                       sizeof(destination_directory), "%s", target->path);
    }

    /* Validate the complete selection before changing either directory. */
    first_source[0] = '\0';
    last_destination[0] = '\0';
    for (index = 0; index < source->entry_count; ++index) {
        entry = &source->entries[index];
        if (entry->selected == 0 || entry->is_parent != 0) continue;
        (void)snprintf(source_path, sizeof(source_path), "%s", entry->path);
        (void)snprintf(name, sizeof(name), "%s", entry->name);
        if (desktop_path_join(destination_directory, name, destination,
                              sizeof(destination)) == 0) {
            desktop_set_status("Destination path is too long.");
            desktop_redraw(NULL);
            return;
        }
        if (strcmp(source_path, destination) == 0) {
            desktop_set_status("Source and destination are the same.");
            desktop_redraw(NULL);
            return;
        }
        if (gem_os_path_exists(destination) != 0) {
            (void)snprintf(
                alert, sizeof(alert),
                "[1][ %.*s already exists in|the destination. ][ OK ]",
                360, name);
            (void)form_alert(1, alert);
            desktop_set_status("Destination already contains that name.");
            desktop_redraw(NULL);
            return;
        }
        if (item_count == 0) {
            (void)snprintf(first_source, sizeof(first_source), "%s",
                           source_path);
        }
        (void)snprintf(last_destination, sizeof(last_destination), "%s",
                       destination);
        ++item_count;
    }
    if (item_count == 0) return;

    for (index = 0; index < source->entry_count; ++index) {
        entry = &source->entries[index];
        if (entry->selected == 0 || entry->is_parent != 0) continue;
        (void)snprintf(source_path, sizeof(source_path), "%s", entry->path);
        (void)snprintf(name, sizeof(name), "%s", entry->name);
        if (desktop_path_join(destination_directory, name, destination,
                              sizeof(destination)) == 0) break;
        if (source->backend == DESKTOP_BROWSER_DISK) {
            okay = desktop_disk_copy_entry(source, entry, destination,
                                           destination_directory);
        } else {
            okay = move != 0 ? gem_os_move_path(source_path, destination)
                             : gem_os_copy_path(source_path, destination);
        }
        if (okay == 0) break;
        ++completed;
    }
    if (last_destination[0] != '\0' && completed > 0) {
        desktop_refresh_file_windows_for_path(last_destination);
    }
    if (move != 0 && first_source[0] != '\0' && completed > 0) {
        desktop_refresh_file_windows_for_path(first_source);
    }
    if (completed != item_count) {
        (void)snprintf(alert, sizeof(alert),
                       "[3][ Unable to %s %.*s. ][ OK ]",
                       move != 0 ? "move" : "copy", 360, name);
        (void)form_alert(1, alert);
        desktop_set_status(move != 0 ? "Move failed." : "Copy failed.");
        desktop_redraw(NULL);
        return;
    }
    if (item_count == 1) {
        (void)snprintf(alert, sizeof(alert), "%s %.*s.",
                       move != 0 ? "Moved" : "Copied", 450, name);
    } else {
        (void)snprintf(alert, sizeof(alert), "%s %d items.",
                       move != 0 ? "Moved" : "Copied", item_count);
    }
    desktop_set_status(alert);
    desktop_redraw(NULL);
}
