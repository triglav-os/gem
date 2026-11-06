/*
 * Serializes resgen's decoded forms into host-native GEM resource files:
 * the RSHDR layout, ICONBLK cursor and icon tables, BITBLK bitmap tables
 * and the trailing pixel data, written to every requested output path.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (C) 2026 tomaz stih
 */

#define _POSIX_C_SOURCE 200809L
#include "resgen.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static size_t align_up(size_t value, size_t alignment)
{
    size_t remainder = value % alignment;

    return (remainder == 0u) ? value : (value + alignment - remainder);
}

static int write_file_bytes(FILE *stream, const void *bytes, size_t count)
{
    return count == 0u || fwrite(bytes, count, 1, stream) == 1;
}

static int write_resource_to_path(const char *path, const void *bytes,
                                  size_t count)
{
    FILE *stream;
    int ok;

    stream = fopen(path, "wb");
    if (stream == NULL) {
        fprintf(stderr, "resgen: unable to open %s for writing: %s\n", path,
                strerror(errno));
        return 0;
    }
    ok = write_file_bytes(stream, bytes, count);
    fclose(stream);
    if (!ok) {
        fprintf(stderr, "resgen: failed while writing %s\n", path);
        return 0;
    }
    return 1;
}

int build_cursor_rsc(const resgen_options_t *options, uint8_t **bytes_out,
                     size_t *size_out)
{
    RSHDR header;
    ICONBLK icons[resgen_cursor_slots];
    cursor_spec_t slots[resgen_cursor_slots];
    size_t total_imdata_bytes = 0u;
    size_t offset;
    uint8_t *bytes;
    size_t rssize;
    int i;

    if (options == NULL || bytes_out == NULL || size_out == NULL) {
        return 0;
    }

    memset(&header, 0, sizeof(header));
    memset(icons, 0, sizeof(icons));
    for (i = 0; i < resgen_cursor_slots; ++i) {
        slots[i] = options->mode.cursor.slots[resgen_cursor_arrow];
        if (options->mode.cursor.slots[i].path != NULL) {
            slots[i] = options->mode.cursor.slots[i];
        }
    }

    header.rsh_vrsn = 0;
    header.rsh_object = (WORD)sizeof(RSHDR);
    header.rsh_tedinfo = header.rsh_object;
    header.rsh_iconblk = (WORD)align_up(sizeof(RSHDR), sizeof(LONG));
    header.rsh_bitblk = (WORD)(header.rsh_iconblk + sizeof(icons));
    header.rsh_frstr = header.rsh_bitblk;
    header.rsh_string = header.rsh_frstr;
    header.rsh_imdata = (WORD)align_up((size_t)header.rsh_string, sizeof(LONG));
    header.rsh_frimg = header.rsh_imdata;
    header.rsh_trindex = header.rsh_frimg;
    header.rsh_nib = resgen_cursor_slots;

    offset = (size_t)header.rsh_imdata;
    for (i = 0; i < resgen_cursor_slots; ++i) {
        size_t plane_bytes = (size_t)slots[i].form.words_per_row *
                             (size_t)slots[i].form.height * sizeof(WORD);

        icons[i].ib_pmask = (LONG)offset;
        offset += plane_bytes;
        icons[i].ib_pdata = (LONG)offset;
        offset += plane_bytes;
        icons[i].ib_xchar = slots[i].hot_x;
        icons[i].ib_ychar = slots[i].hot_y;
        icons[i].ib_wicon = slots[i].form.width;
        icons[i].ib_hicon = slots[i].form.height;
        total_imdata_bytes += plane_bytes * 2u;
    }

    header.rsh_rssize = (WORD)offset;
    rssize = offset;
    bytes = calloc(rssize, 1u);
    if (bytes == NULL) {
        return 0;
    }

    memcpy(bytes, &header, sizeof(header));
    memcpy(bytes + header.rsh_iconblk, icons, sizeof(icons));
    for (i = 0; i < resgen_cursor_slots; ++i) {
        size_t plane_bytes = (size_t)slots[i].form.words_per_row *
                             (size_t)slots[i].form.height * sizeof(WORD);

        memcpy(bytes + (size_t)icons[i].ib_pmask, slots[i].form.mask_words,
               plane_bytes);
        memcpy(bytes + (size_t)icons[i].ib_pdata, slots[i].form.data_words,
               plane_bytes);
    }

    (void)total_imdata_bytes;
    *bytes_out = bytes;
    *size_out = rssize;
    return 1;
}

int build_icon_rsc(const resgen_options_t *options, uint8_t **bytes_out,
                   size_t *size_out)
{
    RSHDR header;
    ICONBLK *icons;
    LONG *string_offsets;
    size_t strings_bytes = 0u;
    size_t offset;
    size_t rssize;
    uint8_t *bytes;
    int i;

    if (options == NULL || bytes_out == NULL || size_out == NULL ||
        options->mode.icon.count <= 0) {
        return 0;
    }

    icons = calloc((size_t)options->mode.icon.count, sizeof(*icons));
    string_offsets =
        calloc((size_t)options->mode.icon.count, sizeof(*string_offsets));
    if (icons == NULL || string_offsets == NULL) {
        free(icons);
        free(string_offsets);
        return 0;
    }

    memset(&header, 0, sizeof(header));
    header.rsh_vrsn = 0;
    header.rsh_object = (WORD)sizeof(RSHDR);
    header.rsh_tedinfo = header.rsh_object;
    header.rsh_iconblk = (WORD)align_up(sizeof(RSHDR), sizeof(LONG));
    header.rsh_bitblk =
        (WORD)(header.rsh_iconblk +
               sizeof(*icons) * (size_t)options->mode.icon.count);
    header.rsh_frstr = header.rsh_bitblk;
    header.rsh_string = (WORD)align_up((size_t)header.rsh_frstr, sizeof(LONG));
    header.rsh_imdata = (WORD)align_up((size_t)header.rsh_string +
                                           sizeof(*string_offsets) *
                                               (size_t)options->mode.icon.count,
                                       sizeof(LONG));
    header.rsh_frimg = header.rsh_imdata;
    header.rsh_trindex = header.rsh_frimg;
    header.rsh_nib = (WORD)options->mode.icon.count;
    header.rsh_nstring = (WORD)options->mode.icon.count;

    offset = (size_t)header.rsh_imdata;
    for (i = 0; i < options->mode.icon.count; ++i) {
        const icon_entry_t *entry = &options->mode.icon.entries[i];
        size_t plane_bytes = (size_t)entry->form.words_per_row *
                             (size_t)entry->form.height * sizeof(WORD);

        icons[i].ib_pmask = (LONG)offset;
        offset += plane_bytes;
        icons[i].ib_pdata = (LONG)offset;
        offset += plane_bytes;
        icons[i].ib_wicon = entry->form.width;
        icons[i].ib_hicon = entry->form.height;
        icons[i].ib_xicon = 0;
        icons[i].ib_yicon = 0;
        icons[i].ib_xtext = 0;
        icons[i].ib_ytext = entry->form.height;
        icons[i].ib_wtext = (WORD)((WORD)strlen(entry->label) * 8);
        icons[i].ib_htext = resgen_default_text_height;
        strings_bytes += strlen(entry->label) + 1u;
    }

    {
        size_t strings_start = align_up(offset, sizeof(LONG));

        offset = strings_start;
        for (i = 0; i < options->mode.icon.count; ++i) {
            string_offsets[i] = (LONG)offset;
            icons[i].ib_ptext = string_offsets[i];
            offset += strlen(options->mode.icon.entries[i].label) + 1u;
        }
    }

    header.rsh_rssize = (WORD)offset;
    rssize = offset;
    bytes = calloc(rssize, 1u);
    if (bytes == NULL) {
        free(icons);
        free(string_offsets);
        return 0;
    }

    memcpy(bytes, &header, sizeof(header));
    memcpy(bytes + header.rsh_iconblk, icons,
           sizeof(*icons) * (size_t)options->mode.icon.count);
    memcpy(bytes + header.rsh_string, string_offsets,
           sizeof(*string_offsets) * (size_t)options->mode.icon.count);

    for (i = 0; i < options->mode.icon.count; ++i) {
        const icon_entry_t *entry = &options->mode.icon.entries[i];
        size_t plane_bytes = (size_t)entry->form.words_per_row *
                             (size_t)entry->form.height * sizeof(WORD);

        memcpy(bytes + (size_t)icons[i].ib_pmask, entry->form.mask_words,
               plane_bytes);
        memcpy(bytes + (size_t)icons[i].ib_pdata, entry->form.data_words,
               plane_bytes);
        memcpy(bytes + (size_t)string_offsets[i], entry->label,
               strlen(entry->label) + 1u);
    }

    free(icons);
    free(string_offsets);
    (void)strings_bytes;
    *bytes_out = bytes;
    *size_out = rssize;
    return 1;
}

int build_bitmap_rsc(const resgen_options_t *options, uint8_t **bytes_out,
                     size_t *size_out)
{
    RSHDR header;
    BITBLK *bitblks;
    size_t offset;
    uint8_t *bytes;
    size_t rssize;
    int i;

    if (options == NULL || bytes_out == NULL || size_out == NULL ||
        options->mode.bitmap.count <= 0) {
        return 0;
    }

    bitblks = calloc((size_t)options->mode.bitmap.count, sizeof(*bitblks));
    if (bitblks == NULL) {
        return 0;
    }

    memset(&header, 0, sizeof(header));
    header.rsh_vrsn = 0;
    header.rsh_object = (WORD)sizeof(RSHDR);
    header.rsh_tedinfo = header.rsh_object;
    header.rsh_iconblk = header.rsh_tedinfo;
    header.rsh_bitblk = (WORD)align_up(sizeof(RSHDR), sizeof(LONG));
    header.rsh_frstr =
        (WORD)(header.rsh_bitblk +
               sizeof(*bitblks) * (size_t)options->mode.bitmap.count);
    header.rsh_string = header.rsh_frstr;
    header.rsh_imdata = (WORD)align_up((size_t)header.rsh_string, sizeof(LONG));
    header.rsh_frimg = header.rsh_imdata;
    header.rsh_trindex = header.rsh_frimg;
    header.rsh_nbb = (WORD)options->mode.bitmap.count;

    offset = (size_t)header.rsh_imdata;
    for (i = 0; i < options->mode.bitmap.count; ++i) {
        const bitmap_entry_t *entry = &options->mode.bitmap.entries[i];
        size_t plane_bytes = (size_t)entry->form.words_per_row *
                             (size_t)entry->form.height * sizeof(WORD);

        bitblks[i].bi_pdata = (LONG)offset;
        bitblks[i].bi_wb =
            (WORD)(entry->form.words_per_row * (WORD)sizeof(WORD));
        bitblks[i].bi_hl = entry->form.height;
        bitblks[i].bi_x = 0;
        bitblks[i].bi_y = 0;
        bitblks[i].bi_color = 1;
        offset += plane_bytes;
    }

    header.rsh_rssize = (WORD)offset;
    rssize = offset;
    bytes = calloc(rssize, 1u);
    if (bytes == NULL) {
        free(bitblks);
        return 0;
    }

    memcpy(bytes, &header, sizeof(header));
    memcpy(bytes + header.rsh_bitblk, bitblks,
           sizeof(*bitblks) * (size_t)options->mode.bitmap.count);

    for (i = 0; i < options->mode.bitmap.count; ++i) {
        const bitmap_entry_t *entry = &options->mode.bitmap.entries[i];
        size_t plane_bytes = (size_t)entry->form.words_per_row *
                             (size_t)entry->form.height * sizeof(WORD);

        memcpy(bytes + (size_t)bitblks[i].bi_pdata, entry->form.data_words,
               plane_bytes);
    }

    free(bitblks);
    *bytes_out = bytes;
    *size_out = rssize;
    return 1;
}

int write_outputs(const resgen_options_t *options, const void *bytes,
                  size_t count)
{
    int i;

    if (options == NULL || bytes == NULL || count == 0u) {
        return 0;
    }
    for (i = 0; i < options->output_count; ++i) {
        if (!write_resource_to_path(options->outputs[i].path, bytes, count)) {
            return 0;
        }
    }
    return 1;
}
