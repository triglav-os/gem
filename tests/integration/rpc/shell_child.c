/*
 * Records the per-process AES shell command and TOS tail received by a child
 * launched through shel_write, using only GEM OS wrappers for file output.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (C) 2026 tomaz stih
 */

#include <gem.h>
#include <gem/os.h>

#include <stdio.h>
#include <string.h>

typedef struct shell_child_result {
    WORD app_id;
    WORD argument_count;
    char first_argument[128];
    char command[256];
    char tail[128];
} shell_child_result_t;

int main(int argc, char **argv)
{
    shell_child_result_t result;
    char output[GEM_OS_PATH_MAX];
    char *home = NULL;
    const char *name;
    int fd;
    int length;

    memset(&result, 0, sizeof(result));
    result.argument_count = (WORD)argc;
    if (argc > 1) {
        if (strlen(argv[1]) >= sizeof(result.first_argument)) {
            return 1;
        }
        strcpy(result.first_argument, argv[1]);
    }
    result.app_id = appl_init();
    if (result.app_id <= 0 || shel_read(result.command, result.tail) == 0 ||
        shel_envrn(&home, "GEMIX_HOME=") == 0) {
        return 1;
    }
    name = (strcmp(result.command, "clock.app") == 0) ? "clock.result"
                                                      : "term.result";
    length = snprintf(output, sizeof(output), "%s/%s", home, name);
    if (length <= 0 || (size_t)length >= sizeof(output)) {
        return 1;
    }
    fd = gem_os_open_write(output);
    if (fd < 0) {
        return 1;
    }
    if (gem_os_write(fd, &result, sizeof(result)) != (int32_t)sizeof(result)) {
        (void)gem_os_close(fd);
        return 1;
    }
    if (gem_os_close(fd) != 0) {
        return 1;
    }
    return (appl_exit() != 0) ? 0 : 1;
}
