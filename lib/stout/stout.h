/*
 * Declares Stout's guest machine state, the parameter block layouts and
 * the trap handlers shared between the emulator core and the trap module.
 * Private to the bundled Stout application and its support library.
 *
 * MIT License (see: LICENSE)
 * Copyright (C) 2026 tomaz stih
 */

#ifndef GEM_SAMPLE_STOUT_H
#define GEM_SAMPLE_STOUT_H

#include <gem.h>
#include <gem/os.h>
#include <gem/aes.h>
#include <gem/vdi.h>

#include <m68k.h>

#include "prg/prg.h"
#include "stout/disk.h"

#include <stdint.h>

#define STOUT_RAM_SIZE 0x01000000u
#define STOUT_SCREEN_WIDTH 640u
#define STOUT_SCREEN_HEIGHT 400u
#define STOUT_SCREEN_BYTES ((STOUT_SCREEN_WIDTH * STOUT_SCREEN_HEIGHT) / 8u)
#define STOUT_TPA_TOP 0x00ff0000u
#define STOUT_SCREEN_BASE 0x00ff8000u
#define STOUT_IOREC_BASE 0x00003000u
#define STOUT_IOREC_STRIDE 0x00000200u
#define STOUT_IOREC_BUFFER_OFFSET 0x00000100u
#define STOUT_IOREC_BUFFER_SIZE 256u
#define STOUT_KBDVECS_BASE 0x00003600u
#define STOUT_KBDVECS_SIZE 36u
#define STOUT_KEYTAB_BASE 0x00003800u
#define STOUT_KEYTAB_UNSHIFT 0x00003900u
#define STOUT_KEYTAB_SHIFT 0x00003980u
#define STOUT_KEYTAB_CAPS 0x00003a00u
#define STOUT_LINEA_BASE 0x00005000u
#define STOUT_LINEA_FONTS 0x00005200u
#define STOUT_LINEA_VECTORS 0x00005220u
#define STOUT_LINEA_FONT_HEADERS 0x00005400u
#define STOUT_LINEA_FONT_OFFSETS 0x00005800u
#define STOUT_LINEA_FONT_DATA 0x00005a10u
#define STOUT_LINEA_HANDLER 0x00006c00u
#define STOUT_STACK_TOP 0x000ff000u
#define STOUT_SUPERVISOR_STACK_TOP STOUT_SCREEN_BASE
#define STOUT_BASEPAGE 0x00008000u
#define STOUT_LOAD_ADDRESS (STOUT_BASEPAGE + 0x100u)
#define STOUT_STOP_ADDRESS 0x00007000u
#define STOUT_PARENT_BASEPAGE 0x00007400u
#define STOUT_GEMDOS_TRAP 1
#define STOUT_AES_VDI_TRAP 2
#define STOUT_LINEA_TRAP 12
#define STOUT_BIOS_TRAP 13
#define STOUT_XBIOS_TRAP 14
#define STOUT_EXIT_TRAP 15
#define STOUT_MAX_CYCLES 0u
#define STOUT_GEMDOS_HANDLES 32

typedef struct stout_gemdos_handle {
    int used;
    int host_fd;
    uint8_t *data;
    size_t size;
    size_t capacity;
    size_t position;
    int writable;
    int disk_image;
    int dirty;
    int metadata_dirty;
    uint8_t attributes;
    uint16_t time;
    uint16_t date;
    char path[GEM_OS_PATH_MAX];
} stout_gemdos_handle_t;

typedef struct stout_gemdos_dta {
    gem_os_dir_t directory;
    int active;
    int disk_image;
    uint32_t address;
    char pattern[128];
    char disk_directory[GEM_OS_PATH_MAX];
    size_t disk_match_index;
    uint16_t attributes;
} stout_gemdos_dta_t;

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
    prg_file_kind_t program_kind;
    uint32_t basepage_address;
    uint32_t parent_basepage_address;
    uint8_t command_tail[128];
    size_t command_tail_length;
    stout_disk_t disk;
    int disk_mounted;
    WORD app_id;
    WORD terminal_id;
    uint32_t bpb_address;
    int console_pending;
    uint8_t console_character;
    uint16_t current_drive;
    char host_drive_root[GEM_OS_PATH_MAX];
    char host_current_path[GEM_OS_PATH_MAX];
    char disk_current_path[GEM_OS_PATH_MAX];
    int supervisor_mode;
    uint32_t dta_address;
    uint32_t heap_next;
    uint32_t resource_address;
    uint32_t resource_size;
    uint32_t logical_screen;
    uint32_t physical_screen;
    uint16_t screen_resolution;
    uint16_t screen_palette[16];
    uint16_t printer_config;
    WORD vdi_handle;
    uint32_t menu_guest_address;
    WORD menu_object_count;
    OBJECT menu_objects[256];
    stout_gemdos_handle_t gemdos_handles[STOUT_GEMDOS_HANDLES];
    stout_gemdos_dta_t dta;
} stout_state_t;

extern stout_state_t g_stout;

/* Service an illegal instruction as a guest fault. */
int stout_handle_illegal(int opcode);
/* Service the trap the guest just raised; nonzero when handled. */
int stout_handle_trap(int trap);
/* Push the return address that ends the program. */
uint32_t stout_push_return_trap(uint32_t sp);
/* Load a big-endian 16-bit value from guest RAM. */
uint16_t stout_read_u16(uint32_t address);
/* Load a big-endian 32-bit value from guest RAM. */
uint32_t stout_read_u32(uint32_t address);
void stout_set_reg(m68k_register_t reg, uint32_t value);
/* Nonzero when a guest span lies inside RAM. */
int stout_valid_range(uint32_t address, size_t size);
/* Store a big-endian 16-bit value into guest RAM. */
void stout_write_u16(uint32_t address, uint16_t value);
/* Store a big-endian 32-bit value into guest RAM. */
void stout_write_u32(uint32_t address, uint32_t value);

/* Shared TOS trap helpers. */
int16_t stout_read_s16(uint32_t address);
void stout_read_string(uint32_t address, char *buffer, size_t size);
uint32_t stout_get_reg(m68k_register_t reg);
void stout_stop_with_code(int code);
int stout_console_input(uint8_t *character);
int stout_console_peek(void);
void stout_console_output(uint8_t character);
int stout_console_open(void);
int stout_console_close(void);
void stout_write_guest_bpb(void);
int stout_handle_gemdos(void);
/* Reads a GEMDOS guest path into a newly allocated host buffer. */
int stout_gemdos_read_file(uint32_t path_address, uint8_t **data_out,
                           size_t *size_out);
int stout_handle_bios(void);
int stout_handle_xbios(void);
void stout_xbios_init(void);
int stout_handle_linea(void);
void stout_linea_init(void);
void stout_gemdos_shutdown(void);

#endif
