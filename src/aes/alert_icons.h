/*
 * Declares monochrome alert icon assets converted from docs/orig/icons
 * so hosted AES alerts can draw Atari-style note, question, and stop
 * pictograms without depending on external image loading at runtime.
 *
 * MIT License (see: LICENSE)
 * Copyright (C) 2026 tomaz stih
 */

#ifndef GEM_SRC_AES_ALERT_ICONS_H
#define GEM_SRC_AES_ALERT_ICONS_H

#include <gem/aes.h>

typedef struct aes_alert_icon_asset {
    const UWORD *mask_bits;
    const UWORD *data_bits;
    WORD width;
    WORD height;
} aes_alert_icon_asset_t;

extern const aes_alert_icon_asset_t aes_alert_exclamation_icon_asset;
extern const aes_alert_icon_asset_t aes_alert_question_icon_asset;
extern const aes_alert_icon_asset_t aes_alert_stop_icon_asset;

#endif
