/*
 * Implements a simple hosted AES file selector. This first pass keeps
 * the classic blocking `fsel_input()` API and provides directory
 * browsing, wildcard filtering, mouse selection, and keyboard
 * navigation on top of the existing hosted window manager.
 *
 * MIT License (see: LICENSE)
 * Copyright (C) 2026 tomaz stih
 */

#include "_aes.h"

#include "platform/os.h"

#include <ctype.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum {
    AES_FSEL_MAX_ENTRIES = 256,
    AES_FSEL_LINE_LEN = AES_PATH_LEN,
    AES_FSEL_MARGIN = 6,
    AES_FSEL_HEADER_LINES = 3,
    AES_FSEL_BUTTON_HEIGHT = 22,
    AES_FSEL_BUTTON_WIDTH = 68
};

typedef struct aes_fsel_entry {
    char name[AES_PATH_LEN];
    WORD is_directory;
} aes_fsel_entry_t;

typedef struct aes_fsel_state {
    WORD handle;
    WORD char_w;
    WORD char_h;
    WORD text_ascent;
    WORD row_h;
    WORD visible_rows;
    WORD top_index;
    WORD selected;
    WORD entry_count;
    WORD done;
    WORD confirmed;
    GRECT work;
    char directory[AES_PATH_LEN];
    char pattern[AES_PATH_LEN];
    char selection[AES_PATH_LEN];
    aes_fsel_entry_t entries[AES_FSEL_MAX_ENTRIES];
} aes_fsel_state_t;

static int aes_fsel_has_wildcards(const char *text);
static void aes_fsel_normalize_dir(char *path);
static void aes_fsel_parent_dir(char *path);
static void aes_fsel_split_input(const char *pipath, const char *pisel,
    char *directory, size_t directory_size,
    char *pattern, size_t pattern_size,
    char *selection, size_t selection_size);
static int aes_fsel_match_pattern(const char *pattern, const char *name);
static int aes_fsel_join_path(const char *directory, const char *name,
    char *path, size_t path_size);
static void aes_fsel_prefix_line(char *dst, size_t dst_size,
    const char *prefix, const char *value);
static int aes_fsel_entry_compare(const void *left, const void *right);
static void aes_fsel_set_selection_from_index(aes_fsel_state_t *state);
static void aes_fsel_reload_entries(aes_fsel_state_t *state);
static void aes_fsel_sync_work(aes_fsel_state_t *state);
static void aes_fsel_draw_hline(WORD x0, WORD x1, WORD y);
static void aes_fsel_draw_vline(WORD x, WORD y0, WORD y1);
static void aes_fsel_button_rects(const aes_fsel_state_t *state,
    GRECT *ok_rect, GRECT *cancel_rect);
static WORD aes_fsel_list_top(const aes_fsel_state_t *state);
static int aes_fsel_entry_rect(const aes_fsel_state_t *state, WORD row,
    GRECT *rect);
static void aes_fsel_scroll_into_view(aes_fsel_state_t *state);
static void aes_fsel_draw(const aes_fsel_state_t *state, const GRECT *dirty);
static void aes_fsel_enter_directory(aes_fsel_state_t *state,
    const char *name);
static void aes_fsel_activate(aes_fsel_state_t *state);
static void aes_fsel_move_selection(aes_fsel_state_t *state, WORD delta);
static void aes_fsel_page_selection(aes_fsel_state_t *state, WORD delta);
static void aes_fsel_accept(aes_fsel_state_t *state,
    char *pipath, char *pisel, WORD *pbutton);

static int aes_fsel_has_wildcards(const char *text)
{
    if (text == NULL) {
        return 0;
    }

    while (*text != '\0') {
        if (*text == '*' || *text == '?') {
            return 1;
        }
        ++text;
    }
    return 0;
}

static void aes_fsel_normalize_dir(char *path)
{
    size_t length;

    if (path == NULL || path[0] == '\0') {
        return;
    }

    length = strlen(path);
    while (length > 1 &&
           (path[length - 1u] == '/' || path[length - 1u] == '\\')) {
        path[length - 1u] = '\0';
        --length;
    }
}

static void aes_fsel_parent_dir(char *path)
{
    char *slash;

    if (path == NULL || path[0] == '\0') {
        return;
    }
    aes_fsel_normalize_dir(path);
    slash = strrchr(path, '/');
    if (slash == NULL) {
        strcpy(path, ".");
        return;
    }
    if (slash == path) {
        slash[1] = '\0';
        return;
    }
    *slash = '\0';
}

static void aes_fsel_split_input(const char *pipath, const char *pisel,
    char *directory, size_t directory_size,
    char *pattern, size_t pattern_size,
    char *selection, size_t selection_size)
{
    char source[AES_PATH_LEN];
    char *slash;
    char cwd[AES_PATH_LEN];

    if (directory != NULL && directory_size > 0u) {
        directory[0] = '\0';
    }
    if (pattern != NULL && pattern_size > 0u) {
        pattern[0] = '\0';
    }
    if (selection != NULL && selection_size > 0u) {
        selection[0] = '\0';
    }

    if (pisel != NULL && selection != NULL && selection_size > 0u) {
        strncpy(selection, pisel, selection_size - 1u);
        selection[selection_size - 1u] = '\0';
    }

    if (pipath == NULL || pipath[0] == '\0') {
        if (!gem_os_getcwd(cwd, sizeof(cwd))) {
            strcpy(cwd, ".");
        }
        if (directory != NULL && directory_size > 0u) {
            strncpy(directory, cwd, directory_size - 1u);
            directory[directory_size - 1u] = '\0';
        }
        if (pattern != NULL && pattern_size > 0u) {
            strcpy(pattern, "*");
        }
        return;
    }

    strncpy(source, pipath, sizeof(source) - 1u);
    source[sizeof(source) - 1u] = '\0';
    slash = strrchr(source, '/');
    if (slash == NULL) {
        slash = strrchr(source, '\\');
    }

    if (slash != NULL) {
        *slash = '\0';
        if (directory != NULL && directory_size > 0u) {
            if (source[0] != '\0') {
                strncpy(directory, source, directory_size - 1u);
                directory[directory_size - 1u] = '\0';
            } else {
                strcpy(directory, "/");
            }
        }
        if (pattern != NULL && pattern_size > 0u) {
            strncpy(pattern, slash + 1, pattern_size - 1u);
            pattern[pattern_size - 1u] = '\0';
        }
    } else {
        if (directory != NULL && directory_size > 0u) {
            if (!gem_os_getcwd(directory, directory_size)) {
                strcpy(directory, ".");
            }
        }
        if (pattern != NULL && pattern_size > 0u) {
            strncpy(pattern, source, pattern_size - 1u);
            pattern[pattern_size - 1u] = '\0';
        }
    }

    if (directory != NULL && directory[0] == '\0') {
        strcpy(directory, ".");
    }
    if (pattern != NULL && pattern[0] == '\0') {
        strcpy(pattern, "*");
    } else if (pattern != NULL && aes_fsel_has_wildcards(pattern) == 0) {
        if (selection != NULL && selection[0] == '\0') {
            strncpy(selection, pattern, selection_size - 1u);
            selection[selection_size - 1u] = '\0';
        }
        strcpy(pattern, "*");
    }
    if (directory != NULL) {
        aes_fsel_normalize_dir(directory);
    }
}

static int aes_fsel_match_pattern(const char *pattern, const char *name)
{
    unsigned char pch;
    unsigned char nch;

    if (pattern == NULL || name == NULL) {
        return 0;
    }
    if (*pattern == '\0') {
        return *name == '\0';
    }
    if (*pattern == '*') {
        do {
            if (aes_fsel_match_pattern(pattern + 1, name) != 0) {
                return 1;
            }
        } while (*name++ != '\0');
        return 0;
    }
    if (*name == '\0') {
        return 0;
    }

    pch = (unsigned char) *pattern;
    nch = (unsigned char) *name;
    if (pch != '?') {
        if (tolower(pch) != tolower(nch)) {
            return 0;
        }
    }
    return aes_fsel_match_pattern(pattern + 1, name + 1);
}

static int aes_fsel_join_path(const char *directory, const char *name,
    char *path, size_t path_size)
{
    int rc;

    if (directory == NULL || name == NULL || path == NULL || path_size == 0u) {
        return 0;
    }
    if (strcmp(directory, "/") == 0) {
        rc = snprintf(path, path_size, "/%s", name);
    } else if (strcmp(directory, ".") == 0) {
        rc = snprintf(path, path_size, "./%s", name);
    } else {
        rc = snprintf(path, path_size, "%s/%s", directory, name);
    }
    return rc >= 0 && (size_t) rc < path_size;
}

static void aes_fsel_prefix_line(char *dst, size_t dst_size,
    const char *prefix, const char *value)
{
    size_t used = 0u;

    if (dst == NULL || dst_size == 0u) {
        return;
    }

    dst[0] = '\0';
    if (prefix != NULL) {
        strncpy(dst, prefix, dst_size - 1u);
        dst[dst_size - 1u] = '\0';
        used = strlen(dst);
    }
    if (value != NULL && used + 1u < dst_size) {
        strncat(dst, value, dst_size - used - 1u);
    }
}

static int aes_fsel_entry_compare(const void *left, const void *right)
{
    const aes_fsel_entry_t *a = (const aes_fsel_entry_t *) left;
    const aes_fsel_entry_t *b = (const aes_fsel_entry_t *) right;
    size_t i = 0u;

    if (a->is_directory != b->is_directory) {
        return (b->is_directory - a->is_directory);
    }

    while (a->name[i] != '\0' || b->name[i] != '\0') {
        int ca = tolower((unsigned char) a->name[i]);
        int cb = tolower((unsigned char) b->name[i]);

        if (ca != cb) {
            return ca - cb;
        }
        if (a->name[i] == '\0' || b->name[i] == '\0') {
            break;
        }
        ++i;
    }
    return 0;
}

static void aes_fsel_set_selection_from_index(aes_fsel_state_t *state)
{
    if (state == NULL || state->selected < 0 ||
        state->selected >= state->entry_count ||
        state->entries[state->selected].is_directory != 0) {
        if (state != NULL) {
            state->selection[0] = '\0';
        }
        return;
    }

    strncpy(state->selection, state->entries[state->selected].name,
        sizeof(state->selection) - 1u);
    state->selection[sizeof(state->selection) - 1u] = '\0';
}

static void aes_fsel_reload_entries(aes_fsel_state_t *state)
{
    gem_os_dir_t dir;
    gem_os_dirent_t dent;
    WORD count = 0;
    WORD selected = NIL;

    if (state == NULL) {
        return;
    }

    state->entry_count = 0;
    state->selected = NIL;

    if (strcmp(state->directory, "/") != 0) {
        strcpy(state->entries[count].name, "..");
        state->entries[count].is_directory = 1;
        if (strcmp(state->selection, "..") == 0) {
            selected = count;
        }
        ++count;
    }

    if (gem_os_dir_open(state->directory, &dir) != 0) {
        while (count < AES_FSEL_MAX_ENTRIES && gem_os_dir_read(&dir, &dent) != 0) {
            if (strcmp(dent.name, ".") == 0) {
                continue;
            }
            if (dent.info.is_directory == 0 &&
                aes_fsel_match_pattern(state->pattern, dent.name) == 0) {
                continue;
            }

            strncpy(state->entries[count].name, dent.name,
                sizeof(state->entries[count].name) - 1u);
            state->entries[count].name[sizeof(state->entries[count].name) - 1u] =
                '\0';
            state->entries[count].is_directory = dent.info.is_directory ? 1 : 0;
            if (strcmp(state->selection, dent.name) == 0) {
                selected = count;
            }
            ++count;
        }
        gem_os_dir_close(&dir);
    }

    if (count > 1) {
        qsort(&state->entries[1], (size_t) (count - 1),
            sizeof(state->entries[0]), aes_fsel_entry_compare);
        selected = NIL;
        for (WORD i = 0; i < count; ++i) {
            if (strcmp(state->selection, state->entries[i].name) == 0) {
                selected = i;
                break;
            }
        }
    }

    state->entry_count = count;
    if (selected == NIL && count > 0) {
        selected = 0;
    }
    state->selected = selected;
    aes_fsel_set_selection_from_index(state);
}

static void aes_fsel_sync_work(aes_fsel_state_t *state)
{
    WORD list_top;
    WORD list_bottom;
    WORD available;

    if (state == NULL) {
        return;
    }

    wind_get(state->handle, WF_CXYWH, &state->work.g_x, &state->work.g_y,
        &state->work.g_w, &state->work.g_h);
    list_top = aes_fsel_list_top(state);
    list_bottom = (WORD) (state->work.g_y + state->work.g_h -
        AES_FSEL_BUTTON_HEIGHT - AES_FSEL_MARGIN * 2);
    available = (WORD) (list_bottom - list_top);
    state->visible_rows = (available > 0) ? (WORD) (available / state->row_h) : 0;
    if (state->visible_rows < 1) {
        state->visible_rows = 1;
    }
}

static void aes_fsel_draw_hline(WORD x0, WORD x1, WORD y)
{
    WORD pts[4];

    pts[0] = x0;
    pts[1] = y;
    pts[2] = x1;
    pts[3] = y;
    vsl_color(_aes.vdi_handle, BLACK);
    v_pline(_aes.vdi_handle, 2, pts);
}

static void aes_fsel_draw_vline(WORD x, WORD y0, WORD y1)
{
    WORD pts[4];

    pts[0] = x;
    pts[1] = y0;
    pts[2] = x;
    pts[3] = y1;
    vsl_color(_aes.vdi_handle, BLACK);
    v_pline(_aes.vdi_handle, 2, pts);
}

static void aes_fsel_button_rects(const aes_fsel_state_t *state,
    GRECT *ok_rect, GRECT *cancel_rect)
{
    WORD y;
    WORD cancel_x;
    WORD ok_x;

    if (state == NULL) {
        return;
    }

    y = (WORD) (state->work.g_y + state->work.g_h - AES_FSEL_BUTTON_HEIGHT -
        AES_FSEL_MARGIN);
    cancel_x = (WORD) (state->work.g_x + state->work.g_w - AES_FSEL_MARGIN -
        AES_FSEL_BUTTON_WIDTH);
    ok_x = (WORD) (cancel_x - AES_FSEL_MARGIN - AES_FSEL_BUTTON_WIDTH);

    if (ok_rect != NULL) {
        ok_rect->g_x = ok_x;
        ok_rect->g_y = y;
        ok_rect->g_w = AES_FSEL_BUTTON_WIDTH;
        ok_rect->g_h = AES_FSEL_BUTTON_HEIGHT;
    }
    if (cancel_rect != NULL) {
        cancel_rect->g_x = cancel_x;
        cancel_rect->g_y = y;
        cancel_rect->g_w = AES_FSEL_BUTTON_WIDTH;
        cancel_rect->g_h = AES_FSEL_BUTTON_HEIGHT;
    }
}

static WORD aes_fsel_list_top(const aes_fsel_state_t *state)
{
    return (WORD) (state->work.g_y + AES_FSEL_MARGIN +
        AES_FSEL_HEADER_LINES * state->row_h);
}

static int aes_fsel_entry_rect(const aes_fsel_state_t *state, WORD row,
    GRECT *rect)
{
    WORD y;

    if (state == NULL || rect == NULL || row < 0 || row >= state->visible_rows) {
        return 0;
    }

    y = (WORD) (aes_fsel_list_top(state) + row * state->row_h);
    rect->g_x = (WORD) (state->work.g_x + AES_FSEL_MARGIN);
    rect->g_y = y;
    rect->g_w = (WORD) (state->work.g_w - AES_FSEL_MARGIN * 2);
    rect->g_h = state->row_h;
    return 1;
}

static void aes_fsel_scroll_into_view(aes_fsel_state_t *state)
{
    WORD max_top;

    if (state == NULL || state->selected == NIL) {
        return;
    }

    if (state->selected < state->top_index) {
        state->top_index = state->selected;
    } else if (state->selected >= state->top_index + state->visible_rows) {
        state->top_index = (WORD) (state->selected - state->visible_rows + 1);
    }

    max_top = 0;
    if (state->entry_count > state->visible_rows) {
        max_top = (WORD) (state->entry_count - state->visible_rows);
    }
    if (state->top_index < 0) {
        state->top_index = 0;
    }
    if (state->top_index > max_top) {
        state->top_index = max_top;
    }
}

static void aes_fsel_draw(const aes_fsel_state_t *state, const GRECT *dirty)
{
    GRECT box;
    GRECT ok_rect;
    GRECT cancel_rect;

    if (state == NULL || _aes_ensure_vdi() == 0) {
        return;
    }

    aes_fsel_button_rects(state, &ok_rect, &cancel_rect);

    wind_update(BEG_UPDATE);
    wind_get(state->handle, WF_FIRSTXYWH, &box.g_x, &box.g_y, &box.g_w,
        &box.g_h);
    while (box.g_w > 0 && box.g_h > 0) {
        WORD x0 = box.g_x;
        WORD y0 = box.g_y;
        WORD x1 = (WORD) (box.g_x + box.g_w - 1);
        WORD y1 = (WORD) (box.g_y + box.g_h - 1);
        WORD clip[4];
        WORD fill[4];
        WORD line_y;
        WORD row;

        if (dirty != NULL) {
            WORD dx1 = (WORD) (dirty->g_x + dirty->g_w - 1);
            WORD dy1 = (WORD) (dirty->g_y + dirty->g_h - 1);

            if (x0 < dirty->g_x) {
                x0 = dirty->g_x;
            }
            if (y0 < dirty->g_y) {
                y0 = dirty->g_y;
            }
            if (x1 > dx1) {
                x1 = dx1;
            }
            if (y1 > dy1) {
                y1 = dy1;
            }
        }

        if (x0 <= x1 && y0 <= y1) {
            char line[AES_FSEL_LINE_LEN];

            clip[0] = x0;
            clip[1] = y0;
            clip[2] = x1;
            clip[3] = y1;
            fill[0] = state->work.g_x;
            fill[1] = state->work.g_y;
            fill[2] = (WORD) (state->work.g_x + state->work.g_w - 1);
            fill[3] = (WORD) (state->work.g_y + state->work.g_h - 1);

            vs_clip(_aes.vdi_handle, 1, clip);
            vsf_color(_aes.vdi_handle, WHITE);
            vr_recfl(_aes.vdi_handle, fill);
            vst_color(_aes.vdi_handle, BLACK);

            line_y = (WORD) (state->work.g_y + AES_FSEL_MARGIN +
                state->text_ascent);
            aes_fsel_prefix_line(line, sizeof(line), "Directory: ",
                state->directory);
            v_gtext(_aes.vdi_handle, (WORD) (state->work.g_x + AES_FSEL_MARGIN),
                line_y, (CONST BYTE *) line);

            line_y = (WORD) (line_y + state->row_h);
            aes_fsel_prefix_line(line, sizeof(line), "Pattern: ",
                state->pattern);
            v_gtext(_aes.vdi_handle, (WORD) (state->work.g_x + AES_FSEL_MARGIN),
                line_y, (CONST BYTE *) line);

            line_y = (WORD) (line_y + state->row_h);
            aes_fsel_prefix_line(line, sizeof(line), "Selected: ",
                (state->selection[0] != '\0') ? state->selection : "(none)");
            v_gtext(_aes.vdi_handle, (WORD) (state->work.g_x + AES_FSEL_MARGIN),
                line_y, (CONST BYTE *) line);

            for (row = 0; row < state->visible_rows; ++row) {
                WORD index = (WORD) (state->top_index + row);
                GRECT rect;
                WORD rect_xy[4];

                if (!aes_fsel_entry_rect(state, row, &rect)) {
                    continue;
                }

                rect_xy[0] = rect.g_x;
                rect_xy[1] = rect.g_y;
                rect_xy[2] = (WORD) (rect.g_x + rect.g_w - 1);
                rect_xy[3] = (WORD) (rect.g_y + rect.g_h - 1);

                if (index < state->entry_count && index == state->selected) {
                    vsf_color(_aes.vdi_handle, BLACK);
                    vr_recfl(_aes.vdi_handle, rect_xy);
                    vst_color(_aes.vdi_handle, WHITE);
                } else {
                    vst_color(_aes.vdi_handle, BLACK);
                }

                if (index < state->entry_count) {
                    snprintf(line, sizeof(line), "%s%s",
                        state->entries[index].name,
                        state->entries[index].is_directory ? "/" : "");
                    v_gtext(_aes.vdi_handle,
                        (WORD) (rect.g_x + 2),
                        (WORD) (rect.g_y + state->text_ascent),
                        (CONST BYTE *) line);
                }
            }

            fill[0] = ok_rect.g_x;
            fill[1] = ok_rect.g_y;
            fill[2] = (WORD) (ok_rect.g_x + ok_rect.g_w - 1);
            fill[3] = (WORD) (ok_rect.g_y + ok_rect.g_h - 1);
            vsf_color(_aes.vdi_handle, WHITE);
            vr_recfl(_aes.vdi_handle, fill);
            aes_fsel_draw_hline(fill[0], fill[2], fill[1]);
            aes_fsel_draw_hline(fill[0], fill[2], fill[3]);
            aes_fsel_draw_vline(fill[0], fill[1], fill[3]);
            aes_fsel_draw_vline(fill[2], fill[1], fill[3]);
            v_gtext(_aes.vdi_handle, (WORD) (ok_rect.g_x + 20),
                (WORD) (ok_rect.g_y + state->text_ascent + 6),
                (CONST BYTE *) "OK");

            fill[0] = cancel_rect.g_x;
            fill[1] = cancel_rect.g_y;
            fill[2] = (WORD) (cancel_rect.g_x + cancel_rect.g_w - 1);
            fill[3] = (WORD) (cancel_rect.g_y + cancel_rect.g_h - 1);
            vsf_color(_aes.vdi_handle, WHITE);
            vr_recfl(_aes.vdi_handle, fill);
            aes_fsel_draw_hline(fill[0], fill[2], fill[1]);
            aes_fsel_draw_hline(fill[0], fill[2], fill[3]);
            aes_fsel_draw_vline(fill[0], fill[1], fill[3]);
            aes_fsel_draw_vline(fill[2], fill[1], fill[3]);
            v_gtext(_aes.vdi_handle, (WORD) (cancel_rect.g_x + 8),
                (WORD) (cancel_rect.g_y + state->text_ascent + 6),
                (CONST BYTE *) "Cancel");

            vs_clip(_aes.vdi_handle, 0, clip);
        }

        wind_get(state->handle, WF_NEXTXYWH, &box.g_x, &box.g_y, &box.g_w,
            &box.g_h);
    }
    wind_update(END_UPDATE);
}

static void aes_fsel_enter_directory(aes_fsel_state_t *state,
    const char *name)
{
    char path[AES_PATH_LEN];

    if (state == NULL || name == NULL) {
        return;
    }
    if (strcmp(name, "..") == 0) {
        aes_fsel_parent_dir(state->directory);
    } else if (aes_fsel_join_path(state->directory, name, path,
               sizeof(path)) != 0) {
        strncpy(state->directory, path, sizeof(state->directory) - 1u);
        state->directory[sizeof(state->directory) - 1u] = '\0';
    }

    aes_fsel_normalize_dir(state->directory);
    state->selection[0] = '\0';
    state->top_index = 0;
    aes_fsel_reload_entries(state);
    aes_fsel_scroll_into_view(state);
}

static void aes_fsel_activate(aes_fsel_state_t *state)
{
    if (state == NULL || state->selected == NIL ||
        state->selected >= state->entry_count) {
        return;
    }
    if (state->entries[state->selected].is_directory != 0) {
        aes_fsel_enter_directory(state, state->entries[state->selected].name);
        return;
    }

    aes_fsel_set_selection_from_index(state);
    state->confirmed = 1;
    state->done = 1;
}

static void aes_fsel_move_selection(aes_fsel_state_t *state, WORD delta)
{
    WORD next;

    if (state == NULL || state->entry_count <= 0) {
        return;
    }

    if (state->selected == NIL) {
        state->selected = 0;
    }

    next = (WORD) (state->selected + delta);
    if (next < 0) {
        next = 0;
    }
    if (next >= state->entry_count) {
        next = (WORD) (state->entry_count - 1);
    }
    state->selected = next;
    aes_fsel_set_selection_from_index(state);
    aes_fsel_scroll_into_view(state);
}

static void aes_fsel_page_selection(aes_fsel_state_t *state, WORD delta)
{
    if (state == NULL) {
        return;
    }
    aes_fsel_move_selection(state, (WORD) (delta * state->visible_rows));
}

static void aes_fsel_accept(aes_fsel_state_t *state,
    char *pipath, char *pisel, WORD *pbutton)
{
    if (state == NULL) {
        return;
    }

    if (pbutton != NULL) {
        *pbutton = (state->confirmed != 0) ? 1 : 0;
    }
    if (pipath != NULL) {
        char path[AES_PATH_LEN];

        if (aes_fsel_join_path(state->directory, state->pattern, path,
                sizeof(path)) == 0) {
            path[0] = '\0';
        }
        strcpy(pipath, path);
    }
    if (pisel != NULL) {
        strcpy(pisel, state->selection);
    }
}

WORD fsel_input(char *pipath, char *pisel, WORD *pbutton)
{
    aes_fsel_state_t state;
    GRECT desk;
    WORD msg[8];

    memset(&state, 0, sizeof(state));
    aes_fsel_split_input(pipath, pisel, state.directory,
        sizeof(state.directory), state.pattern, sizeof(state.pattern),
        state.selection, sizeof(state.selection));

    if (_aes_ensure_vdi() == 0) {
        if (pbutton != NULL) {
            *pbutton = 0;
        }
        return 0;
    }

    state.handle = wind_create(NAME | CLOSER | MOVER, 0, 0, 520, 320);
    if (state.handle <= 0) {
        if (pbutton != NULL) {
            *pbutton = 0;
        }
        return 0;
    }

    state.char_w = AES_CHAR_WIDTH;
    state.char_h = AES_CHAR_HEIGHT;
    state.text_ascent = _vdi_font_ascent();
    state.row_h = (WORD) (_vdi_font_text_height() + 4);
    aes_fsel_reload_entries(&state);

    wind_get(0, WF_CXYWH, &desk.g_x, &desk.g_y, &desk.g_w, &desk.g_h);
    wind_open(state.handle, (WORD) (desk.g_x + 36), (WORD) (desk.g_y + 30),
        520, 320);
    wind_set_str(state.handle, WF_NAME, "File Selector");
    aes_fsel_sync_work(&state);
    aes_fsel_scroll_into_view(&state);
    aes_fsel_draw(&state, NULL);

    while (state.done == 0) {
        WORD event;
        WORD mx = 0;
        WORD my = 0;
        WORD mb = 0;
        WORD ks = 0;
        WORD kr = 0;
        WORD br = 0;

        event = evnt_multi(MU_MESAG | MU_BUTTON | MU_KEYBD,
            1, 1, 1,
            0, 0, 0, 0, 0,
            0, 0, 0, 0, 0,
            msg, 0, 0, &mx, &my, &mb, &ks, &kr, &br);
        (void) mb;
        (void) ks;
        (void) br;

        if ((event & MU_MESAG) != 0 && msg[3] == state.handle) {
            switch (msg[0]) {
            case WM_REDRAW: {
                GRECT dirty;

                dirty.g_x = msg[4];
                dirty.g_y = msg[5];
                dirty.g_w = msg[6];
                dirty.g_h = msg[7];
                aes_fsel_sync_work(&state);
                aes_fsel_draw(&state, &dirty);
                break;
            }
            case WM_TOPPED:
                wind_set(state.handle, WF_TOP, 0, 0, 0, 0);
                break;
            case WM_MOVED:
                wind_set(state.handle, WF_WXYWH, msg[4], msg[5], msg[6],
                    msg[7]);
                aes_fsel_sync_work(&state);
                aes_fsel_scroll_into_view(&state);
                aes_fsel_draw(&state, NULL);
                break;
            case WM_CLOSED:
                state.confirmed = 0;
                state.done = 1;
                break;
            default:
                break;
            }
        }

        if ((event & MU_KEYBD) != 0) {
            WORD ascii = (WORD) (kr & 0xffu);
            WORD scancode = (WORD) ((kr >> 8) & 0xffu);

            if (ascii == 27) {
                state.confirmed = 0;
                state.done = 1;
            } else if (ascii == '\r' || ascii == '\n' ||
                       scancode == 40 || scancode == 28) {
                aes_fsel_activate(&state);
                aes_fsel_draw(&state, NULL);
            } else if (ascii == '\b' || scancode == 42 || scancode == 14) {
                aes_fsel_enter_directory(&state, "..");
                aes_fsel_draw(&state, NULL);
            } else if (scancode == 81 || scancode == 80) {
                aes_fsel_move_selection(&state, 1);
                aes_fsel_draw(&state, NULL);
            } else if (scancode == 82 || scancode == 72) {
                aes_fsel_move_selection(&state, -1);
                aes_fsel_draw(&state, NULL);
            } else if (scancode == 78 || scancode == 81) {
                aes_fsel_page_selection(&state, 1);
                aes_fsel_draw(&state, NULL);
            } else if (scancode == 75) {
                aes_fsel_page_selection(&state, -1);
                aes_fsel_draw(&state, NULL);
            }
        }

        if ((event & MU_BUTTON) != 0 && wind_find(mx, my) == state.handle) {
            GRECT ok_rect;
            GRECT cancel_rect;
            WORD row;

            aes_fsel_button_rects(&state, &ok_rect, &cancel_rect);
            if (_aes_point_in_rect(mx, my, &ok_rect) != 0) {
                aes_fsel_activate(&state);
                continue;
            }
            if (_aes_point_in_rect(mx, my, &cancel_rect) != 0) {
                state.confirmed = 0;
                state.done = 1;
                continue;
            }

            row = (WORD) ((my - aes_fsel_list_top(&state)) / state.row_h);
            if (row >= 0 && row < state.visible_rows) {
                WORD index = (WORD) (state.top_index + row);

                if (index < state.entry_count) {
                    if (state.selected == index) {
                        aes_fsel_activate(&state);
                    } else {
                        state.selected = index;
                        aes_fsel_set_selection_from_index(&state);
                        aes_fsel_scroll_into_view(&state);
                        aes_fsel_draw(&state, NULL);
                    }
                }
            }
        }
    }

    aes_fsel_accept(&state, pipath, pisel, pbutton);
    wind_close(state.handle);
    wind_delete(state.handle);
    return 1;
}
