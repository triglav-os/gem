/*
 * Implements the Stout guest's supported Atari BIOS trap services.
 *
 * MIT License (see: LICENSE)
 * Copyright (C) 2026 tomaz stih
 */

#include "tos.h"

#include <stdio.h>

int stout_handle_bios(void)
{
    uint32_t sp = stout_get_reg(M68K_REG_SP);
    uint16_t opcode;

    if (!stout_valid_range(sp, 2u))
        return 0;
    opcode = stout_read_u16(sp);
    if (!stout_valid_range(sp + 2u, 14u))
        return 0;
    if (opcode == 1u) {
        stout_set_reg(M68K_REG_D0, stout_console_peek() ? 0xffffffffu : 0u);
        return 1;
    }
    if (opcode == 2u) {
        uint8_t character = 0;
        (void)stout_console_input(&character);
        stout_set_reg(M68K_REG_D0, character);
        return 1;
    }
    if (opcode == 3u) {
        stout_console_output((uint8_t)(stout_read_u16(sp + 4u) & 0xffu));
        stout_set_reg(M68K_REG_D0, 1u);
        return 1;
    }
    if (opcode == 5u) { /* Setexc */
        uint16_t vector;
        uint32_t handler;
        uint32_t address;
        uint32_t previous;

        if (!stout_valid_range(sp, 8u))
            return 0;
        vector = stout_read_u16(sp + 2u);
        handler = stout_read_u32(sp + 4u);
        address = (uint32_t)vector * 4u;
        if (!stout_valid_range(address, 4u))
            return 0;
        previous = stout_read_u32(address);
        if (handler != 0xffffffffu)
            stout_write_u32(address, handler);
        stout_set_reg(M68K_REG_D0, previous);
        return 1;
    }
    if (opcode == 10u) { /* Drvmap: C: plus A: when an image is mounted. */
        stout_set_reg(M68K_REG_D0, g_stout.disk_mounted ? 5u : 4u);
        return 1;
    }
    if (opcode == 11u) { /* Kbshift: no hosted modifiers yet. */
        stout_set_reg(M68K_REG_D0, 0u);
        return 1;
    }
    if (!g_stout.disk_mounted) {
        fprintf(stderr, "stout: unsupported BIOS opcode %u\n", opcode);
        return 0;
    }
    if (opcode == 7u) {
        stout_write_guest_bpb();
        stout_set_reg(M68K_REG_D0, g_stout.bpb_address);
        return 1;
    }
    if (opcode == 9u) {
        stout_set_reg(M68K_REG_D0, 0u);
        return 1;
    }
    if (opcode != 4u) {
        fprintf(stderr, "stout: unsupported BIOS opcode %u\n", opcode);
        return 0;
    }
    {
        uint16_t rwflag = stout_read_u16(sp + 2u);
        uint32_t buffer = stout_read_u32(sp + 4u);
        uint16_t count = stout_read_u16(sp + 8u);
        uint32_t sector = stout_read_u32(sp + 10u);
        uint16_t drive = stout_read_u16(sp + 14u);
        uint16_t index;
        int ok = rwflag <= 1u && drive == 0u;

        for (index = 0u; ok && index < count; ++index) {
            uint32_t address = buffer + (uint32_t)index * 512u;
            ok = stout_valid_range(address, 512u) &&
                 (rwflag == 0u
                      ? stout_disk_read_sector(&g_stout.disk, sector + index,
                                               g_stout.ram + address, 512u)
                      : stout_disk_write_sector(&g_stout.disk, sector + index,
                                                g_stout.ram + address, 512u));
        }
        stout_set_reg(M68K_REG_D0, ok ? 0u : 0xffffffffu);
    }
    return 1;
}
