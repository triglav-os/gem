/*
 * Places desktop icons on a regular grid and lets the user drag them to
 * another cell. Cells are kept in desktop-icons.cfg, keyed by the icon's
 * path. The first move pins every icon where it stands, so the vacated cell
 * stays empty; icons added later flow into free cells in column order.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (C) 2026 tomaz stih
 */

#include "desktop.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum {
    /* Largest grid tracked; a 4K desktop needs 50 x 33 cells. */
    DESKTOP_GRID_MAX_COLS = 64,
    DESKTOP_GRID_MAX_ROWS = 48,
    /* gemconf keys hold at most 127 bytes. */
    DESKTOP_LAYOUT_KEY_MAX = 128
};

static const char desktop_layout_component[] = "desktop-icons";
static const char desktop_layout_section[] = "positions";

static gemconf_t desktop_layout;
static int desktop_layout_loaded;

typedef struct desktop_grid {
    WORD cols;
    WORD rows;
    unsigned char used[DESKTOP_GRID_MAX_ROWS][DESKTOP_GRID_MAX_COLS];
} desktop_grid_t;

static void desktop_layout_load(void)
{
    if (desktop_layout_loaded)
        return;
    gemconf_init(&desktop_layout);
    (void)gemconf_load(&desktop_layout, desktop_layout_component);
    desktop_layout_loaded = 1;
}

/* Stable key for an icon: its path, or a hash when that is too long. */
static void desktop_layout_key(const desktop_icon_entry_t *entry, char *key,
                               size_t size)
{
    const char *path = entry->path;
    unsigned long long hash = 1469598103934665603ull;

    if (entry->launcher != 0) {
        (void)snprintf(key, size, "launcher:%d", entry->launcher);
        return;
    }
    if (strlen(path) < size && strlen(path) < DESKTOP_LAYOUT_KEY_MAX) {
        (void)snprintf(key, size, "%s", path);
        return;
    }
    for (const unsigned char *c = (const unsigned char *)path; *c; ++c)
        hash = (hash ^ *c) * 1099511628211ull;
    (void)snprintf(key, size, "#%016llx", hash);
}

static void desktop_grid_init(desktop_grid_t *grid)
{
    WORD cols = (WORD)((g_desktop.work.g_w - DESKTOP_ICON_MARGIN_X -
                        DESKTOP_ICON_OBJECT_W) /
                           DESKTOP_ICON_STEP_X +
                       1);
    WORD rows = (WORD)((g_desktop.work.g_h - DESKTOP_ICON_MARGIN_Y -
                        DESKTOP_ICON_OBJECT_H) /
                           DESKTOP_ICON_STEP_Y +
                       1);

    memset(grid, 0, sizeof(*grid));
    grid->cols = cols < 1 ? 1 : cols > DESKTOP_GRID_MAX_COLS
                                    ? DESKTOP_GRID_MAX_COLS
                                    : cols;
    grid->rows = rows < 1 ? 1 : rows > DESKTOP_GRID_MAX_ROWS
                                    ? DESKTOP_GRID_MAX_ROWS
                                    : rows;
}

/* Saved cell of an icon; zero when absent, malformed or off the grid. */
static int desktop_saved_cell(const desktop_grid_t *grid,
                              const desktop_icon_entry_t *entry, WORD *col,
                              WORD *row)
{
    char key[DESKTOP_LAYOUT_KEY_MAX];
    const char *value;
    int c;
    int r;
    char tail;

    desktop_layout_load();
    desktop_layout_key(entry, key, sizeof(key));
    value = gemconf_get(&desktop_layout, desktop_layout_section, key, NULL);
    if (value == NULL || sscanf(value, "%d , %d %c", &c, &r, &tail) != 2 ||
        c < 0 || r < 0 || c >= grid->cols || r >= grid->rows)
        return 0;
    *col = (WORD)c;
    *row = (WORD)r;
    return 1;
}

static void desktop_place(WORD object_id, WORD col, WORD row)
{
    g_desktop.desktop_tree[object_id].ob_x =
        (WORD)(DESKTOP_ICON_MARGIN_X + col * DESKTOP_ICON_STEP_X);
    g_desktop.desktop_tree[object_id].ob_y =
        (WORD)(DESKTOP_ICON_MARGIN_Y + row * DESKTOP_ICON_STEP_Y);
}

void desktop_layout_icons(void)
{
    desktop_grid_t grid;
    unsigned char placed[DESKTOP_MAX_ICONS] = {0};
    WORD col = 0;
    WORD row = 0;

    desktop_grid_init(&grid);
    /* Icons the user moved keep their cells; the first claim wins. */
    for (WORD i = 0; i < g_desktop.icon_count; ++i) {
        WORD c;
        WORD r;

        if (desktop_saved_cell(&grid, &g_desktop.icons[i], &c, &r) &&
            !grid.used[r][c]) {
            grid.used[r][c] = 1;
            desktop_place(g_desktop.icons[i].object_id, c, r);
            placed[i] = 1;
        }
    }
    /* Unmoved Trash keeps its classic bottom-right corner. */
    for (WORD i = 0; i < g_desktop.icon_count; ++i) {
        if (placed[i] || g_desktop.icons[i].is_trash == 0)
            continue;
        g_desktop.desktop_tree[g_desktop.icons[i].object_id].ob_x =
            (WORD)(g_desktop.work.g_w - DESKTOP_ICON_OBJECT_W - 12);
        g_desktop.desktop_tree[g_desktop.icons[i].object_id].ob_y =
            (WORD)(g_desktop.work.g_h - DESKTOP_ICON_OBJECT_H - 10);
        placed[i] = 1;
    }
    /* Everything else flows down the columns into free cells. */
    for (WORD i = 0; i < g_desktop.icon_count; ++i) {
        if (placed[i])
            continue;
        while (col < grid.cols && grid.used[row][col]) {
            if (++row >= grid.rows) {
                row = 0;
                ++col;
            }
        }
        if (col >= grid.cols) {
            col = (WORD)(grid.cols - 1);
            row = (WORD)(grid.rows - 1);
        } else {
            grid.used[row][col] = 1;
        }
        desktop_place(g_desktop.icons[i].object_id, col, row);
    }
}

/* Cell under a dropped icon's top-left, moved to the nearest free cell. */
static void desktop_drop_cell(const desktop_icon_entry_t *moved, WORD x,
                              WORD y, WORD *col, WORD *row)
{
    desktop_grid_t grid;
    long best = -1;
    WORD want_col;
    WORD want_row;

    desktop_grid_init(&grid);
    want_col = (WORD)((x - DESKTOP_ICON_MARGIN_X + DESKTOP_ICON_STEP_X / 2) /
                      DESKTOP_ICON_STEP_X);
    want_row = (WORD)((y - DESKTOP_ICON_MARGIN_Y + DESKTOP_ICON_STEP_Y / 2) /
                      DESKTOP_ICON_STEP_Y);
    if (x < DESKTOP_ICON_MARGIN_X)
        want_col = 0;
    if (y < DESKTOP_ICON_MARGIN_Y)
        want_row = 0;
    if (want_col >= grid.cols)
        want_col = (WORD)(grid.cols - 1);
    if (want_row >= grid.rows)
        want_row = (WORD)(grid.rows - 1);
    /* Mark cells held by the other icons, wherever the layout put them. */
    for (WORD i = 0; i < g_desktop.icon_count; ++i) {
        const OBJECT *object =
            &g_desktop.desktop_tree[g_desktop.icons[i].object_id];
        WORD c = (WORD)((object->ob_x - DESKTOP_ICON_MARGIN_X +
                         DESKTOP_ICON_STEP_X / 2) /
                        DESKTOP_ICON_STEP_X);
        WORD r = (WORD)((object->ob_y - DESKTOP_ICON_MARGIN_Y +
                         DESKTOP_ICON_STEP_Y / 2) /
                        DESKTOP_ICON_STEP_Y);

        if (&g_desktop.icons[i] == moved || c < 0 || r < 0 ||
            c >= grid.cols || r >= grid.rows)
            continue;
        grid.used[r][c] = 1;
    }
    *col = want_col;
    *row = want_row;
    for (WORD r = 0; r < grid.rows; ++r) {
        for (WORD c = 0; c < grid.cols; ++c) {
            long dc = c - want_col;
            long dr = r - want_row;
            long distance = dc * dc + dr * dr;

            if (!grid.used[r][c] && (best < 0 || distance < best)) {
                best = distance;
                *col = c;
                *row = r;
            }
        }
    }
}

/* Grid cell of an object placed exactly on the grid; zero otherwise. */
static int desktop_object_cell(WORD object_id, WORD *col, WORD *row)
{
    desktop_grid_t grid;
    const OBJECT *object = &g_desktop.desktop_tree[object_id];
    WORD x = (WORD)(object->ob_x - DESKTOP_ICON_MARGIN_X);
    WORD y = (WORD)(object->ob_y - DESKTOP_ICON_MARGIN_Y);

    desktop_grid_init(&grid);
    if (x < 0 || y < 0 || x % DESKTOP_ICON_STEP_X != 0 ||
        y % DESKTOP_ICON_STEP_Y != 0)
        return 0;
    *col = (WORD)(x / DESKTOP_ICON_STEP_X);
    *row = (WORD)(y / DESKTOP_ICON_STEP_Y);
    return *col < grid.cols && *row < grid.rows;
}

static int desktop_store_cell(const desktop_icon_entry_t *entry, WORD col,
                              WORD row)
{
    char key[DESKTOP_LAYOUT_KEY_MAX];
    char value[24];

    desktop_layout_key(entry, key, sizeof(key));
    (void)snprintf(value, sizeof(value), "%d,%d", col, row);
    return gemconf_set(&desktop_layout, desktop_layout_section, key, value);
}

int desktop_drag_icon(WORD object_id, WORD mx, WORD my)
{
    desktop_icon_entry_t *entry = NULL;
    GRECT rect;
    WORD end_x = 0;
    WORD end_y = 0;
    WORD col;
    WORD row;
    int saved = 1;

    for (WORD i = 0; i < g_desktop.icon_count; ++i)
        if (g_desktop.icons[i].object_id == object_id)
            entry = &g_desktop.icons[i];
    if (entry == NULL)
        return 0;
    desktop_object_rect(object_id, &rect);
    if (!graf_dragbox(rect.g_w, rect.g_h, rect.g_x, rect.g_y,
                      g_desktop.work.g_x, g_desktop.work.g_y,
                      g_desktop.work.g_w, g_desktop.work.g_h, &end_x, &end_y))
        return 0;
    (void)mx;
    (void)my;
    desktop_drop_cell(entry, (WORD)(end_x - g_desktop.work.g_x),
                      (WORD)(end_y - g_desktop.work.g_y), &col, &row);
    desktop_layout_load();
    /* Pin every other gridded icon where it stands, so nothing reflows into
     * the cell this icon leaves. Trash in its corner stays off the grid. */
    for (WORD i = 0; i < g_desktop.icon_count; ++i) {
        WORD c;
        WORD r;

        if (&g_desktop.icons[i] != entry &&
            desktop_object_cell(g_desktop.icons[i].object_id, &c, &r))
            saved &= desktop_store_cell(&g_desktop.icons[i], c, r);
    }
    saved &= desktop_store_cell(entry, col, row);
    if (!saved || !gemconf_save(&desktop_layout))
        desktop_set_status("Icon moved; its position could not be saved.");
    else
        desktop_set_status("Icon moved.");
    desktop_layout_icons();
    desktop_redraw(NULL);
    return 1;
}

void desktop_icon_layout_release(void)
{
    if (desktop_layout_loaded)
        gemconf_clear(&desktop_layout);
    desktop_layout_loaded = 0;
}
