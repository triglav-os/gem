/*
 * Implements native Linux keyboard and pointer input for GEM using evdev.
 * Devices may be discovered automatically or supplied explicitly, and all
 * descriptors are non-blocking so AES remains responsive without threads.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (C) 2026 tomaz stih
 */

#define _GNU_SOURCE

#include "keymap.h"

#include "platform/hid.h"
#include "platform/raster.h"

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <linux/input.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

enum {
    linux_hid_max_devices = 32,
    linux_bits_per_long = (int)(sizeof(unsigned long) * 8u)
};

typedef struct linux_hid_device {
    int fd;
    int has_keyboard;
    int has_pointer;
    struct input_absinfo abs_x;
    struct input_absinfo abs_y;
    int has_abs_x;
    int has_abs_y;
} linux_hid_device_t;

static linux_hid_device_t g_devices[linux_hid_max_devices];
static size_t g_device_count;
static size_t g_next_device;
static int16_t g_mouse_x;
static int16_t g_mouse_y;
static uint16_t g_buttons;
static uint16_t g_modifiers;
static int g_caps_lock;
static int g_rel_scale = 3;
static int g_have_abs_pointer;

static int bit_is_set(const unsigned long *bits, unsigned int bit)
{
    return (bits[bit / (unsigned int)linux_bits_per_long] &
            (1ul << (bit % (unsigned int)linux_bits_per_long))) != 0ul;
}

static int option_enabled(const char *name)
{
    const char *value = getenv(name);

    return value != NULL &&
           (strcmp(value, "1") == 0 || strcmp(value, "on") == 0 ||
            strcmp(value, "true") == 0 || strcmp(value, "yes") == 0);
}

static int parse_positive_env(const char *name, int fallback)
{
    const char *value = getenv(name);
    char *end = NULL;
    long parsed;

    if (value == NULL || value[0] == '\0') {
        return fallback;
    }
    parsed = strtol(value, &end, 10);
    if (end == value || *end != '\0' || parsed < 1L || parsed > 32L) {
        return fallback;
    }
    return (int)parsed;
}

static void close_devices(void)
{
    size_t index;

    for (index = 0u; index < g_device_count; ++index) {
        if (option_enabled("GEM_LINUX_GRAB")) {
            (void)ioctl(g_devices[index].fd, EVIOCGRAB, 0);
        }
        (void)close(g_devices[index].fd);
    }
    memset(g_devices, 0, sizeof(g_devices));
    g_device_count = 0u;
}

static int add_device(const char *path)
{
    unsigned long
        event_bits[(EV_MAX + linux_bits_per_long) / linux_bits_per_long];
    unsigned long
        key_bits[(KEY_MAX + linux_bits_per_long) / linux_bits_per_long];
    linux_hid_device_t *device;
    int fd;

    if (g_device_count >= linux_hid_max_devices) {
        return 0;
    }
    fd = open(path, O_RDONLY | O_NONBLOCK | O_CLOEXEC);
    if (fd < 0) {
        return 0;
    }

    memset(event_bits, 0, sizeof(event_bits));
    memset(key_bits, 0, sizeof(key_bits));
    if (ioctl(fd, EVIOCGBIT(0, sizeof(event_bits)), event_bits) < 0) {
        (void)close(fd);
        return 0;
    }
    if (bit_is_set(event_bits, EV_KEY)) {
        (void)ioctl(fd, EVIOCGBIT(EV_KEY, sizeof(key_bits)), key_bits);
    }

    device = &g_devices[g_device_count];
    memset(device, 0, sizeof(*device));
    device->fd = fd;
    device->has_keyboard = bit_is_set(event_bits, EV_KEY) &&
                           bit_is_set(key_bits, KEY_A) &&
                           bit_is_set(key_bits, KEY_ENTER);
    device->has_pointer =
        (bit_is_set(event_bits, EV_REL) || bit_is_set(event_bits, EV_ABS)) &&
        bit_is_set(event_bits, EV_KEY) &&
        (bit_is_set(key_bits, BTN_LEFT) || bit_is_set(key_bits, BTN_TOUCH) ||
         bit_is_set(key_bits, BTN_MOUSE));
    if (!device->has_keyboard && !device->has_pointer) {
        (void)close(fd);
        return 0;
    }

    if (bit_is_set(event_bits, EV_ABS)) {
        device->has_abs_x = ioctl(fd, EVIOCGABS(ABS_X), &device->abs_x) == 0;
        device->has_abs_y = ioctl(fd, EVIOCGABS(ABS_Y), &device->abs_y) == 0;
    }

    /*
     * Prefer absolute pointers (USB tablet, touchscreen). Relative PS/2
     * mice feel glacial on large framebuffers without acceleration, and
     * fighting a tablet makes the cursor stutter.
     */
    if (device->has_pointer && !device->has_abs_x && !device->has_abs_y &&
        g_have_abs_pointer && !option_enabled("GEM_LINUX_KEEP_REL_MOUSE")) {
        (void)close(fd);
        memset(device, 0, sizeof(*device));
        return 0;
    }
    if (option_enabled("GEM_LINUX_GRAB") && ioctl(fd, EVIOCGRAB, 1) < 0) {
        (void)close(fd);
        memset(device, 0, sizeof(*device));
        return 0;
    }
    if (device->has_pointer && (device->has_abs_x || device->has_abs_y)) {
        g_have_abs_pointer = 1;
    }
    ++g_device_count;
    return 1;
}

static void open_explicit_devices(const char *paths)
{
    char *copy = strdup(paths);
    char *save = NULL;
    char *path;

    if (copy == NULL) {
        return;
    }
    for (path = strtok_r(copy, ",:", &save); path != NULL;
         path = strtok_r(NULL, ",:", &save)) {
        if (path[0] != '\0') {
            (void)add_device(path);
        }
    }
    free(copy);
}

static void discover_devices(void)
{
    DIR *directory = opendir("/dev/input");
    struct dirent *entry;

    if (directory == NULL) {
        return;
    }
    while ((entry = readdir(directory)) != NULL) {
        char path[512];

        if (strncmp(entry->d_name, "event", 5u) != 0) {
            continue;
        }
        if (snprintf(path, sizeof(path), "/dev/input/%s", entry->d_name) <
            (int)sizeof(path)) {
            (void)add_device(path);
        }
    }
    (void)closedir(directory);
}

static int translate_key(gem_hid_event_t *event,
                         const struct input_event *input)
{
    uint16_t modifier;
    int pressed;

    if (input->code >= BTN_MISC) {
        return 0;
    }
    pressed = input->value != 0;
    modifier = modifier_for_key(input->code);
    if (modifier != 0u) {
        if (pressed) {
            g_modifiers = (uint16_t)(g_modifiers | modifier);
        } else {
            g_modifiers = (uint16_t)(g_modifiers & (uint16_t)~modifier);
        }
    }
    if (input->code == KEY_CAPSLOCK && input->value == 1) {
        g_caps_lock = !g_caps_lock;
    }

    memset(event, 0, sizeof(*event));
    event->type = GEM_HID_KEY;
    event->flags = (uint16_t)(pressed ? 1u : 0u);
    event->x = g_mouse_x;
    event->y = g_mouse_y;
    event->key = (uint16_t)((uint16_t)usb_scan_for_key(input->code) << 8);
    event->key = (uint16_t)(event->key | ascii_for_key(input->code, g_modifiers,
                                                       g_caps_lock));
    event->mod = g_modifiers;
    return 1;
}

static uint16_t button_for_code(uint16_t code)
{
    switch (code) {
        case BTN_LEFT:
        case BTN_TOUCH:
            return GEM_HID_BUTTON_LEFT;
        case BTN_RIGHT:
            return GEM_HID_BUTTON_RIGHT;
        case BTN_MIDDLE:
            return GEM_HID_BUTTON_MIDDLE;
        default:
            return 0u;
    }
}

static int translate_button(gem_hid_event_t *event,
                            const struct input_event *input)
{
    uint16_t button = button_for_code(input->code);

    if (button == 0u) {
        return 0;
    }
    if (input->value != 0) {
        g_buttons = (uint16_t)(g_buttons | button);
    } else {
        g_buttons = (uint16_t)(g_buttons & (uint16_t)~button);
    }
    memset(event, 0, sizeof(*event));
    event->type = GEM_HID_MOUSE_BUTTON;
    event->flags = g_buttons;
    event->button = button;
    event->x = g_mouse_x;
    event->y = g_mouse_y;
    return 1;
}

static int16_t clamp_coordinate(int value, int maximum)
{
    if (value < 0) {
        return 0;
    }
    if (value > maximum) {
        return (int16_t)maximum;
    }
    return (int16_t)value;
}

static int translate_pointer(linux_hid_device_t *device, gem_hid_event_t *event,
                             const struct input_event *input)
{
    gem_raster_surface_t *surface = gem_raster_surface();
    int old_x = g_mouse_x;
    int old_y = g_mouse_y;
    int max_x;
    int max_y;

    if (surface == NULL) {
        return 0;
    }
    max_x = surface->width - 1;
    max_y = surface->height - 1;

    if (input->type == EV_REL && input->code == REL_X) {
        g_mouse_x =
            clamp_coordinate(g_mouse_x + input->value * g_rel_scale, max_x);
    } else if (input->type == EV_REL && input->code == REL_Y) {
        g_mouse_y =
            clamp_coordinate(g_mouse_y + input->value * g_rel_scale, max_y);
    } else if (input->type == EV_ABS && input->code == ABS_X &&
               device->has_abs_x &&
               device->abs_x.maximum != device->abs_x.minimum) {
        g_mouse_x =
            (int16_t)(((int64_t)input->value - device->abs_x.minimum) * max_x /
                      (device->abs_x.maximum - device->abs_x.minimum));
    } else if (input->type == EV_ABS && input->code == ABS_Y &&
               device->has_abs_y &&
               device->abs_y.maximum != device->abs_y.minimum) {
        g_mouse_y =
            (int16_t)(((int64_t)input->value - device->abs_y.minimum) * max_y /
                      (device->abs_y.maximum - device->abs_y.minimum));
    } else {
        return 0;
    }

    memset(event, 0, sizeof(*event));
    event->type = GEM_HID_MOUSE_MOVE;
    event->flags = g_buttons;
    event->x = g_mouse_x;
    event->y = g_mouse_y;
    event->dx = (int16_t)(g_mouse_x - old_x);
    event->dy = (int16_t)(g_mouse_y - old_y);
    return 1;
}

static int translate_event(linux_hid_device_t *device, gem_hid_event_t *event,
                           const struct input_event *input)
{
    if (input->type == EV_KEY && device->has_keyboard &&
        input->code < BTN_MISC) {
        return translate_key(event, input);
    }
    if (input->type == EV_KEY && device->has_pointer) {
        return translate_button(event, input);
    }
    if ((input->type == EV_REL || input->type == EV_ABS) &&
        device->has_pointer) {
        return translate_pointer(device, event, input);
    }
    return 0;
}

int gem_hid_init(void)
{
    const char *paths = getenv("GEM_LINUX_INPUT");
    gem_raster_surface_t *surface = gem_raster_surface();

    close_devices();
    g_have_abs_pointer = 0;
    /* Relative mice need a large scale on 1280x+ desktops; tablets ignore it.
     */
    g_rel_scale = parse_positive_env("GEM_LINUX_MOUSE_SCALE", 8);

    /*
     * Two-pass discovery: absolute pointers first (tablets), then the rest.
     * That way relative mice can be skipped when a tablet is present.
     */
    if (paths != NULL && paths[0] != '\0' && strcmp(paths, "auto") != 0) {
        open_explicit_devices(paths);
    } else {
        discover_devices();
        /* Second pass not needed if order was lucky; re-scan if empty. */
        if (g_device_count == 0u) {
            discover_devices();
        }
    }

    /* If we only opened relative mice first, drop them and prefer abs. */
    if (!g_have_abs_pointer && g_device_count != 0u) {
        /* keep relative devices; scale applied via g_rel_scale */
    } else if (g_have_abs_pointer) {
        size_t index;
        size_t out = 0u;

        for (index = 0u; index < g_device_count; ++index) {
            linux_hid_device_t *device = &g_devices[index];
            int is_rel_only_pointer =
                device->has_pointer && !device->has_abs_x &&
                !device->has_abs_y && !device->has_keyboard;

            if (is_rel_only_pointer &&
                !option_enabled("GEM_LINUX_KEEP_REL_MOUSE")) {
                if (option_enabled("GEM_LINUX_GRAB")) {
                    (void)ioctl(device->fd, EVIOCGRAB, 0);
                }
                (void)close(device->fd);
                continue;
            }
            if (out != index) {
                g_devices[out] = *device;
            }
            ++out;
        }
        g_device_count = out;
    }

    if (surface != NULL) {
        g_mouse_x = (int16_t)(surface->width / 2u);
        g_mouse_y = (int16_t)(surface->height / 2u);
    }
    g_buttons = 0u;
    g_modifiers = 0u;
    g_caps_lock = 0;
    g_next_device = 0u;
    return g_device_count != 0u;
}

void gem_hid_shutdown(void)
{
    close_devices();
    g_next_device = 0u;
    g_mouse_x = 0;
    g_mouse_y = 0;
    g_buttons = 0u;
    g_modifiers = 0u;
    g_caps_lock = 0;
}

int gem_hid_poll(gem_hid_event_t *event)
{
    size_t checked;
    gem_hid_event_t latest_move;
    int have_move = 0;

    if (event == NULL || g_device_count == 0u) {
        return 0;
    }

    /*
     * Drain every device. Return keys/buttons immediately; coalesce all
     * motion into a single latest-position event so the UI is not flooded
     * with tiny steps (each of which used to full-screen blit).
     */
    for (checked = 0u; checked < g_device_count; ++checked) {
        linux_hid_device_t *device = &g_devices[g_next_device];
        struct input_event input;
        ssize_t count;

        g_next_device = (g_next_device + 1u) % g_device_count;
        for (;;) {
            gem_hid_event_t translated;

            count = read(device->fd, &input, sizeof(input));
            if (count != (ssize_t)sizeof(input)) {
                break;
            }
            if (!translate_event(device, &translated, &input)) {
                continue;
            }
            if (translated.type == GEM_HID_MOUSE_MOVE) {
                latest_move = translated;
                have_move = 1;
                continue;
            }
            *event = translated;
            return 1;
        }
    }
    if (have_move) {
        *event = latest_move;
        return 1;
    }
    return 0;
}
