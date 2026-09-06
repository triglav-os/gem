/*
 * Owns per-connection MFDB storage and dispatches pointer-free raster copies.
 * Uploads initialize storage and downloads cannot expose another connection.
 *
 * MIT License (see: LICENSE)
 * Copyright (C) 2026 tomaz stih
 */
#include "gem/gemd.h"
#include <stdlib.h>
#include <string.h>
void gem_bitmap_free(gem_bitmap_store_t *store)
{
    for (int i = 0; i < 2; ++i) {
        free(store->bytes[i]);
        store->bytes[i] = NULL;
        store->size[i] = 0;
    }
}
WORD gem_bitmap_transfer(gem_bitmap_store_t *store, gem_bitmap_chunk_t *chunk,
                         int download)
{
    unsigned slot = chunk->slot;
    if (slot > 1 || chunk->total > GEM_BITMAP_LIMIT ||
        chunk->length > GEM_BITMAP_CHUNK || chunk->offset > chunk->total ||
        chunk->length > chunk->total - chunk->offset)
        return 0;
    if (!download && chunk->offset == 0) {
        uint8_t *bytes = calloc(chunk->total ? chunk->total : 1, 1);
        if (!bytes)
            return 0;
        free(store->bytes[slot]);
        store->bytes[slot] = bytes;
        store->size[slot] = chunk->total;
    }
    if (!store->bytes[slot] || store->size[slot] != chunk->total)
        return 0;
    if (download)
        memcpy(chunk->data, store->bytes[slot] + chunk->offset, chunk->length);
    else
        memcpy(store->bytes[slot] + chunk->offset, chunk->data, chunk->length);
    return 1;
}
static int unpack(const gem_bitmap_form_t *form, MFDB *mfdb,
                  gem_bitmap_store_t *store, int slot)
{
    memset(mfdb, 0, sizeof(*mfdb));
    if (form->memory == 0)
        return 1;
    if (!gem_bitmap_size(form) || gem_bitmap_size(form) != store->size[slot] ||
        !store->bytes[slot])
        return 0;
    mfdb->fd_addr = store->bytes[slot];
    mfdb->fd_w = form->width;
    mfdb->fd_h = form->height;
    mfdb->fd_wdwidth = form->stride;
    mfdb->fd_stand = form->standard;
    mfdb->fd_nplanes = form->planes;
    return 1;
}
WORD gem_bitmap_execute(gem_bitmap_store_t *store,
                        const gem_bitmap_call_t *call)
{
    MFDB source, destination;
    if (!unpack(&call->source, &source, store, 0) ||
        !unpack(&call->destination, &destination, store, call->alias ? 0 : 1))
        return 0;
    switch (call->operation) {
        case 0:
            vro_cpyfm(call->handle, call->mode, call->xy, &source,
                      &destination);
            break;
        case 1:
            vrt_cpyfm(call->handle, call->mode, call->xy, &source, &destination,
                      call->colors);
            break;
        case 2:
            return vr_trnfm(call->handle, &source, &destination);
        default:
            return 0;
    }
    return 1;
}
