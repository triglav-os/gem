/*
 * Verify the Desktop program-icon resource loader: an icon-only resource in
 * the `resgen icons` layout is copied exactly, while resources with object
 * trees, oversized icons and out-of-bounds offsets are refused.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (C) 2026 tomaz stih
 */
#include "desktop_icon_rsc.h"

#include <assert.h>
#include <stdint.h>
#include <string.h>

enum { PLANE_BYTES = 2 * 32 * sizeof(UWORD) };

static _Alignas(ICONBLK) unsigned char file[1024];
static size_t file_size;

/* Lay out RSHDR, one ICONBLK and its planes the way resgen does. */
static void build(void)
{
    RSHDR header;
    ICONBLK icon;
    size_t icon_at = (sizeof(header) + 7u) & ~(size_t)7u;
    size_t planes_at = icon_at + sizeof(icon);

    memset(file, 0, sizeof(file));
    memset(&header, 0, sizeof(header));
    memset(&icon, 0, sizeof(icon));
    header.rsh_iconblk = (WORD)icon_at;
    header.rsh_nib = 1;
    icon.ib_pmask = (LONG)planes_at;
    icon.ib_pdata = (LONG)(planes_at + PLANE_BYTES);
    icon.ib_wicon = 32;
    icon.ib_hicon = 32;
    for (size_t i = 0; i < PLANE_BYTES / sizeof(UWORD); ++i) {
        UWORD mask = (UWORD)(0xff00u >> (i % 8));
        UWORD data = (UWORD)(i * 0x0101u);

        memcpy(file + planes_at + i * sizeof(UWORD), &mask, sizeof(mask));
        memcpy(file + planes_at + PLANE_BYTES + i * sizeof(UWORD), &data,
               sizeof(data));
    }
    memcpy(file, &header, sizeof(header));
    memcpy(file + icon_at, &icon, sizeof(icon));
    file_size = planes_at + 2u * PLANE_BYTES;
}

static ICONBLK *icon_block(void)
{
    RSHDR header;

    memcpy(&header, file, sizeof(header));
    return (ICONBLK *)(void *)(file + (UWORD)header.rsh_iconblk);
}

static RSHDR *header_block(void)
{
    return (RSHDR *)(void *)file;
}

int main(void)
{
    desktop_rsc_icon_t icon;

    build();
    assert(desktop_icon_rsc_decode(file, file_size, &icon));
    assert(icon.width == 32 && icon.height == 32);
    assert(memcmp(icon.mask, file + icon_block()->ib_pmask, PLANE_BYTES) == 0);
    assert(memcmp(icon.data, file + icon_block()->ib_pdata, PLANE_BYTES) == 0);

    /* Smaller icons keep their own geometry and word layout. */
    icon_block()->ib_wicon = 16;
    icon_block()->ib_hicon = 16;
    assert(desktop_icon_rsc_decode(file, file_size, &icon));
    assert(icon.width == 16 && icon.height == 16);
    assert(memcmp(icon.mask, file + icon_block()->ib_pmask, 32) == 0);

    /* An application's dialog resource is not a program icon. */
    build();
    header_block()->rsh_ntree = 1;
    assert(!desktop_icon_rsc_decode(file, file_size, &icon));
    build();
    header_block()->rsh_nobs = 3;
    assert(!desktop_icon_rsc_decode(file, file_size, &icon));
    build();
    header_block()->rsh_nib = 0;
    assert(!desktop_icon_rsc_decode(file, file_size, &icon));

    /* Oversized or empty icons are refused. */
    build();
    icon_block()->ib_wicon = 33;
    assert(!desktop_icon_rsc_decode(file, file_size, &icon));
    build();
    icon_block()->ib_hicon = 0;
    assert(!desktop_icon_rsc_decode(file, file_size, &icon));

    /* Every plane and table must lie inside the file. */
    build();
    assert(!desktop_icon_rsc_decode(file, file_size - 1u, &icon));
    icon_block()->ib_pmask = -2;
    assert(!desktop_icon_rsc_decode(file, file_size, &icon));
    build();
    icon_block()->ib_pdata = (LONG)file_size;
    assert(!desktop_icon_rsc_decode(file, file_size, &icon));
    build();
    header_block()->rsh_iconblk = (WORD)(file_size - 4u);
    assert(!desktop_icon_rsc_decode(file, file_size, &icon));
    build();
    header_block()->rsh_iconblk = (WORD)0xfff0;
    assert(!desktop_icon_rsc_decode(file, file_size, &icon));
    assert(!desktop_icon_rsc_decode(file, sizeof(RSHDR) - 1u, &icon));
    assert(!desktop_icon_rsc_decode(NULL, file_size, &icon));
    return 0;
}
