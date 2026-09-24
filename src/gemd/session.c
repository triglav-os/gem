/*
 * Owns gemd's connection table: listener setup with a private socket,
 * peer-credential checked accepts, per-session resource cleanup when an
 * application exits or disconnects, the shared server workstation and the
 * ownership checks that authorize each request.
 *
 * MIT License (see: LICENSE)
 * Copyright (C) 2026 tomaz stih
 */

#define _GNU_SOURCE
#include "session.h"

#include "../aes/aes_shel.h"
#include "../aes/system_menu.h"

#include "platform/raster.h"
#include "overlay.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <unistd.h>

void gemd_free_session_menu(gemd_session_t *session)
{
    free(session->menu_objects);
    session->menu_objects = NULL;
    free(session->menu_strings_blob);
    session->menu_strings_blob = NULL;
    session->menu_count = 0;
}

void gemd_set_current_app(const gemd_session_t *session)
{
    if (session == NULL) {
        aes_shel_set_caller_pid(0);
        aes_state.current_app_id = 0;
        global[2] = 0;
        return;
    }
    aes_shel_set_caller_pid(session->pid);
    aes_state.current_app_id = session->app_id;
    global[2] = session->app_id;
}

int gemd_open_server_vdi(WORD work_out[57])
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

void gemd_cleanup_app(WORD app_id)
{
    size_t i;

    if (app_id == 0) {
        return;
    }

    gemd_overlay_cleanup_app(app_id);

    aes_system_menu_unregister_owner(app_id);

    aes_state.current_app_id = app_id;
    global[2] = app_id;
    /* Detach while the session's relocated tree is still alive. In
     * particular, the desktop owner has no fallback menu to switch to. */
    if (aes_state.menu_owner_app_id == app_id) {
        (void)menu_bar(aes_state.menu_tree, 0);
        aes_state.menu_owner_app_id = 0;
    }

    if (aes_state.desktop_owner_app_id == app_id) {
        aes_state.desktop_owner_app_id = 0;
        gemd_location_clear_selection();
    }
    if (aes_state.input_focus_app_id == app_id) {
        aes_state.input_focus_app_id = 0;
    }
    if (aes_state.modal_form_owner_app_id == app_id) {
        GRECT exposed = aes_state.modal_form_rect;

        aes_state.modal_form_owner_app_id = 0;
        aes_set_rect(&aes_state.modal_form_rect, 0, 0, 0, 0);
        if (exposed.g_w > 0 && exposed.g_h > 0)
            aes_redraw_region(&exposed);
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

    /* Drop only what was addressed to the leaving application. Messages it
     * sent, and the WM_REDRAW hints the AES queued to other applications on
     * its behalf while it closed its windows, carry its id as the sender
     * and must still be delivered: purging them left the uncovered areas
     * of other windows showing the closed window's pixels. */
    /* The bar follows whichever window is on top once this application's
     * windows are gone, and falls back to the desktop owner. */
    if (aes_state.menu_owner_app_id == app_id ||
        aes_state.active_app_id == app_id) {
        const aes_window_t *top = aes_find_top_window();
        WORD next = top != NULL ? top->owner : aes_state.desktop_owner_app_id;
        if (next != app_id) {
            aes_menu_switch_to_app(next);
        }
    }

    for (i = 0; i < AES_MAX_MESSAGES; ++i) {
        if (aes_state.messages[i].used != 0 &&
            aes_state.messages[i].dest == app_id) {
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

void gemd_close_session(gemd_session_t *session)
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
    /* The slot is only re-zeroed when a new client lands in it; a stale
     * exclusive claim here would otherwise refuse every later appl_init. */
    session->standalone = 0;
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

void gemd_accept_client(void)
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
            session->pid = (int)peer.pid;
            session->generation = ++g_next_generation;
            session->accepted_at = gem_os_ticks_ms();
        } else
            close(fd);
    }
}

int gemd_init_listener(void)
{
    struct sockaddr_un addr;

    g_listen_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (g_listen_fd < 0) {
        perror("gemd: socket");
        return 0;
    }

    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    if (strlen(gem_rpc_socket_path()) >= sizeof(addr.sun_path)) {
        fprintf(stderr, "gemd: GEMD_SOCKET path exceeds %zu bytes: %s\n",
                sizeof(addr.sun_path) - 1u, gem_rpc_socket_path());
        return 0;
    }
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
    if (lstat(addr.sun_path, &g_socket_identity) != 0) {
        perror("gemd: lstat");
        return 0;
    }
    g_socket_bound = 1;
    if (chmod(addr.sun_path, 0600) != 0) {
        perror("gemd: chmod");
        return 0;
    }
    if (!gemd_nonblocking(g_listen_fd)) {
        perror("gemd: fcntl");
        return 0;
    }
    if (listen(g_listen_fd, GEMD_MAX_SESSIONS) != 0) {
        perror("gemd: listen");
        (void)close(g_listen_fd);
        g_listen_fd = -1;
        return 0;
    }
    return 1;
}

void gemd_shutdown(void)
{
    size_t i;

    for (i = 0; i < GEMD_MAX_SESSIONS; ++i) {
        gemd_close_session(&g_sessions[i]);
    }
    /* Closing windows restores the desktop pattern beneath them, so blank the
     * framebuffer only now -- after every such repaint and while it is still
     * mapped -- to leave the viewer a cleared screen with no window ghosts. */
    gem_raster_clear();
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
            (void)gem_os_unlink(gem_rpc_socket_path());
    }
}

int gemd_session_may_run(const gemd_session_t *session)
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

/* Connection identity, not a caller-supplied handle, determines authority. */
int gemd_authorized(const gemd_session_t *session, uint16_t opcode,
                    const void *payload)
{
    const aes_window_t *window;
    WORD handle;
    if (opcode == GEM_RPC_APPL_INIT) {
        for (size_t i = 0; i < GEMD_MAX_SESSIONS; ++i) {
            if (&g_sessions[i] != session && g_sessions[i].fd >= 0 &&
                g_sessions[i].standalone)
                return 0;
        }
        return 1;
    }
    if (!session->app_id)
        return 0;
    if (opcode == GEM_RPC_LOCATION_LIST || opcode == GEM_RPC_LOCATION_ADD ||
        opcode == GEM_RPC_LOCATION_REMOVE || opcode == GEM_RPC_LOCATION_SELECT) {
        return session->app_id == aes_state.desktop_owner_app_id;
    }
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
