/*
 * Declares the private GEM client RPC helpers shared by the libgem
 * wrappers. This layer marshals a practical subset of AES and VDI
 * calls over a Unix-domain socket to the hosted GEM server.
 *
 * MIT License (see: LICENSE)
 * Copyright (C) 2026 tomaz stih
 */

#ifndef GEM_SRC_GEM__GEM_H
#define GEM_SRC_GEM__GEM_H

#include "gem/gem.h"

#include <stddef.h>
#include <stdint.h>

#define GEMD_SOCKET_PATH "/tmp/gemd.sock"
#define GEM_RPC_MAGIC 0x47454d31u
#define GEM_RPC_VERSION 1u
#define GEM_RPC_TEXT_MAX 512u
#define GEM_RPC_PAYLOAD_MAX 1024u

typedef enum gem_rpc_opcode {
    GEM_RPC_APPL_INIT = 1,
    GEM_RPC_APPL_EXIT,
    GEM_RPC_EVNT_MESAG,
    GEM_RPC_GRAF_HANDLE,
    GEM_RPC_V_OPNVWK,
    GEM_RPC_V_CLSVWK,
    GEM_RPC_V_CLRWK,
    GEM_RPC_V_UPDWK,
    GEM_RPC_VSF_COLOR,
    GEM_RPC_VST_COLOR,
    GEM_RPC_VS_CLIP,
    GEM_RPC_V_PLINE,
    GEM_RPC_V_BAR,
    GEM_RPC_VR_RECFL,
    GEM_RPC_V_GTEXT,
    GEM_RPC_WIND_CREATE,
    GEM_RPC_WIND_OPEN,
    GEM_RPC_WIND_CLOSE,
    GEM_RPC_WIND_DELETE,
    GEM_RPC_WIND_GET,
    GEM_RPC_WIND_SET,
    GEM_RPC_WIND_SET_STR,
    GEM_RPC_WIND_FIND,
    GEM_RPC_WIND_UPDATE,
    GEM_RPC_WIND_CALC
} gem_rpc_opcode_t;

typedef struct gem_rpc_header {
    uint32_t magic;
    uint16_t version;
    uint16_t opcode;
    uint32_t size;
} gem_rpc_header_t;

typedef struct gem_rpc_reply {
    uint32_t magic;
    int32_t status;
    uint32_t size;
} gem_rpc_reply_t;

typedef struct gem_rpc_words16 {
    WORD values[16];
} gem_rpc_words16_t;

typedef struct gem_rpc_words8 {
    WORD values[8];
} gem_rpc_words8_t;

typedef struct gem_rpc_opnvwk_req {
    WORD work_in[11];
} gem_rpc_opnvwk_req_t;

typedef struct gem_rpc_opnvwk_rsp {
    WORD handle;
    WORD work_out[57];
} gem_rpc_opnvwk_rsp_t;

typedef struct gem_rpc_handle_req {
    WORD handle;
} gem_rpc_handle_req_t;

typedef struct gem_rpc_color_req {
    WORD handle;
    WORD color;
} gem_rpc_color_req_t;

typedef struct gem_rpc_clip_req {
    WORD handle;
    WORD enabled;
    WORD xy[4];
} gem_rpc_clip_req_t;

typedef struct gem_rpc_pline_req {
    WORD handle;
    WORD count;
    WORD pxy[256];
} gem_rpc_pline_req_t;

typedef struct gem_rpc_rect_req {
    WORD handle;
    WORD xy[4];
} gem_rpc_rect_req_t;

typedef struct gem_rpc_gtext_req {
    WORD handle;
    WORD x;
    WORD y;
    char text[GEM_RPC_TEXT_MAX];
} gem_rpc_gtext_req_t;

typedef struct gem_rpc_wind_create_req {
    UWORD kind;
    WORD x;
    WORD y;
    WORD w;
    WORD h;
} gem_rpc_wind_create_req_t;

typedef struct gem_rpc_wind_open_req {
    WORD handle;
    WORD x;
    WORD y;
    WORD w;
    WORD h;
} gem_rpc_wind_open_req_t;

typedef struct gem_rpc_wind_get_req {
    WORD handle;
    WORD field;
} gem_rpc_wind_get_req_t;

typedef struct gem_rpc_wind_get_rsp {
    WORD w1;
    WORD w2;
    WORD w3;
    WORD w4;
} gem_rpc_wind_get_rsp_t;

typedef struct gem_rpc_wind_set_req {
    WORD handle;
    WORD field;
    WORD w1;
    WORD w2;
    WORD w3;
    WORD w4;
} gem_rpc_wind_set_req_t;

typedef struct gem_rpc_wind_set_str_req {
    WORD handle;
    WORD field;
    char text[GEM_RPC_TEXT_MAX];
} gem_rpc_wind_set_str_req_t;

typedef struct gem_rpc_wind_find_req {
    WORD x;
    WORD y;
} gem_rpc_wind_find_req_t;

typedef struct gem_rpc_wind_update_req {
    WORD flag;
} gem_rpc_wind_update_req_t;

typedef struct gem_rpc_wind_calc_req {
    WORD type;
    UWORD kind;
    WORD inx;
    WORD iny;
    WORD inw;
    WORD inh;
} gem_rpc_wind_calc_req_t;

typedef struct gem_rpc_wind_calc_rsp {
    WORD outx;
    WORD outy;
    WORD outw;
    WORD outh;
} gem_rpc_wind_calc_rsp_t;

int gem_rpc_call(gem_rpc_opcode_t opcode,
                 const void *request,
                 uint32_t request_size,
                 int32_t *status,
                 void *response,
                 uint32_t response_size);

#endif
