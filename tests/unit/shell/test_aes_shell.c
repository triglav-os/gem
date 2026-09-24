/*
 * Verifies direct AES shell invocation context, environment lookup, path
 * search and synchronized DESKTOP.INF/desktop.cfg compatibility.
 *
 * MIT License (see: LICENSE)
 * Copyright (C) 2026 tomaz stih
 */

#define _POSIX_C_SOURCE 200809L

#include <gem.h>
#include <gem/gemconf.h>

#include <assert.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <threads.h>
#include <unistd.h>

enum { TEST_BUFFER_SIZE = 4096, TEST_ITERATIONS = 100 };

static const char desktop_inf_first[] =
    "#E3A01\r\n#M000001FF A FIRST@ @\r\n"
    "#G0C2C EDITA.APP@ *.TXT@\r\n#ZFIRST-UNKNOWN@\r\n\x1a\0";
static const char desktop_inf_second[] =
    "#E8208\r\n#M010101FF B SECOND@ @\r\n"
    "#P1535 RUNNER.APP@ *.BIN@\r\n#ZSECOND-UNKNOWN@\r\n\x1a\0";

typedef struct writer_context {
    const char *desktop_inf;
    size_t size;
    atomic_int *failed;
} writer_context_t;

static int buffer_is_complete(const char *buffer)
{
    const char *ending;

    if (buffer[0] != '#')
        return 0;
    ending = memchr(buffer, '\0', TEST_BUFFER_SIZE);
    return ending != NULL && ending > buffer && ending[-1] == 0x1a &&
           (strstr(buffer, "FIRST@") != NULL ||
            strstr(buffer, "SECOND@") != NULL) &&
           (strstr(buffer, "EDITA.APP@") != NULL ||
            strstr(buffer, "RUNNER.APP@") != NULL);
}

static int write_shell_buffer(void *argument)
{
    writer_context_t *context = (writer_context_t *)argument;
    char result[TEST_BUFFER_SIZE] = {0};
    int iteration;

    for (iteration = 0; iteration < TEST_ITERATIONS; ++iteration) {
        if (shel_put((char *)context->desktop_inf, (WORD)context->size) == 0 ||
            shel_get(result, sizeof(result)) == 0 ||
            buffer_is_complete(result) == 0) {
            atomic_store(context->failed, 1);
            return 1;
        }
    }
    return 0;
}

int main(int argc, char **argv)
{
    char config_root[] = "/tmp/gem-shell-config-XXXXXX";
    char config_file[256];
    char config_directory[256];
    char command[256];
    char tail[128] = {0};
    char path[260] = "clock.app";
    char final[TEST_BUFFER_SIZE];
    char *environment = NULL;
    gemconf_t config;
    atomic_int failed = 0;
    writer_context_t first = {desktop_inf_first, sizeof(desktop_inf_first) - 1u,
                              &failed};
    writer_context_t second = {desktop_inf_second,
                               sizeof(desktop_inf_second) - 1u, &failed};
    thrd_t first_thread;
    thrd_t second_thread;
    int first_result;
    int second_result;

    assert(argc == 2 && strcmp(argv[1], "notes.txt") == 0);
    assert(mkdtemp(config_root) != NULL);
    assert(setenv("XDG_CONFIG_HOME", config_root, 1) == 0);
    gemconf_init(&config);
    assert(gemconf_load(&config, "desktop"));
    assert(gemconf_set(&config, "locations", "/tmp", "Temporary"));
    assert(gemconf_save(&config));
    gemconf_clear(&config);
    assert(shel_read(command, tail));
    assert(strstr(command, "test_aes_shell") != NULL);
    assert((unsigned char)tail[0] == strlen("notes.txt"));
    assert(memcmp(tail + 1, "notes.txt", strlen("notes.txt")) == 0);
    assert(shel_envrn(&environment, "SHELL_TEST_VALUE="));
    assert(strcmp(environment, "works") == 0);
    assert(shel_find(path));
    assert(path[0] == '/');
    strcpy(path, argv[0]);
    assert(shel_find(path));
    assert(path[0] == '/');

    memset(final, 0, sizeof(final));
    assert(shel_get(final, sizeof(final)));
    assert(strstr(final, "Temporary@") != NULL);
    assert(shel_put((char *)desktop_inf_first,
                    (WORD)(sizeof(desktop_inf_first) - 1u)));
    assert(thrd_create(&first_thread, write_shell_buffer, &first) ==
           thrd_success);
    assert(thrd_create(&second_thread, write_shell_buffer, &second) ==
           thrd_success);
    assert(thrd_join(first_thread, &first_result) == thrd_success);
    assert(thrd_join(second_thread, &second_result) == thrd_success);
    assert(first_result == 0 && second_result == 0);
    assert(atomic_load(&failed) == 0);
    assert(shel_get(final, sizeof(final)));
    assert(buffer_is_complete(final));
    gemconf_init(&config);
    assert(gemconf_load(&config, "desktop"));
    if (strcmp(gemconf_get(&config, "locations", "/tmp", ""), "FIRST") == 0)
        assert(strcmp(gemconf_get(&config, "desktop", "view", ""), "text") ==
               0);
    else {
        assert(strcmp(gemconf_get(&config, "locations", "/tmp", ""),
                      "SECOND") == 0);
        assert(strcmp(gemconf_get(&config, "desktop", "view", ""), "icons") ==
               0);
    }
    assert(strcmp(gemconf_get(&config, "association.000", "executable", ""),
                  "EDITA.APP") == 0 ||
           strcmp(gemconf_get(&config, "association.000", "executable", ""),
                  "RUNNER.APP") == 0);
    assert(gemconf_get(&config, "legacy.000", "record_hex", NULL) != NULL);
    assert(gemconf_set(&config, "locations", "/tmp", "DIRECT"));
    assert(gemconf_save(&config));
    gemconf_clear(&config);
    memset(final, 0, sizeof(final));
    assert(shel_get(final, sizeof(final)));
    assert(strstr(final, "DIRECT@") != NULL);
    assert(!shel_put("not a desktop image", 19));
    memset(final, 0, sizeof(final));
    assert(shel_get(final, sizeof(final)));
    assert(strstr(final, "DIRECT@") != NULL);
    (void)snprintf(config_file, sizeof(config_file), "%s/gem/desktop.cfg",
                   config_root);
    (void)snprintf(config_directory, sizeof(config_directory), "%s/gem",
                   config_root);
    assert(remove(config_file) == 0);
    assert(rmdir(config_directory) == 0);
    assert(rmdir(config_root) == 0);
    return 0;
}
