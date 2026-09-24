/*
 * Executes the AES family of gemd requests: application lifetime, events,
 * windows, menus, standard panels, extended AES packets and copied object
 * trees, whose USERDEF callbacks round-trip to the owning client.
 *
 * MIT License (see: LICENSE)
 * Copyright (C) 2026 tomaz stih
 */

#include "session.h"
#include "overlay.h"

#include <poll.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <sys/socket.h>

static char g_gemd_selected_location[GEM_RPC_LOCATION_PATH_MAX];

static void gemd_location_label(const char *path, char *label,
                                size_t label_size)
{
    const char *leaf = strrchr(path, '/');

    if (strcmp(path, "/") == 0) {
        strncpy(label, "Root", label_size - 1u);
    } else if (leaf != NULL && leaf[1] != '\0') {
        strncpy(label, leaf + 1, label_size - 1u);
    } else {
        strncpy(label, path, label_size - 1u);
    }
    label[label_size - 1u] = '\0';
}

static int gemd_location_valid_directory(const char *path)
{
    gem_os_file_info_t info;

    return path != NULL && path[0] == '/' && gem_os_stat_path(path, &info) != 0 &&
           info.is_directory != 0;
}

static int gemd_location_is_image(const char *path)
{
    const char *dot = path == NULL ? NULL : strrchr(path, '.');
    gem_os_file_info_t info;

    return dot != NULL && gem_os_stat_path(path, &info) != 0 &&
           info.is_directory == 0 &&
           (strcasecmp(dot, ".st") == 0 || strcasecmp(dot, ".msa") == 0);
}

static int gemd_location_list(void *response, uint32_t *response_size)
{
    gem_rpc_location_list_t *list = response;
    size_t i;

    memset(list, 0, sizeof(*list));
    if (gemconf_get_bool(&g_gemd_config, "desktop", "show_hidden", 0) != 0) {
        GEM_RPC_LOCATION_LIST_OPTIONS(*list) |= GEM_RPC_DESKTOP_SHOW_HIDDEN;
    }
    for (i = 0; i < gemconf_count(&g_gemd_config) &&
                list->count < GEM_RPC_LOCATION_MAX;
         ++i) {
        const char *section = gemconf_entry_section(&g_gemd_config, i);
        const char *path = gemconf_entry_key(&g_gemd_config, i);
        const char *label = gemconf_entry_value(&g_gemd_config, i);
        gem_rpc_location_entry_t *entry = &list->entries[list->count];

        if (section == NULL || strcmp(section, "locations") != 0 ||
            (!gemd_location_valid_directory(path) &&
             !gemd_location_is_image(path))) {
            continue;
        }
        strncpy(entry->path, path, sizeof(entry->path) - 1u);
        strncpy(entry->label, label != NULL ? label : "",
                sizeof(entry->label) - 1u);
        if (entry->label[0] == '\0') {
            gemd_location_label(entry->path, entry->label,
                                sizeof(entry->label));
        }
        entry->type = gemd_location_is_image(path)
                          ? GEM_RPC_LOCATION_DISK_IMAGE
                          : GEM_RPC_LOCATION_FOLDER;
        ++list->count;
    }
    *response_size = sizeof(*list);
    return 1;
}

static int gemd_location_add(const gem_rpc_location_req_t *req)
{
    char label[GEM_RPC_LOCATION_LABEL_MAX];

    if (!gemd_location_valid_directory(req->path) &&
        !gemd_location_is_image(req->path)) {
        return -1;
    }
    gemd_location_label(req->path, label, sizeof(label));
    if (gemconf_set(&g_gemd_config, "locations", req->path, label) == 0 ||
        gemconf_save(&g_gemd_config) == 0) {
        return -1;
    }
    gemd_desktop_config_saved();
    return 1;
}

static int gemd_location_remove(const gem_rpc_location_req_t *req)
{
    char label[GEM_RPC_LOCATION_LABEL_MAX];
    char alert[GEM_RPC_TEXT_MAX];

    if (gemconf_get(&g_gemd_config, "locations", req->path, NULL) == NULL) {
        return -1;
    }
    gemd_location_label(req->path, label, sizeof(label));
    (void)snprintf(alert, sizeof(alert),
                   "[2][ Are you sure you want to remove \"%s\"? ]["
                   " Remove | Cancel ]",
                   label);
    if (form_alert(2, alert) != 1) {
        return 2;
    }
    if (gemconf_remove(&g_gemd_config, "locations", req->path) == 0 ||
        gemconf_save(&g_gemd_config) == 0) {
        return -1;
    }
    gemd_desktop_config_saved();
    return 1;
}

int gemd_location_delete_event(void)
{
    gem_rpc_location_req_t req;

    if (g_gemd_selected_location[0] == '\0') {
        return 0;
    }
    memset(&req, 0, sizeof(req));
    strncpy(req.path, g_gemd_selected_location, sizeof(req.path) - 1u);
    (void)gemd_location_remove(&req);
    return 1;
}

void gemd_location_clear_selection(void)
{
    g_gemd_selected_location[0] = '\0';
}

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

int32_t gemd_dispatch(gemd_session_t *session, const gem_rpc_header_t *header,
                      const uint8_t *payload, uint8_t *response,
                      uint32_t *response_size)
{
    int32_t status = 0;

    *response_size = 0u;
    gemd_set_current_app(session);

    if (header->opcode >= GEM_RPC_OVERLAY_CREATE &&
        header->opcode <= GEM_RPC_OVERLAY_UPDATE)
        return gemd_overlay_dispatch(session, header, payload, response,
                                     response_size);

    if (gemd_dispatch_vdi(session, header, payload, response, response_size,
                          &status)) {
        return status;
    }

    switch ((gem_rpc_opcode_t)header->opcode) {
        case GEM_RPC_AES_EXT: {
            gem_aes_packet_t *packet = (gem_aes_packet_t *)response;
            memcpy(packet, payload, sizeof(*packet));
            *response_size = sizeof(*packet);
            if ((packet->function == rpc_appl_read ||
                 packet->function == rpc_menu_register) &&
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

        case GEM_RPC_GRAF_MKSTATE: {
            gem_rpc_words8_t *rsp = (gem_rpc_words8_t *)response;
            memset(rsp, 0, sizeof(*rsp));
            if (aes_wait_hook == gemd_service_modal &&
                session != g_modal_session) {
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
            rsp->title[sizeof(rsp->title) - 1] = '\0';
            if (rsp->mode == GEM_RPC_FSEL_DIRECTORY) {
                status = fsel_directory(rsp->path, &rsp->button);
            } else if (rsp->mode == GEM_RPC_FSEL_LOCATION) {
                status = fsel_location(rsp->path, rsp->name, &rsp->button);
            } else if (rsp->mode == GEM_RPC_FSEL_SAVE) {
                status = fsel_save(rsp->path, rsp->name, &rsp->button);
            } else {
                status = fsel_exinput(rsp->path, rsp->name, &rsp->button,
                                      rsp->title);
            }
            *response_size = sizeof(*rsp);
        } break;

        case GEM_RPC_FSEL_OPTIONS: {
            const gem_rpc_fsel_options_req_t *req =
                (const gem_rpc_fsel_options_req_t *)payload;

            status = fsel_options(req->options, req->setit);
        } break;

        case GEM_RPC_LOCATION_LIST:
            status = gemd_location_list(response, response_size);
            break;

        case GEM_RPC_LOCATION_ADD: {
            gem_rpc_location_req_t req;

            memcpy(&req, payload, sizeof(req));
            req.path[sizeof(req.path) - 1u] = '\0';
            status = gemd_location_add(&req);
        } break;

        case GEM_RPC_LOCATION_REMOVE: {
            gem_rpc_location_req_t req;

            memcpy(&req, payload, sizeof(req));
            req.path[sizeof(req.path) - 1u] = '\0';
            status = gemd_location_remove(&req);
        } break;

        case GEM_RPC_LOCATION_SELECT: {
            gem_rpc_location_req_t req;

            memcpy(&req, payload, sizeof(req));
            req.path[sizeof(req.path) - 1u] = '\0';
            g_gemd_selected_location[0] = '\0';
            if ((gemd_location_valid_directory(req.path) ||
                 gemd_location_is_image(req.path)) &&
                gemconf_get(&g_gemd_config, "locations", req.path, NULL) !=
                    NULL) {
                strncpy(g_gemd_selected_location, req.path,
                        sizeof(g_gemd_selected_location) - 1u);
            }
            status = 1;
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
            if (aes_wait_hook == gemd_service_modal &&
                session != g_modal_session) {
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
