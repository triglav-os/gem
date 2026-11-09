/*
 * Declares the Desktop's loader for program icon resources. A program
 * icon is an icon-only GEM resource as written by `resgen icons` from a
 * 32x32 PNG in the style of samples/data/artwork/icons: one ICONBLK whose
 * mask and data planes use the same monochrome 16-bit word layout as the
 * built-in desktop icons. Parsing works on a memory buffer, performs no
 * file access and bounds-checks every offset because the file is untrusted.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (C) 2026 tomaz stih
 */

#ifndef GEM_DESKTOP_ICON_RSC_H
#define GEM_DESKTOP_ICON_RSC_H

#include <gem/aes.h>

#include <stddef.h>

enum {
    DESKTOP_RSC_ICON_MAX = 32,
    DESKTOP_RSC_ICON_WORDS = DESKTOP_RSC_ICON_MAX * DESKTOP_RSC_ICON_MAX / 16,
    /* Largest icon resource the Desktop reads. */
    DESKTOP_RSC_ICON_MAX_FILE = 64 * 1024
};

/* One icon copied out of a resource, in desktop_icon_asset_t layout. */
typedef struct desktop_rsc_icon {
    UWORD mask[DESKTOP_RSC_ICON_WORDS]; /* Opaque pixels. */
    UWORD data[DESKTOP_RSC_ICON_WORDS]; /* Black (ink) pixels. */
    WORD width;
    WORD height;
} desktop_rsc_icon_t;

/*
 * Copies the first ICONBLK of the resource held in `bytes` into `out`.
 * The resource must contain no object trees (an icon-only resource such
 * as `resgen icons` writes), and the icon must be at most 32x32 with both
 * planes inside the buffer. Returns nonzero when `out` holds the icon.
 */
int desktop_icon_rsc_decode(const unsigned char *bytes, size_t size,
                            desktop_rsc_icon_t *out);

#endif
