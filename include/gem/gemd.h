/*
 * Defines the shared AES, VDI, bitmap and object-tree transport used by
 * libgem and gemd. Packet limits, operation identifiers and validation
 * helpers live here; copied payloads do not transmit application pointers.
 * The marked AES and VDI sections are maintained by their generators in
 * src/gem/. Edit the corresponding schema and regenerate those sections.
 *
 * MIT License (see: LICENSE)
 * Copyright (C) 2026 tomaz stih
 */
#ifndef GEM_GEMD_H
#define GEM_GEMD_H

#include <gem/aes.h>
#include <gem/vdi.h>
#include <stddef.h>
#include <stdint.h>

/* BEGIN GENERATED AES RPC */
enum { GEM_AES_WORDS = 2048, GEM_AES_ARGS = 12 };
typedef struct gem_aes_packet {
    WORD handle;
    uint16_t function;
    WORD args[GEM_AES_ARGS];
    uint16_t counts[GEM_AES_ARGS];
    WORD data[GEM_AES_WORDS];
} gem_aes_packet_t;
enum gem_aes_function {
    rpc_appl_find = 1,
    rpc_appl_bvset = 2,
    rpc_appl_write = 3,
    rpc_appl_read = 4,
    rpc_appl_tplay = 5,
    rpc_appl_trecord = 6,
    rpc_appl_yield = 7,
    rpc_evnt_dclick = 8,
    rpc_menu_register = 9,
    rpc_menu_unregister = 10,
    rpc_form_dial = 11,
    rpc_form_error = 12,
    rpc_graf_rubbox = 13,
    rpc_graf_dragbox = 14,
    rpc_graf_mbox = 15,
    rpc_graf_growbox = 16,
    rpc_graf_shrinkbox = 17,
    rpc_scrp_clear = 18,
    rpc_shel_read = 19,
    rpc_shel_write = 20,
    rpc_shel_get = 21,
    rpc_shel_put = 22,
    rpc_shel_rdef = 23,
    rpc_shel_wdef = 24,
};
/* Validate sizes, array counts and terminated strings before dispatch. */
int gem_aes_validate(const gem_aes_packet_t *packet);
/* Execute a validated packet against the server workstation; copy outputs. */
WORD gem_aes_dispatch(gem_aes_packet_t *packet);
/* Return nonzero for operations that draw into the screen. */
int gem_aes_draws(uint16_t function);
/* END GENERATED AES RPC */

/* BEGIN GENERATED VDI RPC */
enum { GEM_VDI_WORDS = 2048, GEM_VDI_ARGS = 12 };
typedef struct gem_vdi_packet {
    WORD handle;
    uint16_t function;
    WORD args[GEM_VDI_ARGS];
    uint16_t counts[GEM_VDI_ARGS];
    WORD data[GEM_VDI_WORDS];
} gem_vdi_packet_t;
enum gem_vdi_function {
    rpc_v_pmarker = 1,
    rpc_v_cellarray = 2,
    rpc_v_arc = 3,
    rpc_v_pieslice = 4,
    rpc_v_pie = 5,
    rpc_v_circle = 6,
    rpc_v_ellipse = 7,
    rpc_v_ellarc = 8,
    rpc_v_ellpie = 9,
    rpc_v_rfbox = 10,
    rpc_v_justified = 11,
    rpc_vst_height = 12,
    rpc_vst_rotation = 13,
    rpc_vs_color = 14,
    rpc_vsm_type = 15,
    rpc_vsm_height = 16,
    rpc_vsm_color = 17,
    rpc_vq_color = 18,
    rpc_vq_cellarray = 19,
    rpc_vrq_locator = 20,
    rpc_vsm_locator = 21,
    rpc_vrq_valuator = 22,
    rpc_vsm_valuator = 23,
    rpc_vrq_choice = 24,
    rpc_vsm_choice = 25,
    rpc_vsin_mode = 26,
    rpc_vql_attributes = 27,
    rpc_vqm_attributes = 28,
    rpc_vqf_attributes = 29,
    rpc_vqt_attributes = 30,
    rpc_vst_alignment = 31,
    rpc_vq_extnd = 32,
    rpc_v_contourfill = 33,
    rpc_vst_background = 34,
    rpc_vst_effects = 35,
    rpc_vst_point = 36,
    rpc_vsl_ends = 37,
    rpc_vsl_end_style = 38,
    rpc_vsc_form = 39,
    rpc_vsf_udpat = 40,
    rpc_vsl_udsty = 41,
    rpc_vqin_mode = 42,
    rpc_vqt_width = 43,
    rpc_vst_load_fonts = 44,
    rpc_vst_unload_fonts = 45,
    rpc_vq_mouse = 46,
    rpc_vq_key_s = 47,
    rpc_vqt_name = 48,
    rpc_vq_chcells = 49,
    rpc_v_exit_cur = 50,
    rpc_v_enter_cur = 51,
    rpc_v_curup = 52,
    rpc_v_curdown = 53,
    rpc_v_curright = 54,
    rpc_v_curleft = 55,
    rpc_v_curhome = 56,
    rpc_v_eeos = 57,
    rpc_v_eeol = 58,
    rpc_vs_curaddress = 59,
    rpc_v_curtext = 60,
    rpc_v_rvon = 61,
    rpc_v_rvoff = 62,
    rpc_vq_curaddress = 63,
    rpc_vq_tabstatus = 64,
    rpc_v_hardcopy = 65,
    rpc_v_dspcur = 66,
    rpc_v_rmcur = 67,
    rpc_v_form_adv = 68,
    rpc_v_output_window = 69,
    rpc_v_clear_disp_list = 70,
    rpc_v_bit_image = 71,
    rpc_vs_palette = 72,
    rpc_v_meta_extents = 73,
    rpc_v_write_meta = 74,
    rpc_vm_filename = 75,
    rpc_v_get_pixel = 76,
    rpc_v_escape = 77,
};
/* Validate sizes, array counts and terminated strings before dispatch. */
int gem_vdi_validate(const gem_vdi_packet_t *packet);
/* Execute a validated packet against the server workstation; copy outputs. */
WORD gem_vdi_dispatch(gem_vdi_packet_t *packet);
/* Return nonzero for operations that draw into the screen. */
int gem_vdi_draws(uint16_t function);
/* END GENERATED VDI RPC */

/* Connection-owned bitmap storage and bounded transfers. */
#define GEM_BITMAP_LIMIT (8u * 1024u * 1024u)
#define GEM_BITMAP_CHUNK 4096u
typedef struct gem_bitmap_chunk {
    WORD handle;
    uint16_t slot;
    uint32_t total, offset, length;
    uint8_t data[GEM_BITMAP_CHUNK];
} gem_bitmap_chunk_t;
typedef struct gem_bitmap_form {
    WORD width, height, stride, standard, planes, memory;
} gem_bitmap_form_t;
typedef struct gem_bitmap_call {
    WORD handle, operation, mode, alias;
    WORD xy[8], colors[2];
    gem_bitmap_form_t source, destination;
} gem_bitmap_call_t;
typedef struct gem_bitmap_store {
    uint8_t *bytes[2];
    size_t size[2];
} gem_bitmap_store_t;
/* Compute validated mono bitmap storage, or zero for invalid geometry. */
size_t gem_bitmap_size(const gem_bitmap_form_t *form);
/* Release all connection-owned bitmap storage. */
void gem_bitmap_free(gem_bitmap_store_t *store);
/* Upload or download a checked chunk; return nonzero on success. */
WORD gem_bitmap_transfer(gem_bitmap_store_t *store, gem_bitmap_chunk_t *chunk,
                         int download);
/* Execute an MFDB operation using this connection's uploaded storage. */
WORD gem_bitmap_execute(gem_bitmap_store_t *store,
                        const gem_bitmap_call_t *call);

/* Copied AES trees with validated offsets into a bounded byte arena. */
#define GEM_TREE_OBJECTS 128
#define GEM_TREE_BYTES 49152
typedef struct gem_tree_packet {
    uint16_t operation, count;
    uint32_t used;
    uint64_t identity;
    WORD args[12];
    OBJECT objects[GEM_TREE_OBJECTS];
    _Alignas(max_align_t) unsigned char data[GEM_TREE_BYTES];
} gem_tree_packet_t;
enum gem_tree_operation {
    tree_draw,
    tree_edit,
    tree_form_do,
    tree_form_center,
    tree_form_keybd,
    tree_form_button,
    tree_watchbox,
    tree_slidebox
};
/* Copy a client tree and referenced data into a pointer-free packet. */
int gem_tree_pack(gem_tree_packet_t *packet, const OBJECT *tree);
/* Validate graph, offsets and nested data; optionally relocate pointers. */
int gem_tree_decode(gem_tree_packet_t *packet, int relocate);
/* Copy mutable fields and text back without overwriting client pointers. */
void gem_tree_copy_back(OBJECT *tree, const gem_tree_packet_t *packet);
/* Exchange one copied tree operation and update client-owned output fields. */
WORD gem_client_tree(OBJECT *tree, uint16_t operation, WORD args[12]);
/* Run a USERDEF callback in the process that owns its function pointer. */
LONG gem_client_user_callback(PARMBLK *parm);
/* Restore original offsets after server-side execution. */
void gem_tree_encode(gem_tree_packet_t *packet);

#endif /* GEM_GEMD_H */
