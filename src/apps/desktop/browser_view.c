/*
 * Presents and operates the desktop's browser windows: list and icon
 * view geometry, slider synchronization, scrolling, the work-area redraw
 * and the mouse and window-message handling for each open browser.
 *
 * MIT License (see: LICENSE)
 * Copyright (C) 2026 tomaz stih
 */

#define _POSIX_C_SOURCE 200809L

#include "desktop.h"

#include <string.h>

typedef struct desktop_icon_drag_shape {
    WORD points[18];
    WORD count;
} desktop_icon_drag_shape_t;

typedef struct desktop_drag_selection {
    const desktop_browser_window_t *browser;
    WORD primary_index;
    WORD initial_dx;
    WORD initial_dy;
} desktop_drag_selection_t;

static void desktop_browser_set_drop_target(WORD object, UWORD state);
static void desktop_browser_build_list_drag_shape(
    const GRECT *outline, desktop_icon_drag_shape_t *shape);

static int
desktop_browser_entry_has_prg_icon(const desktop_browser_window_t *browser,
                                   const desktop_browser_entry_t *entry)
{
    if (browser == NULL || entry == NULL || entry->is_dir != 0 ||
        entry->is_parent != 0) {
        return 0;
    }
    return entry->is_app != 0;
}

const char *
desktop_browser_entry_prefix(const desktop_browser_window_t *browser,
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

const desktop_icon_asset_t *
desktop_browser_entry_asset(const desktop_browser_window_t *browser,
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

    return (WORD)(browser->entry_count - page_size);
}

static int desktop_browser_row_rect(const desktop_browser_window_t *browser,
                                    WORD index, GRECT *rect)
{
    WORD visible_row;

    if (browser == NULL || rect == NULL || browser->used == 0 || index < 0 ||
        index >= browser->entry_count) {
        return 0;
    }

    visible_row = (WORD)(index - browser->top_index);
    if (visible_row < 0 || visible_row >= browser->rows_visible) {
        return 0;
    }

    rect->g_x = browser->work.g_x;
    rect->g_y = (WORD)(browser->work.g_y + desktop_browser_band_height() +
                       visible_row * DESKTOP_BROWSER_ROW_H);
    rect->g_w = browser->work.g_w;
    rect->g_h = DESKTOP_BROWSER_ROW_H;
    return 1;
}

static int desktop_browser_icon_rect(const desktop_browser_window_t *browser,
                                     WORD index, GRECT *rect)
{
    WORD visible_index;
    WORD row;
    WORD column;

    if (browser == NULL || rect == NULL || browser->used == 0 || index < 0 ||
        index >= browser->entry_count || browser->columns_visible <= 0 ||
        browser->rows_per_page <= 0) {
        return 0;
    }

    visible_index = (WORD)(index - browser->top_index);
    if (visible_index < 0 || visible_index >= browser->rows_per_page) {
        return 0;
    }

    column = (WORD)(visible_index % browser->columns_visible);
    row = (WORD)(visible_index / browser->columns_visible);
    rect->g_x = (WORD)(browser->work.g_x + DESKTOP_ICON_MARGIN_X +
                       column * DESKTOP_BROWSER_ICON_STEP_X);
    rect->g_y = (WORD)(browser->work.g_y + DESKTOP_ICON_MARGIN_Y +
                       row * DESKTOP_ICON_STEP_Y);
    rect->g_w = DESKTOP_ICON_OBJECT_W;
    rect->g_h = DESKTOP_ICON_OBJECT_H;
    return 1;
}

static WORD desktop_browser_entry_at(const desktop_browser_window_t *browser,
                                     WORD x, WORD y)
{
    WORD index;

    if (browser == NULL || x < browser->work.g_x || y < browser->work.g_y ||
        x >= browser->work.g_x + browser->work.g_w ||
        y >= browser->work.g_y + browser->work.g_h ||
        y >= browser->work.g_y + browser->work.g_h -
                 desktop_browser_band_height()) return NIL;
    if (browser->view_mode == DESKTOP_BROWSER_VIEW_ICONS) {
        WORD relative_x =
            (WORD)(x - browser->work.g_x - DESKTOP_ICON_MARGIN_X);
        WORD relative_y =
            (WORD)(y - browser->work.g_y - DESKTOP_ICON_MARGIN_Y);
        GRECT rect;

        if (relative_x < 0 || relative_y < 0) return NIL;
        index = (WORD)(browser->top_index +
                       (relative_y / DESKTOP_ICON_STEP_Y) *
                           browser->columns_visible +
                       relative_x / DESKTOP_BROWSER_ICON_STEP_X);
        if (desktop_browser_icon_rect(browser, index, &rect) == 0 ||
            x >= rect.g_x + rect.g_w || y >= rect.g_y + rect.g_h) return NIL;
    } else {
        if (y < browser->work.g_y + desktop_browser_band_height()) return NIL;
        index = (WORD)(browser->top_index +
                       (y - browser->work.g_y -
                        desktop_browser_band_height()) /
                           DESKTOP_BROWSER_ROW_H);
    }
    return index >= 0 && index < browser->entry_count ? index : NIL;
}

static void desktop_browser_icon_parts(
    const desktop_browser_window_t *browser, WORD index, const GRECT *object,
    GRECT *icon, GRECT *title)
{
    const desktop_browser_entry_t *entry = &browser->entries[index];
    const desktop_icon_asset_t *asset =
        desktop_browser_entry_asset(browser, entry);
    char label[GEM_OS_PATH_MAX];
    WORD extent[8];
    WORD previous_font;
    WORD text_width;
    WORD text_height;
    WORD max_text_width = (WORD)(DESKTOP_BROWSER_ICON_STEP_X - 4);
    WORD label_y;
    WORD label_left;
    WORD label_right;
    size_t label_len;

    (void)strncpy(label, entry->name, sizeof(label) - 1u);
    label[sizeof(label) - 1u] = '\0';
    icon->g_x = (WORD)(object->g_x + (object->g_w - asset->width) / 2);
    icon->g_y = object->g_y;
    icon->g_w = asset->width;
    icon->g_h = asset->height;

    previous_font = vst_font(g_desktop.vdi_handle, SMALL);
    vqt_extent(g_desktop.vdi_handle, label, extent);
    text_width = (WORD)(extent[2] - extent[0] + 1);
    label_len = strlen(label);
    if (text_width > max_text_width && label_len > 0u) {
        size_t visible_len =
            (size_t)(((LONG)label_len * max_text_width) / text_width);

        if (visible_len < 1u) {
            visible_len = 1u;
        }
        label[visible_len] = '\0';
        vqt_extent(g_desktop.vdi_handle, label, extent);
        text_width = (WORD)(extent[2] - extent[0] + 1);
        while (text_width > max_text_width && visible_len > 1u) {
            label[--visible_len] = '\0';
            vqt_extent(g_desktop.vdi_handle, label, extent);
            text_width = (WORD)(extent[2] - extent[0] + 1);
        }
    }
    text_height = (WORD)(extent[5] - extent[1] + 1);
    label_y = (WORD)(object->g_y + DESKTOP_ICON_TEXT_Y + 14);
    title->g_x = (WORD)(object->g_x + (object->g_w - text_width) / 2 - 2);
    title->g_y = (WORD)(label_y - text_height - 2);
    title->g_w = (WORD)(text_width + 4);
    title->g_h = (WORD)(text_height + 5);
    label_left = (WORD)(object->g_x -
                        (DESKTOP_BROWSER_ICON_STEP_X - object->g_w) / 2);
    label_right =
        (WORD)(label_left + DESKTOP_BROWSER_ICON_STEP_X - 1);
    if (title->g_x < label_left) {
        title->g_w = (WORD)(title->g_w - (label_left - title->g_x));
        title->g_x = label_left;
    }
    if (title->g_x + title->g_w - 1 > label_right) {
        title->g_w = (WORD)(label_right - title->g_x + 1);
    }
    (void)vst_font(g_desktop.vdi_handle, previous_font);
}

static void desktop_browser_build_icon_drag_shape(
    const desktop_browser_window_t *browser, WORD index, const GRECT *object,
    desktop_icon_drag_shape_t *shape)
{
    GRECT icon;
    GRECT title;
    WORD icon_right;
    WORD title_right;
    WORD title_bottom;
    WORD *points = shape->points;

    desktop_browser_icon_parts(browser, index, object, &icon, &title);
    icon_right = (WORD)(icon.g_x + icon.g_w - 1);
    title_right = (WORD)(title.g_x + title.g_w - 1);
    title_bottom = (WORD)(title.g_y + title.g_h - 1);

    /* Icon stem plus title bar: the classic upside-down-T drag contour. */
    points[0] = icon.g_x;
    points[1] = icon.g_y;
    points[2] = icon_right;
    points[3] = icon.g_y;
    points[4] = icon_right;
    points[5] = title.g_y;
    points[6] = title_right;
    points[7] = title.g_y;
    points[8] = title_right;
    points[9] = title_bottom;
    points[10] = title.g_x;
    points[11] = title_bottom;
    points[12] = title.g_x;
    points[13] = title.g_y;
    points[14] = icon.g_x;
    points[15] = title.g_y;
    points[16] = icon.g_x;
    points[17] = icon.g_y;
    shape->count = 9;
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
        size = (WORD)((page_size * 1000) / browser->entry_count);
        if (size < 50) {
            size = 50;
        }
        slider =
            (max_top > 0) ? (WORD)((browser->top_index * 1000) / max_top) : 0;
    }

    (void)wind_set(browser->handle, WF_VSLSIZ, size, 0, 0, 0);
    (void)wind_set(browser->handle, WF_VSLIDE, slider, 0, 0, 0);
}

void desktop_browser_sync_work(desktop_browser_window_t *browser)
{
    if (browser == NULL || browser->used == 0) {
        return;
    }

    wind_get(browser->handle, WF_WORKXYWH, &browser->work.g_x,
             &browser->work.g_y, &browser->work.g_w, &browser->work.g_h);
    browser->rows_visible =
        (WORD)((browser->work.g_h - 2 * desktop_browser_band_height()) /
               DESKTOP_BROWSER_ROW_H);
    if (browser->rows_visible < 1) {
        browser->rows_visible = 1;
    }
    browser->columns_visible =
        (WORD)((browser->work.g_w - DESKTOP_ICON_MARGIN_X) /
               DESKTOP_BROWSER_ICON_STEP_X);
    if (browser->columns_visible < 1) {
        browser->columns_visible = 1;
    }
    browser->rows_per_page =
        (WORD)((browser->work.g_h - desktop_browser_band_height() -
                DESKTOP_ICON_MARGIN_Y) /
               DESKTOP_ICON_STEP_Y);
    if (browser->rows_per_page < 1) {
        browser->rows_per_page = 1;
    }
    browser->rows_per_page =
        (WORD)(browser->rows_per_page * browser->columns_visible);
    desktop_browser_sync_sliders(browser);
}

void desktop_browser_redraw(desktop_browser_window_t *browser,
                            const GRECT *dirty)
{
    GRECT visible;
    WORD clip_xy[4];
    WORD fill[4];

    if (browser == NULL || browser->used == 0) {
        return;
    }

    wind_update(BEG_UPDATE);
    wind_get(browser->handle, WF_FIRSTXYWH, &visible.g_x, &visible.g_y,
             &visible.g_w, &visible.g_h);
    while (visible.g_w > 0 && visible.g_h > 0) {
        WORD x0 = visible.g_x;
        WORD y0 = visible.g_y;
        WORD x1 = (WORD)(visible.g_x + visible.g_w - 1);
        WORD y1 = (WORD)(visible.g_y + visible.g_h - 1);
        WORD row;

        if (dirty != NULL) {
            WORD dx1 = (WORD)(dirty->g_x + dirty->g_w - 1);
            WORD dy1 = (WORD)(dirty->g_y + dirty->g_h - 1);

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
                    WORD index = (WORD)(browser->top_index + slot);
                    GRECT icon_rect;

                    if (index >= browser->entry_count ||
                        desktop_browser_icon_rect(browser, index, &icon_rect) ==
                            0) {
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
                        UWORD state = entry->selected != 0 ? SELECTED : NORMAL;

                        desktop_draw_icon_object(asset, entry->name, &icon_rect,
                                                 state);
                    }
                }
            } else {
                (void)vst_font(g_desktop.vdi_handle, DESKTOP_SYSTEM_FONT);
                for (row = 0; row < browser->rows_visible; ++row) {
                    WORD index = (WORD)(browser->top_index + row);
                    WORD row_y = (WORD)(browser->work.g_y +
                                        desktop_browser_band_height() +
                                        row * DESKTOP_BROWSER_ROW_H);
                    WORD row_bottom = (WORD)(row_y + DESKTOP_BROWSER_ROW_H - 1);

                    if (row_bottom < y0 || row_y > y1) {
                        continue;
                    }

                    if (index < browser->entry_count) {
                        const desktop_browser_entry_t *entry =
                            &browser->entries[index];
                        WORD row_fill[4];

                        row_fill[0] = browser->work.g_x;
                        row_fill[1] = row_y;
                        row_fill[2] =
                            (WORD)(browser->work.g_x + browser->work.g_w - 1);
                        row_fill[3] = row_bottom;

                        if (entry->selected != 0) {
                            vsf_color(g_desktop.vdi_handle, WHITE);
                            vr_recfl(g_desktop.vdi_handle, row_fill);
                            vst_color(g_desktop.vdi_handle, BLACK);
                        } else {
                            vst_color(g_desktop.vdi_handle, WHITE);
                        }

                        desktop_browser_list_draw_row(browser, entry, row_y);
                    }
                }
                /* Paint the fixed header last so rows can never cover it. */
                desktop_browser_list_draw_header(browser, y0, y1);
            }
            desktop_browser_draw_status(browser, y0, y1);

            vs_clip(g_desktop.vdi_handle, 0, clip_xy);
        }

        wind_get(browser->handle, WF_NEXTXYWH, &visible.g_x, &visible.g_y,
                 &visible.g_w, &visible.g_h);
    }

    wind_update(END_UPDATE);
}

static void desktop_browser_scroll_to(desktop_browser_window_t *browser,
                                      WORD top)
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

static int desktop_browser_wait_for_drag(WORD x, WORD y, WORD *mouse_x,
                                         WORD *mouse_y, WORD *key_state)
{
    enum { DRAG_POLL_MS = 10 };
    WORD message[8] = {0};
    WORD buttons = 0;
    WORD keys = 0;
    WORD key = 0;
    WORD clicks = 0;
    WORD event;

    for (;;) {
        event = evnt_multi(
            (UWORD)(MU_BUTTON | MU_M1 | MU_TIMER), 1, 1, 0, 0,
            (WORD)(x - DESKTOP_DRAG_THRESHOLD),
            (WORD)(y - DESKTOP_DRAG_THRESHOLD),
            (WORD)(DESKTOP_DRAG_THRESHOLD * 2 + 1),
            (WORD)(DESKTOP_DRAG_THRESHOLD * 2 + 1), 0, 0, 0, 0, 0, message,
            DRAG_POLL_MS, 0, mouse_x, mouse_y, &buttons, &keys, &key, &clicks);
        if ((event & MU_M1) != 0) {
            if (key_state != NULL) *key_state = keys;
            return (buttons & 1) != 0;
        }
        if ((event & MU_BUTTON) != 0) {
            if (key_state != NULL) *key_state = keys;
            return 0;
        }
    }
}

static WORD desktop_browser_drag_target_at(WORD x, WORD y)
{
    WORD object;
    const desktop_icon_entry_t *icon;

    if (wind_find(x, y) != 0) {
        return NIL;
    }
    object = desktop_find_icon_at(x, y);
    icon = desktop_find_icon(object);
    return icon != NULL && icon->is_trash != 0 ? object : NIL;
}

static void desktop_browser_draw_drag_shape(
    const desktop_icon_drag_shape_t *shape, WORD dx, WORD dy)
{
    WORD attributes[4] = {1, BLACK, MD_REPLACE, 1};
    WORD shifted[18];
    WORD index;

    for (index = 0; index < shape->count; ++index) {
        shifted[index * 2] = (WORD)(shape->points[index * 2] + dx);
        shifted[index * 2 + 1] =
            (WORD)(shape->points[index * 2 + 1] + dy);
    }
    v_hide_c(g_desktop.vdi_handle);
    (void)vql_attributes(g_desktop.vdi_handle, attributes);
    (void)vswr_mode(g_desktop.vdi_handle, MD_XOR);
    (void)vsl_type(g_desktop.vdi_handle, 3);
    (void)vsl_color(g_desktop.vdi_handle, WHITE);
    v_pline(g_desktop.vdi_handle, shape->count, shifted);
    (void)vsl_type(g_desktop.vdi_handle, attributes[0]);
    (void)vsl_color(g_desktop.vdi_handle, attributes[1]);
    (void)vswr_mode(g_desktop.vdi_handle, attributes[2]);
    v_show_c(g_desktop.vdi_handle, 1);
}

static int desktop_browser_drag_shape_for_entry(
    const desktop_drag_selection_t *selection, WORD index, WORD dx, WORD dy,
    desktop_icon_drag_shape_t *shape, GRECT *outline_out)
{
    const desktop_browser_window_t *browser = selection->browser;
    GRECT outline;

    if ((browser->view_mode == DESKTOP_BROWSER_VIEW_ICONS
             ? desktop_browser_icon_rect(browser, index, &outline)
             : desktop_browser_row_rect(browser, index, &outline)) == 0) {
        return 0;
    }
    if (browser->view_mode == DESKTOP_BROWSER_VIEW_LIST &&
        outline.g_w > DESKTOP_BROWSER_ICON_STEP_X) {
        outline.g_w = DESKTOP_BROWSER_ICON_STEP_X;
    }
    outline.g_x = (WORD)(outline.g_x + selection->initial_dx + dx);
    outline.g_y = (WORD)(outline.g_y + selection->initial_dy + dy);
    if (browser->view_mode == DESKTOP_BROWSER_VIEW_ICONS) {
        desktop_browser_build_icon_drag_shape(browser, index, &outline, shape);
    } else {
        desktop_browser_build_list_drag_shape(&outline, shape);
    }
    if (outline_out != NULL) *outline_out = outline;
    return 1;
}

static void desktop_browser_draw_move_marker(const GRECT *outline)
{
    WORD attributes[4] = {1, BLACK, MD_REPLACE, 1};
    WORD marker[10];
    WORD x = (WORD)(outline->g_x + outline->g_w - 10);
    WORD y = (WORD)(outline->g_y + 4);

    /* A compact M badge distinguishes Control-move from the plain copy
     * contour without resembling the conventional plus sign for Copy. */
    marker[0] = x;
    marker[1] = (WORD)(y + 7);
    marker[2] = x;
    marker[3] = y;
    marker[4] = (WORD)(x + 3);
    marker[5] = (WORD)(y + 4);
    marker[6] = (WORD)(x + 6);
    marker[7] = y;
    marker[8] = (WORD)(x + 6);
    marker[9] = (WORD)(y + 7);
    v_hide_c(g_desktop.vdi_handle);
    (void)vql_attributes(g_desktop.vdi_handle, attributes);
    (void)vswr_mode(g_desktop.vdi_handle, MD_XOR);
    (void)vsl_type(g_desktop.vdi_handle, 1);
    (void)vsl_color(g_desktop.vdi_handle, WHITE);
    v_pline(g_desktop.vdi_handle, 5, marker);
    (void)vsl_type(g_desktop.vdi_handle, attributes[0]);
    (void)vsl_color(g_desktop.vdi_handle, attributes[1]);
    (void)vswr_mode(g_desktop.vdi_handle, attributes[2]);
    v_show_c(g_desktop.vdi_handle, 1);
}

static void desktop_browser_draw_drag_selection(
    const desktop_drag_selection_t *selection, WORD dx, WORD dy, int move)
{
    const desktop_browser_window_t *browser = selection->browser;
    GRECT primary_outline;
    WORD index;
    int primary_drawn = 0;

    for (index = 0; index < browser->entry_count; ++index) {
        desktop_icon_drag_shape_t shape;

        if (browser->entries[index].selected == 0 ||
            browser->entries[index].is_parent != 0 ||
            desktop_browser_drag_shape_for_entry(selection, index, dx, dy,
                                                  &shape,
                                                  index == selection->primary_index
                                                      ? &primary_outline
                                                      : NULL) == 0) {
            continue;
        }
        desktop_browser_draw_drag_shape(&shape, 0, 0);
        if (index == selection->primary_index) primary_drawn = 1;
    }
    if (move != 0 && primary_drawn != 0) {
        desktop_browser_draw_move_marker(&primary_outline);
    }
}

static int desktop_browser_move_target_at(
    const desktop_drag_selection_t *selection, WORD x, WORD y,
    WORD key_state)
{
    desktop_browser_window_t *target;
    WORD target_index;

    if ((key_state & K_CTRL) == 0 ||
        selection->browser->backend != DESKTOP_BROWSER_FILES) return 0;
    target = desktop_find_browser_by_handle(wind_find(x, y));
    if (target == NULL || target->backend != DESKTOP_BROWSER_FILES) return 0;
    target_index = desktop_browser_entry_at(target, x, y);
    if (target == selection->browser) {
        return target_index >= 0 && target_index < target->entry_count &&
               target_index != selection->primary_index &&
               target->entries[target_index].is_dir != 0 &&
               target->entries[target_index].selected == 0;
    }
    return 1;
}

static int desktop_browser_track_drag_selection(
    const desktop_drag_selection_t *selection, WORD start_x, WORD start_y,
    WORD mouse_x, WORD mouse_y, WORD *final_x, WORD *final_y,
    WORD initial_keys, WORD *final_keys)
{
    enum { DRAG_POLL_MS = 10 };
    WORD message[8] = {0};
    WORD buttons = 1;
    WORD keys = initial_keys;
    WORD key = 0;
    WORD clicks = 0;
    WORD dx = (WORD)(mouse_x - start_x);
    WORD dy = (WORD)(mouse_y - start_y);
    WORD target = NIL;
    UWORD target_state = NORMAL;
    int move = desktop_browser_move_target_at(selection, mouse_x, mouse_y,
                                              keys);
    WORD event;

    desktop_browser_draw_drag_selection(selection, dx, dy, move);
    for (;;) {
        WORD next_dx;
        WORD next_dy;
        WORD next_target;
        int next_move;

        event = evnt_multi(MU_BUTTON | MU_TIMER, 1, 1, 0, 0, 0, 0, 0, 0, 0,
                           0, 0, 0, 0, message, DRAG_POLL_MS, 0, &mouse_x,
                           &mouse_y, &buttons, &keys, &key, &clicks);
        next_dx = (WORD)(mouse_x - start_x);
        next_dy = (WORD)(mouse_y - start_y);
        next_target = desktop_browser_drag_target_at(mouse_x, mouse_y);
        next_move = desktop_browser_move_target_at(selection, mouse_x, mouse_y,
                                                   keys);
        if (next_dx != dx || next_dy != dy || next_target != target ||
            next_move != move) {
            desktop_browser_draw_drag_selection(selection, dx, dy, move);
            if (target != NIL && next_target != target) {
                desktop_browser_set_drop_target(target, target_state);
                target = NIL;
            }
            if (next_target != NIL && next_target != target) {
                target = next_target;
                target_state = g_desktop.desktop_tree[target].ob_state;
                desktop_browser_set_drop_target(
                    target, (UWORD)(target_state | SELECTED));
            }
            dx = next_dx;
            dy = next_dy;
            move = next_move;
            desktop_browser_draw_drag_selection(selection, dx, dy, move);
        }
        if ((event & MU_BUTTON) != 0 || (buttons & 1) == 0) {
            break;
        }
    }
    desktop_browser_draw_drag_selection(selection, dx, dy, move);
    if (target != NIL) {
        desktop_browser_set_drop_target(target, target_state);
    }
    if (final_x != NULL) {
        *final_x = mouse_x;
    }
    if (final_y != NULL) {
        *final_y = mouse_y;
    }
    if (final_keys != NULL) {
        *final_keys = keys;
    }
    return 1;
}

static void desktop_browser_build_list_drag_shape(
    const GRECT *outline, desktop_icon_drag_shape_t *shape)
{
    WORD right = (WORD)(outline->g_x + outline->g_w - 1);
    WORD bottom = (WORD)(outline->g_y + outline->g_h - 1);

    shape->points[0] = outline->g_x;
    shape->points[1] = outline->g_y;
    shape->points[2] = right;
    shape->points[3] = outline->g_y;
    shape->points[4] = right;
    shape->points[5] = bottom;
    shape->points[6] = outline->g_x;
    shape->points[7] = bottom;
    shape->points[8] = outline->g_x;
    shape->points[9] = outline->g_y;
    shape->count = 5;
}

int desktop_browser_begin_drag(WORD handle, WORD index, WORD x, WORD y)
{
    desktop_browser_window_t *browser = desktop_find_browser_by_handle(handle);
    GRECT outline;
    WORD destination_x;
    WORD destination_y;
    WORD key_state;
    WORD mouse_x;
    WORD mouse_y;
    desktop_drag_selection_t selection;

    if (browser == NULL ||
        (browser->backend != DESKTOP_BROWSER_FILES &&
         browser->backend != DESKTOP_BROWSER_DISK) ||
        index < 0 || index >= browser->entry_count ||
        browser->entries[index].is_parent != 0) {
        return 0;
    }
    g_desktop.drag_browser_handle = handle;
    g_desktop.drag_entry_index = index;
    g_desktop.drag_started = 0;
    if ((browser->view_mode == DESKTOP_BROWSER_VIEW_ICONS
             ? desktop_browser_icon_rect(browser, index, &outline)
             : desktop_browser_row_rect(browser, index, &outline)) == 0) {
        g_desktop.drag_browser_handle = NIL;
        g_desktop.drag_entry_index = NIL;
        return 0;
    }
    if (desktop_browser_wait_for_drag(x, y, &mouse_x, &mouse_y,
                                      &key_state) == 0) {
        g_desktop.drag_browser_handle = NIL;
        g_desktop.drag_entry_index = NIL;
        return 0;
    }
    g_desktop.drag_started = 1;
    selection.browser = browser;
    selection.primary_index = index;
    selection.initial_dx = (WORD)(mouse_x - x);
    selection.initial_dy = (WORD)(mouse_y - y);
    if (desktop_browser_track_drag_selection(
            &selection, mouse_x, mouse_y, mouse_x, mouse_y, &destination_x,
            &destination_y, key_state, &key_state) == 0) {
        g_desktop.drag_browser_handle = NIL;
        g_desktop.drag_entry_index = NIL;
        return 0;
    }
    desktop_browser_end_drag(destination_x, destination_y, key_state);
    return 1;
}

static void desktop_browser_set_drop_target(WORD object, UWORD state)
{
    GRECT dirty;

    desktop_object_rect(object, &dirty);
    desktop_expand_icon_damage_rect(&dirty);
    (void)objc_change(g_desktop.desktop_tree, object, 0, dirty.g_x, dirty.g_y,
                      dirty.g_w, dirty.g_h, state, 0);
    desktop_redraw(&dirty);
}

void desktop_browser_end_drag(WORD x, WORD y, WORD key_state)
{
    desktop_browser_window_t *browser =
        desktop_find_browser_by_handle(g_desktop.drag_browser_handle);
    WORD index = g_desktop.drag_entry_index;
    WORD target_handle;
    WORD target_object;
    desktop_browser_window_t *target_browser;

    g_desktop.drag_browser_handle = NIL;
    g_desktop.drag_entry_index = NIL;
    if (browser == NULL || g_desktop.drag_started == 0 || index < 0 ||
        index >= browser->entry_count) {
        g_desktop.drag_started = 0;
        return;
    }
    g_desktop.drag_started = 0;
    browser->last_click_index = NIL;
    browser->last_click_ms = 0u;
    target_handle = wind_find(x, y);
    target_browser = desktop_find_browser_by_handle(target_handle);
    if (target_browser != NULL) {
        desktop_browser_sync_work(target_browser);
        desktop_browser_drop_entry(
            browser, index, target_browser,
            desktop_browser_entry_at(target_browser, x, y),
            (key_state & K_CTRL) != 0);
        return;
    }
    if (target_handle != 0) {
        return;
    }
    target_object = desktop_find_icon_at(x, y);
    if (target_object != NIL) {
        const desktop_icon_entry_t *icon = desktop_find_icon(target_object);

        if (icon != NULL && icon->is_trash != 0) {
            UWORD original_state =
                g_desktop.desktop_tree[target_object].ob_state;

            desktop_browser_set_drop_target(
                target_object, (UWORD)(original_state | SELECTED));
            desktop_trash_browser_entry(browser, index);
            desktop_browser_set_drop_target(target_object, original_state);
        }
    }
}

void desktop_browser_handle_key(WORD key)
{
    desktop_browser_window_t *browser =
        desktop_find_browser_by_handle(g_desktop.active_browser_handle);
    WORD scan = (WORD)((key >> 8) & 0xff);
    WORD character = (WORD)(key & 0xff);
    WORD next;
    WORD page;
    WORD index;

    if (character == 127 || scan == 0x4c || scan == 0x53 || scan == 0x7f) {
        desktop_delete_active_selection();
        return;
    }
    if (browser == NULL) {
        return;
    }
    if (character == 13) {
        if (browser->selected_index != NIL) {
            desktop_browser_open_entry(browser, browser->selected_index);
        }
        return;
    }
    if (scan != 0x48 && scan != 0x50 && scan != 0x4b && scan != 0x4d) {
        return;
    }
    if (browser->entry_count <= 0) {
        return;
    }
    if (browser->selected_index == NIL) {
        next = (scan == 0x48 || scan == 0x4b)
                   ? (WORD)(browser->entry_count - 1)
                   : 0;
    } else {
        WORD delta = 1;

        if (browser->view_mode == DESKTOP_BROWSER_VIEW_ICONS &&
            (scan == 0x48 || scan == 0x50)) {
            delta = browser->columns_visible;
        }
        next = (WORD)(browser->selected_index +
                      ((scan == 0x48 || scan == 0x4b) ? -delta : delta));
        if (next < 0) {
            next = 0;
        }
        if (next >= browser->entry_count) {
            next = (WORD)(browser->entry_count - 1);
        }
    }
    for (index = 0; index < browser->entry_count; ++index) {
        browser->entries[index].selected = 0;
    }
    browser->entries[next].selected = 1;
    browser->selected_index = next;
    browser->last_click_index = NIL;
    page = browser->view_mode == DESKTOP_BROWSER_VIEW_ICONS
               ? browser->rows_per_page
               : browser->rows_visible;
    if (next < browser->top_index) {
        desktop_browser_scroll_to(browser, next);
    } else if (next >= browser->top_index + page) {
        desktop_browser_scroll_to(browser, (WORD)(next - page + 1));
    } else {
        desktop_browser_redraw(browser, NULL);
    }
    desktop_update_desk_menu_labels();
}

static void desktop_browser_clear_selection(desktop_browser_window_t *browser)
{
    WORD index;

    if (browser == NULL) return;
    for (index = 0; index < browser->entry_count; ++index) {
        browser->entries[index].selected = 0;
    }
    browser->selected_index = NIL;
}

static WORD desktop_browser_first_selected(
    const desktop_browser_window_t *browser)
{
    WORD index;

    if (browser == NULL) return NIL;
    for (index = 0; index < browser->entry_count; ++index) {
        if (browser->entries[index].selected != 0) return index;
    }
    return NIL;
}

void desktop_browser_handle_click(WORD handle, WORD mx, WORD my,
                                  WORD key_state)
{
    desktop_browser_window_t *browser = desktop_find_browser_by_handle(handle);
    WORD index;
    uint32_t now;
    int is_double_click;
    int shifted = (key_state & (K_LSHIFT | K_RSHIFT)) != 0;

    if (browser == NULL) {
        return;
    }

    g_desktop.active_browser_handle = handle;

    desktop_browser_sync_work(browser);
    if (mx < browser->work.g_x || my < browser->work.g_y ||
        mx >= browser->work.g_x + browser->work.g_w ||
        my >= browser->work.g_y + browser->work.g_h) {
        return;
    }
    if (my >= browser->work.g_y + browser->work.g_h -
                  desktop_browser_band_height()) {
        return;
    }

    if (browser->view_mode == DESKTOP_BROWSER_VIEW_ICONS) {
        WORD relative_x =
            (WORD)(mx - browser->work.g_x - DESKTOP_ICON_MARGIN_X);
        WORD relative_y =
            (WORD)(my - browser->work.g_y - DESKTOP_ICON_MARGIN_Y);
        WORD column;
        WORD row;

        if (relative_x < 0 || relative_y < 0) {
            return;
        }
        column = (WORD)(relative_x / DESKTOP_BROWSER_ICON_STEP_X);
        row = (WORD)(relative_y / DESKTOP_ICON_STEP_Y);

        index = (WORD)(browser->top_index + row * browser->columns_visible +
                       column);
        if (index >= 0 && index < browser->entry_count) {
            GRECT hit_rect;

            if (desktop_browser_icon_rect(browser, index, &hit_rect) == 0 ||
                mx >= hit_rect.g_x + hit_rect.g_w ||
                my >= hit_rect.g_y + hit_rect.g_h) {
                if (shifted == 0) {
                    desktop_browser_clear_selection(browser);
                    desktop_browser_redraw(browser, NULL);
                    desktop_update_desk_menu_labels();
                }
                return;
            }
        }
    } else {
        if (my < browser->work.g_y + desktop_browser_band_height()) {
            WORD mode = desktop_browser_list_sort_at(browser, mx);

            if (mode != NIL) {
                desktop_browser_set_sort_mode(mode);
            }
            return;
        }
        index = (WORD)(browser->top_index +
                       (my - browser->work.g_y -
                        desktop_browser_band_height()) /
                           DESKTOP_BROWSER_ROW_H);
    }
    if (index < 0 || index >= browser->entry_count) {
        if (shifted == 0) {
            desktop_browser_clear_selection(browser);
            desktop_browser_redraw(browser, NULL);
            desktop_update_desk_menu_labels();
        }
        return;
    }

    now = gem_os_ticks_ms();
    is_double_click =
        shifted == 0 && browser->last_click_index == index &&
        (uint32_t)(now - browser->last_click_ms) <= DESKTOP_DOUBLE_CLICK_MS;
    if (shifted != 0) {
        browser->entries[index].selected =
            browser->entries[index].selected == 0;
        browser->selected_index = browser->entries[index].selected != 0
                                      ? index
                                      : desktop_browser_first_selected(browser);
        browser->last_click_index = NIL;
        browser->last_click_ms = 0u;
    } else {
        if (is_double_click != 0 || browser->entries[index].selected == 0) {
            desktop_browser_clear_selection(browser);
        }
        browser->entries[index].selected = 1;
        browser->selected_index = index;
    }
    desktop_browser_redraw(browser, NULL);
    desktop_update_desk_menu_labels();

    if (shifted != 0) {
        return;
    }

    /* A press on an already-selected item may be either the second click of
     * a double-click or the start of a drag.  Wait for the drag threshold
     * before opening it; opening immediately makes selected items impossible
     * to drag during the double-click interval. */
    if (browser->entries[index].selected != 0 &&
        desktop_browser_begin_drag(handle, index, mx, my) != 0) {
        return;
    }
    if (is_double_click != 0) {
        browser->last_click_index = NIL;
        browser->last_click_ms = 0u;
        desktop_browser_open_entry(browser, index);
    } else {
        browser->last_click_index = index;
        browser->last_click_ms = now;
    }
}

void desktop_browser_handle_message(WORD msg[8])
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
        case WM_SIZED: {
            GRECT before;
            GRECT after;


            wind_get(browser->handle, WF_PXYWH, &before.g_x, &before.g_y,
                     &before.g_w, &before.g_h);
            (void)wind_set(browser->handle, WF_CURRXYWH, msg[4], msg[5], msg[6],
                           msg[7]);
            wind_get(browser->handle, WF_WXYWH, &after.g_x, &after.g_y,
                     &after.g_w, &after.g_h);
            desktop_browser_sync_work(browser);
            desktop_redraw_window_change(&before, &after);
            desktop_browser_redraw(browser, NULL);
            break;
        }
        case WM_TOPPED:
            (void)wind_set(browser->handle, WF_TOP, 0, 0, 0, 0);
            g_desktop.active_browser_handle = browser->handle;
            break;
        case WM_CLOSED:
            desktop_browser_close(browser);
            break;
        case WM_ARROWED:
            if (browser->view_mode == DESKTOP_BROWSER_VIEW_ICONS) {
                switch (msg[4]) {
                    case WA_UPPAGE:
                        desktop_browser_scroll_to(
                            browser, (WORD)(browser->top_index -
                                            browser->rows_per_page));
                        break;
                    case WA_DNPAGE:
                        desktop_browser_scroll_to(
                            browser, (WORD)(browser->top_index +
                                            browser->rows_per_page));
                        break;
                    case WA_UPLINE:
                        desktop_browser_scroll_to(
                            browser, (WORD)(browser->top_index -
                                            browser->columns_visible));
                        break;
                    case WA_DNLINE:
                        desktop_browser_scroll_to(
                            browser, (WORD)(browser->top_index +
                                            browser->columns_visible));
                        break;
                    default:
                        break;
                }
                break;
            }
            switch (msg[4]) {
                case WA_UPPAGE:
                    desktop_browser_scroll_to(
                        browser,
                        (WORD)(browser->top_index - browser->rows_visible));
                    break;
                case WA_DNPAGE:
                    desktop_browser_scroll_to(
                        browser,
                        (WORD)(browser->top_index + browser->rows_visible));
                    break;
                case WA_UPLINE:
                    desktop_browser_scroll_to(browser,
                                              (WORD)(browser->top_index - 1));
                    break;
                case WA_DNLINE:
                    desktop_browser_scroll_to(browser,
                                              (WORD)(browser->top_index + 1));
                    break;
                default:
                    break;
            }
            break;
        case WM_VSLID: {
            WORD max_top = desktop_browser_max_top(browser);
            WORD top = 0;

            if (max_top > 0) {
                top = (WORD)((msg[4] * max_top) / 1000);
            }
            desktop_browser_scroll_to(browser, top);
            break;
        }
        default:
            break;
    }
}
