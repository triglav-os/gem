/*
 * Declares gemd's private session model shared by the server loop, the
 * session table and the request dispatchers: the per-connection state,
 * the process-wide server state and the helpers each module exports.
 *
 * MIT License (see: LICENSE)
 * Copyright (C) 2026 tomaz stih
 */

#ifndef GEMD_SESSION_H
#define GEMD_SESSION_H

#include "../aes/aes_internal.h"
#include "../gem/gem_protocol.h"
#include "transport.h"
#include "drawing.h"
#include "gem/gemd.h"
#include "gem/gemconf.h"

#include "platform/hid.h"
#include "platform/os.h"

#include <signal.h>
#include <stdint.h>
#include <sys/stat.h>

enum { GEMD_MAX_SESSIONS = 16 };

typedef struct gemd_session {
    int fd;
    int pid;
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
    int handling_request;
    gemd_drawing_t drawing;
} gemd_session_t;

/* Process-wide server state, defined in main.c. */
extern int g_listen_fd;
extern uint64_t g_next_generation;
extern gemd_session_t g_sessions[GEMD_MAX_SESSIONS];
extern WORD g_server_vdi_handle;
extern WORD g_server_vdi_refs;
extern WORD g_server_work_out[57];
extern struct stat g_socket_identity;
extern int g_socket_bound;
extern volatile sig_atomic_t g_stopping;
extern gemd_session_t *g_modal_session;
extern gemconf_t g_gemd_config;

/* Record and announce a desktop.cfg save already applied to the cache. */
void gemd_desktop_config_saved(void);

/* Release a session's copied menu objects and strings. */
void gemd_free_session_menu(gemd_session_t *session);
/* Make a session's application current for AES; NULL clears it. */
void gemd_set_current_app(const gemd_session_t *session);
/* Open or reuse the one server workstation; copies work_out when given. */
int gemd_open_server_vdi(WORD work_out[57]);
/* Detach menus, windows, messages and locks owned by an application. */
void gemd_cleanup_app(WORD app_id);
/* Close a connection and release everything it owned. */
void gemd_close_session(gemd_session_t *session);
/* Accept one pending connection into a free session slot. */
void gemd_accept_client(void);
/* Bind the private listening socket; nonzero on success. */
int gemd_init_listener(void);
/* Close every session, the workstation and the listener. */
void gemd_shutdown(void);
/* Handle a physical Delete for the location currently selected by Desktop. */
int gemd_location_delete_event(void);
void gemd_location_clear_selection(void);
/* Nonzero when the session may run a request under update locks. */
int gemd_session_may_run(const gemd_session_t *session);
/* Nonzero when the session may perform an already validated request. */
int gemd_authorized(const gemd_session_t *session, uint16_t opcode,
                    const void *payload);
/* Execute a VDI-family request; 0 when the opcode is an AES one. */
int gemd_dispatch_vdi(gemd_session_t *session, const gem_rpc_header_t *header,
                      const uint8_t *payload, uint8_t *response,
                      uint32_t *response_size, int32_t *status_out);
/* Run a raster request clipped to the session's visible areas. */
int32_t gemd_draw_owned(gemd_session_t *session, const gem_rpc_header_t *header,
                        const uint8_t *payload, uint8_t *response,
                        uint32_t *response_size);
/* Execute any validated, authorized request; returns its status. */
int32_t gemd_dispatch(gemd_session_t *session, const gem_rpc_header_t *header,
                      const uint8_t *payload, uint8_t *response,
                      uint32_t *response_size);
/* Validate, authorize, execute and queue the reply for a ready frame. */
int gemd_handle_request(gemd_session_t *session);
/* Service other connections while a modal panel waits; 0 to cancel. */
int gemd_service_modal(void);

#endif
