/*
 * Fills rectangles into the hosted VDI's packed monochrome surface: the
 * solid replace/erase fast path, the built-in and user 16x16 patterns,
 * and transparent, XOR and erase write modes applied per byte.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (C) 2026 tomaz stih
 */

#include "vdi_internal.h"

#include "vdi_state.h"

#include <string.h>

enum {
    VDI_FILL_HOLLOW = 0,
    VDI_FILL_SOLID = 1,
    VDI_FILL_PATTERN = 2,
    VDI_FILL_HATCH = 3,
    VDI_FILL_USER = 4
};

static const UWORD g_vdi_builtin_patterns[8][16] = {
    {0xffffu, 0xffffu, 0xffffu, 0xffffu, 0xffffu, 0xffffu, 0xffffu, 0xffffu,
     0xffffu, 0xffffu, 0xffffu, 0xffffu, 0xffffu, 0xffffu, 0xffffu, 0xffffu},
    {0xaaaau, 0x5555u, 0xaaaau, 0x5555u, 0xaaaau, 0x5555u, 0xaaaau, 0x5555u,
     0xaaaau, 0x5555u, 0xaaaau, 0x5555u, 0xaaaau, 0x5555u, 0xaaaau, 0x5555u},
    {0x8888u, 0x2222u, 0x8888u, 0x2222u, 0x8888u, 0x2222u, 0x8888u, 0x2222u,
     0x8888u, 0x2222u, 0x8888u, 0x2222u, 0x8888u, 0x2222u, 0x8888u, 0x2222u},
    {0xccccu, 0x3333u, 0xccccu, 0x3333u, 0xccccu, 0x3333u, 0xccccu, 0x3333u,
     0xccccu, 0x3333u, 0xccccu, 0x3333u, 0xccccu, 0x3333u, 0xccccu, 0x3333u},
    {0xf0f0u, 0x0f0fu, 0xf0f0u, 0x0f0fu, 0xf0f0u, 0x0f0fu, 0xf0f0u, 0x0f0fu,
     0xf0f0u, 0x0f0fu, 0xf0f0u, 0x0f0fu, 0xf0f0u, 0x0f0fu, 0xf0f0u, 0x0f0fu},
    {0xff00u, 0x0000u, 0xff00u, 0x0000u, 0xff00u, 0x0000u, 0xff00u, 0x0000u,
     0xff00u, 0x0000u, 0xff00u, 0x0000u, 0xff00u, 0x0000u, 0xff00u, 0x0000u},
    {0xffffu, 0x0000u, 0xffffu, 0x0000u, 0xffffu, 0x0000u, 0xffffu, 0x0000u,
     0xffffu, 0x0000u, 0xffffu, 0x0000u, 0xffffu, 0x0000u, 0xffffu, 0x0000u},
    {0x8080u, 0x0808u, 0x8080u, 0x0808u, 0x8080u, 0x0808u, 0x8080u, 0x0808u,
     0x8080u, 0x0808u, 0x8080u, 0x0808u, 0x8080u, 0x0808u, 0x8080u, 0x0808u}};

static const UWORD *vdi_fill_pattern_row(WORD y)
{
    const UWORD *pattern;

    if (vdi_compat.fill_interior == VDI_FILL_USER) {
        pattern = (const UWORD *)vdi_compat.fill_pattern;
    } else {
        WORD style = vdi_compat.fill_style;

        if (style < 1 || style > 8) {
            style = 2;
        }
        pattern = g_vdi_builtin_patterns[style - 1];
    }
    return &pattern[(unsigned int)y & 15u];
}

/* Expand a 16-bit GEM pattern word into two mono screen bytes (x&16). */
static void vdi_pattern_to_bytes(UWORD pat16, uint8_t out[2])
{
    int i;

    out[0] = 0u;
    out[1] = 0u;
    for (i = 0; i < 8; ++i) {
        if ((pat16 & (UWORD)(0x8000u >> i)) != 0u) {
            out[0] |= (uint8_t)(0x80u >> i);
        }
        if ((pat16 & (UWORD)(0x8000u >> (i + 8))) != 0u) {
            out[1] |= (uint8_t)(0x80u >> i);
        }
    }
}

void vdi_fill_rect(WORD x0, WORD y0, WORD x1, WORD y1, WORD color)
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

    vdi_get_active_clip_rect(&clip);
    if (!vdi_intersect_rects(&rect, &clip, &rect)) {
        return;
    }

    vdi_prepare_screen_write();
    vdi_mark_dirty(rect.x0, rect.y0, rect.x1, rect.y1);

    mode = vdi_write_mode();
    fill_mode = vdi_compat.fill_interior;
    pitch = vdi_state.surface->pitch;
    height = (WORD)(rect.y1 - rect.y0 + 1);
    base = (uint8_t *)vdi_state.surface->pixels + (size_t)rect.y0 * pitch;
    start_byte = (size_t)rect.x0 / 8u;
    end_byte = (size_t)rect.x1 / 8u;
    start_bit = (unsigned int)rect.x0 & 7u;
    end_bit = (unsigned int)rect.x1 & 7u;

    left_mask = (uint8_t)(0xffu >> start_bit);
    right_mask = (uint8_t)(0xffu << (7u - end_bit));

    if (fill_mode == VDI_FILL_HOLLOW) {
        return;
    }

    if (fill_mode == VDI_FILL_SOLID && (mode == 1 || mode == 4)) {
        fill_byte = (mode == 1 && color != 0) ? 0xffu : 0x00u;

        if (start_byte == 0 && end_byte == pitch - 1 && start_bit == 0 &&
            end_bit == 7) {
            memset(base, fill_byte, pitch * (size_t)height);
            return;
        }

        if (start_byte == end_byte) {
            uint8_t mask = left_mask & right_mask;

            for (row = 0; row < height; ++row, base += pitch) {
                if (fill_byte != 0u) {
                    base[start_byte] |= mask;
                } else {
                    base[start_byte] &= (uint8_t)~mask;
                }
            }
            return;
        }

        for (row = 0; row < height; ++row, base += pitch) {
            if (fill_byte != 0u) {
                base[start_byte] |= left_mask;
            } else {
                base[start_byte] &= (uint8_t)~left_mask;
            }
            if (end_byte > start_byte + 1u) {
                memset(&base[start_byte + 1u], fill_byte,
                       end_byte - start_byte - 1u);
            }
            if (fill_byte != 0u) {
                base[end_byte] |= right_mask;
            } else {
                base[end_byte] &= (uint8_t)~right_mask;
            }
        }
        return;
    }

    /*
     * Patterned / XOR / OR fills: operate on mono shadow bytes. Never
     * walk individual pixels for area fills.
     */
    for (row = 0; row < height; ++row, base += pitch) {
        WORD y = (WORD)(rect.y0 + row);
        UWORD pat16;
        uint8_t pat_bytes[2];
        size_t b;

        if (fill_mode == VDI_FILL_SOLID) {
            pat16 = 0xffffu;
        } else {
            pat16 = *vdi_fill_pattern_row(y);
        }
        vdi_pattern_to_bytes(pat16, pat_bytes);

        for (b = start_byte; b <= end_byte; ++b) {
            uint8_t span;
            uint8_t pat;
            uint8_t desired;

            if (start_byte == end_byte) {
                span = (uint8_t)(left_mask & right_mask);
            } else if (b == start_byte) {
                span = left_mask;
            } else if (b == end_byte) {
                span = right_mask;
            } else {
                span = 0xffu;
            }

            /* Pattern byte for absolute column group b*8 (16-bit period). */
            pat = pat_bytes[(b & 1u) != 0u ? 1u : 0u];
            /*
             * Replace mode: pattern-on → color, pattern-off → opposite.
             * That is desired = color ? pat : ~pat within span.
             */
            if (color != 0) {
                desired = pat;
            } else {
                desired = (uint8_t)~pat;
            }

            switch (mode) {
                case 2: /* transparent / OR: only set where pattern wants ink */
                    if (color != 0) {
                        base[b] |= (uint8_t)(pat & span);
                    }
                    break;
                case 3: /* XOR */
                    if (color != 0) {
                        base[b] ^= (uint8_t)(pat & span);
                    }
                    break;
                case 4: /* erase */
                    base[b] &= (uint8_t) ~(span);
                    break;
                case 1:
                default: /* replace */
                    base[b] = (uint8_t)((base[b] & (uint8_t)~span) |
                                        (desired & span));
                    break;
            }
        }
    }
}
