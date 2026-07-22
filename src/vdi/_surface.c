/*
 * Implements private framebuffer, clipping, and MFDB helpers for the
 * hosted GEM VDI layer. These routines own low-level pixel access and
 * line/fill primitives used by the higher-level public entry points.
 *
 * MIT License (see: LICENSE)
 * Copyright (C) 2026 tomaz stih
 */

#include "_internal.h"

#include "_state.h"

#include <stdlib.h>
#include <string.h>

enum {
    VDI_FILL_HOLLOW = 0,
    VDI_FILL_SOLID = 1,
    VDI_FILL_PATTERN = 2,
    VDI_FILL_HATCH = 3,
    VDI_FILL_USER = 4
};

static WORD _vdi_clamp_word(WORD value, WORD low, WORD high)
{
    if (value < low) {
        return low;
    }
    if (value > high) {
        return high;
    }
    return value;
}

static uint8_t *_vdi_screen_row_mutable(WORD y)
{
    if (_vdi.surface == NULL || _vdi.surface->pixels == NULL ||
        y < 0 || y >= _vdi.height) {
        return NULL;
    }

    return (uint8_t *) _vdi.surface->pixels +
        (size_t) y * _vdi.surface->pitch;
}

static const uint8_t *_vdi_screen_row_const(WORD y)
{
    if (_vdi.surface == NULL || _vdi.surface->pixels == NULL ||
        y < 0 || y >= _vdi.height) {
        return NULL;
    }

    return (const uint8_t *) _vdi.surface->pixels +
        (size_t) y * _vdi.surface->pitch;
}

static uint8_t _vdi_screen_mask_for_x(WORD x)
{
    return (uint8_t) (1u << (7u - ((unsigned int) x & 7u)));
}

static void _vdi_apply_mask(uint8_t *byte, uint8_t mask, WORD color)
{
    WORD mode;

    if (byte == NULL || mask == 0u) {
        return;
    }

    mode = _vdi_write_mode();
    switch (mode) {
    case 2:
        if (color != 0) {
            *byte |= mask;
        }
        break;
    case 3:
        if (color != 0) {
            *byte ^= mask;
        }
        break;
    case 4:
        *byte &= (uint8_t) ~mask;
        break;
    case 1:
    default:
        if (color != 0) {
            *byte |= mask;
        } else {
            *byte &= (uint8_t) ~mask;
        }
        break;
    }
}

static WORD _vdi_fill_pattern_bit(WORD x, WORD y, WORD color)
{
    static const UWORD builtin_patterns[8][16] = {
        {
            0xffffu, 0xffffu, 0xffffu, 0xffffu,
            0xffffu, 0xffffu, 0xffffu, 0xffffu,
            0xffffu, 0xffffu, 0xffffu, 0xffffu,
            0xffffu, 0xffffu, 0xffffu, 0xffffu
        },
        {
            0xaaaau, 0x5555u, 0xaaaau, 0x5555u,
            0xaaaau, 0x5555u, 0xaaaau, 0x5555u,
            0xaaaau, 0x5555u, 0xaaaau, 0x5555u,
            0xaaaau, 0x5555u, 0xaaaau, 0x5555u
        },
        {
            0x8888u, 0x2222u, 0x8888u, 0x2222u,
            0x8888u, 0x2222u, 0x8888u, 0x2222u,
            0x8888u, 0x2222u, 0x8888u, 0x2222u,
            0x8888u, 0x2222u, 0x8888u, 0x2222u
        },
        {
            0xccccu, 0x3333u, 0xccccu, 0x3333u,
            0xccccu, 0x3333u, 0xccccu, 0x3333u,
            0xccccu, 0x3333u, 0xccccu, 0x3333u,
            0xccccu, 0x3333u, 0xccccu, 0x3333u
        },
        {
            0xf0f0u, 0x0f0fu, 0xf0f0u, 0x0f0fu,
            0xf0f0u, 0x0f0fu, 0xf0f0u, 0x0f0fu,
            0xf0f0u, 0x0f0fu, 0xf0f0u, 0x0f0fu,
            0xf0f0u, 0x0f0fu, 0xf0f0u, 0x0f0fu
        },
        {
            0xff00u, 0x0000u, 0xff00u, 0x0000u,
            0xff00u, 0x0000u, 0xff00u, 0x0000u,
            0xff00u, 0x0000u, 0xff00u, 0x0000u,
            0xff00u, 0x0000u, 0xff00u, 0x0000u
        },
        {
            0xffffu, 0x0000u, 0xffffu, 0x0000u,
            0xffffu, 0x0000u, 0xffffu, 0x0000u,
            0xffffu, 0x0000u, 0xffffu, 0x0000u,
            0xffffu, 0x0000u, 0xffffu, 0x0000u
        },
        {
            0x8080u, 0x0808u, 0x8080u, 0x0808u,
            0x8080u, 0x0808u, 0x8080u, 0x0808u,
            0x8080u, 0x0808u, 0x8080u, 0x0808u,
            0x8080u, 0x0808u, 0x8080u, 0x0808u
        }
    };
    const UWORD *pattern = NULL;
    WORD row_index;
    WORD bit_on;

    if (_vdi_compat.fill_interior == VDI_FILL_HOLLOW) {
        return 0;
    }

    if (_vdi_compat.fill_interior == VDI_FILL_SOLID) {
        return color;
    }

    if (_vdi_compat.fill_interior == VDI_FILL_USER) {
        pattern = (const UWORD *) _vdi_compat.fill_pattern;
    } else {
        WORD style = _vdi_compat.fill_style;

        if (style < 1 || style > 8) {
            style = 2;
        }
        pattern = builtin_patterns[style - 1];
    }

    row_index = (WORD) ((unsigned int) y & 15u);
    bit_on = (pattern[row_index] &
        (UWORD) (0x8000u >> ((unsigned int) x & 15u))) != 0u;

    if (bit_on != 0) {
        return color;
    }

    return (color != 0) ? 0 : 1;
}

static uint8_t _vdi_mfdb_sample_direct(const MFDB *mfdb, WORD x, WORD y)
{
    const UWORD *row;
    UWORD mask;

    if (_vdi_uses_screen(mfdb)) {
        const uint8_t *screen_row = _vdi_screen_row_const(y);

        if (screen_row == NULL || x < 0 || x >= _vdi.width) {
            return 0;
        }
        return (screen_row[(size_t) x / 8u] & _vdi_screen_mask_for_x(x)) != 0u
            ? 1u : 0u;
    }
    if (mfdb == NULL || mfdb->fd_addr == NULL || mfdb->fd_nplanes != 1 ||
        x < 0 || y < 0 || x >= mfdb->fd_w || y >= mfdb->fd_h) {
        return 0;
    }

    row = (const UWORD *) mfdb->fd_addr +
        (size_t) y * (size_t) mfdb->fd_wdwidth;
    mask = (UWORD) (0x8000u >> ((unsigned int) x & 15u));
    return (row[(size_t) x / 16u] & mask) != 0u ? 1u : 0u;
}

static void _vdi_set_screen_pixel_raw(WORD x, WORD y, WORD color)
{
    uint8_t *row = _vdi_screen_row_mutable(y);
    uint8_t mask;

    if (row == NULL || x < 0 || x >= _vdi.width) {
        return;
    }

    mask = _vdi_screen_mask_for_x(x);
    _vdi_apply_mask(&row[(size_t) x / 8u], mask, color);
}

static WORD _vdi_get_screen_pixel_raw(WORD x, WORD y)
{
    const uint8_t *row = _vdi_screen_row_const(y);

    if (row == NULL || x < 0 || x >= _vdi.width) {
        return 0;
    }

    return (WORD)
        ((row[(size_t) x / 8u] & _vdi_screen_mask_for_x(x)) != 0u);
}

WORD _vdi_get_screen_pixel(WORD x, WORD y)
{
    return _vdi_get_screen_pixel_raw(x, y);
}

void _vdi_set_screen_pixel(WORD x, WORD y, WORD color)
{
    _vdi_prepare_screen_write();
    _vdi_set_screen_pixel_raw(x, y, color);
}

void _vdi_clear_screen(WORD color)
{
    uint8_t value = (uint8_t) ((color != 0) ? 0xffu : 0x00u);

    _vdi_prepare_screen_write();
    memset(_vdi.surface->pixels, value,
        (size_t) _vdi.surface->pitch * (size_t) _vdi.height);
}

int _vdi_clip_line_segment(WORD *x0, WORD *y0, WORD *x1, WORD *y1)
{
    enum {
        _vdi_clip_left = 1,
        _vdi_clip_right = 2,
        _vdi_clip_bottom = 4,
        _vdi_clip_top = 8
    };

    vdi_rect_t clip;

    if (x0 == NULL || y0 == NULL || x1 == NULL || y1 == NULL) {
        return 0;
    }

    _vdi_get_active_clip_rect(&clip);
    for (;;) {
        int code0 = 0;
        int code1 = 0;
        long x = 0;
        long y = 0;
        int out_code;

        if (*x0 < clip.x0) {
            code0 |= _vdi_clip_left;
        } else if (*x0 > clip.x1) {
            code0 |= _vdi_clip_right;
        }
        if (*y0 < clip.y0) {
            code0 |= _vdi_clip_top;
        } else if (*y0 > clip.y1) {
            code0 |= _vdi_clip_bottom;
        }

        if (*x1 < clip.x0) {
            code1 |= _vdi_clip_left;
        } else if (*x1 > clip.x1) {
            code1 |= _vdi_clip_right;
        }
        if (*y1 < clip.y0) {
            code1 |= _vdi_clip_top;
        } else if (*y1 > clip.y1) {
            code1 |= _vdi_clip_bottom;
        }

        if ((code0 | code1) == 0) {
            return 1;
        }
        if ((code0 & code1) != 0) {
            return 0;
        }

        out_code = (code0 != 0) ? code0 : code1;
        if ((out_code & _vdi_clip_top) != 0) {
            if (*y1 == *y0) {
                return 0;
            }
            x = *x0 + ((long) (*x1 - *x0) * (clip.y0 - *y0)) /
                (*y1 - *y0);
            y = clip.y0;
        } else if ((out_code & _vdi_clip_bottom) != 0) {
            if (*y1 == *y0) {
                return 0;
            }
            x = *x0 + ((long) (*x1 - *x0) * (clip.y1 - *y0)) /
                (*y1 - *y0);
            y = clip.y1;
        } else if ((out_code & _vdi_clip_right) != 0) {
            if (*x1 == *x0) {
                return 0;
            }
            y = *y0 + ((long) (*y1 - *y0) * (clip.x1 - *x0)) /
                (*x1 - *x0);
            x = clip.x1;
        } else {
            if (*x1 == *x0) {
                return 0;
            }
            y = *y0 + ((long) (*y1 - *y0) * (clip.x0 - *x0)) /
                (*x1 - *x0);
            x = clip.x0;
        }

        if (out_code == code0) {
            *x0 = (WORD) x;
            *y0 = (WORD) y;
        } else {
            *x1 = (WORD) x;
            *y1 = (WORD) y;
        }
    }
}

static void _vdi_hline_to_row(uint8_t *row, WORD left, WORD right, WORD color)
{
    WORD mode = _vdi_write_mode();
    size_t start_byte = (size_t) left / 8u;
    size_t end_byte = (size_t) right / 8u;
    unsigned int start_bit = (unsigned int) left & 7u;
    unsigned int end_bit = (unsigned int) right & 7u;
    uint8_t left_mask = (uint8_t) (0xffu >> start_bit);
    uint8_t right_mask = (uint8_t) (0xffu << (7u - end_bit));

#define _APPLY(byte_ref, mask) \
    switch (mode) { \
    case 2: if (color != 0) (byte_ref) |= (mask); break; \
    case 3: if (color != 0) (byte_ref) ^= (mask); break; \
    case 4: (byte_ref) &= (uint8_t) ~(mask); break; \
    default: \
        if (color != 0) (byte_ref) |= (mask); \
        else (byte_ref) &= (uint8_t) ~(mask); \
        break; \
    }

    if (start_byte == end_byte) {
        uint8_t mask = left_mask & right_mask;

        _APPLY(row[start_byte], mask);
        return;
    }

    _APPLY(row[start_byte], left_mask);

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
            memset(&row[mid_start], (color != 0) ? 0xffu : 0x00u, mid_count);
            break;
        }
    }

    _APPLY(row[end_byte], right_mask);

#undef _APPLY
}

void _vdi_draw_screen_hline_direct(WORD y, WORD left, WORD right, WORD color)
{
    uint8_t *row;

    if (left > right) {
        WORD tmp = left;

        left = right;
        right = tmp;
    }
    row = _vdi_screen_row_mutable(y);
    if (row == NULL) {
        return;
    }
    _vdi_hline_to_row(row, left, right, color);
}

void _vdi_draw_screen_hline(WORD y, WORD x0, WORD x1, WORD color)
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

    _vdi_get_active_clip_rect(&clip);
    if (y < clip.y0 || y > clip.y1) {
        return;
    }

    left = (x0 < clip.x0) ? clip.x0 : x0;
    right = (x1 > clip.x1) ? clip.x1 : x1;
    if (left > right) {
        return;
    }

    row = _vdi_screen_row_mutable(y);
    if (row == NULL) {
        return;
    }

    _vdi_hline_to_row(row, left, right, color);
}

void _vdi_draw_line_segment(WORD x0, WORD y0, WORD x1, WORD y1, WORD color)
{
    WORD dx;
    WORD sx;
    WORD dy;
    WORD sy;
    WORD err;

    if (!_vdi_clip_line_segment(&x0, &y0, &x1, &y1)) {
        return;
    }

    _vdi_prepare_screen_write();

    if (y0 == y1) {
        _vdi_draw_screen_hline_direct(y0, x0, x1, color);
        return;
    }

    if (x0 == x1) {
        uint8_t mask = _vdi_screen_mask_for_x(x0);
        size_t byte_off = (size_t) x0 / 8u;
        WORD mode = _vdi_write_mode();
        WORD min_y = (y0 < y1) ? y0 : y1;
        WORD max_y = (y0 < y1) ? y1 : y0;
        size_t pitch = _vdi.surface->pitch;
        uint8_t *row = (uint8_t *) _vdi.surface->pixels +
            (size_t) min_y * pitch;
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
                row[byte_off] &= (uint8_t) ~mask;
                break;
            default:
                if (color != 0) {
                    row[byte_off] |= mask;
                } else {
                    row[byte_off] &= (uint8_t) ~mask;
                }
                break;
            }
        }
        return;
    }

    dx = (WORD) abs(x1 - x0);
    sx = (WORD) ((x0 < x1) ? 1 : -1);
    dy = (WORD) -abs(y1 - y0);
    sy = (WORD) ((y0 < y1) ? 1 : -1);
    err = (WORD) (dx + dy);

    FOREVER {
        WORD e2 = (WORD) (2 * err);

        _vdi_set_screen_pixel_raw(x0, y0, color);
        if (x0 == x1 && y0 == y1) {
            return;
        }

        if (e2 >= dy) {
            err = (WORD) (err + dy);
            x0 = (WORD) (x0 + sx);
        }
        if (e2 <= dx) {
            err = (WORD) (err + dx);
            y0 = (WORD) (y0 + sy);
        }
    }
}

void _vdi_fill_rect(WORD x0, WORD y0, WORD x1, WORD y1, WORD color)
{
    vdi_rect_t rect;
    vdi_rect_t clip;
    WORD mode;
    size_t pitch;
    WORD height;
    uint8_t *base;
    size_t start_byte;
    size_t end_byte;
    unsigned int start_bit;
    unsigned int end_bit;
    uint8_t fill_byte;
    uint8_t left_mask;
    uint8_t right_mask;
    WORD row;
    WORD fill_mode;

    rect.x0 = (x0 < x1) ? x0 : x1;
    rect.x1 = (x0 < x1) ? x1 : x0;
    rect.y0 = (y0 < y1) ? y0 : y1;
    rect.y1 = (y0 < y1) ? y1 : y0;

    _vdi_get_active_clip_rect(&clip);
    if (!_vdi_intersect_rects(&rect, &clip, &rect)) {
        return;
    }

    _vdi_prepare_screen_write();

    mode = _vdi_write_mode();
    fill_mode = _vdi_compat.fill_interior;
    pitch = _vdi.surface->pitch;
    height = (WORD) (rect.y1 - rect.y0 + 1);
    base = (uint8_t *) _vdi.surface->pixels + (size_t) rect.y0 * pitch;
    start_byte = (size_t) rect.x0 / 8u;
    end_byte = (size_t) rect.x1 / 8u;
    start_bit = (unsigned int) rect.x0 & 7u;
    end_bit = (unsigned int) rect.x1 & 7u;

    if ((mode == 1 || mode == 4) && fill_mode == VDI_FILL_SOLID) {
        fill_byte = (mode == 1 && color != 0) ? 0xffu : 0x00u;

        if (start_byte == 0 && end_byte == pitch - 1 &&
            start_bit == 0 && end_bit == 7) {
            memset(base, fill_byte, pitch * (size_t) height);
            return;
        }

        left_mask = (uint8_t) (0xffu >> start_bit);
        right_mask = (uint8_t) (0xffu << (7u - end_bit));

        if (start_byte == end_byte) {
            uint8_t mask = left_mask & right_mask;

            for (row = 0; row < height; ++row, base += pitch) {
                if (fill_byte != 0u) {
                    base[start_byte] |= mask;
                } else {
                    base[start_byte] &= (uint8_t) ~mask;
                }
            }
            return;
        }

        for (row = 0; row < height; ++row, base += pitch) {
            if (fill_byte != 0u) {
                base[start_byte] |= left_mask;
            } else {
                base[start_byte] &= (uint8_t) ~left_mask;
            }
            if (end_byte > start_byte + 1u) {
                memset(&base[start_byte + 1u], fill_byte,
                    end_byte - start_byte - 1u);
            }
            if (fill_byte != 0u) {
                base[end_byte] |= right_mask;
            } else {
                base[end_byte] &= (uint8_t) ~right_mask;
            }
        }
        return;
    }

    for (row = 0; row < height; ++row, base += pitch) {
        WORD y = (WORD) (rect.y0 + row);
        WORD x;

        for (x = rect.x0; x <= rect.x1; ++x) {
            WORD pixel = _vdi_fill_pattern_bit(x, y, color);

            _vdi_apply_mask(&base[(size_t) x / 8u],
                _vdi_screen_mask_for_x(x), pixel);
        }
    }
}

int _vdi_uses_screen(const MFDB *mfdb)
{
    return mfdb == NULL || mfdb->fd_addr == NULL;
}

WORD _vdi_mfdb_get_pixel(const MFDB *mfdb, WORD x, WORD y)
{
    return (WORD) _vdi_mfdb_sample_direct(mfdb, x, y);
}

void _vdi_mfdb_set_pixel(MFDB *mfdb, WORD x, WORD y, WORD color)
{
    if (_vdi_uses_screen(mfdb)) {
        if (_vdi_point_visible(x, y)) {
            _vdi_set_screen_pixel(x, y, color);
        }
        return;
    }

    _vdi_mfdb_draw_hline(mfdb, y, x, x, color);
}

void _vdi_mfdb_draw_hline(MFDB *mfdb, WORD y, WORD x0, WORD x1, WORD color)
{
    UWORD *row_words;
    WORD left;
    WORD right;
    WORD x;

    if (x0 > x1) {
        WORD tmp = x0;

        x0 = x1;
        x1 = tmp;
    }
    if (_vdi_uses_screen(mfdb)) {
        _vdi_draw_screen_hline(y, x0, x1, color);
        return;
    }
    if (mfdb == NULL || mfdb->fd_addr == NULL || mfdb->fd_nplanes != 1 ||
        y < 0 || y >= mfdb->fd_h) {
        return;
    }

    left = _vdi_clamp_word(x0, 0, (WORD) (mfdb->fd_w - 1));
    right = _vdi_clamp_word(x1, 0, (WORD) (mfdb->fd_w - 1));
    if (left > right) {
        return;
    }
    row_words = (UWORD *) mfdb->fd_addr +
        (size_t) y * (size_t) mfdb->fd_wdwidth;
    for (x = left; x <= right; ++x) {
        UWORD mask = (UWORD) (0x8000u >> ((unsigned int) x & 15u));
        UWORD *word = &row_words[(size_t) x / 16u];

        if (color != 0) {
            *word |= mask;
        } else {
            *word &= (UWORD) ~mask;
        }
    }
}
