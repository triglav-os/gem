/*
 * Declares monochrome Atari ST desktop icon assets converted from the
 * original reference PNGs. Each icon exposes both its transparency mask
 * and black-pixel bitmap so the portable desktop can render them
 * through hosted GEM AES/VDI callbacks without legacy desktop glue.
 *
 * MIT License (see: LICENSE)
 * Copyright (C) 2026 tomaz stih
 */

#ifndef GEM_DESKTOP_ASSETS_H
#define GEM_DESKTOP_ASSETS_H

#include <gem/aes.h>

typedef struct desktop_icon_asset {
    const UWORD *mask_bits;
    const UWORD *data_bits;
    WORD width;
    WORD height;
} desktop_icon_asset_t;

extern const desktop_icon_asset_t desktop_disk_icon_asset;
extern const desktop_icon_asset_t desktop_doc_icon_asset;
extern const desktop_icon_asset_t desktop_folder_icon_asset;
extern const desktop_icon_asset_t desktop_prg_icon_asset;
extern const desktop_icon_asset_t desktop_trash_icon_asset;

#endif
