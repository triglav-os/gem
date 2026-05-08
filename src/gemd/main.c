/*
 * Runs the first hosted GEM display server. The server owns the shared
 * AES/VDI state, accepts libgem RPC connections over a Unix-domain
 * socket, and executes requests serially against one rasta-backed
 * screen so multiple client processes can share the same desktop area.
 *
 * MIT License (see: LICENSE)
 * Copyright (C) 2026 tomaz stih
 */

#include "../aes/_aes.h"
#include "../gem/_gem.h"

#include "platform/hid.h"

#include <errno.h>
#include <poll.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

enum {
    GEMD_MAX_SESSIONS = 16
};

typedef struct gemd_session {
    int fd;
    WORD app_id;
    WORD vdi_open;
} gemd_session_t;

static int g_listen_fd = -1;
static gemd_session_t g_sessions[GEMD_MAX_SESSIONS];
static WORD g_server_vdi_handle;
static WORD g_server_vdi_refs;
static WORD g_server_work_out[57];

static void gemd_pump_hid(void)
{
    gem_hid_event_t evt;

    if (_aes.update_depth > 0) {
        return;
    }

    while (gem_hid_poll(&evt)) {
        _aes_dispatch_hid_event(&evt);
    }
}

static int gemd_send_all(int fd, const void *buf, size_t size)
{
    const uint8_t *cursor = (const uint8_t *) buf;

    while (size > 0u) {
        ssize_t rc = send(fd, cursor, size, 0);

        if (rc < 0) {
            if (errno == EINTR) {
                continue;
            }
            return 0;
        }
        cursor += (size_t) rc;
        size -= (size_t) rc;
    }
    return 1;
}

static int gemd_recv_all(int fd, void *buf, size_t size)
{
    uint8_t *cursor = (uint8_t *) buf;

    while (size > 0u) {
        ssize_t rc = recv(fd, cursor, size, 0);

        if (rc <= 0) {
            if (rc < 0 && errno == EINTR) {
                continue;
            }
            return 0;
        }
        cursor += (size_t) rc;
        size -= (size_t) rc;
    }
    return 1;
}

static int gemd_send_reply(int fd,
                           int32_t status,
                           const void *payload,
                           uint32_t payload_size)
{
    gem_rpc_reply_t reply;

    reply.magic = GEM_RPC_MAGIC;
    reply.status = status;
    reply.size = payload_size;
    if (!gemd_send_all(fd, &reply, sizeof(reply))) {
        return 0;
    }
    if (payload_size > 0u && payload != NULL) {
        if (!gemd_send_all(fd, payload, payload_size)) {
            return 0;
        }
    }
    return 1;
}

static void gemd_set_current_app(const gemd_session_t *session)
{
    if (session == NULL) {
        _aes.current_app_id = 0;
        global[2] = 0;
        return;
    }
    _aes.current_app_id = session->app_id;
    global[2] = session->app_id;
}

static int gemd_open_server_vdi(WORD work_out[57])
{
    WORD work_in[11];

    if (g_server_vdi_handle != 0) {
        if (work_out != NULL) {
            memcpy(work_out, g_server_work_out, sizeof(g_server_work_out));
        }
        return 1;
    }

    if (_aes.vdi_ready != 0 && _aes.vdi_handle != 0) {
        g_server_vdi_handle = _aes.vdi_handle;
        memcpy(g_server_work_out, _aes.work_out, sizeof(g_server_work_out));
        if (work_out != NULL) {
            memcpy(work_out, g_server_work_out, sizeof(g_server_work_out));
        }
        return 1;
    }

    memset(work_in, 0, sizeof(work_in));
    if (work_out == NULL) {
        v_opnvwk(work_in, &g_server_vdi_handle, g_server_work_out);
    } else {
        v_opnvwk(work_in, &g_server_vdi_handle, g_server_work_out);
        memcpy(work_out, g_server_work_out, sizeof(g_server_work_out));
    }
    return g_server_vdi_handle != 0;
}

static void gemd_shutdown_server_vdi(void)
{
    if (g_server_vdi_handle == 0) {
        return;
    }

    v_clsvwk(g_server_vdi_handle);
    g_server_vdi_handle = 0;
    g_server_vdi_refs = 0;
    memset(g_server_work_out, 0, sizeof(g_server_work_out));
}

static void gemd_cleanup_app(WORD app_id)
{
    size_t i;

    if (app_id == 0) {
        return;
    }

    _aes.current_app_id = app_id;
    global[2] = app_id;

    for (i = 0; i < AES_MAX_WINDOWS; ++i) {
        if (_aes.windows[i].used != 0 && _aes.windows[i].owner == app_id) {
            WORD handle = _aes.windows[i].handle;

            (void) wind_close(handle);
            (void) wind_delete(handle);
        }
    }

    for (i = 0; i < AES_MAX_MESSAGES; ++i) {
        if (_aes.messages[i].used != 0 &&
            (_aes.messages[i].dest == app_id ||
                _aes.messages[i].data[1] == app_id)) {
            memset(&_aes.messages[i], 0, sizeof(_aes.messages[i]));
        }
    }

    if (_aes.current_app_id == app_id) {
        _aes.current_app_id = 0;
        global[2] = 0;
    }

    for (i = 0; i < AES_MAX_APPS; ++i) {
        if (_aes.apps[i].used != 0 && _aes.apps[i].id == app_id) {
            memset(&_aes.apps[i], 0, sizeof(_aes.apps[i]));
            break;
        }
    }
}

static void gemd_close_session(gemd_session_t *session)
{
    if (session == NULL || session->fd < 0) {
        return;
    }

    if (session->app_id != 0) {
        gemd_cleanup_app(session->app_id);
    }
    if (session->vdi_open != 0) {
        session->vdi_open = 0;
        if (g_server_vdi_refs > 0) {
            --g_server_vdi_refs;
        }
    }

    (void) close(session->fd);
    session->fd = -1;
    session->app_id = 0;
    session->vdi_open = 0;
}

static gemd_session_t *gemd_alloc_session(void)
{
    size_t i;

    for (i = 0; i < GEMD_MAX_SESSIONS; ++i) {
        if (g_sessions[i].fd < 0) {
            return &g_sessions[i];
        }
    }
    return NULL;
}

static int gemd_init_listener(void)
{
    struct sockaddr_un addr;

    g_listen_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (g_listen_fd < 0) {
        perror("gemd: socket");
        return 0;
    }

    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, GEMD_SOCKET_PATH, sizeof(addr.sun_path) - 1u);
    (void) unlink(GEMD_SOCKET_PATH);

    if (bind(g_listen_fd, (const struct sockaddr *) &addr, sizeof(addr)) != 0) {
        perror("gemd: bind");
        (void) close(g_listen_fd);
        g_listen_fd = -1;
        return 0;
    }
    if (listen(g_listen_fd, GEMD_MAX_SESSIONS) != 0) {
        perror("gemd: listen");
        (void) close(g_listen_fd);
        g_listen_fd = -1;
        (void) unlink(GEMD_SOCKET_PATH);
        return 0;
    }
    return 1;
}

static void gemd_shutdown(void)
{
    size_t i;

    for (i = 0; i < GEMD_MAX_SESSIONS; ++i) {
        gemd_close_session(&g_sessions[i]);
    }
    gemd_shutdown_server_vdi();
    if (g_listen_fd >= 0) {
        (void) close(g_listen_fd);
        g_listen_fd = -1;
    }
    (void) unlink(GEMD_SOCKET_PATH);
}

static int32_t gemd_dispatch(gemd_session_t *session,
                             const gem_rpc_header_t *header,
                             const uint8_t *payload,
                             uint8_t *response,
                             uint32_t *response_size)
{
    int32_t status = 0;

    *response_size = 0u;
    gemd_set_current_app(session);

    switch ((gem_rpc_opcode_t) header->opcode) {
    case GEM_RPC_APPL_INIT:
        status = appl_init();
        session->app_id = (WORD) status;
        break;

    case GEM_RPC_APPL_EXIT:
        if (session->app_id == 0) {
            status = 0;
            break;
        }
        gemd_cleanup_app(session->app_id);
        session->app_id = 0;
        status = 1;
        break;

    case GEM_RPC_EVNT_MESAG:
        {
            gem_rpc_words8_t *rsp = (gem_rpc_words8_t *) response;

            memset(rsp, 0, sizeof(*rsp));
            if (_aes_dequeue_message(rsp->values) != 0) {
                status = 1;
                *response_size = (uint32_t) sizeof(*rsp);
            } else {
                status = 0;
            }
        }
        break;

    case GEM_RPC_GRAF_HANDLE:
        {
            gem_rpc_words16_t *rsp = (gem_rpc_words16_t *) response;

            memset(rsp, 0, sizeof(*rsp));
            status = graf_handle(&rsp->values[0], &rsp->values[1],
                &rsp->values[2], &rsp->values[3]);
            _aes_trace("gemd graf_handle status=%d aes_vdi=%d ready=%d",
                (int) status, _aes.vdi_handle, _aes.vdi_ready);
            if (g_server_vdi_handle == 0 &&
                _aes.vdi_ready != 0 && _aes.vdi_handle != 0) {
                g_server_vdi_handle = _aes.vdi_handle;
                memcpy(g_server_work_out, _aes.work_out,
                    sizeof(g_server_work_out));
            }
            *response_size = (uint32_t) sizeof(*rsp);
        }
        break;

    case GEM_RPC_V_OPNVWK:
        {
            gem_rpc_opnvwk_rsp_t *rsp = (gem_rpc_opnvwk_rsp_t *) response;

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
            *response_size = (uint32_t) sizeof(*rsp);
        }
        break;

    case GEM_RPC_V_CLSVWK:
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

    case GEM_RPC_VSF_COLOR:
        {
            const gem_rpc_color_req_t *req =
                (const gem_rpc_color_req_t *) payload;

            vsf_color(g_server_vdi_handle, req->color);
            _aes_trace("gemd vsf_color handle=%d color=%d",
                g_server_vdi_handle, req->color);
            status = 1;
        }
        break;

    case GEM_RPC_VST_COLOR:
        {
            const gem_rpc_color_req_t *req =
                (const gem_rpc_color_req_t *) payload;

            vst_color(g_server_vdi_handle, req->color);
            _aes_trace("gemd vst_color handle=%d color=%d",
                g_server_vdi_handle, req->color);
            status = 1;
        }
        break;

    case GEM_RPC_VS_CLIP:
        {
            const gem_rpc_clip_req_t *req =
                (const gem_rpc_clip_req_t *) payload;

            vs_clip(g_server_vdi_handle, req->enabled, (WORD *) req->xy);
            _aes_trace("gemd vs_clip handle=%d enabled=%d rect=%d,%d-%d,%d",
                g_server_vdi_handle, req->enabled, req->xy[0], req->xy[1],
                req->xy[2], req->xy[3]);
            status = 1;
        }
        break;

    case GEM_RPC_V_PLINE:
        {
            const gem_rpc_pline_req_t *req =
                (const gem_rpc_pline_req_t *) payload;

            v_pline(g_server_vdi_handle, req->count, req->pxy);
            status = 1;
        }
        break;

    case GEM_RPC_V_BAR:
        {
            const gem_rpc_rect_req_t *req =
                (const gem_rpc_rect_req_t *) payload;

            v_bar(g_server_vdi_handle, req->xy);
            status = 1;
        }
        break;

    case GEM_RPC_VR_RECFL:
        {
            const gem_rpc_rect_req_t *req =
                (const gem_rpc_rect_req_t *) payload;

            vr_recfl(g_server_vdi_handle, (WORD *) req->xy);
            _aes_trace("gemd vr_recfl handle=%d rect=%d,%d-%d,%d",
                g_server_vdi_handle, req->xy[0], req->xy[1], req->xy[2],
                req->xy[3]);
            status = 1;
        }
        break;

    case GEM_RPC_V_GTEXT:
        {
            const gem_rpc_gtext_req_t *req =
                (const gem_rpc_gtext_req_t *) payload;
            char text[GEM_RPC_TEXT_MAX];

            memcpy(text, req->text, sizeof(text));
            text[sizeof(text) - 1u] = '\0';
            v_gtext(g_server_vdi_handle, req->x, req->y, (const BYTE *) text);
            _aes_trace("gemd v_gtext handle=%d pos=%d,%d text=\"%s\"",
                g_server_vdi_handle, req->x, req->y, text);
            status = 1;
        }
        break;

    case GEM_RPC_WIND_CREATE:
        {
            const gem_rpc_wind_create_req_t *req =
                (const gem_rpc_wind_create_req_t *) payload;

            status = wind_create(req->kind, req->x, req->y, req->w, req->h);
        }
        break;

    case GEM_RPC_WIND_OPEN:
        {
            const gem_rpc_wind_open_req_t *req =
                (const gem_rpc_wind_open_req_t *) payload;

            status = wind_open(req->handle, req->x, req->y, req->w, req->h);
            _aes_trace("gemd wind_open handle=%d rect=%d,%d %dx%d status=%d "
                "aes_vdi=%d ready=%d", req->handle, req->x, req->y, req->w,
                req->h, (int) status, _aes.vdi_handle, _aes.vdi_ready);
        }
        break;

    case GEM_RPC_WIND_CLOSE:
        {
            const gem_rpc_handle_req_t *req =
                (const gem_rpc_handle_req_t *) payload;

            status = wind_close(req->handle);
        }
        break;

    case GEM_RPC_WIND_DELETE:
        {
            const gem_rpc_handle_req_t *req =
                (const gem_rpc_handle_req_t *) payload;

            status = wind_delete(req->handle);
        }
        break;

    case GEM_RPC_WIND_GET:
        {
            const gem_rpc_wind_get_req_t *req =
                (const gem_rpc_wind_get_req_t *) payload;
            gem_rpc_wind_get_rsp_t *rsp =
                (gem_rpc_wind_get_rsp_t *) response;

            memset(rsp, 0, sizeof(*rsp));
            status = wind_get(req->handle, req->field, &rsp->w1, &rsp->w2,
                &rsp->w3, &rsp->w4);
            *response_size = (uint32_t) sizeof(*rsp);
        }
        break;

    case GEM_RPC_WIND_SET:
        {
            const gem_rpc_wind_set_req_t *req =
                (const gem_rpc_wind_set_req_t *) payload;

            status = wind_set(req->handle, req->field, req->w1, req->w2,
                req->w3, req->w4);
        }
        break;

    case GEM_RPC_WIND_SET_STR:
        {
            const gem_rpc_wind_set_str_req_t *req =
                (const gem_rpc_wind_set_str_req_t *) payload;
            char text[GEM_RPC_TEXT_MAX];

            memcpy(text, req->text, sizeof(text));
            text[sizeof(text) - 1u] = '\0';
            status = wind_set_str(req->handle, req->field, text);
        }
        break;

    case GEM_RPC_WIND_FIND:
        {
            const gem_rpc_wind_find_req_t *req =
                (const gem_rpc_wind_find_req_t *) payload;

            status = wind_find(req->x, req->y);
        }
        break;

    case GEM_RPC_WIND_UPDATE:
        {
            const gem_rpc_wind_update_req_t *req =
                (const gem_rpc_wind_update_req_t *) payload;

            status = wind_update(req->flag);
            _aes_trace("gemd wind_update flag=%d status=%d", req->flag,
                (int) status);
        }
        break;

    case GEM_RPC_WIND_CALC:
        {
            const gem_rpc_wind_calc_req_t *req =
                (const gem_rpc_wind_calc_req_t *) payload;
            gem_rpc_wind_calc_rsp_t *rsp =
                (gem_rpc_wind_calc_rsp_t *) response;

            memset(rsp, 0, sizeof(*rsp));
            status = wind_calc(req->type, req->kind, req->inx, req->iny,
                req->inw, req->inh, &rsp->outx, &rsp->outy, &rsp->outw,
                &rsp->outh);
            *response_size = (uint32_t) sizeof(*rsp);
        }
        break;

    default:
        status = -1;
        break;
    }

    return status;
}

static int gemd_handle_request(gemd_session_t *session)
{
    gem_rpc_header_t header;
    uint8_t payload[GEM_RPC_PAYLOAD_MAX];
    uint8_t response[GEM_RPC_PAYLOAD_MAX];
    uint32_t response_size = 0u;
    int32_t status;

    if (!gemd_recv_all(session->fd, &header, sizeof(header))) {
        return 0;
    }
    if (header.magic != GEM_RPC_MAGIC || header.version != GEM_RPC_VERSION ||
        header.size > GEM_RPC_PAYLOAD_MAX) {
        return 0;
    }
    if (header.size > 0u) {
        if (!gemd_recv_all(session->fd, payload, header.size)) {
            return 0;
        }
    }

    status = gemd_dispatch(session, &header, payload, response,
        &response_size);
    return gemd_send_reply(session->fd, status, response, response_size);
}

int main(void)
{
    struct pollfd pollfds[GEMD_MAX_SESSIONS + 1];
    size_t i;

    for (i = 0; i < GEMD_MAX_SESSIONS; ++i) {
        g_sessions[i].fd = -1;
    }

    if (!gemd_init_listener()) {
        return 1;
    }

    printf("gemd listening on %s\n", GEMD_SOCKET_PATH);

    for (;;) {
        int rc;
        nfds_t nfds = 1;

        pollfds[0].fd = g_listen_fd;
        pollfds[0].events = POLLIN;
        pollfds[0].revents = 0;

        for (i = 0; i < GEMD_MAX_SESSIONS; ++i) {
            if (g_sessions[i].fd >= 0) {
                pollfds[nfds].fd = g_sessions[i].fd;
                pollfds[nfds].events = POLLIN;
                pollfds[nfds].revents = 0;
                ++nfds;
            }
        }

        rc = poll(pollfds, nfds, 20);
        gemd_pump_hid();
        if (rc < 0) {
            if (errno == EINTR) {
                continue;
            }
            perror("gemd: poll");
            break;
        }

        if ((pollfds[0].revents & POLLIN) != 0) {
            gemd_session_t *session = gemd_alloc_session();

            if (session != NULL) {
                int fd = accept(g_listen_fd, NULL, NULL);

                if (fd >= 0) {
                    session->fd = fd;
                    session->app_id = 0;
                    session->vdi_open = 0;
                }
            } else {
                int fd = accept(g_listen_fd, NULL, NULL);

                if (fd >= 0) {
                    (void) close(fd);
                }
            }
        }

        nfds = 1;
        for (i = 0; i < GEMD_MAX_SESSIONS; ++i) {
            if (g_sessions[i].fd < 0) {
                continue;
            }
            if ((pollfds[nfds].revents & (POLLERR | POLLHUP | POLLNVAL)) != 0 ||
                ((pollfds[nfds].revents & POLLIN) != 0 &&
                    !gemd_handle_request(&g_sessions[i]))) {
                gemd_close_session(&g_sessions[i]);
            }
            ++nfds;
        }

        gemd_pump_hid();
    }

    gemd_shutdown();
    return 0;
}
