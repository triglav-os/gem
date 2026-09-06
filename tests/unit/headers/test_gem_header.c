/*
 * Verifies that the traditional top-level GEM compatibility header is
 * self-contained and exposes the expected public types and aliases.
 *
 * MIT License (see: LICENSE)
 * Copyright (C) 2026 tomaz stih
 */

#include <gem.h>

_Static_assert(sizeof(WORD) == 2u, "GEM WORD must remain 16 bits");
_Static_assert(WF_WORKXYWH == WF_CXYWH,
    "classic work rectangle alias must match AES");
_Static_assert(WF_CURRXYWH == WF_WXYWH,
    "classic current rectangle alias must match AES");

int main(void)
{
    GRECT rectangle = { 0, 0, 0, 0 };
    MFDB bitmap = { 0 };
    OBJECT object = { 0 };

    return rectangle.g_w != 0 || bitmap.fd_w != 0 || object.ob_width != 0;
}
