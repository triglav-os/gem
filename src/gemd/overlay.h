/*
 * Declares gemd's validated BGRA overlay dispatcher and lifecycle hooks.
 *
 * MIT License (see: LICENSE)
 * Copyright (C) 2026 tomaz stih
 */

#ifndef GEMD_OVERLAY_H
#define GEMD_OVERLAY_H

#include "session.h"

/* Validates and dispatches one overlay RPC for a connected session. */
int32_t gemd_overlay_dispatch(gemd_session_t *session,
                              const gem_rpc_header_t *header,
                              const uint8_t *payload, uint8_t *response,
                              uint32_t *response_size);
/* Destroys every overlay owned by an application that is exiting. */
void gemd_overlay_cleanup_app(WORD app_id);
/* Recomputes overlay visibility and occlusion after window-stack changes. */
void gemd_overlay_window_top_changed(void);

#endif
