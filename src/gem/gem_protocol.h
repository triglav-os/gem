/*
 * Declares the private GEM client RPC helpers shared by the libgem
 * wrappers. This layer marshals a practical subset of AES and VDI
 * calls over a Unix-domain socket to the hosted GEM server.
 *
 * MIT License (see: LICENSE)
 * Copyright (C) 2026 tomaz stih
 */

#ifndef GEM_PROTOCOL_H
#define GEM_PROTOCOL_H

#include "gem/gem.h"

#include <stddef.h>
#include <stdint.h>

#define GEMD_SOCKET_PATH "/tmp/gemd.sock"
#define GEM_RPC_MAGIC 0x47454d31u
#define GEM_RPC_VERSION 2u
#define GEM_RPC_TEXT_MAX 512u
#define GEM_RPC_MENU_MAX_OBJECTS 64u
#define GEM_RPC_MENU_MAX_STRINGS 32u
#define GEM_RPC_MENU_STRING_MAX 64u
#define GEM_RPC_PAYLOAD_MAX 65536u
#define GEM_RPC_LOCATION_MAX 32u
#define GEM_RPC_LOCATION_PATH_MAX 512u
#define GEM_RPC_LOCATION_LABEL_MAX 64u
#define GEM_MSG_LOCATION_CHANGED 0x7f10

typedef enum gem_rpc_opcode {
    GEM_RPC_APPL_INIT = 1,
    GEM_RPC_APPL_EXIT,
    GEM_RPC_EVNT_MESAG,
    GEM_RPC_EVNT_MULTI,
    GEM_RPC_GRAF_HANDLE,
    GEM_RPC_GRAF_MOUSE,
    GEM_RPC_FORM_ALERT,
    GEM_RPC_V_OPNVWK,
    GEM_RPC_V_CLSVWK,
    GEM_RPC_V_CLRWK,
    GEM_RPC_V_UPDWK,
    GEM_RPC_VSF_COLOR,
    GEM_RPC_VST_COLOR,
    GEM_RPC_VSL_TYPE,
    GEM_RPC_VSL_WIDTH,
    GEM_RPC_VSL_COLOR,
    GEM_RPC_VSF_INTERIOR,
    GEM_RPC_VSF_STYLE,
    GEM_RPC_VSF_PERIMETER,
    GEM_RPC_VSWR_MODE,
    GEM_RPC_VST_FONT,
    GEM_RPC_VQT_EXTENT,
    GEM_RPC_VS_CLIP,
    GEM_RPC_V_PLINE,
    GEM_RPC_V_FILLAREA,
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
    GEM_RPC_WIND_CALC,
    GEM_RPC_MENU_BAR,
    GEM_RPC_MENU_TNORMAL,
    GEM_RPC_MENU_CLICK,
    GEM_RPC_GRAF_MKSTATE,
    GEM_RPC_VQT_FONTINFO,
    GEM_RPC_SCRP_READ,
    GEM_RPC_SCRP_WRITE,
    GEM_RPC_FSEL_INPUT,
    GEM_RPC_V_HIDE_C,
    GEM_RPC_V_SHOW_C,
    GEM_RPC_VDI_EXT,
    GEM_RPC_VDI_STANDALONE,
    GEM_RPC_BITMAP_PUT,
    GEM_RPC_BITMAP_GET,
    GEM_RPC_BITMAP_COPY,
    GEM_RPC_AES_TREE,
    GEM_RPC_AES_EXT,
    GEM_RPC_CALLBACK_DONE,
    GEM_RPC_LOCATION_LIST,
    GEM_RPC_LOCATION_ADD,
    GEM_RPC_LOCATION_REMOVE,
    GEM_RPC_LOCATION_SELECT,
    GEM_RPC_OVERLAY_CREATE,
    GEM_RPC_OVERLAY_RESIZE,
    GEM_RPC_OVERLAY_MAP,
    GEM_RPC_OVERLAY_UNMAP,
    GEM_RPC_OVERLAY_DESTROY,
    GEM_RPC_OVERLAY_UPDATE,
    GEM_RPC_FSEL_OPTIONS
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

typedef struct gem_rpc_handle_word_req {
    WORD handle;
    WORD value;
} gem_rpc_handle_word_req_t;

typedef struct gem_rpc_evnt_multi_req {
    UWORD flags;
    UWORD bclk;
    UWORD bmsk;
    UWORD bst;
    UWORD m1flags;
    WORD m1x;
    WORD m1y;
    WORD m1w;
    WORD m1h;
    UWORD m2flags;
    WORD m2x;
    WORD m2y;
    WORD m2w;
    WORD m2h;
    UWORD tlc;
    UWORD thc;
} gem_rpc_evnt_multi_req_t;

typedef struct gem_rpc_evnt_multi_rsp {
    WORD event;
    WORD mx;
    WORD my;
    WORD mb;
    WORD ks;
    WORD kr;
    WORD br;
    WORD msg[8];
} gem_rpc_evnt_multi_rsp_t;

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

/* Path buffers follow the hosted AES implementation's 1024-byte contract. */
typedef struct gem_rpc_path {
    char text[1024];
} gem_rpc_path_t;

typedef struct gem_rpc_fsel {
    char path[1024];
    char name[1024];
    char title[64];
    WORD button;
    WORD mode;
} gem_rpc_fsel_t;

typedef struct gem_rpc_fsel_options_req {
    UWORD options;
    WORD setit;
} gem_rpc_fsel_options_req_t;

enum gem_rpc_fsel_mode {
    GEM_RPC_FSEL_OPEN = 0,
    GEM_RPC_FSEL_SAVE = 1,
    GEM_RPC_FSEL_DIRECTORY = 2,
    GEM_RPC_FSEL_LOCATION = 3
};

typedef struct gem_rpc_location_entry {
    char path[GEM_RPC_LOCATION_PATH_MAX];
    char label[GEM_RPC_LOCATION_LABEL_MAX];
    uint16_t type;
    uint16_t reserved;
} gem_rpc_location_entry_t;

enum { GEM_RPC_LOCATION_FOLDER = 0, GEM_RPC_LOCATION_DISK_IMAGE = 1 };

#define GEM_RPC_DESKTOP_SHOW_HIDDEN 0x00000001u

typedef struct gem_rpc_location_list {
    uint32_t count;
    gem_rpc_location_entry_t entries[GEM_RPC_LOCATION_MAX];
} gem_rpc_location_list_t;

/* Keep the location-list wire size stable for a running mixed-version
 * desktop/gemd pair.  The final entry's previously reserved word carries
 * list-wide Desktop options, including when that entry is in use. */
#define GEM_RPC_LOCATION_LIST_OPTIONS(list) \
    ((list).entries[GEM_RPC_LOCATION_MAX - 1u].reserved)

typedef struct gem_rpc_location_req {
    char path[GEM_RPC_LOCATION_PATH_MAX];
} gem_rpc_location_req_t;

typedef struct gem_rpc_overlay_create_req {
    WORD window;
    uint16_t width;
    uint16_t height;
} gem_rpc_overlay_create_req_t;

typedef struct gem_rpc_overlay_id_req {
    uint32_t id;
} gem_rpc_overlay_id_req_t;

typedef struct gem_rpc_overlay_resize_req {
    uint32_t id;
    uint16_t width;
    uint16_t height;
} gem_rpc_overlay_resize_req_t;

typedef struct gem_rpc_overlay_map_req {
    uint32_t id;
    WORD window;
} gem_rpc_overlay_map_req_t;

typedef struct gem_rpc_overlay_update_req {
    uint32_t id;
    uint16_t x;
    uint16_t y;
    uint16_t width;
    uint16_t height;
    uint16_t pitch;
    uint16_t reserved;
    /* width * height BGRA bytes follow this header in the RPC payload. */
} gem_rpc_overlay_update_req_t;

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

typedef struct gem_rpc_form_alert_req {
    WORD defbut;
    char text[GEM_RPC_TEXT_MAX];
} gem_rpc_form_alert_req_t;

typedef struct gem_rpc_graf_mouse_req {
    WORD mode;
    WORD has_form;
    MFORM form;
} gem_rpc_graf_mouse_req_t;

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

typedef struct gem_rpc_vqt_extent_req {
    WORD handle;
    char text[GEM_RPC_TEXT_MAX];
} gem_rpc_vqt_extent_req_t;

typedef struct gem_rpc_vqt_extent_rsp {
    WORD extent[8];
} gem_rpc_vqt_extent_rsp_t;

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

/*
 * Carries one object's ob_spec string, keyed by its index in the tree
 * being sent. Only G_TITLE/G_STRING objects (plain pointer, not
 * INDIRECT) get an entry; box/container specs travel as raw bytes
 * inside the object array itself.
 */
typedef struct gem_rpc_menu_string {
    WORD object;
    char text[GEM_RPC_MENU_STRING_MAX];
} gem_rpc_menu_string_t;

typedef struct gem_rpc_menu_bar_req {
    WORD show;
    WORD object_count;
    WORD string_count;
    OBJECT objects[GEM_RPC_MENU_MAX_OBJECTS];
    gem_rpc_menu_string_t strings[GEM_RPC_MENU_MAX_STRINGS];
} gem_rpc_menu_bar_req_t;

typedef struct gem_rpc_menu_tnormal_req {
    WORD title;
    WORD normal;
} gem_rpc_menu_tnormal_req_t;

typedef struct gem_rpc_menu_click_req {
    WORD click;
    WORD setit;
} gem_rpc_menu_click_req_t;

int gem_rpc_call(gem_rpc_opcode_t opcode, const void *request,
                 uint32_t request_size, int32_t *status, void *response,
                 uint32_t response_size);
int gem_location_list(gem_rpc_location_list_t *locations);
int gem_location_add(const char *path);
int gem_location_remove(const char *path);
int gem_location_select(const char *path);

/* Private transport configuration; defaults to the historical socket path. */
const char *gem_rpc_socket_path(void);

/* Reject incomplete, unknown or structurally unsafe requests before dispatch.
 */
int gem_rpc_valid_request(uint16_t opcode, const void *payload, uint32_t size);

/* Republish mutations only when tree is the active client menu. */
WORD gem_client_menu_changed(OBJECT *tree);

#endif
