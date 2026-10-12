/*
 * Paints hosted AES window chrome: the frame, the title bar with its
 * icons and text, scroll arrows, slider slots and thumbs and the sizer
 * glyph, plus clearing a frame that closes.
 *
 * MIT License (see: LICENSE)
 * Copyright (C) 2026 tomaz stih
 */

#include "window_internal.h"

#include "../vdi/vdi_internal.h"

#include <string.h>

static void aes_draw_window_icon_offset(WORD x0, WORD y0, WORD x1, WORD y1,
                                        char glyph, WORD dx, WORD dy);

int aes_window_is_top(const aes_window_t *window)
{
    size_t i;
    uint32_t top_z = 0u;

    if (window == NULL || window->used == 0 || window->open == 0) {
        return 0;
    }

    for (i = 0; i < AES_MAX_WINDOWS; ++i) {
        if (aes_state.windows[i].used != 0 && aes_state.windows[i].open != 0 &&
            aes_state.windows[i].z_order > top_z) {
            top_z = aes_state.windows[i].z_order;
        }
    }

    return window->z_order == top_z;
}

static void aes_draw_window_icon(WORD x0, WORD y0, WORD x1, WORD y1, char glyph)
{
    aes_draw_window_icon_offset(x0, y0, x1, y1, glyph, 0, 0);
}

static void aes_draw_window_icon_offset(WORD x0, WORD y0, WORD x1, WORD y1,
                                        char glyph, WORD dx, WORD dy)
{
    char text[2];
    WORD text_attrib[6];
    WORD previous_font = 0;
    WORD system_font = 1;
    WORD text_width;
    WORD text_height;
    WORD text_x;
    WORD text_y;

    if (x0 > x1 || y0 > y1) {
        return;
    }

    if (vqt_attributes(aes_state.vdi_handle, text_attrib) != 0) {
        previous_font = text_attrib[0];
    }
    if (previous_font != 0 && previous_font != system_font) {
        (void)vst_font(aes_state.vdi_handle, system_font);
    }

    text[0] = glyph;
    text[1] = '\0';
    text_width = (WORD)vdi_string_width(text);
    text_height = vdi_font_text_height();
    text_x = (WORD)(x0 + ((x1 - x0 + 1) - text_width) / 2 + dx);
    text_y =
        (WORD)(y0 + ((y1 - y0 + 1) - text_height) / 2 + vdi_font_ascent() + dy);
    aes_draw_text(text_x, text_y, aes_dark_color(), text);

    if (previous_font != 0 && previous_font != system_font) {
        (void)vst_font(aes_state.vdi_handle, previous_font);
    }
}

static void aes_draw_sizer_glyph(const aes_window_t *window)
{
    GRECT rect;
    WORD x1;
    WORD y1;

    if (window == NULL || aes_window_sizer_rect(window, &rect) == 0) {
        return;
    }

    x1 = (WORD)(rect.g_x + rect.g_w - 2);
    y1 = (WORD)(rect.g_y + rect.g_h - 2);
    aes_fill_rect(rect.g_x, rect.g_y, x1, y1, aes_light_color());
    aes_draw_window_icon(rect.g_x, rect.g_y, x1, y1, 6);
}

static void aes_draw_window_title(const aes_window_t *window,
                                  const WORD outer_box[4], WORD title_height)
{
    static const uint8_t title_rows[] = {0x00, 0x55, 0x00, 0x55};
    GRECT closer_rect;
    GRECT fuller_rect;
    WORD text_attrib[6];
    WORD previous_font = 0;
    WORD system_font = 1;
    WORD title_sep[4];
    WORD title_left;
    WORD title_right;
    WORD title_box_left = 0;
    WORD title_box_right = -1;
    WORD title_text_left;
    WORD title_text_right;
    WORD title_top;
    WORD title_bottom;
    WORD text_height;
    WORD title_y;
    WORD title_width;
    WORD available_width;
    WORD title_x = 0;
    int has_closer;
    int has_fuller;
    int active_title;

    if (window == NULL || title_height <= 0) {
        return;
    }

    if (vqt_attributes(aes_state.vdi_handle, text_attrib) != 0) {
        previous_font = text_attrib[0];
    }
    if (previous_font != 0 && previous_font != system_font) {
        (void)vst_font(aes_state.vdi_handle, system_font);
    }

    text_height = vdi_font_text_height();
    active_title = aes_window_is_top(window);
    has_closer = aes_window_closer_rect(window, &closer_rect);
    has_fuller = aes_window_fuller_rect(window, &fuller_rect);
    title_top = (WORD)(outer_box[1] + 1);
    title_bottom = (WORD)(window->work.g_y - 2);
    title_left = (WORD)(outer_box[0] + 1);
    title_right = (WORD)(outer_box[2] - 1);

    title_sep[0] = outer_box[0];
    title_sep[1] = (WORD)(window->work.g_y - 1);
    title_sep[2] = outer_box[2];
    title_sep[3] = title_sep[1];
    vsl_color(aes_state.vdi_handle, aes_dark_color());
    v_pline(aes_state.vdi_handle, 2, title_sep);

    if (has_closer != 0) {
        WORD close_sep[4];
        WORD close_right = (WORD)(closer_rect.g_x + closer_rect.g_w - 2);

        aes_fill_rect(closer_rect.g_x, closer_rect.g_y, close_right,
                      (WORD)(closer_rect.g_y + closer_rect.g_h - 1),
                      aes_light_color());
        close_sep[0] = close_right;
        close_sep[1] = closer_rect.g_y;
        close_sep[2] = close_sep[0];
        close_sep[3] = (WORD)(closer_rect.g_y + closer_rect.g_h - 1);
        vsl_color(aes_state.vdi_handle, aes_dark_color());
        v_pline(aes_state.vdi_handle, 2, close_sep);
        aes_draw_window_icon_offset(
            closer_rect.g_x, closer_rect.g_y, close_right,
            (WORD)(closer_rect.g_y + closer_rect.g_h - 1), 5, 0, 0);
        title_left = (WORD)(close_sep[0] + 1);
    }

    if (has_fuller != 0) {
        WORD fuller_sep[4];
        WORD fuller_left = (WORD)(fuller_rect.g_x + 1);
        WORD fuller_right = (WORD)(fuller_rect.g_x + fuller_rect.g_w - 1);

        aes_fill_rect(fuller_left, fuller_rect.g_y, fuller_right,
                      (WORD)(fuller_rect.g_y + fuller_rect.g_h - 1),
                      aes_light_color());
        fuller_sep[0] = fuller_left;
        fuller_sep[1] = fuller_rect.g_y;
        fuller_sep[2] = fuller_sep[0];
        fuller_sep[3] = (WORD)(fuller_rect.g_y + fuller_rect.g_h - 1);
        vsl_color(aes_state.vdi_handle, aes_dark_color());
        v_pline(aes_state.vdi_handle, 2, fuller_sep);
        aes_draw_window_icon_offset(
            fuller_left, fuller_rect.g_y, fuller_right,
            (WORD)(fuller_rect.g_y + fuller_rect.g_h - 1), 7, 2, 0);
        title_right = (WORD)(fuller_sep[0] - 1);
    }

    if ((window->kind & NAME) != 0u && window->name[0] != '\0') {
        title_width = (WORD)vdi_string_width(window->name);
        available_width = (WORD)(title_right - title_left + 1);
        if (available_width <= title_width) {
            title_x = title_left;
        } else {
            title_x = (WORD)(title_left + (available_width - title_width) / 2);
        }

        title_text_left = (WORD)(title_x - 2);
        title_text_right = (WORD)(title_x + title_width + 1);
        if (title_text_left < title_left) {
            title_text_left = title_left;
        }
        if (title_text_right > title_right) {
            title_text_right = title_right;
        }
        title_box_left = title_text_left;
        title_box_right = title_text_right;
    }

    if (title_left <= title_right && title_top <= title_bottom) {
        if (active_title != 0) {
            if (title_box_left <= title_box_right) {
                WORD left_fill_right = (WORD)(title_box_left - 1);
                WORD right_fill_left = (WORD)(title_box_right + 1);

                if (title_left <= left_fill_right) {
                    aes_fill_pattern_rect(title_left, title_top,
                                          left_fill_right, title_bottom,
                                          title_rows, sizeof(title_rows));
                }
                if (right_fill_left <= title_right) {
                    aes_fill_pattern_rect(right_fill_left, title_top,
                                          title_right, title_bottom, title_rows,
                                          sizeof(title_rows));
                }
            } else {
                aes_fill_pattern_rect(title_left, title_top, title_right,
                                      title_bottom, title_rows,
                                      sizeof(title_rows));
            }
        } else {
            aes_fill_rect(title_left, title_top, title_right, title_bottom,
                          aes_light_color());
        }
    }

    if (title_box_left <= title_box_right) {
        aes_fill_rect(title_box_left, title_top, title_box_right, title_bottom,
                      aes_light_color());
        title_y =
            (WORD)(title_top + (title_bottom - title_top - text_height) / 2 +
                   vdi_font_ascent() + 1);
        aes_draw_text(title_x, title_y, aes_dark_color(), window->name);
    }

    if (previous_font != 0 && previous_font != system_font) {
        (void)vst_font(aes_state.vdi_handle, previous_font);
    }
}

void aes_draw_window_frame(const aes_window_t *window)
{
    static const uint8_t scrollbar_rows[] = {0x88, 0x22};
    WORD outer_box[10];
    WORD left_fill[4];
    WORD right_fill[4];
    WORD bottom_fill[4];
    WORD gutter_x = 0;
    WORD gutter_y = 0;
    WORD gutter_right = 0;
    WORD gutter_bottom = 0;
    WORD gutter_fill[4];
    WORD vertical_sep[4];
    WORD horizontal_sep[4];
    WORD shadow_bottom[4];
    WORD shadow_right[4];
    WORD work_box[10];

    if (aes_state.vdi_ready == 0 || window == NULL || window->open == 0) {
        return;
    }

    aes_trace("draw_window_frame handle=%d outer=%d,%d %dx%d work=%d,%d %dx%d",
              window->handle, window->outer.g_x, window->outer.g_y,
              window->outer.g_w, window->outer.g_h, window->work.g_x,
              window->work.g_y, window->work.g_w, window->work.g_h);

    outer_box[0] = window->outer.g_x;
    outer_box[1] = window->outer.g_y;
    outer_box[2] = (WORD)(window->outer.g_x + window->outer.g_w - 1);
    outer_box[3] = outer_box[1];
    outer_box[4] = outer_box[2];
    outer_box[5] = (WORD)(window->outer.g_y + window->outer.g_h - 1);
    outer_box[6] = outer_box[0];
    outer_box[7] = outer_box[5];
    outer_box[8] = outer_box[0];
    outer_box[9] = outer_box[1];

    work_box[0] = window->work.g_x;
    work_box[1] = window->work.g_y;
    work_box[2] = (WORD)(window->work.g_x + window->work.g_w - 1);
    work_box[3] = work_box[1];
    work_box[4] = work_box[2];
    work_box[5] = (WORD)(window->work.g_y + window->work.g_h - 1);
    work_box[6] = work_box[0];
    work_box[7] = work_box[5];
    work_box[8] = work_box[0];
    work_box[9] = work_box[1];

    vsl_color(aes_state.vdi_handle, WHITE);
    v_pline(aes_state.vdi_handle, 5, outer_box);
    shadow_bottom[0] = (WORD)(window->outer.g_x + 1);
    shadow_bottom[1] = (WORD)(window->outer.g_y + window->outer.g_h);
    shadow_bottom[2] = (WORD)(window->outer.g_x + window->outer.g_w);
    shadow_bottom[3] = shadow_bottom[1];
    shadow_right[0] = (WORD)(window->outer.g_x + window->outer.g_w);
    shadow_right[1] = (WORD)(window->outer.g_y + 1);
    shadow_right[2] = shadow_right[0];
    shadow_right[3] = (WORD)(window->outer.g_y + window->outer.g_h);
    v_pline(aes_state.vdi_handle, 2, shadow_bottom);
    v_pline(aes_state.vdi_handle, 2, shadow_right);

    left_fill[0] = (WORD)(window->outer.g_x + 1);
    left_fill[1] = window->work.g_y;
    left_fill[2] = (WORD)(window->work.g_x - 1);
    left_fill[3] = (WORD)(window->outer.g_y + window->outer.g_h - 2);
    if (left_fill[0] <= left_fill[2] && left_fill[1] <= left_fill[3]) {
        aes_fill_rect(left_fill[0], left_fill[1], left_fill[2], left_fill[3],
                      aes_light_color());
    }

    right_fill[0] = (WORD)(window->work.g_x + window->work.g_w);
    right_fill[1] = window->work.g_y;
    right_fill[2] = (WORD)(window->outer.g_x + window->outer.g_w - 2);
    right_fill[3] = (WORD)(window->outer.g_y + window->outer.g_h - 2);
    if (right_fill[0] <= right_fill[2] && right_fill[1] <= right_fill[3]) {
        aes_fill_rect(right_fill[0], right_fill[1], right_fill[2],
                      right_fill[3], aes_light_color());
    }

    bottom_fill[0] = (WORD)(window->outer.g_x + 1);
    bottom_fill[1] = (WORD)(window->work.g_y + window->work.g_h);
    bottom_fill[2] = (WORD)(window->outer.g_x + window->outer.g_w - 2);
    bottom_fill[3] = (WORD)(window->outer.g_y + window->outer.g_h - 2);
    if (bottom_fill[0] <= bottom_fill[2] && bottom_fill[1] <= bottom_fill[3]) {
        aes_fill_rect(bottom_fill[0], bottom_fill[1], bottom_fill[2],
                      bottom_fill[3], aes_light_color());
    }

    if ((window->kind & SIZER) != 0u) {
        gutter_x = (WORD)(window->outer.g_x + window->outer.g_w -
                          aes_window_right_border(window));
        gutter_y = (WORD)(window->outer.g_y + window->outer.g_h -
                          aes_window_bottom_border(window));
        gutter_right = (WORD)(window->outer.g_x + window->outer.g_w - 2);
        gutter_bottom = (WORD)(window->outer.g_y + window->outer.g_h - 2);

        gutter_fill[0] = gutter_x;
        gutter_fill[1] = window->work.g_y;
        gutter_fill[2] = gutter_right;
        gutter_fill[3] = gutter_bottom;
        aes_fill_rect(gutter_fill[0], gutter_fill[1], gutter_fill[2],
                      gutter_fill[3], aes_light_color());

        gutter_fill[0] = window->outer.g_x + 1;
        gutter_fill[1] = gutter_y;
        gutter_fill[2] = gutter_right;
        gutter_fill[3] = gutter_bottom;
        aes_fill_rect(gutter_fill[0], gutter_fill[1], gutter_fill[2],
                      gutter_fill[3], aes_light_color());

        vertical_sep[0] = gutter_x;
        vertical_sep[1] = window->outer.g_y + 1;
        vertical_sep[2] = vertical_sep[0];
        vertical_sep[3] = (WORD)(window->outer.g_y + window->outer.g_h - 1);
        horizontal_sep[0] = window->outer.g_x + 1;
        horizontal_sep[1] = gutter_y;
        horizontal_sep[2] = (WORD)(window->outer.g_x + window->outer.g_w - 1);
        horizontal_sep[3] = horizontal_sep[1];
        vsl_color(aes_state.vdi_handle, WHITE);
        v_pline(aes_state.vdi_handle, 2, vertical_sep);
        v_pline(aes_state.vdi_handle, 2, horizontal_sep);
    }

    if (window->work.g_w > 0 && window->work.g_h > 0 &&
        (window->kind & SIZER) == 0u) {
        v_pline(aes_state.vdi_handle, 5, work_box);
    }
    aes_draw_window_title(window, outer_box, aes_window_title_height(window));
    if (aes_window_has_vscroll(window) != 0) {
        GRECT track;
        GRECT slot;
        GRECT thumb;
        GRECT button;
        WORD right;
        WORD bottom;

        if (aes_window_vtrack_rect(window, &track) != 0) {
            right = (WORD)(track.g_x + track.g_w - 1);
            bottom = (WORD)(track.g_y + track.g_h - 1);
            aes_fill_pattern_rect(track.g_x, track.g_y, right, bottom,
                                  scrollbar_rows, sizeof(scrollbar_rows));
            vsl_color(aes_state.vdi_handle, aes_dark_color());
            aes_draw_rect_edges(track.g_x, track.g_y, right, bottom, 1, 0, 1,
                                1);
        }
        if (aes_window_vup_rect(window, &button) != 0) {
            right = (WORD)(button.g_x + button.g_w - 1);
            bottom = (WORD)(button.g_y + button.g_h - 1);
            aes_fill_rect(button.g_x, button.g_y, right, bottom,
                          aes_light_color());
            vsl_color(aes_state.vdi_handle, aes_dark_color());
            aes_draw_rect_edges(button.g_x, button.g_y, right, bottom, 0, 0, 1,
                                1);
            aes_draw_window_icon_offset(button.g_x, button.g_y, right, bottom,
                                        1, 0, -1);
        }
        if (aes_window_vdown_rect(window, &button) != 0) {
            right = (WORD)(button.g_x + button.g_w - 1);
            bottom = (WORD)(button.g_y + button.g_h - 1);
            aes_fill_rect(button.g_x, button.g_y, right, bottom,
                          aes_light_color());
            vsl_color(aes_state.vdi_handle, aes_dark_color());
            aes_draw_rect_edges(button.g_x, button.g_y, right, bottom, 1, 0, 0,
                                1);
            aes_draw_window_icon(button.g_x, button.g_y, right, bottom, 2);
        }
        if (aes_window_vthumb_rect(window, &thumb) != 0) {
            int draw_top = 1;
            int draw_bottom = 1;

            right = (WORD)(thumb.g_x + thumb.g_w - 1);
            bottom = (WORD)(thumb.g_y + thumb.g_h - 1);
            if (aes_window_vslot_rect(window, &slot) != 0) {
                if (thumb.g_y <= slot.g_y) {
                    draw_top = 0;
                }
                if (bottom >= slot.g_y + slot.g_h - 1) {
                    draw_bottom = 0;
                }
            }
            aes_fill_rect(thumb.g_x, thumb.g_y, right, bottom,
                          aes_light_color());
            vsl_color(aes_state.vdi_handle, aes_dark_color());
            aes_draw_rect_edges(thumb.g_x, thumb.g_y, right, bottom, draw_top,
                                0, draw_bottom, 1);
        }
    }
    if (aes_window_has_hscroll(window) != 0) {
        GRECT track;
        GRECT slot;
        GRECT thumb;
        GRECT button;
        WORD right;
        WORD bottom;

        if (aes_window_htrack_rect(window, &track) != 0) {
            right = (WORD)(track.g_x + track.g_w - 1);
            bottom = (WORD)(track.g_y + track.g_h - 1);
            aes_fill_pattern_rect(track.g_x, track.g_y, right, bottom,
                                  scrollbar_rows, sizeof(scrollbar_rows));
            vsl_color(aes_state.vdi_handle, aes_dark_color());
            aes_draw_rect_edges(track.g_x, track.g_y, right, bottom, 1, 1, 0,
                                1);
        }
        if (aes_window_hleft_rect(window, &button) != 0) {
            right = (WORD)(button.g_x + button.g_w - 1);
            bottom = (WORD)(button.g_y + button.g_h - 1);
            aes_fill_rect(button.g_x, button.g_y, right, bottom,
                          aes_light_color());
            vsl_color(aes_state.vdi_handle, aes_dark_color());
            aes_draw_rect_edges(button.g_x, button.g_y, right, bottom, 1, 1, 0,
                                0);
            aes_draw_window_icon_offset(button.g_x, button.g_y, right, bottom,
                                        4, 0, 2);
        }
        if (aes_window_hright_rect(window, &button) != 0) {
            right = (WORD)(button.g_x + button.g_w - 1);
            bottom = (WORD)(button.g_y + button.g_h - 1);
            aes_fill_rect(button.g_x, button.g_y, right, bottom,
                          aes_light_color());
            vsl_color(aes_state.vdi_handle, aes_dark_color());
            aes_draw_rect_edges(button.g_x, button.g_y, right, bottom, 1, 0, 0,
                                1);
            aes_draw_window_icon_offset(button.g_x, button.g_y, right, bottom,
                                        3, 0, 2);
        }
        if (aes_window_hthumb_rect(window, &thumb) != 0) {
            int draw_left = 1;
            int draw_right = 1;

            right = (WORD)(thumb.g_x + thumb.g_w - 1);
            bottom = (WORD)(thumb.g_y + thumb.g_h - 1);
            if (aes_window_hslot_rect(window, &slot) != 0) {
                if (thumb.g_x <= slot.g_x) {
                    draw_left = 0;
                }
                if (right >= slot.g_x + slot.g_w - 1) {
                    draw_right = 0;
                }
            }
            aes_fill_rect(thumb.g_x, thumb.g_y, right, bottom,
                          aes_light_color());
            vsl_color(aes_state.vdi_handle, aes_dark_color());
            aes_draw_rect_edges(thumb.g_x, thumb.g_y, right, bottom, 1,
                                draw_right, 0, draw_left);
        }
    }
    aes_draw_sizer_glyph(window);
    aes_present_window_frame(window);
}

void aes_clear_window_frame(const aes_window_t *window)
{
    WORD rect[4];

    if (aes_state.vdi_ready == 0 || window == NULL) {
        return;
    }

    rect[0] = window->outer.g_x;
    rect[1] = window->outer.g_y;
    rect[2] = (WORD)(window->outer.g_x + window->outer.g_w - 1);
    rect[3] = (WORD)(window->outer.g_y + window->outer.g_h - 1);
    vsf_color(aes_state.vdi_handle, BLACK);
    v_bar(aes_state.vdi_handle, rect);
    aes_present_window_frame(window);
}
