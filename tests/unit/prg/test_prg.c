/*
 * Exercises Atari executable classification and loader compatibility.
 *
 * MIT License (see: LICENSE)
 * Copyright (C) 2026 tomaz stih
 */

#include "prg/prg.h"

#include <assert.h>
#include <stdint.h>
#include <string.h>

static void test_all_program_suffixes(void)
{
    static const char *const paths[] = {
        "DEMO.PRG", "demo.tos", "Tool.TtP", "EDITOR.APP", "run.gtp",
        "CLOCK.Acc"};
    size_t index;

    for (index = 0u; index < sizeof(paths) / sizeof(paths[0]); ++index) {
        prg_file_kind_t kind;

        assert(prg_file_kind(paths[index], &kind));
        assert((size_t)kind == index);
    }
    {
        prg_file_kind_t kind;

        assert(!prg_file_kind("README.TXT", &kind));
        assert(!prg_file_kind("NO_SUFFIX", &kind));
    }
}

static void test_unknown_symbol_layout_is_skipped(void)
{
    enum {
        HEADER_SIZE = 28,
        TEXT_SIZE = 4,
        PRIVATE_SYMBOL_SIZE = 48,
        RELOCATION_SIZE = 4,
        PROGRAM_SIZE = HEADER_SIZE + TEXT_SIZE + PRIVATE_SYMBOL_SIZE +
                       RELOCATION_SIZE
    };
    uint8_t program[PROGRAM_SIZE];
    prg_image_t image;
    char error[PRG_ERROR_LENGTH];

    memset(program, 0, sizeof(program));
    program[0] = 0x60u;
    program[1] = 0x1au;
    program[5] = TEXT_SIZE;
    program[17] = PRIVATE_SYMBOL_SIZE;
    memset(program + HEADER_SIZE + TEXT_SIZE, 0xa5, PRIVATE_SYMBOL_SIZE);

    prg_image_init(&image);
    memset(error, 0, sizeof(error));
    assert(prg_read_memory(program, sizeof(program), 0x10000u, &image, error,
                           sizeof(error)));
    assert(image.image_size == TEXT_SIZE);
    assert(image.symbol_count == 0u);
    assert(image.symbols == NULL);
    prg_image_free(&image);
}

int main(void)
{
    test_all_program_suffixes();
    test_unknown_symbol_layout_is_skipped();
    return 0;
}
