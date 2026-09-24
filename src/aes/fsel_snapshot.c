/*
 * Captures and restores screen pixels around the synchronous file chooser.
 * Its caller cannot service redraw messages until the modal call returns, so
 * exposed owners are repainted from this pre-modal snapshot.
 *
 * MIT License (see: LICENSE)
 * Copyright (C) 2026 tomaz stih
 */

#include "fsel_private.h"

#include <stdlib.h>

void aes_fsel_snapshot(aes_fsel_state_t *state)
{
    GRECT desk;
    MFDB screen = {0};
    WORD pxy[8] = {0};

    if (state == NULL) {
        return;
    }
    wind_get(0, WF_CXYWH, &desk.g_x, &desk.g_y, &desk.g_w, &desk.g_h);
    state->background.fd_w = (WORD)(desk.g_x + desk.g_w);
    state->background.fd_h = (WORD)(desk.g_y + desk.g_h);
    state->background.fd_wdwidth = (WORD)((state->background.fd_w + 15) / 16);
    state->background.fd_nplanes = 1;
    state->background.fd_addr =
        calloc((size_t)state->background.fd_wdwidth * 2u,
               (size_t)state->background.fd_h);
    pxy[2] = pxy[6] = (WORD)(state->background.fd_w - 1);
    pxy[3] = pxy[7] = (WORD)(state->background.fd_h - 1);
    if (state->background.fd_addr != NULL) {
        vro_cpyfm(aes_state.vdi_handle, S_ONLY, pxy, &screen,
                  &state->background);
    }
}

void aes_fsel_restore_owner(const aes_fsel_state_t *state, WORD msg[8])
{
    GRECT box;
    MFDB screen = {0};

    if (state == NULL || msg == NULL || state->background.fd_addr == NULL) {
        return;
    }
    wind_update(BEG_UPDATE);
    wind_get(msg[3], WF_FIRSTXYWH, &box.g_x, &box.g_y, &box.g_w, &box.g_h);
    while (box.g_w > 0 && box.g_h > 0) {
        WORD pxy[8];
        GRECT dirty = {msg[4], msg[5], msg[6], msg[7]};

        if (aes_intersect_rects(&box, &dirty, &box)) {
            pxy[0] = pxy[4] = box.g_x;
            pxy[1] = pxy[5] = box.g_y;
            pxy[2] = pxy[6] = (WORD)(box.g_x + box.g_w - 1);
            pxy[3] = pxy[7] = (WORD)(box.g_y + box.g_h - 1);
            vs_clip(aes_state.vdi_handle, 0, NULL);
            vro_cpyfm(aes_state.vdi_handle, S_ONLY, pxy,
                      (MFDB *)&state->background, &screen);
        }
        wind_get(msg[3], WF_NEXTXYWH, &box.g_x, &box.g_y, &box.g_w, &box.g_h);
    }
    wind_update(END_UPDATE);
}
