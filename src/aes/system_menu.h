/*
 * Declares the always-present hosted GEM system menu: a three-bar menu
 * button fixed at the left of the menu bar, like the classic Apple menu.
 * It is gemd's own menu (not any application's), presents registered GEM
 * services above a separator, and keeps Shutdown as its final command.
 *
 * MIT License (see: LICENSE)
 * Copyright (C) 2026 tomaz stih
 */

#ifndef GEM_AES_SYSTEM_MENU_H
#define GEM_AES_SYSTEM_MENU_H

#include "aes_internal.h"

enum {
    AES_SYSTEM_MENU_NONE = 0,
    AES_SYSTEM_MENU_SHUTDOWN = 1,
    AES_SYSTEM_MENU_REGISTERED = 100
};

/* Register a copied service label owned by app_id; returns 0..5 or -1. */
WORD aes_system_menu_register(WORD app_id, const char *text);

/* Remove one registration when it belongs to app_id. */
WORD aes_system_menu_unregister(WORD app_id, WORD menu_id);

/* Remove every system-menu registration owned by app_id. */
void aes_system_menu_unregister_owner(WORD app_id);

/* Width in pixels reserved at the left of the bar for the system title. */
WORD aes_system_menu_width(void);

/* Paint the menu button into the reserved strip. Does nothing without a
 * ready workstation. Call it whenever the bar strip is repainted. */
void aes_system_menu_draw(void);

/* Nonzero when (x, y) is within the system title on the bar. */
int aes_system_menu_hit(WORD x, WORD y);

/* Track a press that hit the system title: open the popup, follow the
 * pointer to its release and act on the chosen item. "Shutdown" runs the
 * installed hook after a confirmation. Returns the chosen item, or
 * AES_SYSTEM_MENU_NONE. Poll HID directly, like the classic menu tracker. */
WORD aes_system_menu_track(const gem_hid_event_t *first_evt);

/* Hook gemd installs so "Shutdown" can stop the server. Returns nonzero
 * when the shutdown was accepted. */
extern int (*aes_system_shutdown_hook)(void);
/* Optional server-side shortcut hook for a physical Delete key. */
extern int (*aes_system_delete_hook)(void);

#endif
