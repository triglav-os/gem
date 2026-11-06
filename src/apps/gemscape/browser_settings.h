/*
 * Declares Gemscape's persistent browser preferences and native settings
 * window.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (C) 2026 tomaz stih
 */
#ifndef GEMSCAPE_BROWSER_SETTINGS_H
#define GEMSCAPE_BROWSER_SETTINGS_H
#include <gem/gemctl.h>

typedef struct gemscape_preferences {
    char homepage[1024];
    int zoom;
    int persistent;
    int session_cookies;
    int cookies;
    int javascript;
    int images;
    int frame_rate;
} gemscape_preferences_t;

typedef struct gemscape_settings_dialog {
    WORD handle;
    WORD vdi;
    int tab;
    int pressed;
    gemscape_preferences_t draft;
    gemctl_text_edit_t home_edit;
    char message[128];
} gemscape_settings_dialog_t;

enum {
    gemscape_settings_none,
    gemscape_settings_saved,
    gemscape_settings_cookies,
    gemscape_settings_cache,
    gemscape_settings_site
};

/* Loads validated preferences from the XDG gemscape component file. */
void gemscape_preferences_load(gemscape_preferences_t *preferences);
/* Atomically saves preferences, returning zero on failure. */
int gemscape_preferences_save(const gemscape_preferences_t *preferences);
/* Resolves and creates a private browser profile; returns zero on failure. */
int gemscape_profile_path(char *path, size_t capacity);
/* Resolve/create disposable browser cache storage, separately from cookies. */
int gemscape_cache_path(char *path, size_t capacity);
/* Resolve the CEF diagnostic log in persistent state storage. */
int gemscape_log_path(char *path, size_t capacity);
/* Shows a website alert (0), confirmation (1), or prompt (2); bounds prompt
 * text. */
int gemscape_script_dialog(int kind, const char *origin, const char *message,
                           char *text, size_t capacity);
/* Opens or raises the non-blocking tabbed preferences window. */
void gemscape_settings_open(gemscape_settings_dialog_t *dialog, WORD vdi,
                            WORD parent,
                            const gemscape_preferences_t *preferences);
/* Closes the settings window and discards unapplied edits. */
void gemscape_settings_close(gemscape_settings_dialog_t *dialog);
/* Draws the visible portions of the settings window. */
void gemscape_settings_draw(gemscape_settings_dialog_t *dialog);
/* Handles a window message owned by this dialog. */
void gemscape_settings_message(gemscape_settings_dialog_t *dialog,
                               const WORD *message);
/* Handles a pointer press in settings; returns a requested browser action. */
int gemscape_settings_click(gemscape_settings_dialog_t *dialog, WORD x, WORD y,
                            gemscape_preferences_t *preferences);
/* Handles keyboard editing; returns a requested browser action. */
int gemscape_settings_key(gemscape_settings_dialog_t *dialog, WORD key,
                          WORD modifiers, gemscape_preferences_t *preferences);
#endif
