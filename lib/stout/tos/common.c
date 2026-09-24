/*
 * Implements helpers shared by Stout's Atari TOS trap service families.
 *
 * MIT License (see: LICENSE)
 * Copyright (C) 2026 tomaz stih
 */

#include "tos.h"

uint32_t stout_push_return_trap(uint32_t sp)
{
    sp -= 4u;
    stout_write_u32(sp, g_stout.stop_address);
    return sp;
}
