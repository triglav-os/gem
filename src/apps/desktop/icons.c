/*
 * Draws and hit-tests the desktop icons: masked icon blitting through the
 * VDI, USERDEF rendering of icon objects, layout in a grid, selection
 * state, status text and click handling including double-click opening.
 *
 * MIT License (see: LICENSE)
 * Copyright (C) 2026 tomaz stih
 */

#define _POSIX_C_SOURCE 200809L

#include "desktop.h"

#include <stdio.h>
#include <string.h>

void desktop_init_object(OBJECT *object, WORD next, WORD head, WORD tail,
                         UWORD type, UWORD flags, UWORD state, LONG spec,
                         WORD x, WORD y, WORD w, WORD h)
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
    return (size_t)((asset->width + 15) / 16);
}

static int desktop_icon_bit(const UWORD *bits,
                            const desktop_icon_asset_t *asset, WORD x, WORD y)
{
    size_t row_words;
    size_t index;
    UWORD word;

    if (bits == NULL || asset == NULL || x < 0 || y < 0 || x >= asset->width ||
        y >= asset->height) {
        return 0;
    }

    row_words = desktop_word_width(asset);
    index = (size_t)y * row_words + (size_t)x / 16u;
    word = bits[index];
    return (word & (WORD)(0x8000u >> (x % 16))) != 0;
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

static void desktop_draw_masked_icon(const desktop_icon_asset_t *asset, WORD x,
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
            desktop_draw_run((WORD)(x + start), (WORD)(y + row),
                             (WORD)(x + col - 1), BLACK);
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
            desktop_draw_run((WORD)(x + start), (WORD)(y + row),
                             (WORD)(x + col - 1), WHITE);
        }
    }
}

const desktop_icon_entry_t *desktop_find_icon(WORD object_id)
{
    WORD i;

    for (i = 0; i < g_desktop.icon_count; ++i) {
        if (g_desktop.icons[i].object_id == object_id) {
            return &g_desktop.icons[i];
        }
    }
    return NULL;
}

int desktop_path_join(const char *dir, const char *name, char *path,
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

    return needed > 0 && (size_t)needed < path_size;
}

void desktop_open_selected_icon(void)
{
    const desktop_icon_entry_t *selected =
        desktop_find_icon(g_desktop.selected_icon);

    if (selected == NULL) {
        desktop_set_status("Open requires an icon selection.");
        desktop_redraw(NULL);
        return;
    }
    if (selected->is_trash != 0) {
        desktop_open_trash();
    } else {
        desktop_open_disk_path(selected->path, selected->label);
    }
}

void desktop_layout_icons(void)
{
    WORD i;
    WORD column = 0;
    WORD row = 0;
    WORD max_rows;

    max_rows = (WORD)((g_desktop.work.g_h - DESKTOP_ICON_MARGIN_Y) /
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

        x = (WORD)(DESKTOP_ICON_MARGIN_X + column * DESKTOP_ICON_STEP_X);
        y = (WORD)(DESKTOP_ICON_MARGIN_Y + row * DESKTOP_ICON_STEP_Y);
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
            (WORD)(g_desktop.work.g_w - DESKTOP_ICON_OBJECT_W - 12);
        g_desktop.desktop_tree[entry->object_id].ob_y =
            (WORD)(g_desktop.work.g_h - DESKTOP_ICON_OBJECT_H - 10);
    }
}

void desktop_draw_icon_object(const desktop_icon_asset_t *asset,
                              const char *label, const GRECT *rect, UWORD state)
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
    WORD text_attributes[10] = {0};
    size_t label_len;

    if (asset == NULL || label == NULL || rect == NULL) {
        return;
    }

    strncpy(display_label, label, sizeof(display_label) - 1u);
    display_label[sizeof(display_label) - 1u] = '\0';

    icon_x = (WORD)(rect->g_x + (rect->g_w - asset->width) / 2);
    icon_y = rect->g_y;
    desktop_draw_masked_icon(asset, icon_x, icon_y);

    previous_font = DESKTOP_SYSTEM_FONT;
    if (vqt_attributes(g_desktop.vdi_handle, text_attributes) != 0) {
        previous_font = text_attributes[0];
    }
    (void)vst_font(g_desktop.vdi_handle, SMALL);
    vqt_extent(g_desktop.vdi_handle, display_label, extent);
    text_width = (WORD)(extent[2] - extent[0] + 1);
    max_text_width = (WORD)(DESKTOP_ICON_STEP_X - 4);
    label_len = strlen(display_label);
    if (text_width > max_text_width && label_len > 0u) {
        size_t visible_len =
            (size_t)(((LONG)label_len * max_text_width) / text_width);

        if (visible_len < 1u) {
            visible_len = 1u;
        }
        display_label[visible_len] = '\0';
        vqt_extent(g_desktop.vdi_handle, display_label, extent);
        text_width = (WORD)(extent[2] - extent[0] + 1);
        while (text_width > max_text_width && visible_len > 1u) {
            display_label[--visible_len] = '\0';
            vqt_extent(g_desktop.vdi_handle, display_label, extent);
            text_width = (WORD)(extent[2] - extent[0] + 1);
        }
    }
    text_height = (WORD)(extent[5] - extent[1] + 1);
    text_x = (WORD)(rect->g_x + (rect->g_w - text_width) / 2);
    label_y = (WORD)(rect->g_y + DESKTOP_ICON_TEXT_Y + 14);
    label_box[0] = (WORD)(text_x - 2);
    label_box[1] = (WORD)(label_y - text_height - 2);
    label_box[2] = (WORD)(text_x + text_width + 1);
    label_box[3] = (WORD)(label_y + 2);
    label_left = (WORD)(rect->g_x - (DESKTOP_ICON_STEP_X - rect->g_w) / 2);
    label_right = (WORD)(label_left + DESKTOP_ICON_STEP_X - 1);
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
    v_gtext(g_desktop.vdi_handle, text_x, label_y, (const BYTE *)display_label);
    (void)vst_font(g_desktop.vdi_handle, previous_font);
}

WORD desktop_user_draw(LONG parm_block)
{
    const PARMBLK *pb = (const PARMBLK *)(intptr_t)parm_block;
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
    desktop_draw_icon_object(entry->draw_info.asset, entry->draw_info.label,
                             &rect, pb->pb_currstate);
    return 0;
}

void desktop_object_rect(WORD object_id, GRECT *rect)
{
    if (rect == NULL) {
        return;
    }

    if (object_id == DESKTOP_ROOT) {
        rect->g_x = g_desktop.desktop_tree[DESKTOP_ROOT].ob_x;
        rect->g_y = g_desktop.desktop_tree[DESKTOP_ROOT].ob_y;
    } else {
        rect->g_x = (WORD)(g_desktop.desktop_tree[DESKTOP_ROOT].ob_x +
                           g_desktop.desktop_tree[object_id].ob_x);
        rect->g_y = (WORD)(g_desktop.desktop_tree[DESKTOP_ROOT].ob_y +
                           g_desktop.desktop_tree[object_id].ob_y);
    }
    rect->g_w = g_desktop.desktop_tree[object_id].ob_width;
    rect->g_h = g_desktop.desktop_tree[object_id].ob_height;
}

void desktop_expand_icon_damage_rect(GRECT *rect)
{
    WORD extra;

    if (rect == NULL || rect->g_w >= DESKTOP_ICON_STEP_X) {
        return;
    }

    extra = (WORD)(DESKTOP_ICON_STEP_X - rect->g_w);
    rect->g_x = (WORD)(rect->g_x - extra / 2);
    rect->g_w = DESKTOP_ICON_STEP_X;
}

WORD desktop_find_icon_at(WORD mx, WORD my)
{
    WORD i;

    for (i = (WORD)(g_desktop.icon_count - 1); i >= 0; --i) {
        const desktop_icon_entry_t *entry = &g_desktop.icons[i];
        GRECT rect;

        desktop_object_rect(entry->object_id, &rect);
        if (mx >= rect.g_x && my >= rect.g_y && mx < rect.g_x + rect.g_w &&
            my < rect.g_y + rect.g_h) {
            return entry->object_id;
        }
        if (i == 0) {
            break;
        }
    }

    return NIL;
}

void desktop_select_icon(WORD object_id)
{
    WORD i;

    g_desktop.selected_icon = object_id;
    for (i = 0; i < g_desktop.icon_count; ++i) {
        g_desktop.desktop_tree[g_desktop.icons[i].object_id].ob_state = NORMAL;
    }
    if (object_id != NIL) {
        g_desktop.desktop_tree[object_id].ob_state = SELECTED;
    }
    desktop_publish_location_selection();
}

static void desktop_status_for_icon(const desktop_icon_entry_t *entry)
{
    char text[96];
    int label_max;

    if (entry == NULL) {
        desktop_set_status("Select a location or the trash can.");
        return;
    }

    if (entry->is_trash != 0) {
        desktop_set_status("Trash selected.");
        return;
    }

    label_max = (int)sizeof(text) - 20;
    (void)snprintf(text, sizeof(text), "%.*s  %llu MB free", label_max,
                   entry->label,
                   (unsigned long long)(entry->avail_bytes / (1024u * 1024u)));
    desktop_set_status(text);
}

void desktop_handle_icon_click(WORD object_id)
{
    const desktop_icon_entry_t *entry = desktop_find_icon(object_id);
    WORD previous = g_desktop.selected_icon;
    GRECT dirty;
    GRECT rect;
    int dirty_set = 0;
    uint32_t now = gem_os_ticks_ms();
    int is_double_click = 0;

    if (g_desktop.last_desktop_click_object == object_id &&
        (uint32_t)(now - g_desktop.last_desktop_click_ms) <=
            DESKTOP_DOUBLE_CLICK_MS) {
        is_double_click = 1;
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
        WORD x1 = (WORD)((dirty.g_x + dirty.g_w - 1) > (rect.g_x + rect.g_w - 1)
                             ? (dirty.g_x + dirty.g_w - 1)
                             : (rect.g_x + rect.g_w - 1));
        WORD y1 = (WORD)((dirty.g_y + dirty.g_h - 1) > (rect.g_y + rect.g_h - 1)
                             ? (dirty.g_y + dirty.g_h - 1)
                             : (rect.g_y + rect.g_h - 1));

        dirty.g_x = x0;
        dirty.g_y = y0;
        dirty.g_w = (WORD)(x1 - x0 + 1);
        dirty.g_h = (WORD)(y1 - y0 + 1);
    }
    desktop_redraw(&dirty);

    if (is_double_click != 0) {
        g_desktop.last_desktop_click_object = NIL;
        g_desktop.last_desktop_click_ms = 0u;
        if (entry != NULL && entry->is_trash != 0) {
            desktop_open_trash();
        } else if (entry != NULL) {
            desktop_open_disk_path(entry->path, entry->label);
        }
    } else {
        g_desktop.last_desktop_click_object = object_id;
        g_desktop.last_desktop_click_ms = now;
    }
}

void desktop_install_trash_icon(void)
{
    desktop_icon_entry_t *entry;

    if (g_desktop.icon_count >= DESKTOP_MAX_ICONS) {
        return;
    }
    entry = &g_desktop.icons[g_desktop.icon_count];
    memset(entry, 0, sizeof(*entry));
    strncpy(entry->label, "Trash", sizeof(entry->label) - 1u);
    strncpy(entry->path, "trash://desktop", sizeof(entry->path) - 1u);
    entry->asset = &desktop_trash_icon_asset;
    entry->draw_info.asset = entry->asset;
    entry->draw_info.label = entry->label;
    entry->userblk.ab_code = (LONG)(intptr_t)desktop_user_draw;
    entry->userblk.ab_parm = (LONG)(intptr_t)&entry->draw_info;
    entry->object_id = (WORD)(DESKTOP_ICON_BASE + g_desktop.icon_count);
    entry->is_trash = 1;
    ++g_desktop.icon_count;
}
