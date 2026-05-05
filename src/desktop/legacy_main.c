/*
 * Launches the imported historical GEM desktop shell through `gemain()`.
 * The wrapper seeds a minimal desktop configuration so the legacy shell
 * can start on a fresh hosted build before real DESKTOP.INF persistence
 * and installer flows are finished.
 *
 * MIT License (see: LICENSE)
 * Copyright (C) 2026 tomaz stih
 */

#include "gem/aes.h"
#include "platform/os.h"

#include <string.h>

int main(void);

/*
 * Seeds a minimal `DESKTOP.INF` payload for fresh hosted runs and then
 * transfers control to the original desktop entry point in `gemain()`.
 */
int main(void)
{
    static char default_inf[] =
        "#M00000101 A A@ @\r\n"
        "#M01000000 C C@ @\r\n"
        "#DFF02 @ *.*@\r\n"
        "#G08FF *.APP@ @\r\n"
        "#P08FF *.EXE@ @\r\n"
        "#P08FF *.COM@ @\r\n"
        "#P08FF *.BAT@ @\r\n"
        "#E1110\r\n";
    int fd;

    fd = gem_os_open_read("bin/desktop.inf");
    if (fd >= 0) {
        (void) gem_os_close(fd);
    } else {
        (void) shel_put(default_inf, (WORD) strlen(default_inf));
    }
    return (int) gemain();
}
