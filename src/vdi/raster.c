/*
 * Implements GEM VDI raster and MFDB-oriented entry points for the
 * hosted port. Keeping copy, cell-array, and bitmap helpers together
 * makes the public raster surface easier to navigate.
 *
 * MIT License (see: LICENSE)
 * Copyright (C) 2026 tomaz stih
 */

#include "gem/vdi.h"

#include "_internal.h"
#include "_state.h"

#include <stdlib.h>
#include <string.h>

static uint8_t _vdi_sample_bitmap_direct(const MFDB *mfdb, WORD x, WORD y)
{
    if (_vdi_uses_screen(mfdb)) {
        if (x < 0 || y < 0 || x >= _vdi.width || y >= _vdi.height) {
            return 0;
        }
        return (uint8_t) (_vdi_get_screen_pixel(x, y) != 0);
    }
    if (mfdb == NULL || mfdb->fd_addr == NULL || mfdb->fd_nplanes != 1 ||
        x < 0 || y < 0 || x >= mfdb->fd_w || y >= mfdb->fd_h) {
        return 0;
    }

    {
        const UWORD *row = (const UWORD *) mfdb->fd_addr +
            (size_t) y * (size_t) mfdb->fd_wdwidth;
        UWORD mask = (UWORD) (0x8000u >> ((unsigned int) x & 15u));

        return (row[(size_t) x / 16u] & mask) != 0u ? 1u : 0u;
    }
}

static void _vdi_get_destination_bounds(MFDB *dst, vdi_rect_t *bounds)
{
    if (_vdi_uses_screen(dst)) {
        _vdi_get_active_clip_rect(bounds);
        return;
    }

    bounds->x0 = 0;
    bounds->y0 = 0;
    bounds->x1 = (WORD) (dst->fd_w - 1);
    bounds->y1 = (WORD) (dst->fd_h - 1);
}

static void _vdi_store_pixel_raw(MFDB *dst, WORD x, WORD y, WORD color)
{
    if (_vdi_uses_screen(dst)) {
        if (_vdi_point_visible(x, y)) {
            uint8_t *row = (uint8_t *) _vdi.surface->pixels +
                (size_t) y * _vdi.surface->pitch;
            uint8_t mask = (uint8_t) (1u << (7u - ((unsigned int) x & 7u)));

            if (color != 0) {
                row[(size_t) x / 8u] |= mask;
            } else {
                row[(size_t) x / 8u] &= (uint8_t) ~mask;
            }
        }
        return;
    }

    if (dst == NULL || dst->fd_addr == NULL || dst->fd_nplanes != 1 ||
        x < 0 || y < 0 || x >= dst->fd_w || y >= dst->fd_h) {
        return;
    }

    {
        UWORD *row = (UWORD *) dst->fd_addr +
            (size_t) y * (size_t) dst->fd_wdwidth;
        UWORD mask = (UWORD) (0x8000u >> ((unsigned int) x & 15u));

        if (color != 0) {
            row[(size_t) x / 16u] |= mask;
        } else {
            row[(size_t) x / 16u] &= (UWORD) ~mask;
        }
    }
}

static void _vdi_blit_bitmap(MFDB *src, MFDB *dst, CONST WORD pxy[8],
    WORD mode, WORD transparent, WORD foreground)
{
    vdi_rect_t dst_rect;
    vdi_rect_t dst_bounds;
    WORD src_x0 = _vdi_min_word(pxy[0], pxy[2]);
    WORD src_y0 = _vdi_min_word(pxy[1], pxy[3]);
    WORD src_w = (WORD) (abs(pxy[2] - pxy[0]) + 1);
    WORD src_h = (WORD) (abs(pxy[3] - pxy[1]) + 1);
    WORD dst_x0 = _vdi_min_word(pxy[4], pxy[6]);
    WORD dst_y0 = _vdi_min_word(pxy[5], pxy[7]);
    WORD dst_w = (WORD) (abs(pxy[6] - pxy[4]) + 1);
    WORD dst_h = (WORD) (abs(pxy[7] - pxy[5]) + 1);
    WORD y;

    dst_rect.x0 = dst_x0;
    dst_rect.y0 = dst_y0;
    dst_rect.x1 = (WORD) (dst_x0 + dst_w - 1);
    dst_rect.y1 = (WORD) (dst_y0 + dst_h - 1);
    _vdi_get_destination_bounds(dst, &dst_bounds);
    if (!_vdi_intersect_rects(&dst_rect, &dst_bounds, &dst_rect)) {
        return;
    }

    if (src_w == dst_w && src_h == dst_h &&
        transparent == 0 && mode == 1 && mode != 6 &&
        _vdi_uses_screen(src) && _vdi_uses_screen(dst) &&
        ((unsigned int) src_x0 & 7u) == ((unsigned int) dst_x0 & 7u)) {
        size_t pitch = _vdi.surface->pitch;
        size_t left_byte = (size_t) dst_rect.x0 / 8u;
        size_t right_byte = (size_t) dst_rect.x1 / 8u;
        size_t row_bytes = right_byte - left_byte + 1u;
        WORD rel_y;

        for (rel_y = 0; rel_y <= dst_rect.y1 - dst_rect.y0; ++rel_y) {
            WORD sy = (WORD) (src_y0 + (dst_rect.y0 - dst_y0) + rel_y);
            WORD dy = (WORD) (dst_rect.y0 + rel_y);
            const uint8_t *sr = (const uint8_t *) _vdi.surface->pixels +
                (size_t) sy * pitch + left_byte;
            uint8_t *dr = (uint8_t *) _vdi.surface->pixels +
                (size_t) dy * pitch + left_byte;

            memmove(dr, sr, row_bytes);
        }
        _vdi_present_screen();
        return;
    }

    for (y = dst_rect.y0; y <= dst_rect.y1; ++y) {
        WORD rel_y = (WORD) (y - dst_y0);
        WORD sample_y = (WORD) (src_y0 + ((LONG) rel_y * src_h) / dst_h);
        WORD x = dst_rect.x0;

        if (mode == 6) {
            for (; x <= dst_rect.x1; ++x) {
                WORD rel_x = (WORD) (x - dst_x0);
                WORD sample_x = (WORD) (src_x0 +
                    ((LONG) rel_x * src_w) / dst_w);

                if (_vdi_sample_bitmap_direct(src, sample_x, sample_y) != 0u) {
                    WORD dest = _vdi_mfdb_get_pixel(dst, x, y);

                    _vdi_store_pixel_raw(dst, x, y,
                        (WORD) ((dest == 0) ? 1 : 0));
                }
            }
            continue;
        }

        while (x <= dst_rect.x1) {
            WORD rel_x = (WORD) (x - dst_x0);
            WORD sample_x = (WORD) (src_x0 + ((LONG) rel_x * src_w) / dst_w);
            uint8_t source = _vdi_sample_bitmap_direct(src, sample_x, sample_y);
            WORD run_color;
            WORD run_start;

            if (transparent != 0 && source == 0u) {
                ++x;
                continue;
            }

            run_color = (transparent != 0) ? foreground :
                (WORD) ((source != 0u) ? 1 : 0);
            run_start = x;
            ++x;
            while (x <= dst_rect.x1) {
                rel_x = (WORD) (x - dst_x0);
                sample_x = (WORD) (src_x0 + ((LONG) rel_x * src_w) / dst_w);
                source = _vdi_sample_bitmap_direct(src, sample_x, sample_y);
                if (transparent != 0) {
                    if (source == 0u) {
                        break;
                    }
                } else if ((WORD) ((source != 0u) ? 1 : 0) != run_color) {
                    break;
                }
                ++x;
            }
            _vdi_mfdb_draw_hline(dst, y, run_start, (WORD) (x - 1), run_color);
        }
    }
}

WORD v_cellarray(WORD handle, WORD xy[], WORD row_length, WORD el_per_row,
    WORD num_rows, WORD wr_mode, WORD colors[])
{
    WORD left;
    WORD top;
    WORD right;
    WORD bottom;
    WORD cols;
    WORD rows;
    WORD cell_width;
    WORD cell_height;
    WORD row;

    (void) row_length;
    (void) wr_mode;

    if (!_vdi_valid_handle(handle) || xy == NULL || colors == NULL ||
        el_per_row <= 0 || num_rows <= 0) {
        return 0;
    }

    _vdi_rect_from_xy(xy, &left, &top, &right, &bottom);
    cols = (el_per_row > 0) ? el_per_row : 1;
    rows = (num_rows > 0) ? num_rows : 1;
    cell_width = (WORD) (((right - left) + cols) / cols);
    cell_height = (WORD) (((bottom - top) + rows) / rows);

    for (row = 0; row < rows; ++row) {
        WORD col;

        for (col = 0; col < cols; ++col) {
            WORD color = colors[row * cols + col];
            WORD x0 = (WORD) (left + col * cell_width);
            WORD y0 = (WORD) (top + row * cell_height);
            WORD x1 = (WORD) (x0 + cell_width - 1);
            WORD y1 = (WORD) (y0 + cell_height - 1);

            _vdi_fill_box(x0, y0, x1, y1, (WORD) ((color != 0) ? 1 : 0));
        }
    }
    _vdi_present_screen();
    return 1;
}

WORD vq_cellarray(WORD handle, WORD xy[], WORD row_length, WORD num_requested,
    WORD *el_used, WORD *rows_used, WORD status[])
{
    WORD left;
    WORD top;
    WORD right;
    WORD bottom;
    WORD cols;
    WORD rows;
    WORD cell_width;
    WORD cell_height;
    WORD row;

    (void) row_length;

    if (!_vdi_valid_handle(handle) || xy == NULL || status == NULL ||
        num_requested <= 0) {
        return 0;
    }

    _vdi_rect_from_xy(xy, &left, &top, &right, &bottom);
    cols = (num_requested > 0) ? num_requested : 1;
    rows = 1;
    cell_width = (WORD) (((right - left) + cols) / cols);
    cell_height = (WORD) (((bottom - top) + rows) / rows);

    for (row = 0; row < rows; ++row) {
        WORD col;

        for (col = 0; col < cols; ++col) {
            WORD sample_x = (WORD) (left + col * cell_width);
            WORD sample_y = (WORD) (top + row * cell_height);

            status[row * cols + col] = _vdi_get_screen_pixel(sample_x,
                sample_y);
        }
    }
    if (el_used != NULL) {
        *el_used = cols;
    }
    if (rows_used != NULL) {
        *rows_used = rows;
    }
    return 1;
}

VOID vro_cpyfm(VDI_HANDLE handle, WORD mode, CONST WORD pxy[8], MFDB *src,
    MFDB *dst)
{
    if (handle != _vdi_handle_screen || !_vdi.open || pxy == NULL) {
        return;
    }

    _vdi_blit_bitmap(src, dst, pxy, mode, 0, 0);
    if (_vdi_uses_screen(dst)) {
        _vdi_present_screen();
    }
}

VOID vrt_cpyfm(VDI_HANDLE handle, WORD mode, CONST WORD pxy[8], MFDB *src,
    MFDB *dst, CONST WORD colors[2])
{
    WORD foreground = 1;

    if (handle != _vdi_handle_screen || !_vdi.open || pxy == NULL) {
        return;
    }
    if (colors != NULL) {
        foreground = _vdi_color_to_pixel(colors[1]);
    }

    _vdi_blit_bitmap(src, dst, pxy, mode, 1, foreground);
    if (_vdi_uses_screen(dst)) {
        _vdi_present_screen();
    }
}

WORD vr_trnfm(WORD handle, MFDB *src, MFDB *dst)
{
    WORD xy[8];

    if (!_vdi_valid_handle(handle) || src == NULL || dst == NULL) {
        return 0;
    }

    xy[0] = 0;
    xy[1] = 0;
    xy[2] = (WORD) (src->fd_w - 1);
    xy[3] = (WORD) (src->fd_h - 1);
    xy[4] = 0;
    xy[5] = 0;
    xy[6] = (WORD) (dst->fd_w - 1);
    xy[7] = (WORD) (dst->fd_h - 1);
    vro_cpyfm(handle, 1, xy, src, dst);
    return 1;
}
