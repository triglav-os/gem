/*
 * Declares the chrome-tracking context and the drag helpers shared by the
 * hosted AES window tracking and dragging modules. Private to libaes.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (C) 2026 tomaz stih
 */

#ifndef GEM_AES_WINDOW_TRACK_H
#define GEM_AES_WINDOW_TRACK_H

#include "aes_internal.h"

/* One chrome press being tracked from the first event to its release. */
typedef struct aes_track {
    aes_window_t *window;
    const gem_hid_event_t *first_evt;
    WORD flags;
    WORD *mepbuff;
    WORD *pmx;
    WORD *pmy;
    WORD *pmb;
    WORD *pks;
    WORD start_x;
    WORD start_y;
    int defer_raise;
} aes_track_t;

/* Queue a WM_* message for a window's owner. */
void aes_queue_window_message(const aes_window_t *window, WORD message, WORD w4,
                              WORD w5, WORD w6, WORD w7);
/* XOR a rubber-band outline; drawing it again erases it. */
void aes_draw_drag_outline(const GRECT *rect);
/* Defer other applications' drawing during a drag. */
void aes_begin_interaction_lock(void);
/* Release the drag interaction lock. */
void aes_end_interaction_lock(void);
/* Keep a dragged frame's title strip on the desktop. */
void aes_clamp_dragged_window_position(const aes_window_t *window,
                                       const GRECT *desktop, WORD *x, WORD *y);
/* Drag the outline of a movable window and queue WM_MOVED. */
WORD aes_track_move(const aes_track_t *track);
/* Drag the outline of a sizable window and queue WM_SIZED. */
WORD aes_track_size(const aes_track_t *track);

#endif
