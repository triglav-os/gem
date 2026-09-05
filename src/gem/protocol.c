/*
 * Validates the private hosted RPC envelope and menu pointer reconstruction.
 * No AES/VDI declarations or application-visible structures are changed.
 *
 * MIT License (see: LICENSE)
 * Copyright (C) 2026 tomaz stih
 */

#include "_gem.h"

#include <stdlib.h>

/* A GEM tree has one parent per object; sibling tails point back to it.
 * Validate without recursion before AES walks any attacker-supplied links. */
static int valid_menu_tree(const gem_rpc_menu_bar_req_t *req)
{
    unsigned char seen[GEM_RPC_MENU_MAX_OBJECTS] = {1};
    WORD queue[GEM_RPC_MENU_MAX_OBJECTS] = {ROOT};
    WORD first = 0, used = 1;
    if (req->objects[ROOT].ob_next != NIL) return 0;
    while (first < used) {
        WORD parent = queue[first++];
        const OBJECT *obj = &req->objects[parent];
        WORD child = obj->ob_head;
        if ((child == NIL) != (obj->ob_tail == NIL)) return 0;
        if (child == NIL) continue;
        for (;;) {
            if (child < 0 || child >= req->object_count || seen[child]) return 0;
            seen[child] = 1;
            queue[used++] = child;
            if (child == obj->ob_tail) {
                if (req->objects[child].ob_next != parent) return 0;
                break;
            }
            child = req->objects[child].ob_next;
        }
    }
    return used == req->object_count;
}

const char *gem_rpc_socket_path(void)
{
    const char *path = getenv("GEMD_SOCKET");
    return path != NULL && path[0] != '\0' ? path : GEMD_SOCKET_PATH;
}

int gem_rpc_valid_request(uint16_t opcode, const void *payload, uint32_t size)
{
    size_t expected;

#define REQUEST(name, type) case GEM_RPC_##name: expected = sizeof(type); break
    switch (opcode) {
    case GEM_RPC_APPL_INIT:
    case GEM_RPC_APPL_EXIT:
    case GEM_RPC_EVNT_MESAG:
    case GEM_RPC_GRAF_HANDLE:
    case GEM_RPC_GRAF_MKSTATE:
    case GEM_RPC_SCRP_READ: expected = 0; break;
    REQUEST(EVNT_MULTI, gem_rpc_evnt_multi_req_t);
    REQUEST(GRAF_MOUSE, gem_rpc_graf_mouse_req_t);
    REQUEST(FORM_ALERT, gem_rpc_form_alert_req_t);
    REQUEST(V_OPNVWK, gem_rpc_opnvwk_req_t);
    case GEM_RPC_V_CLSVWK:
    case GEM_RPC_V_CLRWK:
    case GEM_RPC_V_UPDWK:
    case GEM_RPC_WIND_CLOSE:
    case GEM_RPC_WIND_DELETE:
    case GEM_RPC_V_HIDE_C:
    case GEM_RPC_VQT_FONTINFO: expected = sizeof(gem_rpc_handle_req_t); break;
    case GEM_RPC_VSF_COLOR:
    case GEM_RPC_VST_COLOR:
    case GEM_RPC_VSL_COLOR: expected = sizeof(gem_rpc_color_req_t); break;
    case GEM_RPC_VSL_TYPE:
    case GEM_RPC_VSL_WIDTH:
    case GEM_RPC_VSF_INTERIOR:
    case GEM_RPC_VSF_STYLE:
    case GEM_RPC_VSF_PERIMETER:
    case GEM_RPC_VSWR_MODE:
    case GEM_RPC_VST_FONT: expected = sizeof(gem_rpc_handle_word_req_t); break;
    REQUEST(VQT_EXTENT, gem_rpc_vqt_extent_req_t);
    REQUEST(VS_CLIP, gem_rpc_clip_req_t);
    case GEM_RPC_V_PLINE:
    case GEM_RPC_V_FILLAREA: expected = sizeof(gem_rpc_pline_req_t); break;
    case GEM_RPC_V_BAR:
    case GEM_RPC_VR_RECFL: expected = sizeof(gem_rpc_rect_req_t); break;
    REQUEST(V_GTEXT, gem_rpc_gtext_req_t);
    REQUEST(WIND_CREATE, gem_rpc_wind_create_req_t);
    REQUEST(WIND_OPEN, gem_rpc_wind_open_req_t);
    REQUEST(WIND_GET, gem_rpc_wind_get_req_t);
    REQUEST(WIND_SET, gem_rpc_wind_set_req_t);
    REQUEST(WIND_SET_STR, gem_rpc_wind_set_str_req_t);
    REQUEST(WIND_FIND, gem_rpc_wind_find_req_t);
    REQUEST(WIND_UPDATE, gem_rpc_wind_update_req_t);
    REQUEST(WIND_CALC, gem_rpc_wind_calc_req_t);
    REQUEST(MENU_BAR, gem_rpc_menu_bar_req_t);
    REQUEST(MENU_TNORMAL, gem_rpc_menu_tnormal_req_t);
    REQUEST(MENU_CLICK, gem_rpc_menu_click_req_t);
    REQUEST(SCRP_WRITE, gem_rpc_path_t);
    REQUEST(FSEL_INPUT, gem_rpc_fsel_t);
    REQUEST(V_SHOW_C, gem_rpc_handle_word_req_t);
    default: return 0;
    }
#undef REQUEST
    if (size != expected || (size && payload == NULL)) return 0;
    /* String-valued wind_set uses process pointers; only the copied-string
     * opcode is meaningful across this address-space boundary. */
    if (opcode == GEM_RPC_WIND_SET) {
        const gem_rpc_wind_set_req_t *req = payload;
        if (req->field == WF_NAME || req->field == WF_INFO) return 0;
    }
    if (opcode == GEM_RPC_V_PLINE || opcode == GEM_RPC_V_FILLAREA) {
        const gem_rpc_pline_req_t *req = payload;
        if (req->count < 0 || req->count > 128) return 0;
    }
    if (opcode == GEM_RPC_MENU_BAR) {
        const gem_rpc_menu_bar_req_t *req = payload;
        WORD i;
        unsigned char strings[GEM_RPC_MENU_MAX_OBJECTS] = {0};

        if (req->show == 0) return 1;
        if (req->object_count <= 0 ||
            req->object_count > (WORD) GEM_RPC_MENU_MAX_OBJECTS ||
            req->string_count < 0 ||
            req->string_count > (WORD) GEM_RPC_MENU_MAX_STRINGS) return 0;
        for (i = 0; i < req->string_count; ++i) {
            WORD object = req->strings[i].object;
            if (object < 0 || object >= req->object_count || strings[object])
                return 0;
            strings[object] = 1;
        }
        for (i = 0; i < req->object_count; ++i) {
            const OBJECT *obj = &req->objects[i];
            UWORD type = obj->ob_type & 0xffu;
            if (obj->ob_next < NIL || obj->ob_next >= req->object_count ||
                obj->ob_head < NIL || obj->ob_head >= req->object_count ||
                obj->ob_tail < NIL || obj->ob_tail >= req->object_count ||
                (obj->ob_flags & INDIRECT) != 0) return 0;
            if (type == G_TITLE || type == G_STRING) {
                if (!strings[i]) return 0;
            } else if (type != G_BOX && type != G_IBOX && type != G_BOXCHAR) {
                return 0;
            } else if (strings[i]) {
                return 0;
            }
            if ((i == req->object_count - 1) != ((obj->ob_flags & LASTOB) != 0))
                return 0;
        }
        return valid_menu_tree(req);
    }
    return 1;
}
