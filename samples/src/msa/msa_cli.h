/*
 * Declares the msa tool's option and context records and the host file
 * helpers shared by its command and extraction modules. Private to the
 * tool.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (C) 2026 tomaz stih
 */

#ifndef GEM_SAMPLE_MSA_CLI_H
#define GEM_SAMPLE_MSA_CLI_H

#include "fat12/fat12.h"
#include "msa/msa.h"

#include <stddef.h>
#include <stdint.h>

typedef struct msa_cli_options {
    msa_geometry_t geometry;
    int geometry_given;
} msa_cli_options_t;

typedef struct msa_ls_context {
    int printed_anything;
} msa_ls_context_t;

typedef struct msa_extract_context {
    const fat12_image_t *image;
    int root_fd;
    char error_text[MSA_ERROR_LENGTH];
} msa_extract_context_t;

/* Extract one FAT12 entry into the destination tree. */
int msa_extract_entry(const fat12_dirent_t *entry, const char *path,
                      void *context);
/* Create a directory and its parents. */
int msa_mkdir_p(const char *path);
/* Read a whole file into a new buffer; nonzero on success. */
int msa_read_binary_file(const char *path, uint8_t **data_out, size_t *size_out,
                         char *error_text, size_t error_size);
/* Write a buffer to a path; nonzero on success. */
int msa_write_binary_file(const char *path, const uint8_t *data, size_t size,
                          char *error_text, size_t error_size);

#endif
