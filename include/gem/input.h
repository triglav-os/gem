/*
 * Optional hosted physical-key events for evnt_multi. The standard AES
 * signatures, MU_KEYBD encoding and behavior are unchanged.
 *
 * MIT License (see: LICENSE)
 * Copyright (C) 2026 tomaz stih
 */
#ifndef GEM_INPUT_H
#define GEM_INPUT_H

/* Opt in instead of MU_KEYBD. pkr retains the USB usage in its high byte
 * and translated ASCII in its low byte; pbr contains GEM_KEY_* flags.
 * Releases are delivered only to callers requesting this extension.
 * Request before receiving input. While opted in, the client routes its own
 * keyboard menu shortcuts; AES still handles pointer menu selection.
 * Requesting MU_KEYBD again restores standard AES keyboard dispatch.
 * GEM_KEY_RESET cancels all held keys after input loss/queue overflow.
 * Existing event RPC structures and standard button-click counts are
 * unchanged. Both libgem and gemd must use an SDK with this extension. */
#define GEM_MU_KEY 0x4000
#define GEM_KEY_PRESS 0x0001
#define GEM_KEY_RESET 0x0002

#endif
