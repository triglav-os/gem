/*
 * Incremental RPC framing: a stalled reader/writer cannot stop other apps.
 * Each scheduling turn does bounded I/O with one in-flight reply per client.
 *
 * MIT License (see: LICENSE)
 * Copyright (C) 2026 tomaz stih
 */
#include "transport.h"
#include "platform/os.h"

#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <sys/socket.h>

int gemd_nonblocking(int fd)
{
    int flags = fcntl(fd, F_GETFL, 0);
    return flags >= 0 && fcntl(fd, F_SETFL, flags | O_NONBLOCK) == 0 &&
        fcntl(fd, F_SETFD, FD_CLOEXEC) == 0;
}

/* One read cannot consume bytes belonging to the following request. */
static int receive_part(int fd, void *data, size_t *used, size_t size)
{
    ssize_t count;
    if (*used == size) return 1;
    count = recv(fd, (uint8_t *) data + *used, size - *used, 0);
    if (count < 0 && (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR))
        return 0;
    if (count <= 0) return -1;
    *used += (size_t) count;
    return *used == size;
}

int gemd_receive(int fd, gemd_io_t *io)
{
    int result;
    if (io->ready || io->output_size) return 0;
    if (!io->header_read) io->started = gem_os_ticks_ms();
    result = receive_part(fd, &io->header, &io->header_read, sizeof(io->header));
    if (result <= 0) return result;
    if (io->header.magic != GEM_RPC_MAGIC ||
        io->header.version != GEM_RPC_VERSION ||
        io->header.size > GEM_RPC_PAYLOAD_MAX) return -1;
    result = receive_part(fd, io->payload, &io->payload_read, io->header.size);
    if (result <= 0) return result;
    io->ready = 1;
    return 1;
}

void gemd_reply(gemd_io_t *io, int32_t status, const void *data, uint32_t size)
{
    gem_rpc_reply_t reply;
    if (size > GEM_RPC_PAYLOAD_MAX || (size && !data)) {
        status = -1;
        size = 0;
    }
    reply = (gem_rpc_reply_t) {GEM_RPC_MAGIC, status, size};
    memcpy(io->output, &reply, sizeof(reply));
    if (size) memcpy(io->output + sizeof(reply), data, size);
    io->output_size = sizeof(reply) + size;
    io->output_sent = 0;
    io->header_read = io->payload_read = 0;
    io->ready = 0;
    io->started = gem_os_ticks_ms();
}

int gemd_send(int fd, gemd_io_t *io)
{
    ssize_t count;
    if (!io->output_size) return 1;
    count = send(fd, io->output + io->output_sent,
        io->output_size - io->output_sent, 0);
    if (count < 0 && (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR))
        return 0;
    if (count <= 0) return -1;
    io->output_sent += (size_t) count;
    if (io->output_sent != io->output_size) return 0;
    io->output_size = io->output_sent = 0;
    return 1;
}
