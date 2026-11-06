/*
 * Draws lines into the hosted VDI's packed monochrome surface: Cohen-
 * Sutherland clipping against the active clip, horizontal runs written a
 * byte at a time under the current write mode, vertical runs and the
 * Bresenham general case.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (C) 2026 tomaz stih
 */

#include "vdi_internal.h"

#include "vdi_state.h"

#include <stdlib.h>
#include <string.h>

int vdi_clip_line_segment(WORD *x0, WORD *y0, WORD *x1, WORD *y1)
{
    enum {
        vdi_clip_left = 1,
        vdi_clip_right = 2,
        vdi_clip_bottom = 4,
        vdi_clip_top = 8
    };

    vdi_rect_t clip;

    if (x0 == NULL || y0 == NULL || x1 == NULL || y1 == NULL) {
        return 0;
    }

    vdi_get_active_clip_rect(&clip);
    for (;;) {
        int code0 = 0;
        int code1 = 0;
        long x = 0;
        long y = 0;
        int out_code;

        if (*x0 < clip.x0) {
            code0 |= vdi_clip_left;
        } else if (*x0 > clip.x1) {
            code0 |= vdi_clip_right;
        }
        if (*y0 < clip.y0) {
            code0 |= vdi_clip_top;
        } else if (*y0 > clip.y1) {
            code0 |= vdi_clip_bottom;
        }

        if (*x1 < clip.x0) {
            code1 |= vdi_clip_left;
        } else if (*x1 > clip.x1) {
            code1 |= vdi_clip_right;
        }
        if (*y1 < clip.y0) {
            code1 |= vdi_clip_top;
        } else if (*y1 > clip.y1) {
            code1 |= vdi_clip_bottom;
        }

        if ((code0 | code1) == 0) {
            return 1;
        }
        if ((code0 & code1) != 0) {
            return 0;
        }

        out_code = (code0 != 0) ? code0 : code1;
        if ((out_code & vdi_clip_top) != 0) {
            if (*y1 == *y0) {
                return 0;
            }
            x = *x0 + ((long)(*x1 - *x0) * (clip.y0 - *y0)) / (*y1 - *y0);
            y = clip.y0;
        } else if ((out_code & vdi_clip_bottom) != 0) {
            if (*y1 == *y0) {
                return 0;
            }
            x = *x0 + ((long)(*x1 - *x0) * (clip.y1 - *y0)) / (*y1 - *y0);
            y = clip.y1;
        } else if ((out_code & vdi_clip_right) != 0) {
            if (*x1 == *x0) {
                return 0;
            }
            y = *y0 + ((long)(*y1 - *y0) * (clip.x1 - *x0)) / (*x1 - *x0);
            x = clip.x1;
        } else {
            if (*x1 == *x0) {
                return 0;
            }
            y = *y0 + ((long)(*y1 - *y0) * (clip.x0 - *x0)) / (*x1 - *x0);
            x = clip.x0;
        }

        if (out_code == code0) {
            *x0 = (WORD)x;
            *y0 = (WORD)y;
        } else {
            *x1 = (WORD)x;
            *y1 = (WORD)y;
        }
    }
}

static void vdi_hline_to_row(uint8_t *row, WORD left, WORD right, WORD color)
{
    WORD mode = vdi_write_mode();
    size_t start_byte = (size_t)left / 8u;
    size_t end_byte = (size_t)right / 8u;
    unsigned int start_bit = (unsigned int)left & 7u;
    unsigned int end_bit = (unsigned int)right & 7u;
    uint8_t left_mask = (uint8_t)(0xffu >> start_bit);
    uint8_t right_mask = (uint8_t)(0xffu << (7u - end_bit));

#define VDI_APPLY_MODE(byte_ref, mask)                                         \
    switch (mode) {                                                            \
        case 2:                                                                \
            if (color != 0)                                                    \
                (byte_ref) |= (mask);                                          \
            break;                                                             \
        case 3:                                                                \
            if (color != 0)                                                    \
                (byte_ref) ^= (mask);                                          \
            break;                                                             \
        case 4:                                                                \
            (byte_ref) &= (uint8_t) ~(mask);                                   \
            break;                                                             \
        default:                                                               \
            if (color != 0)                                                    \
                (byte_ref) |= (mask);                                          \
            else                                                               \
                (byte_ref) &= (uint8_t) ~(mask);                               \
            break;                                                             \
    }

    if (start_byte == end_byte) {
        uint8_t mask = left_mask & right_mask;

        VDI_APPLY_MODE(row[start_byte], mask);
        return;
    }

    VDI_APPLY_MODE(row[start_byte], left_mask);

    if (end_byte > start_byte + 1u) {
        size_t mid_start = start_byte + 1u;
        size_t mid_count = end_byte - mid_start;

        switch (mode) {
            case 2:
                if (color != 0) {
                    memset(&row[mid_start], 0xffu, mid_count);
                }
                break;
            case 4:
                memset(&row[mid_start], 0x00u, mid_count);
                break;
            case 3:
                if (color != 0) {
                    size_t i;

                    for (i = mid_start; i < end_byte; ++i) {
                        row[i] ^= 0xffu;
                    }
                }
                break;
            case 1:
            default:
                memset(&row[mid_start], (color != 0) ? 0xffu : 0x00u,
                       mid_count);
                break;
        }
    }

    VDI_APPLY_MODE(row[end_byte], right_mask);

#undef VDI_APPLY_MODE
}

void vdi_draw_screen_hline_direct(WORD y, WORD left, WORD right, WORD color)
{
    uint8_t *row;

    if (left > right) {
        WORD tmp = left;

        left = right;
        right = tmp;
    }
    /* Callers pre-clip against the active clip; this only guards the row
     * buffer itself, since a negative column would index far outside it. */
    if (right < 0 || left >= vdi_state.width) {
        return;
    }
    if (left < 0) {
        left = 0;
    }
    if (right >= vdi_state.width) {
        right = (WORD)(vdi_state.width - 1);
    }
    row = vdi_screen_row_mutable(y);
    if (row == NULL) {
        return;
    }
    vdi_mark_dirty(left, y, right, y);
    vdi_hline_to_row(row, left, right, color);
}

void vdi_draw_screen_hline(WORD y, WORD x0, WORD x1, WORD color)
{
    vdi_rect_t clip;
    uint8_t *row;
    WORD left;
    WORD right;

    if (x0 > x1) {
        WORD tmp = x0;

        x0 = x1;
        x1 = tmp;
    }

    vdi_get_active_clip_rect(&clip);
    if (y < clip.y0 || y > clip.y1) {
        return;
    }

    left = (x0 < clip.x0) ? clip.x0 : x0;
    right = (x1 > clip.x1) ? clip.x1 : x1;
    if (left > right) {
        return;
    }

    row = vdi_screen_row_mutable(y);
    if (row == NULL) {
        return;
    }

    vdi_hline_to_row(row, left, right, color);
}

void vdi_draw_line_segment(WORD x0, WORD y0, WORD x1, WORD y1, WORD color)
{
    WORD dx;
    WORD sx;
    WORD dy;
    WORD sy;
    WORD err;
    UWORD pattern = 0xffffu;
    unsigned int pattern_index = 0u;

    if (!vdi_clip_line_segment(&x0, &y0, &x1, &y1)) {
        return;
    }

    vdi_prepare_screen_write();
    vdi_mark_dirty(x0, y0, x1, y1);

    switch (vdi_compat.line_style) {
        case 2:
            pattern = 0xff00u;
            break;
        case 3:
            pattern = 0xaaaau;
            break;
        case 4:
            pattern = 0xff18u;
            break;
        case 5:
            pattern = 0xf0f0u;
            break;
        case 6:
            pattern = 0xf198u;
            break;
        case 7:
            pattern = (UWORD)vdi_compat.line_pattern;
            break;
        default:
            break;
    }

    if (pattern != 0xffffu) {
        dx = (WORD)abs(x1 - x0);
        sx = (WORD)((x0 < x1) ? 1 : -1);
        dy = (WORD)-abs(y1 - y0);
        sy = (WORD)((y0 < y1) ? 1 : -1);
        err = (WORD)(dx + dy);

        FOREVER
        {
            WORD e2 = (WORD)(2 * err);

            if ((pattern & (UWORD)(0x8000u >> (pattern_index & 15u))) != 0u) {
                vdi_set_screen_pixel_raw(x0, y0, color);
            }
            ++pattern_index;
            if (x0 == x1 && y0 == y1) {
                return;
            }
            if (e2 >= dy) {
                err = (WORD)(err + dy);
                x0 = (WORD)(x0 + sx);
            }
            if (e2 <= dx) {
                err = (WORD)(err + dx);
                y0 = (WORD)(y0 + sy);
            }
        }
    }

    if (y0 == y1) {
        vdi_draw_screen_hline_direct(y0, x0, x1, color);
        return;
    }

    if (x0 == x1) {
        uint8_t mask = vdi_screen_mask_for_x(x0);
        size_t byte_off = (size_t)x0 / 8u;
        WORD mode = vdi_write_mode();
        WORD min_y = (y0 < y1) ? y0 : y1;
        WORD max_y = (y0 < y1) ? y1 : y0;
        size_t pitch = vdi_state.surface->pitch;
        uint8_t *row =
            (uint8_t *)vdi_state.surface->pixels + (size_t)min_y * pitch;
        WORD scan_y;

        for (scan_y = min_y; scan_y <= max_y; ++scan_y, row += pitch) {
            switch (mode) {
                case 2:
                    if (color != 0) {
                        row[byte_off] |= mask;
                    }
                    break;
                case 3:
                    if (color != 0) {
                        row[byte_off] ^= mask;
                    }
                    break;
                case 4:
                    row[byte_off] &= (uint8_t)~mask;
                    break;
                default:
                    if (color != 0) {
                        row[byte_off] |= mask;
                    } else {
                        row[byte_off] &= (uint8_t)~mask;
                    }
                    break;
            }
        }
        return;
    }

    dx = (WORD)abs(x1 - x0);
    sx = (WORD)((x0 < x1) ? 1 : -1);
    dy = (WORD)-abs(y1 - y0);
    sy = (WORD)((y0 < y1) ? 1 : -1);
    err = (WORD)(dx + dy);

    FOREVER
    {
        WORD e2 = (WORD)(2 * err);

        vdi_set_screen_pixel_raw(x0, y0, color);
        if (x0 == x1 && y0 == y1) {
            return;
        }

        if (e2 >= dy) {
            err = (WORD)(err + dy);
            x0 = (WORD)(x0 + sx);
        }
        if (e2 <= dx) {
            err = (WORD)(err + dx);
            y0 = (WORD)(y0 + sy);
        }
    }
}
