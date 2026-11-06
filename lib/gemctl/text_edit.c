/*
 * Implements bounded single-line editing, caret movement and selection.
 * Byte positions match GEM's printable ASCII keyboard interface.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (C) 2026 tomaz stih
 */

#include <gem/gemctl.h>
#include <string.h>

void gemctl_text_edit_init(gemctl_text_edit_t *control, char *text,
                           size_t capacity)
{
    if (control == NULL)
        return;
    memset(control, 0, sizeof(*control));
    control->text = text;
    control->capacity = capacity;
    if (text != NULL && capacity > 0u)
        text[capacity - 1u] = '\0';
}

void gemctl_text_edit_focus(gemctl_text_edit_t *control, int replace)
{
    if (control == NULL || control->text == NULL || control->capacity == 0u)
        return;
    control->active = 1;
    control->cursor = strlen(control->text);
    control->anchor = replace ? 0u : control->cursor;
    control->replace_pending = replace != 0;
}

void gemctl_text_edit_blur(gemctl_text_edit_t *control)
{
    if (control == NULL)
        return;
    control->active = 0;
    control->replace_pending = 0;
    control->anchor = control->cursor;
}

int gemctl_text_edit_active(const gemctl_text_edit_t *control)
{
    return control != NULL && control->active != 0;
}

void gemctl_text_edit_position(gemctl_text_edit_t *control, size_t position,
                               int extend)
{
    size_t length;
    if (control == NULL || control->text == NULL)
        return;
    length = strlen(control->text);
    control->cursor = position < length ? position : length;
    if (!extend)
        control->anchor = control->cursor;
    control->replace_pending = control->anchor != control->cursor;
    control->active = 1;
}

gemctl_text_edit_result_t gemctl_text_edit_insert(gemctl_text_edit_t *control,
                                                  const char *text)
{
    size_t length, count, first, last;
    if (!gemctl_text_edit_active(control) || text == NULL)
        return GEMCTL_TEXT_EDIT_IGNORED;
    length = strlen(control->text);
    count = strlen(text);
    if (control->cursor > length)
        control->cursor = length;
    if (control->anchor > length)
        control->anchor = length;
    first =
        control->cursor < control->anchor ? control->cursor : control->anchor;
    last =
        control->cursor > control->anchor ? control->cursor : control->anchor;
    if (count >= control->capacity ||
        length - (last - first) >= control->capacity - count)
        return GEMCTL_TEXT_EDIT_IGNORED;
    for (size_t i = 0; i < count; ++i)
        if ((unsigned char)text[i] < 32u || (unsigned char)text[i] >= 127u)
            return GEMCTL_TEXT_EDIT_IGNORED;
    memmove(control->text + first + count, control->text + last,
            length - last + 1);
    memcpy(control->text + first, text, count);
    gemctl_text_edit_position(control, first + count, 0);
    return GEMCTL_TEXT_EDIT_CHANGED;
}

gemctl_text_edit_result_t gemctl_text_edit_key_mod(gemctl_text_edit_t *control,
                                                   uint16_t key,
                                                   uint16_t modifiers)
{
    unsigned int ascii = key & 0xffu;
    unsigned int scan = key >> 8;
    size_t length, first, last, target;
    int extend = (modifiers & (K_LSHIFT | K_RSHIFT)) != 0;
    int ctrl = (modifiers & K_CTRL) != 0;
    if (!gemctl_text_edit_active(control))
        return GEMCTL_TEXT_EDIT_IGNORED;
    length = strlen(control->text);
    if (control->cursor > length)
        control->cursor = length;
    if (control->anchor > length)
        control->anchor = length;
    first =
        control->cursor < control->anchor ? control->cursor : control->anchor;
    last =
        control->cursor > control->anchor ? control->cursor : control->anchor;
    if (ascii == 27u)
        return GEMCTL_TEXT_EDIT_CANCEL;
    if (ascii == '\r' || ascii == '\n' ||
        (ascii == 0u && (scan == 28u || scan == 40u)))
        return GEMCTL_TEXT_EDIT_ACCEPT;
    if (ctrl && (scan == 4u || ascii == 1u || ascii == 'a' || ascii == 'A')) {
        gemctl_text_edit_focus(control, 1);
        return GEMCTL_TEXT_EDIT_CHANGED;
    }
    if (ascii == 0u &&
        (scan == 74u || scan == 77u || scan == 79u || scan == 80u)) {
        target = control->cursor;
        if (scan == 74u)
            target = 0;
        if (scan == 77u)
            target = length;
        if (scan == 80u) {
            if (!extend && first != last)
                target = first;
            else if (target > 0) {
                --target;
                if (ctrl)
                    while (target > 0 && control->text[target - 1u] != '/')
                        --target;
            }
        }
        if (scan == 79u) {
            if (!extend && first != last)
                target = last;
            else if (target < length) {
                ++target;
                if (ctrl)
                    while (target < length && control->text[target] != '/')
                        ++target;
            }
        }
        gemctl_text_edit_position(control, target, extend);
        return GEMCTL_TEXT_EDIT_CHANGED;
    }
    if (ascii == '\b' || (ascii == 0u && (scan == 14u || scan == 42u)) ||
        scan == 76u) {
        if (first == last) {
            if (scan == 76u && last < length)
                ++last;
            else if (scan != 76u && first > 0)
                --first;
        }
        memmove(control->text + first, control->text + last,
                length - last + 1u);
        gemctl_text_edit_position(control, first, 0);
        return GEMCTL_TEXT_EDIT_CHANGED;
    }
    if (ctrl || (modifiers & K_ALT) || ascii < 32u || ascii >= 127u)
        return GEMCTL_TEXT_EDIT_IGNORED;
    if (length - (last - first) + 1u >= control->capacity)
        return GEMCTL_TEXT_EDIT_IGNORED;
    memmove(control->text + first + 1u, control->text + last,
            length - last + 1u);
    control->text[first] = (char)ascii;
    gemctl_text_edit_position(control, first + 1u, 0);
    return GEMCTL_TEXT_EDIT_CHANGED;
}

gemctl_text_edit_result_t gemctl_text_edit_key(gemctl_text_edit_t *control,
                                               uint16_t key)
{
    return gemctl_text_edit_key_mod(control, key, 0);
}
