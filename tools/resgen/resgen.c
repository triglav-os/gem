/*
 * Generates small host-native GEM resource files from command-line
 * inputs: parses the cursor, icon and bitmap modes and their options,
 * then hands decoded PNG forms (png_form.c) to the resource writer
 * (rsc_writer.c). ImageMagick's `convert` performs the pixel decoding so
 * the generated `.rsc` files stay plain GEM data.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (C) 2026 tomaz stih
 */

#define _POSIX_C_SOURCE 200809L

#include "resgen.h"

#include <ctype.h>
#include <errno.h>
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

static void print_usage(FILE *stream, const char *program_name)
{
    fprintf(stream,
            "Usage:\n"
            "  %s cursors -t 01234567 CUR0.PNG ... CUR7.PNG -o out.rsc\n"
            "  %s icons -o out.rsc [-o out2.rsc] ICON1.PNG [ICON2.PNG ...]\n"
            "  %s bitmaps -o out.rsc [-o out2.rsc] BMP1.PNG [BMP2.PNG ...]\n"
            "\n"
            "Cursor mode options:\n"
            "  -t TYPES  selector list, e.g. '01234567' in Atari ST order\n"
            "            0=arrow 1=text 2=bee 3=point 4=flat 5=thin\n"
            "            6=thick 7=outline (legacy 'a' and 'b' still work)\n"
            "  -A X,Y    arrow hot spot override (selector 0)\n"
            "  -B X,Y    bee hot spot override (selector 2)\n"
            "\n"
            "Shared options:\n"
            "  -o FILE   output resource path (repeatable)\n"
            "  -h        show this help text\n",
            program_name, program_name, program_name);
}

static int parse_hotspot(const char *text, WORD *x_out, WORD *y_out)
{
    char *end = NULL;
    long x;
    long y;

    if (text == NULL || x_out == NULL || y_out == NULL) {
        return 0;
    }

    x = strtol(text, &end, 10);
    if (end == text || end == NULL || *end != ',') {
        return 0;
    }
    y = strtol(end + 1, &end, 10);
    if (end == NULL || *end != '\0' || x < 0 || y < 0 || x > 32767L ||
        y > 32767L) {
        return 0;
    }

    *x_out = (WORD)x;
    *y_out = (WORD)y;
    return 1;
}

static int add_output_path(resgen_options_t *options, const char *path)
{
    if (options == NULL || path == NULL) {
        return 0;
    }
    if (options->output_count >= resgen_max_outputs) {
        fprintf(stderr, "%s: too many -o outputs\n", options->program_name);
        return 0;
    }

    options->outputs[options->output_count].path = path;
    ++options->output_count;
    return 1;
}

static char *make_icon_label(const char *path)
{
    const char *base;
    const char *dot;
    size_t length;
    char *label;
    size_t i;

    if (path == NULL) {
        return NULL;
    }

    base = strrchr(path, '/');
    base = (base == NULL) ? path : base + 1;
    dot = strrchr(base, '.');
    length = (dot != NULL && dot > base) ? (size_t)(dot - base) : strlen(base);
    label = malloc(length + 1u);
    if (label == NULL) {
        return NULL;
    }
    for (i = 0; i < length; ++i) {
        label[i] = (char)toupper((unsigned char)base[i]);
    }
    label[length] = '\0';
    return label;
}

static void free_options(resgen_options_t *options)
{
    int i;

    if (options == NULL) {
        return;
    }

    if (options->mode_name != NULL &&
        strcmp(options->mode_name, "cursors") == 0) {
        for (i = 0; i < resgen_cursor_slots; ++i) {
            free_image_form(&options->mode.cursor.slots[i].form);
        }
    } else if (options->mode_name != NULL &&
               strcmp(options->mode_name, "icons") == 0) {
        if (options->mode.icon.entries != NULL) {
            for (i = 0; i < options->mode.icon.count; ++i) {
                free(options->mode.icon.entries[i].label);
                free_image_form(&options->mode.icon.entries[i].form);
            }
            free(options->mode.icon.entries);
        }
    } else if (options->mode_name != NULL &&
               strcmp(options->mode_name, "bitmaps") == 0) {
        if (options->mode.bitmap.entries != NULL) {
            for (i = 0; i < options->mode.bitmap.count; ++i) {
                free_image_form(&options->mode.bitmap.entries[i].form);
            }
            free(options->mode.bitmap.entries);
        }
    }
    memset(options, 0, sizeof(*options));
}

static int parse_cursor_mode(int argc, char **argv, resgen_options_t *options)
{
    int opt;
    int file_count;
    int i;

    options->mode.cursor.slots[resgen_cursor_arrow].hot_x =
        resgen_default_cursor_arrow_hot_x;
    options->mode.cursor.slots[resgen_cursor_arrow].hot_y =
        resgen_default_cursor_arrow_hot_y;
    options->mode.cursor.slots[resgen_cursor_text].hot_x =
        resgen_default_cursor_text_hot_x;
    options->mode.cursor.slots[resgen_cursor_text].hot_y =
        resgen_default_cursor_text_hot_y;
    options->mode.cursor.slots[resgen_cursor_bee].hot_x =
        resgen_default_cursor_bee_hot_x;
    options->mode.cursor.slots[resgen_cursor_bee].hot_y =
        resgen_default_cursor_bee_hot_y;
    options->mode.cursor.slots[resgen_cursor_point_hand].hot_x =
        resgen_default_cursor_point_hand_hot_x;
    options->mode.cursor.slots[resgen_cursor_point_hand].hot_y =
        resgen_default_cursor_point_hand_hot_y;
    options->mode.cursor.slots[resgen_cursor_flat_hand].hot_x =
        resgen_default_cursor_flat_hand_hot_x;
    options->mode.cursor.slots[resgen_cursor_flat_hand].hot_y =
        resgen_default_cursor_flat_hand_hot_y;
    options->mode.cursor.slots[resgen_cursor_thin_cross].hot_x =
        resgen_default_cursor_cross_hot_x;
    options->mode.cursor.slots[resgen_cursor_thin_cross].hot_y =
        resgen_default_cursor_cross_hot_y;
    options->mode.cursor.slots[resgen_cursor_thick_cross].hot_x =
        resgen_default_cursor_cross_hot_x;
    options->mode.cursor.slots[resgen_cursor_thick_cross].hot_y =
        resgen_default_cursor_cross_hot_y;
    options->mode.cursor.slots[resgen_cursor_outline_cross].hot_x =
        resgen_default_cursor_cross_hot_x;
    options->mode.cursor.slots[resgen_cursor_outline_cross].hot_y =
        resgen_default_cursor_cross_hot_y;

    optind = 1;
    while ((opt = getopt(argc, argv, "t:A:B:o:h")) != -1) {
        switch (opt) {
            case 't':
                options->mode.cursor.type_list = optarg;
                break;
            case 'A':
                if (!parse_hotspot(
                        optarg,
                        &options->mode.cursor.slots[resgen_cursor_arrow].hot_x,
                        &options->mode.cursor.slots[resgen_cursor_arrow]
                             .hot_y)) {
                    return 0;
                }
                break;
            case 'B':
                if (!parse_hotspot(
                        optarg,
                        &options->mode.cursor.slots[resgen_cursor_bee].hot_x,
                        &options->mode.cursor.slots[resgen_cursor_bee].hot_y)) {
                    return 0;
                }
                break;
            case 'o':
                if (!add_output_path(options, optarg)) {
                    return 0;
                }
                break;
            case 'h':
                print_usage(stdout, options->program_name);
                exit(0);
            default:
                return 0;
        }
    }

    file_count = argc - optind;
    if (options->mode.cursor.type_list == NULL || options->output_count == 0 ||
        file_count <= 0 ||
        (size_t)file_count != strlen(options->mode.cursor.type_list)) {
        return 0;
    }

    for (i = 0; i < file_count; ++i) {
        const char type = options->mode.cursor.type_list[i];
        const char *path = argv[optind + i];
        int slot = -1;

        switch (type) {
            case '0':
            case 'a':
                slot = resgen_cursor_arrow;
                break;
            case '1':
                slot = resgen_cursor_text;
                break;
            case '2':
            case 'b':
                slot = resgen_cursor_bee;
                break;
            case '3':
                slot = resgen_cursor_point_hand;
                break;
            case '4':
                slot = resgen_cursor_flat_hand;
                break;
            case '5':
                slot = resgen_cursor_thin_cross;
                break;
            case '6':
                slot = resgen_cursor_thick_cross;
                break;
            case '7':
                slot = resgen_cursor_outline_cross;
                break;
            default:
                return 0;
        }

        options->mode.cursor.slots[slot].path = path;
    }

    if (options->mode.cursor.slots[resgen_cursor_arrow].path == NULL ||
        options->mode.cursor.slots[resgen_cursor_bee].path == NULL) {
        return 0;
    }
    for (i = 0; i < resgen_cursor_slots; ++i) {
        if (options->mode.cursor.slots[i].path == NULL) {
            continue;
        }
        if (!load_png_form(options->mode.cursor.slots[i].path,
                           &options->mode.cursor.slots[i].form)) {
            return 0;
        }
    }
    return 1;
}

static int parse_icon_mode(int argc, char **argv, resgen_options_t *options)
{
    int opt;
    int count;
    int i;

    optind = 1;
    while ((opt = getopt(argc, argv, "o:h")) != -1) {
        switch (opt) {
            case 'o':
                if (!add_output_path(options, optarg)) {
                    return 0;
                }
                break;
            case 'h':
                print_usage(stdout, options->program_name);
                exit(0);
            default:
                return 0;
        }
    }

    count = argc - optind;
    if (options->output_count == 0 || count <= 0) {
        return 0;
    }

    options->mode.icon.entries =
        calloc((size_t)count, sizeof(*options->mode.icon.entries));
    if (options->mode.icon.entries == NULL) {
        return 0;
    }
    options->mode.icon.count = count;
    for (i = 0; i < count; ++i) {
        icon_entry_t *entry = &options->mode.icon.entries[i];

        entry->path = argv[optind + i];
        entry->label = make_icon_label(entry->path);
        if (entry->label == NULL || !load_png_form(entry->path, &entry->form)) {
            return 0;
        }
    }
    return 1;
}

static int parse_bitmap_mode(int argc, char **argv, resgen_options_t *options)
{
    int opt;
    int count;
    int i;

    optind = 1;
    while ((opt = getopt(argc, argv, "o:h")) != -1) {
        switch (opt) {
            case 'o':
                if (!add_output_path(options, optarg)) {
                    return 0;
                }
                break;
            case 'h':
                print_usage(stdout, options->program_name);
                exit(0);
            default:
                return 0;
        }
    }

    count = argc - optind;
    if (options->output_count == 0 || count <= 0) {
        return 0;
    }

    options->mode.bitmap.entries =
        calloc((size_t)count, sizeof(*options->mode.bitmap.entries));
    if (options->mode.bitmap.entries == NULL) {
        return 0;
    }
    options->mode.bitmap.count = count;
    for (i = 0; i < count; ++i) {
        bitmap_entry_t *entry = &options->mode.bitmap.entries[i];

        entry->path = argv[optind + i];
        if (!load_png_form(entry->path, &entry->form)) {
            return 0;
        }
    }
    return 1;
}

int main(int argc, char **argv)
{
    resgen_options_t options;
    uint8_t *bytes = NULL;
    size_t size = 0u;
    int ok = 0;

    memset(&options, 0, sizeof(options));
    options.program_name = (argc > 0 && argv[0] != NULL) ? argv[0] : "resgen";

    if (argc < 2) {
        print_usage(stderr, options.program_name);
        return 1;
    }
    if (strcmp(argv[1], "-h") == 0 || strcmp(argv[1], "--help") == 0) {
        print_usage(stdout, options.program_name);
        return 0;
    }

    options.mode_name = argv[1];
    if (strcmp(options.mode_name, "cursors") == 0) {
        if (!parse_cursor_mode(argc - 1, argv + 1, &options) ||
            !build_cursor_rsc(&options, &bytes, &size) ||
            !write_outputs(&options, bytes, size)) {
            print_usage(stderr, options.program_name);
            free(bytes);
            free_options(&options);
            return 1;
        }
        ok = 1;
    } else if (strcmp(options.mode_name, "icons") == 0) {
        if (!parse_icon_mode(argc - 1, argv + 1, &options) ||
            !build_icon_rsc(&options, &bytes, &size) ||
            !write_outputs(&options, bytes, size)) {
            print_usage(stderr, options.program_name);
            free(bytes);
            free_options(&options);
            return 1;
        }
        ok = 1;
    } else if (strcmp(options.mode_name, "bitmaps") == 0) {
        if (!parse_bitmap_mode(argc - 1, argv + 1, &options) ||
            !build_bitmap_rsc(&options, &bytes, &size) ||
            !write_outputs(&options, bytes, size)) {
            print_usage(stderr, options.program_name);
            free(bytes);
            free_options(&options);
            return 1;
        }
        ok = 1;
    } else {
        print_usage(stderr, options.program_name);
    }

    free(bytes);
    free_options(&options);
    return ok ? 0 : 1;
}
