/*
 * Declares reusable GEM controls that are not part of the historical AES
 * object/form API. Controls keep their model and input behavior independent
 * of any particular dialog or renderer.
 *
 * MIT License (see: LICENSE)
 * Copyright (C) 2026 tomaz stih
 */

#ifndef GEM_GEMCTL_H
#define GEM_GEMCTL_H

#include <gem/aes.h>
#include <gem/vdi.h>

#include <stddef.h>
#include <stdint.h>

typedef struct gemctl_text_edit {
    char *text;
    size_t capacity;
    uint8_t active;
    uint8_t replace_pending;
} gemctl_text_edit_t;

typedef enum gemctl_text_edit_result {
    GEMCTL_TEXT_EDIT_IGNORED = 0,
    GEMCTL_TEXT_EDIT_CHANGED,
    GEMCTL_TEXT_EDIT_ACCEPT,
    GEMCTL_TEXT_EDIT_CANCEL
} gemctl_text_edit_result_t;

/* Initializes a text edit over caller-owned storage. */
void gemctl_text_edit_init(gemctl_text_edit_t *control, char *text,
                           size_t capacity);
/* Gives a text edit focus; replace selects all existing text logically. */
void gemctl_text_edit_focus(gemctl_text_edit_t *control, int replace);
/* Removes keyboard focus and any pending replacement from a text edit. */
void gemctl_text_edit_blur(gemctl_text_edit_t *control);
/* Returns nonzero while the text edit owns keyboard focus. */
int gemctl_text_edit_active(const gemctl_text_edit_t *control);
/* Applies one GEM key and reports whether text or edit state changed. */
gemctl_text_edit_result_t gemctl_text_edit_key(gemctl_text_edit_t *control,
                                               uint16_t key);

typedef struct gemctl_chrome {
    GRECT toolbar;
    GRECT content;
    GRECT status;
    WORD band_height;
} gemctl_chrome_t;

/* Calculates standard toolbar, content and status rectangles within work. */
void gemctl_chrome_layout(const GRECT *work, WORD char_height, WORD box_height,
                          gemctl_chrome_t *chrome);
/* Draws a standard empty application toolbar band. */
void gemctl_toolbar_draw(VDI_HANDLE handle, const GRECT *rect);
/* Draws a standard status band and optional left-aligned text. */
void gemctl_status_draw(VDI_HANDLE handle, const GRECT *rect, const char *text);
/* Draws one vertical separator between toolbar slots. */
void gemctl_toolbar_separator(VDI_HANDLE handle, WORD x, WORD y0, WORD y1);
/* Centers and draws a monochrome BITBLK in a toolbar slot. */
void gemctl_toolbar_bitmap(VDI_HANDLE handle, const BITBLK *bitmap,
                           const GRECT *slot);
/* Draws a magnifying-glass toolbar icon with a plus or minus sign. */
void gemctl_toolbar_zoom_icon(VDI_HANDLE handle, const GRECT *slot,
                              int zoom_in);

#endif
