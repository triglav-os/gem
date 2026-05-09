/*
 * Runs simple Atari ST `.PRG` and `.APP` binaries on top of Musashi and
 * forwards a small GEM-visible trap surface into the hosted AES/VDI
 * implementation. The initial goal is a narrow execution slice that is
 * sufficient for classic GEM demos such as `DEMO.PRG`.
 *
 * MIT License (see: LICENSE)
 * Copyright (C) 2026 tomaz stih
 */

#include <gem.h>
#include <gem/aes.h>
#include <gem/vdi.h>

#include <m68k.h>

#include "prg/prg.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define STOUT_RAM_SIZE 0x01000000u
#define STOUT_LOAD_ADDRESS 0x00010000u
#define STOUT_STACK_TOP 0x000ff000u
#define STOUT_EXIT_TRAP 15
#define STOUT_MAX_CYCLES 5000000

#define AES_TRAP_MAGIC 200u
#define VDI_TRAP_MAGIC 115u

typedef struct stout_aespb {
    uint32_t control;
    uint32_t global;
    uint32_t intin;
    uint32_t intout;
    uint32_t addrin;
    uint32_t addrout;
} stout_aespb_t;

typedef struct stout_vdipb {
    uint32_t control;
    uint32_t intin;
    uint32_t ptsin;
    uint32_t intout;
    uint32_t ptsout;
} stout_vdipb_t;

typedef struct stout_state {
    uint8_t *ram;
    prg_image_t program;
    uint32_t load_address;
    uint32_t stack_top;
    uint32_t stop_address;
    int running;
    int exit_code;
    int trace_traps;
} stout_state_t;

static stout_state_t g_stout;

static uint16_t stout_read_u16(uint32_t address)
{
    return (uint16_t) (((uint16_t) g_stout.ram[address] << 8)
        | (uint16_t) g_stout.ram[address + 1u]);
}

static uint32_t stout_read_u32(uint32_t address)
{
    return ((uint32_t) g_stout.ram[address] << 24)
        | ((uint32_t) g_stout.ram[address + 1u] << 16)
        | ((uint32_t) g_stout.ram[address + 2u] << 8)
        | (uint32_t) g_stout.ram[address + 3u];
}

static int16_t stout_read_s16(uint32_t address)
{
    return (int16_t) stout_read_u16(address);
}

static void stout_write_u16(uint32_t address, uint16_t value)
{
    g_stout.ram[address] = (uint8_t) ((value >> 8) & 0xffu);
    g_stout.ram[address + 1u] = (uint8_t) (value & 0xffu);
}

static void stout_write_u32(uint32_t address, uint32_t value)
{
    g_stout.ram[address] = (uint8_t) ((value >> 24) & 0xffu);
    g_stout.ram[address + 1u] = (uint8_t) ((value >> 16) & 0xffu);
    g_stout.ram[address + 2u] = (uint8_t) ((value >> 8) & 0xffu);
    g_stout.ram[address + 3u] = (uint8_t) (value & 0xffu);
}

static int stout_valid_range(uint32_t address, size_t size)
{
    if (address >= STOUT_RAM_SIZE) {
        return 0;
    }
    if (size > (size_t) STOUT_RAM_SIZE - (size_t) address) {
        return 0;
    }
    return 1;
}

static void stout_read_string(uint32_t address, char *buffer, size_t size)
{
    size_t index;

    if (buffer == NULL || size == 0u) {
        return;
    }

    for (index = 0u; index + 1u < size; ++index) {
        if (!stout_valid_range(address + (uint32_t) index, 1u)) {
            break;
        }
        buffer[index] = (char) g_stout.ram[address + (uint32_t) index];
        if (buffer[index] == '\0') {
            return;
        }
    }

    buffer[index] = '\0';
}

static uint32_t stout_get_reg(m68k_register_t reg)
{
    return m68k_get_reg(NULL, reg);
}

static void stout_set_reg(m68k_register_t reg, uint32_t value)
{
    m68k_set_reg(reg, value);
}

static void stout_stop_with_code(int code)
{
    g_stout.running = 0;
    g_stout.exit_code = code;
}

static uint32_t stout_push_return_trap(uint32_t sp)
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
    case 4:
        stout_set_reg(M68K_REG_D0, 2u);
        stout_set_reg(M68K_REG_SP, sp + 2u);
        return 1;
    default:
        fprintf(stderr, "stout: unsupported XBIOS opcode %u\n", opcode);
        return 0;
    }
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

    if (d0 != AES_TRAP_MAGIC || !stout_valid_range(d1, sizeof(pb))) {
        return 0;
    }

    pb.control = stout_read_u32(d1);
    pb.global = stout_read_u32(d1 + 4u);
    pb.intin = stout_read_u32(d1 + 8u);
    pb.intout = stout_read_u32(d1 + 12u);
    pb.addrin = stout_read_u32(d1 + 16u);
    pb.addrout = stout_read_u32(d1 + 20u);

    opcode = stout_read_u16(pb.control);
    intin0 = stout_read_s16(pb.intin + 0u);
    intin1 = stout_read_s16(pb.intin + 2u);
    intin2 = stout_read_s16(pb.intin + 4u);
    intin3 = stout_read_s16(pb.intin + 6u);
    intin4 = stout_read_s16(pb.intin + 8u);

    switch (opcode) {
    case 10:
        stout_write_u16(pb.intout, (uint16_t) appl_init());
        return 1;
    case 19:
        stout_write_u16(pb.intout, (uint16_t) appl_exit());
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

        event = evnt_multi((UWORD) intin0,
            (UWORD) intin1, (UWORD) intin2, (UWORD) intin3,
            (UWORD) intin4,
            stout_read_s16(pb.intin + 10u),
            stout_read_s16(pb.intin + 12u),
            stout_read_s16(pb.intin + 14u),
            stout_read_s16(pb.intin + 16u),
            (UWORD) stout_read_s16(pb.intin + 18u),
            stout_read_s16(pb.intin + 20u),
            stout_read_s16(pb.intin + 22u),
            stout_read_s16(pb.intin + 24u),
            stout_read_s16(pb.intin + 26u),
            message,
            (UWORD) stout_read_s16(pb.intin + 28u),
            (UWORD) stout_read_s16(pb.intin + 30u),
            &mx, &my, &mb, &ks, &kr, &br);
        stout_write_u16(pb.intout + 0u, (uint16_t) event);
        stout_write_u16(pb.intout + 2u, (uint16_t) mx);
        stout_write_u16(pb.intout + 4u, (uint16_t) my);
        stout_write_u16(pb.intout + 6u, (uint16_t) mb);
        stout_write_u16(pb.intout + 8u, (uint16_t) ks);
        stout_write_u16(pb.intout + 10u, (uint16_t) kr);
        stout_write_u16(pb.intout + 12u, (uint16_t) br);
        for (index = 0u; index < 8u; ++index) {
            stout_write_u16(message_ptr + (uint32_t) (index * 2u),
                (uint16_t) message[index]);
        }
        return 1;
    }
    case 52: {
        char alert[256];
        uint32_t string_ptr = stout_read_u32(pb.addrin);

        stout_read_string(string_ptr, alert, sizeof(alert));
        stout_write_u16(pb.intout, (uint16_t) form_alert(intin0, alert));
        return 1;
    }
    case 77: {
        WORD wchar;
        WORD hchar;
        WORD wbox;
        WORD hbox;
        WORD handle = graf_handle(&wchar, &hchar, &wbox, &hbox);

        stout_write_u16(pb.intout + 0u, (uint16_t) handle);
        stout_write_u16(pb.intout + 2u, (uint16_t) wchar);
        stout_write_u16(pb.intout + 4u, (uint16_t) hchar);
        stout_write_u16(pb.intout + 6u, (uint16_t) wbox);
        stout_write_u16(pb.intout + 8u, (uint16_t) hbox);
        return 1;
    }
    case 78: {
        uint32_t form_ptr = stout_read_u32(pb.addrin);

        stout_write_u16(pb.intout, (uint16_t) graf_mouse(intin0,
            (MFORM *) (uintptr_t) form_ptr));
        return 1;
    }
    case 100:
        stout_write_u16(pb.intout, (uint16_t) wind_create((UWORD) intin0,
            intin1, intin2, intin3, intin4));
        return 1;
    case 101:
        stout_write_u16(pb.intout, (uint16_t) wind_open(intin0,
            intin1, intin2, intin3, intin4));
        return 1;
    case 102:
        stout_write_u16(pb.intout, (uint16_t) wind_close(intin0));
        return 1;
    case 103:
        stout_write_u16(pb.intout, (uint16_t) wind_delete(intin0));
        return 1;
    case 104: {
        WORD w1;
        WORD w2;
        WORD w3;
        WORD w4;
        WORD status = wind_get(intin0, intin1, &w1, &w2, &w3, &w4);

        stout_write_u16(pb.intout + 0u, (uint16_t) status);
        stout_write_u16(pb.intout + 2u, (uint16_t) w1);
        stout_write_u16(pb.intout + 4u, (uint16_t) w2);
        stout_write_u16(pb.intout + 6u, (uint16_t) w3);
        stout_write_u16(pb.intout + 8u, (uint16_t) w4);
        return 1;
    }
    case 105:
        if (intin1 == WF_NAME || intin1 == WF_INFO) {
            char text[256];
            uint32_t text_ptr =
                ((uint32_t) (uint16_t) intin2 << 16)
                | (uint32_t) (uint16_t) intin3;

            stout_read_string(text_ptr, text, sizeof(text));
            stout_write_u16(pb.intout, (uint16_t) wind_set_str(intin0,
                intin1, text));
        } else {
            stout_write_u16(pb.intout, (uint16_t) wind_set(intin0, intin1,
                intin2, intin3, intin4, stout_read_s16(pb.intin + 10u)));
        }
        return 1;
    case 107:
        stout_write_u16(pb.intout, (uint16_t) wind_update(intin0));
        return 1;
    default:
        fprintf(stderr, "stout: unsupported AES opcode %u\n", opcode);
        return 0;
    }
}

static void stout_copy_words_from_guest(uint32_t address, WORD *words,
    size_t count)
{
    size_t index;

    for (index = 0u; index < count; ++index) {
        words[index] = stout_read_s16(address + (uint32_t) (index * 2u));
    }
}

static void stout_copy_words_to_guest(uint32_t address, const WORD *words,
    size_t count)
{
    size_t index;

    for (index = 0u; index < count; ++index) {
        stout_write_u16(address + (uint32_t) (index * 2u),
            (uint16_t) words[index]);
    }
}

static int stout_handle_vdi(void)
{
    stout_vdipb_t pb;
    uint32_t d0 = stout_get_reg(M68K_REG_D0);
    uint32_t d1 = stout_get_reg(M68K_REG_D1);
    WORD control[12];
    WORD intin[64];
    WORD ptsin[256];
    WORD intout[64];
    WORD handle;
    WORD status;

    if (d0 != VDI_TRAP_MAGIC || !stout_valid_range(d1, sizeof(pb))) {
        return 0;
    }

    pb.control = stout_read_u32(d1);
    pb.intin = stout_read_u32(d1 + 4u);
    pb.ptsin = stout_read_u32(d1 + 8u);
    pb.intout = stout_read_u32(d1 + 12u);
    pb.ptsout = stout_read_u32(d1 + 16u);

    stout_copy_words_from_guest(pb.control, control, 12u);
    handle = control[6];

    if (control[3] > 0) {
        stout_copy_words_from_guest(pb.intin, intin, (size_t) control[3]);
    }
    if (control[1] > 0) {
        stout_copy_words_from_guest(pb.ptsin, ptsin,
            (size_t) control[1] * 2u);
    }

    switch (control[0]) {
    case 6:
        v_pline(handle, control[1], ptsin);
        return 1;
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
    case 15:
        status = vsl_type(handle, intin[0]);
        stout_write_u16(pb.intout, (uint16_t) status);
        return 1;
    case 16:
        status = vsl_width(handle, ptsin[0]);
        stout_write_u16(pb.intout, (uint16_t) status);
        return 1;
    case 17:
        vsl_color(handle, intin[0]);
        stout_write_u16(pb.intout, (uint16_t) intin[0]);
        return 1;
    case 23:
        status = vsf_interior(handle, intin[0]);
        stout_write_u16(pb.intout, (uint16_t) status);
        return 1;
    case 24:
        status = vsf_style(handle, intin[0]);
        stout_write_u16(pb.intout, (uint16_t) status);
        return 1;
    case 25:
        vsf_color(handle, intin[0]);
        stout_write_u16(pb.intout, (uint16_t) intin[0]);
        return 1;
    case 32:
        status = vswr_mode(handle, intin[0]);
        stout_write_u16(pb.intout, (uint16_t) status);
        return 1;
    case 100:
        v_opnvwk(intin, &handle, intout);
        control[6] = handle;
        stout_copy_words_to_guest(pb.control, control, 12u);
        stout_copy_words_to_guest(pb.intout, intout, 57u);
        stout_copy_words_to_guest(pb.ptsout, intout + 45u, 12u);
        return 1;
    case 101:
        v_clsvwk(handle);
        return 1;
    case 104:
        status = vsf_perimeter(handle, intin[0]);
        stout_write_u16(pb.intout, (uint16_t) status);
        return 1;
    case 114:
        v_bar(handle, ptsin);
        return 1;
    case 129:
        vs_clip(handle, intin[0], ptsin);
        return 1;
    default:
        fprintf(stderr, "stout: unsupported VDI opcode %d\n", control[0]);
        return 0;
    }
}

static int stout_handle_trap(int trap)
{
    if (g_stout.trace_traps != 0) {
        fprintf(stderr, "stout: trap #%d d0=%u d1=0x%08x pc=0x%08x\n",
            trap, stout_get_reg(M68K_REG_D0), stout_get_reg(M68K_REG_D1),
            stout_get_reg(M68K_REG_PC));
    }

    switch (trap) {
    case 2:
        if (stout_handle_aes()) {
            return 1;
        }
        if (stout_handle_vdi()) {
            return 1;
        }
        return 0;
    case 14:
        return stout_handle_xbios();
    case STOUT_EXIT_TRAP:
        stout_stop_with_code((int) stout_get_reg(M68K_REG_D0));
        return 1;
    default:
        fprintf(stderr, "stout: unsupported trap #%d\n", trap);
        return 0;
    }
}

static int stout_handle_illegal(int opcode)
{
    fprintf(stderr, "stout: illegal opcode 0x%04x at pc=0x%08x\n",
        opcode, stout_get_reg(M68K_REG_PC));
    stout_stop_with_code(1);
    return 1;
}

unsigned int m68k_read_memory_8(unsigned int address)
{
    if (!stout_valid_range(address, 1u)) {
        return 0u;
    }
    return g_stout.ram[address];
}

unsigned int m68k_read_memory_16(unsigned int address)
{
    if (!stout_valid_range(address, 2u)) {
        return 0u;
    }
    return stout_read_u16(address);
}

unsigned int m68k_read_memory_32(unsigned int address)
{
    if (!stout_valid_range(address, 4u)) {
        return 0u;
    }
    return stout_read_u32(address);
}

unsigned int m68k_read_immediate_16(unsigned int address)
{
    return m68k_read_memory_16(address);
}

unsigned int m68k_read_immediate_32(unsigned int address)
{
    return m68k_read_memory_32(address);
}

unsigned int m68k_read_pcrelative_8(unsigned int address)
{
    return m68k_read_memory_8(address);
}

unsigned int m68k_read_pcrelative_16(unsigned int address)
{
    return m68k_read_memory_16(address);
}

unsigned int m68k_read_pcrelative_32(unsigned int address)
{
    return m68k_read_memory_32(address);
}

unsigned int m68k_read_disassembler_8(unsigned int address)
{
    return m68k_read_memory_8(address);
}

unsigned int m68k_read_disassembler_16(unsigned int address)
{
    return m68k_read_memory_16(address);
}

unsigned int m68k_read_disassembler_32(unsigned int address)
{
    return m68k_read_memory_32(address);
}

void m68k_write_memory_8(unsigned int address, unsigned int value)
{
    if (!stout_valid_range(address, 1u)) {
        return;
    }
    g_stout.ram[address] = (uint8_t) (value & 0xffu);
}

void m68k_write_memory_16(unsigned int address, unsigned int value)
{
    if (!stout_valid_range(address, 2u)) {
        return;
    }
    stout_write_u16(address, (uint16_t) value);
}

void m68k_write_memory_32(unsigned int address, unsigned int value)
{
    if (!stout_valid_range(address, 4u)) {
        return;
    }
    stout_write_u32(address, value);
}

void m68k_write_memory_32_pd(unsigned int address, unsigned int value)
{
    m68k_write_memory_16(address + 2u, (value >> 16) & 0xffffu);
    m68k_write_memory_16(address, value & 0xffffu);
}

static int stout_find_entry(uint32_t *entry_out)
{
    static const char *const names[] = {
        "main",
        "_main",
        "__main"
    };
    size_t index;

    for (index = 0u; index < sizeof(names) / sizeof(names[0]); ++index) {
        const prg_symbol_t *symbol = prg_find_symbol(&g_stout.program,
            names[index]);

        if (symbol != NULL) {
            *entry_out = g_stout.load_address + symbol->value;
            return 1;
        }
    }

    *entry_out = g_stout.load_address;
    return 1;
}

static int stout_load_program(const char *path)
{
    char error_text[PRG_ERROR_LENGTH];

    prg_image_init(&g_stout.program);
    if (!prg_read_file(path, g_stout.load_address, &g_stout.program,
            error_text, sizeof(error_text))) {
        fprintf(stderr, "stout: %s\n", error_text);
        return 0;
    }

    if (!stout_valid_range(g_stout.load_address, g_stout.program.image_size)) {
        fprintf(stderr, "stout: program does not fit guest RAM\n");
        return 0;
    }

    memcpy(g_stout.ram + g_stout.load_address, g_stout.program.image,
        g_stout.program.image_size);
    return 1;
}

static int stout_boot(const char *path)
{
    uint32_t entry;
    uint32_t sp;

    memset(&g_stout, 0, sizeof(g_stout));
    g_stout.ram = (uint8_t *) calloc(1u, STOUT_RAM_SIZE);
    if (g_stout.ram == NULL) {
        fprintf(stderr, "stout: out of memory\n");
        return 0;
    }

    g_stout.load_address = STOUT_LOAD_ADDRESS;
    g_stout.stack_top = STOUT_STACK_TOP;
    g_stout.stop_address = 0x00008000u;
    g_stout.trace_traps = (getenv("STOUT_TRACE") != NULL);

    stout_write_u16(g_stout.stop_address, (uint16_t) (0x4e40u | STOUT_EXIT_TRAP));

    if (!stout_load_program(path)) {
        return 0;
    }
    if (!stout_find_entry(&entry)) {
        fprintf(stderr, "stout: failed to choose an entry point\n");
        return 0;
    }

    m68k_init();
    m68k_set_cpu_type(M68K_CPU_TYPE_68000);
    m68k_set_trap_instr_callback(stout_handle_trap);
    m68k_set_illg_instr_callback(stout_handle_illegal);
    m68k_pulse_reset();

    sp = stout_push_return_trap(g_stout.stack_top);
    stout_set_reg(M68K_REG_SP, sp);
    stout_set_reg(M68K_REG_A7, sp);
    stout_set_reg(M68K_REG_PC, entry);
    stout_set_reg(M68K_REG_SR, 0u);

    g_stout.running = 1;
    return 1;
}

static int stout_run(void)
{
    long total_cycles = 0;

    while (g_stout.running != 0 && total_cycles < STOUT_MAX_CYCLES) {
        total_cycles += m68k_execute(1000);
    }

    if (g_stout.running != 0) {
        fprintf(stderr, "stout: execution limit reached\n");
        return 1;
    }

    return g_stout.exit_code;
}

static void stout_shutdown(void)
{
    prg_image_free(&g_stout.program);
    free(g_stout.ram);
    memset(&g_stout, 0, sizeof(g_stout));
}

int main(int argc, char **argv)
{
    int exit_code;

    if (argc != 2) {
        fprintf(stderr, "usage: stout <filename>\n");
        return 1;
    }

    if (!stout_boot(argv[1])) {
        stout_shutdown();
        return 1;
    }

    exit_code = stout_run();
    stout_shutdown();
    return exit_code;
}
