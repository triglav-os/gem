/*
 * Implements editable text fields for hosted AES forms: field discovery,
 * caret placement from clicks, insertion and deletion against the template
 * and validation strings, and the objc_edit entry point.
 *
 * MIT License (see: LICENSE)
 * Copyright (C) 2026 tomaz stih
 */

#include "form_internal.h"

#include <stdint.h>
#include <string.h>

int aes_form_is_editable(const OBJECT *tree, WORD object)
{
    if (tree == NULL || object < 0) {
        return 0;
    }

    return tree[object].ob_type == G_FTEXT ||
           tree[object].ob_type == G_FBOXTEXT;
}

WORD aes_form_find_next_editable(OBJECT *tree, WORD current, int backwards)
{
    WORD first = NIL;
    WORD last = aes_form_last_object(tree);
    WORD i;

    if (tree == NULL) {
        return NIL;
    }

    for (i = ROOT; i <= last; ++i) {
        if (aes_form_is_editable(tree, i) != 0) {
            first = i;
            break;
        }
    }
    if (first == NIL) {
        return NIL;
    }
    if (current == NIL || current < ROOT || current > last) {
        return first;
    }

    if (backwards == 0) {
        for (i = (WORD)(current + 1); i <= last; ++i) {
            if (aes_form_is_editable(tree, i) != 0) {
                return i;
            }
        }
        for (i = ROOT; i < current; ++i) {
            if (aes_form_is_editable(tree, i) != 0) {
                return i;
            }
        }
    } else {
        for (i = (WORD)(current - 1); i >= ROOT; --i) {
            if (aes_form_is_editable(tree, i) != 0) {
                return i;
            }
            if (i == ROOT) {
                break;
            }
        }
        for (i = last; i > current; --i) {
            if (aes_form_is_editable(tree, i) != 0) {
                return i;
            }
        }
    }

    return current;
}

void aes_form_set_active_field(OBJECT *tree, WORD object, WORD *idx)
{
    WORD current = aes_state.edit_object;

    if (tree == NULL || object < 0 || aes_form_is_editable(tree, object) == 0 ||
        idx == NULL) {
        return;
    }

    if (aes_state.edit_tree == tree && current >= 0 &&
        aes_form_is_editable(tree, current) != 0 && current != object) {
        tree[current].ob_state &= (UWORD)~SELECTED;
        aes_form_redraw_object(tree, current);
    }

    tree[object].ob_state |= SELECTED;
    objc_edit(tree, object, 0, idx, EDINIT);
    aes_form_redraw_object(tree, object);
}

void aes_form_set_caret_from_click(OBJECT *tree, WORD object, WORD mouse_x,
                                   WORD *idx)
{
    TEDINFO *ted;
    char *buffer;
    GRECT rect;
    WORD length;
    WORD rel_x;
    WORD index;
    char prefix[MAX_LEN];

    if (tree == NULL || object < 0 || idx == NULL ||
        aes_form_is_editable(tree, object) == 0 || tree[object].ob_spec == 0) {
        return;
    }

    ted = (TEDINFO *)(intptr_t)tree[object].ob_spec;
    buffer = (char *)(intptr_t)ted->te_ptext;
    if (buffer == NULL) {
        return;
    }

    aes_form_object_rect(tree, object, &rect);
    length = (WORD)strlen(buffer);
    rel_x = (WORD)(mouse_x - rect.g_x -
                   (tree[object].ob_type == G_FBOXTEXT ? 3 : 2));
    if (rel_x <= 0) {
        *idx = 0;
        objc_edit(tree, object, 0, idx, EDINIT);
        return;
    }

    for (index = 0; index < length; ++index) {
        WORD left_width;
        WORD right_width;
        WORD midpoint;

        memcpy(prefix, buffer, (size_t)index);
        prefix[index] = '\0';
        left_width = (WORD)vdi_string_width(prefix);

        memcpy(prefix, buffer, (size_t)(index + 1));
        prefix[index + 1] = '\0';
        right_width = (WORD)vdi_string_width(prefix);

        midpoint = (WORD)(left_width + (right_width - left_width) / 2);
        if (rel_x <= midpoint) {
            *idx = index;
            objc_edit(tree, object, 0, idx, EDINIT);
            return;
        }
    }

    *idx = length;
    objc_edit(tree, object, 0, idx, EDINIT);
}

static void aes_form_delete_backward(OBJECT *tree, WORD object, WORD *idx)
{
    TEDINFO *ted;
    char *buffer;
    size_t length;

    if (tree == NULL || object < 0 || idx == NULL || *idx <= 0 ||
        tree[object].ob_spec == 0) {
        return;
    }

    ted = (TEDINFO *)(intptr_t)tree[object].ob_spec;
    buffer = (char *)(intptr_t)ted->te_ptext;
    if (buffer == NULL) {
        return;
    }

    length = strlen(buffer);
    --(*idx);
    memmove(&buffer[*idx], &buffer[*idx + 1], length - (size_t)*idx);
    objc_edit(tree, object, 0, idx, EDINIT);
}

static void aes_form_delete_forward(OBJECT *tree, WORD object, WORD *idx)
{
    TEDINFO *ted;
    char *buffer;
    size_t length;

    if (tree == NULL || object < 0 || idx == NULL ||
        tree[object].ob_spec == 0) {
        return;
    }

    ted = (TEDINFO *)(intptr_t)tree[object].ob_spec;
    buffer = (char *)(intptr_t)ted->te_ptext;
    if (buffer == NULL) {
        return;
    }

    length = strlen(buffer);
    if ((size_t)*idx >= length) {
        return;
    }

    memmove(&buffer[*idx], &buffer[*idx + 1], length - (size_t)*idx);
    objc_edit(tree, object, 0, idx, EDINIT);
}

int aes_form_apply_key(OBJECT *tree, WORD object, WORD key, WORD *idx)
{
    WORD ascii;
    WORD scancode;

    if (tree == NULL || object < 0 || idx == NULL) {
        return 0;
    }

    ascii = (WORD)(key & 0xffu);
    scancode = (WORD)((key >> 8) & 0xffu);

    if (ascii == '\b' ||
        (ascii == 0 && (scancode == 42 || scancode == 14))) {
        aes_form_delete_backward(tree, object, idx);
        return 1;
    }
    if (scancode == 76 || scancode == 83) {
        aes_form_delete_forward(tree, object, idx);
        return 1;
    }
    if (scancode == 80 || scancode == 75) {
        if (*idx > 0) {
            --(*idx);
            objc_edit(tree, object, 0, idx, EDINIT);
        }
        return 1;
    }
    if (scancode == 79) {
        TEDINFO *ted = (TEDINFO *)(intptr_t)tree[object].ob_spec;
        char *buffer = (char *)(intptr_t)ted->te_ptext;
        WORD length = (buffer != NULL) ? (WORD)strlen(buffer) : 0;

        if (*idx < length) {
            ++(*idx);
            objc_edit(tree, object, 0, idx, EDINIT);
        }
        return 1;
    }
    if (scancode == 74 || scancode == 71) {
        *idx = 0;
        objc_edit(tree, object, 0, idx, EDINIT);
        return 1;
    }
    if (scancode == 77) {
        TEDINFO *ted = (TEDINFO *)(intptr_t)tree[object].ob_spec;
        char *buffer = (char *)(intptr_t)ted->te_ptext;

        *idx = (buffer != NULL) ? (WORD)strlen(buffer) : 0;
        objc_edit(tree, object, 0, idx, EDINIT);
        return 1;
    }
    if (ascii >= 32 && ascii <= 126) {
        objc_edit(tree, object, ascii, idx, EDCHAR);
        return 1;
    }

    return 0;
}

WORD objc_edit(OBJECT *tree, WORD object, WORD charidx, WORD *idx, WORD kind)
{
    TEDINFO *ted;
    char *text;
    size_t length;

    if (tree == NULL || object < 0 || idx == NULL) {
        return 0;
    }
    if (kind == EDSTART || kind == EDINIT) {
        aes_state.edit_tree = tree;
        aes_state.edit_object = object;
        aes_state.edit_index = *idx;
        return 1;
    }
    if (kind == EDEND) {
        if (aes_state.edit_tree == tree && aes_state.edit_object == object) {
            aes_state.edit_tree = NULL;
            aes_state.edit_object = NIL;
            aes_state.edit_index = 0;
        }
        return 1;
    }
    if (kind != EDCHAR || tree[object].ob_spec == 0) {
        return 1;
    }

    ted = (TEDINFO *)(intptr_t)tree[object].ob_spec;
    text = (char *)(intptr_t)ted->te_ptext;
    if (text == NULL) {
        return 0;
    }

    length = strlen(text);
    if (*idx < ted->te_txtlen - 1 && charidx >= 32 && charidx <= 126) {
        if ((size_t)*idx > length) {
            *idx = (WORD)length;
        }
        if (length < (size_t)(ted->te_txtlen - 1)) {
            memmove(&text[*idx + 1], &text[*idx], length - (size_t)*idx + 1u);
            text[*idx] = (char)charidx;
            ++(*idx);
        } else if ((size_t)*idx < length) {
            memmove(&text[*idx + 1], &text[*idx], length - (size_t)*idx);
            text[*idx] = (char)charidx;
            text[ted->te_txtlen - 1] = '\0';
            ++(*idx);
        } else {
            text[*idx] = (char)charidx;
            text[*idx + 1] = '\0';
            ++(*idx);
        }
    }
    aes_state.edit_tree = tree;
    aes_state.edit_object = object;
    aes_state.edit_index = *idx;
    return 1;
}
