/*
 * Implements the GEM desktop sample's application shell: menu and desktop
 * object trees, Desk menu bindings to open browser windows, the main event
 * loop that routes messages to browsers and icons, and startup/shutdown.
 * Browsers, disks, icons and repainting live in their own modules.
 *
 * MIT License (see: LICENSE)
 * Copyright (C) 2026 tomaz stih
 */

#define _POSIX_C_SOURCE 200809L

#include "desktop.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static desktop_browser_window_t *desktop_browser_for_desk_slot(WORD slot);

desktop_state_t g_desktop;

static void desktop_set_title(OBJECT *tree, WORD title, const char *text,
                              int visible)
{
    if (visible) {
        tree[title].ob_spec = (LONG)(intptr_t)text;
    } else {
        tree[title].ob_spec = (LONG)(intptr_t)"";
        tree[title].ob_width = 0;
    }
}

void desktop_update_desk_menu_labels(void)
{
    static const WORD menu_items[6] = {MENU_DESK_1, MENU_DESK_2, MENU_DESK_3,
                                       MENU_DESK_4, MENU_DESK_5, MENU_DESK_6};
    OBJECT *tree = g_desktop.menu_tree;
    WORD item_index;
    WORD used_count = 0;
    desktop_browser_window_t *active =
        desktop_find_browser_by_handle(g_desktop.active_browser_handle);
    desktop_browser_entry_t *active_entry = NULL;

    if (active != NULL && active->selected_index >= 0 &&
        active->selected_index < active->entry_count) {
        active_entry = &active->entries[active->selected_index];
    }

    for (item_index = 0; item_index < 6; ++item_index) {
        OBJECT *item = &tree[menu_items[item_index]];
        desktop_browser_window_t *browser =
            desktop_browser_for_desk_slot(item_index);

        item->ob_next =
            item_index < 5 ? menu_items[item_index + 1] : MENU_DESK_BOX;

        g_desktop.desk_menu_labels[item_index][0] = '\0';
        if (browser != NULL) {
            (void)snprintf(
                g_desktop.desk_menu_labels[item_index],
                sizeof(g_desktop.desk_menu_labels[item_index]), "  %.*s",
                (int)sizeof(g_desktop.desk_menu_labels[item_index]) - 3,
                browser->title);
            item->ob_spec =
                (LONG)(intptr_t)g_desktop.desk_menu_labels[item_index];
            item->ob_state &= (UWORD)~DISABLED;
            item->ob_flags &= (UWORD)~HIDETREE;
            ++used_count;
        } else {
            item->ob_spec = (LONG)(intptr_t)"";
            item->ob_state |= DISABLED;
            item->ob_flags |= HIDETREE;
        }
    }

    tree[MENU_DESK_BOX].ob_head = MENU_DESK_1;
    tree[MENU_DESK_BOX].ob_tail = MENU_DESK_6;
    tree[MENU_DESK_BOX].ob_height = (WORD)(used_count ? used_count : 1);

    /* Desk lists open file managers; File and Arrange act on them. None of
     * the three belong on the bare desktop, so collapse every title to zero
     * width until at least one file-manager window is open. */
    desktop_set_title(tree, MENU_TITLE_DESK, " Desk ", used_count > 0);
    desktop_set_title(tree, MENU_TITLE_FILE, " File ", used_count > 0);
    desktop_set_title(tree, MENU_TITLE_ARRANGE, " Arrange ", used_count > 0);

    /* Reflect the current view and sort in the Arrange menu ticks. */
    (void)menu_icheck(tree, MENU_ARRANGE_SHOW_AS_ICONS,
                      g_desktop.icon_view ? 1 : 0);
    (void)menu_icheck(tree, MENU_ARRANGE_SORT_NAME,
                      g_desktop.sort_mode == DESKTOP_SORT_NAME);
    (void)menu_icheck(tree, MENU_ARRANGE_SORT_DATE,
                      g_desktop.sort_mode == DESKTOP_SORT_DATE);
    (void)menu_icheck(tree, MENU_ARRANGE_SORT_SIZE,
                      g_desktop.sort_mode == DESKTOP_SORT_SIZE);
    (void)menu_icheck(tree, MENU_ARRANGE_SORT_TYPE,
                      g_desktop.sort_mode == DESKTOP_SORT_TYPE);
    (void)menu_ienable(tree, MENU_FILE_OPEN, active_entry != NULL);
    (void)menu_ienable(tree, MENU_FILE_DELETE,
                       active_entry != NULL && active_entry->is_parent == 0);
    (void)menu_ienable(
        tree, MENU_FILE_RESTORE,
        active != NULL && active->backend == DESKTOP_BROWSER_TRASH &&
            strcmp(active->path, "trash://") == 0 && active_entry != NULL &&
            active_entry->is_trash_root_item != 0);
    (void)menu_ienable(tree, MENU_FILE_EMPTY_TRASH,
                       g_desktop.trash_nonempty != 0);

    if (g_desktop.work.g_w > 0) {
        (void)menu_bar(tree, 1);
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

void desktop_refresh_icon_bindings(void)
{
    WORD i;

    for (i = 0; i < g_desktop.icon_count; ++i) {
        g_desktop.icons[i].draw_info.asset = g_desktop.icons[i].asset;
        g_desktop.icons[i].draw_info.label = g_desktop.icons[i].label;
        g_desktop.icons[i].userblk.ab_code = (LONG)(intptr_t)desktop_user_draw;
        g_desktop.icons[i].userblk.ab_parm =
            (LONG)(intptr_t)&g_desktop.icons[i].draw_info;
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

/*
 * Build the desktop menu bar. Three titles sit on the bar:
 *
 *   Desk     - the list of open file-manager windows (filled in at runtime).
 *   File     - Open plus Trash, restore and permanent-delete commands.
 *   Arrange  - the view toggle ("Show as icons") and the sort radio group
 *              (name / date / size / type).
 *
 * All three belong to the file manager, so they are collapsed to zero width
 * on the bare desktop and only revealed once a browser window is open (see
 * desktop_update_desk_menu_labels). The two leading spaces in each command
 * string reserve the column that menu_icheck draws a tick into.
 */
static void desktop_init_menu_tree(void)
{
    OBJECT *tree = g_desktop.menu_tree;

    desktop_init_object(&tree[MENU_ROOT], -1, MENU_BAR_BOX, MENU_POPUPS, G_IBOX,
                        NONE, NORMAL, 0L, 0, 0, 0, 0);
    desktop_init_object(&tree[MENU_BAR_BOX], MENU_POPUPS, MENU_TITLES,
                        MENU_TITLES, G_BOX, NONE, NORMAL, 0x1100L, 0, 0, 80, 1);
    desktop_init_object(&tree[MENU_TITLES], MENU_BAR_BOX, MENU_TITLE_DESK,
                        MENU_TITLE_ARRANGE, G_IBOX, NONE, NORMAL, 0L, 0, 0, 80,
                        1);
    desktop_init_object(&tree[MENU_POPUPS], MENU_ROOT, MENU_DESK_BOX,
                        MENU_ARRANGE_BOX, G_IBOX, NONE, NORMAL, 0L, 0, 0, 0, 0);

    /* Bar titles. */
    desktop_init_object(&tree[MENU_TITLE_DESK], MENU_TITLE_FILE, NIL, NIL,
                        G_TITLE, NONE, NORMAL, (LONG)(intptr_t)" Desk ", 0, 0,
                        6, 1);
    desktop_init_object(&tree[MENU_TITLE_FILE], MENU_TITLE_ARRANGE, NIL, NIL,
                        G_TITLE, NONE, NORMAL, (LONG)(intptr_t)" File ", 6, 0,
                        6, 1);
    desktop_init_object(&tree[MENU_TITLE_ARRANGE], MENU_TITLES, NIL, NIL,
                        G_TITLE, NONE, NORMAL, (LONG)(intptr_t)" Arrange ", 12,
                        0, 9, 1);

    /* Desk popup: six slots for open file-manager windows, filled in by
     * desktop_update_desk_menu_labels and hidden while unused. */
    desktop_init_object(&tree[MENU_DESK_BOX], MENU_FILE_BOX, MENU_DESK_1,
                        MENU_DESK_6, G_BOX, NONE, NORMAL, 0x1100L, 0, 0, 22, 6);
    desktop_init_object(&tree[MENU_DESK_1], MENU_DESK_2, NIL, NIL, G_STRING,
                        NONE, DISABLED, (LONG)(intptr_t)"", 0, 0, 22, 1);
    desktop_init_object(&tree[MENU_DESK_2], MENU_DESK_3, NIL, NIL, G_STRING,
                        NONE, DISABLED, (LONG)(intptr_t)"", 0, 1, 22, 1);
    desktop_init_object(&tree[MENU_DESK_3], MENU_DESK_4, NIL, NIL, G_STRING,
                        NONE, DISABLED, (LONG)(intptr_t)"", 0, 2, 22, 1);
    desktop_init_object(&tree[MENU_DESK_4], MENU_DESK_5, NIL, NIL, G_STRING,
                        NONE, DISABLED, (LONG)(intptr_t)"", 0, 3, 22, 1);
    desktop_init_object(&tree[MENU_DESK_5], MENU_DESK_6, NIL, NIL, G_STRING,
                        NONE, DISABLED, (LONG)(intptr_t)"", 0, 4, 22, 1);
    desktop_init_object(&tree[MENU_DESK_6], MENU_DESK_BOX, NIL, NIL, G_STRING,
                        NONE, DISABLED, (LONG)(intptr_t)"", 0, 5, 22, 1);

    /* File popup. */
    desktop_init_object(&tree[MENU_FILE_BOX], MENU_ARRANGE_BOX, MENU_FILE_OPEN,
                        MENU_FILE_EMPTY_TRASH, G_BOX, NONE, NORMAL, 0x1100L, 0,
                        0, 18, 4);
    desktop_init_object(&tree[MENU_FILE_OPEN], MENU_FILE_DELETE, NIL, NIL,
                        G_STRING, NONE, NORMAL, (LONG)(intptr_t)"  Open", 0, 0,
                        18, 1);
    desktop_init_object(&tree[MENU_FILE_DELETE], MENU_FILE_RESTORE, NIL, NIL,
                        G_STRING, NONE, NORMAL, (LONG)(intptr_t)"  Delete", 0,
                        1, 18, 1);
    desktop_init_object(&tree[MENU_FILE_RESTORE], MENU_FILE_EMPTY_TRASH, NIL,
                        NIL, G_STRING, NONE, NORMAL,
                        (LONG)(intptr_t)"  Restore", 0, 2, 18, 1);
    desktop_init_object(&tree[MENU_FILE_EMPTY_TRASH], MENU_FILE_BOX, NIL, NIL,
                        G_STRING, NONE, NORMAL, (LONG)(intptr_t)"  Empty Trash",
                        0, 3, 18, 1);

    /* Arrange popup: view toggle, a separator, then the sort radio group. */
    desktop_init_object(&tree[MENU_ARRANGE_BOX], MENU_POPUPS,
                        MENU_ARRANGE_SHOW_AS_ICONS, MENU_ARRANGE_SORT_TYPE,
                        G_BOX, NONE, NORMAL, 0x1100L, 0, 0, 20, 6);
    desktop_init_object(&tree[MENU_ARRANGE_SHOW_AS_ICONS],
                        MENU_ARRANGE_SEPARATOR, NIL, NIL, G_STRING, NONE,
                        NORMAL, (LONG)(intptr_t)"  Show as icons", 0, 0, 20, 1);
    desktop_init_object(&tree[MENU_ARRANGE_SEPARATOR], MENU_ARRANGE_SORT_NAME,
                        NIL, NIL, G_STRING, NONE, DISABLED,
                        (LONG)(intptr_t)"-------------------", 0, 1, 20, 1);
    desktop_init_object(&tree[MENU_ARRANGE_SORT_NAME], MENU_ARRANGE_SORT_DATE,
                        NIL, NIL, G_STRING, NONE, NORMAL,
                        (LONG)(intptr_t)"  Sort by name", 0, 2, 20, 1);
    desktop_init_object(&tree[MENU_ARRANGE_SORT_DATE], MENU_ARRANGE_SORT_SIZE,
                        NIL, NIL, G_STRING, NONE, NORMAL,
                        (LONG)(intptr_t)"  Sort by date", 0, 3, 20, 1);
    desktop_init_object(&tree[MENU_ARRANGE_SORT_SIZE], MENU_ARRANGE_SORT_TYPE,
                        NIL, NIL, G_STRING, NONE, NORMAL,
                        (LONG)(intptr_t)"  Sort by size", 0, 4, 20, 1);
    desktop_init_object(&tree[MENU_ARRANGE_SORT_TYPE], MENU_ARRANGE_BOX, NIL,
                        NIL, G_STRING, LASTOB, NORMAL,
                        (LONG)(intptr_t)"  Sort by type", 0, 5, 20, 1);
}

static void desktop_init_desktop_tree(void)
{
    WORD i;
    WORD last_object = DESKTOP_ROOT;

    desktop_reset_tree_links();
    desktop_init_object(&g_desktop.desktop_tree[DESKTOP_ROOT], NIL, NIL, NIL,
                        G_IBOX, NONE, NORMAL, 0L, g_desktop.work.g_x,
                        g_desktop.work.g_y, g_desktop.work.g_w,
                        g_desktop.work.g_h);

    for (i = 0; i < g_desktop.icon_count; ++i) {
        desktop_icon_entry_t *entry = &g_desktop.icons[i];

        desktop_init_object(&g_desktop.desktop_tree[entry->object_id],
                            entry->object_id, NIL, NIL, G_USERDEF, SELECTABLE,
                            NORMAL, (LONG)(intptr_t)&entry->userblk, 0, 0,
                            DESKTOP_ICON_OBJECT_W, DESKTOP_ICON_OBJECT_H);
        objc_add(g_desktop.desktop_tree, DESKTOP_ROOT, entry->object_id);
        last_object = entry->object_id;
    }

    g_desktop.desktop_tree[last_object].ob_flags |= LASTOB;
}

/*
 * Re-apply the current view and sort settings to every open file manager and
 * repaint it. Pass resort when the sort order changed; the view mode is always
 * refreshed so "Show as icons" takes effect immediately.
 */
static void desktop_refresh_browsers(int resort)
{
    WORD i;

    for (i = 0; i < DESKTOP_BROWSER_MAX_WINDOWS; ++i) {
        desktop_browser_window_t *browser = &g_desktop.browsers[i];

        if (browser->used == 0) {
            continue;
        }
        if (resort) {
            desktop_browser_resort(browser);
        }
        browser->view_mode = g_desktop.icon_view ? DESKTOP_BROWSER_VIEW_ICONS
                                                 : DESKTOP_BROWSER_VIEW_LIST;
        if (browser->view_mode == DESKTOP_BROWSER_VIEW_LIST) {
            WORD entry_index;

            for (entry_index = 0; entry_index < browser->entry_count;
                 ++entry_index) {
                browser->entries[entry_index].selected = 0;
            }
            browser->selected_index = NIL;
            browser->last_click_index = NIL;
            browser->last_click_ms = 0u;
        }
        browser->top_index = 0;
        desktop_browser_sync_work(browser);
        desktop_browser_redraw(browser, NULL);
    }
}

void desktop_browser_set_sort_mode(WORD mode)
{
    static const char *const sort_names[] = {"name", "date", "size", "type"};
    char text[96];

    if (mode < DESKTOP_SORT_NAME || mode > DESKTOP_SORT_TYPE) {
        return;
    }

    g_desktop.sort_mode = mode;
    desktop_refresh_browsers(1);
    desktop_update_desk_menu_labels();
    (void)snprintf(text, sizeof(text), "Sorted by %s.", sort_names[mode]);
    desktop_set_status(text);
    desktop_redraw(NULL);
}

static void desktop_handle_menu_item(WORD item)
{
    char text[96];

    switch (item) {
        case MENU_DESK_1:
        case MENU_DESK_2:
        case MENU_DESK_3:
        case MENU_DESK_4:
        case MENU_DESK_5:
        case MENU_DESK_6: {
            desktop_browser_window_t *browser =
                desktop_browser_for_desk_slot((WORD)(item - MENU_DESK_1));

            if (browser != NULL) {
                (void)wind_set(browser->handle, WF_TOP, 0, 0, 0, 0);
                g_desktop.active_browser_handle = browser->handle;
                (void)snprintf(text, sizeof(text), "Window selected: %.*s",
                               (int)sizeof(text) - 18, browser->title);
                desktop_set_status(text);
            } else {
                desktop_set_status("No window in that Desk slot.");
            }
            break;
        }
        case MENU_FILE_OPEN: {
            desktop_browser_window_t *browser =
                desktop_find_browser_by_handle(g_desktop.active_browser_handle);

            if (browser != NULL && browser->selected_index >= 0 &&
                browser->selected_index < browser->entry_count) {
                desktop_browser_open_entry(browser, browser->selected_index);
            } else {
                desktop_set_status("Select a File Manager item first.");
            }
        } break;
        case MENU_FILE_DELETE:
            desktop_delete_active_selection();
            break;
        case MENU_FILE_RESTORE:
            desktop_restore_active_selection();
            break;
        case MENU_FILE_EMPTY_TRASH:
            desktop_empty_trash();
            break;
        case MENU_ARRANGE_SHOW_AS_ICONS:
            /* One toggle drives both views; the tick tracks the state. */
            g_desktop.icon_view = g_desktop.icon_view ? 0 : 1;
            desktop_refresh_browsers(0);
            desktop_update_desk_menu_labels();
            desktop_set_status(g_desktop.icon_view
                                   ? "Showing entries as icons."
                                   : "Showing entries as text.");
            break;
        case MENU_ARRANGE_SORT_NAME:
        case MENU_ARRANGE_SORT_DATE:
        case MENU_ARRANGE_SORT_SIZE:
        case MENU_ARRANGE_SORT_TYPE: {
            WORD mode = (WORD)(item - MENU_ARRANGE_SORT_NAME);

            desktop_browser_set_sort_mode(mode);
            return;
        }
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
    g_desktop.active_browser_handle = NIL;
    g_desktop.drag_browser_handle = NIL;
    g_desktop.drag_entry_index = NIL;
    g_desktop.locations_menu_id = NIL;
    g_desktop.remove_location_menu_id = NIL;
    gemconf_init(&g_desktop.config);

    if (gem_os_init() == 0) {
        gemconf_clear(&g_desktop.config);
        return 0;
    }

    g_desktop.app_id = appl_init();
    if (g_desktop.app_id <= 0) {
        gemconf_clear(&g_desktop.config);
        gem_os_shutdown();
        return 0;
    }
    (void)gemconf_load(&g_desktop.config, "desktop");
    g_desktop.locations_menu_id =
        menu_register(g_desktop.app_id, "Add Location...");
    g_desktop.remove_location_menu_id =
        menu_register(g_desktop.app_id, "Remove Location...");

    g_desktop.vdi_handle =
        graf_handle(&g_desktop.char_width, &g_desktop.char_height,
                    &g_desktop.box_width, &g_desktop.box_height);
    v_opnvwk(work_in, &g_desktop.vdi_handle, work_out);
    if (g_desktop.vdi_handle == 0) {
        appl_exit();
        gemconf_clear(&g_desktop.config);
        gem_os_shutdown();
        return 0;
    }

    g_desktop.screen_width = (WORD)(work_out[0] + 1);
    g_desktop.screen_height = (WORD)(work_out[1] + 1);

    desktop_init_menu_tree();
    /* Normalize the bare-desktop menu before its first server-side draw.
     * Otherwise Desk/File/Arrange briefly use their template widths and then
     * collapse when the open-browser count is evaluated below. */
    desktop_update_desk_menu_labels();
    menu_click(1, 1);
    menu_bar(g_desktop.menu_tree, 1);
    desktop_probe_disks();
    desktop_sort_icons_by_label();
    desktop_install_trash_icon();
    desktop_refresh_trash_availability();
    desktop_update_desk_menu_labels();
    wind_get(0, WF_WORKXYWH, &g_desktop.work.g_x, &g_desktop.work.g_y,
             &g_desktop.work.g_w, &g_desktop.work.g_h);
    desktop_set_status("Select a location or the trash can.");
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
    if (g_desktop.locations_menu_id != NIL) {
        (void)menu_unregister(g_desktop.locations_menu_id);
    }
    if (g_desktop.remove_location_menu_id != NIL) {
        (void)menu_unregister(g_desktop.remove_location_menu_id);
    }
    if (g_desktop.vdi_handle != 0) {
        v_clsvwk(g_desktop.vdi_handle);
    }
    if (g_desktop.app_id >= 0) {
        appl_exit();
    }
    gemconf_clear(&g_desktop.config);
    gem_os_shutdown();
}

int main(void)
{
    WORD done = 0;

    if (desktop_init() == 0) {
        return 1;
    }

    while (done == 0) {
        WORD msg[8] = {0};
        WORD mx = 0;
        WORD my = 0;
        WORD mb = 0;
        WORD ks = 0;
        WORD kr = 0;
        WORD br = 0;
        WORD event;

        event = evnt_multi((UWORD)(MU_MESAG | MU_BUTTON | MU_KEYBD), 1, 1, 1, 0,
                           0, 0, 0, 0, 0, 0, 0, 0, 0, msg, 0, 0, &mx, &my, &mb,
                           &ks, &kr, &br);
        (void)mb;
        (void)ks;
        (void)br;

        if ((event & MU_KEYBD) != 0) {
            WORD ch = (WORD)(kr & 0xff);

            WORD scan = (WORD)((kr >> 8) & 0xff);
#ifndef GEM_DESKTOP_HOSTED
            (void)scan;
#endif

            if (ch == 27 || ch == 'q' || ch == 'Q') {
                done = 1;
            }
            else if (ch == 13 && g_desktop.selected_icon != NIL &&
                     g_desktop.active_browser_handle == NIL) {
                desktop_open_selected_icon();
            }
#ifdef GEM_DESKTOP_HOSTED
            else if (ch == 127 || scan == 0x4c || scan == 0x53 ||
                     scan == 0x7f) {
                if (g_desktop.selected_icon != NIL) {
                    desktop_delete_selected_location();
                } else {
                    desktop_delete_active_selection();
                }
            }
#endif
            else {
                desktop_browser_handle_key(kr);
            }
        }

        if ((event & MU_MESAG) != 0) {
            if (msg[0] == AC_OPEN && msg[3] == g_desktop.locations_menu_id) {
                desktop_choose_location();
            } else if (msg[0] == AC_OPEN &&
                       msg[3] == g_desktop.remove_location_menu_id) {
                desktop_delete_selected_location();
            }
#ifndef GEM_DESKTOP_HOSTED
            else if (msg[0] == GEM_MSG_LOCATION_CHANGED) {
                desktop_refresh_locations();
                desktop_set_status("Location removed.");
            }
#endif
            else if (msg[0] == MN_SELECTED) {
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
                desktop_browser_handle_click(handle, mx, my, ks);
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
                desktop_set_status("Select a location or the trash can.");
                desktop_redraw(&dirty);
            }
        }
    }

    desktop_shutdown();
    return 0;
}
