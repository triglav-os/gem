/*
 * Marshals presenter-only BGRA overlay operations from libgem clients to the
 * owning gemd display server.
 *
 * MIT License (see: LICENSE)
 * Copyright (C) 2026 tomaz stih
 */
#include "gem/overlay.h"
#include "gem_protocol.h"

#include <stdlib.h>
#include <string.h>

gem_overlay_id_t gem_overlay_create(WORD window_handle, uint16_t width,
                                    uint16_t height)
{
    gem_rpc_overlay_create_req_t req = {window_handle, width, height};
    gem_overlay_id_t id = 0;
    int32_t status = 0;

    if (!gem_rpc_call(GEM_RPC_OVERLAY_CREATE, &req, sizeof(req), &status, &id,
                      sizeof(id)) ||
        status <= 0)
        return 0;
    return id;
}

int gem_overlay_resize(gem_overlay_id_t id, uint16_t width, uint16_t height)
{
    gem_rpc_overlay_resize_req_t req = {id, width, height};
    int32_t status = 0;

    return gem_rpc_call(GEM_RPC_OVERLAY_RESIZE, &req, sizeof(req), &status,
                        NULL, 0) &&
           status > 0;
}

int gem_overlay_map(gem_overlay_id_t id, WORD window_handle)
{
    gem_rpc_overlay_map_req_t req = {id, window_handle};
    int32_t status = 0;

    return gem_rpc_call(GEM_RPC_OVERLAY_MAP, &req, sizeof(req), &status, NULL,
                        0) &&
           status > 0;
}

int gem_overlay_unmap(gem_overlay_id_t id)
{
    gem_rpc_overlay_id_req_t req = {id};
    int32_t status = 0;

    return gem_rpc_call(GEM_RPC_OVERLAY_UNMAP, &req, sizeof(req), &status, NULL,
                        0) &&
           status > 0;
}

int gem_overlay_destroy(gem_overlay_id_t id)
{
    gem_rpc_overlay_id_req_t req = {id};
    int32_t status = 0;

    return gem_rpc_call(GEM_RPC_OVERLAY_DESTROY, &req, sizeof(req), &status,
                        NULL, 0) &&
           status > 0;
}

int gem_overlay_update(gem_overlay_id_t id, uint16_t x, uint16_t y,
                       uint16_t width, uint16_t height, const uint8_t *bgra,
                       uint16_t pitch)
{
    uint8_t *request;
    uint16_t row;

    if (bgra == NULL || width == 0u || height == 0u ||
        pitch < (uint32_t)width * 4u)
        return 0;
    row = 0;
    while (row < height) {
        gem_rpc_overlay_update_req_t header;
        uint32_t rows = (GEM_RPC_PAYLOAD_MAX - sizeof(header)) / pitch;
        uint16_t chunk_height;
        size_t bytes;
        int32_t status = 0;

        if (rows == 0u)
            return 0;
        chunk_height =
            (uint16_t)(((uint32_t)height - row < rows) ? (uint32_t)height - row
                                                       : rows);
        header = (gem_rpc_overlay_update_req_t){
            id, x, (uint16_t)(y + row), width, chunk_height, pitch, 0};
        bytes = (size_t)pitch * chunk_height;
        request = malloc(sizeof(header) + bytes);
        if (request == NULL)
            return 0;
        memcpy(request, &header, sizeof(header));
        memcpy(request + sizeof(header), bgra + (size_t)row * pitch, bytes);
        if (!gem_rpc_call(GEM_RPC_OVERLAY_UPDATE, request,
                          (uint32_t)(sizeof(header) + bytes), &status, NULL,
                          0) ||
            status <= 0) {
            free(request);
            return 0;
        }
        free(request);
        row = (uint16_t)(row + chunk_height);
    }
    return 1;
}
