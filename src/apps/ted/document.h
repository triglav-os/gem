/*
 * Ted's bounded UTF-8-preserving document and single-step undo model.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (C) 2026 tomaz stih
 */
#ifndef TED_DOCUMENT_H
#define TED_DOCUMENT_H
#include <stddef.h>
enum { ted_limit = 1024 * 1024 };
typedef struct ted_document {
    char *text, *undo;
    size_t length, cursor, anchor, undo_cursor;
    int dirty, undo_dirty;
} ted_document_t;
/* Initialize an empty document; return zero on allocation failure. */
int ted_document_init(ted_document_t *doc);
/* Release all document storage. */
void ted_document_free(ted_document_t *doc);
/* Replace selection atomically with bounded text; reject NUL/binary content. */
int ted_document_insert(ted_document_t *doc, const char *text, size_t length);
/* Restore the last edit, keeping a redo snapshot. */
void ted_document_undo(ted_document_t *doc);
/* Return the previous or next UTF-8 character boundary. */
size_t ted_document_step(const ted_document_t *doc, size_t offset,
                         int direction);
/* Locate a zero-based line start; return EOF when the line does not exist. */
size_t ted_document_line(const ted_document_t *doc, size_t line);
/* Return the line and byte column of an offset. */
void ted_document_position(const ted_document_t *doc, size_t offset,
                           size_t *line, size_t *column);
#endif
