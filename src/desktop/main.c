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

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

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
    DESKTOP_MAX_ICONS = DESKTOP_MAX_DISKS + 1,
    DESKTOP_OBJECT_COUNT = DESKTOP_ICON_BASE + DESKTOP_MAX_ICONS,
    DESKTOP_ICON_OBJECT_W = 72,
    DESKTOP_ICON_OBJECT_H = 58,
    DESKTOP_ICON_TEXT_Y = 40,
    DESKTOP_ICON_STEP_X = 78,
    DESKTOP_ICON_STEP_Y = 66,
    DESKTOP_ICON_MARGIN_X = 16,
    DESKTOP_ICON_MARGIN_Y = 16
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
} desktop_icon_entry_t;

typedef struct desktop_state {
    OBJECT desktop_tree[DESKTOP_OBJECT_COUNT];
    OBJECT menu_tree[MENU_OBJECT_COUNT];
    desktop_icon_entry_t icons[DESKTOP_MAX_ICONS];
    WORD icon_count;
    WORD selected_icon;
    WORD app_id;
    VDI_HANDLE vdi_handle;
    WORD screen_width;
    WORD screen_height;
    GRECT work;
    char status_text[96];
} desktop_state_t;

static desktop_state_t g_desktop;

static WORD desktop_user_draw(LONG parm_block);
static void desktop_set_status(const char *text);
static void desktop_redraw(const GRECT *dirty);
static void desktop_layout_icons(void);
static void desktop_handle_menu_item(WORD item);
static void desktop_handle_icon_click(WORD object_id);
static void desktop_refresh_icon_bindings(void);
static void desktop_draw_background_pattern(const GRECT *dirty);
static void desktop_object_rect(WORD object_id, GRECT *rect);

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
    if (gem_os_space(path, &entry->total_bytes, &entry->avail_bytes) == 0) {
        entry->total_bytes = 0;
        entry->avail_bytes = 0;
    }
    ++g_desktop.icon_count;
}

static void desktop_add_disk_icon_with_label(const char *path,
                                             const char *label)
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
    if (label != NULL && label[0] != '\0') {
        strncpy(entry->label, label, sizeof(entry->label) - 1u);
        entry->label[sizeof(entry->label) - 1u] = '\0';
    } else {
        desktop_label_from_path(path, entry->label, sizeof(entry->label));
    }
    entry->asset = &desktop_disk_icon_asset;
    entry->draw_info.asset = entry->asset;
    entry->draw_info.label = entry->label;
    entry->userblk.ab_code = (LONG) (intptr_t) desktop_user_draw;
    entry->userblk.ab_parm = (LONG) (intptr_t) &entry->draw_info;
    entry->object_id = (WORD) (DESKTOP_ICON_BASE + g_desktop.icon_count);
    if (gem_os_space(path, &entry->total_bytes, &entry->avail_bytes) == 0) {
        entry->total_bytes = 0;
        entry->avail_bytes = 0;
    }
    ++g_desktop.icon_count;
}

static void desktop_probe_disks(void)
{
    gem_os_volume_iter_t iter;
    gem_os_volume_t volume;

    g_desktop.icon_count = 0;

    if (gem_os_volume_iter_open(&iter) != 0) {
        while (g_desktop.icon_count < DESKTOP_MAX_DISKS &&
               gem_os_volume_iter_read(&iter, &volume) != 0) {
            desktop_add_disk_icon_with_label(volume.mount_path, volume.label);
        }
        gem_os_volume_iter_close(&iter);
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
        G_STRING, NONE, NORMAL, (LONG) (intptr_t) "1", 0, 2, 22, 1);
    desktop_init_object(&tree[MENU_DESK_2], MENU_DESK_3, NIL, NIL,
        G_STRING, NONE, NORMAL, (LONG) (intptr_t) "2", 0, 3, 22, 1);
    desktop_init_object(&tree[MENU_DESK_3], MENU_DESK_4, NIL, NIL,
        G_STRING, NONE, NORMAL, (LONG) (intptr_t) "3", 0, 4, 22, 1);
    desktop_init_object(&tree[MENU_DESK_4], MENU_DESK_5, NIL, NIL,
        G_STRING, NONE, NORMAL, (LONG) (intptr_t) "4", 0, 5, 22, 1);
    desktop_init_object(&tree[MENU_DESK_5], MENU_DESK_6, NIL, NIL,
        G_STRING, NONE, NORMAL, (LONG) (intptr_t) "5", 0, 6, 22, 1);
    desktop_init_object(&tree[MENU_DESK_6], MENU_DESK_BOX, NIL, NIL,
        G_STRING, LASTOB, NORMAL, (LONG) (intptr_t) "6", 0, 7, 22, 1);

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
    }
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

static WORD desktop_user_draw(LONG parm_block)
{
    const PARMBLK *pb = (const PARMBLK *) (intptr_t) parm_block;
    const desktop_icon_draw_t *draw_info;
    WORD extent[8];
    WORD label_box[4];
    WORD text_x;
    WORD icon_x;
    WORD icon_y;
    WORD label_y;
    WORD previous_font;
    WORD text_height;
    WORD text_width;

    if (pb == NULL) {
        return 0;
    }

    draw_info = (const desktop_icon_draw_t *) (intptr_t) pb->pb_parm;
    if (draw_info == NULL || draw_info->asset == NULL) {
        return 0;
    }

    icon_x = (WORD) (pb->pb_x + (pb->pb_w - draw_info->asset->width) / 2);
    icon_y = pb->pb_y;
    desktop_draw_masked_icon(draw_info->asset, icon_x, icon_y);

    previous_font = vst_font(g_desktop.vdi_handle, SMALL);
    vqt_extent(g_desktop.vdi_handle, (char *) draw_info->label, extent);
    text_width = (WORD) (extent[2] - extent[0] + 1);
    text_height = (WORD) (extent[5] - extent[1] + 1);
    text_x = (WORD) (pb->pb_x + (pb->pb_w - text_width) / 2);
    label_y = (WORD) (pb->pb_y + DESKTOP_ICON_TEXT_Y + 14);
    label_box[0] = (WORD) (text_x - 7);
    label_box[1] = (WORD) (label_y - text_height - 2);
    label_box[2] = (WORD) (text_x + text_width + 6);
    label_box[3] = (WORD) (label_y + 2);
    if (label_box[0] < pb->pb_x) {
        label_box[0] = pb->pb_x;
    }
    if (label_box[2] > pb->pb_x + pb->pb_w - 1) {
        label_box[2] = (WORD) (pb->pb_x + pb->pb_w - 1);
    }
    if ((pb->pb_currstate & SELECTED) != 0u) {
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
        (const BYTE *) draw_info->label);
    (void) vst_font(g_desktop.vdi_handle, previous_font);
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
    WORD x;
    WORD y;
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

    vsf_color(g_desktop.vdi_handle, BLACK);
    v_bar(g_desktop.vdi_handle, fill);

    vsf_color(g_desktop.vdi_handle, WHITE);
    for (y = y0;
         y <= y1;
         ++y) {
        for (x = x0;
             x <= x1;
             x = (WORD) (x + 2)) {
            WORD dot[4];
            WORD start_x = x;

            if (((y - g_desktop.work.g_y) & 1) != 0) {
                start_x = (WORD) (start_x + 1);
            }
            if (start_x > x1) {
                continue;
            }
            dot[0] = start_x;
            dot[1] = y;
            dot[2] = start_x;
            dot[3] = y;
            v_bar(g_desktop.vdi_handle, dot);
        }
    }
}

static void desktop_redraw(const GRECT *dirty)
{
    WORD clip_xy[4];
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

    wind_update(BEG_UPDATE);
    (void) vswr_mode(g_desktop.vdi_handle, MD_REPLACE);
    clip_xy[0] = x0;
    clip_xy[1] = y0;
    clip_xy[2] = x1;
    clip_xy[3] = y1;
    vs_clip(g_desktop.vdi_handle, 1, clip_xy);
    desktop_draw_background_pattern(dirty);
    objc_draw(g_desktop.desktop_tree, ROOT, MAX_DEPTH,
        x0, y0, (WORD) (x1 - x0 + 1), (WORD) (y1 - y0 + 1));
    vs_clip(g_desktop.vdi_handle, 0, NULL);
    v_updwk(g_desktop.vdi_handle);
    wind_update(END_UPDATE);
}

static void desktop_object_rect(WORD object_id, GRECT *rect)
{
    WORD x;
    WORD y;

    if (rect == NULL) {
        return;
    }

    objc_offset(g_desktop.desktop_tree, object_id, &x, &y);
    rect->g_x = x;
    rect->g_y = y;
    rect->g_w = g_desktop.desktop_tree[object_id].ob_width;
    rect->g_h = g_desktop.desktop_tree[object_id].ob_height;
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

    desktop_select_icon(object_id);
    desktop_status_for_icon(entry);
    if (previous != NIL) {
        desktop_object_rect(previous, &dirty);
        dirty_set = 1;
    }
    desktop_object_rect(object_id, &rect);
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
        (void) snprintf(text, sizeof(text), "Desktop slot %d selected.",
            (int) (item - MENU_DESK_1 + 1));
        desktop_set_status(text);
        break;
    case MENU_FILE_OPEN:
        if (selected != NULL && selected->is_trash == 0) {
            (void) snprintf(text, sizeof(text), "Open %.72s", selected->label);
            desktop_set_status(text);
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
        desktop_set_status("Items are already shown as icons.");
        break;
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
    menu_click(1, 1);
    menu_bar(g_desktop.menu_tree, 1);
    wind_get(0, WF_WORKXYWH, &g_desktop.work.g_x, &g_desktop.work.g_y,
        &g_desktop.work.g_w, &g_desktop.work.g_h);

    desktop_probe_disks();
    desktop_sort_icons_by_label();
    desktop_install_trash_icon();
    desktop_set_status("Select a disk or the trash can.");
    desktop_init_desktop_tree();
    desktop_layout_icons();
    desktop_redraw(NULL);
    return 1;
}

static void desktop_shutdown(void)
{
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
            }
        }

        if ((event & MU_BUTTON) != 0) {
            WORD object = objc_find(g_desktop.desktop_tree, ROOT, MAX_DEPTH, mx, my);

            if (my < g_desktop.work.g_y) {
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
                desktop_select_icon(NIL);
                desktop_set_status("Select a disk or the trash can.");
                desktop_redraw(&dirty);
            }
        }
    }

    desktop_shutdown();
    return 0;
}
