/*
 * Implements the Stout guest's supported Atari XBIOS trap services.
 *
 * MIT License (see: LICENSE)
 * Copyright (C) 2026 tomaz stih
 */

#include "tos.h"

#include <stdio.h>
#include <string.h>

static void linea_write_font(uint32_t address, uint16_t id, uint16_t point,
                             uint32_t next)
{
    static const char name[] = "Stout system font";

    stout_write_u16(address + 0u, id);
    stout_write_u16(address + 2u, point);
    memcpy(g_stout.ram + address + 4u, name, sizeof(name));
    stout_write_u16(address + 36u, 0u);
    stout_write_u16(address + 38u, 255u);
    stout_write_u16(address + 40u, 13u);
    stout_write_u16(address + 42u, 12u);
    stout_write_u16(address + 44u, 8u);
    stout_write_u16(address + 46u, 2u);
    stout_write_u16(address + 48u, 2u);
    stout_write_u16(address + 50u, 8u);
    stout_write_u16(address + 52u, 8u);
    stout_write_u16(address + 58u, 1u);
    stout_write_u16(address + 60u, 1u);
    stout_write_u16(address + 62u, 0x5555u);
    stout_write_u16(address + 64u, 0x5555u);
    stout_write_u16(address + 66u, 0x000cu);
    stout_write_u32(address + 68u, 0u);
    stout_write_u32(address + 72u, STOUT_LINEA_FONT_OFFSETS);
    stout_write_u32(address + 76u, STOUT_LINEA_FONT_DATA);
    stout_write_u16(address + 80u, 256u);
    stout_write_u16(address + 82u, 16u);
    stout_write_u32(address + 84u, next);
}

void stout_linea_init(void)
{
    size_t index;
    uint32_t first = STOUT_LINEA_FONT_HEADERS;
    uint32_t second = first + 0x100u;
    uint32_t third = second + 0x100u;

    /* Enter a guest stub through the real 68000 Line-A exception vector.
     * The private trap supplies the hosted result and RTE restores the
     * interrupted program just as the Atari handler does. */
    stout_write_u32(10u * 4u, STOUT_LINEA_HANDLER);
    stout_write_u16(STOUT_LINEA_HANDLER,
                    (uint16_t)(0x4e40u | STOUT_LINEA_TRAP));
    stout_write_u16(STOUT_LINEA_HANDLER + 2u, 0x4e73u); /* RTE */

    stout_write_u16(STOUT_LINEA_BASE - 12u, STOUT_SCREEN_WIDTH);
    stout_write_u16(STOUT_LINEA_BASE - 4u, STOUT_SCREEN_HEIGHT);
    stout_write_u16(STOUT_LINEA_BASE - 2u, STOUT_SCREEN_WIDTH / 8u);
    stout_write_u32(STOUT_LINEA_BASE - 22u, STOUT_LINEA_FONT_DATA);
    stout_write_u16(STOUT_LINEA_BASE - 18u, 255u);
    stout_write_u16(STOUT_LINEA_BASE - 16u, 0u);
    stout_write_u16(STOUT_LINEA_BASE - 14u, 256u);
    stout_write_u16(STOUT_LINEA_BASE, 1u);
    stout_write_u16(STOUT_LINEA_BASE + 2u, STOUT_SCREEN_WIDTH / 8u);
    stout_write_u32(STOUT_LINEA_FONTS + 0u, first);
    stout_write_u32(STOUT_LINEA_FONTS + 4u, second);
    stout_write_u32(STOUT_LINEA_FONTS + 8u, third);
    stout_write_u32(STOUT_LINEA_FONTS + 12u, 0u);
    linea_write_font(first, 1u, 8u, second);
    linea_write_font(second, 1u, 9u, third);
    linea_write_font(third, 1u, 10u, 0u);
    for (index = 0u; index <= 256u; ++index)
        stout_write_u16(STOUT_LINEA_FONT_OFFSETS + (uint32_t)index * 2u,
                        (uint16_t)(index * 8u));
    for (index = 0u; index < 16u; ++index)
        stout_write_u32(STOUT_LINEA_VECTORS + (uint32_t)index * 4u, 0u);
}

int stout_handle_linea(void)
{
    uint32_t sp = stout_get_reg(M68K_REG_SP);
    uint32_t pc;
    uint16_t opcode;
    uint16_t function;

    if (!stout_valid_range(sp, 6u)) {
        if (g_stout.trace_traps != 0)
            fprintf(stderr, "stout: invalid Line-A frame sp=0x%08x\n", sp);
        return 0;
    }
    pc = stout_read_u32(sp + 2u);
    if (g_stout.trace_traps != 0)
        fprintf(stderr, "stout: Line-A frame sp=0x%08x pc=0x%08x\n", sp, pc);
    if (!stout_valid_range(pc, 2u))
        return 0;
    opcode = stout_read_u16(pc);
    function = (uint16_t)(opcode & 0x0fffu);
    if ((opcode & 0xf000u) != 0xa000u || function > 15u) {
        fprintf(stderr, "stout: unsupported Line-A opcode 0x%04x\n", opcode);
        return 0;
    }

    stout_write_u32(sp + 2u, pc + 2u);
    switch (function) {
        case 0:
            stout_set_reg(M68K_REG_D0, STOUT_LINEA_BASE);
            stout_set_reg(M68K_REG_A0, STOUT_LINEA_BASE);
            stout_set_reg(M68K_REG_A1, STOUT_LINEA_FONTS);
            stout_set_reg(M68K_REG_A2, STOUT_LINEA_VECTORS);
            break;
        case 3:
        case 4: {
            WORD points[4];
            WORD color = stout_read_s16(STOUT_LINEA_BASE + 24u) != 0 ? 1 : 0;
            WORD mode = (WORD)(stout_read_s16(STOUT_LINEA_BASE + 36u) + 1);

            points[0] = stout_read_s16(STOUT_LINEA_BASE + 38u);
            points[1] = stout_read_s16(STOUT_LINEA_BASE + 40u);
            points[2] = stout_read_s16(STOUT_LINEA_BASE + 42u);
            points[3] = function == 4u ? points[1]
                                       : stout_read_s16(STOUT_LINEA_BASE + 44u);
            (void)vswr_mode(g_stout.vdi_handle, mode);
            (void)vsl_color(g_stout.vdi_handle, color);
            v_pline(g_stout.vdi_handle, 2, points);
        } break;
        case 5: {
            WORD box[4];
            WORD color = stout_read_s16(STOUT_LINEA_BASE + 24u) != 0 ? 1 : 0;
            WORD mode = (WORD)(stout_read_s16(STOUT_LINEA_BASE + 36u) + 1);

            box[0] = stout_read_s16(STOUT_LINEA_BASE + 38u);
            box[1] = stout_read_s16(STOUT_LINEA_BASE + 40u);
            box[2] = stout_read_s16(STOUT_LINEA_BASE + 42u);
            box[3] = stout_read_s16(STOUT_LINEA_BASE + 44u);
            (void)vswr_mode(g_stout.vdi_handle, mode);
            (void)vsf_color(g_stout.vdi_handle, color);
            v_bar(g_stout.vdi_handle, box);
        } break;
        case 9:
            v_show_c(g_stout.vdi_handle, 0);
            break;
        case 10:
            v_hide_c(g_stout.vdi_handle);
            break;
        default:
            fprintf(stderr, "stout: unsupported Line-A opcode 0x%04x\n",
                    opcode);
            break;
    }
    return 1;
}

void stout_xbios_init(void)
{
    uint32_t device;

    for (device = 0u; device < 3u; ++device) {
        uint32_t record = STOUT_IOREC_BASE + device * STOUT_IOREC_STRIDE;

        stout_write_u32(record, record + STOUT_IOREC_BUFFER_OFFSET);
        stout_write_u16(record + 4u, STOUT_IOREC_BUFFER_SIZE);
        stout_write_u16(record + 6u, 0u);
        stout_write_u16(record + 8u, 0u);
        stout_write_u16(record + 10u, STOUT_IOREC_BUFFER_SIZE / 4u);
        stout_write_u16(record + 12u, (STOUT_IOREC_BUFFER_SIZE * 3u) / 4u);
    }
    memset(g_stout.ram + STOUT_KBDVECS_BASE, 0, STOUT_KBDVECS_SIZE);
    memset(g_stout.ram + STOUT_KEYTAB_UNSHIFT, 0, 128u);
    memset(g_stout.ram + STOUT_KEYTAB_SHIFT, 0, 128u);
    memset(g_stout.ram + STOUT_KEYTAB_CAPS, 0, 128u);
    stout_write_u32(STOUT_KEYTAB_BASE + 0u, STOUT_KEYTAB_UNSHIFT);
    stout_write_u32(STOUT_KEYTAB_BASE + 4u, STOUT_KEYTAB_SHIFT);
    stout_write_u32(STOUT_KEYTAB_BASE + 8u, STOUT_KEYTAB_CAPS);
    memset(g_stout.screen_palette, 0, sizeof(g_stout.screen_palette));
    g_stout.screen_palette[0] = 0x0777u;
}

int stout_handle_xbios(void)
{
    uint32_t sp = stout_get_reg(M68K_REG_SP);
    uint16_t opcode;

    if (!stout_valid_range(sp, 2u))
        return 0;
    opcode = stout_read_u16(sp);
    switch (opcode) {
        case 2: /* Physbase */
            stout_set_reg(M68K_REG_D0, g_stout.physical_screen);
            return 1;
        case 3: /* Logbase */
            stout_set_reg(M68K_REG_D0, g_stout.logical_screen);
            return 1;
        case 4: /* Getrez */
            stout_set_reg(M68K_REG_D0, g_stout.screen_resolution);
            return 1;
        case 5: { /* Setscreen */
            int32_t logical = (int32_t)stout_read_u32(sp + 2u);
            int32_t physical = (int32_t)stout_read_u32(sp + 6u);
            int16_t resolution = stout_read_s16(sp + 10u);
            int valid =
                (resolution == -1 || resolution == 2) &&
                (logical < 0 ||
                 stout_valid_range((uint32_t)logical, STOUT_SCREEN_BYTES)) &&
                (physical < 0 ||
                 stout_valid_range((uint32_t)physical, STOUT_SCREEN_BYTES));

            if (valid != 0) {
                if (logical >= 0)
                    g_stout.logical_screen = (uint32_t)logical;
                if (physical >= 0)
                    g_stout.physical_screen = (uint32_t)physical;
                if (resolution >= 0)
                    g_stout.screen_resolution = (uint16_t)resolution;
            }
            stout_set_reg(M68K_REG_D0, valid != 0 ? 0u : 0xffffffffu);
            return 1;
        }
        case 6: { /* Setpalette */
            uint32_t address;
            size_t index;

            if (!stout_valid_range(sp, 6u))
                return 0;
            address = stout_read_u32(sp + 2u);
            if (!stout_valid_range(address, sizeof(g_stout.screen_palette)))
                return 0;
            for (index = 0u; index < 16u; ++index)
                g_stout.screen_palette[index] =
                    stout_read_u16(address + (uint32_t)index * 2u);
            return 1;
        }
        case 7: { /* Setcolor */
            uint16_t index;
            int16_t color;
            uint16_t previous;

            if (!stout_valid_range(sp, 6u))
                return 0;
            index = stout_read_u16(sp + 2u);
            color = stout_read_s16(sp + 4u);
            if (index >= 16u)
                return 0;
            previous = g_stout.screen_palette[index];
            if (color != -1)
                g_stout.screen_palette[index] = (uint16_t)color;
            stout_set_reg(M68K_REG_D0, previous);
            return 1;
        }
        case 14: { /* Iorec */
            uint16_t device = stout_read_u16(sp + 2u);

            stout_set_reg(M68K_REG_D0, device < 3u ? STOUT_IOREC_BASE +
                                                         (uint32_t)device *
                                                             STOUT_IOREC_STRIDE
                                                   : 0xffffffffu);
            return 1;
        }
        case 16: { /* Keytbl */
            size_t index;

            if (!stout_valid_range(sp, 14u))
                return 0;
            for (index = 0u; index < 3u; ++index) {
                uint32_t table = stout_read_u32(sp + 2u +
                                                (uint32_t)index * 4u);

                if (table == 0xffffffffu)
                    continue;
                if (!stout_valid_range(table, 128u))
                    return 0;
                stout_write_u32(STOUT_KEYTAB_BASE + (uint32_t)index * 4u,
                                table);
            }
            stout_set_reg(M68K_REG_D0, STOUT_KEYTAB_BASE);
            return 1;
        }
        case 33: { /* Setprt */
            int16_t requested;
            uint16_t previous;

            if (!stout_valid_range(sp, 4u))
                return 0;
            requested = stout_read_s16(sp + 2u);
            previous = g_stout.printer_config;
            if (requested != -1)
                g_stout.printer_config = (uint16_t)requested;
            stout_set_reg(M68K_REG_D0, previous);
            return 1;
        }
        case 34: /* Kbdvbase */
            stout_set_reg(M68K_REG_D0, STOUT_KBDVECS_BASE);
            return 1;
        case 37: /* Vsync */
            gem_os_sleep_ms(20u);
            return 1;
        case 8: {
            uint32_t buffer;
            uint16_t drive;
            uint16_t sector;
            uint16_t track;
            uint16_t side;
            uint16_t count;
            const fat12_image_t *fat = stout_disk_fat12(&g_stout.disk);
            uint32_t first;
            uint16_t index;
            int ok;

            if (!stout_valid_range(sp, 20u))
                return 0;
            buffer = stout_read_u32(sp + 2u);
            drive = stout_read_u16(sp + 10u);
            sector = stout_read_u16(sp + 12u);
            track = stout_read_u16(sp + 14u);
            side = stout_read_u16(sp + 16u);
            count = stout_read_u16(sp + 18u);
            first = fat != NULL && fat->bpb.sectors_per_track != 0
                                 ? ((uint32_t)track * fat->bpb.sides + side) *
                                           fat->bpb.sectors_per_track +
                                       sector - 1u
                                 : 0xffffffffu;
            ok = g_stout.disk_mounted && drive == 0u && sector != 0u &&
                 first != 0xffffffffu;
            for (index = 0u; ok && index < count; ++index) {
                uint32_t address = buffer + (uint32_t)index * 512u;
                ok = stout_valid_range(address, 512u) &&
                     stout_disk_read_sector(&g_stout.disk, first + index,
                                            g_stout.ram + address, 512u);
            }
            stout_set_reg(M68K_REG_D0, ok ? 0u : 0xffffffffu);
            return 1;
        }
        case 9: {
            uint32_t buffer;
            uint16_t drive;
            uint16_t sector;
            uint16_t track;
            uint16_t side;
            uint16_t count;
            const fat12_image_t *fat = stout_disk_fat12(&g_stout.disk);
            uint32_t first;
            uint16_t index;
            int ok;

            if (!stout_valid_range(sp, 20u))
                return 0;
            buffer = stout_read_u32(sp + 2u);
            drive = stout_read_u16(sp + 10u);
            sector = stout_read_u16(sp + 12u);
            track = stout_read_u16(sp + 14u);
            side = stout_read_u16(sp + 16u);
            count = stout_read_u16(sp + 18u);
            first = fat != NULL && fat->bpb.sectors_per_track != 0
                                 ? ((uint32_t)track * fat->bpb.sides + side) *
                                           fat->bpb.sectors_per_track +
                                       sector - 1u
                                 : 0xffffffffu;
            ok = g_stout.disk_mounted && drive == 0u && sector != 0u &&
                 first != 0xffffffffu;

            for (index = 0u; ok && index < count; ++index) {
                uint32_t address = buffer + (uint32_t)index * 512u;

                ok = stout_valid_range(address, 512u) &&
                     stout_disk_write_sector(&g_stout.disk, first + index,
                                             g_stout.ram + address, 512u);
            }
            stout_set_reg(M68K_REG_D0, ok ? 0u : 0xffffffffu);
            return 1;
        }
        case 38: { /* Supexec: call a guest routine in supervisor context. */
            uint32_t function = stout_read_u32(sp + 2u);
            uint32_t return_pc = stout_get_reg(M68K_REG_PC);
            uint32_t callback_sp;
            uint32_t program_end =
                g_stout.load_address + (uint32_t)g_stout.program.image_size;

            if (function < g_stout.load_address || function >= program_end ||
                !stout_valid_range(function, 2u) || sp < 4u) {
                stout_set_reg(M68K_REG_D0, 0xffffffffu);
                return 1;
            }
            /* The trap dispatcher leaves the XBIOS arguments on the guest
             * stack.  Put the post-trap PC below them so the callback's RTS
             * returns to the instruction after XBIOS while preserving the
             * caller's normal stack cleanup. */
            callback_sp = sp - 4u;
            stout_write_u32(callback_sp, return_pc);
            stout_set_reg(M68K_REG_SP, callback_sp);
            stout_set_reg(M68K_REG_PC, function);
            return 1;
        }
        case 64: /* Blitmode: Stout exposes no hardware blitter. */
            if (!stout_valid_range(sp, 4u))
                return 0;
            stout_set_reg(M68K_REG_D0, 0u);
            return 1;
        default:
            fprintf(stderr, "stout: unsupported XBIOS opcode %u\n", opcode);
            return 0;
    }
}
