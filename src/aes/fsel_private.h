/*
 * Declares the hosted file chooser's private model, modern GEM layout and
 * navigation helpers. Open, save and directory modes share this state while
 * retaining the public AES buffer conventions.
 *
 * MIT License (see: LICENSE)
 * Copyright (C) 2026 tomaz stih
 */

#ifndef GEM_AES_FSEL_PRIVATE_H
#define GEM_AES_FSEL_PRIVATE_H

#include "aes_internal.h"
#include <gem/gemctl.h>

#include <stddef.h>
#include <stdint.h>

enum {
    AES_FSEL_MAX_ENTRIES = 256,
    AES_FSEL_MAX_LOCATIONS = 16,
    AES_FSEL_MAX_HISTORY = 16,
    AES_FSEL_LINE_LEN = AES_PATH_LEN,
    AES_FSEL_MARGIN = 8,
    AES_FSEL_TOOL_HEIGHT = 26,
    AES_FSEL_BUTTON_HEIGHT = 26,
    AES_FSEL_BUTTON_WIDTH = 82,
    AES_FSEL_LOCATION_WIDTH = 126,
    AES_FSEL_ICON_WIDTH = 16,
    AES_FSEL_OPEN = 0,
    AES_FSEL_SAVE = 1,
    AES_FSEL_DIRECTORY = 2,
    AES_FSEL_LOCATION = 3,
    AES_FSEL_CONTROL_NONE = 0,
    AES_FSEL_CONTROL_ACCEPT = 1,
    AES_FSEL_CONTROL_CANCEL = 2,
    AES_FSEL_CONTROL_BACK = 3,
    AES_FSEL_CONTROL_FORWARD = 4,
    AES_FSEL_CONTROL_UP = 5,
    AES_FSEL_CONTROL_SCROLL_UP = 6,
    AES_FSEL_CONTROL_SCROLL_DOWN = 7
};

/* Copy text into a chooser buffer, truncating it with a final null byte. */
void aes_fsel_copy_text(char *target, size_t target_size, const char *source);

typedef struct aes_fsel_entry {
    char name[AES_PATH_LEN];
    uint64_t size_bytes;
    WORD is_directory;
} aes_fsel_entry_t;

typedef struct aes_fsel_location {
    char label[32];
    char path[AES_PATH_LEN];
} aes_fsel_location_t;

typedef struct aes_fsel_state {
    WORD handle;
    WORD mode;
    WORD char_w;
    WORD char_h;
    WORD text_ascent;
    WORD row_h;
    WORD visible_rows;
    WORD top_index;
    WORD selected;
    WORD entry_count;
    WORD selected_location;
    WORD location_count;
    WORD history_count;
    WORD history_index;
    WORD done;
    WORD confirmed;
    WORD editing_name;
    WORD show_hidden;
    gemctl_text_edit_t path_edit;
    WORD pressed_control;
    WORD last_click_entry;
    uint32_t last_click_ms;
    GRECT work;
    MFDB background;
    char title[64];
    char directory[AES_PATH_LEN];
    char pattern[AES_PATH_LEN];
    char selection[AES_PATH_LEN];
    aes_fsel_entry_t entries[AES_FSEL_MAX_ENTRIES];
    aes_fsel_location_t locations[AES_FSEL_MAX_LOCATIONS];
    char history[AES_FSEL_MAX_HISTORY][AES_PATH_LEN];
} aes_fsel_state_t;

/* Strip trailing separators, keeping a lone root. */
void aes_fsel_normalize_dir(char *path);
/* Replace a directory path with its parent. */
void aes_fsel_parent_dir(char *path);
/* Split the caller's path and selection into directory, pattern and name. */
void aes_fsel_split_input(const char *pipath, const char *pisel,
                          char *directory, size_t directory_size, char *pattern,
                          size_t pattern_size, char *selection,
                          size_t selection_size);
/* Join a directory and name; nonzero when the result fits. */
int aes_fsel_join_path(const char *directory, const char *name, char *path,
                       size_t path_size);
/* Match one case-insensitive wildcard pattern. */
int aes_fsel_match_pattern(const char *pattern, const char *name);
/* Copy the highlighted file into the filename field. */
void aes_fsel_set_selection_from_index(aes_fsel_state_t *state);
/* Rescan the directory through the pattern, yielding periodically. */
void aes_fsel_reload_entries(aes_fsel_state_t *state);
/* Build the concise Home, Workspace, Computer and mounted-volume list. */
void aes_fsel_build_locations(aes_fsel_state_t *state);
/* Navigate to a directory and optionally append it to history. */
void aes_fsel_navigate(aes_fsel_state_t *state, const char *path, int remember);
/* Enter a named child or the `..` parent entry. */
void aes_fsel_enter_directory(aes_fsel_state_t *state, const char *name);
/* Move backward or forward through chooser history. */
void aes_fsel_move_history(aes_fsel_state_t *state, WORD delta);
/* Capture the pre-modal screen for synchronous redraw restoration. */
void aes_fsel_snapshot(aes_fsel_state_t *state);
/* Restore an exposed owner from the pre-modal screen snapshot. */
void aes_fsel_restore_owner(const aes_fsel_state_t *state, WORD msg[8]);
/* Return the file pane used by both painting and pointer hit testing. */
void aes_fsel_file_rect(const aes_fsel_state_t *state, GRECT *rect);
/* Return the Locations pane used by painting and pointer hit testing. */
void aes_fsel_location_rect(const aes_fsel_state_t *state, GRECT *rect);
/* Return the editable filename field. */
void aes_fsel_name_rect(const aes_fsel_state_t *state, GRECT *rect);
/* Return the editable toolbar path field. */
void aes_fsel_path_rect(const aes_fsel_state_t *state, GRECT *rect);
/* Return the first file/location row coordinate. */
WORD aes_fsel_rows_top(const aes_fsel_state_t *state);
/* Return the coordinate below the file and Locations panes. */
WORD aes_fsel_body_bottom(const aes_fsel_state_t *state);
/* Refresh cached metrics from the selector window's work area. */
void aes_fsel_sync_work(aes_fsel_state_t *state);
/* Return one toolbar/action/scroll control rectangle. */
void aes_fsel_control_rect(const aes_fsel_state_t *state, WORD control,
                           GRECT *rect);
/* Return the file row at a point, or NIL. */
WORD aes_fsel_entry_at(const aes_fsel_state_t *state, WORD x, WORD y);
/* Return the location row at a point, or NIL. */
WORD aes_fsel_location_at(const aes_fsel_state_t *state, WORD x, WORD y);
/* Return nonzero when a point lies in the filename field. */
int aes_fsel_name_hit(const aes_fsel_state_t *state, WORD x, WORD y);
/* Return nonzero when a point lies in the toolbar path field. */
int aes_fsel_path_hit(const aes_fsel_state_t *state, WORD x, WORD y);
/* Adjust the top index so the selection is visible. */
void aes_fsel_scroll_into_view(aes_fsel_state_t *state);
/* Repaint the selector, limited to dirty when given. */
void aes_fsel_draw(const aes_fsel_state_t *state, const GRECT *dirty);

#endif
