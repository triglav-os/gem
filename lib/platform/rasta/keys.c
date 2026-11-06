/*
 * Translates Rasta's USB HID usage codes into GEM keyboard codes: modifier
 * tracking and the scan/ASCII composition that AES applications receive.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (C) 2026 tomaz stih
 */

#include "rasta_internal.h"

uint16_t rasta_modifier_mask(uint16_t key)
{
    switch (key) {
        case 224u:
        case 228u:
            return rasta_mod_ctrl;
        case 225u:
            return rasta_mod_lshift;
        case 229u:
            return rasta_mod_rshift;
        case 226u:
        case 230u:
            return rasta_mod_alt;
        default:
            return 0u;
    }
}

static int rasta_shift_active(uint16_t mods)
{
    return (mods & (rasta_mod_lshift | rasta_mod_rshift)) != 0u;
}

uint16_t rasta_key_to_gem(uint16_t key, uint16_t mods)
{
    uint8_t ascii = 0u;
    int shifted = rasta_shift_active(mods);

    if (key >= 4u && key <= 29u) {
        ascii = (uint8_t)((shifted != 0 ? 'A' : 'a') + (char)(key - 4u));
    } else if (key >= 30u && key <= 38u) {
        static const char unshifted_digits[] = "123456789";
        static const char shifted_digits[] = "!@#$%^&*(";

        ascii = (uint8_t)((shifted != 0 ? shifted_digits
                                        : unshifted_digits)[key - 30u]);
    } else {
        switch (key) {
            case 39u:
                ascii = (uint8_t)((shifted != 0) ? ')' : '0');
                break;
            case 40u:
                ascii = '\n';
                break;
            case 41u:
                ascii = 27u;
                break;
            case 42u:
                ascii = '\b';
                break;
            case 43u:
                ascii = '\t';
                break;
            case 44u:
                ascii = ' ';
                break;
            case 45u:
                ascii = (uint8_t)((shifted != 0) ? '_' : '-');
                break;
            case 46u:
                ascii = (uint8_t)((shifted != 0) ? '+' : '=');
                break;
            case 47u:
                ascii = (uint8_t)((shifted != 0) ? '{' : '[');
                break;
            case 48u:
                ascii = (uint8_t)((shifted != 0) ? '}' : ']');
                break;
            case 49u:
                ascii = (uint8_t)((shifted != 0) ? '|' : '\\');
                break;
            case 51u:
                ascii = (uint8_t)((shifted != 0) ? ':' : ';');
                break;
            case 52u:
                ascii = (uint8_t)((shifted != 0) ? '"' : '\'');
                break;
            case 53u:
                ascii = (uint8_t)((shifted != 0) ? '~' : '`');
                break;
            case 54u:
                ascii = (uint8_t)((shifted != 0) ? '<' : ',');
                break;
            case 55u:
                ascii = (uint8_t)((shifted != 0) ? '>' : '.');
                break;
            case 56u:
                ascii = (uint8_t)((shifted != 0) ? '?' : '/');
                break;
            default:
                break;
        }
    }

    return (uint16_t)(((key & 0xffu) << 8) | ascii);
}
