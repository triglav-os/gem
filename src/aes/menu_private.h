/*
 * Declares the private helpers shared by the hosted AES menu model, layout
 * and tracking modules. These are libaes implementation details, not public
 * GEM interfaces.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (C) 2026 tomaz stih
 */

#ifndef GEM_AES_MENU_PRIVATE_H
#define GEM_AES_MENU_PRIVATE_H

#include "aes_internal.h"

/* Object holding the popup boxes (ROOT when popups hang directly). */
WORD aes_menu_popup_container(OBJECT *tree);
/* First popup box under the container, or NIL. */
WORD aes_menu_first_popup_child(OBJECT *tree, WORD popup_parent);
/* Highest object index reachable from ROOT. */
WORD aes_menu_last_object(OBJECT *tree);
/* Object whose children are the G_TITLE entries, or NIL. */
WORD aes_menu_title_container(OBJECT *tree);
/* Popup box paired with a title by position, or NIL. */
WORD aes_menu_popup_for_title(OBJECT *tree, WORD title);

#endif
