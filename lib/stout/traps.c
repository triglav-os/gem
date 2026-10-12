/*
 * Routes Atari system traps raised by a Stout guest to the GEMDOS, AES/VDI,
 * BIOS and XBIOS compatibility handlers. Guest memory is validated before
 * every access.
 *
 * MIT License (see: LICENSE)
 * Copyright (C) 2026 tomaz stih
 */

#include "stout.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

uint16_t stout_read_u16(uint32_t address)
{
    return (uint16_t)(((uint16_t)g_stout.ram[address] << 8) |
                      (uint16_t)g_stout.ram[address + 1u]);
}

uint32_t stout_read_u32(uint32_t address)
{
    return ((uint32_t)g_stout.ram[address] << 24) |
           ((uint32_t)g_stout.ram[address + 1u] << 16) |
           ((uint32_t)g_stout.ram[address + 2u] << 8) |
           (uint32_t)g_stout.ram[address + 3u];
}

/*
 * Atari numbers the window rectangle selectors WF_WORKXYWH=4 and
 * WF_CURRXYWH=5; the hosted AES header uses 4 for the outer frame and 5
 * for the work area. Guest programs speak the Atari ABI, so their field
 * numbers are translated here for wind_get and wind_set.
 */
static WORD stout_wind_field(WORD field)
{
    if (field == 4) {
        return WF_CXYWH;
    }
    if (field == 5) {
        return WF_WXYWH;
    }
    return field;
}

static char *stout_guest_string(uint32_t address)
{
    size_t remaining;

    if (!stout_valid_range(address, 1u))
        return NULL;
    remaining = (size_t)STOUT_RAM_SIZE - (size_t)address;
    if (memchr(g_stout.ram + address, '\0', remaining) == NULL)
        return NULL;
    return (char *)(g_stout.ram + address);
}

static int stout_decode_tree(uint32_t address, OBJECT *objects,
                             WORD *object_count, uint8_t *active)
{
    WORD index;
    WORD count = 0;
    WORD resource_count = 0;

    if (g_stout.resource_address != 0u) {
        uint32_t object_table =
            g_stout.resource_address +
            stout_read_u16(g_stout.resource_address + 2u);
        uint16_t total = stout_read_u16(g_stout.resource_address + 20u);

        if (address >= object_table &&
            (address - object_table) % 24u == 0u) {
            uint32_t first = (address - object_table) / 24u;

            if (first < total && total - first <= 256u)
                resource_count = (WORD)(total - first);
        }
    }

    for (index = 0; index < (resource_count != 0 ? resource_count : 256);
         ++index) {
        uint32_t item = address + (uint32_t)index * 24u;
        OBJECT *object = &objects[index];
        uint32_t spec;

        if (!stout_valid_range(item, 24u))
            return 0;
        object->ob_next = stout_read_s16(item + 0u);
        object->ob_head = stout_read_s16(item + 2u);
        object->ob_tail = stout_read_s16(item + 4u);
        object->ob_type = stout_read_u16(item + 6u);
        object->ob_flags = stout_read_u16(item + 8u);
        object->ob_state = stout_read_u16(item + 10u);
        spec = stout_read_u32(item + 12u);
        if ((object->ob_flags & INDIRECT) != 0u) {
            if (!stout_valid_range(spec, 4u))
                return 0;
            spec = stout_read_u32(spec);
            object->ob_flags &= (UWORD)~INDIRECT;
        }
        object->ob_spec = (LONG)(uint32_t)spec;
        object->ob_x = stout_read_s16(item + 16u);
        object->ob_y = stout_read_s16(item + 18u);
        object->ob_width = stout_read_s16(item + 20u);
        object->ob_height = stout_read_s16(item + 22u);
        if (resource_count == 0 && (object->ob_flags & LASTOB) != 0u) {
            count = (WORD)(index + 1);
            break;
        }
    }
    if (resource_count != 0) {
        uint8_t reachable[256] = {0};
        int changed = 1;

        reachable[0] = 1u;
        while (changed != 0) {
            changed = 0;
            for (index = 0; index < resource_count; ++index) {
                WORD child;
                WORD steps = 0;

                if (reachable[index] == 0u || objects[index].ob_head == NIL)
                    continue;
                child = objects[index].ob_head;
                for (;;) {
                    if (child < 0 || child >= resource_count ||
                        steps++ >= resource_count)
                        return 0;
                    if (reachable[child] == 0u) {
                        reachable[child] = 1u;
                        changed = 1;
                    }
                    if (child == objects[index].ob_tail)
                        break;
                    child = objects[child].ob_next;
                }
            }
        }
        for (index = 0; index < resource_count; ++index) {
            if (reachable[index] != 0u)
                count = (WORD)(index + 1);
        }
        for (index = 0; index < count; ++index) {
            if (active != NULL)
                active[index] = reachable[index];
            if (reachable[index] == 0u) {
                memset(&objects[index], 0, sizeof(objects[index]));
                objects[index].ob_next = NIL;
                objects[index].ob_head = NIL;
                objects[index].ob_tail = NIL;
                objects[index].ob_type = G_IBOX;
                objects[index].ob_flags = HIDETREE;
            } else {
                objects[index].ob_flags &= (UWORD)~LASTOB;
            }
        }
        objects[count - 1].ob_flags |= LASTOB;
    } else if (active != NULL) {
        for (index = 0; index < count; ++index)
            active[index] = 1u;
    }
    if (count == 0)
        return 0;
    for (index = 0; index < count; ++index) {
        OBJECT *object = &objects[index];
        uint32_t spec = (uint32_t)object->ob_spec;
        UWORD type = (UWORD)(object->ob_type & 0xffu);

        if ((type == G_TITLE || type == G_STRING || type == G_BUTTON) &&
            spec != 0u) {
            char *string = stout_guest_string(spec);

            if (string == NULL)
                return 0;
            object->ob_spec = (LONG)(intptr_t)string;
        }
    }
    *object_count = count;
    return 1;
}

typedef struct stout_tree_bridge {
    OBJECT objects[256];
    TEDINFO text[256];
    ICONBLK icons[256];
    BITBLK images[256];
    uint8_t active[256];
    WORD count;
} stout_tree_bridge_t;

enum {
    /* Private libvdi selectors for Atari-compatible TEDINFO layout. */
    stout_ibm_font_compat = -3,
    stout_small_font_compat = -5
};

static int stout_decode_draw_tree(uint32_t address, stout_tree_bridge_t *bridge)
{
    WORD index;

    if (!stout_decode_tree(address, bridge->objects, &bridge->count,
                           bridge->active))
        return 0;
    for (index = 0; index < bridge->count; ++index) {
        OBJECT *object = &bridge->objects[index];
        uint32_t spec = (uint32_t)object->ob_spec;
        UWORD type = (UWORD)(object->ob_type & 0xffu);

        if (type == G_TEXT || type == G_BOXTEXT || type == G_FTEXT ||
            type == G_FBOXTEXT) {
            TEDINFO *text = &bridge->text[index];
            uint32_t current;
            uint32_t template;
            uint32_t valid;

            if (!stout_valid_range(spec, 28u))
                return 0;
            current = stout_read_u32(spec + 0u);
            template = stout_read_u32(spec + 4u);
            valid = stout_read_u32(spec + 8u);
            text->te_font = stout_read_s16(spec + 12u);
            if (text->te_font == IBM)
                text->te_font = stout_ibm_font_compat;
            else if (text->te_font == SMALL)
                text->te_font = stout_small_font_compat;
            text->te_junk1 = stout_read_s16(spec + 14u);
            text->te_just = stout_read_s16(spec + 16u);
            text->te_color = stout_read_s16(spec + 18u);
            text->te_junk2 = stout_read_s16(spec + 20u);
            text->te_thickness = stout_read_s16(spec + 22u);
            text->te_txtlen = stout_read_s16(spec + 24u);
            text->te_tmplen = stout_read_s16(spec + 26u);
            if (text->te_txtlen <= 0 ||
                !stout_valid_range(current, (size_t)text->te_txtlen) ||
                memchr(g_stout.ram + current, '\0', (size_t)text->te_txtlen) ==
                    NULL)
                return 0;
            text->te_ptext = (LONG)(intptr_t)(g_stout.ram + current);
            text->te_ptmplt = (LONG)(intptr_t)stout_guest_string(template);
            text->te_pvalid = (LONG)(intptr_t)stout_guest_string(valid);
            if (text->te_ptmplt == 0 || text->te_pvalid == 0)
                return 0;
            object->ob_spec = (LONG)(intptr_t)text;
        } else if (type == G_IMAGE) {
            BITBLK *image = &bridge->images[index];
            uint32_t data;
            size_t bytes;

            if (!stout_valid_range(spec, 14u))
                return 0;
            data = stout_read_u32(spec);
            image->bi_wb = stout_read_s16(spec + 4u);
            image->bi_hl = stout_read_s16(spec + 6u);
            image->bi_x = stout_read_s16(spec + 8u);
            image->bi_y = stout_read_s16(spec + 10u);
            image->bi_color = stout_read_s16(spec + 12u);
            if (image->bi_wb <= 0 || image->bi_hl <= 0)
                return 0;
            bytes = (size_t)image->bi_wb * (size_t)image->bi_hl;
            if (!stout_valid_range(data, bytes))
                return 0;
            image->bi_pdata = (LONG)(intptr_t)(g_stout.ram + data);
            object->ob_spec = (LONG)(intptr_t)image;
        } else if (type == G_ICON) {
            ICONBLK *icon = &bridge->icons[index];
            uint32_t mask;
            uint32_t data;
            uint32_t label;
            size_t bytes;

            if (!stout_valid_range(spec, 34u))
                return 0;
            mask = stout_read_u32(spec + 0u);
            data = stout_read_u32(spec + 4u);
            label = stout_read_u32(spec + 8u);
            icon->ib_char = stout_read_s16(spec + 12u);
            icon->ib_xchar = stout_read_s16(spec + 14u);
            icon->ib_ychar = stout_read_s16(spec + 16u);
            icon->ib_xicon = stout_read_s16(spec + 18u);
            icon->ib_yicon = stout_read_s16(spec + 20u);
            icon->ib_wicon = stout_read_s16(spec + 22u);
            icon->ib_hicon = stout_read_s16(spec + 24u);
            icon->ib_xtext = stout_read_s16(spec + 26u);
            icon->ib_ytext = stout_read_s16(spec + 28u);
            icon->ib_wtext = stout_read_s16(spec + 30u);
            icon->ib_htext = stout_read_s16(spec + 32u);
            if (icon->ib_wicon <= 0 || icon->ib_hicon <= 0)
                return 0;
            bytes = ((size_t)icon->ib_wicon + 15u) / 16u * 2u *
                    (size_t)icon->ib_hicon;
            if (!stout_valid_range(mask, bytes) ||
                !stout_valid_range(data, bytes))
                return 0;
            icon->ib_pmask = (LONG)(intptr_t)(g_stout.ram + mask);
            icon->ib_pdata = (LONG)(intptr_t)(g_stout.ram + data);
            icon->ib_ptext = (LONG)(intptr_t)stout_guest_string(label);
            if (icon->ib_ptext == 0)
                return 0;
            object->ob_spec = (LONG)(intptr_t)icon;
        }
    }
    return 1;
}

static void stout_store_tree_fields(uint32_t address,
                                    const stout_tree_bridge_t *bridge)
{
    WORD index;

    for (index = 0; index < bridge->count; ++index) {
        uint32_t item = address + (uint32_t)index * 24u;
        const OBJECT *object = &bridge->objects[index];

        if (bridge->active[index] == 0u)
            continue;

        stout_write_u16(item + 8u, object->ob_flags);
        stout_write_u16(item + 10u, object->ob_state);
        stout_write_u16(item + 16u, (uint16_t)object->ob_x);
        stout_write_u16(item + 18u, (uint16_t)object->ob_y);
        stout_write_u16(item + 20u, (uint16_t)object->ob_width);
        stout_write_u16(item + 22u, (uint16_t)object->ob_height);
    }
}

static int stout_decode_menu(uint32_t address)
{
    WORD count;

    if (!stout_decode_tree(address, g_stout.menu_objects, &count, NULL))
        return 0;
    g_stout.menu_guest_address = address;
    g_stout.menu_object_count = count;
    return 1;
}

static void stout_store_menu_state(WORD object)
{
    if (object >= 0 && object < g_stout.menu_object_count)
        stout_write_u16(g_stout.menu_guest_address + (uint32_t)object * 24u +
                            10u,
                        g_stout.menu_objects[object].ob_state);
}

int16_t stout_read_s16(uint32_t address)
{
    return (int16_t)stout_read_u16(address);
}

void stout_write_u16(uint32_t address, uint16_t value)
{
    g_stout.ram[address] = (uint8_t)((value >> 8) & 0xffu);
    g_stout.ram[address + 1u] = (uint8_t)(value & 0xffu);
}

void stout_write_u32(uint32_t address, uint32_t value)
{
    g_stout.ram[address] = (uint8_t)((value >> 24) & 0xffu);
    g_stout.ram[address + 1u] = (uint8_t)((value >> 16) & 0xffu);
    g_stout.ram[address + 2u] = (uint8_t)((value >> 8) & 0xffu);
    g_stout.ram[address + 3u] = (uint8_t)(value & 0xffu);
}

int stout_valid_range(uint32_t address, size_t size)
{
    if (address >= STOUT_RAM_SIZE) {
        return 0;
    }
    if (size > (size_t)STOUT_RAM_SIZE - (size_t)address) {
        return 0;
    }
    return 1;
}

void stout_read_string(uint32_t address, char *buffer, size_t size)
{
    size_t index;

    if (buffer == NULL || size == 0u) {
        return;
    }

    for (index = 0u; index + 1u < size; ++index) {
        if (!stout_valid_range(address + (uint32_t)index, 1u)) {
            break;
        }
        buffer[index] = (char)g_stout.ram[address + (uint32_t)index];
        if (buffer[index] == '\0') {
            return;
        }
    }

    buffer[index] = '\0';
}

uint32_t stout_get_reg(m68k_register_t reg)
{
    return m68k_get_reg(NULL, reg);
}

void stout_set_reg(m68k_register_t reg, uint32_t value)
{
    m68k_set_reg(reg, value);
}

void stout_stop_with_code(int code)
{
    g_stout.running = 0;
    g_stout.exit_code = code;
    m68k_end_timeslice();
}

static int stout_console_message(WORD message[8])
{
    return g_stout.app_id > 0 && appl_read(g_stout.app_id, 8, message) != 0;
}

static void stout_ensure_terminal(void)
{
    char terminal_name[] = "TERMINAL";
    char executable[GEM_OS_PATH_MAX];
    gem_os_pid_t pid;

    g_stout.terminal_id = appl_find(terminal_name);
    if (g_stout.terminal_id > 0) {
        return;
    }
    if (gem_os_find_executable("terminal", executable, sizeof(executable)) ==
            0 &&
        gem_os_find_file("bin/apps/terminal", executable, sizeof(executable)) ==
            0) {
        return;
    }
    if (gem_os_spawn_gem(executable, executable, NULL, 0u, g_stout.app_id,
                         &pid) != 0) {
        gem_os_sleep_ms(100u);
        g_stout.terminal_id = appl_find(terminal_name);
    }
}

int stout_console_open(void)
{
    WORD message[8];

    if (g_stout.app_id <= 0)
        g_stout.app_id = appl_init();
    if (g_stout.app_id <= 0)
        return 0;
    stout_ensure_terminal();
    if (g_stout.terminal_id <= 0)
        return 0;
    message[0] = GEM_MSG_CONSOLE_REGISTER;
    message[1] = g_stout.app_id;
    memset(message + 2, 0, sizeof(message) - 4u);
    return appl_write(g_stout.terminal_id, 8, message) != 0;
}

int stout_console_close(void)
{
    int result = 1;

    if (g_stout.app_id > 0)
        result = appl_exit() != 0;
    g_stout.app_id = 0;
    g_stout.terminal_id = 0;
    return result;
}

int stout_console_input(uint8_t *character)
{
    WORD message[8];

    if (g_stout.console_pending) {
        *character = g_stout.console_character;
        g_stout.console_pending = 0;
        return 1;
    }
    while (stout_console_message(message) != 0) {
        if (message[0] == GEM_MSG_CONSOLE_INPUT) {
            *character = (uint8_t)(message[1] & 0xff);
            return 1;
        }
    }
    return 0;
}

int stout_console_peek(void)
{
    uint8_t character;

    if (g_stout.console_pending)
        return 1;
    if (!stout_console_input(&character))
        return 0;
    g_stout.console_pending = 1;
    g_stout.console_character = character;
    return 1;
}

void stout_console_output(uint8_t character)
{
    WORD message[8] = {GEM_MSG_CONSOLE_OUTPUT, character, 0, 0, 0, 0, 0, 0};

    if (g_stout.terminal_id > 0) {
        (void)appl_write(g_stout.terminal_id, 8, message);
    } else {
        (void)fputc(character, stdout);
        (void)fflush(stdout);
    }
}

void stout_write_guest_bpb(void)
{
    const fat12_image_t *fat = stout_disk_fat12(&g_stout.disk);
    uint32_t address = g_stout.bpb_address;

    if (fat == NULL || !stout_valid_range(address, 22u))
        return;
    stout_write_u16(address + 0u, fat->bpb.sectors_per_cluster);
    stout_write_u16(address + 2u, fat->bpb.bytes_per_sector);
    stout_write_u16(address + 4u, fat->bpb.reserved_sectors);
    stout_write_u16(address + 6u, fat->bpb.fat_count);
    stout_write_u16(address + 8u, fat->bpb.root_entries);
    stout_write_u16(address + 10u, fat->bpb.total_sectors);
    stout_write_u16(address + 12u, fat->bpb.sectors_per_fat);
    stout_write_u16(address + 14u, fat->bpb.sectors_per_track);
    stout_write_u16(address + 16u, fat->bpb.sides);
    stout_write_u32(address + 18u, 0u);
}

#if 0 /* Implemented in tos/gemdos.c. */
static int stout_handle_gemdos(void)
{
    uint32_t sp = stout_get_reg(M68K_REG_SP);
    uint16_t function;

    if (!stout_valid_range(sp, 2u)) return 0;
    function = stout_read_u16(sp);
    switch (function) {
        case 0: /* Pterm0 */
            stout_stop_with_code(0);
            stout_set_reg(M68K_REG_SP, sp + 2u);
            return 1;
        case 1: { /* Cconin */
            uint8_t character = 0;
            (void)stout_console_input(&character);
            stout_set_reg(M68K_REG_D0, character);
            stout_set_reg(M68K_REG_SP, sp + 2u);
            return 1;
        }
        case 2: /* Cconout */
            if (!stout_valid_range(sp + 2u, 2u)) return 0;
            stout_console_output((uint8_t)(stout_read_u16(sp + 2u) & 0xffu));
            stout_set_reg(M68K_REG_D0, 0u);
            stout_set_reg(M68K_REG_SP, sp + 4u);
            return 1;
        case 9: { /* Cconws */
            char text[256];
            uint32_t address;
            size_t i;
            if (!stout_valid_range(sp + 2u, 4u)) return 0;
            address = stout_read_u32(sp + 2u);
            stout_read_string(address, text, sizeof(text));
            for (i = 0u; text[i] != '\0'; ++i)
                stout_console_output((uint8_t)text[i]);
            stout_set_reg(M68K_REG_D0, 0u);
            stout_set_reg(M68K_REG_SP, sp + 6u);
            return 1;
        }
        case 11: /* Cconis */ {
            stout_set_reg(M68K_REG_D0,
                          stout_console_peek() ? 0xffffffffu : 0u);
            stout_set_reg(M68K_REG_SP, sp + 2u);
            return 1;
        }
        case 40: /* Fwrite */ {
            uint16_t handle;
            uint32_t count;
            uint32_t address;
            uint32_t i;
            if (!stout_valid_range(sp + 2u, 10u)) return 0;
            handle = stout_read_u16(sp + 2u);
            count = stout_read_u32(sp + 4u);
            address = stout_read_u32(sp + 8u);
            if ((handle != 1u && handle != 2u) ||
                count > STOUT_RAM_SIZE ||
                !stout_valid_range(address, (size_t)count)) return 0;
            for (i = 0u; i < count; ++i)
                stout_console_output(g_stout.ram[address + i]);
            stout_set_reg(M68K_REG_D0, count);
            stout_set_reg(M68K_REG_SP, sp + 12u);
            return 1;
        }
        case 42: /* Fread, console input */
            if (!stout_valid_range(sp + 2u, 10u)) return 0;
            if (stout_read_u16(sp + 2u) != 0u) return 0;
            {
                uint32_t count = stout_read_u32(sp + 4u);
                uint32_t address = stout_read_u32(sp + 8u);
                uint8_t character;
                if (!stout_valid_range(address, (size_t)count) || count == 0u)
                    return 0;
                if (!stout_console_input(&character)) {
                    stout_set_reg(M68K_REG_D0, 0u);
                } else {
                    g_stout.ram[address] = character;
                    stout_set_reg(M68K_REG_D0, 1u);
                }
                stout_set_reg(M68K_REG_SP, sp + 12u);
            }
            return 1;
        default:
            return 0;
    }
}

static int stout_handle_bios(void)
{
    uint32_t sp = stout_get_reg(M68K_REG_SP);
    uint16_t opcode;

    if (!stout_valid_range(sp, 2u)) return 0;
    opcode = stout_read_u16(sp);
    if (!stout_valid_range(sp + 2u, 14u)) return 0;
    if (opcode == 1u) { /* Bconstat */
        stout_set_reg(M68K_REG_D0, stout_console_peek() ? 0xffffffffu : 0u);
        stout_set_reg(M68K_REG_SP, sp + 4u);
        return 1;
    }
    if (opcode == 2u) { /* Bconin */
        uint8_t character = 0;
        (void)stout_console_input(&character);
        stout_set_reg(M68K_REG_D0, character);
        stout_set_reg(M68K_REG_SP, sp + 4u);
        return 1;
    }
    if (opcode == 3u) { /* Bconout */
        stout_console_output((uint8_t)(stout_read_u16(sp + 4u) & 0xffu));
        stout_set_reg(M68K_REG_D0, 1u);
        stout_set_reg(M68K_REG_SP, sp + 6u);
        return 1;
    }
    if (!g_stout.disk_mounted) {
        fprintf(stderr, "stout: unsupported BIOS opcode %u\n", opcode);
        return 0;
    }
    if (opcode == 7u) { /* Getbpb */
        stout_write_guest_bpb();
        stout_set_reg(M68K_REG_D0, g_stout.bpb_address);
        stout_set_reg(M68K_REG_SP, sp + 4u);
        return 1;
    }
    if (opcode == 9u) { /* Mediach */
        stout_set_reg(M68K_REG_D0, 0u);
        stout_set_reg(M68K_REG_SP, sp + 4u);
        return 1;
    }
    if (opcode == 10u) { /* Drvmap: drive A: */
        stout_set_reg(M68K_REG_D0, 1u);
        stout_set_reg(M68K_REG_SP, sp + 4u);
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
        int ok = rwflag == 0u && drive == 0u;

        for (index = 0u; ok && index < count; ++index) {
            uint32_t address = buffer + (uint32_t)index * 512u;
            ok = stout_valid_range(address, 512u) &&
                 stout_disk_read_sector(&g_stout.disk, sector + index,
                                        g_stout.ram + address, 512u);
        }
        stout_set_reg(M68K_REG_D0, ok ? 0u : 0xffffffffu);
        stout_set_reg(M68K_REG_SP, sp + 16u);
    }
    return 1;
}

uint32_t stout_push_return_trap(uint32_t sp)
{
    sp -= 4u;
    stout_write_u32(sp, g_stout.stop_address);
    return sp;
}

static int stout_handle_xbios(void)
{
    uint32_t sp = stout_get_reg(M68K_REG_SP);
    uint16_t opcode = stout_read_u16(sp);

    switch (opcode) {
        case 7: /* Getbpb */
            if (!g_stout.disk_mounted) return 0;
            stout_write_guest_bpb();
            stout_set_reg(M68K_REG_D0, g_stout.bpb_address);
            stout_set_reg(M68K_REG_SP, sp + 2u);
            return 1;
        case 8: { /* Floprd */
            uint32_t buffer = stout_read_u32(sp + 2u);
            uint16_t drive = stout_read_u16(sp + 6u);
            uint16_t sector = stout_read_u16(sp + 8u);
            uint16_t track = stout_read_u16(sp + 10u);
            uint16_t side = stout_read_u16(sp + 12u);
            uint16_t count = stout_read_u16(sp + 14u);
            const fat12_image_t *fat = stout_disk_fat12(&g_stout.disk);
            uint32_t first = fat != NULL && fat->bpb.sectors_per_track != 0
                                 ? ((uint32_t)track * fat->bpb.sides + side) *
                                       fat->bpb.sectors_per_track + sector - 1u
                                 : 0xffffffffu;
            uint16_t index;
            int ok = g_stout.disk_mounted && drive == 0u && first != 0xffffffffu;
            for (index = 0u; ok && index < count; ++index) {
                uint32_t address = buffer + (uint32_t)index * 512u;
                ok = stout_valid_range(address, 512u) &&
                     stout_disk_read_sector(&g_stout.disk, first + index,
                                            g_stout.ram + address, 512u);
            }
            stout_set_reg(M68K_REG_D0, ok ? 0u : 0xffffffffu);
            stout_set_reg(M68K_REG_SP, sp + 16u);
            return 1;
        }
        case 9: /* Flopwr: mounted images are deliberately read-only. */
            stout_set_reg(M68K_REG_D0, 0xffffffffu);
            stout_set_reg(M68K_REG_SP, sp + 16u);
            return 1;
        case 10: /* Drvmap */
            stout_set_reg(M68K_REG_D0, g_stout.disk_mounted ? 1u : 0u);
            stout_set_reg(M68K_REG_SP, sp + 2u);
            return 1;
        case 4:
            stout_set_reg(M68K_REG_D0, 2u);
            stout_set_reg(M68K_REG_SP, sp + 2u);
            return 1;
        default:
            fprintf(stderr, "stout: unsupported XBIOS opcode %u\n", opcode);
            return 0;
    }
}

#endif

enum {
    stout_rsh_header_size = 36u,
    stout_object_size = 24u,
    stout_tedinfo_size = 28u,
    stout_iconblk_size = 34u,
    stout_bitblk_size = 14u
};

static int stout_resource_range(uint32_t offset, size_t length)
{
    return offset <= g_stout.resource_size &&
           length <= g_stout.resource_size - offset;
}

static uint32_t stout_resource_pointer(uint32_t offset, size_t length)
{
    if (offset == 0u || !stout_resource_range(offset, length))
        return 0u;
    return g_stout.resource_address + offset;
}

static int stout_resource_relocate_long(uint32_t address, size_t length)
{
    uint32_t offset = stout_read_u32(address);
    uint32_t pointer = stout_resource_pointer(offset, length);

    if (offset != 0u && pointer == 0u)
        return 0;
    stout_write_u32(address, pointer);
    return 1;
}

static int stout_resource_fix_tedinfo(uint32_t address)
{
    return stout_resource_relocate_long(address, 1u) &&
           stout_resource_relocate_long(address + 4u, 1u) &&
           stout_resource_relocate_long(address + 8u, 1u);
}

static int stout_resource_fix_iconblk(uint32_t address)
{
    return stout_resource_relocate_long(address, 2u) &&
           stout_resource_relocate_long(address + 4u, 2u) &&
           stout_resource_relocate_long(address + 8u, 1u);
}

static WORD stout_resource_coordinate(uint16_t packed, WORD cell)
{
    int8_t pixels = (int8_t)(packed >> 8);

    return (WORD)((packed & 0xffu) * (uint16_t)cell + pixels);
}

static void stout_resource_fix_coordinates(uint32_t address)
{
    stout_write_u16(address + 16u,
                    (uint16_t)stout_resource_coordinate(
                        stout_read_u16(address + 16u), 8));
    stout_write_u16(address + 18u,
                    (uint16_t)stout_resource_coordinate(
                        stout_read_u16(address + 18u), 16));
    stout_write_u16(address + 20u,
                    (uint16_t)stout_resource_coordinate(
                        stout_read_u16(address + 20u), 8));
    stout_write_u16(address + 22u,
                    (uint16_t)stout_resource_coordinate(
                        stout_read_u16(address + 22u), 16));
}

static int stout_resource_fix_object(uint32_t address)
{
    uint16_t type = stout_read_u16(address + 6u) & 0x00ffu;
    uint32_t spec = address + 12u;
    int okay = 1;

    switch (type) {
        case G_STRING:
        case G_TITLE:
        case G_BUTTON:
            okay = stout_resource_relocate_long(spec, 1u);
            break;
        case G_TEXT:
        case G_BOXTEXT:
        case G_FTEXT:
        case G_FBOXTEXT:
            okay = stout_resource_relocate_long(spec, stout_tedinfo_size);
            break;
        case G_ICON:
            okay = stout_resource_relocate_long(spec, stout_iconblk_size);
            break;
        case G_IMAGE:
            okay = stout_resource_relocate_long(spec, stout_bitblk_size);
            break;
        case G_USERDEF:
            okay = stout_resource_relocate_long(spec, 8u);
            break;
        default:
            break;
    }
    if (!okay)
        return 0;
    stout_resource_fix_coordinates(address);
    return 1;
}

static int stout_resource_load(uint32_t filename_address)
{
    uint8_t *data = NULL;
    size_t size = 0u;
    uint32_t base;
    uint32_t object_offset;
    uint32_t tedinfo_offset;
    uint32_t iconblk_offset;
    uint32_t bitblk_offset;
    uint32_t tree_offset;
    uint16_t objects;
    uint16_t trees;
    uint16_t tedinfos;
    uint16_t icons;
    uint16_t bitblks;
    uint16_t strings;
    uint16_t images;
    uint16_t index;
    int okay = 0;

    if (!stout_gemdos_read_file(filename_address, &data, &size) ||
        size < stout_rsh_header_size || size > UINT16_MAX ||
        g_stout.heap_next > STOUT_TPA_TOP ||
        size > STOUT_TPA_TOP - g_stout.heap_next)
        goto done;
    base = (g_stout.heap_next + 3u) & ~3u;
    if (!stout_valid_range(base, size))
        goto done;
    memcpy(g_stout.ram + base, data, size);
    g_stout.resource_address = base;
    g_stout.resource_size = (uint32_t)size;
    if (stout_read_u16(base + 34u) > size)
        goto invalid;
    object_offset = stout_read_u16(base + 2u);
    tedinfo_offset = stout_read_u16(base + 4u);
    iconblk_offset = stout_read_u16(base + 6u);
    bitblk_offset = stout_read_u16(base + 8u);
    tree_offset = stout_read_u16(base + 18u);
    objects = stout_read_u16(base + 20u);
    trees = stout_read_u16(base + 22u);
    tedinfos = stout_read_u16(base + 24u);
    icons = stout_read_u16(base + 26u);
    bitblks = stout_read_u16(base + 28u);
    strings = stout_read_u16(base + 30u);
    images = stout_read_u16(base + 32u);
    if ((objects != 0u &&
         !stout_resource_range(object_offset,
                               (size_t)objects * stout_object_size)) ||
        (tedinfos != 0u &&
         !stout_resource_range(tedinfo_offset,
                               (size_t)tedinfos * stout_tedinfo_size)) ||
        (icons != 0u &&
         !stout_resource_range(iconblk_offset,
                               (size_t)icons * stout_iconblk_size)) ||
        (bitblks != 0u &&
         !stout_resource_range(bitblk_offset,
                               (size_t)bitblks * stout_bitblk_size)) ||
        (trees != 0u &&
         !stout_resource_range(tree_offset, (size_t)trees * 4u)) ||
        (strings != 0u &&
         !stout_resource_range(stout_read_u16(base + 10u),
                               (size_t)strings * 4u)) ||
        (images != 0u &&
         !stout_resource_range(stout_read_u16(base + 16u),
                               (size_t)images * 4u)))
        goto invalid;
    for (index = 0u; index < tedinfos; ++index) {
        if (!stout_resource_fix_tedinfo(
                base + tedinfo_offset + (uint32_t)index * stout_tedinfo_size))
            goto invalid;
    }
    for (index = 0u; index < icons; ++index) {
        if (!stout_resource_fix_iconblk(
                base + iconblk_offset + (uint32_t)index * stout_iconblk_size))
            goto invalid;
    }
    for (index = 0u; index < bitblks; ++index) {
        if (!stout_resource_relocate_long(
                base + bitblk_offset + (uint32_t)index * stout_bitblk_size,
                2u))
            goto invalid;
    }
    for (index = 0u; index < objects; ++index) {
        if (!stout_resource_fix_object(
                base + object_offset + (uint32_t)index * stout_object_size))
            goto invalid;
    }
    for (index = 0u; index < trees; ++index) {
        if (!stout_resource_relocate_long(base + tree_offset +
                                               (uint32_t)index * 4u,
                                           stout_object_size))
            goto invalid;
    }
    for (index = 0u; index < strings; ++index) {
        if (!stout_resource_relocate_long(
                base + stout_read_u16(base + 10u) + (uint32_t)index * 4u,
                1u))
            goto invalid;
    }
    for (index = 0u; index < images; ++index) {
        if (!stout_resource_relocate_long(
                base + stout_read_u16(base + 16u) + (uint32_t)index * 4u,
                2u))
            goto invalid;
    }
    g_stout.heap_next = (base + (uint32_t)size + 3u) & ~3u;
    okay = 1;
    goto done;
invalid:
    g_stout.resource_address = 0u;
    g_stout.resource_size = 0u;
done:
    free(data);
    return okay;
}

static uint32_t stout_resource_address_for(WORD type, WORD index)
{
    uint32_t base = g_stout.resource_address;
    uint32_t table;
    uint32_t stride;
    uint16_t count;

    if (base == 0u || index < 0)
        return 0u;
    switch (type) {
        case R_TREE:
            count = stout_read_u16(base + 22u);
            table = stout_read_u16(base + 18u);
            if ((uint16_t)index >= count ||
                !stout_resource_range(table + (uint32_t)index * 4u, 4u))
                return 0u;
            return stout_read_u32(base + table + (uint32_t)index * 4u);
        case R_OBJECT:
            table = stout_read_u16(base + 2u);
            count = stout_read_u16(base + 20u);
            stride = stout_object_size;
            break;
        case R_TEDINFO:
            table = stout_read_u16(base + 4u);
            count = stout_read_u16(base + 24u);
            stride = stout_tedinfo_size;
            break;
        case R_ICONBLK:
            table = stout_read_u16(base + 6u);
            count = stout_read_u16(base + 26u);
            stride = stout_iconblk_size;
            break;
        case R_BITBLK:
            table = stout_read_u16(base + 8u);
            count = stout_read_u16(base + 28u);
            stride = stout_bitblk_size;
            break;
        case R_STRING:
            table = stout_read_u16(base + 10u);
            count = stout_read_u16(base + 30u);
            stride = 4u;
            if ((uint16_t)index >= count ||
                !stout_resource_range(table + (uint32_t)index * stride,
                                      stride))
                return 0u;
            return stout_read_u32(base + table + (uint32_t)index * stride);
        case R_FRSTR:
            table = stout_read_u16(base + 10u);
            count = stout_read_u16(base + 30u);
            stride = 4u;
            if ((uint16_t)index >= count ||
                !stout_resource_range(table + (uint32_t)index * stride,
                                      stride))
                return 0u;
            return base + table + (uint32_t)index * stride;
        case R_IMAGEDATA:
            table = stout_read_u16(base + 16u);
            count = stout_read_u16(base + 32u);
            stride = 4u;
            if ((uint16_t)index >= count ||
                !stout_resource_range(table + (uint32_t)index * stride,
                                      stride))
                return 0u;
            return stout_read_u32(base + table + (uint32_t)index * stride);
        case R_FRIMG:
            table = stout_read_u16(base + 16u);
            count = stout_read_u16(base + 32u);
            stride = 4u;
            if ((uint16_t)index >= count ||
                !stout_resource_range(table + (uint32_t)index * stride,
                                      stride))
                return 0u;
            return base + table + (uint32_t)index * stride;
        default:
            return 0u;
    }
    if ((uint16_t)index >= count ||
        !stout_resource_range(table + (uint32_t)index * stride, stride))
        return 0u;
    return base + table + (uint32_t)index * stride;
}

static int stout_handle_aes(void)
{
    stout_aespb_t pb;
    uint32_t d0 = stout_get_reg(M68K_REG_D0);
    uint32_t d1 = stout_get_reg(M68K_REG_D1);
    uint16_t opcode;
    WORD intin0;
    WORD intin1;
    WORD intin2;
    WORD intin3;
    WORD intin4;

    if ((uint16_t)d0 != AES_TRAP_MAGIC || !stout_valid_range(d1, sizeof(pb))) {
        return 0;
    }

    pb.control = stout_read_u32(d1);
    pb.global = stout_read_u32(d1 + 4u);
    pb.intin = stout_read_u32(d1 + 8u);
    pb.intout = stout_read_u32(d1 + 12u);
    pb.addrin = stout_read_u32(d1 + 16u);
    pb.addrout = stout_read_u32(d1 + 20u);

    opcode = stout_read_u16(pb.control);
    if (g_stout.trace_traps != 0)
        fprintf(stderr, "stout: AES opcode=%u\n", opcode);
    intin0 = stout_read_s16(pb.intin + 0u);
    intin1 = stout_read_s16(pb.intin + 2u);
    intin2 = stout_read_s16(pb.intin + 4u);
    intin3 = stout_read_s16(pb.intin + 6u);
    intin4 = stout_read_s16(pb.intin + 8u);

    switch (opcode) {
        case 10:
            if (g_stout.app_id <= 0)
                g_stout.app_id = appl_init();
            if (stout_valid_range(pb.global, 30u)) {
                stout_write_u16(pb.global + 0u, 0x0140u);
                stout_write_u16(pb.global + 2u, 0xffffu);
                stout_write_u16(pb.global + 4u, (uint16_t)g_stout.app_id);
                stout_write_u32(pb.global + 10u, 0u);
                stout_write_u32(pb.global + 14u, 0u);
            }
            stout_write_u16(pb.intout, (uint16_t)g_stout.app_id);
            return 1;
        case 13: { /* appl_find */
            char *name = stout_guest_string(stout_read_u32(pb.addrin));
            WORD status = name != NULL ? appl_find(name) : 0;

            stout_write_u16(pb.intout, (uint16_t)status);
            return 1;
        }
        case 30: { /* menu_bar */
            uint32_t tree = stout_read_u32(pb.addrin);
            WORD status;

            if (intin0 != 0 && !stout_decode_menu(tree)) {
                fprintf(stderr,
                        "stout: invalid or oversized menu tree at 0x%08x\n",
                        tree);
                return 0;
            }
            if (g_stout.trace_traps != 0)
                fprintf(stderr, "stout: AES menu tree=0x%08x objects=%d\n",
                        tree, g_stout.menu_object_count);
            status =
                menu_bar(intin0 != 0 ? g_stout.menu_objects : NULL, intin0);
            stout_write_u16(pb.intout, (uint16_t)status);
            return 1;
        }
        case 31: { /* menu_icheck */
            WORD status = menu_icheck(g_stout.menu_objects, intin0, intin1);

            stout_store_menu_state(intin0);
            stout_write_u16(pb.intout, (uint16_t)status);
            return 1;
        }
        case 32: { /* menu_ienable */
            WORD status = menu_ienable(g_stout.menu_objects, intin0, intin1);

            stout_store_menu_state(intin0);
            stout_write_u16(pb.intout, (uint16_t)status);
            return 1;
        }
        case 33: { /* menu_tnormal */
            WORD status = menu_tnormal(g_stout.menu_objects, intin0, intin1);

            stout_store_menu_state(intin0);
            stout_write_u16(pb.intout, (uint16_t)status);
            return 1;
        }
        case 34: { /* menu_text */
            char *label = stout_guest_string(stout_read_u32(pb.addrin));
            WORD status = label != NULL
                              ? menu_text(g_stout.menu_objects, intin0, label)
                              : 0;

            stout_write_u16(pb.intout, (uint16_t)status);
            return 1;
        }
        case 35: { /* menu_register */
            char *label = stout_guest_string(stout_read_u32(pb.addrin));
            WORD status = label != NULL ? menu_register(intin0, label) : 0;

            stout_write_u16(pb.intout, (uint16_t)status);
            return 1;
        }
        case 42: { /* objc_draw */
            stout_tree_bridge_t *tree =
                (stout_tree_bridge_t *)calloc(1u, sizeof(*tree));
            uint32_t tree_address = stout_read_u32(pb.addrin);
            WORD result;

            if (tree == NULL)
                return 0;
            if (!stout_decode_draw_tree(tree_address, tree) || intin0 < 0 ||
                intin0 >= tree->count) {
                free(tree);
                return 0;
            }
            if (g_stout.trace_traps != 0)
                fprintf(stderr,
                        "stout: AES draw root=%d,%d %dx%d clip=%d,%d %dx%d\n",
                        tree->objects[0].ob_x, tree->objects[0].ob_y,
                        tree->objects[0].ob_width, tree->objects[0].ob_height,
                        intin2, intin3, intin4, stout_read_s16(pb.intin + 10u));
            result = objc_draw(tree->objects, intin0, intin1, intin2, intin3,
                               intin4, stout_read_s16(pb.intin + 10u));
            stout_write_u16(pb.intout, (uint16_t)result);
            free(tree);
            return 1;
        }
        case 43: { /* objc_find */
            stout_tree_bridge_t *tree =
                (stout_tree_bridge_t *)calloc(1u, sizeof(*tree));
            uint32_t tree_address = stout_read_u32(pb.addrin);
            WORD result;

            if (tree == NULL)
                return 0;
            if (!stout_decode_draw_tree(tree_address, tree) || intin0 < 0 ||
                intin0 >= tree->count) {
                free(tree);
                return 0;
            }
            result = objc_find(tree->objects, intin0, intin1, intin2, intin3);
            stout_write_u16(pb.intout, (uint16_t)result);
            free(tree);
            return 1;
        }
        case 44: { /* objc_offset */
            OBJECT decoded[256];
            OBJECT *tree;
            WORD count;
            WORD x;
            WORD y;
            WORD result;
            uint32_t tree_address = stout_read_u32(pb.addrin);

            if (tree_address == g_stout.menu_guest_address) {
                tree = g_stout.menu_objects;
                count = g_stout.menu_object_count;
            } else {
                tree = decoded;
                if (!stout_decode_tree(tree_address, tree, &count, NULL))
                    return 0;
            }
            if (intin0 < 0 || intin0 >= count)
                return 0;
            result = objc_offset(tree, intin0, &x, &y);
            stout_write_u16(pb.intout + 0u, (uint16_t)result);
            stout_write_u16(pb.intout + 2u, (uint16_t)x);
            stout_write_u16(pb.intout + 4u, (uint16_t)y);
            return 1;
        }
        case 46: { /* objc_edit */
            stout_tree_bridge_t *tree =
                (stout_tree_bridge_t *)calloc(1u, sizeof(*tree));
            uint32_t tree_address = stout_read_u32(pb.addrin);
            WORD index = intin2;
            WORD result;

            if (tree == NULL)
                return 0;
            if (!stout_decode_draw_tree(tree_address, tree) || intin0 < 0 ||
                intin0 >= tree->count) {
                free(tree);
                return 0;
            }
            result = objc_edit(tree->objects, intin0, intin1, &index, intin3);
            stout_store_tree_fields(tree_address, tree);
            stout_write_u16(pb.intout + 0u, (uint16_t)result);
            stout_write_u16(pb.intout + 2u, (uint16_t)index);
            free(tree);
            return 1;
        }
        case 47: { /* objc_change */
            stout_tree_bridge_t *tree =
                (stout_tree_bridge_t *)calloc(1u, sizeof(*tree));
            uint32_t tree_address = stout_read_u32(pb.addrin);
            WORD result;

            if (tree == NULL)
                return 0;
            if (!stout_decode_draw_tree(tree_address, tree) || intin0 < 0 ||
                intin0 >= tree->count) {
                free(tree);
                return 0;
            }
            result = objc_change(
                tree->objects, intin0, intin1, intin2, intin3, intin4,
                stout_read_s16(pb.intin + 10u),
                stout_read_s16(pb.intin + 12u),
                stout_read_s16(pb.intin + 14u));
            stout_store_tree_fields(tree_address, tree);
            stout_write_u16(pb.intout, (uint16_t)result);
            free(tree);
            return 1;
        }
        case 50: { /* form_do */
            stout_tree_bridge_t *tree =
                (stout_tree_bridge_t *)calloc(1u, sizeof(*tree));
            uint32_t tree_address = stout_read_u32(pb.addrin);
            WORD result;

            if (tree == NULL)
                return 0;
            if (!stout_decode_draw_tree(tree_address, tree) || intin0 < 0 ||
                intin0 >= tree->count) {
                free(tree);
                return 0;
            }
            result = form_do(tree->objects, intin0);
            stout_store_tree_fields(tree_address, tree);
            stout_write_u16(pb.intout, (uint16_t)result);
            free(tree);
            return 1;
        }
        case 51: { /* form_dial */
            WORD status = form_dial(
                intin0, intin1, intin2, intin3, intin4,
                stout_read_s16(pb.intin + 10u), stout_read_s16(pb.intin + 12u),
                stout_read_s16(pb.intin + 14u), stout_read_s16(pb.intin + 16u));

            stout_write_u16(pb.intout, (uint16_t)status);
            return 1;
        }
        case 54: { /* form_center */
            /* Atari high resolution uses gl_hbox = gl_hchar + 3. */
            enum { atari_high_menu_height = 19 };
            LONG available;
            WORD x;
            WORD y;
            WORD w;
            WORD h;
            uint32_t tree_address = stout_read_u32(pb.addrin);

            if (!stout_valid_range(tree_address, 24u))
                return 0;
            w = stout_read_s16(tree_address + 20u);
            h = stout_read_s16(tree_address + 22u);
            available = (LONG)STOUT_SCREEN_WIDTH - w;
            x = available > 0 ? (WORD)(available / 2) : 0;
            available = (LONG)STOUT_SCREEN_HEIGHT - atari_high_menu_height - h;
            y = (WORD)(atari_high_menu_height +
                       (available > 0 ? available / 2 : 0));
            if (g_stout.trace_traps != 0)
                fprintf(stderr,
                        "stout: AES center tree=0x%08x root=%d,%d %dx%d -> "
                        "%d,%d\n",
                        tree_address, stout_read_s16(tree_address + 16u),
                        stout_read_s16(tree_address + 18u), w, h, x, y);
            stout_write_u16(tree_address + 16u, (uint16_t)x);
            stout_write_u16(tree_address + 18u, (uint16_t)y);
            stout_write_u16(pb.intout + 0u, 1u);
            stout_write_u16(pb.intout + 2u, (uint16_t)x);
            stout_write_u16(pb.intout + 4u, (uint16_t)y);
            stout_write_u16(pb.intout + 6u, (uint16_t)w);
            stout_write_u16(pb.intout + 8u, (uint16_t)h);
            return 1;
        }
        case 19:
            stout_write_u16(pb.intout, (uint16_t)stout_console_close());
            return 1;
        case 20: /* evnt_keybd */
            stout_write_u16(pb.intout, (uint16_t)evnt_keybd());
            return 1;
        case 21: { /* evnt_button */
            WORD mx;
            WORD my;
            WORD mb;
            WORD ks;
            WORD event = evnt_button(intin0, (UWORD)intin1, (UWORD)intin2,
                                     &mx, &my, &mb, &ks);

            stout_write_u16(pb.intout + 0u, (uint16_t)event);
            stout_write_u16(pb.intout + 2u, (uint16_t)mx);
            stout_write_u16(pb.intout + 4u, (uint16_t)my);
            stout_write_u16(pb.intout + 6u, (uint16_t)mb);
            stout_write_u16(pb.intout + 8u, (uint16_t)ks);
            return 1;
        }
        case 22: { /* evnt_mouse */
            WORD mx;
            WORD my;
            WORD mb;
            WORD ks;
            WORD event = evnt_mouse(intin0, intin1, intin2, intin3, intin4,
                                    &mx, &my, &mb, &ks);

            stout_write_u16(pb.intout + 0u, (uint16_t)event);
            stout_write_u16(pb.intout + 2u, (uint16_t)mx);
            stout_write_u16(pb.intout + 4u, (uint16_t)my);
            stout_write_u16(pb.intout + 6u, (uint16_t)mb);
            stout_write_u16(pb.intout + 8u, (uint16_t)ks);
            return 1;
        }
        case 23: { /* evnt_mesag */
            WORD message[8];
            uint32_t message_ptr = stout_read_u32(pb.addrin);
            WORD event;
            size_t index;

            if (!stout_valid_range(message_ptr, sizeof(message)))
                return 0;
            event = evnt_mesag(message);
            stout_write_u16(pb.intout, (uint16_t)event);
            for (index = 0u; index < 8u; ++index) {
                stout_write_u16(message_ptr + (uint32_t)(index * 2u),
                                (uint16_t)message[index]);
            }
            return 1;
        }
        case 24: /* evnt_timer */
            stout_write_u16(pb.intout,
                            (uint16_t)evnt_timer(intin0, intin1));
            return 1;
        case 25: {
            WORD message[8];
            WORD mx;
            WORD my;
            WORD mb;
            WORD ks;
            WORD kr;
            WORD br;
            WORD event;
            uint32_t message_ptr = stout_read_u32(pb.addrin);
            size_t index;

            event = evnt_multi(
                (UWORD)intin0, (UWORD)intin1, (UWORD)intin2, (UWORD)intin3,
                (UWORD)intin4, stout_read_s16(pb.intin + 10u),
                stout_read_s16(pb.intin + 12u), stout_read_s16(pb.intin + 14u),
                stout_read_s16(pb.intin + 16u),
                (UWORD)stout_read_s16(pb.intin + 18u),
                stout_read_s16(pb.intin + 20u), stout_read_s16(pb.intin + 22u),
                stout_read_s16(pb.intin + 24u), stout_read_s16(pb.intin + 26u),
                message, (UWORD)stout_read_s16(pb.intin + 28u),
                (UWORD)stout_read_s16(pb.intin + 30u), &mx, &my, &mb, &ks, &kr,
                &br);
            stout_write_u16(pb.intout + 0u, (uint16_t)event);
            stout_write_u16(pb.intout + 2u, (uint16_t)mx);
            stout_write_u16(pb.intout + 4u, (uint16_t)my);
            stout_write_u16(pb.intout + 6u, (uint16_t)mb);
            stout_write_u16(pb.intout + 8u, (uint16_t)ks);
            stout_write_u16(pb.intout + 10u, (uint16_t)kr);
            stout_write_u16(pb.intout + 12u, (uint16_t)br);
            for (index = 0u; index < 8u; ++index) {
                stout_write_u16(message_ptr + (uint32_t)(index * 2u),
                                (uint16_t)message[index]);
            }
            return 1;
        }
        case 52: {
            char alert[256];
            uint32_t string_ptr = stout_read_u32(pb.addrin);

            stout_read_string(string_ptr, alert, sizeof(alert));
            if (g_stout.trace_traps != 0)
                fprintf(stderr, "stout: AES alert=%s\n", alert);
            stout_write_u16(pb.intout, (uint16_t)form_alert(intin0, alert));
            return 1;
        }
        case 77: {
            WORD handle = graf_handle(NULL, NULL, NULL, NULL);

            stout_write_u16(pb.intout + 0u, (uint16_t)handle);
            /* Atari high resolution uses the 8x16 system character cell.
             * Keep these guest metrics out of hosted AES: native resources
             * use the established hosted 6x8 coordinate grid. */
            stout_write_u16(pb.intout + 2u, 8u);
            stout_write_u16(pb.intout + 4u, 16u);
            stout_write_u16(pb.intout + 6u, 8u);
            stout_write_u16(pb.intout + 8u, 16u);
            return 1;
        }
        case 78: {
            uint32_t form_ptr = stout_read_u32(pb.addrin);

            if (g_stout.trace_traps != 0)
                fprintf(stderr, "stout: AES graf_mouse mode=%d form=0x%08x\n",
                        intin0, form_ptr);
            stout_write_u16(
                pb.intout,
                (uint16_t)graf_mouse(intin0, (MFORM *)(uintptr_t)form_ptr));
            return 1;
        }
        case 100:
            stout_write_u16(pb.intout,
                            (uint16_t)wind_create((UWORD)intin0, intin1, intin2,
                                                  intin3, intin4));
            return 1;
        case 101:
            stout_write_u16(
                pb.intout,
                (uint16_t)wind_open(intin0, intin1, intin2, intin3, intin4));
            return 1;
        case 102:
            stout_write_u16(pb.intout, (uint16_t)wind_close(intin0));
            return 1;
        case 103:
            stout_write_u16(pb.intout, (uint16_t)wind_delete(intin0));
            return 1;
        case 104: {
            WORD w1;
            WORD w2;
            WORD w3;
            WORD w4;
            WORD status;

            if (intin0 == 0 && (intin1 == 4 || intin1 == 5)) {
                w1 = 0;
                w2 = 0;
                w3 = STOUT_SCREEN_WIDTH;
                w4 = STOUT_SCREEN_HEIGHT;
                status = 1;
            } else {
                status = wind_get(intin0, stout_wind_field(intin1), &w1, &w2,
                                  &w3, &w4);
            }

            stout_write_u16(pb.intout + 0u, (uint16_t)status);
            stout_write_u16(pb.intout + 2u, (uint16_t)w1);
            stout_write_u16(pb.intout + 4u, (uint16_t)w2);
            stout_write_u16(pb.intout + 6u, (uint16_t)w3);
            stout_write_u16(pb.intout + 8u, (uint16_t)w4);
            return 1;
        }
        case 105:
            if (intin1 == WF_NAME || intin1 == WF_INFO) {
                char text[256];
                uint32_t text_ptr = ((uint32_t)(uint16_t)intin2 << 16) |
                                    (uint32_t)(uint16_t)intin3;

                stout_read_string(text_ptr, text, sizeof(text));
                stout_write_u16(pb.intout,
                                (uint16_t)wind_set_str(intin0, intin1, text));
            } else {
                stout_write_u16(pb.intout, (uint16_t)wind_set(
                                               intin0, stout_wind_field(intin1),
                                               intin2, intin3, intin4,
                                               stout_read_s16(pb.intin + 10u)));
            }
            return 1;
        case 107:
            stout_write_u16(pb.intout, (uint16_t)wind_update(intin0));
            return 1;
        case 108: {
            WORD x;
            WORD y;
            WORD w;
            WORD h;
            WORD status =
                wind_calc(intin0, (UWORD)intin1, intin2, intin3, intin4,
                          stout_read_s16(pb.intin + 10u), &x, &y, &w, &h);

            stout_write_u16(pb.intout + 0u, (uint16_t)status);
            stout_write_u16(pb.intout + 2u, (uint16_t)x);
            stout_write_u16(pb.intout + 4u, (uint16_t)y);
            stout_write_u16(pb.intout + 6u, (uint16_t)w);
            stout_write_u16(pb.intout + 8u, (uint16_t)h);
            return 1;
        }
        case 110: { /* rsrc_load */
            uint32_t filename = stout_read_u32(pb.addrin);
            WORD status = (WORD)stout_resource_load(filename);

            if (g_stout.trace_traps != 0) {
                char path[GEM_OS_PATH_MAX];

                stout_read_string(filename, path, sizeof(path));
                fprintf(stderr, "stout: AES rsrc_load path='%s' result=%d\n",
                        path, status);
            }
            if (status != 0 && stout_valid_range(pb.global, 18u)) {
                uint32_t trees =
                    g_stout.resource_address +
                    stout_read_u16(g_stout.resource_address + 18u);

                stout_write_u32(pb.global + 10u, trees);
                stout_write_u32(pb.global + 14u,
                                g_stout.resource_address);
            }
            stout_write_u16(pb.intout, (uint16_t)status);
            return 1;
        }
        case 111: /* rsrc_free */
            g_stout.resource_address = 0u;
            g_stout.resource_size = 0u;
            if (stout_valid_range(pb.global, 18u)) {
                stout_write_u32(pb.global + 10u, 0u);
                stout_write_u32(pb.global + 14u, 0u);
            }
            stout_write_u16(pb.intout, 1u);
            return 1;
        case 112: { /* rsrc_gaddr */
            uint32_t address = stout_resource_address_for(intin0, intin1);

            if (g_stout.trace_traps != 0)
                fprintf(stderr,
                        "stout: AES rsrc_gaddr type=%d index=%d "
                        "address=0x%08x\n",
                        intin0, intin1, address);
            stout_write_u16(pb.intout, address != 0u ? 1u : 0u);
            if (stout_valid_range(pb.addrout, 4u))
                stout_write_u32(pb.addrout, address);
            return 1;
        }
        case 113: /* rsrc_saddr */
            stout_write_u16(pb.intout,
                            g_stout.resource_address != 0u ? 1u : 0u);
            return 1;
        case 114: { /* rsrc_obfix */
            uint32_t tree = stout_read_u32(pb.addrin);
            uint32_t object;
            int loaded = tree >= g_stout.resource_address &&
                         tree < g_stout.resource_address +
                                    g_stout.resource_size;
            int status = 0;

            if (intin0 >= 0 &&
                (uint32_t)(uint16_t)intin0 <=
                    (UINT32_MAX - tree) / stout_object_size) {
                object = tree +
                         (uint32_t)(uint16_t)intin0 * stout_object_size;
                status = stout_valid_range(object, stout_object_size);
                if (status != 0 && !loaded)
                    stout_resource_fix_coordinates(object);
            }

            stout_write_u16(pb.intout, status != 0 ? 1u : 0u);
            return 1;
        }
        case 124: { /* shel_find */
            uint32_t path_address = stout_read_u32(pb.addrin);
            uint8_t *data = NULL;
            size_t size = 0u;
            WORD status = 0;

            if (stout_guest_string(path_address) != NULL &&
                stout_gemdos_read_file(path_address, &data, &size)) {
                status = 1;
            }
            free(data);
            stout_write_u16(pb.intout, (uint16_t)status);
            return 1;
        }
        default:
            fprintf(stderr, "stout: unsupported AES opcode %u\n", opcode);
            stout_write_u16(pb.intout, 0u);
            return 1;
    }
}

static void stout_copy_words_from_guest(uint32_t address, WORD *words,
                                        size_t count)
{
    size_t index;

    for (index = 0u; index < count; ++index) {
        words[index] = stout_read_s16(address + (uint32_t)(index * 2u));
    }
}

static void stout_copy_words_to_guest(uint32_t address, const WORD *words,
                                      size_t count)
{
    size_t index;

    for (index = 0u; index < count; ++index) {
        stout_write_u16(address + (uint32_t)(index * 2u),
                        (uint16_t)words[index]);
    }
}

typedef struct stout_mfdb_bridge {
    MFDB form;
    uint32_t guest_address;
    size_t bytes;
    uint16_t *storage;
} stout_mfdb_bridge_t;

static int stout_decode_mfdb(uint32_t address, stout_mfdb_bridge_t *bridge)
{
    uint32_t pixels;
    size_t words;
    size_t index;

    memset(bridge, 0, sizeof(*bridge));
    if (!stout_valid_range(address, 20u))
        return 0;
    pixels = stout_read_u32(address);
    bridge->form.fd_w = stout_read_s16(address + 4u);
    bridge->form.fd_h = stout_read_s16(address + 6u);
    bridge->form.fd_wdwidth = stout_read_s16(address + 8u);
    bridge->form.fd_stand = stout_read_s16(address + 10u);
    bridge->form.fd_nplanes = stout_read_s16(address + 12u);
    bridge->form.fd_r1 = stout_read_s16(address + 14u);
    bridge->form.fd_r2 = stout_read_s16(address + 16u);
    bridge->form.fd_r3 = stout_read_s16(address + 18u);
    if (pixels == 0u)
        return 1;
    if (bridge->form.fd_wdwidth <= 0 || bridge->form.fd_h <= 0 ||
        bridge->form.fd_nplanes <= 0)
        return 0;
    words = (size_t)bridge->form.fd_wdwidth * (size_t)bridge->form.fd_h *
            (size_t)bridge->form.fd_nplanes;
    if (words > STOUT_RAM_SIZE / 2u || !stout_valid_range(pixels, words * 2u))
        return 0;
    bridge->storage = (uint16_t *)malloc(words * sizeof(uint16_t));
    if (bridge->storage == NULL)
        return 0;
    bridge->guest_address = pixels;
    bridge->bytes = words * 2u;
    bridge->form.fd_addr = bridge->storage;
    for (index = 0u; index < words; ++index)
        bridge->storage[index] = stout_read_u16(pixels + (uint32_t)index * 2u);
    return 1;
}

static void stout_store_mfdb(const stout_mfdb_bridge_t *bridge)
{
    const uint16_t *pixels = (const uint16_t *)bridge->form.fd_addr;
    size_t words = bridge->bytes / 2u;
    size_t index;

    if (bridge->guest_address == 0u || pixels == NULL)
        return;
    for (index = 0u; index < words; ++index)
        stout_write_u16(bridge->guest_address + (uint32_t)index * 2u,
                        pixels[index]);
}

static void stout_free_mfdb(stout_mfdb_bridge_t *bridge)
{
    free(bridge->storage);
    bridge->storage = NULL;
}

static int stout_handle_vdi(void)
{
    stout_vdipb_t pb;
    uint32_t d0 = stout_get_reg(M68K_REG_D0);
    uint32_t d1 = stout_get_reg(M68K_REG_D1);
    WORD control[12] = {0};
    WORD intin[64] = {0};
    WORD ptsin[256] = {0};
    WORD intout[64] = {0};
    WORD handle;
    WORD status;

    if ((uint16_t)d0 != VDI_TRAP_MAGIC || !stout_valid_range(d1, sizeof(pb))) {
        return 0;
    }

    pb.control = stout_read_u32(d1);
    pb.intin = stout_read_u32(d1 + 4u);
    pb.ptsin = stout_read_u32(d1 + 8u);
    pb.intout = stout_read_u32(d1 + 12u);
    pb.ptsout = stout_read_u32(d1 + 16u);

    if (!stout_valid_range(pb.control, sizeof(control)))
        return 0;
    stout_copy_words_from_guest(pb.control, control, 12u);
    if (g_stout.trace_traps != 0)
        fprintf(stderr, "stout: VDI opcode=%d subopcode=%d handle=%d\n",
                control[0], control[5], control[6]);
    if (control[3] < 0 || control[3] > 64 || control[1] < 0 ||
        control[1] > 128 ||
        !stout_valid_range(pb.intin, (size_t)control[3] * 2u) ||
        !stout_valid_range(pb.ptsin, (size_t)control[1] * 4u)) {
        fprintf(stderr, "stout: invalid VDI parameter counts or addresses\n");
        return 0;
    }
    handle = control[6];

    if (control[3] > 0) {
        stout_copy_words_from_guest(pb.intin, intin, (size_t)control[3]);
    }
    if (control[1] > 0) {
        stout_copy_words_from_guest(pb.ptsin, ptsin, (size_t)control[1] * 2u);
    }

    switch (control[0]) {
        case 6:
            v_pline(handle, control[1], ptsin);
            return 1;
        case 8: { /* v_gtext */
            char text[65];
            WORD index;

            for (index = 0; index < control[3] && index < 64; ++index)
                text[index] = (char)(intin[index] & 0xff);
            text[index] = '\0';
            v_gtext(handle, ptsin[0], ptsin[1], (const BYTE *)text);
            return 1;
        }
        case 9:
            v_fillarea(handle, control[1], ptsin);
            return 1;
        case 11:
            if (control[5] == 1) {
                v_bar(handle, ptsin);
                return 1;
            }
            fprintf(stderr, "stout: unsupported VDI GDP subopcode %d\n",
                    control[5]);
            return 0;
        case 12: { /* vst_height */
            WORD char_width;
            WORD char_height;
            WORD cell_width;
            WORD cell_height;

            status = vst_height(handle, ptsin[1], &char_width, &char_height,
                                &cell_width, &cell_height);
            stout_write_u16(pb.intout, (uint16_t)status);
            stout_write_u16(pb.ptsout + 0u, (uint16_t)char_width);
            stout_write_u16(pb.ptsout + 2u, (uint16_t)char_height);
            stout_write_u16(pb.ptsout + 4u, (uint16_t)cell_width);
            stout_write_u16(pb.ptsout + 6u, (uint16_t)cell_height);
            return 1;
        }
        case 15:
            status = vsl_type(handle, intin[0]);
            stout_write_u16(pb.intout, (uint16_t)status);
            return 1;
        case 16:
            status = vsl_width(handle, ptsin[0]);
            stout_write_u16(pb.intout, (uint16_t)status);
            return 1;
        case 17:
            vsl_color(handle, intin[0]);
            stout_write_u16(pb.intout, (uint16_t)intin[0]);
            return 1;
        case 23:
            status = vsf_interior(handle, intin[0]);
            stout_write_u16(pb.intout, (uint16_t)status);
            return 1;
        case 24:
            status = vsf_style(handle, intin[0]);
            stout_write_u16(pb.intout, (uint16_t)status);
            return 1;
        case 25:
            vsf_color(handle, intin[0]);
            stout_write_u16(pb.intout, (uint16_t)intin[0]);
            return 1;
        case 32:
            status = vswr_mode(handle, intin[0]);
            stout_write_u16(pb.intout, (uint16_t)status);
            return 1;
        case 38: { /* vqt_attributes */
            WORD attributes[10] = {0};

            status = vqt_attributes(handle, attributes);
            stout_copy_words_to_guest(pb.intout, attributes, 6u);
            stout_copy_words_to_guest(pb.ptsout, attributes + 6u, 4u);
            return status != 0;
        }
        case 39:
            status = vst_alignment(handle, intin[0], intin[1]);
            stout_write_u16(pb.intout, (uint16_t)status);
            return 1;
        case 100:
            v_opnvwk(intin, &handle, intout);
            g_stout.vdi_handle = handle;
            if (g_stout.trace_traps != 0)
                fprintf(stderr,
                        "stout: VDI open handle=%d size=%dx%d colors=%d\n",
                        handle, intout[0] + 1, intout[1] + 1, intout[13]);
            intout[0] = STOUT_SCREEN_WIDTH - 1u;
            intout[1] = STOUT_SCREEN_HEIGHT - 1u;
            control[6] = handle;
            stout_copy_words_to_guest(pb.control, control, 12u);
            stout_copy_words_to_guest(pb.intout, intout, 57u);
            stout_copy_words_to_guest(pb.ptsout, intout + 45u, 12u);
            return 1;
        case 101:
            v_clsvwk(handle);
            if (g_stout.vdi_handle == handle)
                g_stout.vdi_handle = 0;
            return 1;
        case 102:
            status = vq_extnd(handle, intin[0], intout);
            intout[0] = STOUT_SCREEN_WIDTH - 1u;
            intout[1] = STOUT_SCREEN_HEIGHT - 1u;
            stout_write_u16(pb.intout, (uint16_t)status);
            stout_copy_words_to_guest(pb.intout, intout, 45u);
            stout_copy_words_to_guest(pb.ptsout, intout + 45u, 12u);
            return 1;
        case 104:
            status = vsf_perimeter(handle, intin[0]);
            stout_write_u16(pb.intout, (uint16_t)status);
            return 1;
        case 106:
            status = vst_effects(handle, intin[0]);
            stout_write_u16(pb.intout, (uint16_t)status);
            return 1;
        case 109:   /* vro_cpyfm */
        case 110:   /* vr_trnfm */
        case 121: { /* vrt_cpyfm */
            stout_mfdb_bridge_t source = {0};
            stout_mfdb_bridge_t destination = {0};
            uint32_t source_address = ((uint32_t)(uint16_t)control[7] << 16) |
                                      (uint32_t)(uint16_t)control[8];
            uint32_t destination_address =
                ((uint32_t)(uint16_t)control[9] << 16) |
                (uint32_t)(uint16_t)control[10];

            if (!stout_decode_mfdb(source_address, &source) ||
                !stout_decode_mfdb(destination_address, &destination)) {
                stout_free_mfdb(&source);
                stout_free_mfdb(&destination);
                return 0;
            }
            if (source.guest_address != 0u &&
                source.guest_address == destination.guest_address) {
                stout_free_mfdb(&destination);
                destination.form.fd_addr = source.form.fd_addr;
            }
            if (control[0] == 109) {
                vro_cpyfm(handle, intin[0], ptsin, &source.form,
                          &destination.form);
            } else if (control[0] == 110) {
                status = vr_trnfm(handle, &source.form, &destination.form);
                stout_write_u16(pb.intout, (uint16_t)status);
            } else {
                vrt_cpyfm(handle, intin[0], ptsin, &source.form,
                          &destination.form, intin + 1);
            }
            stout_store_mfdb(&destination);
            stout_free_mfdb(&destination);
            stout_free_mfdb(&source);
            return 1;
        }
        case 114:
            v_bar(handle, ptsin);
            return 1;
        case 122: /* v_show_c */
            v_show_c(handle, intin[0]);
            return 1;
        case 123: /* v_hide_c */
            v_hide_c(handle);
            return 1;
        case 124: { /* vq_mouse */
            WORD buttons;
            WORD x;
            WORD y;

            vq_mouse(handle, &buttons, &x, &y);
            stout_write_u16(pb.intout, (uint16_t)buttons);
            stout_write_u16(pb.ptsout, (uint16_t)x);
            stout_write_u16(pb.ptsout + 2u, (uint16_t)y);
            return 1;
        }
        case 125: /* vex_butv */
        case 126: /* vex_motv */
        case 127: /* vex_curv */
            /*
             * A hosted callback cannot execute a 68k function directly.
             * Accept the registration and report no previous guest vector;
             * AES input remains available through evnt_multi/vq_mouse.
             */
            control[9] = 0;
            control[10] = 0;
            stout_copy_words_to_guest(pb.control, control, 12u);
            return 1;
        case 128: { /* vq_key_s */
            WORD keys;

            status = vq_key_s(handle, &keys);
            stout_write_u16(pb.intout, (uint16_t)keys);
            return status != 0;
        }
        case 129:
            vs_clip(handle, intin[0], ptsin);
            return 1;
        default:
            fprintf(stderr, "stout: unsupported VDI opcode %d\n", control[0]);
            control[2] = 0;
            control[4] = 0;
            stout_copy_words_to_guest(pb.control, control, 12u);
            return 1;
    }
}

int stout_handle_trap(int trap)
{
    if (g_stout.trace_traps != 0) {
        uint32_t sp = stout_get_reg(M68K_REG_SP);
        uint16_t function =
            stout_valid_range(sp, 2u) ? stout_read_u16(sp) : 0xffffu;

        fprintf(stderr,
                "stout: trap #%d function=%u d0=0x%08x d1=0x%08x "
                "pc=0x%08x\n",
                trap, function, stout_get_reg(M68K_REG_D0),
                stout_get_reg(M68K_REG_D1), stout_get_reg(M68K_REG_PC));
    }

    switch (trap) {
        case STOUT_GEMDOS_TRAP:
            return stout_handle_gemdos();
        case STOUT_AES_VDI_TRAP:
            /* vq_gdos uses the trap directly instead of a VDI parameter
             * block.  Report the ROM-compatible "no GDOS" result. */
            if ((uint16_t)stout_get_reg(M68K_REG_D0) == 0xfffeu) {
                stout_set_reg(M68K_REG_D0, 0xfffffffeu);
                return 1;
            }
            if (stout_handle_aes()) {
                return 1;
            }
            if (stout_handle_vdi()) {
                return 1;
            }
            return 0;
        case STOUT_LINEA_TRAP:
            return stout_handle_linea();
        case STOUT_BIOS_TRAP:
            return stout_handle_bios();
        case STOUT_XBIOS_TRAP:
            return stout_handle_xbios();
        case STOUT_EXIT_TRAP:
            if (stout_get_reg(M68K_REG_PC) == g_stout.stop_address + 2u) {
                stout_stop_with_code((int)stout_get_reg(M68K_REG_D0));
                return 1;
            }
            return 0;
        default:
            fprintf(stderr, "stout: unsupported trap #%d\n", trap);
            return 0;
    }
}

int stout_handle_illegal(int opcode)
{
    fprintf(stderr, "stout: illegal opcode 0x%04x at pc=0x%08x sp=0x%08x\n",
            opcode, stout_get_reg(M68K_REG_PC), stout_get_reg(M68K_REG_SP));
    stout_stop_with_code(1);
    return 1;
}
