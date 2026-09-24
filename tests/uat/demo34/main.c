/*
 * Exercises the modern open, save and directory chooser modes through their
 * public AES entry points while preserving the classic fsel_input buffers.
 *
 * MIT License (see: LICENSE)
 * Copyright (C) 2026 tomaz stih
 */

#include <gem.h>

#include <string.h>

#ifndef DEMO34_FIXTURE_DIR
#define DEMO34_FIXTURE_DIR "/tmp"
#endif

int main(void)
{
    char path[260] = DEMO34_FIXTURE_DIR "/*";
    char name[260] = "";
    WORD button = 0;
    WORD app_id = appl_init();

    if (app_id <= 0) {
        return 1;
    }
    (void)fsel_input(path, name, &button);

    strncpy(name, "gem-notes.txt", sizeof(name) - 1u);
    name[sizeof(name) - 1u] = '\0';
    (void)fsel_save(path, name, &button);

    strcpy(path, DEMO34_FIXTURE_DIR);
    (void)fsel_directory(path, &button);

    appl_exit();
    return 0;
}
