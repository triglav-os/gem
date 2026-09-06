/*
 * Implements VDI workstation aliases and client-owned input/callback state.
 * Request strings use cooperative AES events so a slow typist does not block
 * gemd. Historical callback registration retains addresses only in the client.
 *
 * MIT License (see: LICENSE)
 * Copyright (C) 2026 tomaz stih
 */
#include "gem_protocol.h"
#include <stddef.h>
static VOID (*callbacks[4])(void);
WORD v_opnwk(WORD in[], WORD *handle, WORD out[])
{
    v_opnvwk(in, handle, out);
    return handle ? *handle : 0;
}
WORD v_opnrwk(WORD in[], WORD *handle, WORD out[])
{
    return v_opnwk(in, handle, out);
}
WORD v_clswk(WORD handle)
{
    WORD out[57];
    WORD valid = vq_extnd(handle, 0, out);
    if (valid)
        v_clsvwk(handle);
    return valid;
}
static WORD register_callback(WORD handle, unsigned slot,
                              VOID (*callback)(void), VOID (**previous)(void))
{
    WORD out[57];
    if (!vq_extnd(handle, 0, out))
        return 0;
    if (previous)
        *previous = callbacks[slot];
    callbacks[slot] = callback;
    return 1;
}
WORD vex_timv(WORD handle, VOID (*callback)(void), VOID (**previous)(void),
              WORD *scale)
{
    WORD result = register_callback(handle, 0, callback, previous);
    if (result && scale)
        *scale = 1;
    return result;
}
WORD vex_butv(WORD handle, VOID (*callback)(void), VOID (**previous)(void))
{
    return register_callback(handle, 1, callback, previous);
}
WORD vex_motv(WORD handle, VOID (*callback)(void), VOID (**previous)(void))
{
    return register_callback(handle, 2, callback, previous);
}
WORD vex_curv(WORD handle, VOID (*callback)(void), VOID (**previous)(void))
{
    return register_callback(handle, 3, callback, previous);
}
VOID vrq_string(VDI_HANDLE handle, WORD length, WORD echo, WORD *xy, BYTE *text)
{
    WORD used = 0, out[57];
    if (!text || length < 0 || !vq_extnd(handle, 0, out))
        return;
    text[0] = 0;
    for (;;) {
        WORD key = 0;
        if (!evnt_multi(MU_KEYBD, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, NULL,
                        0, 0, NULL, NULL, NULL, NULL, &key, NULL))
            return;
        BYTE ch = (BYTE)(key & 0xff);
        if (ch == '\r' || ch == '\n')
            return;
        if (ch == '\b') {
            if (used)
                text[--used] = 0;
        } else if (ch && used < length) {
            text[used++] = ch;
            text[used] = 0;
        }
        if (echo && xy)
            v_gtext(handle, xy[0], xy[1], text);
    }
}
WORD vsm_string(WORD handle, WORD length, WORD echo, WORD *xy, BYTE *text)
{
    WORD out[57];
    if (!text || length < 0 || !vq_extnd(handle, 0, out))
        return 0;
    vrq_string(handle, length, echo, xy, text);
    return 1;
}
