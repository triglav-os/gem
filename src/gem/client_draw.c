/*
 * Marshals the immediate-mode VDI calls that predate the generated packet
 * transport: workstation open/close, colors, line and fill attributes,
 * clipping, polylines, bars, text and text extents. Arrays are copied into
 * bounded request structures; no application address crosses the socket.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (C) 2026 tomaz stih
 */

#include "gem_protocol.h"
#include "gem/gemd.h"

#include <string.h>

VOID v_opnvwk(WORD work_in[11], VDI_HANDLE *handle, WORD work_out[57])
{
    int32_t status = 0;
    gem_rpc_opnvwk_req_t req;
    gem_rpc_opnvwk_rsp_t rsp;

    if (global[2] == 0) {
        int32_t granted = 0;
        if (!appl_init() ||
            !gem_rpc_call(GEM_RPC_VDI_STANDALONE, NULL, 0, &granted, NULL, 0) ||
            !granted) {
            if (handle)
                *handle = 0;
            return;
        }
    }

    memset(&req, 0, sizeof(req));
    memset(&rsp, 0, sizeof(rsp));
    if (work_in != NULL) {
        memcpy(req.work_in, work_in, sizeof(req.work_in));
    }
    if (!gem_rpc_call(GEM_RPC_V_OPNVWK, &req, sizeof(req), &status, &rsp,
                      sizeof(rsp))) {
        if (handle != NULL) {
            *handle = 0;
        }
        if (work_out != NULL) {
            memset(work_out, 0, sizeof(rsp.work_out));
        }
        return;
    }
    if (handle != NULL) {
        *handle = rsp.handle;
    }
    if (work_out != NULL) {
        memcpy(work_out, rsp.work_out, sizeof(rsp.work_out));
    }
}

VOID v_clsvwk(VDI_HANDLE handle)
{
    int32_t status = 0;
    gem_rpc_handle_req_t req;

    req.handle = handle;
    (void)gem_rpc_call(GEM_RPC_V_CLSVWK, &req, sizeof(req), &status, NULL, 0u);
}

VOID v_clrwk(VDI_HANDLE handle)
{
    int32_t status = 0;
    gem_rpc_handle_req_t req;

    req.handle = handle;
    (void)gem_rpc_call(GEM_RPC_V_CLRWK, &req, sizeof(req), &status, NULL, 0u);
}

WORD v_updwk(WORD handle)
{
    int32_t status = 0;
    gem_rpc_handle_req_t req;

    req.handle = handle;
    if (!gem_rpc_call(GEM_RPC_V_UPDWK, &req, sizeof(req), &status, NULL, 0u)) {
        return 0;
    }
    return (WORD)status;
}

VOID vsf_color(WORD handle, WORD color)
{
    int32_t status = 0;
    gem_rpc_color_req_t req;

    req.handle = handle;
    req.color = color;
    (void)gem_rpc_call(GEM_RPC_VSF_COLOR, &req, sizeof(req), &status, NULL, 0u);
}

WORD vsl_type(WORD handle, WORD style)
{
    int32_t status = 0;
    gem_rpc_handle_word_req_t req;

    req.handle = handle;
    req.value = style;
    if (!gem_rpc_call(GEM_RPC_VSL_TYPE, &req, sizeof(req), &status, NULL, 0u)) {
        return 0;
    }
    return (WORD)status;
}

WORD vsl_width(WORD handle, WORD width)
{
    int32_t status = 0;
    gem_rpc_handle_word_req_t req;

    req.handle = handle;
    req.value = width;
    if (!gem_rpc_call(GEM_RPC_VSL_WIDTH, &req, sizeof(req), &status, NULL,
                      0u)) {
        return 0;
    }
    return (WORD)status;
}

VOID vsl_color(WORD handle, WORD color)
{
    int32_t status = 0;
    gem_rpc_color_req_t req;

    req.handle = handle;
    req.color = color;
    (void)gem_rpc_call(GEM_RPC_VSL_COLOR, &req, sizeof(req), &status, NULL, 0u);
}

WORD vsf_interior(WORD handle, WORD style)
{
    int32_t status = 0;
    gem_rpc_handle_word_req_t req;

    req.handle = handle;
    req.value = style;
    if (!gem_rpc_call(GEM_RPC_VSF_INTERIOR, &req, sizeof(req), &status, NULL,
                      0u)) {
        return 0;
    }
    return (WORD)status;
}

WORD vsf_style(WORD handle, WORD style)
{
    int32_t status = 0;
    gem_rpc_handle_word_req_t req;

    req.handle = handle;
    req.value = style;
    if (!gem_rpc_call(GEM_RPC_VSF_STYLE, &req, sizeof(req), &status, NULL,
                      0u)) {
        return 0;
    }
    return (WORD)status;
}

WORD vsf_perimeter(WORD handle, WORD per_vis)
{
    int32_t status = 0;
    gem_rpc_handle_word_req_t req;

    req.handle = handle;
    req.value = per_vis;
    if (!gem_rpc_call(GEM_RPC_VSF_PERIMETER, &req, sizeof(req), &status, NULL,
                      0u)) {
        return 0;
    }
    return (WORD)status;
}

WORD vswr_mode(WORD handle, WORD mode)
{
    int32_t status = 0;
    gem_rpc_handle_word_req_t req;

    req.handle = handle;
    req.value = mode;
    if (!gem_rpc_call(GEM_RPC_VSWR_MODE, &req, sizeof(req), &status, NULL,
                      0u)) {
        return 0;
    }
    return (WORD)status;
}

WORD vst_font(WORD handle, WORD font)
{
    int32_t status = 0;
    gem_rpc_handle_word_req_t req;

    req.handle = handle;
    req.value = font;
    if (!gem_rpc_call(GEM_RPC_VST_FONT, &req, sizeof(req), &status, NULL, 0u)) {
        return 0;
    }
    return (WORD)status;
}

VOID vst_color(WORD handle, WORD color)
{
    int32_t status = 0;
    gem_rpc_color_req_t req;

    req.handle = handle;
    req.color = color;
    (void)gem_rpc_call(GEM_RPC_VST_COLOR, &req, sizeof(req), &status, NULL, 0u);
}

VOID vs_clip(WORD handle, WORD clip_flag, WORD xy[4])
{
    int32_t status = 0;
    gem_rpc_clip_req_t req;

    memset(&req, 0, sizeof(req));
    req.handle = handle;
    req.enabled = clip_flag;
    if (xy != NULL) {
        memcpy(req.xy, xy, sizeof(req.xy));
    }
    (void)gem_rpc_call(GEM_RPC_VS_CLIP, &req, sizeof(req), &status, NULL, 0u);
}

VOID v_pline(VDI_HANDLE handle, WORD count, CONST WORD *pxy)
{
    int32_t status = 0;
    gem_rpc_pline_req_t req;
    size_t values;

    memset(&req, 0, sizeof(req));
    req.handle = handle;
    req.count = count;
    values = (count > 0) ? (size_t)count * 2u : 0u;
    if (values > sizeof(req.pxy) / sizeof(req.pxy[0])) {
        values = sizeof(req.pxy) / sizeof(req.pxy[0]);
        req.count = (WORD)(values / 2u);
    }
    if (pxy != NULL && values > 0u) {
        memcpy(req.pxy, pxy, values * sizeof(req.pxy[0]));
    }
    (void)gem_rpc_call(GEM_RPC_V_PLINE, &req, sizeof(req), &status, NULL, 0u);
}

VOID v_fillarea(WORD handle, WORD count, WORD xy[])
{
    int32_t status = 0;
    gem_rpc_pline_req_t req;
    size_t values;

    memset(&req, 0, sizeof(req));
    req.handle = handle;
    req.count = count;
    values = (count > 0) ? (size_t)count * 2u : 0u;
    if (values > sizeof(req.pxy) / sizeof(req.pxy[0])) {
        values = sizeof(req.pxy) / sizeof(req.pxy[0]);
        req.count = (WORD)(values / 2u);
    }
    if (xy != NULL && values > 0u) {
        memcpy(req.pxy, xy, values * sizeof(req.pxy[0]));
    }
    (void)gem_rpc_call(GEM_RPC_V_FILLAREA, &req, sizeof(req), &status, NULL,
                       0u);
}

VOID v_bar(VDI_HANDLE handle, CONST WORD xy[4])
{
    int32_t status = 0;
    gem_rpc_rect_req_t req;

    memset(&req, 0, sizeof(req));
    req.handle = handle;
    if (xy != NULL) {
        memcpy(req.xy, xy, sizeof(req.xy));
    }
    (void)gem_rpc_call(GEM_RPC_V_BAR, &req, sizeof(req), &status, NULL, 0u);
}

VOID vr_recfl(WORD handle, WORD pxy[4])
{
    int32_t status = 0;
    gem_rpc_rect_req_t req;

    memset(&req, 0, sizeof(req));
    req.handle = handle;
    if (pxy != NULL) {
        memcpy(req.xy, pxy, sizeof(req.xy));
    }
    (void)gem_rpc_call(GEM_RPC_VR_RECFL, &req, sizeof(req), &status, NULL, 0u);
}

VOID v_gtext(VDI_HANDLE handle, WORD x, WORD y, CONST BYTE *text)
{
    int32_t status = 0;
    gem_rpc_gtext_req_t req;

    memset(&req, 0, sizeof(req));
    req.handle = handle;
    req.x = x;
    req.y = y;
    if (text != NULL) {
        strncpy(req.text, (const char *)text, sizeof(req.text) - 1u);
    }
    (void)gem_rpc_call(GEM_RPC_V_GTEXT, &req, sizeof(req), &status, NULL, 0u);
}

WORD vqt_extent(WORD handle, char *string, WORD extent[8])
{
    int32_t status = 0;
    gem_rpc_vqt_extent_req_t req;
    gem_rpc_vqt_extent_rsp_t rsp;

    memset(&req, 0, sizeof(req));
    memset(&rsp, 0, sizeof(rsp));
    req.handle = handle;
    if (string != NULL) {
        strncpy(req.text, string, sizeof(req.text) - 1u);
    }
    if (!gem_rpc_call(GEM_RPC_VQT_EXTENT, &req, sizeof(req), &status, &rsp,
                      sizeof(rsp))) {
        return 0;
    }
    if (extent != NULL) {
        memcpy(extent, rsp.extent, sizeof(rsp.extent));
    }
    return (WORD)status;
}

VOID v_rbox(WORD handle, WORD xy[4])
{
    WORD pts[10];

    if (xy == NULL) {
        return;
    }

    pts[0] = xy[0];
    pts[1] = xy[1];
    pts[2] = xy[2];
    pts[3] = xy[1];
    pts[4] = xy[2];
    pts[5] = xy[3];
    pts[6] = xy[0];
    pts[7] = xy[3];
    pts[8] = xy[0];
    pts[9] = xy[1];
    v_pline(handle, 5, pts);
}
