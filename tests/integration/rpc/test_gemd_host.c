/*
 * Checks hostile hosted framebuffer paths and application-id rollover using
 * private implementation state, without changing the public GEM interfaces.
 *
 * MIT License (see: LICENSE)
 * Copyright (C) 2026 tomaz stih
 */
#define _POSIX_C_SOURCE 200809L
#include "../../src/aes/_aes.h"
#include "platform/raster.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static void framebuffer_paths(void)
{
    char directory[] = "/tmp/gem-host-test.XXXXXXXX";
    char target[128], path[128];
    struct stat st;
    FILE *file;
    assert(mkdtemp(directory));
    snprintf(target, sizeof(target), "%s/target", directory);
    snprintf(path, sizeof(path), "%s/framebuffer", directory);
    file = fopen(target, "wb");
    assert(file && fputs("preserved", file) >= 0 && fclose(file) == 0);
    assert(setenv("GEM_RASTA_FRAMEBUFFER", path, 1) == 0);
    assert(symlink(target, path) == 0);
    assert(!gem_raster_init(80, 80, GEM_RASTER_MONO1));
    assert(stat(target, &st) == 0 && st.st_size == 9);
    assert(unlink(path) == 0 && link(target, path) == 0);
    assert(!gem_raster_init(80, 80, GEM_RASTER_MONO1));
    assert(stat(target, &st) == 0 && st.st_size == 9);
    assert(unlink(path) == 0 && mkfifo(path, 0600) == 0);
    assert(!gem_raster_init(80, 80, GEM_RASTER_MONO1));
    assert(unlink(path) == 0);
    assert(gem_raster_init(80, 80, GEM_RASTER_MONO1));
    assert(stat(path, &st) == 0 && st.st_size == 800 &&
        (st.st_mode & 0777) == 0600);
    memset(gem_raster_surface()->pixels, 0x5a, 800);
    gem_raster_present();
    file = fopen(path, "rb");
    assert(file && fgetc(file) == 0x5a && fclose(file) == 0);
    /* The viewer may replace its file when its geometry changes. */
    assert(unlink(path) == 0);
    file = fopen(path, "wb");
    assert(file && fclose(file) == 0);
    assert(gem_raster_resync());
    gem_raster_present();
    file = fopen(path, "rb");
    assert(file && fgetc(file) == 0x5a && fclose(file) == 0);
    gem_raster_shutdown();
    assert(unlink(path) == 0 && unlink(target) == 0 && rmdir(directory) == 0);
}

static void application_ids(void)
{
    WORD first = appl_init(), last, wrapped;
    assert(first == 1);
    _aes.next_app_id = 32767;
    last = appl_init();
    wrapped = appl_init();
    assert(last == 32767 && wrapped == 2);
    assert(_aes_find_app_by_id(first) && _aes_find_app_by_id(last));
    assert(appl_exit());
    _aes.current_app_id = last;
    assert(appl_exit());
    _aes.current_app_id = first;
    assert(appl_exit());
}

static void selector_patterns(void)
{
    static const struct {const char *pattern, *name; int matches;} cases[] = {
        {"", "", 1}, {"", "a", 0}, {"*", "", 1}, {"?", "", 0},
        {"?", "a", 1}, {"?", "ab", 0}, {"*.TXT", "notes.txt", 1},
        {"*.txt", "notes.png", 0}, {"a*b*c", "axbyc", 1},
        {"a*b*c", "axbycd", 0}, {"**a***?*", "AB", 1},
        {"*ab*ab", "abab", 1}, {"*ab*ab", "aba", 0},
        {"*.?", "name.c", 1}, {"*.?", "name.cpp", 0}
    };
    char hostile[202], name[201];
    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); ++i)
        assert(_aes_fsel_match_pattern(cases[i].pattern, cases[i].name) ==
            cases[i].matches);
    for (size_t i = 0; i < 100; ++i) {
        hostile[i * 2] = '*'; hostile[i * 2 + 1] = 'a';
    }
    hostile[200] = 'b'; hostile[201] = '\0';
    memset(name, 'a', 200); name[200] = '\0';
    assert(!_aes_fsel_match_pattern(hostile, name));
    hostile[200] = '*';
    assert(_aes_fsel_match_pattern(hostile, name));
}

int main(void)
{
    framebuffer_paths();
    application_ids();
    selector_patterns();
    puts("hostile framebuffer paths, viewer replacement and app-id rollover passed");
    return 0;
}
