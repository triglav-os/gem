/*
 * Marshals mouse state, font metrics, scrap paths and file selectors.
 * Pointer outputs are copied back to client memory, never sent as addresses.
 *
 * MIT License (see: LICENSE)
 * Copyright (C) 2026 tomaz stih
 */

#include "gem_protocol.h"

#include <string.h>

VOID v_hide_c(VDI_HANDLE handle)
{
    gem_rpc_handle_req_t req = {handle};
    (void)gem_rpc_call(GEM_RPC_V_HIDE_C, &req, sizeof(req), NULL, NULL, 0);
}

VOID v_show_c(VDI_HANDLE handle, WORD reset)
{
    gem_rpc_handle_word_req_t req = {handle, reset};
    (void)gem_rpc_call(GEM_RPC_V_SHOW_C, &req, sizeof(req), NULL, NULL, 0);
}

VOID graf_mkstate(WORD *mx, WORD *my, WORD *buttons, WORD *keys)
{
    gem_rpc_words8_t rsp = {{0}};

    if (!gem_rpc_call(GEM_RPC_GRAF_MKSTATE, NULL, 0, NULL, &rsp, sizeof(rsp))) {
        return;
    }
    if (mx != NULL)
        *mx = rsp.values[0];
    if (my != NULL)
        *my = rsp.values[1];
    if (buttons != NULL)
        *buttons = rsp.values[2];
    if (keys != NULL)
        *keys = rsp.values[3];
}

WORD vqt_fontinfo(WORD handle, WORD *min_ade, WORD *max_ade, WORD distances[],
                  WORD *max_width, WORD effects[])
{
    gem_rpc_handle_req_t req = {handle};
    gem_rpc_words16_t rsp = {{0}};
    int32_t status = 0;

    if (!gem_rpc_call(GEM_RPC_VQT_FONTINFO, &req, sizeof(req), &status, &rsp,
                      sizeof(rsp))) {
        return 0;
    }
    if (min_ade != NULL)
        *min_ade = rsp.values[0];
    if (max_ade != NULL)
        *max_ade = rsp.values[1];
    if (distances != NULL)
        memcpy(distances, &rsp.values[2], 5 * sizeof(WORD));
    if (max_width != NULL)
        *max_width = rsp.values[7];
    if (effects != NULL)
        memcpy(effects, &rsp.values[8], 3 * sizeof(WORD));
    return (WORD)status;
}

WORD scrp_read(char *path)
{
    gem_rpc_path_t rsp = {{0}};
    int32_t status = 0;

    if (path == NULL ||
        !gem_rpc_call(GEM_RPC_SCRP_READ, NULL, 0, &status, &rsp, sizeof(rsp))) {
        return 0;
    }
    rsp.text[sizeof(rsp.text) - 1] = '\0';
    strcpy(path, rsp.text);
    return (WORD)status;
}

WORD scrp_write(char *path)
{
    gem_rpc_path_t req = {{0}};
    int32_t status = 0;

    if (path == NULL || strlen(path) >= sizeof(req.text))
        return 0;
    strcpy(req.text, path);
    if (!gem_rpc_call(GEM_RPC_SCRP_WRITE, &req, sizeof(req), &status, NULL, 0))
        return 0;
    return (WORD)status;
}

WORD fsel_input(char *path, char *name, WORD *button)
{
    gem_rpc_fsel_t req = {{0}, {0}, 0};
    gem_rpc_fsel_t rsp = {{0}, {0}, 0};
    int32_t status = 0;

    if (path == NULL || name == NULL || button == NULL ||
        strlen(path) >= sizeof(req.path) || strlen(name) >= sizeof(req.name))
        return 0;
    strcpy(req.path, path);
    strcpy(req.name, name);
    if (!gem_rpc_call(GEM_RPC_FSEL_INPUT, &req, sizeof(req), &status, &rsp,
                      sizeof(rsp)))
        return 0;
    rsp.path[sizeof(rsp.path) - 1] = '\0';
    rsp.name[sizeof(rsp.name) - 1] = '\0';
    strcpy(path, rsp.path);
    strcpy(name, rsp.name);
    *button = rsp.button;
    return (WORD)status;
}
