/*
 * Decodes PNG images into GEM monochrome forms for resgen by streaming
 * ImageMagick's `convert` output: pixels are classified as ink, paper or
 * transparent from their RGBA text values and packed into mask and data
 * word planes.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (C) 2026 tomaz stih
 */

#define _POSIX_C_SOURCE 200809L
#include "resgen.h"

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

void free_image_form(image_form_t *form)
{
    if (form == NULL) {
        return;
    }
    free(form->mask_words);
    free(form->data_words);
    memset(form, 0, sizeof(*form));
}

static int spawn_convert_stream(const char *path, FILE **stream_out,
                                pid_t *pid_out)
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
        char *const argv[] = {"convert", (char *)path, "txt:-", NULL};

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
        (void)waitpid(pid, NULL, 0);
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

static int parse_hex_rgba(const char *hex, unsigned int *red_out,
                          unsigned int *green_out, unsigned int *blue_out,
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
        if (sscanf(hex + 1, "%2x%2x%2x%2x", &red, &green, &blue, &alpha) != 4) {
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
    form->words_per_row = (WORD)((width + 15) / 16);
    word_count = (size_t)form->words_per_row * (size_t)height;
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

    if (form == NULL || x < 0 || y < 0 || x >= form->width ||
        y >= form->height) {
        return;
    }

    index = (size_t)y * (size_t)form->words_per_row + (size_t)x / 16u;
    bit = (WORD)((UWORD)0x8000u >> ((unsigned int)x & 15u));
    form->mask_words[index] |= bit;
    if (black) {
        form->data_words[index] |= bit;
    }
}

int load_png_form(const char *path, image_form_t *form)
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

            if (sscanf(line, "# ImageMagick pixel enumeration: %d,%d,", &width,
                       &height) == 2) {
                if (width <= 0 || height <= 0 ||
                    !alloc_form_words(form, (WORD)width, (WORD)height)) {
                    fclose(stream);
                    (void)waitpid(pid, NULL, 0);
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
                (void)waitpid(pid, NULL, 0);
                return 0;
            }
            if (!visible) {
                continue;
            }
            set_form_pixel(form, (WORD)x, (WORD)y, black);
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
