/*
 * Declares the private hosted AES runtime state and helper routines
 * shared by the public AES entry points and the internal utility layer.
 *
 * MIT License (see: LICENSE)
 * Copyright (C) 2026 tomaz stih
 */

#ifndef GEM_SRC_AES__AES_H
#define GEM_SRC_AES__AES_H

#include "gem/aes.h"
#include "gem/vdi.h"

#include "platform/hid.h"

#include <stddef.h>

enum {
    AES_MAX_APPS = 16,
    AES_MAX_MESSAGES = 16,
    AES_MAX_WINDOWS = 16,
    AES_MAX_MENU_SAVED_REGIONS = 2,
    AES_PATH_LEN = 260,
    AES_SHELL_BUF_LEN = 1024,
    AES_DECOR = 8,
    AES_CHAR_WIDTH = 6,
    AES_CHAR_HEIGHT = 8
};

typedef struct aes_message {
    WORD used;
    WORD dest;
    WORD data[8];
} aes_message_t;

typedef struct aes_app {
    WORD used;
    WORD id;
    char name[9];
} aes_app_t;

typedef struct aes_window {
    WORD used;
    WORD handle;
    WORD owner;
    WORD open;
    WORD iconified;
    uint32_t z_order;
    UWORD kind;
    GRECT outer;
    GRECT work;
    GRECT previous_outer;
    GRECT restored_outer;
    char name[128];
    char info[128];
} aes_window_t;

typedef WORD (*aes_user_draw_t)(LONG parm_block);

typedef struct aes_state {
    WORD initialized;
    WORD next_app_id;
    WORD current_app_id;
    uint32_t next_window_z;
    WORD dclick_rate;
    WORD menu_click;
    WORD update_depth;
    WORD vdi_ready;
    VDI_HANDLE vdi_handle;
    WORD work_in[11];
    WORD work_out[57];
    OBJECT *menu_tree;
    WORD menu_visible;
    WORD menu_popup_root_direct;
    OBJECT *edit_tree;
    WORD edit_object;
    WORD edit_index;
    WORD menu_saved_count;
    GRECT menu_saved_rects[AES_MAX_MENU_SAVED_REGIONS];
    uint8_t *menu_saved_pixels[AES_MAX_MENU_SAVED_REGIONS];
    char scrap_path[AES_PATH_LEN];
    char shell_cmd[AES_PATH_LEN];
    char shell_tail[AES_PATH_LEN];
    char shell_dir[AES_PATH_LEN];
    char shell_buf[AES_SHELL_BUF_LEN];
    WORD shell_buf_len;
    void *resource_data;
    size_t resource_size;
    WORD resource_is_builtin;
    aes_app_t apps[AES_MAX_APPS];
    aes_message_t messages[AES_MAX_MESSAGES];
    aes_window_t windows[AES_MAX_WINDOWS];
    WORD desktop_bvdisk;
    WORD desktop_bvhard;
    WORD enum_handle;
    WORD enum_stage;
    WORD enum_count;
    WORD enum_index;
    GRECT enum_rects[64];
} aes_state_t;

extern aes_state_t _aes;
extern WORD global[15];

enum {
    AES_WINDOW_PART_NONE = 0,
    AES_WINDOW_PART_WORK = 1,
    AES_WINDOW_PART_TITLE = 2,
    AES_WINDOW_PART_CLOSER = 3,
    AES_WINDOW_PART_FULLER = 4,
    AES_WINDOW_PART_SIZER = 5
};

/*
 * Starts a deferred VDI update batch so multiple AES drawing calls can
 * be presented as one frame.
 */
void _vdi_begin_update(void);

/*
 * Ends a deferred VDI update batch and flushes any pending frame.
 */
void _vdi_end_update(void);

/*
 * Presents the current VDI screen contents immediately.
 */
void _vdi_present_screen(void);

/*
 * Updates the shared VDI mouse state from a hosted input event.
 */
void _vdi_set_mouse_state(WORD x, WORD y, WORD status);

/*
 * Returns the shared hosted AES chrome height used by menu bars,
 * window titles, and standard dialog buttons.
 */
WORD _aes_chrome_height(void);

/*
 * Returns the hosted top menu-bar height, including the white top row
 * and the bottom separator rule used by classic monochrome GEM.
 */
WORD _aes_menu_chrome_height(void);

/*
 * Returns the pixel width of one C string in the active VDI font.
 */
WORD _vdi_string_width(const char *string);

/*
 * Returns the ascent of the active VDI font in pixels.
 */
WORD _vdi_font_ascent(void);

/*
 * Returns the full cell height of the active VDI font in pixels.
 */
WORD _vdi_font_text_height(void);

/*
 * Returns the active VDI write mode.
 */
WORD _vdi_write_mode(void);

/*
 * Restores any saved software cursor background before direct writes.
 */
void _vdi_prepare_screen_write(void);

/*
 * Plots one pixel through the active VDI clipping and write mode path.
 */
void _vdi_plot_pixel(WORD x, WORD y, WORD color);

int gem_builtin_rsrc_load(const char *filename);
int gem_builtin_rsrc_gaddr(WORD type, WORD index, void **addr);
void gem_builtin_rsrc_free(void);

void _aes_trace(const char *fmt, ...);
WORD _aes_menu_bar_height(void);
void _aes_store_mouse_state(const gem_hid_event_t *evt);
void _aes_menu_prepare_tree(OBJECT *tree);
void _aes_menu_hide_popups(OBJECT *tree);
void _aes_menu_redraw_tree(OBJECT *tree);
void _aes_menu_clear_saved_region(void);
WORD _aes_menu_event(OBJECT *tree,
                     const gem_hid_event_t *first_evt,
                     WORD mepbuff[8]);
WORD _aes_min_word(WORD left, WORD right);
WORD _aes_max_word(WORD left, WORD right);
void _aes_set_rect(GRECT *rect, WORD x, WORD y, WORD w, WORD h);
int _aes_point_in_rect(WORD x, WORD y, const GRECT *rect);
void _aes_reset_state(void);
int _aes_ensure_vdi(void);
aes_app_t *_aes_find_app_by_id(WORD id);
aes_window_t *_aes_find_window(WORD handle);
void _aes_compute_work(aes_window_t *window);
WORD _aes_wind_set_text(WORD handle, WORD field, const char *text);
void _aes_draw_window_frame(const aes_window_t *window);
void _aes_clear_window_frame(const aes_window_t *window);
void _aes_redraw_open_windows(void);
void _aes_redraw_region(const GRECT *dirty);
void _aes_redraw_window_change(const GRECT *before, const GRECT *after);
void _aes_redraw_window_title_states(const aes_window_t *previous_top,
                                     const aes_window_t *new_top);
void _aes_raise_window(aes_window_t *window);
WORD _aes_window_hit_part(const aes_window_t *window, WORD x, WORD y);
void _aes_object_extent(OBJECT *tree, WORD object, WORD *x, WORD *y);
void _aes_draw_tree_recursive(const OBJECT *tree,
                              WORD object,
                              WORD parent_x,
                              WORD parent_y,
                              WORD depth,
                              WORD clip[4]);
WORD _aes_find_in_subtree(OBJECT *tree,
                          WORD object,
                          WORD parent_x,
                          WORD parent_y,
                          WORD depth,
                          WORD mx,
                          WORD my);
int _aes_load_file(const char *filename, void **data_out, size_t *size_out);
int _aes_try_resolve_path(const char *filename,
                          char *resolved,
                          size_t resolved_size);

#endif
