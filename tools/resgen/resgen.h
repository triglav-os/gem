/*
 * Declares resgen's option and form model shared by its argument parser,
 * PNG decoder and resource writer.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (C) 2026 tomaz stih
 */

#ifndef GEM_TOOLS_RESGEN_H
#define GEM_TOOLS_RESGEN_H

#include <gem.h>

#include <stddef.h>
#include <stdint.h>

enum {
    resgen_cursor_slots = 8,
    resgen_max_outputs = 8,
    resgen_default_text_height = 8,
    resgen_default_cursor_arrow_hot_x = 0,
    resgen_default_cursor_arrow_hot_y = 0,
    resgen_default_cursor_text_hot_x = 8,
    resgen_default_cursor_text_hot_y = 8,
    resgen_default_cursor_bee_hot_x = 7,
    resgen_default_cursor_bee_hot_y = 7,
    resgen_default_cursor_point_hand_hot_x = 4,
    resgen_default_cursor_point_hand_hot_y = 0,
    resgen_default_cursor_flat_hand_hot_x = 7,
    resgen_default_cursor_flat_hand_hot_y = 0,
    resgen_default_cursor_cross_hot_x = 8,
    resgen_default_cursor_cross_hot_y = 8
};

enum {
    resgen_cursor_arrow = 0,
    resgen_cursor_text = 1,
    resgen_cursor_bee = 2,
    resgen_cursor_point_hand = 3,
    resgen_cursor_flat_hand = 4,
    resgen_cursor_thin_cross = 5,
    resgen_cursor_thick_cross = 6,
    resgen_cursor_outline_cross = 7
};

typedef struct resource_output {
    const char *path;
} resource_output_t;

typedef struct image_form {
    WORD width;
    WORD height;
    WORD words_per_row;
    WORD *mask_words;
    WORD *data_words;
} image_form_t;

typedef struct icon_entry {
    const char *path;
    char *label;
    image_form_t form;
} icon_entry_t;

typedef struct bitmap_entry {
    const char *path;
    image_form_t form;
} bitmap_entry_t;

typedef struct cursor_spec {
    const char *path;
    WORD hot_x;
    WORD hot_y;
    image_form_t form;
} cursor_spec_t;

typedef struct cursor_options {
    cursor_spec_t slots[resgen_cursor_slots];
    const char *type_list;
} cursor_options_t;

typedef struct icon_options {
    icon_entry_t *entries;
    int count;
} icon_options_t;

typedef struct bitmap_options {
    bitmap_entry_t *entries;
    int count;
} bitmap_options_t;

typedef struct resgen_options {
    const char *program_name;
    const char *mode_name;
    resource_output_t outputs[resgen_max_outputs];
    int output_count;
    union {
        cursor_options_t cursor;
        icon_options_t icon;
        bitmap_options_t bitmap;
    } mode;
} resgen_options_t;

/* Release the mask and data planes of a decoded form. */
void free_image_form(image_form_t *form);
/* Decode a PNG into a form; nonzero on success. */
int load_png_form(const char *path, image_form_t *form);
/* Build the cursor resource image; returns its size or 0. */
int build_cursor_rsc(const resgen_options_t *options, uint8_t **bytes_out,
                     size_t *size_out);
/* Build the icon resource image; returns its size or 0. */
int build_icon_rsc(const resgen_options_t *options, uint8_t **bytes_out,
                   size_t *size_out);
/* Build the bitmap resource image; returns its size or 0. */
int build_bitmap_rsc(const resgen_options_t *options, uint8_t **bytes_out,
                     size_t *size_out);
/* Write one resource image to every configured output path. */
int write_outputs(const resgen_options_t *options, const void *bytes,
                  size_t count);

#endif
