/*
 * Generates a tiny host-native AES resource file for demo27. The file
 * stores string references as file-relative offsets so `rsrc_obfix()`
 * can relocate them after `rsrc_load()`.
 *
 * MIT License (see: LICENSE)
 * Copyright (C) 2026 tomaz stih
 */

#include <gem.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum {
    D27_ROOT = 0,
    D27_TEXT,
    D27_OK,
    D27_CANCEL,
    D27_OBJECT_COUNT
};

#define D27_STRING_COUNT 3

static size_t align_up(size_t value, size_t alignment)
{
    size_t remainder = value % alignment;

    return (remainder == 0) ? value : (value + alignment - remainder);
}

static int write_zeroes(FILE *stream, size_t count)
{
    static const char zeroes[16] = {0};

    while (count > 0) {
        size_t chunk = (count > sizeof(zeroes)) ? sizeof(zeroes) : count;

        if (fwrite(zeroes, chunk, 1, stream) != 1) {
            return 0;
        }
        count -= chunk;
    }
    return 1;
}

static int write_resource(FILE *stream)
{
    static const char *strings[D27_STRING_COUNT] = {
        "Loaded from demo27.rsc",
        "OK",
        "Cancel"
    };
    RSHDR header;
    WORD trindex[1];
    OBJECT objects[D27_OBJECT_COUNT];
    LONG string_offsets[D27_STRING_COUNT];
    size_t string_bytes = 0;
    size_t strings_start;
    char *string_block;
    size_t offset;
    int i;

    memset(&header, 0, sizeof(header));
    memset(objects, 0, sizeof(objects));

    header.rsh_vrsn = 0;
    header.rsh_trindex = (WORD) sizeof(RSHDR);
    header.rsh_object = (WORD) align_up(
        header.rsh_trindex + sizeof(trindex), sizeof(void *));
    header.rsh_tedinfo = (WORD) (header.rsh_object + sizeof(objects));
    header.rsh_iconblk = header.rsh_tedinfo;
    header.rsh_bitblk = header.rsh_iconblk;
    header.rsh_frstr = header.rsh_bitblk;
    header.rsh_string = (WORD) align_up(header.rsh_frstr, sizeof(LONG));
    header.rsh_imdata = (WORD) (header.rsh_string + sizeof(string_offsets));
    header.rsh_frimg = header.rsh_imdata;
    header.rsh_nobs = D27_OBJECT_COUNT;
    header.rsh_ntree = 1;
    header.rsh_nted = 0;
    header.rsh_nib = 0;
    header.rsh_nbb = 0;
    header.rsh_nstring = D27_STRING_COUNT;
    header.rsh_nimages = 0;

    trindex[0] = header.rsh_object;
    strings_start = header.rsh_imdata;
    for (i = 0; i < D27_STRING_COUNT; ++i) {
        size_t len = strlen(strings[i]) + 1u;

        string_offsets[i] = (LONG) strings_start;
        strings_start += len;
        string_bytes += len;
    }
    header.rsh_rssize = (WORD) strings_start;

    objects[D27_ROOT].ob_next = NIL;
    objects[D27_ROOT].ob_head = D27_TEXT;
    objects[D27_ROOT].ob_tail = D27_CANCEL;
    objects[D27_ROOT].ob_type = G_BOX;
    objects[D27_ROOT].ob_flags = LASTOB;
    objects[D27_ROOT].ob_state = NORMAL;
    objects[D27_ROOT].ob_spec = 0;
    objects[D27_ROOT].ob_x = 0;
    objects[D27_ROOT].ob_y = 0;
    objects[D27_ROOT].ob_width = 220;
    objects[D27_ROOT].ob_height = 96;

    objects[D27_TEXT].ob_next = D27_OK;
    objects[D27_TEXT].ob_head = NIL;
    objects[D27_TEXT].ob_tail = NIL;
    objects[D27_TEXT].ob_type = G_STRING;
    objects[D27_TEXT].ob_flags = NONE;
    objects[D27_TEXT].ob_state = NORMAL;
    objects[D27_TEXT].ob_spec = string_offsets[0];
    objects[D27_TEXT].ob_x = 14;
    objects[D27_TEXT].ob_y = 20;
    objects[D27_TEXT].ob_width = 160;
    objects[D27_TEXT].ob_height = 8;

    objects[D27_OK].ob_next = D27_CANCEL;
    objects[D27_OK].ob_head = NIL;
    objects[D27_OK].ob_tail = NIL;
    objects[D27_OK].ob_type = G_BUTTON;
    objects[D27_OK].ob_flags = SELECTABLE | EXIT | DEFAULT;
    objects[D27_OK].ob_state = NORMAL;
    objects[D27_OK].ob_spec = string_offsets[1];
    objects[D27_OK].ob_x = 96;
    objects[D27_OK].ob_y = 58;
    objects[D27_OK].ob_width = 48;
    objects[D27_OK].ob_height = 22;

    objects[D27_CANCEL].ob_next = D27_ROOT;
    objects[D27_CANCEL].ob_head = NIL;
    objects[D27_CANCEL].ob_tail = NIL;
    objects[D27_CANCEL].ob_type = G_BUTTON;
    objects[D27_CANCEL].ob_flags = SELECTABLE | EXIT;
    objects[D27_CANCEL].ob_state = NORMAL;
    objects[D27_CANCEL].ob_spec = string_offsets[2];
    objects[D27_CANCEL].ob_x = 152;
    objects[D27_CANCEL].ob_y = 58;
    objects[D27_CANCEL].ob_width = 56;
    objects[D27_CANCEL].ob_height = 22;

    string_block = malloc(string_bytes);
    if (string_block == NULL) {
        return 0;
    }

    offset = 0;
    for (i = 0; i < D27_STRING_COUNT; ++i) {
        size_t len = strlen(strings[i]) + 1u;

        memcpy(string_block + offset, strings[i], len);
        offset += len;
    }

    if (fwrite(&header, sizeof(header), 1, stream) != 1 ||
        fwrite(trindex, sizeof(trindex), 1, stream) != 1 ||
        !write_zeroes(stream, (size_t) header.rsh_object -
            (sizeof(header) + sizeof(trindex))) ||
        fwrite(objects, sizeof(objects), 1, stream) != 1 ||
        !write_zeroes(stream, (size_t) header.rsh_string -
            (header.rsh_object + sizeof(objects))) ||
        fwrite(string_offsets, sizeof(string_offsets), 1, stream) != 1 ||
        fwrite(string_block, string_bytes, 1, stream) != 1) {
        free(string_block);
        return 0;
    }

    free(string_block);
    return 1;
}

int main(int argc, char **argv)
{
    FILE *sample_file;
    FILE *bin_file;

    if (argc != 3) {
        return 1;
    }

    sample_file = fopen(argv[1], "wb");
    if (sample_file == NULL) {
        return 1;
    }
    if (!write_resource(sample_file)) {
        fclose(sample_file);
        return 1;
    }
    fclose(sample_file);

    bin_file = fopen(argv[2], "wb");
    if (bin_file == NULL) {
        return 1;
    }
    if (!write_resource(bin_file)) {
        fclose(bin_file);
        return 1;
    }
    fclose(bin_file);

    return 0;
}
