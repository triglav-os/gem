/*
 * Verify bounded text replacement, UTF-8 navigation and undo preservation.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (C) 2026 tomaz stih
 */
#include "document.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>

int main(void)
{
    ted_document_t doc;
    assert(ted_document_init(&doc));
    assert(ted_document_insert(&doc, "one\ntwo", 7));
    assert(ted_document_line(&doc, 1) == 4);
    size_t line, column;
    ted_document_position(&doc, 6, &line, &column);
    assert(line == 1 && column == 2);
    doc.anchor = 4;
    assert(ted_document_insert(&doc, "\xc3\xa9", 2));
    assert(strcmp(doc.text, "one\n\xc3\xa9") == 0);
    assert(ted_document_step(&doc, 6, -1) == 4);
    assert(ted_document_step(&doc, 4, 1) == 6);
    assert(!ted_document_insert(&doc, "a\0b", 3));
    assert(doc.length == 6);
    ted_document_undo(&doc);
    assert(strcmp(doc.text, "one\ntwo") == 0);
    ted_document_undo(&doc);
    assert(strcmp(doc.text, "one\n\xc3\xa9") == 0);
    doc.anchor = 0;
    char *large = malloc(ted_limit);
    assert(large);
    memset(large, 'x', ted_limit);
    assert(ted_document_insert(&doc, large, ted_limit));
    assert(!ted_document_insert(&doc, "!", 1));
    assert(doc.length == ted_limit);
    ted_document_undo(&doc);
    assert(strcmp(doc.text, "one\n\xc3\xa9") == 0);
    free(large);
    ted_document_free(&doc);
    assert(ted_document_init(&doc));
    assert(ted_document_insert(&doc, "", 0));
    assert(!doc.dirty);
    assert(ted_document_insert(&doc, "a", 1));
    ted_document_undo(&doc);
    assert(!doc.dirty && !doc.length);
    ted_document_free(&doc);
    return 0;
}
