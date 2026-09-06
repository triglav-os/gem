/*
 * Runs the first hosted GEM display server. The server owns the shared
 * AES/VDI state, accepts libgem RPC connections over a Unix-domain
 * socket, and executes requests serially against one rasta-backed
 * screen so multiple client processes can share the same desktop area.
 *
 * MIT License (see: LICENSE)
 * Copyright (C) 2026 tomaz stih
 */

#define _GNU_SOURCE
#include "../aes/aes_internal.h"
#include "../gem/gem_protocol.h"
#include "transport.h"
#include "drawing.h"
#include "gem/gemd.h"

#include "platform/hid.h"
#include "platform/os.h"

#include <errno.h>
#include <poll.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <unistd.h>

enum { GEMD_MAX_SESSIONS = 16 };

typedef struct gemd_session {
    int fd;
    uint64_t generation;
    WORD app_id;
    WORD vdi_open;
    WORD standalone;
    WORD dialog_active;
    GRECT dialog;
    uint64_t edit_identity;
    WORD edit_object, edit_index;
    gem_bitmap_store_t bitmaps;
    OBJECT *menu_objects;
    char *menu_strings_blob;
    WORD menu_count;
    gemd_io_t io;
    uint32_t accepted_at;
    uint32_t update_started;
    gemd_drawing_t drawing;
} gemd_session_t;

static void gemd_free_session_menu(gemd_session_t *session)
{
    free(session->menu_objects);
    session->menu_objects = NULL;
    free(session->menu_strings_blob);
    session->menu_strings_blob = NULL;
    session->menu_count = 0;
}

static int g_listen_fd = -1;
static uint64_t g_next_generation;
static gemd_session_t g_sessions[GEMD_MAX_SESSIONS];
static WORD g_server_vdi_handle;
static WORD g_server_vdi_refs;
static WORD g_server_work_out[57];
static struct stat g_socket_identity;
static int g_socket_bound;
static volatile sig_atomic_t g_stopping;
static gemd_session_t *g_modal_session;
static int gemd_service_modal(void);
static int gemd_handle_request(gemd_session_t *session);
static WORD gemd_user_callback(LONG parameter)
{
    gemd_session_t *session = g_modal_session;
    gemd_io_t *saved;
    PARMBLK parm = *(PARMBLK *)(intptr_t)parameter;
    LONG result = 0;
    int finished = 0;
    uint32_t started = gem_os_ticks_ms();
    if (!session)
        return 0;
    saved = malloc(sizeof(*saved));
    if (!saved)
        return 0;
    gemd_drawing_save(&session->drawing);
    *saved = session->io;
    memset(&session->io, 0, sizeof(session->io));
    parm.pb_tree = parm.pb_parm = 0;
    gemd_reply(&session->io, INT32_MIN, &parm, sizeof(parm));
    while (!g_stopping && gem_os_ticks_ms() - started < 2000u) {
        if (session->io.output_size) {
            if (gemd_send(session->fd, &session->io) < 0)
                break;
        } else if (finished)
            break;
        else {
            if (gemd_receive(session->fd, &session->io) < 0)
                break;
            if (session->io.ready) {
                if (session->io.header.opcode == GEM_RPC_CALLBACK_DONE &&
                    session->io.header.size == sizeof(result)) {
                    memcpy(&result, session->io.payload, sizeof(result));
                    gemd_reply(&session->io, 1, NULL, 0);
                    finished = 1;
                } else if (gemd_vdi_request(session->io.header.opcode)) {
                    (void)gemd_handle_request(session);
                } else
                    break;
            }
        }
        struct pollfd fd = {session->fd,
                            session->io.output_size ? POLLOUT : POLLIN, 0};
        (void)poll(&fd, 1, 1);
    }
    if (!finished)
        shutdown(session->fd, SHUT_RDWR);
    session->io = *saved;
    free(saved);
    return (WORD)result;
}

static void gemd_pump_hid(void)
{
    gem_hid_event_t evt;

    if (aes_state.update_depth > 0) {
        return;
    }

    for (unsigned i = 0; i < 128 && gem_hid_poll(&evt); ++i) {
        uint32_t started = gem_os_ticks_ms();
        aes_dispatch_hid_event(&evt);
        uint32_t elapsed = gem_os_ticks_ms() - started;

        /* Native menu/drag tracking waits for the user without servicing
         * RPC sockets. That server-imposed pause is not client inactivity.
         * Preserve each deadline's remaining time, including partial frames
         * and replies queued immediately before the menu opened. */
        if (elapsed != 0) {
            for (size_t j = 0; j < GEMD_MAX_SESSIONS; ++j) {
                gemd_session_t *session = &g_sessions[j];
                if (session->fd < 0)
                    continue;
                session->io.started += elapsed;
                session->accepted_at += elapsed;
                session->update_started += elapsed;
            }
        }
    }
}

static void gemd_set_current_app(const gemd_session_t *session)
{
    if (session == NULL) {
        aes_state.current_app_id = 0;
        global[2] = 0;
        return;
    }
    aes_state.current_app_id = session->app_id;
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

    if (aes_state.vdi_ready != 0 && aes_state.vdi_handle != 0) {
        g_server_vdi_handle = aes_state.vdi_handle;
        memcpy(g_server_work_out, aes_state.work_out,
               sizeof(g_server_work_out));
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

    aes_state.current_app_id = app_id;
    global[2] = app_id;
    /* Detach while the session's relocated tree is still alive. In
     * particular, the desktop owner has no fallback menu to switch to. */
    if (aes_state.menu_owner_app_id == app_id) {
        (void)menu_bar(aes_state.menu_tree, 0);
        aes_state.menu_owner_app_id = 0;
    }

    if ((aes_state.menu_owner_app_id == app_id ||
         aes_state.active_app_id == app_id) &&
        aes_state.desktop_owner_app_id != app_id) {
        aes_menu_switch_to_app(aes_state.desktop_owner_app_id);
    }
    if (aes_state.desktop_owner_app_id == app_id) {
        aes_state.desktop_owner_app_id = 0;
    }

    aes_state.current_app_id = app_id;
    global[2] = app_id;

    for (i = 0; i < AES_MAX_WINDOWS; ++i) {
        if (aes_state.windows[i].used != 0 &&
            aes_state.windows[i].owner == app_id) {
            WORD handle = aes_state.windows[i].handle;

            (void)wind_close(handle);
            (void)wind_delete(handle);
        }
    }

    for (i = 0; i < AES_MAX_MESSAGES; ++i) {
        if (aes_state.messages[i].used != 0 &&
            (aes_state.messages[i].dest == app_id ||
             aes_state.messages[i].data[1] == app_id)) {
            memset(&aes_state.messages[i], 0, sizeof(aes_state.messages[i]));
        }
    }

    /*
     * If this app disconnected between its own wind_update(BEG_UPDATE)
     * and END_UPDATE (crash, kill, or just a bug), aes_state.update_depth
     * would otherwise stay stuck above 0 forever -- it is a single
     * counter shared by every app, so gemd_pump_hid()'s "don't touch
     * input mid-update" guard would then silently stop processing
     * mouse/keyboard input for the *entire* desktop, for every app,
     * until gemd itself was restarted. Since gemd can legitimately
     * interleave different apps' BEG/END pairs, only unwind exactly
     * what this app itself still owed -- not the whole counter, which
     * may also carry another app's still-legitimate in-progress
     * update. current_app_id is already this app_id here, so
     * wind_update() finds the right per-app count.
     */
    {
        aes_app_t *app = aes_find_app_by_id(app_id);
        WORD owed = (app != NULL) ? app->update_depth : 0;

        while (owed > 0) {
            (void)wind_update(END_UPDATE);
            --owed;
        }
    }

    if (aes_state.current_app_id == app_id) {
        aes_state.current_app_id = 0;
        global[2] = 0;
    }

    for (i = 0; i < AES_MAX_APPS; ++i) {
        if (aes_state.apps[i].used != 0 && aes_state.apps[i].id == app_id) {
            memset(&aes_state.apps[i], 0, sizeof(aes_state.apps[i]));
            break;
        }
    }
    /* A desktop/menu owner may exit before the other two applications. */
    if (!aes_state.desktop_owner_app_id) {
        for (i = 0; i < AES_MAX_APPS; ++i) {
            if (aes_state.apps[i].used && aes_state.apps[i].menu_visible &&
                aes_state.apps[i].menu_tree) {
                aes_state.desktop_owner_app_id = aes_state.apps[i].id;
                break;
            }
        }
    }
    if (!aes_find_app_by_id(aes_state.active_app_id)) {
        const aes_window_t *top = aes_find_top_window();
        aes_menu_switch_to_app(top ? top->owner
                                   : aes_state.desktop_owner_app_id);
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
    gemd_free_session_menu(session);
    gem_bitmap_free(&session->bitmaps);

    (void)close(session->fd);
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

static void gemd_accept_client(void)
{
    gemd_session_t *session = gemd_alloc_session();
    int fd = accept(g_listen_fd, NULL, NULL);
    if (fd >= 0) {
        struct ucred peer;
        socklen_t length = sizeof(peer);
        if (session && gemd_nonblocking(fd) &&
            getsockopt(fd, SOL_SOCKET, SO_PEERCRED, &peer, &length) == 0 &&
            peer.uid == geteuid()) {
            memset(session, 0, sizeof(*session));
            session->fd = fd;
            session->generation = ++g_next_generation;
            session->accepted_at = gem_os_ticks_ms();
        } else
            close(fd);
    }
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
    if (strlen(gem_rpc_socket_path()) >= sizeof(addr.sun_path))
        return 0;
    strcpy(addr.sun_path, gem_rpc_socket_path());
    /* Never unlink an existing path: it may be another live server or a file.
     */
    (void)umask(0077);

    if (bind(g_listen_fd, (const struct sockaddr *)&addr, sizeof(addr)) != 0) {
        perror("gemd: bind");
        (void)close(g_listen_fd);
        g_listen_fd = -1;
        return 0;
    }
    if (lstat(addr.sun_path, &g_socket_identity) != 0)
        return 0;
    g_socket_bound = 1;
    if (chmod(addr.sun_path, 0600) != 0)
        return 0;
    if (!gemd_nonblocking(g_listen_fd))
        return 0;
    if (listen(g_listen_fd, GEMD_MAX_SESSIONS) != 0) {
        perror("gemd: listen");
        (void)close(g_listen_fd);
        g_listen_fd = -1;
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
        (void)close(g_listen_fd);
        g_listen_fd = -1;
    }
    if (g_socket_bound) {
        struct stat current;
        if (lstat(gem_rpc_socket_path(), &current) == 0 &&
            current.st_dev == g_socket_identity.st_dev &&
            current.st_ino == g_socket_identity.st_ino &&
            S_ISSOCK(current.st_mode))
            (void)unlink(gem_rpc_socket_path());
    }
}

static int gemd_session_may_run(const gemd_session_t *session)
{
    aes_app_t *app;

    if (session == NULL) {
        return 0;
    }

    if (aes_state.update_depth == 0) {
        return 1;
    }

    app = aes_find_app_by_id(session->app_id);
    return (app != NULL && app->update_depth > 0) ? 1 : 0;
}

/* Draw trees within owned windows, visible desktop, or an explicit dialog. */
static WORD gemd_tree_draw(gemd_session_t *session, gem_tree_packet_t *tree)
{
    WORD *a = tree->args;
    GRECT requested = {a[2], a[3], a[4], a[5]};
    WORD result = 1;
    vdi_begin_update();
    for (size_t i = 0; i <= AES_MAX_WINDOWS + 1; ++i) {
        GRECT base, damage, visible[64];
        const aes_window_t *window =
            i < AES_MAX_WINDOWS ? &aes_state.windows[i] : NULL;
        int desktop = i == AES_MAX_WINDOWS;
        if (window) {
            if (!window->used || !window->open ||
                window->owner != session->app_id)
                continue;
            base = window->work;
        } else if (desktop) {
            if (aes_state.desktop_owner_app_id != session->app_id)
                continue;
            aes_desktop_rect(&base);
        } else {
            if (!session->dialog_active)
                continue;
            base = session->dialog;
        }
        if (!aes_intersect_rects(&base, &requested, &damage))
            continue;
        WORD count = window || desktop
                         ? aes_clip_visible_rects(window, &damage, visible, 64)
                         : 1;
        if (!window && !desktop)
            visible[0] = damage;
        for (WORD j = 0; j < count; ++j)
            result = objc_draw(tree->objects, a[0], a[1], visible[j].g_x,
                               visible[j].g_y, visible[j].g_w, visible[j].g_h);
    }
    vdi_end_update();
    return result;
}

static int32_t gemd_dispatch(gemd_session_t *session,
                             const gem_rpc_header_t *header,
                             const uint8_t *payload, uint8_t *response,
                             uint32_t *response_size)
{
    int32_t status = 0;

    *response_size = 0u;
    gemd_set_current_app(session);

    switch ((gem_rpc_opcode_t)header->opcode) {
        case GEM_RPC_AES_EXT: {
            gem_aes_packet_t *packet = (gem_aes_packet_t *)response;
            memcpy(packet, payload, sizeof(*packet));
            *response_size = sizeof(*packet);
            if ((packet->function == rpc_appl_read ||
                 packet->function == rpc_menu_register ||
                 packet->function == rpc_menu_unregister) &&
                packet->args[0] != session->app_id)
                break;
            if (packet->function == rpc_appl_write)
                packet->data[1] = session->app_id;
            if (packet->function == rpc_form_dial) {
                if (packet->args[0] == FMD_START ||
                    packet->args[0] == FMD_GROW) {
                    GRECT screen = {0, 0, (WORD)(aes_state.work_out[0] + 1),
                                    (WORD)(aes_state.work_out[1] + 1)};
                    GRECT requested = {packet->args[5], packet->args[6],
                                       packet->args[7], packet->args[8]};
                    session->dialog_active = aes_intersect_rects(
                        &screen, &requested, &session->dialog);
                } else
                    session->dialog_active = 0;
            }
            status = gem_aes_dispatch(packet);
            break;
        }
        case GEM_RPC_AES_TREE: {
            gem_tree_packet_t *tree = (gem_tree_packet_t *)response;
            memcpy(tree, payload, sizeof(*tree));
            *response_size = sizeof(*tree);
            if (tree->operation > tree_slidebox || !gem_tree_decode(tree, 1))
                break;
            for (unsigned i = 0; i < tree->count; ++i) {
                if ((tree->objects[i].ob_type & 0xff) == G_USERDEF) {
                    USERBLK *user =
                        (USERBLK *)(intptr_t)tree->objects[i].ob_spec;
                    user->ab_code = (LONG)(intptr_t)gemd_user_callback;
                }
            }
            WORD *a = tree->args;
            if (tree->operation != tree_form_center &&
                (a[0] < 0 || a[0] >= tree->count)) {
                gem_tree_encode(tree);
                break;
            }
            if (tree->identity && tree->identity == session->edit_identity &&
                session->edit_object >= 0 &&
                session->edit_object < tree->count) {
                aes_state.edit_tree = tree->objects;
                aes_state.edit_object = session->edit_object;
                aes_state.edit_index = session->edit_index;
            }
            switch (tree->operation) {
                case tree_draw:
                    status = gemd_tree_draw(session, tree);
                    break;
                case tree_edit: {
                    unsigned type = tree->objects[a[0]].ob_type & 0xff;
                    if (type == G_TEXT || type == G_BOXTEXT ||
                        type == G_FTEXT || type == G_FBOXTEXT) {
                        TEDINFO *ted =
                            (TEDINFO *)(intptr_t)tree->objects[a[0]].ob_spec;
                        if (a[2] >= 0 && a[2] < ted->te_txtlen)
                            status = objc_edit(tree->objects, a[0], a[1], &a[2],
                                               a[3]);
                    }
                    break;
                }
                case tree_form_do:
                    status = form_do(tree->objects, a[0]);
                    break;
                case tree_form_center:
                    status =
                        form_center(tree->objects, &a[0], &a[1], &a[2], &a[3]);
                    break;
                case tree_form_keybd:
                    status = form_keybd(tree->objects, a[0], a[1], a[2], &a[3],
                                        &a[4]);
                    break;
                case tree_form_button:
                    status = form_button(tree->objects, a[0], a[1], &a[2]);
                    break;
                case tree_watchbox:
                    status = graf_watchbox(tree->objects, a[0], a[1], a[2]);
                    break;
                case tree_slidebox:
                    if (a[1] >= 0 && a[1] < tree->count)
                        status = graf_slidebox(tree->objects, a[0], a[1], a[2]);
                    break;
            }
            if (tree->operation == tree_edit) {
                session->edit_identity =
                    aes_state.edit_tree == tree->objects ? tree->identity : 0;
                session->edit_object = aes_state.edit_object;
                session->edit_index = aes_state.edit_index;
            }
            /* Native edit/hover state must never retain this temporary tree. */
            if (aes_state.edit_tree == tree->objects)
                aes_state.edit_tree = NULL;
            if (aes_state.hover_tree == tree->objects)
                aes_state.hover_tree = NULL;
            gem_tree_encode(tree);
            break;
        }
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
        case GEM_RPC_GRAF_MKSTATE: {
            gem_rpc_words8_t *rsp = (gem_rpc_words8_t *)response;
            memset(rsp, 0, sizeof(*rsp));
            if (g_modal_session && session != g_modal_session) {
                rsp->values[0] = vdi_state.mouse_x;
                rsp->values[1] = vdi_state.mouse_y;
                rsp->values[2] = vdi_state.mouse_status;
                rsp->values[3] = aes_state.key_state;
            } else {
                graf_mkstate(&rsp->values[0], &rsp->values[1], &rsp->values[2],
                             &rsp->values[3]);
            }
            *response_size = sizeof(*rsp);
            status = 1;
        } break;
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
        case GEM_RPC_SCRP_READ: {
            gem_rpc_path_t *rsp = (gem_rpc_path_t *)response;
            memset(rsp, 0, sizeof(*rsp));
            status = scrp_read(rsp->text);
            *response_size = sizeof(*rsp);
        } break;
        case GEM_RPC_SCRP_WRITE: {
            gem_rpc_path_t req;
            memcpy(&req, payload, sizeof(req));
            req.text[sizeof(req.text) - 1] = '\0';
            status = scrp_write(req.text);
        } break;
        case GEM_RPC_FSEL_INPUT: {
            gem_rpc_fsel_t *rsp = (gem_rpc_fsel_t *)response;
            memcpy(rsp, payload, sizeof(*rsp));
            rsp->path[sizeof(rsp->path) - 1] = '\0';
            rsp->name[sizeof(rsp->name) - 1] = '\0';
            status = fsel_input(rsp->path, rsp->name, &rsp->button);
            *response_size = sizeof(*rsp);
        } break;
        case GEM_RPC_APPL_INIT:
            status = session->app_id ? session->app_id : appl_init();
            session->app_id = (WORD)status;
            break;

        case GEM_RPC_APPL_EXIT:
            if (session->app_id == 0) {
                status = 0;
                break;
            }
            gemd_cleanup_app(session->app_id);
            session->app_id = 0;
            /* Allow the exit reply to drain before a new initialization
             * deadline. The original acceptance time may already be many
             * seconds old. */
            session->accepted_at = gem_os_ticks_ms();
            session->drawing.initialized = 0;
            gemd_free_session_menu(session);
            status = 1;
            break;

        case GEM_RPC_EVNT_MESAG: {
            gem_rpc_words8_t *rsp = (gem_rpc_words8_t *)response;

            memset(rsp, 0, sizeof(*rsp));
            *response_size = (uint32_t)sizeof(*rsp);
            if (aes_dequeue_message(rsp->values) != 0) {
                status = 1;
                *response_size = (uint32_t)sizeof(*rsp);
            } else {
                status = 0;
            }
        } break;

        case GEM_RPC_EVNT_MULTI: {
            const gem_rpc_evnt_multi_req_t *req =
                (const gem_rpc_evnt_multi_req_t *)payload;
            gem_rpc_evnt_multi_rsp_t *rsp =
                (gem_rpc_evnt_multi_rsp_t *)response;
            UWORD client_wants_timer = (UWORD)(req->flags & MU_TIMER);
            UWORD bounded_flags = (UWORD)(req->flags | MU_TIMER);
            UWORD bounded_tlc = 0u;
            UWORD bounded_thc = 0u;

            /*
             * Input is already routed into application queues. Poll them
             * without sleeping: even a two-millisecond wait here multiplies
             * across idle clients and delays every drawing RPC. libgem owns
             * timer deadlines and yields between empty polls. Swallow the
             * synthetic timeout when the caller did not request MU_TIMER.
             */
            memset(rsp, 0, sizeof(*rsp));
            if (g_modal_session && session != g_modal_session) {
                /* Classic synchronous panels keep input modal, but other
                 * processes can still receive redraws and advance timers. */
                rsp->mx = vdi_state.mouse_x;
                rsp->my = vdi_state.mouse_y;
                rsp->mb = vdi_state.mouse_status;
                rsp->ks = aes_state.key_state;
                if ((req->flags & MU_MESAG) && aes_dequeue_message(rsp->msg))
                    rsp->event = MU_MESAG;
                else if (client_wants_timer)
                    rsp->event = MU_TIMER;
                status = rsp->event;
                *response_size = sizeof(*rsp);
                break;
            }
            rsp->event =
                evnt_multi(bounded_flags, req->bclk, req->bmsk, req->bst,
                           req->m1flags, req->m1x, req->m1y, req->m1w, req->m1h,
                           req->m2flags, req->m2x, req->m2y, req->m2w, req->m2h,
                           rsp->msg, bounded_tlc, bounded_thc, &rsp->mx,
                           &rsp->my, &rsp->mb, &rsp->ks, &rsp->kr, &rsp->br);
            if (rsp->event == MU_TIMER && client_wants_timer == 0u) {
                rsp->event = 0;
            }
            status = rsp->event;
            *response_size = (uint32_t)sizeof(*rsp);
        } break;

        case GEM_RPC_GRAF_HANDLE: {
            gem_rpc_words16_t *rsp = (gem_rpc_words16_t *)response;

            memset(rsp, 0, sizeof(*rsp));
            status = graf_handle(&rsp->values[0], &rsp->values[1],
                                 &rsp->values[2], &rsp->values[3]);
            aes_trace("gemd graf_handle status=%d aes_vdi=%d ready=%d",
                      (int)status, aes_state.vdi_handle, aes_state.vdi_ready);
            if (g_server_vdi_handle == 0 && aes_state.vdi_ready != 0 &&
                aes_state.vdi_handle != 0) {
                g_server_vdi_handle = aes_state.vdi_handle;
                memcpy(g_server_work_out, aes_state.work_out,
                       sizeof(g_server_work_out));
            }
            *response_size = (uint32_t)sizeof(*rsp);
        } break;

        case GEM_RPC_GRAF_MOUSE: {
            const gem_rpc_graf_mouse_req_t *req =
                (const gem_rpc_graf_mouse_req_t *)payload;

            status = graf_mouse(req->mode,
                                req->has_form ? (void *)&req->form : NULL);
        } break;

        case GEM_RPC_FORM_ALERT: {
            const gem_rpc_form_alert_req_t *req =
                (const gem_rpc_form_alert_req_t *)payload;
            char text[GEM_RPC_TEXT_MAX];

            memcpy(text, req->text, sizeof(text));
            text[sizeof(text) - 1u] = '\0';
            status = form_alert(req->defbut, text);
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

        case GEM_RPC_WIND_CREATE: {
            const gem_rpc_wind_create_req_t *req =
                (const gem_rpc_wind_create_req_t *)payload;

            status = wind_create(req->kind, req->x, req->y, req->w, req->h);
        } break;

        case GEM_RPC_WIND_OPEN: {
            const gem_rpc_wind_open_req_t *req =
                (const gem_rpc_wind_open_req_t *)payload;

            status = wind_open(req->handle, req->x, req->y, req->w, req->h);
            aes_trace("gemd wind_open handle=%d rect=%d,%d %dx%d status=%d "
                      "aes_vdi=%d ready=%d",
                      req->handle, req->x, req->y, req->w, req->h, (int)status,
                      aes_state.vdi_handle, aes_state.vdi_ready);
        } break;

        case GEM_RPC_WIND_CLOSE: {
            const gem_rpc_handle_req_t *req =
                (const gem_rpc_handle_req_t *)payload;

            status = wind_close(req->handle);
        } break;

        case GEM_RPC_WIND_DELETE: {
            const gem_rpc_handle_req_t *req =
                (const gem_rpc_handle_req_t *)payload;

            status = wind_delete(req->handle);
        } break;

        case GEM_RPC_WIND_GET: {
            const gem_rpc_wind_get_req_t *req =
                (const gem_rpc_wind_get_req_t *)payload;
            gem_rpc_wind_get_rsp_t *rsp = (gem_rpc_wind_get_rsp_t *)response;

            memset(rsp, 0, sizeof(*rsp));
            status = wind_get(req->handle, req->field, &rsp->w1, &rsp->w2,
                              &rsp->w3, &rsp->w4);
            *response_size = (uint32_t)sizeof(*rsp);
        } break;

        case GEM_RPC_WIND_SET: {
            const gem_rpc_wind_set_req_t *req =
                (const gem_rpc_wind_set_req_t *)payload;

            status = wind_set(req->handle, req->field, req->w1, req->w2,
                              req->w3, req->w4);
        } break;

        case GEM_RPC_WIND_SET_STR: {
            const gem_rpc_wind_set_str_req_t *req =
                (const gem_rpc_wind_set_str_req_t *)payload;
            char text[GEM_RPC_TEXT_MAX];

            memcpy(text, req->text, sizeof(text));
            text[sizeof(text) - 1u] = '\0';
            status = wind_set_str(req->handle, req->field, text);
        } break;

        case GEM_RPC_WIND_FIND: {
            const gem_rpc_wind_find_req_t *req =
                (const gem_rpc_wind_find_req_t *)payload;

            status = wind_find(req->x, req->y);
        } break;

        case GEM_RPC_WIND_UPDATE: {
            const gem_rpc_wind_update_req_t *req =
                (const gem_rpc_wind_update_req_t *)payload;

            aes_app_t *app = aes_find_app_by_id(session->app_id);
            if (app && req->flag == BEG_UPDATE && app->update_depth == 0)
                session->update_started = gem_os_ticks_ms();
            status = wind_update(req->flag);
            aes_trace("gemd wind_update flag=%d status=%d", req->flag,
                      (int)status);
        } break;

        case GEM_RPC_WIND_CALC: {
            const gem_rpc_wind_calc_req_t *req =
                (const gem_rpc_wind_calc_req_t *)payload;
            gem_rpc_wind_calc_rsp_t *rsp = (gem_rpc_wind_calc_rsp_t *)response;

            memset(rsp, 0, sizeof(*rsp));
            status = wind_calc(req->type, req->kind, req->inx, req->iny,
                               req->inw, req->inh, &rsp->outx, &rsp->outy,
                               &rsp->outw, &rsp->outh);
            *response_size = (uint32_t)sizeof(*rsp);
        } break;

        case GEM_RPC_MENU_BAR: {
            const gem_rpc_menu_bar_req_t *req =
                (const gem_rpc_menu_bar_req_t *)payload;
            WORD count = req->object_count;
            WORD i;
            OBJECT *objects;
            char *strings_blob = NULL;

            if (req->show == 0) {
                status = menu_bar(session->menu_objects, 0);
                gemd_free_session_menu(session);
                break;
            }

            if (count <= 0 || count > (WORD)GEM_RPC_MENU_MAX_OBJECTS) {
                status = 0;
                break;
            }

            objects = malloc((size_t)count * sizeof(OBJECT));
            if (objects == NULL) {
                status = 0;
                break;
            }
            memcpy(objects, req->objects, (size_t)count * sizeof(OBJECT));

            if (req->string_count > 0) {
                strings_blob =
                    malloc((size_t)req->string_count * GEM_RPC_MENU_STRING_MAX);
                if (strings_blob == NULL) {
                    free(objects);
                    status = 0;
                    break;
                }
            }

            for (i = 0; i < req->string_count; ++i) {
                WORD obj_index = req->strings[i].object;
                char *slot = strings_blob + (size_t)i * GEM_RPC_MENU_STRING_MAX;

                memcpy(slot, req->strings[i].text, GEM_RPC_MENU_STRING_MAX);
                slot[GEM_RPC_MENU_STRING_MAX - 1u] = '\0';
                if (obj_index >= 0 && obj_index < count) {
                    objects[obj_index].ob_spec = (LONG)(intptr_t)slot;
                }
            }

            if (session->menu_objects != NULL) {
                menu_bar(session->menu_objects, 0);
            }
            gemd_free_session_menu(session);
            session->menu_objects = objects;
            session->menu_strings_blob = strings_blob;
            session->menu_count = count;

            status = menu_bar(objects, req->show);
            aes_trace(
                "gemd menu_bar app=%d objects=%d strings=%d "
                "title0=%s show=%d status=%d",
                session->app_id, count, req->string_count,
                req->string_count > 0
                    ? (const char *)(intptr_t)objects[req->strings[0].object]
                          .ob_spec
                    : "(none)",
                req->show, (int)status);
        } break;

        case GEM_RPC_MENU_TNORMAL: {
            const gem_rpc_menu_tnormal_req_t *req =
                (const gem_rpc_menu_tnormal_req_t *)payload;

            if (session->menu_objects == NULL || req->title < 0 ||
                req->title >= session->menu_count ||
                (session->menu_objects[req->title].ob_type & 0xffu) !=
                    G_TITLE) {
                status = 0;
            } else {
                status = menu_tnormal(session->menu_objects, req->title,
                                      req->normal);
            }
        } break;

        case GEM_RPC_MENU_CLICK: {
            const gem_rpc_menu_click_req_t *req =
                (const gem_rpc_menu_click_req_t *)payload;

            status = menu_click(req->click, req->setit);
        } break;

        default:
            status = -1;
            break;
    }

    return status;
}

/* Connection identity, not a caller-supplied handle, determines authority. */
static int gemd_authorized(const gemd_session_t *session, uint16_t opcode,
                           const void *payload)
{
    const aes_window_t *window;
    WORD handle;
    if (opcode == GEM_RPC_APPL_INIT) {
        for (size_t i = 0; i < GEMD_MAX_SESSIONS; ++i) {
            if (&g_sessions[i] != session && g_sessions[i].standalone)
                return 0;
        }
        return 1;
    }
    if (!session->app_id)
        return 0;
    if (gemd_vdi_request(opcode)) {
        memcpy(&handle, payload, sizeof(handle));
        if (!g_server_vdi_handle || handle != g_server_vdi_handle)
            return 0;
    }
    switch (opcode) {
        case GEM_RPC_WIND_OPEN:
        case GEM_RPC_WIND_CLOSE:
        case GEM_RPC_WIND_DELETE:
        case GEM_RPC_WIND_SET:
        case GEM_RPC_WIND_SET_STR:
            memcpy(&handle, payload, sizeof(handle));
            window = aes_find_window(handle);
            return window && window->owner == session->app_id;
        case GEM_RPC_WIND_UPDATE: {
            const gem_rpc_wind_update_req_t *req = payload;
            const aes_app_t *app = aes_find_app_by_id(session->app_id);
            return app &&
                   ((req->flag == BEG_UPDATE && app->update_depth < 64) ||
                    (req->flag == END_UPDATE && app->update_depth > 0));
        }
        case GEM_RPC_WIND_CREATE: {
            size_t i;
            int count = 0;
            for (i = 0; i < AES_MAX_WINDOWS; ++i)
                if (aes_state.windows[i].used &&
                    aes_state.windows[i].owner == session->app_id)
                    ++count;
            return count < 8;
        }
        default:
            return 1;
    }
}

/* Raster writes are confined to the client's visible work areas. Only the
 * desktop owner also paints uncovered desktop; AES alone paints shared chrome.
 */
static int32_t gemd_draw_owned(gemd_session_t *session,
                               const gem_rpc_header_t *header,
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

static int gemd_handle_request(gemd_session_t *session)
{
    const gem_rpc_header_t header = session->io.header;
    const uint8_t *payload = session->io.payload;
    _Alignas(max_align_t) uint8_t response[GEM_RPC_PAYLOAD_MAX];
    uint32_t response_size = 0u;
    int32_t status;

    if (!gem_rpc_valid_request(header.opcode, payload, header.size)) {
        gemd_reply(&session->io, -1, NULL, 0);
        return 1;
    }
    if (!gemd_authorized(session, header.opcode, payload)) {
        gemd_reply(&session->io, 0, NULL, 0);
        return 1;
    }
    if (gemd_vdi_request(header.opcode)) {
        gemd_drawing_t server;
        gemd_drawing_save(&server);
        if (!session->drawing.initialized)
            gemd_drawing_init(&session->drawing);
        gemd_drawing_restore(&session->drawing);
        if (gemd_raster_request(header.opcode) ||
            (header.opcode == GEM_RPC_BITMAP_COPY &&
             !((const gem_bitmap_call_t *)payload)->destination.memory) ||
            (header.opcode == GEM_RPC_VDI_EXT &&
             gem_vdi_draws(((const gem_vdi_packet_t *)payload)->function))) {
            if (header.opcode == GEM_RPC_VDI_EXT) {
                memcpy(response, payload, sizeof(gem_vdi_packet_t));
                response_size = sizeof(gem_vdi_packet_t);
            }
            status = gemd_draw_owned(session, &header, payload, response,
                                     &response_size);
            if (session->standalone)
                gemd_drawing_save(&session->drawing);
        } else {
            status = gemd_dispatch(session, &header, payload, response,
                                   &response_size);
            gemd_drawing_save(&session->drawing);
        }
        gemd_drawing_restore(&server);
    } else {
        if (header.opcode == GEM_RPC_FORM_ALERT ||
            header.opcode == GEM_RPC_FSEL_INPUT ||
            header.opcode == GEM_RPC_AES_TREE ||
            header.opcode == GEM_RPC_AES_EXT) {
            g_modal_session = session;
            aes_wait_hook = gemd_service_modal;
        }
        status =
            gemd_dispatch(session, &header, payload, response, &response_size);
        if (g_modal_session == session) {
            aes_wait_hook = NULL;
            g_modal_session = NULL;
        }
    }
    gemd_reply(&session->io, status, response, response_size);
    return 1;
}

/* Cooperate from AES panel waits without recursively opening another panel.
 * Attribute and app identity restoration keeps the suspended AES call intact.
 */
static int gemd_service_modal(void)
{
    gemd_drawing_t drawing;
    WORD app_id = aes_state.current_app_id;
    size_t i;
    struct pollfd owner = {g_modal_session->fd, POLLRDHUP, 0};
    if (g_stopping || poll(&owner, 1, 0) < 0 ||
        (owner.revents & (POLLRDHUP | POLLHUP | POLLERR | POLLNVAL)))
        return 0;
    gemd_drawing_save(&drawing);
    gemd_accept_client();
    for (i = 0; i < GEMD_MAX_SESSIONS; ++i) {
        gemd_session_t *session = &g_sessions[i];
        struct pollfd fd;
        uint32_t now = gem_os_ticks_ms();
        aes_app_t *app = aes_find_app_by_id(session->app_id);
        if (session->fd < 0)
            continue;
        if (app && app->update_depth && now - session->update_started > 5000u) {
            if (session == g_modal_session) {
                gemd_drawing_restore(&drawing);
                aes_state.current_app_id = global[2] = app_id;
                return 0;
            }
            aes_trace("modal update timeout app=%d opcode=%u age=%u",
                      session->app_id, session->io.header.opcode,
                      now - session->update_started);
            gemd_close_session(session);
            continue;
        }
        if (session == g_modal_session)
            continue;
        fd.fd = session->fd;
        fd.events = session->io.output_size ? POLLOUT
                                            : (session->io.ready ? 0 : POLLIN);
        fd.revents = 0;
        (void)poll(&fd, 1, 0);
        if ((fd.revents & (POLLHUP | POLLERR | POLLNVAL)) ||
            ((fd.revents & POLLIN) &&
             gemd_receive(session->fd, &session->io) < 0) ||
            ((fd.revents & POLLOUT) &&
             gemd_send(session->fd, &session->io) < 0)) {
            gemd_close_session(session);
            continue;
        }
        /* Receiving a new frame can advance io.started. Read the clock
         * afterwards: an earlier sample minus a newer timestamp wraps to
         * UINT32_MAX and would disconnect a healthy client immediately. */
        now = gem_os_ticks_ms();
        if ((((session->io.header_read && !session->io.ready) ||
              session->io.output_size) &&
             now - session->io.started > 2000u) ||
            (!session->app_id && now - session->accepted_at > 5000u)) {
            aes_trace("modal transport timeout app=%d opcode=%u age=%u",
                      session->app_id, session->io.header.opcode,
                      now - session->io.started);
            gemd_close_session(session);
            continue;
        }
        if (session->io.ready && gemd_session_may_run(session) &&
            session->io.header.opcode != GEM_RPC_FORM_ALERT &&
            session->io.header.opcode != GEM_RPC_FSEL_INPUT &&
            session->io.header.opcode != GEM_RPC_AES_TREE &&
            session->io.header.opcode != GEM_RPC_AES_EXT)
            (void)gemd_handle_request(session);
    }
    gemd_drawing_restore(&drawing);
    aes_state.current_app_id = global[2] = app_id;
    return 1;
}

static void gemd_stop(int signal_number)
{
    (void)signal_number;
    g_stopping = 1;
}

int main(void)
{
    size_t i;

    aes_external_input = 1;

    /*
     * Writing to a session socket whose peer already closed its end
     * raises SIGPIPE, whose default disposition kills the process --
     * taking down every other connected client with it. gemd_send
     * already handles a plain -1/EPIPE return gracefully; ignoring the
     * signal is what lets that code path run instead of the process
     * dying first.
     */
    (void)signal(SIGPIPE, SIG_IGN);
    (void)signal(SIGTERM, gemd_stop);
    (void)signal(SIGINT, gemd_stop);

    for (i = 0; i < GEMD_MAX_SESSIONS; ++i) {
        g_sessions[i].fd = -1;
    }

    if (!gemd_init_listener()) {
        gemd_shutdown();
        return 1;
    }

    printf("gemd listening on %s\n", gem_rpc_socket_path());
    fflush(stdout);

    while (!g_stopping) {
        /*
         * pollfds[] and poll_owner[] are built together, in the same
         * pass, so pollfds[k] and poll_owner[k] always describe the
         * same fd by construction. Earlier versions rebuilt this
         * fd-to-session mapping a second time (by re-running the same
         * "skip if not connected" scan) for the close/dispatch pass
         * below; that second derivation could drift from the first
         * whenever a session was accepted, closed, or reused in
         * between, silently pointing a session at another session's
         * poll result. That one fragile pattern was the root cause of
         * several different-looking failures (a session that never
         * got serviced, one serviced with the wrong readiness state,
         * a freshly-accepted connection closed on the spot). Deriving
         * the mapping exactly once removes the whole class of bug
         * rather than one instance of it.
         */
        struct pollfd pollfds[GEMD_MAX_SESSIONS + 1];
        gemd_session_t *poll_owner[GEMD_MAX_SESSIONS + 1];
        uint64_t poll_generation[GEMD_MAX_SESSIONS + 1];
        nfds_t nfds = 0;
        nfds_t k;
        int rc;

        pollfds[nfds].fd = g_listen_fd;
        pollfds[nfds].events = POLLIN;
        pollfds[nfds].revents = 0;
        poll_owner[nfds] = NULL;
        ++nfds;

        for (i = 0; i < GEMD_MAX_SESSIONS; ++i) {
            if (g_sessions[i].fd >= 0) {
                pollfds[nfds].fd = g_sessions[i].fd;
                pollfds[nfds].events =
                    g_sessions[i].io.output_size
                        ? POLLOUT
                        : (g_sessions[i].io.ready ? 0 : POLLIN);
                pollfds[nfds].revents = 0;
                poll_owner[nfds] = &g_sessions[i];
                poll_generation[nfds] = g_sessions[i].generation;
                ++nfds;
            }
        }

        rc = poll(pollfds, nfds, 20);
        if (rc < 0) {
            if (errno == EINTR) {
                continue;
            }
            perror("gemd: poll");
            break;
        }
        gemd_pump_hid();

        for (k = 1; k < nfds; ++k) {
            gemd_session_t *session = poll_owner[k];

            /* A modal callback may have closed and reused this session slot
             * (and even its fd) since this poll snapshot was built. */
            if (session == NULL || session->fd != pollfds[k].fd ||
                session->generation != poll_generation[k]) {
                continue;
            }
            /* Bound unfinished frames, unread replies, init and update locks.
             * Deadlines exclude synchronous HID tracking pauses; client
             * activity alone never resets an existing lock deadline. */
            {
                uint32_t now = gem_os_ticks_ms();
                aes_app_t *app = aes_find_app_by_id(session->app_id);
                if (((session->io.header_read && !session->io.ready) ||
                     session->io.output_size) &&
                    now - session->io.started > 2000u) {
                    gemd_close_session(session);
                    continue;
                }
                if ((!session->app_id && now - session->accepted_at > 5000u) ||
                    (app && app->update_depth &&
                     now - session->update_started > 5000u)) {
                    gemd_close_session(session);
                    continue;
                }
            }
            if ((pollfds[k].revents & (POLLERR | POLLHUP | POLLNVAL)) != 0 ||
                ((pollfds[k].revents & POLLIN) != 0 &&
                 gemd_receive(session->fd, &session->io) < 0) ||
                ((pollfds[k].revents & POLLOUT) != 0 &&
                 gemd_send(session->fd, &session->io) < 0)) {
                gemd_close_session(session);
                continue;
            }
            /* Recheck after every dispatch: an earlier client may have locked.
             */
            if (session->io.ready && gemd_session_may_run(session))
                (void)gemd_handle_request(session);
        }

        if ((pollfds[0].revents & POLLIN) != 0) {
            gemd_accept_client();
        }

        gemd_pump_hid();
    }

    gemd_shutdown();
    return 0;
}
