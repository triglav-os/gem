/*
 * Implements the first libgem client wrappers. The current slice
 * focuses on workstation, window, and immediate-mode drawing calls that
 * can be marshalled safely without object-tree pointer graphs.
 *
 * MIT License (see: LICENSE)
 * Copyright (C) 2026 tomaz stih
 */

#include "_gem.h"

#include "platform/os.h"

#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

WORD contrl[12] __attribute__((weak));
WORD intin[128] __attribute__((weak));
WORD ptsin[256] __attribute__((weak));
WORD intout[128] __attribute__((weak));
WORD ptsout[256] __attribute__((weak));
WORD global[15];

static int g_gem_socket = -1;

static void gem_rpc_disconnect(void)
{
    if (g_gem_socket >= 0) {
        (void) close(g_gem_socket);
        g_gem_socket = -1;
    }
}

static int gem_rpc_send_all(int fd, const void *buf, size_t size)
{
    const uint8_t *cursor = (const uint8_t *) buf;

    while (size > 0u) {
        ssize_t rc = send(fd, cursor, size, 0);

        if (rc < 0) {
            if (errno == EINTR) {
                continue;
            }
            gem_rpc_disconnect();
            return 0;
        }
        cursor += (size_t) rc;
        size -= (size_t) rc;
    }
    return 1;
}

static int gem_rpc_recv_all(int fd, void *buf, size_t size)
{
    uint8_t *cursor = (uint8_t *) buf;

    while (size > 0u) {
        ssize_t rc = recv(fd, cursor, size, 0);

        if (rc <= 0) {
            if (rc < 0 && errno == EINTR) {
                continue;
            }
            gem_rpc_disconnect();
            return 0;
        }
        cursor += (size_t) rc;
        size -= (size_t) rc;
    }
    return 1;
}

static int gem_rpc_connect(void)
{
    struct sockaddr_un addr;
    int fd;

    if (g_gem_socket >= 0) {
        return 1;
    }

    fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0) {
        return 0;
    }

    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, GEMD_SOCKET_PATH, sizeof(addr.sun_path) - 1u);
    if (connect(fd, (const struct sockaddr *) &addr, sizeof(addr)) != 0) {
        (void) close(fd);
        return 0;
    }

    g_gem_socket = fd;
    return 1;
}

int gem_rpc_call(gem_rpc_opcode_t opcode,
                 const void *request,
                 uint32_t request_size,
                 int32_t *status,
                 void *response,
                 uint32_t response_size)
{
    gem_rpc_header_t header;
    gem_rpc_reply_t reply;

    if (!gem_rpc_connect()) {
        return 0;
    }

    header.magic = GEM_RPC_MAGIC;
    header.version = GEM_RPC_VERSION;
    header.opcode = (uint16_t) opcode;
    header.size = request_size;

    if (!gem_rpc_send_all(g_gem_socket, &header, sizeof(header))) {
        return 0;
    }
    if (request_size > 0u && request != NULL) {
        if (!gem_rpc_send_all(g_gem_socket, request, request_size)) {
            return 0;
        }
    }
    if (!gem_rpc_recv_all(g_gem_socket, &reply, sizeof(reply))) {
        return 0;
    }
    if (reply.magic != GEM_RPC_MAGIC) {
        return 0;
    }
    if (status != NULL) {
        *status = reply.status;
    }
    if (reply.size > 0u) {
        if (response == NULL || response_size < reply.size) {
            return 0;
        }
        if (!gem_rpc_recv_all(g_gem_socket, response, reply.size)) {
            return 0;
        }
    }
    return 1;
}

WORD appl_init(void)
{
    int32_t status = 0;

    if (!gem_rpc_call(GEM_RPC_APPL_INIT, NULL, 0u, &status, NULL, 0u)) {
        return 0;
    }
    global[2] = (WORD) status;
    return (WORD) status;
}

WORD appl_exit(void)
{
    int32_t status = 0;

    if (!gem_rpc_call(GEM_RPC_APPL_EXIT, NULL, 0u, &status, NULL, 0u)) {
        return 0;
    }
    gem_rpc_disconnect();
    global[2] = 0;
    return (WORD) status;
}

WORD evnt_mesag(WORD msg[8])
{
    int32_t status = 0;
    gem_rpc_words8_t rsp;

    memset(&rsp, 0, sizeof(rsp));
    FOREVER {
        if (!gem_rpc_call(GEM_RPC_EVNT_MESAG, NULL, 0u, &status, &rsp,
                sizeof(rsp))) {
            return 0;
        }
        if (status != 0) {
            if (msg != NULL) {
                memcpy(msg, rsp.values, sizeof(rsp.values));
            }
            return 1;
        }
        gem_os_sleep_ms(1u);
    }
}

WORD evnt_multi(UWORD flags,
                UWORD bclk,
                UWORD bmsk,
                UWORD bst,
                UWORD m1flags,
                WORD m1x,
                WORD m1y,
                WORD m1w,
                WORD m1h,
                UWORD m2flags,
                WORD m2x,
                WORD m2y,
                WORD m2w,
                WORD m2h,
                WORD mepbuff[8],
                UWORD tlc,
                UWORD thc,
                WORD *pmx,
                WORD *pmy,
                WORD *pmb,
                WORD *pks,
                WORD *pkr,
                WORD *pbr)
{
    enum { GEM_EVENT_RPC_SLICE_MS = 2 };
    int32_t status = 0;
    gem_rpc_evnt_multi_req_t req;
    gem_rpc_evnt_multi_rsp_t rsp;
    uint32_t requested_timeout = (uint32_t) tlc |
        ((uint32_t) thc << 16);
    uint32_t timer_start = gem_os_ticks_ms();

    memset(&req, 0, sizeof(req));
    memset(&rsp, 0, sizeof(rsp));
    req.flags = flags;
    req.bclk = bclk;
    req.bmsk = bmsk;
    req.bst = bst;
    req.m1flags = m1flags;
    req.m1x = m1x;
    req.m1y = m1y;
    req.m1w = m1w;
    req.m1h = m1h;
    req.m2flags = m2flags;
    req.m2x = m2x;
    req.m2y = m2y;
    req.m2w = m2w;
    req.m2h = m2h;
    for (;;) {
        if ((flags & MU_TIMER) != 0u) {
            uint32_t elapsed = gem_os_ticks_ms() - timer_start;
            uint32_t remaining = (elapsed < requested_timeout) ?
                requested_timeout - elapsed : 0u;
            uint32_t slice = (remaining < GEM_EVENT_RPC_SLICE_MS) ?
                remaining : GEM_EVENT_RPC_SLICE_MS;

            req.tlc = (UWORD) slice;
            req.thc = 0;
        } else {
            req.tlc = tlc;
            req.thc = thc;
        }

        memset(&rsp, 0, sizeof(rsp));
        if (!gem_rpc_call(GEM_RPC_EVNT_MULTI, &req, sizeof(req), &status,
                &rsp, sizeof(rsp))) {
            return 0;
        }
        if ((flags & MU_TIMER) == 0u || rsp.event != MU_TIMER ||
            (uint32_t) (gem_os_ticks_ms() - timer_start) >=
                requested_timeout) {
            break;
        }
    }

    if (mepbuff != NULL) {
        memcpy(mepbuff, rsp.msg, sizeof(rsp.msg));
    }
    if (pmx != NULL) {
        *pmx = rsp.mx;
    }
    if (pmy != NULL) {
        *pmy = rsp.my;
    }
    if (pmb != NULL) {
        *pmb = rsp.mb;
    }
    if (pks != NULL) {
        *pks = rsp.ks;
    }
    if (pkr != NULL) {
        *pkr = rsp.kr;
    }
    if (pbr != NULL) {
        *pbr = rsp.br;
    }
    return rsp.event;
}

WORD graf_handle(WORD *charw, WORD *charh, WORD *boxw, WORD *boxh)
{
    int32_t status = 0;
    gem_rpc_words16_t rsp;

    memset(&rsp, 0, sizeof(rsp));
    if (!gem_rpc_call(GEM_RPC_GRAF_HANDLE, NULL, 0u, &status, &rsp,
            sizeof(rsp))) {
        return 0;
    }
    if (charw != NULL) {
        *charw = rsp.values[0];
    }
    if (charh != NULL) {
        *charh = rsp.values[1];
    }
    if (boxw != NULL) {
        *boxw = rsp.values[2];
    }
    if (boxh != NULL) {
        *boxh = rsp.values[3];
    }
    return (WORD) status;
}

WORD graf_mouse(WORD mode, void *form)
{
    int32_t status = 0;
    gem_rpc_graf_mouse_req_t req;

    (void) form;
    req.mode = mode;
    if (!gem_rpc_call(GEM_RPC_GRAF_MOUSE, &req, sizeof(req), &status,
            NULL, 0u)) {
        return 0;
    }
    return (WORD) status;
}

WORD form_alert(WORD defbut, char *astring)
{
    int32_t status = 0;
    gem_rpc_form_alert_req_t req;

    memset(&req, 0, sizeof(req));
    req.defbut = defbut;
    if (astring != NULL) {
        strncpy(req.text, astring, sizeof(req.text) - 1u);
    }
    if (!gem_rpc_call(GEM_RPC_FORM_ALERT, &req, sizeof(req), &status,
            NULL, 0u)) {
        return 0;
    }
    return (WORD) status;
}

VOID v_opnvwk(WORD work_in[11], VDI_HANDLE *handle, WORD work_out[57])
{
    int32_t status = 0;
    gem_rpc_opnvwk_req_t req;
    gem_rpc_opnvwk_rsp_t rsp;

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
    (void) gem_rpc_call(GEM_RPC_V_CLSVWK, &req, sizeof(req), &status, NULL, 0u);
}

VOID v_clrwk(VDI_HANDLE handle)
{
    int32_t status = 0;
    gem_rpc_handle_req_t req;

    req.handle = handle;
    (void) gem_rpc_call(GEM_RPC_V_CLRWK, &req, sizeof(req), &status, NULL, 0u);
}

WORD v_updwk(WORD handle)
{
    int32_t status = 0;
    gem_rpc_handle_req_t req;

    req.handle = handle;
    if (!gem_rpc_call(GEM_RPC_V_UPDWK, &req, sizeof(req), &status, NULL, 0u)) {
        return 0;
    }
    return (WORD) status;
}

VOID vsf_color(WORD handle, WORD color)
{
    int32_t status = 0;
    gem_rpc_color_req_t req;

    req.handle = handle;
    req.color = color;
    (void) gem_rpc_call(GEM_RPC_VSF_COLOR, &req, sizeof(req), &status,
        NULL, 0u);
}

WORD vsl_type(WORD handle, WORD style)
{
    int32_t status = 0;
    gem_rpc_handle_word_req_t req;

    req.handle = handle;
    req.value = style;
    if (!gem_rpc_call(GEM_RPC_VSL_TYPE, &req, sizeof(req), &status,
            NULL, 0u)) {
        return 0;
    }
    return (WORD) status;
}

WORD vsl_width(WORD handle, WORD width)
{
    int32_t status = 0;
    gem_rpc_handle_word_req_t req;

    req.handle = handle;
    req.value = width;
    if (!gem_rpc_call(GEM_RPC_VSL_WIDTH, &req, sizeof(req), &status,
            NULL, 0u)) {
        return 0;
    }
    return (WORD) status;
}

VOID vsl_color(WORD handle, WORD color)
{
    int32_t status = 0;
    gem_rpc_color_req_t req;

    req.handle = handle;
    req.color = color;
    (void) gem_rpc_call(GEM_RPC_VSL_COLOR, &req, sizeof(req), &status,
        NULL, 0u);
}

WORD vsf_interior(WORD handle, WORD style)
{
    int32_t status = 0;
    gem_rpc_handle_word_req_t req;

    req.handle = handle;
    req.value = style;
    if (!gem_rpc_call(GEM_RPC_VSF_INTERIOR, &req, sizeof(req), &status,
            NULL, 0u)) {
        return 0;
    }
    return (WORD) status;
}

WORD vsf_style(WORD handle, WORD style)
{
    int32_t status = 0;
    gem_rpc_handle_word_req_t req;

    req.handle = handle;
    req.value = style;
    if (!gem_rpc_call(GEM_RPC_VSF_STYLE, &req, sizeof(req), &status,
            NULL, 0u)) {
        return 0;
    }
    return (WORD) status;
}

WORD vsf_perimeter(WORD handle, WORD per_vis)
{
    int32_t status = 0;
    gem_rpc_handle_word_req_t req;

    req.handle = handle;
    req.value = per_vis;
    if (!gem_rpc_call(GEM_RPC_VSF_PERIMETER, &req, sizeof(req), &status,
            NULL, 0u)) {
        return 0;
    }
    return (WORD) status;
}

WORD vswr_mode(WORD handle, WORD mode)
{
    int32_t status = 0;
    gem_rpc_handle_word_req_t req;

    req.handle = handle;
    req.value = mode;
    if (!gem_rpc_call(GEM_RPC_VSWR_MODE, &req, sizeof(req), &status,
            NULL, 0u)) {
        return 0;
    }
    return (WORD) status;
}

WORD vst_font(WORD handle, WORD font)
{
    int32_t status = 0;
    gem_rpc_handle_word_req_t req;

    req.handle = handle;
    req.value = font;
    if (!gem_rpc_call(GEM_RPC_VST_FONT, &req, sizeof(req), &status,
            NULL, 0u)) {
        return 0;
    }
    return (WORD) status;
}

VOID vst_color(WORD handle, WORD color)
{
    int32_t status = 0;
    gem_rpc_color_req_t req;

    req.handle = handle;
    req.color = color;
    (void) gem_rpc_call(GEM_RPC_VST_COLOR, &req, sizeof(req), &status,
        NULL, 0u);
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
    (void) gem_rpc_call(GEM_RPC_VS_CLIP, &req, sizeof(req), &status, NULL, 0u);
}

VOID v_pline(VDI_HANDLE handle, WORD count, CONST WORD *pxy)
{
    int32_t status = 0;
    gem_rpc_pline_req_t req;
    size_t values;

    memset(&req, 0, sizeof(req));
    req.handle = handle;
    req.count = count;
    values = (count > 0) ? (size_t) count * 2u : 0u;
    if (values > sizeof(req.pxy) / sizeof(req.pxy[0])) {
        values = sizeof(req.pxy) / sizeof(req.pxy[0]);
        req.count = (WORD) (values / 2u);
    }
    if (pxy != NULL && values > 0u) {
        memcpy(req.pxy, pxy, values * sizeof(req.pxy[0]));
    }
    (void) gem_rpc_call(GEM_RPC_V_PLINE, &req, sizeof(req), &status, NULL, 0u);
}

VOID v_fillarea(WORD handle, WORD count, WORD xy[])
{
    int32_t status = 0;
    gem_rpc_pline_req_t req;
    size_t values;

    memset(&req, 0, sizeof(req));
    req.handle = handle;
    req.count = count;
    values = (count > 0) ? (size_t) count * 2u : 0u;
    if (values > sizeof(req.pxy) / sizeof(req.pxy[0])) {
        values = sizeof(req.pxy) / sizeof(req.pxy[0]);
        req.count = (WORD) (values / 2u);
    }
    if (xy != NULL && values > 0u) {
        memcpy(req.pxy, xy, values * sizeof(req.pxy[0]));
    }
    (void) gem_rpc_call(GEM_RPC_V_FILLAREA, &req, sizeof(req), &status,
        NULL, 0u);
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
    (void) gem_rpc_call(GEM_RPC_V_BAR, &req, sizeof(req), &status, NULL, 0u);
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
    (void) gem_rpc_call(GEM_RPC_VR_RECFL, &req, sizeof(req), &status, NULL, 0u);
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
        strncpy(req.text, (const char *) text, sizeof(req.text) - 1u);
    }
    (void) gem_rpc_call(GEM_RPC_V_GTEXT, &req, sizeof(req), &status, NULL, 0u);
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
    return (WORD) status;
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

WORD wind_create(UWORD kind, WORD x, WORD y, WORD w, WORD h)
{
    int32_t status = 0;
    gem_rpc_wind_create_req_t req;

    req.kind = kind;
    req.x = x;
    req.y = y;
    req.w = w;
    req.h = h;
    if (!gem_rpc_call(GEM_RPC_WIND_CREATE, &req, sizeof(req), &status,
            NULL, 0u)) {
        return 0;
    }
    return (WORD) status;
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
    if (!gem_rpc_call(GEM_RPC_WIND_OPEN, &req, sizeof(req), &status,
            NULL, 0u)) {
        return 0;
    }
    return (WORD) status;
}

WORD wind_close(WORD handle)
{
    int32_t status = 0;
    gem_rpc_handle_req_t req;

    req.handle = handle;
    if (!gem_rpc_call(GEM_RPC_WIND_CLOSE, &req, sizeof(req), &status,
            NULL, 0u)) {
        return 0;
    }
    return (WORD) status;
}

WORD wind_delete(WORD handle)
{
    int32_t status = 0;
    gem_rpc_handle_req_t req;

    req.handle = handle;
    if (!gem_rpc_call(GEM_RPC_WIND_DELETE, &req, sizeof(req), &status,
            NULL, 0u)) {
        return 0;
    }
    return (WORD) status;
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
    return (WORD) status;
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
    if (!gem_rpc_call(GEM_RPC_WIND_SET, &req, sizeof(req), &status,
            NULL, 0u)) {
        return 0;
    }
    return (WORD) status;
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
    if (!gem_rpc_call(GEM_RPC_WIND_SET_STR, &req, sizeof(req), &status,
            NULL, 0u)) {
        return 0;
    }
    return (WORD) status;
}

WORD wind_find(WORD x, WORD y)
{
    int32_t status = 0;
    gem_rpc_wind_find_req_t req;

    req.x = x;
    req.y = y;
    if (!gem_rpc_call(GEM_RPC_WIND_FIND, &req, sizeof(req), &status,
            NULL, 0u)) {
        return 0;
    }
    return (WORD) status;
}

WORD wind_update(WORD flag)
{
    int32_t status = 0;
    gem_rpc_wind_update_req_t req;

    req.flag = flag;
    if (!gem_rpc_call(GEM_RPC_WIND_UPDATE, &req, sizeof(req), &status,
            NULL, 0u)) {
        return 0;
    }
    return (WORD) status;
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
    return (WORD) status;
}

static void gem_menu_tree_extent_visit(const OBJECT *tree, WORD object,
    uint8_t visited[GEM_RPC_MENU_MAX_OBJECTS], WORD *extent)
{
    if (tree == NULL || visited == NULL || extent == NULL ||
        object < ROOT || object >= (WORD) GEM_RPC_MENU_MAX_OBJECTS) {
        return;
    }
    if (visited[object] != 0u) {
        return;
    }

    visited[object] = 1u;
    if (object > *extent) {
        *extent = object;
    }

    gem_menu_tree_extent_visit(tree, tree[object].ob_head, visited, extent);
    gem_menu_tree_extent_visit(tree, tree[object].ob_tail, visited, extent);
    gem_menu_tree_extent_visit(tree, tree[object].ob_next, visited, extent);
}

/*
 * Menu trees are small graphs, not flat arrays with a reliable global
 * terminator. Some trees place LASTOB on several sibling chains, while
 * others are short static arrays that would be overrun by a blind scan.
 * Walk the references reachable from ROOT and use the highest visited
 * slot as the serialization extent.
 */
static WORD gem_menu_tree_extent(const OBJECT *tree)
{
    uint8_t visited[GEM_RPC_MENU_MAX_OBJECTS] = {0};
    WORD extent = ROOT;

    gem_menu_tree_extent_visit(tree, ROOT, visited, &extent);
    return extent;
}

WORD menu_bar(OBJECT *tree, WORD show)
{
    int32_t status = 0;
    gem_rpc_menu_bar_req_t req;
    WORD extent;
    WORD i;

    if (tree == NULL) {
        return 0;
    }

    memset(&req, 0, sizeof(req));
    req.show = show;
    extent = gem_menu_tree_extent(tree);
    req.object_count = (WORD) (extent + 1);
    if (req.object_count > (WORD) GEM_RPC_MENU_MAX_OBJECTS) {
        req.object_count = (WORD) GEM_RPC_MENU_MAX_OBJECTS;
    }
    memcpy(req.objects, tree,
        (size_t) req.object_count * sizeof(OBJECT));

    for (i = 0; i < req.object_count &&
            req.string_count < (WORD) GEM_RPC_MENU_MAX_STRINGS; ++i) {
        const char *text;

        if (tree[i].ob_type != G_TITLE && tree[i].ob_type != G_STRING) {
            continue;
        }
        if ((tree[i].ob_flags & INDIRECT) != 0 || tree[i].ob_spec == 0) {
            continue;
        }
        text = (const char *) (intptr_t) tree[i].ob_spec;
        req.strings[req.string_count].object = i;
        strncpy(req.strings[req.string_count].text, text,
            sizeof(req.strings[req.string_count].text) - 1u);
        ++req.string_count;
    }

    if (!gem_rpc_call(GEM_RPC_MENU_BAR, &req, sizeof(req), &status,
            NULL, 0u)) {
        return 0;
    }
    return (WORD) status;
}

WORD menu_tnormal(OBJECT *tree, WORD title, WORD normal)
{
    int32_t status = 0;
    gem_rpc_menu_tnormal_req_t req;

    (void) tree;
    req.title = title;
    req.normal = normal;
    if (!gem_rpc_call(GEM_RPC_MENU_TNORMAL, &req, sizeof(req), &status,
            NULL, 0u)) {
        return 0;
    }
    return (WORD) status;
}

WORD menu_click(WORD click, WORD setit)
{
    int32_t status = 0;
    gem_rpc_menu_click_req_t req;

    req.click = click;
    req.setit = setit;
    if (!gem_rpc_call(GEM_RPC_MENU_CLICK, &req, sizeof(req), &status,
            NULL, 0u)) {
        return 0;
    }
    return (WORD) status;
}
