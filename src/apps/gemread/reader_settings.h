/*
 * Gemread's validated persistent display and file-selector preferences.
 *
 * SPDX-License-Identifier: AGPL-3.0-or-later
 * Copyright (C) 2026 tomaz stih
 */
#ifndef GEMREAD_SETTINGS_H
#define GEMREAD_SETTINGS_H
#include <gem.h>
#include <gem/os.h>

typedef struct gemread_preferences {
    int zoom, fit_width, thumbnails, antialias, remember_folder;
    char folder[GEM_OS_PATH_MAX];
} gemread_preferences_t;

/* Load defaults and validated overrides from gemread.cfg. */
void gemread_preferences_load(gemread_preferences_t *preferences);
/* Atomically merge preferences into gemread.cfg; return nonzero on success. */
int gemread_preferences_save(const gemread_preferences_t *preferences);
/* Show a tabbed settings form; change preferences only after a successful Save.
 */
int gemread_settings_dialog(gemread_preferences_t *preferences, WORD parent,
                            WORD vdi);
#endif
