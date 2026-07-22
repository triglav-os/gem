/*
 * Implements a fresh portable GEM desktop that relies only on the
 * hosted AES, VDI, and operating-system wrappers. The desktop keeps the
 * original menu structure, draws Atari ST disk and trash icons from
 * monochrome masked resources, and avoids any hosted window manager
 * dependencies by rendering directly into the desktop work area.
 *
 * MIT License (see: LICENSE)
 * Copyright (C) 2026 tomaz stih
 */

#define _POSIX_C_SOURCE 200809L

#include <gem.h>

#include "platform/os.h"

#include "desktop_assets.h"

#include <dirent.h>
#include <mntent.h>
#include <stddef.h>
#include <errno.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <strings.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

enum {
    DESKTOP_ROOT = 0,
    DESKTOP_ICON_BASE
};

enum {
    MENU_ROOT = 0,
    MENU_BAR_BOX,
    MENU_TITLES,
    MENU_TITLE_DESK,
    MENU_TITLE_FILE,
    MENU_TITLE_OPTIONS,
    MENU_TITLE_ARRANGE,
    MENU_POPUPS,
    MENU_DESK_BOX,
    MENU_DESK_ABOUT,
    MENU_DESK_SEPARATOR,
    MENU_DESK_1,
    MENU_DESK_2,
    MENU_DESK_3,
    MENU_DESK_4,
    MENU_DESK_5,
    MENU_DESK_6,
    MENU_FILE_BOX,
    MENU_FILE_OPEN,
    MENU_FILE_INFO,
    MENU_FILE_SEPARATOR_1,
    MENU_FILE_DELETE,
    MENU_FILE_FORMAT,
    MENU_FILE_SEPARATOR_2,
    MENU_FILE_OUTPUT,
    MENU_FILE_EXIT,
    MENU_OPTIONS_BOX,
    MENU_OPTIONS_INSTALL_DISK,
    MENU_OPTIONS_CONFIGURE_APP,
    MENU_OPTIONS_SEPARATOR,
    MENU_OPTIONS_PREFS,
    MENU_OPTIONS_SAVE,
    MENU_OPTIONS_DOS,
    MENU_ARRANGE_BOX,
    MENU_ARRANGE_SHOW_AS_ICONS,
    MENU_ARRANGE_SEPARATOR,
    MENU_ARRANGE_SORT_NAME,
    MENU_ARRANGE_SORT_TYPE,
    MENU_ARRANGE_SORT_SIZE,
    MENU_ARRANGE_SORT_DATE,
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
    DESKTOP_BROWSER_MAX_WINDOWS = 4,
    DESKTOP_BROWSER_MAX_ENTRIES = 256,
    DESKTOP_BROWSER_ROW_H = 18,
    DESKTOP_DOUBLE_CLICK_MS = 700
};

enum {
    DESKTOP_BROWSER_VIEW_LIST = 0,
    DESKTOP_BROWSER_VIEW_ICONS = 1
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
    mode_t mode;
    WORD is_dir;
    WORD is_app;
    WORD is_executable;
    WORD is_parent;
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
    WORD entry_count;
    GRECT work;
    char path[GEM_OS_PATH_MAX];
    char title[96];
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
    WORD app_id;
    VDI_HANDLE vdi_handle;
    WORD screen_width;
    WORD screen_height;
    GRECT work;
    char status_text[96];
    uint32_t last_desktop_click_ms;
    WORD last_desktop_click_object;
    desktop_browser_window_t browsers[DESKTOP_BROWSER_MAX_WINDOWS];
} desktop_state_t;

static desktop_state_t g_desktop;

static WORD desktop_user_draw(LONG parm_block);
static void desktop_draw_icon_object(const desktop_icon_asset_t *asset,
    const char *label, const GRECT *rect, UWORD state);
static void desktop_set_status(const char *text);
static void desktop_redraw(const GRECT *dirty);
static void desktop_layout_icons(void);
static void desktop_handle_menu_item(WORD item);
static void desktop_handle_icon_click(WORD object_id);
static void desktop_refresh_icon_bindings(void);
static void desktop_draw_background_pattern(const GRECT *dirty);
static void desktop_object_rect(WORD object_id, GRECT *rect);
static void desktop_expand_icon_damage_rect(GRECT *rect);
static void desktop_open_selected_icon(void);
static void desktop_open_disk_path(const char *path, const char *label);
static desktop_browser_window_t *desktop_find_browser_by_handle(WORD handle);
static void desktop_browser_handle_message(WORD msg[8]);
static void desktop_browser_handle_click(WORD handle, WORD mx, WORD my);
static void desktop_redraw_window_change(const GRECT *before,
    const GRECT *after);
static int desktop_browser_row_rect(const desktop_browser_window_t *browser,
    WORD index, GRECT *rect);
static int desktop_browser_icon_rect(const desktop_browser_window_t *browser,
    WORD index, GRECT *rect);
static WORD desktop_find_icon_at(WORD mx, WORD my);
static desktop_browser_window_t *desktop_browser_for_desk_slot(WORD slot);
static int desktop_launch_hosted_app(const char *path, const char *name);
static void desktop_update_desk_menu_labels(void);

static void desktop_init_object(OBJECT *object,
                                WORD next,
                                WORD head,
                                WORD tail,
                                UWORD type,
                                UWORD flags,
                                UWORD state,
                                LONG spec,
                                WORD x,
                                WORD y,
                                WORD w,
                                WORD h)
{
    object->ob_next = next;
    object->ob_head = head;
    object->ob_tail = tail;
    object->ob_type = type;
    object->ob_flags = flags;
    object->ob_state = state;
    object->ob_spec = spec;
    object->ob_x = x;
    object->ob_y = y;
    object->ob_width = w;
    object->ob_height = h;
}

static size_t desktop_word_width(const desktop_icon_asset_t *asset)
{
    return (size_t) ((asset->width + 15) / 16);
}

static int desktop_icon_bit(const UWORD *bits,
                            const desktop_icon_asset_t *asset,
                            WORD x,
                            WORD y)
{
    size_t row_words;
    size_t index;
    UWORD word;

    if (bits == NULL || asset == NULL || x < 0 || y < 0 ||
        x >= asset->width || y >= asset->height) {
        return 0;
    }

    row_words = desktop_word_width(asset);
    index = (size_t) y * row_words + (size_t) x / 16u;
    word = bits[index];
    return (word & (WORD) (0x8000u >> (x % 16))) != 0;
}

static void desktop_draw_run(WORD x0, WORD y, WORD x1, WORD color)
{
    WORD rect[4];

    rect[0] = x0;
    rect[1] = y;
    rect[2] = x1;
    rect[3] = y;
    vsf_color(g_desktop.vdi_handle, color);
    v_bar(g_desktop.vdi_handle, rect);
}

static void desktop_draw_masked_icon(const desktop_icon_asset_t *asset,
                                     WORD x,
                                     WORD y)
{
    WORD row;

    if (asset == NULL) {
        return;
    }

    for (row = 0; row < asset->height; ++row) {
        WORD col = 0;

        while (col < asset->width) {
            WORD start = col;

            while (col < asset->width &&
                   desktop_icon_bit(asset->mask_bits, asset, col, row) == 0) {
                ++col;
            }
            if (col >= asset->width) {
                break;
            }
            start = col;
            while (col < asset->width &&
                   desktop_icon_bit(asset->mask_bits, asset, col, row) != 0) {
                ++col;
            }
            desktop_draw_run((WORD) (x + start), (WORD) (y + row),
                (WORD) (x + col - 1), BLACK);
        }
    }

    for (row = 0; row < asset->height; ++row) {
        WORD col = 0;

        while (col < asset->width) {
            WORD start;

            while (col < asset->width &&
                   desktop_icon_bit(asset->data_bits, asset, col, row) == 0) {
                ++col;
            }
            if (col >= asset->width) {
                break;
            }
            start = col;
            while (col < asset->width &&
                   desktop_icon_bit(asset->data_bits, asset, col, row) != 0) {
                ++col;
            }
            desktop_draw_run((WORD) (x + start), (WORD) (y + row),
                (WORD) (x + col - 1), WHITE);
        }
    }
}

static const desktop_icon_entry_t *desktop_find_icon(WORD object_id)
{
    WORD i;

    for (i = 0; i < g_desktop.icon_count; ++i) {
        if (g_desktop.icons[i].object_id == object_id) {
            return &g_desktop.icons[i];
        }
    }
    return NULL;
}

static int desktop_path_join(const char *dir,
                             const char *name,
                             char *path,
                             size_t path_size)
{
    int needed;

    if (dir == NULL || name == NULL || path == NULL || path_size == 0u) {
        return 0;
    }

    if (strcmp(dir, "/") == 0) {
        needed = snprintf(path, path_size, "/%s", name);
    } else {
        needed = snprintf(path, path_size, "%s/%s", dir, name);
    }

    return needed > 0 && (size_t) needed < path_size;
}

static void desktop_browser_update_title(desktop_browser_window_t *browser)
{
    const char *base;
    int max_label;

    if (browser == NULL) {
        return;
    }

    if (strcmp(browser->path, "/") == 0) {
        strncpy(browser->title, "File Manager - /",
            sizeof(browser->title) - 1u);
        browser->title[sizeof(browser->title) - 1u] = '\0';
        return;
    }

    base = strrchr(browser->path, '/');
    if (base != NULL && base[1] != '\0') {
        ++base;
    } else {
        base = browser->path;
    }

    max_label = (int) sizeof(browser->title) - 16;
    (void) snprintf(browser->title, sizeof(browser->title),
        "File Manager - %.*s", max_label, base);
}

static int desktop_browser_entry_cmp(const void *left, const void *right)
{
    const desktop_browser_entry_t *a = (const desktop_browser_entry_t *) left;
    const desktop_browser_entry_t *b = (const desktop_browser_entry_t *) right;

    if (a->is_parent != b->is_parent) {
        return (a->is_parent != 0) ? -1 : 1;
    }
    if (a->is_dir != b->is_dir) {
        return (a->is_dir != 0) ? -1 : 1;
    }
    return strcmp(a->name, b->name);
}

static int desktop_browser_name_is_app(const char *name, mode_t mode)
{
    const char *ext;
    int executable;

    if (name == NULL || S_ISREG(mode) == 0) {
        return 0;
    }

    ext = strrchr(name, '.');
    if (ext != NULL &&
        (strcasecmp(ext, ".app") == 0 || strcasecmp(ext, ".prg") == 0)) {
        return 1;
    }
    if (ext != NULL &&
        (strcasecmp(ext, ".so") == 0 || strcasecmp(ext, ".a") == 0 ||
         strcasecmp(ext, ".o") == 0)) {
        return 0;
    }
    if (ext != NULL && strcasecmp(ext, "_hosted") == 0) {
        return 0;
    }
    if (strstr(name, "_hosted") != NULL) {
        return 0;
    }

    executable = ((mode & S_IXUSR) != 0) || ((mode & S_IXGRP) != 0) ||
        ((mode & S_IXOTH) != 0);
    if (executable == 0) {
        return 0;
    }
    if (strcmp(name, "gemd") == 0) {
        return 0;
    }
    if (strncmp(name, "lib", 3) == 0 && ext == NULL) {
        return 0;
    }

    return 1;
}

static int desktop_browser_entry_path(const desktop_browser_window_t *browser,
    const desktop_browser_entry_t *entry, char *path, size_t path_size)
{
    if (browser == NULL || entry == NULL || path == NULL || path_size == 0u) {
        return 0;
    }
    if (entry->is_parent != 0) {
        strncpy(path, browser->path, path_size - 1u);
        path[path_size - 1u] = '\0';
        return 1;
    }

    return desktop_path_join(browser->path, entry->name, path, path_size);
}

static int desktop_browser_entry_is_launchable(
    const desktop_browser_window_t *browser,
    const desktop_browser_entry_t *entry)
{
    if (browser == NULL || entry == NULL || entry->is_dir != 0 ||
        entry->is_parent != 0) {
        return 0;
    }
    return entry->is_app != 0;
}

static int desktop_browser_entry_has_prg_icon(
    const desktop_browser_window_t *browser,
    const desktop_browser_entry_t *entry)
{
    if (browser == NULL || entry == NULL || entry->is_dir != 0 ||
        entry->is_parent != 0) {
        return 0;
    }
    return entry->is_executable != 0;
}

static int desktop_launch_hosted_app(const char *path, const char *name)
{
    pid_t pid;
    char status[128];

    if (path == NULL || path[0] == '\0') {
        return 0;
    }

    pid = fork();
    if (pid < 0) {
        (void) snprintf(status, sizeof(status), "Launch failed: %s",
            strerror(errno));
        desktop_set_status(status);
        desktop_redraw(NULL);
        return 0;
    }

    if (pid == 0) {
        char *const argv[] = {(char *) path, NULL};

        (void) setsid();
        execv(path, argv);
        _exit(127);
    }

    (void) snprintf(status, sizeof(status), "Launched %.*s",
        (int) sizeof(status) - 10, (name != NULL) ? name : path);
    desktop_set_status(status);
    desktop_redraw(NULL);
    return 1;
}

static int desktop_browser_load_path(desktop_browser_window_t *browser,
                                     const char *path)
{
    DIR *dir;
    struct dirent *entry;

    if (browser == NULL || path == NULL) {
        return 0;
    }

    dir = opendir(path);
    if (dir == NULL) {
        return 0;
    }

    browser->entry_count = 0;
    browser->selected_index = NIL;
    browser->top_index = 0;
    browser->last_click_ms = 0u;
    browser->last_click_index = NIL;
    strncpy(browser->path, path, sizeof(browser->path) - 1u);
    browser->path[sizeof(browser->path) - 1u] = '\0';

    if (strcmp(path, "/") != 0 &&
        browser->entry_count < DESKTOP_BROWSER_MAX_ENTRIES) {
        desktop_browser_entry_t *parent = &browser->entries[browser->entry_count++];

        memset(parent, 0, sizeof(*parent));
        strncpy(parent->name, "..", sizeof(parent->name) - 1u);
        parent->name[sizeof(parent->name) - 1u] = '\0';
        strncpy(parent->path, path, sizeof(parent->path) - 1u);
        parent->path[sizeof(parent->path) - 1u] = '\0';
        parent->is_dir = 1;
        parent->is_parent = 1;
    }

    while ((entry = readdir(dir)) != NULL &&
           browser->entry_count < DESKTOP_BROWSER_MAX_ENTRIES) {
        desktop_browser_entry_t *item;
        struct stat st;

        if (strcmp(entry->d_name, ".") == 0 ||
            strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        item = &browser->entries[browser->entry_count];
        memset(item, 0, sizeof(*item));
        strncpy(item->name, entry->d_name, sizeof(item->name) - 1u);
        item->name[sizeof(item->name) - 1u] = '\0';
        if (desktop_path_join(path, entry->d_name, item->path,
                sizeof(item->path)) == 0) {
            continue;
        }
        if (stat(item->path, &st) == 0) {
            item->mode = st.st_mode;
            item->is_dir = S_ISDIR(st.st_mode) ? 1 : 0;
            item->is_executable =
                ((st.st_mode & S_IXUSR) != 0 || (st.st_mode & S_IXGRP) != 0 ||
                 (st.st_mode & S_IXOTH) != 0) ? 1 : 0;
            item->is_app = desktop_browser_name_is_app(item->name,
                st.st_mode) ? 1 : 0;
            item->size_bytes = (uint64_t) st.st_size;
        }
        ++browser->entry_count;
    }

    closedir(dir);
    qsort(browser->entries, (size_t) browser->entry_count,
        sizeof(browser->entries[0]), desktop_browser_entry_cmp);
    desktop_browser_update_title(browser);
    return 1;
}

static WORD desktop_browser_max_top(const desktop_browser_window_t *browser)
{
    WORD page_size;

    if (browser == NULL) {
        return 0;
    }

    page_size = browser->rows_visible;
    if (browser->view_mode == DESKTOP_BROWSER_VIEW_ICONS) {
        page_size = browser->rows_per_page;
    }
    if (page_size <= 0 || browser->entry_count <= page_size) {
        return 0;
    }

    return (WORD) (browser->entry_count - page_size);
}

static int desktop_browser_row_rect(const desktop_browser_window_t *browser,
                                    WORD index,
                                    GRECT *rect)
{
    WORD visible_row;

    if (browser == NULL || rect == NULL || browser->used == 0 ||
        index < 0 || index >= browser->entry_count) {
        return 0;
    }

    visible_row = (WORD) (index - browser->top_index);
    if (visible_row < 0 || visible_row >= browser->rows_visible) {
        return 0;
    }

    rect->g_x = browser->work.g_x;
    rect->g_y = (WORD) (browser->work.g_y + visible_row *
        DESKTOP_BROWSER_ROW_H);
    rect->g_w = browser->work.g_w;
    rect->g_h = DESKTOP_BROWSER_ROW_H;
    return 1;
}

static int desktop_browser_icon_rect(const desktop_browser_window_t *browser,
                                     WORD index,
                                     GRECT *rect)
{
    WORD visible_index;
    WORD row;
    WORD column;

    if (browser == NULL || rect == NULL || browser->used == 0 ||
        index < 0 || index >= browser->entry_count ||
        browser->columns_visible <= 0 || browser->rows_per_page <= 0) {
        return 0;
    }

    visible_index = (WORD) (index - browser->top_index);
    if (visible_index < 0 || visible_index >= browser->rows_per_page) {
        return 0;
    }

    column = (WORD) (visible_index % browser->columns_visible);
    row = (WORD) (visible_index / browser->columns_visible);
    rect->g_x = (WORD) (browser->work.g_x + DESKTOP_ICON_MARGIN_X +
        column * DESKTOP_ICON_STEP_X);
    rect->g_y = (WORD) (browser->work.g_y + DESKTOP_ICON_MARGIN_Y +
        row * DESKTOP_ICON_STEP_Y);
    rect->g_w = DESKTOP_ICON_OBJECT_W;
    rect->g_h = DESKTOP_ICON_OBJECT_H;
    return 1;
}

static void desktop_browser_sync_sliders(desktop_browser_window_t *browser)
{
    WORD max_top;
    WORD size;
    WORD slider;
    WORD page_size;

    if (browser == NULL || browser->used == 0) {
        return;
    }

    max_top = desktop_browser_max_top(browser);
    if (browser->top_index > max_top) {
        browser->top_index = max_top;
    }

    page_size = browser->rows_visible;
    if (browser->view_mode == DESKTOP_BROWSER_VIEW_ICONS) {
        page_size = browser->rows_per_page;
    }

    if (browser->entry_count <= 0 || page_size >= browser->entry_count) {
        size = 1000;
        slider = 0;
    } else {
        size = (WORD) ((page_size * 1000) / browser->entry_count);
        if (size < 50) {
            size = 50;
        }
        slider = (max_top > 0) ?
            (WORD) ((browser->top_index * 1000) / max_top) : 0;
    }

    (void) wind_set(browser->handle, WF_VSLSIZ, size, 0, 0, 0);
    (void) wind_set(browser->handle, WF_VSLIDE, slider, 0, 0, 0);
}

static void desktop_browser_sync_work(desktop_browser_window_t *browser)
{
    if (browser == NULL || browser->used == 0) {
        return;
    }

    wind_get(browser->handle, WF_WORKXYWH, &browser->work.g_x,
        &browser->work.g_y, &browser->work.g_w, &browser->work.g_h);
    browser->rows_visible = (WORD) (browser->work.g_h / DESKTOP_BROWSER_ROW_H);
    if (browser->rows_visible < 1) {
        browser->rows_visible = 1;
    }
    browser->columns_visible = (WORD) ((browser->work.g_w -
        DESKTOP_ICON_MARGIN_X) / DESKTOP_ICON_STEP_X);
    if (browser->columns_visible < 1) {
        browser->columns_visible = 1;
    }
    browser->rows_per_page = (WORD) ((browser->work.g_h -
        DESKTOP_ICON_MARGIN_Y) / DESKTOP_ICON_STEP_Y);
    if (browser->rows_per_page < 1) {
        browser->rows_per_page = 1;
    }
    browser->rows_per_page = (WORD) (browser->rows_per_page *
        browser->columns_visible);
    desktop_browser_sync_sliders(browser);
}

static const char *desktop_browser_entry_prefix(
    const desktop_browser_window_t *browser,
    const desktop_browser_entry_t *entry)
{
    if (entry == NULL) {
        return " ";
    }
    if (entry->is_parent != 0) {
        return "<";
    }
    if (entry->is_dir != 0) {
        return "/";
    }
    if (desktop_browser_entry_has_prg_icon(browser, entry) != 0) {
        return "*";
    }
    return " ";
}

static const desktop_icon_asset_t *desktop_browser_entry_asset(
    const desktop_browser_window_t *browser,
    const desktop_browser_entry_t *entry)
{
    if (entry == NULL) {
        return &desktop_doc_icon_asset;
    }
    if (entry->is_parent != 0 || entry->is_dir != 0) {
        return &desktop_folder_icon_asset;
    }
    if (desktop_browser_entry_has_prg_icon(browser, entry) != 0) {
        return &desktop_prg_icon_asset;
    }
    return &desktop_doc_icon_asset;
}

static void desktop_browser_redraw(desktop_browser_window_t *browser,
                                   const GRECT *dirty)
{
    GRECT visible;
    WORD clip_xy[4];
    WORD fill[4];
    WORD previous_font;

    if (browser == NULL || browser->used == 0) {
        return;
    }

    wind_update(BEG_UPDATE);
    wind_get(browser->handle, WF_FIRSTXYWH, &visible.g_x, &visible.g_y,
        &visible.g_w, &visible.g_h);
    previous_font = vst_font(g_desktop.vdi_handle, SMALL);

    while (visible.g_w > 0 && visible.g_h > 0) {
        WORD x0 = visible.g_x;
        WORD y0 = visible.g_y;
        WORD x1 = (WORD) (visible.g_x + visible.g_w - 1);
        WORD y1 = (WORD) (visible.g_y + visible.g_h - 1);
        WORD row;

        if (dirty != NULL) {
            WORD dx1 = (WORD) (dirty->g_x + dirty->g_w - 1);
            WORD dy1 = (WORD) (dirty->g_y + dirty->g_h - 1);

            if (x0 < dirty->g_x) {
                x0 = dirty->g_x;
            }
            if (y0 < dirty->g_y) {
                y0 = dirty->g_y;
            }
            if (x1 > dx1) {
                x1 = dx1;
            }
            if (y1 > dy1) {
                y1 = dy1;
            }
        }

        if (x0 <= x1 && y0 <= y1) {
            clip_xy[0] = x0;
            clip_xy[1] = y0;
            clip_xy[2] = x1;
            clip_xy[3] = y1;
            fill[0] = x0;
            fill[1] = y0;
            fill[2] = x1;
            fill[3] = y1;
            vs_clip(g_desktop.vdi_handle, 1, clip_xy);
            vsf_color(g_desktop.vdi_handle, BLACK);
            vr_recfl(g_desktop.vdi_handle, fill);

            if (browser->view_mode == DESKTOP_BROWSER_VIEW_ICONS) {
                WORD slot;

                for (slot = 0; slot < browser->rows_per_page; ++slot) {
                    WORD index = (WORD) (browser->top_index + slot);
                    GRECT icon_rect;

                    if (index >= browser->entry_count ||
                        desktop_browser_icon_rect(browser, index, &icon_rect) == 0) {
                        continue;
                    }
                    if (icon_rect.g_x > x1 || icon_rect.g_y > y1 ||
                        icon_rect.g_x + icon_rect.g_w - 1 < x0 ||
                        icon_rect.g_y + icon_rect.g_h - 1 < y0) {
                        continue;
                    }

                    {
                        const desktop_browser_entry_t *entry =
                            &browser->entries[index];
                        const desktop_icon_asset_t *asset =
                            desktop_browser_entry_asset(browser, entry);
                        UWORD state = (browser->selected_index == index) ?
                            SELECTED : NORMAL;

                        desktop_draw_icon_object(asset, entry->name,
                            &icon_rect, state);
                    }
                }
            } else {
                for (row = 0; row < browser->rows_visible; ++row) {
                    WORD index = (WORD) (browser->top_index + row);
                    WORD row_y = (WORD) (browser->work.g_y +
                        row * DESKTOP_BROWSER_ROW_H);
                    WORD row_bottom = (WORD) (row_y + DESKTOP_BROWSER_ROW_H - 1);

                    if (row_bottom < y0 || row_y > y1) {
                        continue;
                    }

                    if (index < browser->entry_count) {
                        const desktop_browser_entry_t *entry = &browser->entries[index];
                        WORD row_fill[4];
                        char line[128];

                        row_fill[0] = browser->work.g_x;
                        row_fill[1] = row_y;
                        row_fill[2] = (WORD) (browser->work.g_x + browser->work.g_w - 1);
                        row_fill[3] = row_bottom;

                        if (browser->selected_index == index) {
                            vsf_color(g_desktop.vdi_handle, WHITE);
                            vr_recfl(g_desktop.vdi_handle, row_fill);
                            vst_color(g_desktop.vdi_handle, BLACK);
                        } else {
                            vst_color(g_desktop.vdi_handle, WHITE);
                        }

                        (void) snprintf(line, sizeof(line), "%s %.*s",
                            desktop_browser_entry_prefix(browser, entry),
                            (int) sizeof(line) - 3, entry->name);
                        v_gtext(g_desktop.vdi_handle,
                            (WORD) (browser->work.g_x + 8),
                            (WORD) (row_y + 13), (const BYTE *) line);
                    }
                }
            }

            vs_clip(g_desktop.vdi_handle, 0, clip_xy);
        }

        wind_get(browser->handle, WF_NEXTXYWH, &visible.g_x, &visible.g_y,
            &visible.g_w, &visible.g_h);
    }

    (void) vst_font(g_desktop.vdi_handle, previous_font);
    wind_update(END_UPDATE);
}

static void desktop_browser_scroll_to(desktop_browser_window_t *browser, WORD top)
{
    WORD max_top;

    if (browser == NULL || browser->used == 0) {
        return;
    }

    max_top = desktop_browser_max_top(browser);
    if (top < 0) {
        top = 0;
    }
    if (top > max_top) {
        top = max_top;
    }
    if (top == browser->top_index) {
        return;
    }

    browser->top_index = top;
    desktop_browser_sync_sliders(browser);
    desktop_browser_redraw(browser, NULL);
}

static desktop_browser_window_t *desktop_find_browser_by_handle(WORD handle)
{
    WORD i;

    for (i = 0; i < DESKTOP_BROWSER_MAX_WINDOWS; ++i) {
        if (g_desktop.browsers[i].used != 0 &&
            g_desktop.browsers[i].handle == handle) {
            return &g_desktop.browsers[i];
        }
    }

    return NULL;
}

static desktop_browser_window_t *desktop_alloc_browser(void)
{
    WORD i;

    for (i = 0; i < DESKTOP_BROWSER_MAX_WINDOWS; ++i) {
        if (g_desktop.browsers[i].used == 0) {
            memset(&g_desktop.browsers[i], 0, sizeof(g_desktop.browsers[i]));
            g_desktop.browsers[i].selected_index = NIL;
            g_desktop.browsers[i].last_click_index = NIL;
            return &g_desktop.browsers[i];
        }
    }

    return NULL;
}

static void desktop_browser_open_entry(desktop_browser_window_t *browser,
                                       WORD index)
{
    const desktop_browser_entry_t *entry;
    char parent[GEM_OS_PATH_MAX];
    char path[GEM_OS_PATH_MAX];
    char *slash;

    if (browser == NULL || index < 0 || index >= browser->entry_count) {
        return;
    }

    entry = &browser->entries[index];
    if (entry->is_parent != 0) {
        strncpy(parent, browser->path, sizeof(parent) - 1u);
        parent[sizeof(parent) - 1u] = '\0';
        slash = strrchr(parent, '/');
        if (slash == NULL || slash == parent) {
            strncpy(parent, "/", sizeof(parent) - 1u);
            parent[sizeof(parent) - 1u] = '\0';
        } else {
            *slash = '\0';
        }
        if (desktop_browser_load_path(browser, parent) != 0) {
            wind_set_str(browser->handle, WF_NAME, browser->title);
            desktop_browser_sync_work(browser);
            desktop_update_desk_menu_labels();
            desktop_browser_redraw(browser, NULL);
        }
        return;
    }

    if (entry->is_dir != 0) {
        if (desktop_browser_entry_path(browser, entry, path, sizeof(path)) != 0 &&
            desktop_browser_load_path(browser, path) != 0) {
            wind_set_str(browser->handle, WF_NAME, browser->title);
            desktop_browser_sync_work(browser);
            desktop_update_desk_menu_labels();
            desktop_browser_redraw(browser, NULL);
        }
        return;
    }

    if (desktop_browser_entry_is_launchable(browser, entry) != 0 &&
        desktop_browser_entry_path(browser, entry, path, sizeof(path)) != 0) {
        (void) desktop_launch_hosted_app(path, entry->name);
    } else {
        char text[128];

        (void) snprintf(text, sizeof(text), "File selected: %.*s",
            (int) sizeof(text) - 17, entry->name);
        desktop_set_status(text);
    }
    desktop_redraw(NULL);
}

static void desktop_open_disk_path(const char *path, const char *label)
{
    desktop_browser_window_t *browser;
    GRECT outer;
    WORD work_x;
    WORD work_y;
    WORD work_w = 360;
    WORD work_h = 220;
    WORD used_windows = 0;
    WORD i;
    WORD max_work_x;
    WORD max_work_y;

    (void) label;

    if (path == NULL) {
        return;
    }

    for (i = 0; i < DESKTOP_BROWSER_MAX_WINDOWS; ++i) {
        if (g_desktop.browsers[i].used != 0) {
            ++used_windows;
        }
    }

    browser = desktop_alloc_browser();
    if (browser == NULL) {
        desktop_set_status("No free file windows.");
        desktop_redraw(NULL);
        return;
    }

    if (desktop_browser_load_path(browser, path) == 0) {
        desktop_set_status("Unable to open directory.");
        return;
    }

    work_x = (WORD) (g_desktop.work.g_x + 220 + used_windows * 24);
    work_y = (WORD) (g_desktop.work.g_y + 40 + used_windows * 20);
    max_work_x = (WORD) (g_desktop.work.g_x + g_desktop.work.g_w - work_w - 24);
    max_work_y = (WORD) (g_desktop.work.g_y + g_desktop.work.g_h - work_h - 24);
    if (max_work_x < g_desktop.work.g_x) {
        max_work_x = g_desktop.work.g_x;
    }
    if (max_work_y < g_desktop.work.g_y) {
        max_work_y = g_desktop.work.g_y;
    }
    if (work_x > max_work_x) {
        work_x = max_work_x;
    }
    if (work_y > max_work_y) {
        work_y = max_work_y;
    }
    (void) wind_calc(WC_BORDER, NAME | CLOSER | MOVER | SIZER |
        UPARROW | DNARROW | VSLIDE, work_x, work_y, work_w, work_h,
        &outer.g_x, &outer.g_y, &outer.g_w, &outer.g_h);
    browser->handle = wind_create(NAME | CLOSER | MOVER | SIZER |
        UPARROW | DNARROW | VSLIDE, 0, 0, g_desktop.work.g_w, g_desktop.work.g_h);
    if (browser->handle <= 0) {
        memset(browser, 0, sizeof(*browser));
        desktop_set_status("Unable to create file window.");
        desktop_redraw(NULL);
        return;
    }

    browser->used = 1;
    browser->view_mode = DESKTOP_BROWSER_VIEW_LIST;
    wind_set_str(browser->handle, WF_NAME, browser->title);
    if (!wind_open(browser->handle, outer.g_x, outer.g_y, outer.g_w, outer.g_h)) {
        wind_delete(browser->handle);
        memset(browser, 0, sizeof(*browser));
        desktop_set_status("Unable to open file window.");
        desktop_redraw(NULL);
        return;
    }

    desktop_browser_sync_work(browser);
    desktop_update_desk_menu_labels();
    desktop_browser_redraw(browser, NULL);
}

static void desktop_open_selected_icon(void)
{
    const desktop_icon_entry_t *selected = desktop_find_icon(g_desktop.selected_icon);

    if (selected == NULL || selected->is_trash != 0) {
        desktop_set_status("Open requires a disk selection.");
        desktop_redraw(NULL);
        return;
    }

    desktop_open_disk_path(selected->path, selected->label);
}

static void desktop_browser_close(desktop_browser_window_t *browser)
{
    GRECT outer;

    if (browser == NULL || browser->used == 0) {
        return;
    }

    wind_get(browser->handle, WF_WXYWH, &outer.g_x, &outer.g_y,
        &outer.g_w, &outer.g_h);
    (void) wind_close(browser->handle);
    (void) wind_delete(browser->handle);
    desktop_redraw(&outer);
    memset(browser, 0, sizeof(*browser));
    desktop_update_desk_menu_labels();
}

static void desktop_browser_handle_click(WORD handle, WORD mx, WORD my)
{
    desktop_browser_window_t *browser = desktop_find_browser_by_handle(handle);
    WORD index;
    WORD previous_index;
    uint32_t now;
    GRECT dirty;
    GRECT rect;
    int dirty_set = 0;
    int is_repeat_click = 0;

    if (browser == NULL) {
        return;
    }

    desktop_browser_sync_work(browser);
    if (mx < browser->work.g_x || my < browser->work.g_y ||
        mx >= browser->work.g_x + browser->work.g_w ||
        my >= browser->work.g_y + browser->work.g_h) {
        return;
    }

    if (browser->view_mode == DESKTOP_BROWSER_VIEW_ICONS) {
        WORD relative_x = (WORD) (mx - browser->work.g_x -
            DESKTOP_ICON_MARGIN_X);
        WORD relative_y = (WORD) (my - browser->work.g_y -
            DESKTOP_ICON_MARGIN_Y);
        WORD column;
        WORD row;

        if (relative_x < 0 || relative_y < 0) {
            return;
        }
        column = (WORD) (relative_x / DESKTOP_ICON_STEP_X);
        row = (WORD) (relative_y / DESKTOP_ICON_STEP_Y);

        index = (WORD) (browser->top_index +
            row * browser->columns_visible + column);
        if (index >= 0 && index < browser->entry_count) {
            GRECT hit_rect;

            if (desktop_browser_icon_rect(browser, index, &hit_rect) == 0 ||
                mx >= hit_rect.g_x + hit_rect.g_w ||
                my >= hit_rect.g_y + hit_rect.g_h) {
                return;
            }
        }
    } else {
        index = (WORD) (browser->top_index +
            (my - browser->work.g_y) / DESKTOP_BROWSER_ROW_H);
    }
    if (index < 0 || index >= browser->entry_count) {
        return;
    }

    now = gem_os_ticks_ms();
    previous_index = browser->selected_index;
    if (previous_index == index) {
        is_repeat_click = 1;
    }
    browser->selected_index = index;
    if (((browser->view_mode == DESKTOP_BROWSER_VIEW_ICONS) ?
            desktop_browser_icon_rect(browser, previous_index, &dirty) :
            desktop_browser_row_rect(browser, previous_index, &dirty)) != 0) {
        if (browser->view_mode == DESKTOP_BROWSER_VIEW_ICONS) {
            desktop_expand_icon_damage_rect(&dirty);
        }
        dirty_set = 1;
    }
    if (((browser->view_mode == DESKTOP_BROWSER_VIEW_ICONS) ?
            desktop_browser_icon_rect(browser, index, &rect) :
            desktop_browser_row_rect(browser, index, &rect)) != 0) {
        if (browser->view_mode == DESKTOP_BROWSER_VIEW_ICONS) {
            desktop_expand_icon_damage_rect(&rect);
        }
        if (dirty_set == 0) {
            dirty = rect;
            dirty_set = 1;
        } else {
            WORD x0 = (dirty.g_x < rect.g_x) ? dirty.g_x : rect.g_x;
            WORD y0 = (dirty.g_y < rect.g_y) ? dirty.g_y : rect.g_y;
            WORD x1 = (WORD) (((dirty.g_x + dirty.g_w - 1) >
                (rect.g_x + rect.g_w - 1)) ?
                (dirty.g_x + dirty.g_w - 1) :
                (rect.g_x + rect.g_w - 1));
            WORD y1 = (WORD) (((dirty.g_y + dirty.g_h - 1) >
                (rect.g_y + rect.g_h - 1)) ?
                (dirty.g_y + dirty.g_h - 1) :
                (rect.g_y + rect.g_h - 1));

            dirty.g_x = x0;
            dirty.g_y = y0;
            dirty.g_w = (WORD) (x1 - x0 + 1);
            dirty.g_h = (WORD) (y1 - y0 + 1);
        }
    }
    desktop_browser_redraw(browser, dirty_set != 0 ? &dirty : NULL);

    if ((browser->last_click_index == index &&
         (uint32_t) (now - browser->last_click_ms) <=
            DESKTOP_DOUBLE_CLICK_MS) ||
        is_repeat_click != 0) {
        browser->last_click_index = NIL;
        browser->last_click_ms = 0u;
        desktop_browser_open_entry(browser, index);
    } else {
        browser->last_click_index = index;
        browser->last_click_ms = now;
    }
}

static void desktop_browser_handle_message(WORD msg[8])
{
    desktop_browser_window_t *browser;

    if (msg == NULL) {
        return;
    }

    browser = desktop_find_browser_by_handle(msg[3]);
    if (browser == NULL) {
        return;
    }

    switch (msg[0]) {
    case WM_REDRAW:
        desktop_browser_sync_work(browser);
        desktop_browser_redraw(browser, NULL);
        break;
    case WM_MOVED:
    case WM_SIZED:
    {
        GRECT before;
        GRECT after;

        wind_get(browser->handle, WF_PXYWH, &before.g_x, &before.g_y,
            &before.g_w, &before.g_h);
        (void) wind_set(browser->handle, WF_CURRXYWH, msg[4], msg[5],
            msg[6], msg[7]);
        wind_get(browser->handle, WF_WXYWH, &after.g_x, &after.g_y,
            &after.g_w, &after.g_h);
        desktop_browser_sync_work(browser);
        desktop_redraw_window_change(&before, &after);
        desktop_browser_redraw(browser, NULL);
        break;
    }
    case WM_TOPPED:
        (void) wind_set(browser->handle, WF_TOP, 0, 0, 0, 0);
        break;
    case WM_CLOSED:
        desktop_browser_close(browser);
        break;
    case WM_ARROWED:
        if (browser->view_mode == DESKTOP_BROWSER_VIEW_ICONS) {
            switch (msg[4]) {
            case WA_UPPAGE:
                desktop_browser_scroll_to(browser,
                    (WORD) (browser->top_index - browser->rows_per_page));
                break;
            case WA_DNPAGE:
                desktop_browser_scroll_to(browser,
                    (WORD) (browser->top_index + browser->rows_per_page));
                break;
            case WA_UPLINE:
                desktop_browser_scroll_to(browser,
                    (WORD) (browser->top_index - browser->columns_visible));
                break;
            case WA_DNLINE:
                desktop_browser_scroll_to(browser,
                    (WORD) (browser->top_index + browser->columns_visible));
                break;
            default:
                break;
            }
            break;
        }
        switch (msg[4]) {
        case WA_UPPAGE:
            desktop_browser_scroll_to(browser,
                (WORD) (browser->top_index - browser->rows_visible));
            break;
        case WA_DNPAGE:
            desktop_browser_scroll_to(browser,
                (WORD) (browser->top_index + browser->rows_visible));
            break;
        case WA_UPLINE:
            desktop_browser_scroll_to(browser, (WORD) (browser->top_index - 1));
            break;
        case WA_DNLINE:
            desktop_browser_scroll_to(browser, (WORD) (browser->top_index + 1));
            break;
        default:
            break;
        }
        break;
    case WM_VSLID:
    {
        WORD max_top = desktop_browser_max_top(browser);
        WORD top = 0;

        if (max_top > 0) {
            top = (WORD) ((msg[4] * max_top) / 1000);
        }
        desktop_browser_scroll_to(browser, top);
        break;
    }
    default:
        break;
    }
}

static int desktop_mount_type_hidden(const char *fstype)
{
    static const char *const hidden_types[] = {
        "autofs",
        "binfmt_misc",
        "bpf",
        "cgroup",
        "cgroup2",
        "configfs",
        "debugfs",
        "devpts",
        "devtmpfs",
        "efivarfs",
        "fusectl",
        "hugetlbfs",
        "mqueue",
        "nsfs",
        "overlay",
        "proc",
        "pstore",
        "ramfs",
        "rpc_pipefs",
        "securityfs",
        "selinuxfs",
        "squashfs",
        "sysfs",
        "tmpfs",
        "tracefs"
    };
    size_t i;

    if (fstype == NULL || fstype[0] == '\0') {
        return 1;
    }

    for (i = 0; i < sizeof(hidden_types) / sizeof(hidden_types[0]); ++i) {
        if (strcmp(fstype, hidden_types[i]) == 0) {
            return 1;
        }
    }

    return 0;
}

static int desktop_mount_path_visible(const char *path)
{
    if (path == NULL || path[0] != '/') {
        return 0;
    }

    if (strcmp(path, "/") == 0) {
        return 1;
    }

    if (strncmp(path, "/boot", 5) == 0 ||
        strncmp(path, "/dev", 4) == 0 ||
        strncmp(path, "/proc", 5) == 0 ||
        strncmp(path, "/sys", 4) == 0) {
        return 0;
    }

    if (strncmp(path, "/media/", 7) == 0 ||
        strncmp(path, "/mnt/", 5) == 0 ||
        strncmp(path, "/run/media/", 11) == 0) {
        return 1;
    }

    return 0;
}

static int desktop_mount_visible(const struct mntent *mount)
{
    if (mount == NULL) {
        return 0;
    }

    if (desktop_mount_type_hidden(mount->mnt_type) != 0) {
        return 0;
    }

    return desktop_mount_path_visible(mount->mnt_dir);
}

static int desktop_path_space(const char *path,
                              uint64_t *total_bytes,
                              uint64_t *avail_bytes)
{
    struct statvfs fs;
    uint64_t block_size;

    if (path == NULL || statvfs(path, &fs) != 0) {
        return 0;
    }

    block_size = (uint64_t) fs.f_frsize;
    if (block_size == 0u) {
        block_size = (uint64_t) fs.f_bsize;
    }

    if (total_bytes != NULL) {
        *total_bytes = block_size * (uint64_t) fs.f_blocks;
    }
    if (avail_bytes != NULL) {
        *avail_bytes = block_size * (uint64_t) fs.f_bavail;
    }

    return 1;
}

static void desktop_label_from_path(const char *path,
                                    char *label,
                                    size_t label_size)
{
    const char *base;
    size_t i;
    int new_word = 1;

    if (label_size == 0u) {
        return;
    }

    if (strcmp(path, "/") == 0) {
        strncpy(label, "ROOT", label_size - 1u);
        label[label_size - 1u] = '\0';
        return;
    }

    base = strrchr(path, '/');
    if (base != NULL && base[1] != '\0') {
        ++base;
    } else {
        base = path;
    }

    strncpy(label, base, label_size - 1u);
    label[label_size - 1u] = '\0';
    for (i = 0; label[i] != '\0'; ++i) {
        if (label[i] == ' ' || label[i] == '_' || label[i] == '-') {
            new_word = 1;
            continue;
        }
        if (new_word != 0 && label[i] >= 'a' && label[i] <= 'z') {
            label[i] = (char) (label[i] - ('a' - 'A'));
        } else if (new_word == 0 && label[i] >= 'A' && label[i] <= 'Z') {
            label[i] = (char) (label[i] + ('a' - 'A'));
        }
        new_word = 0;
    }
}

static int desktop_has_path(const char *path)
{
    WORD i;

    for (i = 0; i < g_desktop.icon_count; ++i) {
        if (strcmp(g_desktop.icons[i].path, path) == 0) {
            return 1;
        }
    }
    return 0;
}

static void desktop_add_disk_icon(const char *path)
{
    desktop_icon_entry_t *entry;

    if (g_desktop.icon_count >= DESKTOP_MAX_DISKS || path == NULL ||
        desktop_has_path(path) != 0) {
        return;
    }

    entry = &g_desktop.icons[g_desktop.icon_count];
    memset(entry, 0, sizeof(*entry));
    strncpy(entry->path, path, sizeof(entry->path) - 1u);
    entry->path[sizeof(entry->path) - 1u] = '\0';
    desktop_label_from_path(path, entry->label, sizeof(entry->label));
    entry->asset = &desktop_disk_icon_asset;
    entry->draw_info.asset = entry->asset;
    entry->draw_info.label = entry->label;
    entry->userblk.ab_code = (LONG) (intptr_t) desktop_user_draw;
    entry->userblk.ab_parm = (LONG) (intptr_t) &entry->draw_info;
    entry->object_id = (WORD) (DESKTOP_ICON_BASE + g_desktop.icon_count);
    if (desktop_path_space(path, &entry->total_bytes, &entry->avail_bytes) == 0) {
        entry->total_bytes = 0;
        entry->avail_bytes = 0;
    }
    ++g_desktop.icon_count;
}

static void desktop_add_folder_icon(const char *path, const char *label)
{
    desktop_icon_entry_t *entry;

    if (g_desktop.icon_count >= DESKTOP_MAX_ICONS || path == NULL ||
        desktop_has_path(path) != 0) {
        return;
    }

    entry = &g_desktop.icons[g_desktop.icon_count];
    memset(entry, 0, sizeof(*entry));
    strncpy(entry->path, path, sizeof(entry->path) - 1u);
    entry->path[sizeof(entry->path) - 1u] = '\0';
    if (label != NULL && label[0] != '\0') {
        strncpy(entry->label, label, sizeof(entry->label) - 1u);
        entry->label[sizeof(entry->label) - 1u] = '\0';
    } else {
        desktop_label_from_path(path, entry->label, sizeof(entry->label));
    }
    entry->asset = &desktop_folder_icon_asset;
    entry->draw_info.asset = entry->asset;
    entry->draw_info.label = entry->label;
    entry->userblk.ab_code = (LONG) (intptr_t) desktop_user_draw;
    entry->userblk.ab_parm = (LONG) (intptr_t) &entry->draw_info;
    entry->object_id = (WORD) (DESKTOP_ICON_BASE + g_desktop.icon_count);
    entry->is_folder = 1;
    if (desktop_path_space(path, &entry->total_bytes, &entry->avail_bytes) == 0) {
        entry->total_bytes = 0;
        entry->avail_bytes = 0;
    }
    ++g_desktop.icon_count;
}

static void desktop_probe_disks(void)
{
    FILE *mounts;
    struct mntent *mount;
    char cwd[GEM_OS_PATH_MAX];

    g_desktop.icon_count = 0;

    if (getcwd(cwd, sizeof(cwd)) != NULL) {
        desktop_add_folder_icon(cwd, "Workspace");
    }

    mounts = setmntent("/proc/mounts", "r");
    if (mounts != NULL) {
        while (g_desktop.icon_count < DESKTOP_MAX_DISKS &&
               (mount = getmntent(mounts)) != NULL) {
            if (desktop_mount_visible(mount) == 0) {
                continue;
            }
            desktop_add_disk_icon(mount->mnt_dir);
        }
        endmntent(mounts);
    }

    if (g_desktop.icon_count == 0) {
        desktop_add_disk_icon("/");
    }
}

static void desktop_sort_icons_by_label(void)
{
    WORD i;
    WORD j;

    if (g_desktop.icon_count < 2) {
        desktop_refresh_icon_bindings();
        return;
    }

    for (i = 0; i < g_desktop.icon_count - 1; ++i) {
        for (j = (WORD) (i + 1); j < g_desktop.icon_count; ++j) {
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

static void desktop_update_desk_menu_labels(void)
{
    static const WORD menu_items[6] = {
        MENU_DESK_1, MENU_DESK_2, MENU_DESK_3,
        MENU_DESK_4, MENU_DESK_5, MENU_DESK_6
    };
    OBJECT *tree = g_desktop.menu_tree;
    WORD item_index;
    WORD used_count = 0;

    for (item_index = 0; item_index < 6; ++item_index) {
        OBJECT *item = &tree[menu_items[item_index]];
        desktop_browser_window_t *browser =
            desktop_browser_for_desk_slot(item_index);

        g_desktop.desk_menu_labels[item_index][0] = '\0';
        if (browser != NULL) {
            (void) snprintf(g_desktop.desk_menu_labels[item_index],
                sizeof(g_desktop.desk_menu_labels[item_index]), "  %.*s",
                (int) sizeof(g_desktop.desk_menu_labels[item_index]) - 3,
                browser->title);
            item->ob_spec = (LONG) (intptr_t)
                g_desktop.desk_menu_labels[item_index];
            item->ob_state &= (UWORD) ~DISABLED;
            ++used_count;
        } else {
            item->ob_spec = (LONG) (intptr_t) "";
            item->ob_state |= DISABLED;
        }
    }

    if (used_count > 0) {
        WORD last_item = menu_items[used_count - 1];

        tree[MENU_DESK_BOX].ob_tail = last_item;
        tree[last_item].ob_next = MENU_DESK_BOX;
        tree[MENU_DESK_BOX].ob_height = (WORD) (used_count + 2);
    } else {
        tree[MENU_DESK_BOX].ob_tail = MENU_DESK_SEPARATOR;
        tree[MENU_DESK_SEPARATOR].ob_next = MENU_DESK_BOX;
        tree[MENU_DESK_BOX].ob_height = 2;
    }
}

static desktop_browser_window_t *desktop_browser_for_desk_slot(WORD slot)
{
    WORD i;
    WORD seen = 0;

    if (slot < 0) {
        return NULL;
    }

    for (i = 0; i < DESKTOP_BROWSER_MAX_WINDOWS; ++i) {
        if (g_desktop.browsers[i].used == 0) {
            continue;
        }
        if (seen == slot) {
            return &g_desktop.browsers[i];
        }
        ++seen;
    }

    return NULL;
}

static void desktop_refresh_icon_bindings(void)
{
    WORD i;

    for (i = 0; i < g_desktop.icon_count; ++i) {
        g_desktop.icons[i].draw_info.asset = g_desktop.icons[i].asset;
        g_desktop.icons[i].draw_info.label = g_desktop.icons[i].label;
        g_desktop.icons[i].userblk.ab_code =
            (LONG) (intptr_t) desktop_user_draw;
        g_desktop.icons[i].userblk.ab_parm =
            (LONG) (intptr_t) &g_desktop.icons[i].draw_info;
    }
}

static void desktop_reset_tree_links(void)
{
    WORD i;

    for (i = 0; i < DESKTOP_OBJECT_COUNT; ++i) {
        g_desktop.desktop_tree[i].ob_next = i;
        g_desktop.desktop_tree[i].ob_head = NIL;
        g_desktop.desktop_tree[i].ob_tail = NIL;
    }
}

static void desktop_init_menu_tree(void)
{
    OBJECT *tree = g_desktop.menu_tree;

    desktop_init_object(&tree[MENU_ROOT], -1, MENU_BAR_BOX, MENU_POPUPS,
        G_IBOX, NONE, NORMAL, 0L, 0, 0, 0, 0);
    desktop_init_object(&tree[MENU_BAR_BOX], MENU_POPUPS, MENU_TITLES,
        MENU_TITLES, G_BOX, NONE, NORMAL, 0x1100L, 0, 0, 80, 1);
    desktop_init_object(&tree[MENU_TITLES], MENU_BAR_BOX, MENU_TITLE_DESK,
        MENU_TITLE_ARRANGE, G_IBOX, NONE, NORMAL, 0L, 0, 0, 80, 1);
    desktop_init_object(&tree[MENU_POPUPS], MENU_ROOT, MENU_DESK_BOX,
        MENU_ARRANGE_BOX, G_IBOX, NONE, NORMAL, 0L, 0, 0, 0, 0);
    desktop_init_object(&tree[MENU_TITLE_DESK], MENU_TITLE_FILE, NIL, NIL,
        G_TITLE, NONE, NORMAL, (LONG) (intptr_t) " Desk ", 0, 0, 6, 1);
    desktop_init_object(&tree[MENU_TITLE_FILE], MENU_TITLE_OPTIONS, NIL, NIL,
        G_TITLE, NONE, NORMAL, (LONG) (intptr_t) " File ", 6, 0, 6, 1);
    desktop_init_object(&tree[MENU_TITLE_OPTIONS], MENU_TITLE_ARRANGE, NIL, NIL,
        G_TITLE, NONE, NORMAL, (LONG) (intptr_t) " Options ", 12, 0, 9, 1);
    desktop_init_object(&tree[MENU_TITLE_ARRANGE], MENU_TITLES, NIL, NIL,
        G_TITLE, LASTOB, NORMAL, (LONG) (intptr_t) " Arrange ", 21, 0, 9, 1);

    desktop_init_object(&tree[MENU_DESK_BOX], MENU_FILE_BOX, MENU_DESK_ABOUT,
        MENU_DESK_6, G_BOX, NONE, NORMAL, 0x1100L, 0, 0, 22, 8);
    desktop_init_object(&tree[MENU_DESK_ABOUT], MENU_DESK_SEPARATOR, NIL, NIL,
        G_STRING, NONE, NORMAL, (LONG) (intptr_t) "  Desktop info...   ", 0, 0, 22, 1);
    desktop_init_object(&tree[MENU_DESK_SEPARATOR], MENU_DESK_1, NIL, NIL,
        G_STRING, NONE, DISABLED, (LONG) (intptr_t) "--------------------", 0, 1, 22, 1);
    desktop_init_object(&tree[MENU_DESK_1], MENU_DESK_2, NIL, NIL,
        G_STRING, NONE, DISABLED, (LONG) (intptr_t) "", 0, 2, 22, 1);
    desktop_init_object(&tree[MENU_DESK_2], MENU_DESK_3, NIL, NIL,
        G_STRING, NONE, DISABLED, (LONG) (intptr_t) "", 0, 3, 22, 1);
    desktop_init_object(&tree[MENU_DESK_3], MENU_DESK_4, NIL, NIL,
        G_STRING, NONE, DISABLED, (LONG) (intptr_t) "", 0, 4, 22, 1);
    desktop_init_object(&tree[MENU_DESK_4], MENU_DESK_5, NIL, NIL,
        G_STRING, NONE, DISABLED, (LONG) (intptr_t) "", 0, 5, 22, 1);
    desktop_init_object(&tree[MENU_DESK_5], MENU_DESK_6, NIL, NIL,
        G_STRING, NONE, DISABLED, (LONG) (intptr_t) "", 0, 6, 22, 1);
    desktop_init_object(&tree[MENU_DESK_6], MENU_DESK_BOX, NIL, NIL,
        G_STRING, LASTOB, DISABLED, (LONG) (intptr_t) "", 0, 7, 22, 1);

    desktop_init_object(&tree[MENU_FILE_BOX], MENU_OPTIONS_BOX, MENU_FILE_OPEN,
        MENU_FILE_EXIT, G_BOX, NONE, NORMAL, 0x1100L, 0, 0, 31, 8);
    desktop_init_object(&tree[MENU_FILE_OPEN], MENU_FILE_INFO, NIL, NIL,
        G_STRING, NONE, NORMAL, (LONG) (intptr_t) "  Open", 0, 0, 31, 1);
    desktop_init_object(&tree[MENU_FILE_INFO], MENU_FILE_SEPARATOR_1, NIL, NIL,
        G_STRING, NONE, NORMAL, (LONG) (intptr_t) "  Info/Rename...  \aI", 0, 1, 31, 1);
    desktop_init_object(&tree[MENU_FILE_SEPARATOR_1], MENU_FILE_DELETE, NIL, NIL,
        G_STRING, NONE, DISABLED, (LONG) (intptr_t) "---------------------", 0, 2, 31, 1);
    desktop_init_object(&tree[MENU_FILE_DELETE], MENU_FILE_FORMAT, NIL, NIL,
        G_STRING, NONE, NORMAL, (LONG) (intptr_t) "  Delete...       \aD", 0, 3, 31, 1);
    desktop_init_object(&tree[MENU_FILE_FORMAT], MENU_FILE_SEPARATOR_2, NIL, NIL,
        G_STRING, NONE, NORMAL, (LONG) (intptr_t) "  Format...", 0, 4, 31, 1);
    desktop_init_object(&tree[MENU_FILE_SEPARATOR_2], MENU_FILE_OUTPUT, NIL, NIL,
        G_STRING, NONE, DISABLED, (LONG) (intptr_t) "---------------------", 0, 5, 31, 1);
    desktop_init_object(&tree[MENU_FILE_OUTPUT], MENU_FILE_EXIT, NIL, NIL,
        G_STRING, NONE, NORMAL, (LONG) (intptr_t) "  To Output       ^U", 0, 6, 31, 1);
    desktop_init_object(&tree[MENU_FILE_EXIT], MENU_FILE_BOX, NIL, NIL,
        G_STRING, LASTOB, NORMAL, (LONG) (intptr_t) "  Exit to DOS     ^Q", 0, 7, 31, 1);

    desktop_init_object(&tree[MENU_OPTIONS_BOX], MENU_ARRANGE_BOX,
        MENU_OPTIONS_INSTALL_DISK, MENU_OPTIONS_DOS,
        G_BOX, NONE, NORMAL, 0x1100L, 0, 0, 36, 6);
    desktop_init_object(&tree[MENU_OPTIONS_INSTALL_DISK], MENU_OPTIONS_CONFIGURE_APP,
        NIL, NIL, G_STRING, NONE, NORMAL,
        (LONG) (intptr_t) "  Install disk drive...", 0, 0, 36, 1);
    desktop_init_object(&tree[MENU_OPTIONS_CONFIGURE_APP], MENU_OPTIONS_SEPARATOR,
        NIL, NIL, G_STRING, NONE, NORMAL,
        (LONG) (intptr_t) "  Configure application...  \aA", 0, 1, 36, 1);
    desktop_init_object(&tree[MENU_OPTIONS_SEPARATOR], MENU_OPTIONS_PREFS,
        NIL, NIL, G_STRING, NONE, DISABLED,
        (LONG) (intptr_t) "-------------------------------", 0, 2, 36, 1);
    desktop_init_object(&tree[MENU_OPTIONS_PREFS], MENU_OPTIONS_SAVE,
        NIL, NIL, G_STRING, NONE, NORMAL,
        (LONG) (intptr_t) "  Set preferences...", 0, 3, 36, 1);
    desktop_init_object(&tree[MENU_OPTIONS_SAVE], MENU_OPTIONS_DOS, NIL, NIL,
        G_STRING, NONE, NORMAL,
        (LONG) (intptr_t) "  Save desktop              \aV", 0, 4, 36, 1);
    desktop_init_object(&tree[MENU_OPTIONS_DOS], MENU_OPTIONS_BOX, NIL, NIL,
        G_STRING, LASTOB, NORMAL,
        (LONG) (intptr_t) "  Enter DOS commands        \aC", 0, 5, 36, 1);

    desktop_init_object(&tree[MENU_ARRANGE_BOX], MENU_POPUPS,
        MENU_ARRANGE_SHOW_AS_ICONS, MENU_ARRANGE_SORT_DATE,
        G_BOX, NONE, NORMAL, 0x1100L, 0, 0, 24, 6);
    desktop_init_object(&tree[MENU_ARRANGE_SHOW_AS_ICONS], MENU_ARRANGE_SEPARATOR,
        NIL, NIL, G_STRING, NONE, NORMAL,
        (LONG) (intptr_t) "  Show as icons  \aS", 0, 0, 24, 1);
    desktop_init_object(&tree[MENU_ARRANGE_SEPARATOR], MENU_ARRANGE_SORT_NAME,
        NIL, NIL, G_STRING, NONE, DISABLED,
        (LONG) (intptr_t) "--------------------", 0, 1, 24, 1);
    desktop_init_object(&tree[MENU_ARRANGE_SORT_NAME], MENU_ARRANGE_SORT_TYPE,
        NIL, NIL, G_STRING, NONE, NORMAL,
        (LONG) (intptr_t) "  Sort by name   \aN", 0, 2, 24, 1);
    desktop_init_object(&tree[MENU_ARRANGE_SORT_TYPE], MENU_ARRANGE_SORT_SIZE,
        NIL, NIL, G_STRING, NONE, NORMAL,
        (LONG) (intptr_t) "  Sort by type   \aP", 0, 3, 24, 1);
    desktop_init_object(&tree[MENU_ARRANGE_SORT_SIZE], MENU_ARRANGE_SORT_DATE,
        NIL, NIL, G_STRING, NONE, NORMAL,
        (LONG) (intptr_t) "  Sort by size   \aZ", 0, 4, 24, 1);
    desktop_init_object(&tree[MENU_ARRANGE_SORT_DATE], MENU_ARRANGE_BOX, NIL, NIL,
        G_STRING, LASTOB, NORMAL,
        (LONG) (intptr_t) "  Sort by date   \aT", 0, 5, 24, 1);
}

static void desktop_init_desktop_tree(void)
{
    WORD i;
    WORD last_object = DESKTOP_ROOT;

    desktop_reset_tree_links();
    desktop_init_object(&g_desktop.desktop_tree[DESKTOP_ROOT], NIL,
        NIL, NIL, G_IBOX, NONE, NORMAL, 0L,
        g_desktop.work.g_x, g_desktop.work.g_y, g_desktop.work.g_w,
        g_desktop.work.g_h);

    for (i = 0; i < g_desktop.icon_count; ++i) {
        desktop_icon_entry_t *entry = &g_desktop.icons[i];

        desktop_init_object(&g_desktop.desktop_tree[entry->object_id],
            entry->object_id, NIL, NIL, G_USERDEF, SELECTABLE, NORMAL,
            (LONG) (intptr_t) &entry->userblk, 0, 0,
            DESKTOP_ICON_OBJECT_W, DESKTOP_ICON_OBJECT_H);
        objc_add(g_desktop.desktop_tree, DESKTOP_ROOT, entry->object_id);
        last_object = entry->object_id;
    }

    g_desktop.desktop_tree[last_object].ob_flags |= LASTOB;
}

static void desktop_layout_icons(void)
{
    WORD i;
    WORD column = 0;
    WORD row = 0;
    WORD max_rows;

    max_rows = (WORD) ((g_desktop.work.g_h - DESKTOP_ICON_MARGIN_Y) /
        DESKTOP_ICON_STEP_Y);
    if (max_rows < 1) {
        max_rows = 1;
    }

    for (i = 0; i < g_desktop.icon_count; ++i) {
        desktop_icon_entry_t *entry = &g_desktop.icons[i];
        WORD x;
        WORD y;

        if (entry->is_trash != 0) {
            continue;
        }

        x = (WORD) (DESKTOP_ICON_MARGIN_X + column * DESKTOP_ICON_STEP_X);
        y = (WORD) (DESKTOP_ICON_MARGIN_Y + row * DESKTOP_ICON_STEP_Y);
        g_desktop.desktop_tree[entry->object_id].ob_x = x;
        g_desktop.desktop_tree[entry->object_id].ob_y = y;
        ++row;
        if (row >= max_rows) {
            row = 0;
            ++column;
        }
    }

    for (i = 0; i < g_desktop.icon_count; ++i) {
        desktop_icon_entry_t *entry = &g_desktop.icons[i];

        if (entry->is_trash == 0) {
            continue;
        }
        g_desktop.desktop_tree[entry->object_id].ob_x =
            (WORD) (g_desktop.work.g_w - DESKTOP_ICON_OBJECT_W - 12);
        g_desktop.desktop_tree[entry->object_id].ob_y =
            (WORD) (g_desktop.work.g_h - DESKTOP_ICON_OBJECT_H - 10);
    }
}

static void desktop_draw_icon_object(const desktop_icon_asset_t *asset,
                                     const char *label,
                                     const GRECT *rect,
                                     UWORD state)
{
    char display_label[GEM_OS_PATH_MAX];
    WORD extent[8];
    WORD label_box[4];
    WORD text_x;
    WORD icon_x;
    WORD icon_y;
    WORD label_y;
    WORD previous_font;
    WORD text_height;
    WORD text_width;
    WORD max_text_width;
    WORD label_left;
    WORD label_right;
    size_t label_len;

    if (asset == NULL || label == NULL || rect == NULL) {
        return;
    }

    strncpy(display_label, label, sizeof(display_label) - 1u);
    display_label[sizeof(display_label) - 1u] = '\0';

    icon_x = (WORD) (rect->g_x + (rect->g_w - asset->width) / 2);
    icon_y = rect->g_y;
    desktop_draw_masked_icon(asset, icon_x, icon_y);

    previous_font = vst_font(g_desktop.vdi_handle, SMALL);
    vqt_extent(g_desktop.vdi_handle, display_label, extent);
    text_width = (WORD) (extent[2] - extent[0] + 1);
    max_text_width = (WORD) (DESKTOP_ICON_STEP_X - 4);
    label_len = strlen(display_label);
    if (text_width > max_text_width && label_len > 0u) {
        size_t visible_len = (size_t) (((LONG) label_len * max_text_width) /
            text_width);

        if (visible_len < 1u) {
            visible_len = 1u;
        }
        display_label[visible_len] = '\0';
        vqt_extent(g_desktop.vdi_handle, display_label, extent);
        text_width = (WORD) (extent[2] - extent[0] + 1);
        while (text_width > max_text_width && visible_len > 1u) {
            display_label[--visible_len] = '\0';
            vqt_extent(g_desktop.vdi_handle, display_label, extent);
            text_width = (WORD) (extent[2] - extent[0] + 1);
        }
    }
    text_height = (WORD) (extent[5] - extent[1] + 1);
    text_x = (WORD) (rect->g_x + (rect->g_w - text_width) / 2);
    label_y = (WORD) (rect->g_y + DESKTOP_ICON_TEXT_Y + 14);
    label_box[0] = (WORD) (text_x - 2);
    label_box[1] = (WORD) (label_y - text_height - 2);
    label_box[2] = (WORD) (text_x + text_width + 1);
    label_box[3] = (WORD) (label_y + 2);
    label_left = (WORD) (rect->g_x -
        (DESKTOP_ICON_STEP_X - rect->g_w) / 2);
    label_right = (WORD) (label_left + DESKTOP_ICON_STEP_X - 1);
    if (label_box[0] < label_left) {
        label_box[0] = label_left;
    }
    if (label_box[2] > label_right) {
        label_box[2] = label_right;
    }
    if ((state & SELECTED) != 0u) {
        vsf_color(g_desktop.vdi_handle, WHITE);
        v_bar(g_desktop.vdi_handle, label_box);
        vst_color(g_desktop.vdi_handle, BLACK);
    } else {
        vsf_color(g_desktop.vdi_handle, BLACK);
        v_bar(g_desktop.vdi_handle, label_box);
        vsl_color(g_desktop.vdi_handle, WHITE);
        v_rbox(g_desktop.vdi_handle, label_box);
        vst_color(g_desktop.vdi_handle, WHITE);
    }
    v_gtext(g_desktop.vdi_handle, text_x, label_y,
        (const BYTE *) display_label);
    (void) vst_font(g_desktop.vdi_handle, previous_font);
}

static WORD desktop_user_draw(LONG parm_block)
{
    const PARMBLK *pb = (const PARMBLK *) (intptr_t) parm_block;
    const desktop_icon_entry_t *entry;
    GRECT rect;

    if (pb == NULL) {
        return 0;
    }

    entry = desktop_find_icon(pb->pb_obj);
    if (entry == NULL) {
        return 0;
    }

    rect.g_x = pb->pb_x;
    rect.g_y = pb->pb_y;
    rect.g_w = pb->pb_w;
    rect.g_h = pb->pb_h;
    desktop_draw_icon_object(entry->draw_info.asset,
        entry->draw_info.label, &rect, pb->pb_currstate);
    return 0;
}

static void desktop_set_status(const char *text)
{
    if (text == NULL) {
        g_desktop.status_text[0] = '\0';
    } else {
        strncpy(g_desktop.status_text, text, sizeof(g_desktop.status_text) - 1u);
        g_desktop.status_text[sizeof(g_desktop.status_text) - 1u] = '\0';
    }
}

static void desktop_draw_background_pattern(const GRECT *dirty)
{
    WORD fill[4];
    WORD x0;
    WORD y0;
    WORD x1;
    WORD y1;

    x0 = g_desktop.work.g_x;
    y0 = g_desktop.work.g_y;
    x1 = (WORD) (g_desktop.work.g_x + g_desktop.work.g_w - 1);
    y1 = (WORD) (g_desktop.work.g_y + g_desktop.work.g_h - 1);

    if (dirty != NULL) {
        WORD dirty_x1 = (WORD) (dirty->g_x + dirty->g_w - 1);
        WORD dirty_y1 = (WORD) (dirty->g_y + dirty->g_h - 1);

        if (x0 < dirty->g_x) {
            x0 = dirty->g_x;
        }
        if (y0 < dirty->g_y) {
            y0 = dirty->g_y;
        }
        if (x1 > dirty_x1) {
            x1 = dirty_x1;
        }
        if (y1 > dirty_y1) {
            y1 = dirty_y1;
        }
    }

    if (x0 > x1 || y0 > y1) {
        return;
    }

    fill[0] = x0;
    fill[1] = y0;
    fill[2] = x1;
    fill[3] = y1;

    (void) vsf_interior(g_desktop.vdi_handle, FIS_PATTERN);
    (void) vsf_style(g_desktop.vdi_handle, 2);
    vsf_color(g_desktop.vdi_handle, BLACK);
    v_bar(g_desktop.vdi_handle, fill);
    (void) vsf_interior(g_desktop.vdi_handle, FIS_SOLID);
    (void) vsf_style(g_desktop.vdi_handle, 1);
}

static void desktop_redraw(const GRECT *dirty)
{
    GRECT visible;

    wind_update(BEG_UPDATE);
    (void) vswr_mode(g_desktop.vdi_handle, MD_REPLACE);
    wind_get(0, WF_FIRSTXYWH, &visible.g_x, &visible.g_y,
        &visible.g_w, &visible.g_h);
    while (visible.g_w > 0 && visible.g_h > 0) {
        WORD x0 = visible.g_x;
        WORD y0 = visible.g_y;
        WORD x1 = (WORD) (visible.g_x + visible.g_w - 1);
        WORD y1 = (WORD) (visible.g_y + visible.g_h - 1);
        WORD clip_xy[4];
        GRECT clipped_dirty;

        if (dirty != NULL) {
            WORD dirty_x1 = (WORD) (dirty->g_x + dirty->g_w - 1);
            WORD dirty_y1 = (WORD) (dirty->g_y + dirty->g_h - 1);

            if (x0 < dirty->g_x) {
                x0 = dirty->g_x;
            }
            if (y0 < dirty->g_y) {
                y0 = dirty->g_y;
            }
            if (x1 > dirty_x1) {
                x1 = dirty_x1;
            }
            if (y1 > dirty_y1) {
                y1 = dirty_y1;
            }
        }

        if (x0 <= x1 && y0 <= y1) {
            clip_xy[0] = x0;
            clip_xy[1] = y0;
            clip_xy[2] = x1;
            clip_xy[3] = y1;
            clipped_dirty.g_x = x0;
            clipped_dirty.g_y = y0;
            clipped_dirty.g_w = (WORD) (x1 - x0 + 1);
            clipped_dirty.g_h = (WORD) (y1 - y0 + 1);
            vs_clip(g_desktop.vdi_handle, 1, clip_xy);
            desktop_draw_background_pattern(&clipped_dirty);
            objc_draw(g_desktop.desktop_tree, ROOT, MAX_DEPTH,
                x0, y0, clipped_dirty.g_w, clipped_dirty.g_h);
            vs_clip(g_desktop.vdi_handle, 0, clip_xy);
        }

        wind_get(0, WF_NEXTXYWH, &visible.g_x, &visible.g_y,
            &visible.g_w, &visible.g_h);
    }
    v_updwk(g_desktop.vdi_handle);
    wind_update(END_UPDATE);
}

static void desktop_redraw_window_change(const GRECT *before,
                                         const GRECT *after)
{
    GRECT fragments[4];
    WORD count = 0;
    WORD before_right;
    WORD before_bottom;
    WORD overlap_x0;
    WORD overlap_y0;
    WORD overlap_x1;
    WORD overlap_y1;
    WORD after_right;
    WORD after_bottom;
    WORD i;

    if (before == NULL || before->g_w <= 0 || before->g_h <= 0) {
        return;
    }

    if (after == NULL || after->g_w <= 0 || after->g_h <= 0) {
        desktop_redraw(before);
        return;
    }

    before_right = (WORD) (before->g_x + before->g_w - 1);
    before_bottom = (WORD) (before->g_y + before->g_h - 1);
    after_right = (WORD) (after->g_x + after->g_w - 1);
    after_bottom = (WORD) (after->g_y + after->g_h - 1);
    overlap_x0 = (before->g_x > after->g_x) ? before->g_x : after->g_x;
    overlap_y0 = (before->g_y > after->g_y) ? before->g_y : after->g_y;
    overlap_x1 = (before_right < after_right) ? before_right : after_right;
    overlap_y1 = (before_bottom < after_bottom) ? before_bottom : after_bottom;

    if (overlap_x0 > overlap_x1 || overlap_y0 > overlap_y1) {
        desktop_redraw(before);
        return;
    }

    if (before->g_y < overlap_y0) {
        fragments[count].g_x = before->g_x;
        fragments[count].g_y = before->g_y;
        fragments[count].g_w = before->g_w;
        fragments[count].g_h = (WORD) (overlap_y0 - before->g_y);
        ++count;
    }
    if (overlap_y1 < before_bottom) {
        fragments[count].g_x = before->g_x;
        fragments[count].g_y = (WORD) (overlap_y1 + 1);
        fragments[count].g_w = before->g_w;
        fragments[count].g_h = (WORD) (before_bottom - overlap_y1);
        ++count;
    }
    if (before->g_x < overlap_x0) {
        fragments[count].g_x = before->g_x;
        fragments[count].g_y = overlap_y0;
        fragments[count].g_w = (WORD) (overlap_x0 - before->g_x);
        fragments[count].g_h = (WORD) (overlap_y1 - overlap_y0 + 1);
        ++count;
    }
    if (overlap_x1 < before_right) {
        fragments[count].g_x = (WORD) (overlap_x1 + 1);
        fragments[count].g_y = overlap_y0;
        fragments[count].g_w = (WORD) (before_right - overlap_x1);
        fragments[count].g_h = (WORD) (overlap_y1 - overlap_y0 + 1);
        ++count;
    }

    for (i = 0; i < count; ++i) {
        if (fragments[i].g_w > 0 && fragments[i].g_h > 0) {
            desktop_redraw(&fragments[i]);
        }
    }
}

static void desktop_object_rect(WORD object_id, GRECT *rect)
{
    if (rect == NULL) {
        return;
    }

    if (object_id == DESKTOP_ROOT) {
        rect->g_x = g_desktop.desktop_tree[DESKTOP_ROOT].ob_x;
        rect->g_y = g_desktop.desktop_tree[DESKTOP_ROOT].ob_y;
    } else {
        rect->g_x = (WORD) (g_desktop.desktop_tree[DESKTOP_ROOT].ob_x +
            g_desktop.desktop_tree[object_id].ob_x);
        rect->g_y = (WORD) (g_desktop.desktop_tree[DESKTOP_ROOT].ob_y +
            g_desktop.desktop_tree[object_id].ob_y);
    }
    rect->g_w = g_desktop.desktop_tree[object_id].ob_width;
    rect->g_h = g_desktop.desktop_tree[object_id].ob_height;
}

static void desktop_expand_icon_damage_rect(GRECT *rect)
{
    WORD extra;

    if (rect == NULL || rect->g_w >= DESKTOP_ICON_STEP_X) {
        return;
    }

    extra = (WORD) (DESKTOP_ICON_STEP_X - rect->g_w);
    rect->g_x = (WORD) (rect->g_x - extra / 2);
    rect->g_w = DESKTOP_ICON_STEP_X;
}

static WORD desktop_find_icon_at(WORD mx, WORD my)
{
    WORD i;

    for (i = (WORD) (g_desktop.icon_count - 1); i >= 0; --i) {
        const desktop_icon_entry_t *entry = &g_desktop.icons[i];
        GRECT rect;

        desktop_object_rect(entry->object_id, &rect);
        if (mx >= rect.g_x && my >= rect.g_y &&
            mx < rect.g_x + rect.g_w &&
            my < rect.g_y + rect.g_h) {
            return entry->object_id;
        }
        if (i == 0) {
            break;
        }
    }

    return NIL;
}

static void desktop_select_icon(WORD object_id)
{
    WORD i;

    g_desktop.selected_icon = object_id;
    for (i = 0; i < g_desktop.icon_count; ++i) {
        g_desktop.desktop_tree[g_desktop.icons[i].object_id].ob_state = NORMAL;
    }
    if (object_id != NIL) {
        g_desktop.desktop_tree[object_id].ob_state = SELECTED;
    }
}

static void desktop_status_for_icon(const desktop_icon_entry_t *entry)
{
    char text[96];
    int label_max;

    if (entry == NULL) {
        desktop_set_status("Select a disk or the trash can.");
        return;
    }

    if (entry->is_trash != 0) {
        desktop_set_status("Trash selected.");
        return;
    }

    label_max = (int) sizeof(text) - 20;
    (void) snprintf(text, sizeof(text), "%.*s  %llu MB free",
        label_max, entry->label,
        (unsigned long long) (entry->avail_bytes / (1024u * 1024u)));
    desktop_set_status(text);
}

static void desktop_handle_icon_click(WORD object_id)
{
    const desktop_icon_entry_t *entry = desktop_find_icon(object_id);
    WORD previous = g_desktop.selected_icon;
    GRECT dirty;
    GRECT rect;
    int dirty_set = 0;
    uint32_t now = gem_os_ticks_ms();
    int is_double_click = 0;
    int is_repeat_click = 0;

    if (g_desktop.last_desktop_click_object == object_id &&
        (uint32_t) (now - g_desktop.last_desktop_click_ms) <=
            DESKTOP_DOUBLE_CLICK_MS) {
        is_double_click = 1;
    }
    if (previous == object_id) {
        is_repeat_click = 1;
    }

    desktop_select_icon(object_id);
    desktop_status_for_icon(entry);
    if (previous != NIL) {
        desktop_object_rect(previous, &dirty);
        desktop_expand_icon_damage_rect(&dirty);
        dirty_set = 1;
    }
    desktop_object_rect(object_id, &rect);
    desktop_expand_icon_damage_rect(&rect);
    if (dirty_set == 0) {
        dirty = rect;
    } else {
        WORD x0 = dirty.g_x < rect.g_x ? dirty.g_x : rect.g_x;
        WORD y0 = dirty.g_y < rect.g_y ? dirty.g_y : rect.g_y;
        WORD x1 = (WORD) ((dirty.g_x + dirty.g_w - 1) >
            (rect.g_x + rect.g_w - 1) ?
            (dirty.g_x + dirty.g_w - 1) :
            (rect.g_x + rect.g_w - 1));
        WORD y1 = (WORD) ((dirty.g_y + dirty.g_h - 1) >
            (rect.g_y + rect.g_h - 1) ?
            (dirty.g_y + dirty.g_h - 1) :
            (rect.g_y + rect.g_h - 1));

        dirty.g_x = x0;
        dirty.g_y = y0;
        dirty.g_w = (WORD) (x1 - x0 + 1);
        dirty.g_h = (WORD) (y1 - y0 + 1);
    }
    desktop_redraw(&dirty);

    g_desktop.last_desktop_click_object = object_id;
    g_desktop.last_desktop_click_ms = now;
    if ((is_double_click != 0 || is_repeat_click != 0) &&
        entry != NULL && entry->is_trash == 0) {
        desktop_open_disk_path(entry->path, entry->label);
    }
}

static void desktop_install_trash_icon(void)
{
    desktop_icon_entry_t *entry;

    entry = &g_desktop.icons[g_desktop.icon_count];
    memset(entry, 0, sizeof(*entry));
    strncpy(entry->label, "Trash", sizeof(entry->label) - 1u);
    strncpy(entry->path, "trash://desktop", sizeof(entry->path) - 1u);
    entry->asset = &desktop_trash_icon_asset;
    entry->draw_info.asset = entry->asset;
    entry->draw_info.label = entry->label;
    entry->userblk.ab_code = (LONG) (intptr_t) desktop_user_draw;
    entry->userblk.ab_parm = (LONG) (intptr_t) &entry->draw_info;
    entry->object_id = (WORD) (DESKTOP_ICON_BASE + g_desktop.icon_count);
    entry->is_trash = 1;
    ++g_desktop.icon_count;
}

static void desktop_handle_menu_item(WORD item)
{
    char text[96];
    const desktop_icon_entry_t *selected = desktop_find_icon(g_desktop.selected_icon);

    switch (item) {
    case MENU_DESK_ABOUT:
        desktop_set_status("Digital Research GEM Desktop, hosted AES edition.");
        break;
    case MENU_DESK_1:
    case MENU_DESK_2:
    case MENU_DESK_3:
    case MENU_DESK_4:
    case MENU_DESK_5:
    case MENU_DESK_6:
    {
        desktop_browser_window_t *browser =
            desktop_browser_for_desk_slot((WORD) (item - MENU_DESK_1));

        if (browser != NULL) {
            (void) wind_set(browser->handle, WF_TOP, 0, 0, 0, 0);
            (void) snprintf(text, sizeof(text), "Window selected: %.*s",
                (int) sizeof(text) - 18, browser->title);
            desktop_set_status(text);
        } else {
            desktop_set_status("No window in that Desk slot.");
        }
        break;
    }
    case MENU_FILE_OPEN:
        if (selected != NULL && selected->is_trash == 0) {
            desktop_open_selected_icon();
        } else {
            desktop_set_status("Open requires a disk selection.");
        }
        break;
    case MENU_FILE_INFO:
        if (selected != NULL && selected->is_trash == 0) {
            (void) snprintf(text, sizeof(text), "%s total %llu MB",
                selected->label,
                (unsigned long long) (selected->total_bytes / (1024u * 1024u)));
            desktop_set_status(text);
        } else {
            desktop_set_status("Info/Rename requires a disk selection.");
        }
        break;
    case MENU_FILE_DELETE:
        desktop_set_status("Delete is not available on the desktop root.");
        break;
    case MENU_FILE_FORMAT:
        desktop_set_status("Format is intentionally not implemented.");
        break;
    case MENU_FILE_OUTPUT:
        desktop_set_status("To Output selected.");
        break;
    case MENU_FILE_EXIT:
        desktop_set_status("Exit selected. Press q or Esc to quit.");
        break;
    case MENU_OPTIONS_INSTALL_DISK:
        desktop_set_status("Disk install is automatic from host mount points.");
        break;
    case MENU_OPTIONS_CONFIGURE_APP:
        desktop_set_status("Application configuration is not wired yet.");
        break;
    case MENU_OPTIONS_PREFS:
        desktop_set_status("Preferences are fixed in this portable desktop.");
        break;
    case MENU_OPTIONS_SAVE:
        desktop_set_status("Save desktop is not implemented yet.");
        break;
    case MENU_OPTIONS_DOS:
        desktop_set_status("DOS command entry is not available.");
        break;
    case MENU_ARRANGE_SHOW_AS_ICONS:
    {
        WORD i;
        int changed = 0;

        for (i = 0; i < DESKTOP_BROWSER_MAX_WINDOWS; ++i) {
            if (g_desktop.browsers[i].used == 0) {
                continue;
            }
            g_desktop.browsers[i].view_mode = DESKTOP_BROWSER_VIEW_ICONS;
            g_desktop.browsers[i].top_index = 0;
            desktop_browser_sync_work(&g_desktop.browsers[i]);
            desktop_browser_redraw(&g_desktop.browsers[i], NULL);
            changed = 1;
        }
        if (changed != 0) {
            desktop_set_status("File windows switched to icon view.");
        } else {
            desktop_set_status("Open a file window first.");
        }
        break;
    }
    case MENU_ARRANGE_SORT_NAME:
        desktop_sort_icons_by_label();
        desktop_layout_icons();
        desktop_set_status("Icons sorted by name.");
        break;
    case MENU_ARRANGE_SORT_TYPE:
        desktop_set_status("All desktop entries are disks or trash.");
        break;
    case MENU_ARRANGE_SORT_SIZE:
        desktop_set_status("Disk sizes shown in the status line.");
        break;
    case MENU_ARRANGE_SORT_DATE:
        desktop_set_status("Date sorting is not applicable to disk icons.");
        break;
    default:
        break;
    }
    desktop_redraw(NULL);
}

static int desktop_init(void)
{
    WORD work_in[11] = {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 2};
    WORD work_out[57];

    memset(&g_desktop, 0, sizeof(g_desktop));
    g_desktop.selected_icon = NIL;

    if (gem_os_init() == 0) {
        return 0;
    }

    g_desktop.app_id = appl_init();
    if (g_desktop.app_id < 0) {
        gem_os_shutdown();
        return 0;
    }

    g_desktop.vdi_handle = graf_handle(NULL, NULL, NULL, NULL);
    v_opnvwk(work_in, &g_desktop.vdi_handle, work_out);
    if (g_desktop.vdi_handle == 0) {
        appl_exit();
        gem_os_shutdown();
        return 0;
    }

    g_desktop.screen_width = (WORD) (work_out[0] + 1);
    g_desktop.screen_height = (WORD) (work_out[1] + 1);

    desktop_init_menu_tree();
    desktop_probe_disks();
    desktop_sort_icons_by_label();
    desktop_install_trash_icon();
    desktop_update_desk_menu_labels();
    menu_click(1, 1);
    menu_bar(g_desktop.menu_tree, 1);
    wind_get(0, WF_WORKXYWH, &g_desktop.work.g_x, &g_desktop.work.g_y,
        &g_desktop.work.g_w, &g_desktop.work.g_h);
    desktop_set_status("Select a disk or the trash can.");
    desktop_init_desktop_tree();
    desktop_layout_icons();
    desktop_redraw(NULL);
    return 1;
}

static void desktop_shutdown(void)
{
    WORD i;

    for (i = 0; i < DESKTOP_BROWSER_MAX_WINDOWS; ++i) {
        if (g_desktop.browsers[i].used != 0) {
            desktop_browser_close(&g_desktop.browsers[i]);
        }
    }
    menu_bar(g_desktop.menu_tree, 0);
    if (g_desktop.vdi_handle != 0) {
        v_clsvwk(g_desktop.vdi_handle);
    }
    if (g_desktop.app_id >= 0) {
        appl_exit();
    }
    gem_os_shutdown();
}

int main(void)
{
    WORD done = 0;

    if (desktop_init() == 0) {
        return 1;
    }

    while (done == 0) {
        WORD msg[8] = { 0 };
        WORD mx = 0;
        WORD my = 0;
        WORD mb = 0;
        WORD ks = 0;
        WORD kr = 0;
        WORD br = 0;
        WORD event;

        event = evnt_multi((UWORD) (MU_MESAG | MU_BUTTON | MU_KEYBD),
            1, 1, 1,
            0, 0, 0, 0, 0,
            0, 0, 0, 0, 0,
            msg, 0, 0, &mx, &my, &mb, &ks, &kr, &br);
        (void) mb;
        (void) ks;
        (void) br;

        if ((event & MU_KEYBD) != 0) {
            WORD ch = (WORD) (kr & 0xff);

            if (ch == 27 || ch == 'q' || ch == 'Q') {
                done = 1;
            }
        }

        if ((event & MU_MESAG) != 0) {
            if (msg[0] == MN_SELECTED) {
                desktop_handle_menu_item(msg[4]);
                menu_tnormal(g_desktop.menu_tree, msg[3], 1);
            } else if (msg[0] == WM_REDRAW && msg[3] == 0) {
                GRECT dirty;

                dirty.g_x = msg[4];
                dirty.g_y = msg[5];
                dirty.g_w = msg[6];
                dirty.g_h = msg[7];
                desktop_redraw(&dirty);
            } else {
                desktop_browser_handle_message(msg);
            }
        }

        if ((event & MU_BUTTON) != 0) {
            WORD handle = wind_find(mx, my);
            WORD object = desktop_find_icon_at(mx, my);

            if (my < g_desktop.work.g_y) {
                continue;
            }

            if (handle > 0) {
                desktop_browser_handle_click(handle, mx, my);
                continue;
            }

            if (object >= DESKTOP_ICON_BASE &&
                object < DESKTOP_ICON_BASE + g_desktop.icon_count) {
                desktop_handle_icon_click(object);
            } else {
                GRECT dirty;

                if (g_desktop.selected_icon == NIL) {
                    continue;
                }
                desktop_object_rect(g_desktop.selected_icon, &dirty);
                desktop_expand_icon_damage_rect(&dirty);
                desktop_select_icon(NIL);
                desktop_set_status("Select a disk or the trash can.");
                desktop_redraw(&dirty);
            }
        }
    }

    desktop_shutdown();
    return 0;
}
