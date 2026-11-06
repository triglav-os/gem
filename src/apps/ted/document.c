/*
 * Bounded plain-text edits and undo, independent of GEM rendering or files.
 * Existing UTF-8 bytes survive editing; cursor movement skips continuation
 * bytes.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (C) 2026 tomaz stih
 */
#include "document.h"
#include <stdlib.h>
#include <string.h>

int ted_document_init(ted_document_t *doc)
{
    memset(doc, 0, sizeof(*doc));
    doc->text = calloc(1, 1);
    return doc->text != NULL;
}
void ted_document_free(ted_document_t *doc)
{
    free(doc->text);
    free(doc->undo);
    memset(doc, 0, sizeof(*doc));
}
int ted_document_insert(ted_document_t *doc, const char *text, size_t length)
{
    size_t first = doc->cursor < doc->anchor ? doc->cursor : doc->anchor;
    size_t last = doc->cursor > doc->anchor ? doc->cursor : doc->anchor;
    if (last > doc->length)
        return 0;
    if (first == last && !length)
        return 1;
    if (length > ted_limit ||
        doc->length - (last - first) > ted_limit - length ||
        (length && (!text || memchr(text, 0, length))))
        return 0;
    size_t size = doc->length - (last - first) + length;
    char *next = malloc(size + 1);
    if (!next)
        return 0;
    memcpy(next, doc->text, first);
    if (length)
        memcpy(next + first, text, length);
    memcpy(next + first + length, doc->text + last, doc->length - last + 1);
    free(doc->undo);
    doc->undo = doc->text;
    doc->undo_cursor = doc->cursor;
    doc->undo_dirty = doc->dirty;
    doc->text = next;
    doc->length = size;
    doc->cursor = doc->anchor = first + length;
    doc->dirty = 1;
    return 1;
}
void ted_document_undo(ted_document_t *doc)
{
    if (!doc->undo)
        return;
    char *previous = doc->text;
    size_t cursor = doc->cursor;
    int dirty = doc->dirty;
    doc->text = doc->undo;
    doc->undo = previous;
    doc->length = strlen(doc->text);
    doc->cursor = doc->anchor = doc->undo_cursor;
    doc->dirty = doc->undo_dirty;
    doc->undo_cursor = cursor;
    doc->undo_dirty = dirty;
}
size_t ted_document_step(const ted_document_t *doc, size_t offset,
                         int direction)
{
    if (direction < 0) {
        if (offset)
            --offset;
        while (offset && ((unsigned char)doc->text[offset] & 0xc0) == 0x80)
            --offset;
    } else if (offset < doc->length) {
        ++offset;
        while (offset < doc->length &&
               ((unsigned char)doc->text[offset] & 0xc0) == 0x80)
            ++offset;
    }
    return offset;
}
size_t ted_document_line(const ted_document_t *doc, size_t line)
{
    size_t pos = 0;
    while (pos < doc->length && line)
        if (doc->text[pos++] == '\n')
            --line;
    return pos;
}
void ted_document_position(const ted_document_t *doc, size_t offset,
                           size_t *line, size_t *column)
{
    size_t start = 0;
    *line = 0;
    for (size_t i = 0; i < offset && i < doc->length; ++i)
        if (doc->text[i] == '\n') {
            ++*line;
            start = i + 1;
        }
    *column = offset - start;
}
