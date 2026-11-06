/*
 * Executes the VDI family of gemd requests against the shared server
 * workstation: attributes, primitives, text, bitmaps and the extended VDI
 * packet, with raster output confined to the session's visible areas.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (C) 2026 tomaz stih
 */

#include "session.h"

#include <string.h>

/* Execute one VDI-family request; returns 0 when the opcode belongs to the
 * AES family so the caller dispatches it there. */
int gemd_dispatch_vdi(gemd_session_t *session, const gem_rpc_header_t *header,
                      const uint8_t *payload, uint8_t *response,
                      uint32_t *response_size, int32_t *status_out)
{
    int32_t status = 0;

    switch ((gem_rpc_opcode_t)header->opcode) {
        case GEM_RPC_BITMAP_PUT:
        case GEM_RPC_BITMAP_GET: {
            gem_bitmap_chunk_t chunk;
            memcpy(&chunk, payload, sizeof(chunk));
            status = gem_bitmap_transfer(&session->bitmaps, &chunk,
                                         header->opcode == GEM_RPC_BITMAP_GET);
            if (header->opcode == GEM_RPC_BITMAP_GET) {
                memcpy(response, &chunk, sizeof(chunk));
                *response_size = sizeof(chunk);
            }
            break;
        }

        case GEM_RPC_BITMAP_COPY:
            status = gem_bitmap_execute(&session->bitmaps,
                                        (const gem_bitmap_call_t *)payload);
            break;

        case GEM_RPC_VDI_EXT: {
            gem_vdi_packet_t *packet = (gem_vdi_packet_t *)response;
            memcpy(packet, payload, sizeof(*packet));
            status = gem_vdi_dispatch(packet);
            *response_size = sizeof(*packet);
            break;
        }

        case GEM_RPC_VDI_STANDALONE:
            status = 1;
            for (size_t i = 0; i < GEMD_MAX_SESSIONS; ++i) {
                if (&g_sessions[i] != session && g_sessions[i].app_id)
                    status = 0;
            }
            if (status)
                session->standalone = 1;
            break;

        case GEM_RPC_V_HIDE_C:
            v_hide_c(g_server_vdi_handle);
            status = 1;
            break;

        case GEM_RPC_V_SHOW_C:
            v_show_c(g_server_vdi_handle,
                     ((const gem_rpc_handle_word_req_t *)payload)->value);
            status = 1;
            break;

        case GEM_RPC_VQT_FONTINFO: {
            const gem_rpc_handle_req_t *req =
                (const gem_rpc_handle_req_t *)payload;
            gem_rpc_words16_t *rsp = (gem_rpc_words16_t *)response;
            memset(rsp, 0, sizeof(*rsp));
            status =
                vqt_fontinfo(req->handle, &rsp->values[0], &rsp->values[1],
                             &rsp->values[2], &rsp->values[7], &rsp->values[8]);
            *response_size = sizeof(*rsp);
        } break;

        case GEM_RPC_V_OPNVWK: {
            gem_rpc_opnvwk_rsp_t *rsp = (gem_rpc_opnvwk_rsp_t *)response;

            memset(rsp, 0, sizeof(*rsp));
            if (!gemd_open_server_vdi(rsp->work_out)) {
                status = 0;
                break;
            }
            if (session->vdi_open == 0) {
                session->vdi_open = 1;
                ++g_server_vdi_refs;
            }
            rsp->handle = g_server_vdi_handle;
            status = rsp->handle;
            *response_size = (uint32_t)sizeof(*rsp);
        } break;

        case GEM_RPC_V_CLSVWK:
            session->drawing.initialized = 0;
            if (session->vdi_open != 0) {
                session->vdi_open = 0;
                if (g_server_vdi_refs > 0) {
                    --g_server_vdi_refs;
                }
            }
            status = 1;
            break;

        case GEM_RPC_V_CLRWK:
            if (g_server_vdi_handle != 0) {
                v_clrwk(g_server_vdi_handle);
                status = 1;
            }
            break;

        case GEM_RPC_V_UPDWK:
            if (g_server_vdi_handle != 0) {
                status = v_updwk(g_server_vdi_handle);
            }
            break;

        case GEM_RPC_VSL_TYPE: {
            const gem_rpc_handle_word_req_t *req =
                (const gem_rpc_handle_word_req_t *)payload;

            status = vsl_type(g_server_vdi_handle, req->value);
        } break;

        case GEM_RPC_VSL_WIDTH: {
            const gem_rpc_handle_word_req_t *req =
                (const gem_rpc_handle_word_req_t *)payload;

            status = vsl_width(g_server_vdi_handle, req->value);
        } break;

        case GEM_RPC_VSL_COLOR: {
            const gem_rpc_color_req_t *req =
                (const gem_rpc_color_req_t *)payload;

            vsl_color(g_server_vdi_handle, req->color);
            status = 1;
        } break;

        case GEM_RPC_VSF_COLOR: {
            const gem_rpc_color_req_t *req =
                (const gem_rpc_color_req_t *)payload;

            vsf_color(g_server_vdi_handle, req->color);
            aes_trace("gemd vsf_color handle=%d color=%d", g_server_vdi_handle,
                      req->color);
            status = 1;
        } break;

        case GEM_RPC_VSF_INTERIOR: {
            const gem_rpc_handle_word_req_t *req =
                (const gem_rpc_handle_word_req_t *)payload;

            status = vsf_interior(g_server_vdi_handle, req->value);
        } break;

        case GEM_RPC_VSF_STYLE: {
            const gem_rpc_handle_word_req_t *req =
                (const gem_rpc_handle_word_req_t *)payload;

            status = vsf_style(g_server_vdi_handle, req->value);
        } break;

        case GEM_RPC_VSF_PERIMETER: {
            const gem_rpc_handle_word_req_t *req =
                (const gem_rpc_handle_word_req_t *)payload;

            status = vsf_perimeter(g_server_vdi_handle, req->value);
        } break;

        case GEM_RPC_VSWR_MODE: {
            const gem_rpc_handle_word_req_t *req =
                (const gem_rpc_handle_word_req_t *)payload;

            status = vswr_mode(g_server_vdi_handle, req->value);
        } break;

        case GEM_RPC_VST_FONT: {
            const gem_rpc_handle_word_req_t *req =
                (const gem_rpc_handle_word_req_t *)payload;

            status = vst_font(g_server_vdi_handle, req->value);
        } break;

        case GEM_RPC_VST_COLOR: {
            const gem_rpc_color_req_t *req =
                (const gem_rpc_color_req_t *)payload;

            vst_color(g_server_vdi_handle, req->color);
            aes_trace("gemd vst_color handle=%d color=%d", g_server_vdi_handle,
                      req->color);
            status = 1;
        } break;

        case GEM_RPC_VS_CLIP: {
            const gem_rpc_clip_req_t *req = (const gem_rpc_clip_req_t *)payload;

            vs_clip(g_server_vdi_handle, req->enabled, (WORD *)req->xy);
            aes_trace("gemd vs_clip handle=%d enabled=%d rect=%d,%d-%d,%d",
                      g_server_vdi_handle, req->enabled, req->xy[0], req->xy[1],
                      req->xy[2], req->xy[3]);
            status = 1;
        } break;

        case GEM_RPC_V_PLINE: {
            const gem_rpc_pline_req_t *req =
                (const gem_rpc_pline_req_t *)payload;

            v_pline(g_server_vdi_handle, req->count, req->pxy);
            status = 1;
        } break;

        case GEM_RPC_V_FILLAREA: {
            const gem_rpc_pline_req_t *req =
                (const gem_rpc_pline_req_t *)payload;

            v_fillarea(g_server_vdi_handle, req->count, (WORD *)req->pxy);
            status = 1;
        } break;

        case GEM_RPC_V_BAR: {
            const gem_rpc_rect_req_t *req = (const gem_rpc_rect_req_t *)payload;

            v_bar(g_server_vdi_handle, req->xy);
            status = 1;
        } break;

        case GEM_RPC_VR_RECFL: {
            const gem_rpc_rect_req_t *req = (const gem_rpc_rect_req_t *)payload;

            vr_recfl(g_server_vdi_handle, (WORD *)req->xy);
            aes_trace("gemd vr_recfl handle=%d rect=%d,%d-%d,%d",
                      g_server_vdi_handle, req->xy[0], req->xy[1], req->xy[2],
                      req->xy[3]);
            status = 1;
        } break;

        case GEM_RPC_V_GTEXT: {
            const gem_rpc_gtext_req_t *req =
                (const gem_rpc_gtext_req_t *)payload;
            char text[GEM_RPC_TEXT_MAX];

            memcpy(text, req->text, sizeof(text));
            text[sizeof(text) - 1u] = '\0';
            v_gtext(g_server_vdi_handle, req->x, req->y, (const BYTE *)text);
            aes_trace("gemd v_gtext handle=%d pos=%d,%d text=\"%s\"",
                      g_server_vdi_handle, req->x, req->y, text);
            status = 1;
        } break;

        case GEM_RPC_VQT_EXTENT: {
            const gem_rpc_vqt_extent_req_t *req =
                (const gem_rpc_vqt_extent_req_t *)payload;
            gem_rpc_vqt_extent_rsp_t *rsp =
                (gem_rpc_vqt_extent_rsp_t *)response;
            char text[GEM_RPC_TEXT_MAX];

            memset(rsp, 0, sizeof(*rsp));
            memcpy(text, req->text, sizeof(text));
            text[sizeof(text) - 1u] = '\0';
            status = vqt_extent(g_server_vdi_handle, text, rsp->extent);
            *response_size = (uint32_t)sizeof(*rsp);
        } break;
        default:
            return 0;
    }
    *status_out = status;
    return 1;
}

/* Raster writes are confined to the client's visible work areas. Only the
 * desktop owner also paints uncovered desktop; AES alone paints shared chrome.
 */
int32_t gemd_draw_owned(gemd_session_t *session, const gem_rpc_header_t *header,
                        const uint8_t *payload, uint8_t *response,
                        uint32_t *response_size)
{
    size_t i;
    GRECT requested;
    vdi_rect_t clip;
    vdi_get_active_clip_rect(&clip);
    aes_set_rect(&requested, clip.x0, clip.y0, (WORD)(clip.x1 - clip.x0 + 1),
                 (WORD)(clip.y1 - clip.y0 + 1));
    if (session->standalone)
        return gemd_dispatch(session, header, payload, response, response_size);
    vdi_begin_update();
    for (i = 0; i <= AES_MAX_WINDOWS; ++i) {
        const aes_window_t *window =
            i < AES_MAX_WINDOWS ? &aes_state.windows[i] : NULL;
        GRECT base, damage, visible[64];
        WORD count, j;
        if (window) {
            if (!window->used || !window->open ||
                window->owner != session->app_id)
                continue;
            base = window->work;
        } else {
            if (aes_state.desktop_owner_app_id != session->app_id)
                continue;
            aes_desktop_rect(&base);
        }
        if (!aes_intersect_rects(&base, &requested, &damage))
            continue;
        count = aes_clip_visible_rects(window, &damage, visible, 64);
        for (j = 0; j < count; ++j) {
            WORD xy[4] = {visible[j].g_x, visible[j].g_y,
                          (WORD)(visible[j].g_x + visible[j].g_w - 1),
                          (WORD)(visible[j].g_y + visible[j].g_h - 1)};
            vs_clip(g_server_vdi_handle, 1, xy);
            if (header->opcode == GEM_RPC_V_CLRWK) {
                vdi_state.fill_color = 0;
                vdi_compat.write_mode = MD_REPLACE;
                vdi_compat.fill_interior = FIS_SOLID;
                vdi_compat.fill_perimeter = 0;
                v_bar(g_server_vdi_handle, xy);
            } else {
                (void)gemd_dispatch(session, header, payload, response,
                                    response_size);
            }
        }
    }
    vdi_end_update();
    return 1;
}
