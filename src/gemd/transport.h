/*
 * Bounded, non-blocking framing for one private gemd connection.
 *
 * MIT License (see: LICENSE)
 * Copyright (C) 2026 tomaz stih
 */
#ifndef GEMD_TRANSPORT_H
#define GEMD_TRANSPORT_H

#include "../gem/gem_protocol.h"

typedef struct gemd_io {
    gem_rpc_header_t header;
    _Alignas(max_align_t) uint8_t payload[GEM_RPC_PAYLOAD_MAX];
    uint8_t output[sizeof(gem_rpc_reply_t) + GEM_RPC_PAYLOAD_MAX];
    size_t header_read, payload_read, output_size, output_sent;
    uint32_t started;
    int ready;
} gemd_io_t;

/* Set non-blocking and close-on-exec flags before accepting any requests. */
int gemd_nonblocking(int fd);
/* Receive at most one bounded request; -1 closes, 0 waits, 1 is ready. */
int gemd_receive(int fd, gemd_io_t *io);
/* Queue one reply without blocking on an uncooperative peer. */
void gemd_reply(gemd_io_t *io, int32_t status, const void *data, uint32_t size);
/* Drain queued output; -1 closes, 0 waits, 1 completes. */
int gemd_send(int fd, gemd_io_t *io);

#endif
