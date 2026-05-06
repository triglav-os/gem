#define _POSIX_C_SOURCE 200809L

/*
 * Generates small host-native GEM resource files from command-line
 * inputs. The current tool supports cursor and icon resource modes and
 * relies on ImageMagick's `convert` utility for PNG pixel decoding so
 * the generated `.rsc` files can stay plain GEM data.
 *
 * MIT License (see: LICENSE)
 * Copyright (C) 2026 tomaz stih
 */

#include <gem.h>

#include <ctype.h>
#include <errno.h>
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

enum {
    resgen_cursor_slots = 8,
    resgen_max_outputs = 8,
    resgen_default_text_height = 8,
    resgen_default_cursor_bee_hot_x = 7,
    resgen_default_cursor_bee_hot_y = 7
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
    cursor_spec_t arrow;
    cursor_spec_t bee;
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

static void print_usage(FILE *stream, const char *program_name)
{
    fprintf(stream,
        "Usage:\n"
        "  %s cursors -t ab POINTER.PNG BEE.PNG -o out.rsc [-o out2.rsc]\n"
        "  %s icons -o out.rsc [-o out2.rsc] ICON1.PNG [ICON2.PNG ...]\n"
        "  %s bitmaps -o out.rsc [-o out2.rsc] BMP1.PNG [BMP2.PNG ...]\n"
        "\n"
        "Cursor mode options:\n"
        "  -t TYPES  file type list, e.g. 'ab' for arrow then bee\n"
        "  -A X,Y    arrow hot spot (default 0,0)\n"
        "  -B X,Y    bee hot spot (default 7,7)\n"
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
    if (end == NULL || *end != '\0' || x < 0 || y < 0 ||
        x > 32767L || y > 32767L) {
        return 0;
    }

    *x_out = (WORD) x;
    *y_out = (WORD) y;
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
    length = (dot != NULL && dot > base) ? (size_t) (dot - base) :
        strlen(base);
    label = malloc(length + 1u);
    if (label == NULL) {
        return NULL;
    }
    for (i = 0; i < length; ++i) {
        label[i] = (char) toupper((unsigned char) base[i]);
    }
    label[length] = '\0';
    return label;
}

static void free_image_form(image_form_t *form)
{
    if (form == NULL) {
        return;
    }
    free(form->mask_words);
    free(form->data_words);
    memset(form, 0, sizeof(*form));
}

static void free_options(resgen_options_t *options)
{
    int i;

    if (options == NULL) {
        return;
    }

    if (options->mode_name != NULL &&
        strcmp(options->mode_name, "cursors") == 0) {
        free_image_form(&options->mode.cursor.arrow.form);
        free_image_form(&options->mode.cursor.bee.form);
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

static int spawn_convert_stream(const char *path, FILE **stream_out, pid_t *pid_out)
{
    int pipe_fds[2];
    pid_t pid;

    if (path == NULL || stream_out == NULL || pid_out == NULL) {
        return 0;
    }
    if (pipe(pipe_fds) != 0) {
        return 0;
    }

    pid = fork();
    if (pid < 0) {
        close(pipe_fds[0]);
        close(pipe_fds[1]);
        return 0;
    }
    if (pid == 0) {
        char *const argv[] = {
            "convert",
            (char *) path,
            "txt:-",
            NULL
        };

        close(pipe_fds[0]);
        if (dup2(pipe_fds[1], STDOUT_FILENO) < 0) {
            _exit(127);
        }
        close(pipe_fds[1]);
        execvp("convert", argv);
        _exit(127);
    }

    close(pipe_fds[1]);
    *stream_out = fdopen(pipe_fds[0], "r");
    if (*stream_out == NULL) {
        close(pipe_fds[0]);
        (void) waitpid(pid, NULL, 0);
        return 0;
    }

    *pid_out = pid;
    return 1;
}

static int finish_convert_stream(FILE *stream, pid_t pid)
{
    int status = 0;

    if (stream != NULL) {
        fclose(stream);
    }
    if (waitpid(pid, &status, 0) < 0) {
        return 0;
    }
    return WIFEXITED(status) && WEXITSTATUS(status) == 0;
}

static int pixel_is_transparent(const char *hex)
{
    return hex != NULL && strcmp(hex, "#00000000") == 0;
}

static int parse_hex_rgba(const char *hex,
                          unsigned int *red_out,
                          unsigned int *green_out,
                          unsigned int *blue_out,
                          unsigned int *alpha_out)
{
    unsigned int red = 0u;
    unsigned int green = 0u;
    unsigned int blue = 0u;
    unsigned int alpha = 255u;

    if (hex == NULL || hex[0] != '#') {
        return 0;
    }

    if (strlen(hex) == 7u) {
        if (sscanf(hex + 1, "%2x%2x%2x", &red, &green, &blue) != 3) {
            return 0;
        }
    } else if (strlen(hex) == 9u) {
        if (sscanf(hex + 1, "%2x%2x%2x%2x", &red, &green, &blue,
            &alpha) != 4) {
            return 0;
        }
    } else {
        return 0;
    }

    if (red_out != NULL) {
        *red_out = red;
    }
    if (green_out != NULL) {
        *green_out = green;
    }
    if (blue_out != NULL) {
        *blue_out = blue;
    }
    if (alpha_out != NULL) {
        *alpha_out = alpha;
    }
    return 1;
}

static int pixel_is_black(const char *hex)
{
    return hex != NULL && strcmp(hex, "#000000FF") == 0;
}

static int pixel_is_white(const char *hex)
{
    return hex != NULL &&
        (strcmp(hex, "#FFFFFFFF") == 0 || strcmp(hex, "#FFFFFF") == 0);
}

static int classify_pixel(const char *hex, int *visible_out, int *black_out)
{
    unsigned int red = 0u;
    unsigned int green = 0u;
    unsigned int blue = 0u;
    unsigned int alpha = 0u;
    unsigned int max_channel;
    unsigned int min_channel;

    if (visible_out == NULL || black_out == NULL) {
        return 0;
    }

    if (pixel_is_transparent(hex)) {
        *visible_out = 0;
        *black_out = 0;
        return 1;
    }
    if (pixel_is_black(hex)) {
        *visible_out = 1;
        *black_out = 1;
        return 1;
    }
    if (pixel_is_white(hex)) {
        *visible_out = 1;
        *black_out = 0;
        return 1;
    }
    if (!parse_hex_rgba(hex, &red, &green, &blue, &alpha)) {
        return 0;
    }
    if (alpha < 32u) {
        *visible_out = 0;
        *black_out = 0;
        return 1;
    }

    max_channel = red;
    if (green > max_channel) {
        max_channel = green;
    }
    if (blue > max_channel) {
        max_channel = blue;
    }
    min_channel = red;
    if (green < min_channel) {
        min_channel = green;
    }
    if (blue < min_channel) {
        min_channel = blue;
    }

    *visible_out = 1;
    *black_out = (min_channel < 224u);
    return 1;
}

static int alloc_form_words(image_form_t *form, WORD width, WORD height)
{
    size_t word_count;

    if (form == NULL || width <= 0 || height <= 0) {
        return 0;
    }

    form->width = width;
    form->height = height;
    form->words_per_row = (WORD) ((width + 15) / 16);
    word_count = (size_t) form->words_per_row * (size_t) height;
    form->mask_words = calloc(word_count, sizeof(WORD));
    form->data_words = calloc(word_count, sizeof(WORD));
    if (form->mask_words == NULL || form->data_words == NULL) {
        free_image_form(form);
        return 0;
    }
    return 1;
}

static void set_form_pixel(image_form_t *form, WORD x, WORD y, int black)
{
    size_t index;
    WORD bit;

    if (form == NULL || x < 0 || y < 0 || x >= form->width || y >= form->height) {
        return;
    }

    index = (size_t) y * (size_t) form->words_per_row + (size_t) x / 16u;
    bit = (WORD) ((UWORD) 0x8000u >> ((unsigned int) x & 15u));
    form->mask_words[index] |= bit;
    if (black) {
        form->data_words[index] |= bit;
    }
}

static int load_png_form(const char *path, image_form_t *form)
{
    FILE *stream = NULL;
    pid_t pid = -1;
    char line[256];
    int header_seen = 0;

    if (path == NULL || form == NULL) {
        return 0;
    }
    if (!spawn_convert_stream(path, &stream, &pid)) {
        fprintf(stderr, "resgen: unable to start convert for %s\n", path);
        return 0;
    }

    while (fgets(line, sizeof(line), stream) != NULL) {
        if (!header_seen) {
            int width = 0;
            int height = 0;

            if (sscanf(line, "# ImageMagick pixel enumeration: %d,%d,",
                &width, &height) == 2) {
                if (width <= 0 || height <= 0 ||
                    !alloc_form_words(form, (WORD) width, (WORD) height)) {
                    fclose(stream);
                    (void) waitpid(pid, NULL, 0);
                    return 0;
                }
                header_seen = 1;
            }
            continue;
        } else {
            int x = 0;
            int y = 0;
            int visible = 0;
            int black = 0;
            char hex[16];

            if (sscanf(line, "%d,%d: %*[^#] %15s", &x, &y, hex) != 3) {
                continue;
            }
            if (!classify_pixel(hex, &visible, &black)) {
                fprintf(stderr,
                    "resgen: unsupported non-monochrome pixel %s in %s\n",
                    hex, path);
                fclose(stream);
                (void) waitpid(pid, NULL, 0);
                return 0;
            }
            if (!visible) {
                continue;
            }
            set_form_pixel(form, (WORD) x, (WORD) y, black);
        }
    }

    if (!finish_convert_stream(stream, pid)) {
        fprintf(stderr, "resgen: convert failed for %s\n", path);
        return 0;
    }
    if (!header_seen) {
        fprintf(stderr, "resgen: no pixel header from convert for %s\n", path);
        return 0;
    }
    return 1;
}

static size_t align_up(size_t value, size_t alignment)
{
    size_t remainder = value % alignment;

    return (remainder == 0u) ? value : (value + alignment - remainder);
}

static int write_file_bytes(FILE *stream, const void *bytes, size_t count)
{
    return count == 0u || fwrite(bytes, count, 1, stream) == 1;
}

static int write_resource_to_path(const char *path,
                                  const void *bytes,
                                  size_t count)
{
    FILE *stream;
    int ok;

    stream = fopen(path, "wb");
    if (stream == NULL) {
        fprintf(stderr, "resgen: unable to open %s for writing: %s\n",
            path, strerror(errno));
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

static int build_cursor_rsc(const resgen_options_t *options,
                            uint8_t **bytes_out,
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
        slots[i] = options->mode.cursor.arrow;
    }
    slots[0] = options->mode.cursor.arrow;
    slots[2] = options->mode.cursor.bee;

    header.rsh_vrsn = 0;
    header.rsh_object = (WORD) sizeof(RSHDR);
    header.rsh_tedinfo = header.rsh_object;
    header.rsh_iconblk = (WORD) align_up(sizeof(RSHDR), sizeof(LONG));
    header.rsh_bitblk = (WORD) (header.rsh_iconblk + sizeof(icons));
    header.rsh_frstr = header.rsh_bitblk;
    header.rsh_string = header.rsh_frstr;
    header.rsh_imdata = (WORD) align_up((size_t) header.rsh_string,
        sizeof(LONG));
    header.rsh_frimg = header.rsh_imdata;
    header.rsh_trindex = header.rsh_frimg;
    header.rsh_nib = resgen_cursor_slots;

    offset = (size_t) header.rsh_imdata;
    for (i = 0; i < resgen_cursor_slots; ++i) {
        size_t plane_bytes = (size_t) slots[i].form.words_per_row *
            (size_t) slots[i].form.height * sizeof(WORD);

        icons[i].ib_pmask = (LONG) offset;
        offset += plane_bytes;
        icons[i].ib_pdata = (LONG) offset;
        offset += plane_bytes;
        icons[i].ib_xchar = slots[i].hot_x;
        icons[i].ib_ychar = slots[i].hot_y;
        icons[i].ib_wicon = slots[i].form.width;
        icons[i].ib_hicon = slots[i].form.height;
        total_imdata_bytes += plane_bytes * 2u;
    }

    header.rsh_rssize = (WORD) offset;
    rssize = offset;
    bytes = calloc(rssize, 1u);
    if (bytes == NULL) {
        return 0;
    }

    memcpy(bytes, &header, sizeof(header));
    memcpy(bytes + header.rsh_iconblk, icons, sizeof(icons));
    for (i = 0; i < resgen_cursor_slots; ++i) {
        size_t plane_bytes = (size_t) slots[i].form.words_per_row *
            (size_t) slots[i].form.height * sizeof(WORD);

        memcpy(bytes + (size_t) icons[i].ib_pmask, slots[i].form.mask_words,
            plane_bytes);
        memcpy(bytes + (size_t) icons[i].ib_pdata, slots[i].form.data_words,
            plane_bytes);
    }

    (void) total_imdata_bytes;
    *bytes_out = bytes;
    *size_out = rssize;
    return 1;
}

static int build_icon_rsc(const resgen_options_t *options,
                          uint8_t **bytes_out,
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

    icons = calloc((size_t) options->mode.icon.count, sizeof(*icons));
    string_offsets = calloc((size_t) options->mode.icon.count,
        sizeof(*string_offsets));
    if (icons == NULL || string_offsets == NULL) {
        free(icons);
        free(string_offsets);
        return 0;
    }

    memset(&header, 0, sizeof(header));
    header.rsh_vrsn = 0;
    header.rsh_object = (WORD) sizeof(RSHDR);
    header.rsh_tedinfo = header.rsh_object;
    header.rsh_iconblk = (WORD) align_up(sizeof(RSHDR), sizeof(LONG));
    header.rsh_bitblk = (WORD) (header.rsh_iconblk +
        sizeof(*icons) * (size_t) options->mode.icon.count);
    header.rsh_frstr = header.rsh_bitblk;
    header.rsh_string = (WORD) align_up((size_t) header.rsh_frstr,
        sizeof(LONG));
    header.rsh_imdata = (WORD) align_up(
        (size_t) header.rsh_string +
        sizeof(*string_offsets) * (size_t) options->mode.icon.count,
        sizeof(LONG));
    header.rsh_frimg = header.rsh_imdata;
    header.rsh_trindex = header.rsh_frimg;
    header.rsh_nib = (WORD) options->mode.icon.count;
    header.rsh_nstring = (WORD) options->mode.icon.count;

    offset = (size_t) header.rsh_imdata;
    for (i = 0; i < options->mode.icon.count; ++i) {
        const icon_entry_t *entry = &options->mode.icon.entries[i];
        size_t plane_bytes = (size_t) entry->form.words_per_row *
            (size_t) entry->form.height * sizeof(WORD);

        icons[i].ib_pmask = (LONG) offset;
        offset += plane_bytes;
        icons[i].ib_pdata = (LONG) offset;
        offset += plane_bytes;
        icons[i].ib_wicon = entry->form.width;
        icons[i].ib_hicon = entry->form.height;
        icons[i].ib_xicon = 0;
        icons[i].ib_yicon = 0;
        icons[i].ib_xtext = 0;
        icons[i].ib_ytext = entry->form.height;
        icons[i].ib_wtext = (WORD) ((WORD) strlen(entry->label) * 8);
        icons[i].ib_htext = resgen_default_text_height;
        strings_bytes += strlen(entry->label) + 1u;
    }

    {
        size_t strings_start = align_up(offset, sizeof(LONG));

        offset = strings_start;
        for (i = 0; i < options->mode.icon.count; ++i) {
            string_offsets[i] = (LONG) offset;
            icons[i].ib_ptext = string_offsets[i];
            offset += strlen(options->mode.icon.entries[i].label) + 1u;
        }
    }

    header.rsh_rssize = (WORD) offset;
    rssize = offset;
    bytes = calloc(rssize, 1u);
    if (bytes == NULL) {
        free(icons);
        free(string_offsets);
        return 0;
    }

    memcpy(bytes, &header, sizeof(header));
    memcpy(bytes + header.rsh_iconblk, icons,
        sizeof(*icons) * (size_t) options->mode.icon.count);
    memcpy(bytes + header.rsh_string, string_offsets,
        sizeof(*string_offsets) * (size_t) options->mode.icon.count);

    for (i = 0; i < options->mode.icon.count; ++i) {
        const icon_entry_t *entry = &options->mode.icon.entries[i];
        size_t plane_bytes = (size_t) entry->form.words_per_row *
            (size_t) entry->form.height * sizeof(WORD);

        memcpy(bytes + (size_t) icons[i].ib_pmask, entry->form.mask_words,
            plane_bytes);
        memcpy(bytes + (size_t) icons[i].ib_pdata, entry->form.data_words,
            plane_bytes);
        memcpy(bytes + (size_t) string_offsets[i], entry->label,
            strlen(entry->label) + 1u);
    }

    free(icons);
    free(string_offsets);
    (void) strings_bytes;
    *bytes_out = bytes;
    *size_out = rssize;
    return 1;
}

static int build_bitmap_rsc(const resgen_options_t *options,
                            uint8_t **bytes_out,
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

    bitblks = calloc((size_t) options->mode.bitmap.count, sizeof(*bitblks));
    if (bitblks == NULL) {
        return 0;
    }

    memset(&header, 0, sizeof(header));
    header.rsh_vrsn = 0;
    header.rsh_object = (WORD) sizeof(RSHDR);
    header.rsh_tedinfo = header.rsh_object;
    header.rsh_iconblk = header.rsh_tedinfo;
    header.rsh_bitblk = (WORD) align_up(sizeof(RSHDR), sizeof(LONG));
    header.rsh_frstr = (WORD) (header.rsh_bitblk +
        sizeof(*bitblks) * (size_t) options->mode.bitmap.count);
    header.rsh_string = header.rsh_frstr;
    header.rsh_imdata = (WORD) align_up((size_t) header.rsh_string,
        sizeof(LONG));
    header.rsh_frimg = header.rsh_imdata;
    header.rsh_trindex = header.rsh_frimg;
    header.rsh_nbb = (WORD) options->mode.bitmap.count;

    offset = (size_t) header.rsh_imdata;
    for (i = 0; i < options->mode.bitmap.count; ++i) {
        const bitmap_entry_t *entry = &options->mode.bitmap.entries[i];
        size_t plane_bytes = (size_t) entry->form.words_per_row *
            (size_t) entry->form.height * sizeof(WORD);

        bitblks[i].bi_pdata = (LONG) offset;
        bitblks[i].bi_wb = (WORD) (entry->form.words_per_row *
            (WORD) sizeof(WORD));
        bitblks[i].bi_hl = entry->form.height;
        bitblks[i].bi_x = 0;
        bitblks[i].bi_y = 0;
        bitblks[i].bi_color = 1;
        offset += plane_bytes;
    }

    header.rsh_rssize = (WORD) offset;
    rssize = offset;
    bytes = calloc(rssize, 1u);
    if (bytes == NULL) {
        free(bitblks);
        return 0;
    }

    memcpy(bytes, &header, sizeof(header));
    memcpy(bytes + header.rsh_bitblk, bitblks,
        sizeof(*bitblks) * (size_t) options->mode.bitmap.count);

    for (i = 0; i < options->mode.bitmap.count; ++i) {
        const bitmap_entry_t *entry = &options->mode.bitmap.entries[i];
        size_t plane_bytes = (size_t) entry->form.words_per_row *
            (size_t) entry->form.height * sizeof(WORD);

        memcpy(bytes + (size_t) bitblks[i].bi_pdata, entry->form.data_words,
            plane_bytes);
    }

    free(bitblks);
    *bytes_out = bytes;
    *size_out = rssize;
    return 1;
}

static int write_outputs(const resgen_options_t *options,
                         const void *bytes,
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

static int parse_cursor_mode(int argc,
                             char **argv,
                             resgen_options_t *options)
{
    int opt;
    int file_count;
    int i;

    options->mode.cursor.arrow.hot_x = 0;
    options->mode.cursor.arrow.hot_y = 0;
    options->mode.cursor.bee.hot_x = resgen_default_cursor_bee_hot_x;
    options->mode.cursor.bee.hot_y = resgen_default_cursor_bee_hot_y;

    optind = 1;
    while ((opt = getopt(argc, argv, "t:A:B:o:h")) != -1) {
        switch (opt) {
        case 't':
            options->mode.cursor.type_list = optarg;
            break;
        case 'A':
            if (!parse_hotspot(optarg, &options->mode.cursor.arrow.hot_x,
                &options->mode.cursor.arrow.hot_y)) {
                return 0;
            }
            break;
        case 'B':
            if (!parse_hotspot(optarg, &options->mode.cursor.bee.hot_x,
                &options->mode.cursor.bee.hot_y)) {
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
        (size_t) file_count != strlen(options->mode.cursor.type_list)) {
        return 0;
    }

    for (i = 0; i < file_count; ++i) {
        const char type = options->mode.cursor.type_list[i];
        const char *path = argv[optind + i];

        switch (type) {
        case 'a':
            options->mode.cursor.arrow.path = path;
            break;
        case 'b':
            options->mode.cursor.bee.path = path;
            break;
        default:
            return 0;
        }
    }

    if (options->mode.cursor.arrow.path == NULL ||
        options->mode.cursor.bee.path == NULL) {
        return 0;
    }
    if (!load_png_form(options->mode.cursor.arrow.path,
        &options->mode.cursor.arrow.form)) {
        return 0;
    }
    if (!load_png_form(options->mode.cursor.bee.path,
        &options->mode.cursor.bee.form)) {
        return 0;
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

    options->mode.icon.entries = calloc((size_t) count,
        sizeof(*options->mode.icon.entries));
    if (options->mode.icon.entries == NULL) {
        return 0;
    }
    options->mode.icon.count = count;
    for (i = 0; i < count; ++i) {
        icon_entry_t *entry = &options->mode.icon.entries[i];

        entry->path = argv[optind + i];
        entry->label = make_icon_label(entry->path);
        if (entry->label == NULL ||
            !load_png_form(entry->path, &entry->form)) {
            return 0;
        }
    }
    return 1;
}

static int parse_bitmap_mode(int argc,
                             char **argv,
                             resgen_options_t *options)
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

    options->mode.bitmap.entries = calloc((size_t) count,
        sizeof(*options->mode.bitmap.entries));
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
