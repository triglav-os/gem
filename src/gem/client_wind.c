/*
 * Marshals the AES window calls: creation, opening, closing, deletion,
 * field queries and updates, hit testing, update locking and border/work
 * conversion. String-valued fields travel through the copied-string form.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (C) 2026 tomaz stih
 */

#include "gem_protocol.h"
#include "gem/gemd.h"

#include <string.h>

WORD wind_create(UWORD kind, WORD x, WORD y, WORD w, WORD h)
{
    int32_t status = 0;
    gem_rpc_wind_create_req_t req;

    req.kind = kind;
    req.x = x;
    req.y = y;
    req.w = w;
    req.h = h;
    if (!gem_rpc_call(GEM_RPC_WIND_CREATE, &req, sizeof(req), &status, NULL,
                      0u)) {
        return 0;
    }
    return (WORD)status;
}

WORD wind_open(WORD handle, WORD x, WORD y, WORD w, WORD h)
{
    int32_t status = 0;
    gem_rpc_wind_open_req_t req;

    req.handle = handle;
    req.x = x;
    req.y = y;
    req.w = w;
    req.h = h;
    if (!gem_rpc_call(GEM_RPC_WIND_OPEN, &req, sizeof(req), &status, NULL,
                      0u)) {
        return 0;
    }
    return (WORD)status;
}

WORD wind_close(WORD handle)
{
    int32_t status = 0;
    gem_rpc_handle_req_t req;

    req.handle = handle;
    if (!gem_rpc_call(GEM_RPC_WIND_CLOSE, &req, sizeof(req), &status, NULL,
                      0u)) {
        return 0;
    }
    return (WORD)status;
}

WORD wind_delete(WORD handle)
{
    int32_t status = 0;
    gem_rpc_handle_req_t req;

    req.handle = handle;
    if (!gem_rpc_call(GEM_RPC_WIND_DELETE, &req, sizeof(req), &status, NULL,
                      0u)) {
        return 0;
    }
    return (WORD)status;
}

WORD wind_get(WORD handle, WORD field, WORD *w1, WORD *w2, WORD *w3, WORD *w4)
{
    int32_t status = 0;
    gem_rpc_wind_get_req_t req;
    gem_rpc_wind_get_rsp_t rsp;

    req.handle = handle;
    req.field = field;
    memset(&rsp, 0, sizeof(rsp));
    if (!gem_rpc_call(GEM_RPC_WIND_GET, &req, sizeof(req), &status, &rsp,
                      sizeof(rsp))) {
        return 0;
    }
    if (w1 != NULL) {
        *w1 = rsp.w1;
    }
    if (w2 != NULL) {
        *w2 = rsp.w2;
    }
    if (w3 != NULL) {
        *w3 = rsp.w3;
    }
    if (w4 != NULL) {
        *w4 = rsp.w4;
    }
    return (WORD)status;
}

WORD wind_set(WORD handle, WORD field, WORD w1, WORD w2, WORD w3, WORD w4)
{
    int32_t status = 0;
    gem_rpc_wind_set_req_t req;

    req.handle = handle;
    req.field = field;
    req.w1 = w1;
    req.w2 = w2;
    req.w3 = w3;
    req.w4 = w4;
    if (!gem_rpc_call(GEM_RPC_WIND_SET, &req, sizeof(req), &status, NULL, 0u)) {
        return 0;
    }
    return (WORD)status;
}

WORD wind_set_str(WORD handle, WORD field, const char *text)
{
    int32_t status = 0;
    gem_rpc_wind_set_str_req_t req;

    memset(&req, 0, sizeof(req));
    req.handle = handle;
    req.field = field;
    if (text != NULL) {
        strncpy(req.text, text, sizeof(req.text) - 1u);
    }
    if (!gem_rpc_call(GEM_RPC_WIND_SET_STR, &req, sizeof(req), &status, NULL,
                      0u)) {
        return 0;
    }
    return (WORD)status;
}

WORD wind_find(WORD x, WORD y)
{
    int32_t status = 0;
    gem_rpc_wind_find_req_t req;

    req.x = x;
    req.y = y;
    if (!gem_rpc_call(GEM_RPC_WIND_FIND, &req, sizeof(req), &status, NULL,
                      0u)) {
        return 0;
    }
    return (WORD)status;
}

WORD wind_update(WORD flag)
{
    int32_t status = 0;
    gem_rpc_wind_update_req_t req;

    req.flag = flag;
    if (!gem_rpc_call(GEM_RPC_WIND_UPDATE, &req, sizeof(req), &status, NULL,
                      0u)) {
        return 0;
    }
    return (WORD)status;
}

WORD wind_calc(WORD type, UWORD kind, WORD inx, WORD iny, WORD inw, WORD inh,
               WORD *outx, WORD *outy, WORD *outw, WORD *outh)
{
    int32_t status = 0;
    gem_rpc_wind_calc_req_t req;
    gem_rpc_wind_calc_rsp_t rsp;

    req.type = type;
    req.kind = kind;
    req.inx = inx;
    req.iny = iny;
    req.inw = inw;
    req.inh = inh;
    memset(&rsp, 0, sizeof(rsp));
    if (!gem_rpc_call(GEM_RPC_WIND_CALC, &req, sizeof(req), &status, &rsp,
                      sizeof(rsp))) {
        return 0;
    }
    if (outx != NULL) {
        *outx = rsp.outx;
    }
    if (outy != NULL) {
        *outy = rsp.outy;
    }
    if (outw != NULL) {
        *outw = rsp.outw;
    }
    if (outh != NULL) {
        *outh = rsp.outh;
    }
    return (WORD)status;
}
