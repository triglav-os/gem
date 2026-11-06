/*
 * Maps Linux evdev key codes to GEM keyboard codes: modifier bits, the
 * USB HID scan codes AES applications expect and the ASCII produced under
 * the active shift state.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (C) 2026 tomaz stih
 */

#include "keymap.h"

#include <linux/input-event-codes.h>

uint16_t modifier_for_key(uint16_t code)
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

uint8_t usb_scan_for_key(uint16_t code)
{
    if (code >= KEY_A && code <= KEY_Z) {
        return (uint8_t)(4u + code - KEY_A);
    }
    if (code >= KEY_1 && code <= KEY_9) {
        return (uint8_t)(30u + code - KEY_1);
    }

    switch (code) {
        case KEY_0:
            return 39u;
        case KEY_ENTER:
            return 40u;
        case KEY_ESC:
            return 41u;
        case KEY_BACKSPACE:
            return 42u;
        case KEY_TAB:
            return 43u;
        case KEY_SPACE:
            return 44u;
        case KEY_MINUS:
            return 45u;
        case KEY_EQUAL:
            return 46u;
        case KEY_LEFTBRACE:
            return 47u;
        case KEY_RIGHTBRACE:
            return 48u;
        case KEY_BACKSLASH:
            return 49u;
        case KEY_SEMICOLON:
            return 51u;
        case KEY_APOSTROPHE:
            return 52u;
        case KEY_GRAVE:
            return 53u;
        case KEY_COMMA:
            return 54u;
        case KEY_DOT:
            return 55u;
        case KEY_SLASH:
            return 56u;
        case KEY_CAPSLOCK:
            return 57u;
        case KEY_F1:
            return 58u;
        case KEY_F2:
            return 59u;
        case KEY_F3:
            return 60u;
        case KEY_F4:
            return 61u;
        case KEY_F5:
            return 62u;
        case KEY_F6:
            return 63u;
        case KEY_F7:
            return 64u;
        case KEY_F8:
            return 65u;
        case KEY_F9:
            return 66u;
        case KEY_F10:
            return 67u;
        case KEY_F11:
            return 68u;
        case KEY_F12:
            return 69u;
        case KEY_HOME:
            return 74u;
        case KEY_PAGEUP:
            return 75u;
        case KEY_DELETE:
            return 76u;
        case KEY_END:
            return 77u;
        case KEY_PAGEDOWN:
            return 78u;
        case KEY_RIGHT:
            return 79u;
        case KEY_LEFT:
            return 80u;
        case KEY_DOWN:
            return 81u;
        case KEY_UP:
            return 82u;
        case KEY_LEFTCTRL:
            return 224u;
        case KEY_LEFTSHIFT:
            return 225u;
        case KEY_LEFTALT:
            return 226u;
        case KEY_RIGHTCTRL:
            return 228u;
        case KEY_RIGHTSHIFT:
            return 229u;
        case KEY_RIGHTALT:
            return 230u;
        default:
            return (uint8_t)code;
    }
}

uint8_t ascii_for_key(uint16_t code, uint16_t modifiers, int caps_lock)
{
    int shifted = (modifiers & (gem_mod_lshift | gem_mod_rshift)) != 0u;
    int upper = shifted != caps_lock;
    char letter = 0;

    /*
     * Linux KEY_* letter codes are NOT A..Z contiguous (they follow the
     * physical QWERTY rows: Q..P, A..L, Z..M). Mapping with
     * 'a' + (code - KEY_A) turns L into 'i' and S into 'b'.
     */
    switch (code) {
        case KEY_A:
            letter = 'a';
            break;
        case KEY_B:
            letter = 'b';
            break;
        case KEY_C:
            letter = 'c';
            break;
        case KEY_D:
            letter = 'd';
            break;
        case KEY_E:
            letter = 'e';
            break;
        case KEY_F:
            letter = 'f';
            break;
        case KEY_G:
            letter = 'g';
            break;
        case KEY_H:
            letter = 'h';
            break;
        case KEY_I:
            letter = 'i';
            break;
        case KEY_J:
            letter = 'j';
            break;
        case KEY_K:
            letter = 'k';
            break;
        case KEY_L:
            letter = 'l';
            break;
        case KEY_M:
            letter = 'm';
            break;
        case KEY_N:
            letter = 'n';
            break;
        case KEY_O:
            letter = 'o';
            break;
        case KEY_P:
            letter = 'p';
            break;
        case KEY_Q:
            letter = 'q';
            break;
        case KEY_R:
            letter = 'r';
            break;
        case KEY_S:
            letter = 's';
            break;
        case KEY_T:
            letter = 't';
            break;
        case KEY_U:
            letter = 'u';
            break;
        case KEY_V:
            letter = 'v';
            break;
        case KEY_W:
            letter = 'w';
            break;
        case KEY_X:
            letter = 'x';
            break;
        case KEY_Y:
            letter = 'y';
            break;
        case KEY_Z:
            letter = 'z';
            break;
        default:
            break;
    }
    if (letter != 0) {
        if (upper) {
            letter = (char)(letter - 'a' + 'A');
        }
        return (uint8_t)letter;
    }

    if (code >= KEY_1 && code <= KEY_9) {
        static const char normal[] = "123456789";
        static const char shifted_digits[] = "!@#$%^&*(";

        return (uint8_t)(shifted ? shifted_digits[code - KEY_1]
                                 : normal[code - KEY_1]);
    }
    switch (code) {
        case KEY_0:
            return (uint8_t)(shifted ? ')' : '0');
        case KEY_ENTER:
        case KEY_KPENTER:
            return '\n';
        case KEY_ESC:
            return 27u;
        case KEY_BACKSPACE:
            return '\b';
        case KEY_TAB:
            return '\t';
        case KEY_SPACE:
            return ' ';
        case KEY_MINUS:
            return (uint8_t)(shifted ? '_' : '-');
        case KEY_EQUAL:
            return (uint8_t)(shifted ? '+' : '=');
        case KEY_LEFTBRACE:
            return (uint8_t)(shifted ? '{' : '[');
        case KEY_RIGHTBRACE:
            return (uint8_t)(shifted ? '}' : ']');
        case KEY_BACKSLASH:
            return (uint8_t)(shifted ? '|' : '\\');
        case KEY_SEMICOLON:
            return (uint8_t)(shifted ? ':' : ';');
        case KEY_APOSTROPHE:
            return (uint8_t)(shifted ? '"' : '\'');
        case KEY_GRAVE:
            return (uint8_t)(shifted ? '~' : '`');
        case KEY_COMMA:
            return (uint8_t)(shifted ? '<' : ',');
        case KEY_DOT:
            return (uint8_t)(shifted ? '>' : '.');
        case KEY_SLASH:
            return (uint8_t)(shifted ? '?' : '/');
        case KEY_KP0:
            return '0';
        case KEY_KP1:
            return '1';
        case KEY_KP2:
            return '2';
        case KEY_KP3:
            return '3';
        case KEY_KP4:
            return '4';
        case KEY_KP5:
            return '5';
        case KEY_KP6:
            return '6';
        case KEY_KP7:
            return '7';
        case KEY_KP8:
            return '8';
        case KEY_KP9:
            return '9';
        case KEY_KPDOT:
            return '.';
        case KEY_KPMINUS:
            return '-';
        case KEY_KPPLUS:
            return '+';
        case KEY_KPASTERISK:
            return '*';
        case KEY_KPSLASH:
            return '/';
        default:
            return 0u;
    }
}
