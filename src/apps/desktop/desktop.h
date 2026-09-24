/*
 * Declares the GEM desktop sample's shared state and the helpers its
 * browser, disk, icon, repaint and shell modules exchange. Private to the
 * desktop executable.
 *
 * MIT License (see: LICENSE)
 * Copyright (C) 2026 tomaz stih
 */

#ifndef GEM_SAMPLE_DESKTOP_H
#define GEM_SAMPLE_DESKTOP_H

#include <gem.h>
#ifndef GEM_DESKTOP_HOSTED
#include "gem_protocol.h"
#endif
#include <gem/gemconf.h>

#include <gem/os.h>

#include "desktop_assets.h"
#include "gem_trash.h"
#include "stout/disk.h"

#include <stddef.h>
#include <stdint.h>

enum { DESKTOP_ROOT = 0, DESKTOP_ICON_BASE };

enum {
    MENU_ROOT = 0,
    MENU_BAR_BOX,
    MENU_TITLES,
    MENU_TITLE_DESK,
    MENU_TITLE_FILE,
    MENU_TITLE_ARRANGE,
    MENU_POPUPS,
    MENU_DESK_BOX,
    MENU_DESK_1,
    MENU_DESK_2,
    MENU_DESK_3,
    MENU_DESK_4,
    MENU_DESK_5,
    MENU_DESK_6,
    MENU_FILE_BOX,
    MENU_FILE_OPEN,
    MENU_FILE_DELETE,
    MENU_FILE_RESTORE,
    MENU_FILE_EMPTY_TRASH,
    MENU_ARRANGE_BOX,
    MENU_ARRANGE_SHOW_AS_ICONS,
    MENU_ARRANGE_SEPARATOR,
    MENU_ARRANGE_SORT_NAME,
    MENU_ARRANGE_SORT_DATE,
    MENU_ARRANGE_SORT_SIZE,
    MENU_ARRANGE_SORT_TYPE,
    MENU_OBJECT_COUNT
};

enum {
    DESKTOP_MAX_DISKS = 12,
    DESKTOP_MAX_ICONS = DESKTOP_MAX_DISKS + 2,
    DESKTOP_OBJECT_COUNT = DESKTOP_ICON_BASE + DESKTOP_MAX_ICONS,
    DESKTOP_ICON_OBJECT_W = 72,
    DESKTOP_ICON_OBJECT_H = 58,
    DESKTOP_ICON_TEXT_Y = 40,
    DESKTOP_ICON_STEP_X = 78,
    DESKTOP_ICON_STEP_Y = 66,
    DESKTOP_ICON_MARGIN_X = 16,
    DESKTOP_ICON_MARGIN_Y = 16,
    DESKTOP_BROWSER_ICON_STEP_X = 96,
    DESKTOP_BROWSER_MAX_WINDOWS = 4,
    DESKTOP_BROWSER_MAX_ENTRIES = 256,
    DESKTOP_BROWSER_ROW_H = 18,
    DESKTOP_SYSTEM_FONT = 1,
    DESKTOP_DRAG_THRESHOLD = 3,
    DESKTOP_DOUBLE_CLICK_MS = 700
};

enum { DESKTOP_BROWSER_VIEW_LIST = 0, DESKTOP_BROWSER_VIEW_ICONS = 1 };
enum { DESKTOP_BROWSER_FILES = 0, DESKTOP_BROWSER_TRASH = 1,
       DESKTOP_BROWSER_DISK = 2 };
enum {
    DESKTOP_SORT_NAME = 0,
    DESKTOP_SORT_DATE,
    DESKTOP_SORT_SIZE,
    DESKTOP_SORT_TYPE
};

typedef struct desktop_icon_draw {
    const desktop_icon_asset_t *asset;
    const char *label;
} desktop_icon_draw_t;

typedef struct desktop_icon_entry {
    char label[24];
    char path[GEM_OS_PATH_MAX];
    uint64_t total_bytes;
    uint64_t avail_bytes;
    const desktop_icon_asset_t *asset;
    USERBLK userblk;
    desktop_icon_draw_t draw_info;
    WORD object_id;
    WORD is_trash;
    WORD is_folder;
} desktop_icon_entry_t;

typedef struct desktop_browser_entry {
    char name[GEM_OS_PATH_MAX];
    char path[GEM_OS_PATH_MAX];
    uint64_t size_bytes;
    int64_t mtime;
    WORD is_dir;
    WORD is_app;
    WORD is_executable;
    WORD is_parent;
    WORD selected;
    WORD is_trash_root_item;
    gem_trash_item_t trash_item;
} desktop_browser_entry_t;

typedef struct desktop_browser_window {
    WORD used;
    WORD handle;
    WORD selected_index;
    WORD top_index;
    WORD rows_visible;
    WORD columns_visible;
    WORD rows_per_page;
    WORD view_mode;
    WORD backend;
    WORD entry_count;
    GRECT work;
    char path[GEM_OS_PATH_MAX];
    char title[96];
    char trash_payload_root[GEM_OS_PATH_MAX];
    gem_trash_item_t trash_root_item;
    stout_disk_t disk;
    WORD disk_open;
    char disk_root[GEM_OS_PATH_MAX];
    uint32_t last_click_ms;
    WORD last_click_index;
    desktop_browser_entry_t entries[DESKTOP_BROWSER_MAX_ENTRIES];
} desktop_browser_window_t;

typedef struct desktop_state {
    OBJECT desktop_tree[DESKTOP_OBJECT_COUNT];
    OBJECT menu_tree[MENU_OBJECT_COUNT];
    desktop_icon_entry_t icons[DESKTOP_MAX_ICONS];
    char desk_menu_labels[6][24];
    WORD icon_count;
    WORD selected_icon;
    WORD active_browser_handle;
    WORD icon_view;
    WORD sort_mode;
    WORD show_hidden;
    WORD trash_nonempty;
    WORD app_id;
    WORD locations_menu_id;
    WORD remove_location_menu_id;
    VDI_HANDLE vdi_handle;
    WORD char_width;
    WORD char_height;
    WORD box_width;
    WORD box_height;
    WORD screen_width;
    WORD screen_height;
    GRECT work;
    char status_text[96];
    gemconf_t config;
    uint32_t last_desktop_click_ms;
    WORD last_desktop_click_object;
    WORD drag_browser_handle;
    WORD drag_entry_index;
    WORD drag_started;
    desktop_browser_window_t browsers[DESKTOP_BROWSER_MAX_WINDOWS];
} desktop_state_t;

/* The one desktop instance, defined in main.c. */
extern desktop_state_t g_desktop;

/* Close and release a browser window. */
void desktop_browser_close(desktop_browser_window_t *browser);
/* Re-sort a populated browser after the sort mode changes. */
void desktop_browser_resort(desktop_browser_window_t *browser);
/* Handle a work-area click in the browser owning handle. */
void desktop_browser_handle_click(WORD handle, WORD mx, WORD my,
                                  WORD key_state);
/* Handle a WM_* message addressed to a browser window. */
void desktop_browser_handle_message(WORD msg[8]);
/* Route Return and arrow-key navigation to the active browser. */
void desktop_browser_handle_key(WORD key);
/* Remember a selected filesystem entry as a possible drag source. */
int desktop_browser_begin_drag(WORD handle, WORD index, WORD x, WORD y);
/* Complete a browser-to-browser drop using Trash or filesystem handling. */
void desktop_browser_drop_entry(desktop_browser_window_t *source,
                                WORD source_index,
                                desktop_browser_window_t *target,
                                WORD target_index, int move);
/* Complete or cancel a pending browser drag at the supplied point. */
void desktop_browser_end_drag(WORD x, WORD y, WORD key_state);
/* Height shared by the list header and browser status band. */
WORD desktop_browser_band_height(void);
/* Sort mode selected by a list-view column header or Arrange menu item. */
void desktop_browser_set_sort_mode(WORD mode);
/* Open the entry at index: descend, browse or launch. */
void desktop_browser_open_entry(desktop_browser_window_t *browser, WORD index);
/* Repaint a browser's work area, limited to dirty when given. */
void desktop_browser_redraw(desktop_browser_window_t *browser,
                            const GRECT *dirty);
/* Reload a browser's current filesystem directory or virtual Trash view. */
int desktop_browser_reload(desktop_browser_window_t *browser);
/* Refresh a browser's cached work area and row metrics. */
void desktop_browser_sync_work(desktop_browser_window_t *browser);
/* Paint one icon with its label into rect. */
void desktop_draw_icon_object(const desktop_icon_asset_t *asset,
                              const char *label, const GRECT *rect,
                              UWORD state);
/* Grow a damage rectangle to whole icon cells. */
void desktop_expand_icon_damage_rect(GRECT *rect);
/* Browser owning a window handle, or NULL. */
desktop_browser_window_t *desktop_find_browser_by_handle(WORD handle);
/* Icon asset registered under name, or NULL. */
const desktop_icon_entry_t *desktop_find_icon(WORD object_id);
/* Object id of the desktop icon under a point, or NIL. */
WORD desktop_find_icon_at(WORD mx, WORD my);
/* Select or open the icon object that was clicked. */
void desktop_handle_icon_click(WORD object_id);
/* Initialize one OBJECT with links, type, flags and geometry. */
void desktop_init_object(OBJECT *object, WORD next, WORD head, WORD tail,
                         UWORD type, UWORD flags, UWORD state, LONG spec,
                         WORD x, WORD y, WORD w, WORD h);
/* Append the trash icon to the desktop icon list. */
void desktop_install_trash_icon(void);
/* Place desktop icons in a grid within the work area. */
void desktop_layout_icons(void);
/* Absolute rectangle of a desktop tree object. */
void desktop_object_rect(WORD object_id, GRECT *rect);
/* Open a browser window on a disk or folder path. */
void desktop_open_disk_path(const char *path, const char *label);
/* Open the virtual Trash root in a normal file-manager window. */
void desktop_open_trash(void);
/* Open whatever the selected desktop icon refers to. */
void desktop_open_selected_icon(void);
/* Join a directory and name into out; nonzero when it fits. */
int desktop_path_join(const char *dir, const char *name, char *path,
                      size_t path_size);
/* Rebuild the configured desktop location icons. */
void desktop_probe_disks(void);
/* Publish the selected configured location to gemd. */
void desktop_publish_location_selection(void);
/* Re-read locations after a gemd-owned configuration change. */
void desktop_refresh_locations(void);
/* Open the Locations selector, persist a chosen directory and rebuild icons. */
void desktop_choose_location(void);
/* Remove the selected configured desktop location. */
void desktop_delete_selected_location(void);
/* Repaint the desktop background and icons within dirty. */
void desktop_redraw(const GRECT *dirty);
/* Repaint desktop areas uncovered by a window change. */
void desktop_redraw_window_change(const GRECT *before, const GRECT *after);
/* Point every icon object at its USERBLK draw record. */
void desktop_refresh_icon_bindings(void);
/* Change the selected icon and repaint both icons. */
void desktop_select_icon(WORD object_id);
/* Replace the status line text. */
void desktop_set_status(const char *text);
/* Sort desktop icons alphabetically, keeping trash last. */
void desktop_sort_icons_by_label(void);
/* Rebuild the Desk menu entries from the desktop icons. */
void desktop_update_desk_menu_labels(void);
/* Trash or permanently remove the active browser selection after consent. */
void desktop_delete_active_selection(void);
/* Restore the selected top-level Trash item after consent. */
void desktop_restore_active_selection(void);
/* Permanently empty all known Trash stores after consent. */
void desktop_empty_trash(void);
/* Refresh the cached availability used by the global Empty Trash command. */
void desktop_refresh_trash_availability(void);
/* Reload every filesystem window displaying the parent of path. */
void desktop_refresh_file_windows_for_path(const char *path);
/* Move a selected normal-browser entry to Trash after confirmation. */
void desktop_trash_browser_entry(desktop_browser_window_t *browser, WORD index);
/* USERDEF callback rendering a desktop icon object. */
WORD desktop_user_draw(LONG parm_block);
/* Nonzero when an entry is a launchable application. */
int desktop_browser_entry_is_launchable(const desktop_browser_window_t *browser,
                                        const desktop_browser_entry_t *entry);
/* Resolve desktop.cfg/built-in associations and launch through AES shell. */
int desktop_browser_launch_entry(const desktop_browser_window_t *browser,
                                 const desktop_browser_entry_t *entry,
                                 char *status, size_t status_size);
/* One-character type marker shown before an entry name. */
const char *
desktop_browser_entry_prefix(const desktop_browser_window_t *browser,
                             const desktop_browser_entry_t *entry);
/* Icon asset used for an entry in icon view. */
const desktop_icon_asset_t *
desktop_browser_entry_asset(const desktop_browser_window_t *browser,
                            const desktop_browser_entry_t *entry);
/* Draw the headings across the top of a browser in list view. */
void desktop_browser_list_draw_header(const desktop_browser_window_t *browser,
                                      WORD clip_y0, WORD clip_y1);
/* Draw the four visible fields for one browser list row. */
void desktop_browser_list_draw_row(const desktop_browser_window_t *browser,
                                   const desktop_browser_entry_t *entry,
                                   WORD row_y);
/* Return the sort mode belonging to a list header x coordinate. */
WORD desktop_browser_list_sort_at(const desktop_browser_window_t *browser,
                                  WORD x);
/* Draw the fixed file/folder-count status band at the browser bottom. */
void desktop_browser_draw_status(const desktop_browser_window_t *browser,
                                 WORD clip_y0, WORD clip_y1);

#endif
