/*
 * Lays out and paints the modern GEM file chooser. The monochrome interface
 * uses standard GEM geometry: toolbar buttons, an inset path, Locations,
 * file columns, filename field, scroll controls and modal action buttons.
 *
 * MIT License (see: LICENSE)
 * Copyright (C) 2026 tomaz stih
 */

#include "fsel_private.h"

#include <stdio.h>
#include <string.h>

enum { AES_FSEL_TOOL_BUTTON = 24 };

static void aes_fsel_line(WORD x0, WORD y0, WORD x1, WORD y1)
{
    WORD points[4] = {x0, y0, x1, y1};

    v_pline(aes_state.vdi_handle, 2, points);
}

static void aes_fsel_outline(const GRECT *rect)
{
    vsl_color(aes_state.vdi_handle, aes_dark_color());
    aes_fsel_line(rect->g_x, rect->g_y, (WORD)(rect->g_x + rect->g_w - 1),
                  rect->g_y);
    aes_fsel_line(rect->g_x, (WORD)(rect->g_y + rect->g_h - 1),
                  (WORD)(rect->g_x + rect->g_w - 1),
                  (WORD)(rect->g_y + rect->g_h - 1));
    aes_fsel_line(rect->g_x, rect->g_y, rect->g_x,
                  (WORD)(rect->g_y + rect->g_h - 1));
    aes_fsel_line((WORD)(rect->g_x + rect->g_w - 1), rect->g_y,
                  (WORD)(rect->g_x + rect->g_w - 1),
                  (WORD)(rect->g_y + rect->g_h - 1));
}

static void aes_fsel_text(WORD x, WORD y, const char *text, WORD max_width)
{
    char visible[AES_FSEL_LINE_LEN];
    size_t count;

    if (text == NULL || max_width <= 0) {
        return;
    }
    count = (size_t)(max_width / 6);
    if (count >= sizeof(visible)) {
        count = sizeof(visible) - 1u;
    }
    strncpy(visible, text, count);
    visible[count] = '\0';
    v_gtext(aes_state.vdi_handle, x, y, (CONST BYTE *)visible);
}

static void aes_fsel_draw_icon(WORD x, WORD y, int directory)
{
    if (directory != 0) {
        aes_fsel_line(x, (WORD)(y + 4), (WORD)(x + 4), (WORD)(y + 4));
        aes_fsel_line(x, (WORD)(y + 4), x, (WORD)(y + 12));
        aes_fsel_line((WORD)(x + 4), (WORD)(y + 2), (WORD)(x + 8),
                      (WORD)(y + 4));
        aes_fsel_line((WORD)(x + 4), (WORD)(y + 2), (WORD)(x + 4),
                      (WORD)(y + 4));
        aes_fsel_line(x, (WORD)(y + 12), (WORD)(x + 13), (WORD)(y + 12));
        aes_fsel_line((WORD)(x + 13), (WORD)(y + 4), (WORD)(x + 13),
                      (WORD)(y + 12));
        aes_fsel_line((WORD)(x + 4), (WORD)(y + 4), (WORD)(x + 13),
                      (WORD)(y + 4));
    } else {
        aes_fsel_line((WORD)(x + 2), (WORD)(y + 1), (WORD)(x + 9),
                      (WORD)(y + 1));
        aes_fsel_line((WORD)(x + 2), (WORD)(y + 1), (WORD)(x + 2),
                      (WORD)(y + 13));
        aes_fsel_line((WORD)(x + 2), (WORD)(y + 13), (WORD)(x + 12),
                      (WORD)(y + 13));
        aes_fsel_line((WORD)(x + 12), (WORD)(y + 5), (WORD)(x + 12),
                      (WORD)(y + 13));
        aes_fsel_line((WORD)(x + 9), (WORD)(y + 1), (WORD)(x + 12),
                      (WORD)(y + 5));
        aes_fsel_line((WORD)(x + 9), (WORD)(y + 1), (WORD)(x + 9),
                      (WORD)(y + 5));
        aes_fsel_line((WORD)(x + 9), (WORD)(y + 5), (WORD)(x + 12),
                      (WORD)(y + 5));
    }
}

static void aes_fsel_draw_button(const aes_fsel_state_t *state, WORD control,
                                 const char *label)
{
    GRECT rect;
    WORD fill[4];
    WORD selected = state->pressed_control == control;
    WORD width = vdi_string_width(label);

    aes_fsel_control_rect(state, control, &rect);
    fill[0] = rect.g_x;
    fill[1] = rect.g_y;
    fill[2] = (WORD)(rect.g_x + rect.g_w - 1);
    fill[3] = (WORD)(rect.g_y + rect.g_h - 1);
    vsf_color(aes_state.vdi_handle,
              selected ? aes_dark_color() : aes_light_color());
    vr_recfl(aes_state.vdi_handle, fill);
    aes_fsel_outline(&rect);
    vst_color(aes_state.vdi_handle,
              selected ? aes_light_color() : aes_dark_color());
    v_gtext(
        aes_state.vdi_handle, (WORD)(rect.g_x + (rect.g_w - width) / 2),
        (WORD)(rect.g_y + (rect.g_h - state->char_h) / 2 + state->text_ascent),
        (CONST BYTE *)label);
}

static void aes_fsel_draw_toolbar(const aes_fsel_state_t *state)
{
    GRECT path;
    WORD i;
    static const char *const labels[] = {"<", ">", "^"};

    for (i = 0; i < 3; ++i) {
        aes_fsel_draw_button(state, (WORD)(AES_FSEL_CONTROL_BACK + i),
                             labels[i]);
    }
    aes_fsel_path_rect(state, &path);
    aes_fsel_outline(&path);
    vst_color(aes_state.vdi_handle, aes_dark_color());
    aes_fsel_text(
        (WORD)(path.g_x + 5),
        (WORD)(path.g_y + (path.g_h - state->char_h) / 2 + state->text_ascent),
        state->directory, (WORD)(path.g_w - 10));
    if (gemctl_text_edit_active(&state->path_edit)) {
        WORD width = vdi_string_width(state->directory);
        WORD cursor_x = (WORD)(path.g_x + 5 + width);

        if (cursor_x < path.g_x + path.g_w - 3) {
            aes_fsel_line(cursor_x, (WORD)(path.g_y + 4), cursor_x,
                          (WORD)(path.g_y + path.g_h - 5));
        }
    }
}

static void aes_fsel_draw_locations(const aes_fsel_state_t *state)
{
    GRECT pane;
    WORD row;

    aes_fsel_location_rect(state, &pane);
    aes_fsel_outline(&pane);
    vst_color(aes_state.vdi_handle, aes_dark_color());
    aes_fsel_text((WORD)(pane.g_x + 4), (WORD)(pane.g_y + state->text_ascent),
                  "Locations", (WORD)(pane.g_w - 8));
    aes_fsel_line(pane.g_x, (WORD)(pane.g_y + state->row_h),
                  (WORD)(pane.g_x + pane.g_w - 1),
                  (WORD)(pane.g_y + state->row_h));
    for (row = 0; row < state->location_count; ++row) {
        WORD y = (WORD)(aes_fsel_rows_top(state) + row * state->row_h);
        WORD selected = state->selected_location == row;
        WORD fill[4] = {(WORD)(pane.g_x + 1), y,
                        (WORD)(pane.g_x + pane.g_w - 2),
                        (WORD)(y + state->row_h - 1)};

        if (y + state->row_h > pane.g_y + pane.g_h) {
            break;
        }
        if (selected) {
            vsf_color(aes_state.vdi_handle, aes_dark_color());
            vr_recfl(aes_state.vdi_handle, fill);
        }
        vst_color(aes_state.vdi_handle,
                  selected ? aes_light_color() : aes_dark_color());
        vsl_color(aes_state.vdi_handle,
                  selected ? aes_light_color() : aes_dark_color());
        aes_fsel_draw_icon((WORD)(pane.g_x + 4), (WORD)(y + 1), 1);
        aes_fsel_text((WORD)(pane.g_x + 23), (WORD)(y + state->text_ascent),
                      state->locations[row].label, (WORD)(pane.g_w - 27));
    }
}

static void aes_fsel_size_text(uint64_t bytes, char text[24])
{
    if (bytes >= 1024u * 1024u) {
        (void)snprintf(text, 24, "%llu MB",
                       (unsigned long long)(bytes / (1024u * 1024u)));
    } else if (bytes >= 1024u) {
        (void)snprintf(text, 24, "%llu KB",
                       (unsigned long long)(bytes / 1024u));
    } else {
        (void)snprintf(text, 24, "%llu B", (unsigned long long)bytes);
    }
}

static void aes_fsel_draw_files(const aes_fsel_state_t *state)
{
    GRECT file;
    WORD row;
    WORD content_right;
    WORD size_x;
    WORD type_x;

    aes_fsel_file_rect(state, &file);
    aes_fsel_outline(&file);
    content_right = (WORD)(file.g_x + file.g_w);
    size_x = (WORD)(content_right - 68);
    type_x = (WORD)(size_x - 76);
    vst_color(aes_state.vdi_handle, aes_dark_color());
    aes_fsel_text((WORD)(file.g_x + 22), (WORD)(file.g_y + state->text_ascent),
                  "Name", (WORD)(type_x - file.g_x - 24));
    aes_fsel_text((WORD)(type_x + 3), (WORD)(file.g_y + state->text_ascent),
                  "Type", 70);
    aes_fsel_text((WORD)(size_x + 3), (WORD)(file.g_y + state->text_ascent),
                  "Size", 62);
    aes_fsel_line(file.g_x, (WORD)(file.g_y + state->row_h),
                  (WORD)(file.g_x + file.g_w - 1),
                  (WORD)(file.g_y + state->row_h));
    aes_fsel_line(type_x, file.g_y, type_x, (WORD)(file.g_y + file.g_h - 1));
    aes_fsel_line(size_x, file.g_y, size_x, (WORD)(file.g_y + file.g_h - 1));

    for (row = 0; row < state->visible_rows; ++row) {
        WORD index = (WORD)(state->top_index + row);
        WORD y = (WORD)(aes_fsel_rows_top(state) + row * state->row_h);
        WORD selected = index < state->entry_count && index == state->selected;
        WORD fill[4] = {(WORD)(file.g_x + 1), y, (WORD)(content_right - 1),
                        (WORD)(y + state->row_h - 1)};

        if (index >= state->entry_count || y >= aes_fsel_body_bottom(state)) {
            break;
        }
        if (selected) {
            vsf_color(aes_state.vdi_handle, aes_dark_color());
            vr_recfl(aes_state.vdi_handle, fill);
        }
        vst_color(aes_state.vdi_handle,
                  selected ? aes_light_color() : aes_dark_color());
        vsl_color(aes_state.vdi_handle,
                  selected ? aes_light_color() : aes_dark_color());
        aes_fsel_draw_icon((WORD)(file.g_x + 4), (WORD)(y + 1),
                           state->entries[index].is_directory);
        aes_fsel_text((WORD)(file.g_x + 22), (WORD)(y + state->text_ascent),
                      state->entries[index].name,
                      (WORD)(type_x - file.g_x - 25));
        aes_fsel_text((WORD)(type_x + 3), (WORD)(y + state->text_ascent),
                      state->entries[index].is_directory ? "Folder" : "File",
                      70);
        if (state->entries[index].is_directory == 0) {
            char size[24];

            aes_fsel_size_text(state->entries[index].size_bytes, size);
            aes_fsel_text((WORD)(size_x + 3), (WORD)(y + state->text_ascent),
                          size, 62);
        }
    }
}

static void aes_fsel_draw_actions(const aes_fsel_state_t *state)
{
    const char *accept = state->mode == AES_FSEL_SAVE        ? "Save"
                         : (state->mode == AES_FSEL_DIRECTORY ||
                            state->mode == AES_FSEL_LOCATION) ? "Select"
                         : "Open";

    if (state->mode != AES_FSEL_DIRECTORY) {
        GRECT name;

        aes_fsel_name_rect(state, &name);
        vst_color(aes_state.vdi_handle, aes_dark_color());
        v_gtext(aes_state.vdi_handle, (WORD)(state->work.g_x + AES_FSEL_MARGIN),
                (WORD)(name.g_y + (name.g_h - state->char_h) / 2 +
                       state->text_ascent),
                (CONST BYTE *)"Name:");
        aes_fsel_outline(&name);
        aes_fsel_text((WORD)(name.g_x + 4),
                      (WORD)(name.g_y + (name.g_h - state->char_h) / 2 +
                             state->text_ascent),
                      state->selection, (WORD)(name.g_w - 8));
        if (state->editing_name != 0) {
            WORD width = vdi_string_width(state->selection);
            WORD cursor_x = (WORD)(name.g_x + 4 + width);

            if (cursor_x < name.g_x + name.g_w - 3) {
                aes_fsel_line(cursor_x, (WORD)(name.g_y + 4), cursor_x,
                              (WORD)(name.g_y + name.g_h - 5));
            }
        }
    }
    aes_fsel_draw_button(state, AES_FSEL_CONTROL_ACCEPT, accept);
    aes_fsel_draw_button(state, AES_FSEL_CONTROL_CANCEL, "Cancel");
}

void aes_fsel_draw(const aes_fsel_state_t *state, const GRECT *dirty)
{
    GRECT visible;

    if (state == NULL || aes_ensure_vdi() == 0) {
        return;
    }
    wind_update(BEG_UPDATE);
    wind_get(state->handle, WF_FIRSTXYWH, &visible.g_x, &visible.g_y,
             &visible.g_w, &visible.g_h);
    while (visible.g_w > 0 && visible.g_h > 0) {
        GRECT clip_rect = visible;

        if ((dirty == NULL ||
             aes_intersect_rects(&visible, dirty, &clip_rect)) &&
            clip_rect.g_w > 0 && clip_rect.g_h > 0) {
            WORD clip[4] = {clip_rect.g_x, clip_rect.g_y,
                            (WORD)(clip_rect.g_x + clip_rect.g_w - 1),
                            (WORD)(clip_rect.g_y + clip_rect.g_h - 1)};
            WORD fill[4] = {state->work.g_x, state->work.g_y,
                            (WORD)(state->work.g_x + state->work.g_w - 1),
                            (WORD)(state->work.g_y + state->work.g_h - 1)};

            vs_clip(aes_state.vdi_handle, 1, clip);
            vswr_mode(aes_state.vdi_handle, MD_REPLACE);
            vsf_interior(aes_state.vdi_handle, FIS_SOLID);
            vsf_color(aes_state.vdi_handle, aes_light_color());
            vsl_width(aes_state.vdi_handle, 1);
            vr_recfl(aes_state.vdi_handle, fill);
            aes_fsel_draw_toolbar(state);
            aes_fsel_draw_locations(state);
            aes_fsel_draw_files(state);
            aes_fsel_draw_actions(state);
            vs_clip(aes_state.vdi_handle, 0, clip);
        }
        wind_get(state->handle, WF_NEXTXYWH, &visible.g_x, &visible.g_y,
                 &visible.g_w, &visible.g_h);
    }
    wind_update(END_UPDATE);
}
