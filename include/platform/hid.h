/*
 * Declares the input abstraction used by GEM to read mouse, keyboard,
 * and quit events without depending on a specific window system or
 * device API.
 *
 * MIT License (see: LICENSE)
 * Copyright (C) 2026 tomaz stih
 */

#ifndef GEM_HID_H
#define GEM_HID_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum gem_hid_event_type {
    GEM_HID_NONE         = 0,
    GEM_HID_MOUSE_MOVE   = 1,
    GEM_HID_MOUSE_BUTTON = 2,
    GEM_HID_KEY          = 3,
    GEM_HID_QUIT         = 4
} gem_hid_event_type_t;

typedef enum gem_hid_button {
    GEM_HID_BUTTON_LEFT   = 1,
    GEM_HID_BUTTON_RIGHT  = 2,
    GEM_HID_BUTTON_MIDDLE = 4
} gem_hid_button_t;

typedef struct gem_hid_event {
    gem_hid_event_type_t type;
    uint16_t             flags;

    int16_t              x;
    int16_t              y;
    int16_t              dx;
    int16_t              dy;

    uint16_t             button;
    uint16_t             key;
    uint16_t             mod;
} gem_hid_event_t;

/*
 * Initializes the platform input backend.
 * Returns non-zero on success and zero on failure.
 */
int  gem_hid_init(void);

/*
 * Shuts down the platform input backend and releases its resources.
 */
void gem_hid_shutdown(void);

/*
 * Polls for the next pending input event.
 * Writes the event to `evt` when one is available.
 * Returns non-zero when an event was produced, or zero otherwise.
 */
int  gem_hid_poll(gem_hid_event_t *evt);

#ifdef __cplusplus
}
#endif

#endif
