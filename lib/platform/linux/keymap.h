/*
 * Declares the evdev-to-GEM key translation shared by the Linux input
 * backend: modifier bits in the GEM layout, and the scan code and ASCII
 * lookups for a key code under a given modifier state.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (C) 2026 tomaz stih
 */

#ifndef GEM_PLATFORM_LINUX_KEYMAP_H
#define GEM_PLATFORM_LINUX_KEYMAP_H

#include <stdint.h>

enum {
    gem_mod_rshift = 0x0001,
    gem_mod_lshift = 0x0002,
    gem_mod_ctrl = 0x0004,
    gem_mod_alt = 0x0008
};

/* GEM modifier bit for a modifier key code, or 0 for other keys. */
uint16_t modifier_for_key(uint16_t code);
/* USB HID scan code for an evdev key code, or 0 when unmapped. */
uint8_t usb_scan_for_key(uint16_t code);
/* ASCII for an evdev key code under the given modifiers and caps lock. */
uint8_t ascii_for_key(uint16_t code, uint16_t modifiers, int caps_lock);

#endif
