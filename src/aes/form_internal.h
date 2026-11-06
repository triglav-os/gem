/*
 * Declares the private helpers shared between the hosted AES form loop and
 * its editable-field implementation. These are libaes implementation
 * details, not public GEM interfaces.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (C) 2026 tomaz stih
 */

#ifndef GEM_AES_FORM_INTERNAL_H
#define GEM_AES_FORM_INTERNAL_H

#include "aes_internal.h"

/* Index of the last object reachable from ROOT. */
WORD aes_form_last_object(OBJECT *tree);
/* Absolute screen rectangle of an object. */
void aes_form_object_rect(OBJECT *tree, WORD object, GRECT *rect);
/* Redraw one object of a form within its own bounds. */
void aes_form_redraw_object(OBJECT *tree, WORD object);
/* Nonzero when an object is an editable text field. */
int aes_form_is_editable(const OBJECT *tree, WORD object);
/* Next (or previous) editable field after current, or NIL. */
WORD aes_form_find_next_editable(OBJECT *tree, WORD current, int backwards);
/* Make a field the active edit target; idx receives its caret. */
void aes_form_set_active_field(OBJECT *tree, WORD object, WORD *idx);
/* Place the caret of a field from a mouse x coordinate. */
void aes_form_set_caret_from_click(OBJECT *tree, WORD object, WORD mouse_x,
                                   WORD *idx);
/* Apply a key to the active field; nonzero when the text changed. */
int aes_form_apply_key(OBJECT *tree, WORD object, WORD key, WORD *idx);

#endif
