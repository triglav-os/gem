/*
 * Implements native Linux keyboard and pointer input for GEM using evdev.
 * Devices may be discovered automatically or supplied explicitly, and all
 * descriptors are non-blocking so AES remains responsive without threads.
 *
 * MIT License (see: LICENSE)
 * Copyright (C) 2026 tomaz stih
 */

#define _GNU_SOURCE

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
    linux_bits_per_long = (int) (sizeof(unsigned long) * 8u),
    gem_mod_rshift = 0x0001,
    gem_mod_lshift = 0x0002,
    gem_mod_ctrl = 0x0004,
    gem_mod_alt = 0x0008
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

static int bit_is_set(const unsigned long *bits, unsigned int bit)
{
    return (bits[bit / (unsigned int) linux_bits_per_long] &
        (1ul << (bit % (unsigned int) linux_bits_per_long))) != 0ul;
}

static int option_enabled(const char *name)
{
    const char *value = getenv(name);

    return value != NULL &&
        (strcmp(value, "1") == 0 || strcmp(value, "on") == 0 ||
        strcmp(value, "true") == 0 || strcmp(value, "yes") == 0);
}

static void close_devices(void)
{
    size_t index;

    for (index = 0u; index < g_device_count; ++index) {
        if (option_enabled("GEM_LINUX_GRAB")) {
            (void) ioctl(g_devices[index].fd, EVIOCGRAB, 0);
        }
        (void) close(g_devices[index].fd);
    }
    memset(g_devices, 0, sizeof(g_devices));
    g_device_count = 0u;
}

static int add_device(const char *path)
{
    unsigned long event_bits[(EV_MAX + linux_bits_per_long) /
        linux_bits_per_long];
    unsigned long key_bits[(KEY_MAX + linux_bits_per_long) /
        linux_bits_per_long];
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
        (void) close(fd);
        return 0;
    }
    if (bit_is_set(event_bits, EV_KEY)) {
        (void) ioctl(fd, EVIOCGBIT(EV_KEY, sizeof(key_bits)), key_bits);
    }

    device = &g_devices[g_device_count];
    memset(device, 0, sizeof(*device));
    device->fd = fd;
    device->has_keyboard = bit_is_set(event_bits, EV_KEY) &&
        bit_is_set(key_bits, KEY_A) && bit_is_set(key_bits, KEY_ENTER);
    device->has_pointer =
        (bit_is_set(event_bits, EV_REL) ||
        bit_is_set(event_bits, EV_ABS)) &&
        bit_is_set(event_bits, EV_KEY) &&
        (bit_is_set(key_bits, BTN_LEFT) ||
        bit_is_set(key_bits, BTN_TOUCH));
    if (!device->has_keyboard && !device->has_pointer) {
        (void) close(fd);
        return 0;
    }

    if (bit_is_set(event_bits, EV_ABS)) {
        device->has_abs_x =
            ioctl(fd, EVIOCGABS(ABS_X), &device->abs_x) == 0;
        device->has_abs_y =
            ioctl(fd, EVIOCGABS(ABS_Y), &device->abs_y) == 0;
    }
    if (option_enabled("GEM_LINUX_GRAB") &&
        ioctl(fd, EVIOCGRAB, 1) < 0) {
        (void) close(fd);
        memset(device, 0, sizeof(*device));
        return 0;
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
            (void) add_device(path);
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
        if (snprintf(path, sizeof(path), "/dev/input/%s",
            entry->d_name) < (int) sizeof(path)) {
            (void) add_device(path);
        }
    }
    (void) closedir(directory);
}

static uint16_t modifier_for_key(uint16_t code)
{
    switch (code) {
    case KEY_LEFTSHIFT:
        return gem_mod_lshift;
    case KEY_RIGHTSHIFT:
        return gem_mod_rshift;
    case KEY_LEFTCTRL:
    case KEY_RIGHTCTRL:
        return gem_mod_ctrl;
    case KEY_LEFTALT:
    case KEY_RIGHTALT:
        return gem_mod_alt;
    default:
        return 0u;
    }
}

static uint8_t usb_scan_for_key(uint16_t code)
{
    if (code >= KEY_A && code <= KEY_Z) {
        return (uint8_t) (4u + code - KEY_A);
    }
    if (code >= KEY_1 && code <= KEY_9) {
        return (uint8_t) (30u + code - KEY_1);
    }

    switch (code) {
    case KEY_0: return 39u;
    case KEY_ENTER: return 40u;
    case KEY_ESC: return 41u;
    case KEY_BACKSPACE: return 42u;
    case KEY_TAB: return 43u;
    case KEY_SPACE: return 44u;
    case KEY_MINUS: return 45u;
    case KEY_EQUAL: return 46u;
    case KEY_LEFTBRACE: return 47u;
    case KEY_RIGHTBRACE: return 48u;
    case KEY_BACKSLASH: return 49u;
    case KEY_SEMICOLON: return 51u;
    case KEY_APOSTROPHE: return 52u;
    case KEY_GRAVE: return 53u;
    case KEY_COMMA: return 54u;
    case KEY_DOT: return 55u;
    case KEY_SLASH: return 56u;
    case KEY_CAPSLOCK: return 57u;
    case KEY_F1: return 58u;
    case KEY_F2: return 59u;
    case KEY_F3: return 60u;
    case KEY_F4: return 61u;
    case KEY_F5: return 62u;
    case KEY_F6: return 63u;
    case KEY_F7: return 64u;
    case KEY_F8: return 65u;
    case KEY_F9: return 66u;
    case KEY_F10: return 67u;
    case KEY_F11: return 68u;
    case KEY_F12: return 69u;
    case KEY_HOME: return 74u;
    case KEY_PAGEUP: return 75u;
    case KEY_DELETE: return 76u;
    case KEY_END: return 77u;
    case KEY_PAGEDOWN: return 78u;
    case KEY_RIGHT: return 79u;
    case KEY_LEFT: return 80u;
    case KEY_DOWN: return 81u;
    case KEY_UP: return 82u;
    case KEY_LEFTCTRL: return 224u;
    case KEY_LEFTSHIFT: return 225u;
    case KEY_LEFTALT: return 226u;
    case KEY_RIGHTCTRL: return 228u;
    case KEY_RIGHTSHIFT: return 229u;
    case KEY_RIGHTALT: return 230u;
    default: return (uint8_t) code;
    }
}

static uint8_t ascii_for_key(uint16_t code)
{
    int shifted = (g_modifiers & (gem_mod_lshift | gem_mod_rshift)) != 0u;

    if (code >= KEY_A && code <= KEY_Z) {
        int upper = shifted != g_caps_lock;

        return (uint8_t) ((upper ? 'A' : 'a') + code - KEY_A);
    }
    if (code >= KEY_1 && code <= KEY_9) {
        static const char normal[] = "123456789";
        static const char shifted_digits[] = "!@#$%^&*(";

        return (uint8_t) (shifted ?
            shifted_digits[code - KEY_1] : normal[code - KEY_1]);
    }
    switch (code) {
    case KEY_0: return (uint8_t) (shifted ? ')' : '0');
    case KEY_ENTER:
    case KEY_KPENTER: return '\n';
    case KEY_ESC: return 27u;
    case KEY_BACKSPACE: return '\b';
    case KEY_TAB: return '\t';
    case KEY_SPACE: return ' ';
    case KEY_MINUS: return (uint8_t) (shifted ? '_' : '-');
    case KEY_EQUAL: return (uint8_t) (shifted ? '+' : '=');
    case KEY_LEFTBRACE: return (uint8_t) (shifted ? '{' : '[');
    case KEY_RIGHTBRACE: return (uint8_t) (shifted ? '}' : ']');
    case KEY_BACKSLASH: return (uint8_t) (shifted ? '|' : '\\');
    case KEY_SEMICOLON: return (uint8_t) (shifted ? ':' : ';');
    case KEY_APOSTROPHE: return (uint8_t) (shifted ? '"' : '\'');
    case KEY_GRAVE: return (uint8_t) (shifted ? '~' : '`');
    case KEY_COMMA: return (uint8_t) (shifted ? '<' : ',');
    case KEY_DOT: return (uint8_t) (shifted ? '>' : '.');
    case KEY_SLASH: return (uint8_t) (shifted ? '?' : '/');
    case KEY_KP0: return '0';
    case KEY_KP1: return '1';
    case KEY_KP2: return '2';
    case KEY_KP3: return '3';
    case KEY_KP4: return '4';
    case KEY_KP5: return '5';
    case KEY_KP6: return '6';
    case KEY_KP7: return '7';
    case KEY_KP8: return '8';
    case KEY_KP9: return '9';
    case KEY_KPDOT: return '.';
    case KEY_KPMINUS: return '-';
    case KEY_KPPLUS: return '+';
    case KEY_KPASTERISK: return '*';
    case KEY_KPSLASH: return '/';
    default: return 0u;
    }
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
            g_modifiers = (uint16_t) (g_modifiers | modifier);
        } else {
            g_modifiers = (uint16_t) (g_modifiers & (uint16_t) ~modifier);
        }
    }
    if (input->code == KEY_CAPSLOCK && input->value == 1) {
        g_caps_lock = !g_caps_lock;
    }

    memset(event, 0, sizeof(*event));
    event->type = GEM_HID_KEY;
    event->flags = (uint16_t) (pressed ? 1u : 0u);
    event->x = g_mouse_x;
    event->y = g_mouse_y;
    event->key = (uint16_t) ((uint16_t) usb_scan_for_key(input->code) << 8);
    event->key = (uint16_t) (event->key | ascii_for_key(input->code));
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
        g_buttons = (uint16_t) (g_buttons | button);
    } else {
        g_buttons = (uint16_t) (g_buttons & (uint16_t) ~button);
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
        return (int16_t) maximum;
    }
    return (int16_t) value;
}

static int translate_pointer(linux_hid_device_t *device,
    gem_hid_event_t *event, const struct input_event *input)
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
        g_mouse_x = clamp_coordinate(g_mouse_x + input->value, max_x);
    } else if (input->type == EV_REL && input->code == REL_Y) {
        g_mouse_y = clamp_coordinate(g_mouse_y + input->value, max_y);
    } else if (input->type == EV_ABS && input->code == ABS_X &&
        device->has_abs_x &&
        device->abs_x.maximum != device->abs_x.minimum) {
        g_mouse_x = (int16_t) (((int64_t) input->value -
            device->abs_x.minimum) * max_x /
            (device->abs_x.maximum - device->abs_x.minimum));
    } else if (input->type == EV_ABS && input->code == ABS_Y &&
        device->has_abs_y &&
        device->abs_y.maximum != device->abs_y.minimum) {
        g_mouse_y = (int16_t) (((int64_t) input->value -
            device->abs_y.minimum) * max_y /
            (device->abs_y.maximum - device->abs_y.minimum));
    } else {
        return 0;
    }

    memset(event, 0, sizeof(*event));
    event->type = GEM_HID_MOUSE_MOVE;
    event->flags = g_buttons;
    event->x = g_mouse_x;
    event->y = g_mouse_y;
    event->dx = (int16_t) (g_mouse_x - old_x);
    event->dy = (int16_t) (g_mouse_y - old_y);
    return 1;
}

static int translate_event(linux_hid_device_t *device,
    gem_hid_event_t *event, const struct input_event *input)
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
    if (paths != NULL && paths[0] != '\0' &&
        strcmp(paths, "auto") != 0) {
        open_explicit_devices(paths);
    } else {
        discover_devices();
    }
    if (surface != NULL) {
        g_mouse_x = (int16_t) (surface->width / 2u);
        g_mouse_y = (int16_t) (surface->height / 2u);
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

    if (event == NULL || g_device_count == 0u) {
        return 0;
    }
    for (checked = 0u; checked < g_device_count; ++checked) {
        linux_hid_device_t *device = &g_devices[g_next_device];
        struct input_event input;
        ssize_t count;

        g_next_device = (g_next_device + 1u) % g_device_count;
        for (;;) {
            count = read(device->fd, &input, sizeof(input));
            if (count != (ssize_t) sizeof(input)) {
                break;
            }
            if (translate_event(device, event, &input)) {
                return 1;
            }
        }
    }
    return 0;
}
