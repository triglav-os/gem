/*
 * Implements server-side ownership, clipping and presenter synchronization
 * for application BGRA overlays.
 *
 * MIT License (see: LICENSE)
 * Copyright (C) 2026 tomaz stih
 */
#include "overlay.h"

#include "platform/raster.h"

#include <stdlib.h>
#include <string.h>

enum { gemd_overlay_max = 8, gemd_overlay_limit = 8 * 1024 * 1024 };
typedef struct gemd_overlay {
    uint32_t id;
    WORD owner;
    WORD window;
    uint16_t width;
    uint16_t height;
    uint8_t *pixels;
} gemd_overlay_t;

static gemd_overlay_t g_overlays[gemd_overlay_max];
static uint32_t g_next_overlay_id = 1u;

void gemd_overlay_window_top_changed(void)
{
    for (size_t i = 0; i < gemd_overlay_max; ++i) {
        gemd_overlay_t *overlay = &g_overlays[i];
        aes_window_t *target;
        int occlusion[16][4];
        unsigned count = 0;

        if (overlay->id == 0u || overlay->window == 0)
            continue;
        target = aes_find_window(overlay->window);
        if (target == NULL || target->open == 0) {
            gem_raster_overlay_set_visible(overlay->id, 0);
            continue;
        }
        for (size_t w = 0; w < AES_MAX_WINDOWS && count < 16u; ++w) {
            aes_window_t *cover = &aes_state.windows[w];
            GRECT intersection;
            if (cover->used == 0 || cover->open == 0 ||
                cover->handle == target->handle ||
                cover->z_order <= target->z_order)
                continue;
            intersection.g_x = cover->outer.g_x;
            intersection.g_y = cover->outer.g_y;
            intersection.g_w = cover->outer.g_w;
            intersection.g_h = cover->outer.g_h;
            if (intersection.g_x < target->work.g_x) {
                intersection.g_w -= target->work.g_x - intersection.g_x;
                intersection.g_x = target->work.g_x;
            }
            if (intersection.g_y < target->work.g_y) {
                intersection.g_h -= target->work.g_y - intersection.g_y;
                intersection.g_y = target->work.g_y;
            }
            if (intersection.g_x + intersection.g_w >
                target->work.g_x + target->work.g_w)
                intersection.g_w =
                    target->work.g_x + target->work.g_w - intersection.g_x;
            if (intersection.g_y + intersection.g_h >
                target->work.g_y + target->work.g_h)
                intersection.g_h =
                    target->work.g_y + target->work.g_h - intersection.g_y;
            if (intersection.g_w > 0 && intersection.g_h > 0) {
                occlusion[count][0] = intersection.g_x;
                occlusion[count][1] = intersection.g_y;
                occlusion[count][2] = intersection.g_w;
                occlusion[count][3] = intersection.g_h;
                ++count;
            }
        }
        gem_raster_overlay_set_visible(overlay->id, 1);
        gem_raster_overlay_set_occlusion(overlay->id, &occlusion[0][0], count);
    }
}

static gemd_overlay_t *find_overlay(uint32_t id, WORD owner)
{
    for (size_t i = 0; i < gemd_overlay_max; ++i)
        if (g_overlays[i].id == id && g_overlays[i].owner == owner)
            return &g_overlays[i];
    return NULL;
}

static int map_overlay(gemd_overlay_t *overlay, WORD window)
{
    aes_window_t *window_state = aes_find_window(window);

    if (window_state == NULL || window_state->owner != overlay->owner)
        return 0;
    overlay->window = window;
    if (!gem_raster_overlay_create(overlay->id, window_state->work.g_x,
                                   window_state->work.g_y, overlay->width,
                                   overlay->height))
        return 0;
    gemd_overlay_window_top_changed();
    return 1;
}

static int valid_size(uint16_t width, uint16_t height)
{
    return width != 0u && height != 0u &&
           (size_t)width * height * 4u <= gemd_overlay_limit;
}

int32_t gemd_overlay_dispatch(gemd_session_t *session,
                              const gem_rpc_header_t *header,
                              const uint8_t *payload, uint8_t *response,
                              uint32_t *response_size)
{
    *response_size = 0u;
    switch ((gem_rpc_opcode_t)header->opcode) {
        case GEM_RPC_OVERLAY_CREATE: {
            const gem_rpc_overlay_create_req_t *req =
                (const gem_rpc_overlay_create_req_t *)payload;
            aes_window_t *window = aes_find_window(req->window);
            gemd_overlay_t *overlay = NULL;

            if (!valid_size(req->width, req->height) || window == NULL ||
                window->owner != session->app_id)
                return 0;
            for (size_t i = 0; i < gemd_overlay_max; ++i)
                if (g_overlays[i].id == 0u) {
                    overlay = &g_overlays[i];
                    break;
                }
            if (overlay == NULL)
                return 0;
            overlay->pixels = calloc(1u, (size_t)req->width * req->height * 4u);
            if (overlay->pixels == NULL)
                return 0;
            overlay->id = g_next_overlay_id++;
            if (overlay->id == 0u)
                overlay->id = g_next_overlay_id++;
            overlay->owner = session->app_id;
            overlay->width = req->width;
            overlay->height = req->height;
            if (!map_overlay(overlay, req->window)) {
                free(overlay->pixels);
                memset(overlay, 0, sizeof(*overlay));
                return 0;
            }
            memcpy(response, &overlay->id, sizeof(overlay->id));
            *response_size = sizeof(overlay->id);
            return 1;
        }
        case GEM_RPC_OVERLAY_RESIZE: {
            const gem_rpc_overlay_resize_req_t *req =
                (const gem_rpc_overlay_resize_req_t *)payload;
            gemd_overlay_t *overlay = find_overlay(req->id, session->app_id);
            uint8_t *pixels;

            if (overlay == NULL || !valid_size(req->width, req->height))
                return 0;
            pixels = calloc(1u, (size_t)req->width * req->height * 4u);
            if (pixels == NULL)
                return 0;
            free(overlay->pixels);
            overlay->pixels = pixels;
            overlay->width = req->width;
            overlay->height = req->height;
            return map_overlay(overlay, overlay->window) ? 1 : 0;
        }
        case GEM_RPC_OVERLAY_MAP: {
            const gem_rpc_overlay_map_req_t *req =
                (const gem_rpc_overlay_map_req_t *)payload;
            gemd_overlay_t *overlay = find_overlay(req->id, session->app_id);

            if (overlay == NULL)
                return 0;
            return map_overlay(overlay, req->window) ? 1 : 0;
        }
        case GEM_RPC_OVERLAY_UNMAP: {
            const gem_rpc_overlay_id_req_t *req =
                (const gem_rpc_overlay_id_req_t *)payload;
            gemd_overlay_t *overlay = find_overlay(req->id, session->app_id);

            if (overlay == NULL)
                return 0;
            gem_raster_overlay_destroy(overlay->id);
            overlay->window = 0;
            return 1;
        }
        case GEM_RPC_OVERLAY_DESTROY: {
            const gem_rpc_overlay_id_req_t *req =
                (const gem_rpc_overlay_id_req_t *)payload;
            gemd_overlay_t *overlay = find_overlay(req->id, session->app_id);

            if (overlay == NULL)
                return 0;
            gem_raster_overlay_destroy(overlay->id);
            free(overlay->pixels);
            memset(overlay, 0, sizeof(*overlay));
            return 1;
        }
        case GEM_RPC_OVERLAY_UPDATE: {
            const gem_rpc_overlay_update_req_t *req =
                (const gem_rpc_overlay_update_req_t *)payload;
            gemd_overlay_t *overlay = find_overlay(req->id, session->app_id);
            const uint8_t *pixels = payload + sizeof(*req);

            if (overlay == NULL || overlay->window == 0 ||
                req->x + req->width > overlay->width ||
                req->y + req->height > overlay->height)
                return 0;
            for (uint16_t row = 0; row < req->height; ++row)
                memcpy(
                    overlay->pixels +
                        ((size_t)(req->y + row) * overlay->width + req->x) * 4u,
                    pixels + (size_t)row * req->pitch, (size_t)req->width * 4u);
            return gem_raster_overlay_update(overlay->id, req->x, req->y,
                                             req->width, req->height, pixels,
                                             req->pitch)
                       ? 1
                       : 0;
        }
        default:
            return -1;
    }
}

void gemd_overlay_cleanup_app(WORD app_id)
{
    for (size_t i = 0; i < gemd_overlay_max; ++i) {
        if (g_overlays[i].id != 0u && g_overlays[i].owner == app_id) {
            gem_raster_overlay_destroy(g_overlays[i].id);
            free(g_overlays[i].pixels);
            memset(&g_overlays[i], 0, sizeof(g_overlays[i]));
        }
    }
}
