/*
 * Resolves shared Desktop, File Manager and Trash icon resources, with
 * bounded caching and stable storage for built-in icon replacements.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (C) 2026 tomaz stih
 */

#ifndef GEM_DESKTOP_ICON_LOOKUP_H
#define GEM_DESKTOP_ICON_LOOKUP_H

#include "desktop_assets.h"
#include "desktop_icon_rsc.h"

typedef struct desktop_custom_icon {
    desktop_icon_asset_t asset;
    desktop_rsc_icon_t bitmap;
} desktop_custom_icon_t;

/* Look up a leaf name without its extension; NULL means absent or invalid.
 * Cached pointers are valid only until the next lookup or cache refresh. */
const desktop_icon_asset_t *desktop_program_icon(const char *program);
/* Look up `icons/.<ext>.rsc` for a file name's extension, lowercased; the
 * longest multi-part extension wins. NULL when none exists or is valid.
 * Cached pointers are valid only until the next lookup or cache refresh. */
const desktop_icon_asset_t *desktop_extension_icon(const char *file);
/* Reload common icons and discard cached per-entry lookups. */
void desktop_program_icons_forget(void);
/* Resolve folder, disk, document, program or trash before built-in artwork.
 * Returned common assets have stable addresses across cache refreshes. */
const desktop_icon_asset_t *
desktop_standard_icon(const desktop_icon_asset_t *fallback);
/* Try a named icon, then a well-known folder name (Home, Programs, Documents,
 * Library, ignoring ASCII case) or a file extension, then its common type
 * icon, then built-in artwork.
 * Use the result immediately, before another lookup or cache refresh. */
const desktop_icon_asset_t *
desktop_named_icon(const char *name, const desktop_icon_asset_t *fallback);
/* Resolve an icon and copy its bitmap into caller-owned persistent storage.
 * The fallback must be one of the built-in Desktop assets. */
const desktop_icon_asset_t *
desktop_icon_copy(const char *name, const desktop_icon_asset_t *fallback,
                  desktop_custom_icon_t *storage);

#endif
