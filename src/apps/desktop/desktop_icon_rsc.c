/*
 * Loads a program's desktop icon from an icon-only GEM resource. The
 * header and ICONBLK are copied out with memcpy so unaligned input is
 * safe, resource offsets are treated as unsigned file offsets, and both
 * bitmap planes are checked to lie wholly inside the buffer before they
 * are copied. Resources with object trees are refused so that an
 * application's own dialog resource is never mistaken for its icon.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (C) 2026 tomaz stih
 */

#include "desktop_icon_rsc.h"

#include <stdint.h>
#include <string.h>

/* Nonzero when `length` bytes at file offset `offset` lie inside size. */
static int span_ok(intptr_t offset, size_t length, size_t size)
{
    return offset >= 0 && (uintptr_t)offset <= size &&
           length <= size - (size_t)offset;
}

int desktop_icon_rsc_decode(const unsigned char *bytes, size_t size,
                            desktop_rsc_icon_t *out)
{
    RSHDR header;
    ICONBLK icon;
    size_t plane;

    if (bytes == NULL || out == NULL || size < sizeof(header)) {
        return 0;
    }
    memcpy(&header, bytes, sizeof(header));
    if (header.rsh_ntree != 0 || header.rsh_nobs != 0 || header.rsh_nib < 1 ||
        !span_ok((UWORD)header.rsh_iconblk, sizeof(icon), size)) {
        return 0;
    }
    memcpy(&icon, bytes + (UWORD)header.rsh_iconblk, sizeof(icon));
    if (icon.ib_wicon < 1 || icon.ib_wicon > DESKTOP_RSC_ICON_MAX ||
        icon.ib_hicon < 1 || icon.ib_hicon > DESKTOP_RSC_ICON_MAX) {
        return 0;
    }
    plane = (size_t)((icon.ib_wicon + 15) / 16) * (size_t)icon.ib_hicon *
            sizeof(UWORD);
    if (!span_ok(icon.ib_pmask, plane, size) ||
        !span_ok(icon.ib_pdata, plane, size)) {
        return 0;
    }
    memset(out, 0, sizeof(*out));
    memcpy(out->mask, bytes + icon.ib_pmask, plane);
    memcpy(out->data, bytes + icon.ib_pdata, plane);
    out->width = icon.ib_wicon;
    out->height = icon.ib_hicon;
    return 1;
}
