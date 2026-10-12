/*
 * Runs Atari PRG-family applications on a Musashi 68000 core: loads and
 * relocates the image, provides guest memory callbacks, boots the program
 * with a return trap and drives the emulation loop. System traps live in
 * libstout.
 *
 * MIT License (see: LICENSE)
 * Copyright (C) 2026 tomaz stih
 */

#include "stout.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

stout_state_t g_stout;

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
    g_stout.ram[address] = (uint8_t)(value & 0xffu);
}

void m68k_write_memory_16(unsigned int address, unsigned int value)
{
    if (!stout_valid_range(address, 2u)) {
        return;
    }
    stout_write_u16(address, (uint16_t)value);
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
    static const char *const names[] = {"main", "_main", "__main"};
    size_t index;

    for (index = 0u; index < sizeof(names) / sizeof(names[0]); ++index) {
        const prg_symbol_t *symbol =
            prg_find_symbol(&g_stout.program, names[index]);

        if (symbol != NULL) {
            *entry_out = g_stout.load_address + symbol->value;
            return 1;
        }
    }

    *entry_out = g_stout.load_address;
    return 1;
}

static void stout_write_basepage(void)
{
    uint32_t text = g_stout.load_address;
    uint32_t data = text + g_stout.program.header.text_size;
    uint32_t bss = data + g_stout.program.header.data_size;
    uint32_t command = g_stout.basepage_address + 0x80u;

    stout_write_u32(g_stout.basepage_address + 0u, g_stout.basepage_address);
    stout_write_u32(g_stout.basepage_address + 4u, STOUT_TPA_TOP);
    stout_write_u32(g_stout.basepage_address + 8u, text);
    stout_write_u32(g_stout.basepage_address + 12u,
                    g_stout.program.header.text_size);
    stout_write_u32(g_stout.basepage_address + 16u, data);
    stout_write_u32(g_stout.basepage_address + 20u,
                    g_stout.program.header.data_size);
    stout_write_u32(g_stout.basepage_address + 24u, bss);
    stout_write_u32(g_stout.basepage_address + 28u,
                    g_stout.program.header.bss_size);
    stout_write_u32(g_stout.basepage_address + 32u, g_stout.dta_address);
    stout_write_u32(g_stout.basepage_address + 36u,
                    g_stout.parent_basepage_address);
    stout_write_u32(g_stout.basepage_address + 40u, 0u); /* Reserved. */
    stout_write_u32(g_stout.basepage_address + 44u, 0u); /* No environment. */
    memset(g_stout.ram + command, 0, 128u);
    g_stout.ram[command] = (uint8_t)g_stout.command_tail_length;
    memcpy(g_stout.ram + command + 1u, g_stout.command_tail,
           g_stout.command_tail_length);
}

static int stout_load_program(const char *path)
{
    char error_text[PRG_ERROR_LENGTH];
    uint8_t *disk_data = NULL;
    size_t disk_size = 0u;
    int result;

    prg_image_init(&g_stout.program);
    if ((path[0] == 'A' || path[0] == 'a') && path[1] == ':' &&
        g_stout.disk_mounted) {
        result =
            stout_disk_read_path(&g_stout.disk, path + 2, &disk_data,
                                 &disk_size) &&
            prg_read_memory(disk_data, disk_size, g_stout.load_address,
                            &g_stout.program, error_text, sizeof(error_text));
        free(disk_data);
        disk_data = NULL;
    } else {
        result = prg_read_file(path, g_stout.load_address, &g_stout.program,
                               error_text, sizeof(error_text));
    }
    if (!result) {
        if (disk_data != NULL)
            free(disk_data);
        fprintf(stderr, "stout: %s\n", error_text);
        return 0;
    }

    if (!stout_valid_range(g_stout.load_address, g_stout.program.image_size) ||
        g_stout.program.header.bss_size > STOUT_RAM_SIZE -
                                              g_stout.load_address -
                                              g_stout.program.image_size) {
        fprintf(stderr, "stout: program does not fit guest RAM\n");
        return 0;
    }

    memcpy(g_stout.ram + g_stout.load_address, g_stout.program.image,
           g_stout.program.image_size);
    return 1;
}

static int stout_prepare_host_drive(const char *program_path,
                                    const char *disk_path)
{
    const char *configured = getenv("STOUT_DRIVE_C");
    const char *source = program_path;
    char absolute[GEM_OS_PATH_MAX];
    char current[GEM_OS_PATH_MAX];
    char resolved[GEM_OS_PATH_MAX];
    int length;

    g_stout.disk_current_path[0] = '\0';
    if (disk_path != NULL && program_path != NULL &&
        (program_path[0] == 'A' || program_path[0] == 'a') &&
        program_path[1] == ':') {
        const char *first = program_path + 2u;
        const char *last = strrchr(first, '\\');
        const char *forward = strrchr(first, '/');
        size_t directory_length;

        if (forward != NULL && (last == NULL || forward > last))
            last = forward;
        while (*first == '/' || *first == '\\')
            ++first;
        directory_length = last != NULL && last >= first
                               ? (size_t)(last - first)
                               : 0u;
        if (directory_length >= sizeof(g_stout.disk_current_path))
            return 0;
        memcpy(g_stout.disk_current_path, first, directory_length);
        g_stout.disk_current_path[directory_length] = '\0';
        for (length = 0; g_stout.disk_current_path[length] != '\0'; ++length) {
            if (g_stout.disk_current_path[length] == '\\')
                g_stout.disk_current_path[length] = '/';
        }
    }

    if (disk_path != NULL &&
        (program_path[0] == 'A' || program_path[0] == 'a') &&
        program_path[1] == ':' &&
        (configured == NULL || configured[0] == '\0')) {
        g_stout.host_drive_root[0] = '\0';
        g_stout.host_current_path[0] = '\0';
        g_stout.current_drive = 0u;
        return 1;
    }
    if (configured != NULL && configured[0] != '\0')
        source = configured;
    if (source == NULL || source[0] == '\0')
        return 0;
    if (source[0] == '/') {
        length = snprintf(absolute, sizeof(absolute), "%s", source);
    } else if (gem_os_getcwd(current, sizeof(current)) != 0) {
        length = snprintf(absolute, sizeof(absolute), "%s/%s", current, source);
    } else {
        return 0;
    }
    if (length <= 0 || (size_t)length >= sizeof(absolute))
        return 0;
    if (configured == NULL || configured[0] == '\0') {
        char *slash = strrchr(absolute, '/');

        if (slash == NULL)
            return 0;
        if (slash == absolute)
            slash[1] = '\0';
        else
            *slash = '\0';
        length = snprintf(g_stout.host_drive_root,
                          sizeof(g_stout.host_drive_root), "%s", absolute);
    } else {
        length = snprintf(g_stout.host_drive_root,
                          sizeof(g_stout.host_drive_root), "%s", absolute);
    }
    if (length <= 0 || (size_t)length >= sizeof(g_stout.host_drive_root) ||
        gem_os_mkdir_p(g_stout.host_drive_root) == 0 ||
        gem_os_resolve_under(g_stout.host_drive_root, g_stout.host_drive_root,
                             resolved, sizeof(resolved)) == 0) {
        return 0;
    }
    memcpy(g_stout.host_drive_root, resolved, strlen(resolved) + 1u);
    g_stout.host_current_path[0] = '\0';
    g_stout.current_drive =
        disk_path != NULL &&
                (program_path[0] == 'A' || program_path[0] == 'a') &&
                program_path[1] == ':'
            ? 0u
            : 2u;
    return 1;
}

static int stout_boot(const char *path, const char *disk_path,
                      prg_file_kind_t program_kind, const uint8_t *tail,
                      size_t tail_length)
{
    uint32_t entry;
    uint32_t sp;

    memset(&g_stout, 0, sizeof(g_stout));
    g_stout.ram = (uint8_t *)calloc(1u, STOUT_RAM_SIZE);
    if (g_stout.ram == NULL) {
        fprintf(stderr, "stout: out of memory\n");
        return 0;
    }

    g_stout.load_address = STOUT_LOAD_ADDRESS;
    g_stout.current_drive = 2u;
    g_stout.dta_address = STOUT_BASEPAGE + 0x80u;
    g_stout.program_kind = program_kind;
    g_stout.basepage_address = STOUT_BASEPAGE;
    g_stout.parent_basepage_address =
        program_kind == PRG_FILE_ACC ? 0u : STOUT_PARENT_BASEPAGE;
    g_stout.command_tail_length = tail_length;
    if (tail_length != 0u)
        memcpy(g_stout.command_tail, tail, tail_length);
    g_stout.bpb_address = 0x00002000u;
    g_stout.logical_screen = STOUT_SCREEN_BASE;
    g_stout.physical_screen = STOUT_SCREEN_BASE;
    g_stout.screen_resolution = 2u;
    stout_xbios_init();
    stout_linea_init();
    g_stout.stack_top = STOUT_STACK_TOP;
    g_stout.stop_address = STOUT_STOP_ADDRESS;
    g_stout.trace_traps = (getenv("STOUT_TRACE") != NULL);
    stout_disk_init(&g_stout.disk);
    if (disk_path != NULL) {
        char error_text[256];
        if (!stout_disk_open(&g_stout.disk, disk_path, error_text,
                             sizeof(error_text))) {
            fprintf(stderr, "stout: %s\n", error_text);
            return 0;
        }
        g_stout.disk_mounted = 1;
    }

    stout_write_u16(g_stout.stop_address,
                    (uint16_t)(0x4e40u | STOUT_EXIT_TRAP));

    if (!stout_load_program(path)) {
        return 0;
    }
    g_stout.heap_next =
        (g_stout.load_address + (uint32_t)g_stout.program.image_size +
         g_stout.program.header.bss_size + 3u) &
        ~3u;
    if (!stout_prepare_host_drive(path, disk_path)) {
        fprintf(stderr, "stout: unable to prepare writable drive C:\n");
        return 0;
    }
    stout_write_basepage();
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
    /* TOS startup code receives the basepage pointer at 4(sp). */
    stout_write_u32(sp + 4u, g_stout.basepage_address);
    /* Musashi's public SR setter intentionally does not exchange stack
     * banks. Select user mode, then initialize the now-inactive supervisor
     * stack and the active user stack explicitly. */
    stout_set_reg(M68K_REG_SR, 0u);
    stout_set_reg(M68K_REG_ISP, STOUT_SUPERVISOR_STACK_TOP);
    stout_set_reg(M68K_REG_SP, sp);
    stout_set_reg(M68K_REG_A7, sp);
    stout_set_reg(M68K_REG_A0, g_stout.basepage_address);
    stout_set_reg(M68K_REG_PC, entry);

    g_stout.running = 1;
    return 1;
}

static int stout_run(void)
{
    uint64_t total_cycles = 0;
    uint64_t limit = STOUT_MAX_CYCLES;
    const char *setting = getenv("STOUT_MAX_CYCLES");
    if (setting) {
        char *end;
        errno = 0;
        unsigned long long value = strtoull(setting, &end, 10);
        if (!*setting || *setting == '-' || *end || errno) {
            fprintf(stderr, "stout: invalid STOUT_MAX_CYCLES\n");
            return 1;
        }
        limit = value;
    }

    while (g_stout.running != 0 && (!limit || total_cycles < limit)) {
        int cycles = m68k_execute(limit ? 1000 : 10000);
        if (cycles > 0)
            total_cycles += (unsigned)cycles;
        /* Interactive guest loops must share the host with other applications.
         */
        if (!limit)
            gem_os_sleep_ms(1);
    }

    if (g_stout.running != 0) {
        fprintf(stderr, "stout: execution limit reached at pc=0x%08x\n",
                stout_get_reg(M68K_REG_PC));
        return 1;
    }

    return g_stout.exit_code;
}

static void stout_shutdown(void)
{
    stout_gemdos_shutdown();
    (void)stout_console_close();
    prg_image_free(&g_stout.program);
    stout_disk_free(&g_stout.disk);
    free(g_stout.ram);
    memset(&g_stout, 0, sizeof(g_stout));
}

int main(int argc, char **argv)
{
    const char *program_path;
    const char *disk_path = NULL;
    char resolved_program[GEM_OS_PATH_MAX];
    prg_file_kind_t program_kind;
    uint8_t command_tail[127];
    size_t command_tail_length = 0u;
    int argument = 2;
    int exit_code;

    if (argc < 2) {
        fprintf(stderr,
                "usage: stout <program> [disk.st|disk.msa] [parameters...]\n");
        return 1;
    }

    program_path = argv[1];
    if (argument < argc &&
        (strstr(argv[argument], ".st") || strstr(argv[argument], ".ST") ||
         strstr(argv[argument], ".msa") || strstr(argv[argument], ".MSA"))) {
        disk_path = argv[argument++];
    }
    if ((program_path[0] == 'A' || program_path[0] == 'a') &&
        program_path[1] == ':' && disk_path != NULL) {
        if (!prg_file_kind(program_path, &program_kind)) {
            fprintf(stderr, "stout: unsupported program type: %s\n",
                    program_path);
            return 1;
        }
    } else if (prg_file_kind(program_path, &program_kind)) {
        if (gem_os_access(program_path, GEM_OS_ACCESS_READ) != 0) {
            /* An explicit readable path needs no search-path rewriting. */
        } else {
            int found =
                program_kind == PRG_FILE_ACC
                    ? gem_os_find_accessory(program_path, resolved_program,
                                            sizeof(resolved_program))
                    : gem_os_find_auto_program(program_path, resolved_program,
                                               sizeof(resolved_program));
            if (!found && program_kind != PRG_FILE_ACC) {
                found = gem_os_find_accessory(program_path, resolved_program,
                                              sizeof(resolved_program));
            }
            if (!found || !prg_file_kind(resolved_program, &program_kind)) {
                fprintf(stderr, "stout: program not found: %s\n", program_path);
                return 1;
            }
            program_path = resolved_program;
        }
    } else {
        if (gem_os_find_accessory(program_path, resolved_program,
                                  sizeof(resolved_program)) == 0 ||
            !prg_file_kind(resolved_program, &program_kind)) {
            fprintf(stderr, "stout: unsupported program type: %s\n",
                    program_path);
            return 1;
        }
        program_path = resolved_program;
    }
    while (argument < argc) {
        size_t length = strlen(argv[argument]);
        if (command_tail_length != 0u &&
            command_tail_length < sizeof(command_tail) - 1u)
            command_tail[command_tail_length++] = ' ';
        if (length > sizeof(command_tail) - 1u - command_tail_length)
            length = sizeof(command_tail) - 1u - command_tail_length;
        memcpy(command_tail + command_tail_length, argv[argument], length);
        command_tail_length += length;
        ++argument;
    }

    if (!stout_boot(program_path, disk_path, program_kind, command_tail,
                    command_tail_length)) {
        stout_shutdown();
        return 1;
    }

    if (program_kind == PRG_FILE_TOS || program_kind == PRG_FILE_TTP)
        (void)stout_console_open();
    exit_code = stout_run();
    stout_shutdown();
    return exit_code;
}
