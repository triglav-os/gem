/*
 * Copies MFDB bytes in bounded chunks and reconstructs output in client memory.
 * Same-buffer copies preserve aliasing; screen MFDBs contain no process
 * pointer.
 *
 * MIT License (see: LICENSE)
 * Copyright (C) 2026 tomaz stih
 */
#include "gem_protocol.h"
#include "gem/gemd.h"
#include <string.h>
static gem_bitmap_form_t pack(const MFDB *mfdb)
{
    gem_bitmap_form_t form = {0};
    if (mfdb && mfdb->fd_addr) {
        form.width = mfdb->fd_w;
        form.height = mfdb->fd_h;
        form.stride = mfdb->fd_wdwidth;
        form.standard = mfdb->fd_stand;
        form.planes = mfdb->fd_nplanes;
        form.memory = 1;
    }
    return form;
}
static int transfer(WORD handle, unsigned slot, MFDB *mfdb, int download)
{
    gem_bitmap_form_t form = pack(mfdb);
    size_t size = gem_bitmap_size(&form);
    if (!form.memory)
        return 1;
    if (!size)
        return 0;
    for (size_t offset = 0; offset < size; offset += GEM_BITMAP_CHUNK) {
        gem_bitmap_chunk_t chunk = {.handle = handle,
                                    .slot = (uint16_t)slot,
                                    .total = (uint32_t)size,
                                    .offset = (uint32_t)offset};
        int32_t status = 0;
        chunk.length =
            (uint32_t)(size - offset < GEM_BITMAP_CHUNK ? size - offset
                                                        : GEM_BITMAP_CHUNK);
        if (!download)
            memcpy(chunk.data, (uint8_t *)mfdb->fd_addr + offset, chunk.length);
        if (!gem_rpc_call(download ? GEM_RPC_BITMAP_GET : GEM_RPC_BITMAP_PUT,
                          &chunk, sizeof(chunk), &status,
                          download ? &chunk : NULL,
                          download ? sizeof(chunk) : 0) ||
            !status)
            return 0;
        if (download)
            memcpy((uint8_t *)mfdb->fd_addr + offset, chunk.data, chunk.length);
    }
    return 1;
}
static WORD copy(WORD handle, WORD operation, WORD mode, const WORD *xy,
                 MFDB *src, MFDB *dst, const WORD *colors)
{
    gem_bitmap_call_t call = {
        .handle = handle, .operation = operation, .mode = mode};
    int32_t status = 0;
    call.source = pack(src);
    call.destination = pack(dst);
    call.alias = src && dst && src->fd_addr && src->fd_addr == dst->fd_addr;
    if (xy)
        memcpy(call.xy, xy, sizeof(call.xy));
    if (colors)
        memcpy(call.colors, colors, sizeof(call.colors));
    if (!transfer(handle, 0, src, 0) ||
        (!call.alias && !transfer(handle, 1, dst, 0)))
        return 0;
    if (!gem_rpc_call(GEM_RPC_BITMAP_COPY, &call, sizeof(call), &status, NULL,
                      0) ||
        !status)
        return 0;
    if (!transfer(handle, call.alias ? 0 : 1, dst, 1))
        return 0;
    return (WORD)status;
}
VOID vro_cpyfm(VDI_HANDLE handle, WORD mode, CONST WORD xy[8], MFDB *src,
               MFDB *dst)
{
    (void)copy(handle, 0, mode, xy, src, dst, NULL);
}
VOID vrt_cpyfm(VDI_HANDLE handle, WORD mode, CONST WORD xy[8], MFDB *src,
               MFDB *dst, CONST WORD colors[2])
{
    (void)copy(handle, 1, mode, xy, src, dst, colors);
}
WORD vr_trnfm(WORD handle, MFDB *src, MFDB *dst)
{
    return copy(handle, 2, 1, NULL, src, dst, NULL);
}
