/*
 * Declares the public GEM AES interface in a form that tracks the
 * historical programmer's guide and the imported GEM bindings. The
 * header includes the classic AES object and resource structures,
 * message and event constants, and the standard AES library calls.
 *
 * MIT License (see: LICENSE)
 * Copyright (C) 2026 tomaz stih
 */

#ifndef GEM_AES_H
#define GEM_AES_H

#include "gem/portab.h"

#ifdef DEFAULT
#undef DEFAULT
#endif

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Root object index and null object marker.
 */
#define ROOT 0
#define NIL  (-1)

/*
 * Keyboard shift-state bits reported by AES input calls.
 */
#define K_RSHIFT 0x0001
#define K_LSHIFT 0x0002
#define K_CTRL   0x0004
#define K_ALT    0x0008

/*
 * Maximum object-tree traversal depth used by the classic AES API.
 */
#define MAX_LEN   81
#define MAX_DEPTH 8

/*
 * Classic fill-pattern selectors used by object colors and alerts.
 */
#define IP_HOLLOW 0
#define IP_1PATT  1
#define IP_2PATT  2
#define IP_3PATT  3
#define IP_4PATT  4
#define IP_5PATT  5
#define IP_6PATT  6
#define IP_SOLID  7

/*
 * Common object color words used by historical GEM resources.
 */
#define SYS_FG 0x1100
#define WTS_FG 0x11a1
#define WTN_FG 0x1100

/*
 * VDI fill styles and write modes reused by object colors and images.
 */
#define MD_REPLACE 1
#define MD_TRANS   2
#define MD_XOR     3
#define MD_ERASE   4

#define ALL_WHITE  0
#define S_AND_D    1
#define S_ONLY     3
#define NOTS_AND_D 4
#define S_XOR_D    6
#define S_OR_D     7
#define D_INVERT  10
#define NOTS_OR_D 13
#define ALL_BLACK 15

#define FIS_HOLLOW  0
#define FIS_SOLID   1
#define FIS_PATTERN 2
#define FIS_HATCH   3
#define FIS_USER    4

#define IBM   3
#define SMALL 5

/*
 * Object-library color indices.
 */
#define WHITE    0
#define BLACK    1
#define RED      2
#define GREEN    3
#define BLUE     4
#define CYAN     5
#define YELLOW   6
#define MAGENTA  7
#define LWHITE   8
#define LBLACK   9
#define LRED    10
#define LGREEN  11
#define LBLUE   12
#define LCYAN   13
#define LYELLOW 14
#define LMAGENTA 15

/*
 * AES object types.
 */
#define G_BOX      20
#define G_TEXT     21
#define G_BOXTEXT  22
#define G_IMAGE    23
#define G_USERDEF  24
#define G_PROGDEF  G_USERDEF
#define G_IBOX     25
#define G_BUTTON   26
#define G_BOXCHAR  27
#define G_STRING   28
#define G_FTEXT    29
#define G_FBOXTEXT 30
#define G_ICON     31
#define G_TITLE    32

/*
 * AES object flags.
 */
#define NONE       0x0000
#define SELECTABLE 0x0001
#define DEFAULT    0x0002
#define EXIT       0x0004
#define EDITABLE   0x0008
#define RBUTTON    0x0010
#define LASTOB     0x0020
#define TOUCHEXIT  0x0040
#define HIDETREE   0x0080
#define INDIRECT   0x0100

/*
 * AES object states.
 */
#define NORMAL    0x0000
#define SELECTED  0x0001
#define CROSSED   0x0002
#define CHECKED   0x0004
#define DISABLED  0x0008
#define OUTLINED  0x0010
#define SHADOWED  0x0020
#define WHITEBAK  0x0040
#define DRAW3D    0x0080

/*
 * Editable-text operation selectors.
 */
#define EDSTART 0
#define EDINIT  1
#define EDCHAR  2
#define EDEND   3

/*
 * Text justification values.
 */
#define TE_LEFT  0
#define TE_RIGHT 1
#define TE_CNTR  2

/*
 * Standard AES event flags.
 */
#define MU_KEYBD  0x0001
#define MU_BUTTON 0x0002
#define MU_M1     0x0004
#define MU_M2     0x0008
#define MU_MESAG  0x0010
#define MU_TIMER  0x0020
#define MU_SDMSG  0x0040
#define MU_MUTEX  0x0080

/*
 * AES message types.
 */
#define MN_SELECTED 10

#define WM_REDRAW   20
#define WM_TOPPED   21
#define WM_CLOSED   22
#define WM_FULLED   23
#define WM_ARROWED  24
#define WM_HSLID    25
#define WM_VSLID    26
#define WM_SIZED    27
#define WM_MOVED    28
#define WM_NEWTOP   29
#define WM_UNTOPPED 30

#define AC_OPEN   40
#define AC_CLOSE  41
#define AC_ABORT  42

#define CT_UPDATE 50
#define CT_MOVE   51
#define CT_NEWTOP 52

/*
 * Form-dial operation selectors.
 */
#define FMD_START  0
#define FMD_GROW   1
#define FMD_SHRINK 2
#define FMD_FINISH 3

/*
 * Standard graf_mouse() cursor selectors.
 */
#define ARROW   0
#define TEXT_CRSR 1
#define HGLASS  2
#define POINT_HAND 3
#define FLAT_HAND 4
#define THIN_CROSS 5
#define THICK_CROSS 6
#define OUTLN_CROSS 7
#define USER_DEF 255
#define M_OFF   256
#define M_ON    257

/*
 * Window border element flags.
 */
#define NAME     0x0001
#define CLOSER   0x0002
#define FULLER   0x0004
#define MOVER    0x0008
#define INFO     0x0010
#define SIZER    0x0020
#define UPARROW  0x0040
#define DNARROW  0x0080
#define VSLIDE   0x0100
#define LFARROW  0x0200
#define RTARROW  0x0400
#define HSLIDE   0x0800
#define HOTCLOSE 0x1000

/*
 * Window kind selectors used by wind_calc().
 */
#define WC_BORDER 0
#define WC_WORK   1

/*
 * Window field selectors.
 */
#define WF_KIND       1
#define WF_NAME       2
#define WF_INFO       3
#define WF_WXYWH      4
#define WF_CXYWH      5
#define WF_PXYWH      6
#define WF_FXYWH      7
#define WF_HSLIDE     8
#define WF_VSLIDE     9
#define WF_TOP       10
#define WF_FIRSTXYWH 11
#define WF_NEXTXYWH  12
#define WF_IGNORE    13
#define WF_NEWDESK   14
#define WF_HSLSIZ    15
#define WF_VSLSIZ    16
#define WF_SCREEN    17
#define WF_TATTRB    18
#define WF_SIZTOP    19

/*
 * Window-manager update modes and arrow-message codes.
 */
#define END_UPDATE 0
#define BEG_UPDATE 1

#define WA_UPPAGE 0
#define WA_DNPAGE 1
#define WA_UPLINE 2
#define WA_DNLINE 3
#define WA_LFPAGE 4
#define WA_RTPAGE 5
#define WA_LFLINE 6
#define WA_RTLINE 7

/*
 * Resource type selectors for rsrc_gaddr() and rsrc_saddr().
 */
#define R_TREE      0
#define R_OBJECT    1
#define R_TEDINFO   2
#define R_ICONBLK   3
#define R_BITBLK    4
#define R_STRING    5
#define R_IMAGEDATA 6
#define R_OBSPEC    7
#define R_TEPTEXT   8
#define R_TEPTMPLT  9
#define R_TEPVALID 10
#define R_IBPMASK  11
#define R_IBPDATA  12
#define R_IBPTEXT  13
#define R_BIPDATA  14
#define R_FRSTR    15
#define R_FRIMG    16

/*
 * Scrap file-type flags used by the historical clipboard directory.
 */
#define SC_FTCSV 0x0001
#define SC_FTTXT 0x0002
#define SC_FTGEM 0x0004
#define SC_FTIMG 0x0008
#define SC_FTDCA 0x0010
#define SC_FTUSR 0x8000

/*
 * Rectangle in GEM origin-width-height form.
 */
typedef struct grect {
    WORD g_x;
    WORD g_y;
    WORD g_w;
    WORD g_h;
} GRECT;

/*
 * Rectangle list node used by window and redraw logic.
 */
typedef struct orect {
    struct orect *o_link;
    WORD o_x;
    WORD o_y;
    WORD o_w;
    WORD o_h;
} ORECT;

/*
 * AES object tree entry.
 */
typedef struct object {
    WORD  ob_next;
    WORD  ob_head;
    WORD  ob_tail;
    UWORD ob_type;
    UWORD ob_flags;
    UWORD ob_state;
    LONG  ob_spec;
    WORD  ob_x;
    WORD  ob_y;
    WORD  ob_width;
    WORD  ob_height;
} OBJECT;

/*
 * AES editable text information block.
 */
typedef struct text_edinfo {
    LONG te_ptext;
    LONG te_ptmplt;
    LONG te_pvalid;
    WORD te_font;
    WORD te_junk1;
    WORD te_just;
    WORD te_color;
    WORD te_junk2;
    WORD te_thickness;
    WORD te_txtlen;
    WORD te_tmplen;
} TEDINFO;

/*
 * AES icon object description block.
 */
typedef struct icon_block {
    LONG ib_pmask;
    LONG ib_pdata;
    LONG ib_ptext;
    WORD ib_char;
    WORD ib_xchar;
    WORD ib_ychar;
    WORD ib_xicon;
    WORD ib_yicon;
    WORD ib_wicon;
    WORD ib_hicon;
    WORD ib_xtext;
    WORD ib_ytext;
    WORD ib_wtext;
    WORD ib_htext;
} ICONBLK;

/*
 * AES bit-image object block.
 */
typedef struct bit_block {
    LONG bi_pdata;
    WORD bi_wb;
    WORD bi_hl;
    WORD bi_x;
    WORD bi_y;
    WORD bi_color;
} BITBLK;

/*
 * AES application-defined object block.
 */
typedef struct appl_blk {
    LONG ab_code;
    LONG ab_parm;
} APPLBLK;

typedef APPLBLK USERBLK;

/*
 * Parameters passed to an application-defined object draw/change
 * routine.
 */
typedef struct parm_blk {
    LONG pb_tree;
    WORD pb_obj;
    WORD pb_prevstate;
    WORD pb_currstate;
    WORD pb_x;
    WORD pb_y;
    WORD pb_w;
    WORD pb_h;
    WORD pb_xc;
    WORD pb_yc;
    WORD pb_wc;
    WORD pb_hc;
    LONG pb_parm;
} PARMBLK;

/*
 * Mouse rectangle block used by evnt_multi().
 */
typedef struct moblk {
    WORD m_out;
    WORD m_x;
    WORD m_y;
    WORD m_w;
    WORD m_h;
} MOBLK;

/*
 * Resource-file header.
 */
typedef struct rshdr {
    WORD rsh_vrsn;
    WORD rsh_object;
    WORD rsh_tedinfo;
    WORD rsh_iconblk;
    WORD rsh_bitblk;
    WORD rsh_frstr;
    WORD rsh_string;
    WORD rsh_imdata;
    WORD rsh_frimg;
    WORD rsh_trindex;
    WORD rsh_nobs;
    WORD rsh_ntree;
    WORD rsh_nted;
    WORD rsh_nib;
    WORD rsh_nbb;
    WORD rsh_nstring;
    WORD rsh_nimages;
    WORD rsh_rssize;
} RSHDR;

/*
 * Corner-coordinate rectangle used by some GEM examples and helper code.
 */
typedef struct vrect {
    WORD x1;
    WORD y1;
    WORD x2;
    WORD y2;
} VRECT;

/*
 * AES/VDI parameter arrays traditionally maintained per client.
 */
extern WORD global[15];
extern WORD contrl[12];
extern WORD intin[128];
extern WORD ptsin[256];
extern WORD intout[128];
extern WORD ptsout[256];

/*
 * Initialize the calling application and return its AES application id.
 */
WORD appl_init(VOID);

/*
 * Terminate the calling application's AES session.
 */
WORD appl_exit(void);

/*
 * Look up an application id by padded application name.
 */
WORD appl_find(char *name);

/*
 * Set historical boot-volume flags used by some desktop environments.
 */
WORD appl_bvset(WORD bvdisk, WORD bvhard);

/*
 * Send a message buffer to another AES application.
 */
WORD appl_write(WORD rwid, WORD length, void *pbuff);

/*
 * Receive a message buffer from another AES application.
 */
WORD appl_read(WORD ap_rwid, WORD ap_rlength, void *ap_rpbuff);

/*
 * Play back a recorded AES event stream.
 */
WORD appl_tplay(void *pbuff, WORD length, WORD scale);

/*
 * Record AES events into a caller-provided buffer.
 */
WORD appl_trecord(void *pbuff, WORD length);

/*
 * Yield execution to the AES scheduler without terminating the
 * application.
 */
WORD appl_yield(void);

/*
 * Wait for and return the next keyboard event.
 */
WORD evnt_keybd(void);

/*
 * Wait for a mouse-button transition matching the supplied mask/state.
 */
WORD evnt_button(WORD clicks, UWORD mask, UWORD state,
                 WORD *pmx, WORD *pmy, WORD *pmb, WORD *pks);

/*
 * Wait for the mouse to enter or leave a rectangle.
 */
WORD evnt_mouse(WORD flags, WORD x, WORD y, WORD w, WORD h,
                WORD *pmx, WORD *pmy, WORD *pmb, WORD *pks);

/*
 * Wait for and receive the next AES message.
 */
WORD evnt_mesag(WORD msg[8]);

/*
 * Delay for the requested timer interval.
 */
WORD evnt_timer(WORD locnt, WORD hicnt);

/*
 * Wait for a combination of keyboard, button, mouse, message, and timer
 * events.
 */
WORD evnt_multi(UWORD flags,
                UWORD bclk, UWORD bmsk, UWORD bst,
                UWORD m1flags, WORD m1x, WORD m1y, WORD m1w, WORD m1h,
                UWORD m2flags, WORD m2x, WORD m2y, WORD m2w, WORD m2h,
                WORD mepbuff[8],
                UWORD tlc, UWORD thc,
                WORD *pmx, WORD *pmy, WORD *pmb, WORD *pks,
                WORD *pkr, WORD *pbr);

/*
 * Read or set the AES double-click speed preference.
 */
WORD evnt_dclick(WORD clicks, WORD setget);

/*
 * Show or hide a menu bar tree.
 */
WORD menu_bar(OBJECT *tree, WORD show);

/*
 * Set or clear the checkmark state of a menu item.
 */
WORD menu_icheck(OBJECT *tree, WORD item, WORD check);

/*
 * Enable or disable a menu item.
 */
WORD menu_ienable(OBJECT *tree, WORD item, WORD enable);

/*
 * Set a menu title back to its normal or highlighted appearance.
 */
WORD menu_tnormal(OBJECT *tree, WORD title, WORD normal);

/*
 * Replace the text of a menu item.
 */
WORD menu_text(OBJECT *tree, WORD item, char *text);

/*
 * Register a desk accessory or application menu name.
 */
WORD menu_register(WORD pid, char *pstr);

/*
 * Unregister a previously registered menu entry.
 */
WORD menu_unregister(WORD mid);

/*
 * Read or set the menu click-speed preference.
 */
WORD menu_click(WORD click, WORD setit);

/*
 * Add a child object beneath a parent in an object tree.
 */
WORD objc_add(OBJECT *tree, WORD parent, WORD child);

/*
 * Delete an object from its parent's child list.
 */
WORD objc_delete(OBJECT *tree, WORD object);

/*
 * Draw an object tree clipped to the supplied rectangle.
 */
WORD objc_draw(OBJECT *tree, WORD startob, WORD depth,
               WORD xc, WORD yc, WORD wc, WORD hc);

/*
 * Find the deepest object at the given mouse coordinates.
 */
WORD objc_find(OBJECT *tree, WORD startob, WORD depth, WORD mx, WORD my);

/*
 * Return the absolute screen position of an object.
 */
WORD objc_offset(OBJECT *tree, WORD obj, WORD *poffx, WORD *poffy);

/*
 * Reorder an object among its siblings.
 */
WORD objc_order(OBJECT *tree, WORD object, WORD newpos);

/*
 * Edit a TEDINFO-based object using a character input.
 */
WORD objc_edit(OBJECT *tree, WORD object, WORD charidx, WORD *idx,
               WORD kind);

/*
 * Change an object's state and optionally redraw it.
 */
WORD objc_change(OBJECT *tree, WORD object, WORD depth,
                 WORD xc, WORD yc, WORD wc, WORD hc,
                 WORD newstate, WORD redraw);

/*
 * Run a dialog until an exit object is selected.
 */
WORD form_do(OBJECT *tree, WORD startob);

/*
 * Animate or finalize a dialog grow/shrink operation.
 */
WORD form_dial(WORD flag, WORD x1, WORD y1, WORD w1, WORD h1,
               WORD x2, WORD y2, WORD w2, WORD h2);

/*
 * Display a formatted alert box and return the selected button.
 */
WORD form_alert(WORD defbut, char *astring);

/*
 * Display the standard AES error alert for an error code.
 */
WORD form_error(WORD errnum);

/*
 * Compute the centered rectangle for a dialog tree.
 */
WORD form_center(OBJECT *tree, WORD *cx, WORD *cy, WORD *cw, WORD *ch);

/*
 * Process a keyboard event for a form and compute the next object.
 */
WORD form_keybd(OBJECT *tree, WORD object, WORD next,
                WORD thechar, WORD *newobj, WORD *newchar);

/*
 * Process a button click for a form and compute the next object.
 */
WORD form_button(OBJECT *tree, WORD object, WORD clicks, WORD *newobj);

/*
 * Let the user stretch a rubber box from an origin.
 */
WORD graf_rubbox(WORD xorigin, WORD yorigin, WORD wmin, WORD hmin,
                 WORD *pwend, WORD *phend);

#define graf_rubberbox graf_rubbox

/*
 * Let the user drag a box within a bounding rectangle.
 */
WORD graf_dragbox(WORD w, WORD h, WORD sx, WORD sy,
                  WORD xc, WORD yc, WORD wc, WORD hc,
                  WORD *pdx, WORD *pdy);

/*
 * Move a box visually from one rectangle to another.
 */
WORD graf_mbox(WORD w, WORD h, WORD srcx, WORD srcy, WORD dstx, WORD dsty);

#define graf_movebox graf_mbox

/*
 * Animate a growing box.
 */
WORD graf_growbox(WORD x1, WORD y1, WORD w1, WORD h1,
                  WORD x2, WORD y2, WORD w2, WORD h2);

/*
 * Animate a shrinking box.
 */
WORD graf_shrinkbox(WORD x1, WORD y1, WORD w1, WORD h1,
                    WORD x2, WORD y2, WORD w2, WORD h2);

/*
 * Track an object while the user holds the mouse button.
 */
WORD graf_watchbox(OBJECT *tree, WORD object, UWORD in_state,
                   UWORD out_state);

/*
 * Return a slider position relative to its parent elevator range.
 */
WORD graf_slidebox(OBJECT *tree, WORD parent, WORD object,
                   WORD orientation);

/*
 * Query character and box metrics for the current workstation.
 */
WORD graf_handle(WORD *pwchar, WORD *phchar, WORD *pwbox, WORD *phbox);

/*
 * Change or hide the AES mouse cursor.
 */
WORD graf_mouse(WORD m_number, void *m_addr);

/*
 * Query the current mouse position, button state, and key state.
 */
VOID graf_mkstate(WORD *pmx, WORD *pmy, WORD *pmstate, WORD *pkstate);

/*
 * Read the current scrap directory path.
 */
WORD scrp_read(char *pscrap);

/*
 * Write the current scrap directory path.
 */
WORD scrp_write(char *pscrap);

/*
 * Clear the historical scrap directory contents.
 */
WORD scrp_clear(void);

/*
 * Display the file selector and return the chosen path, file, and
 * button.
 */
WORD fsel_input(char *pipath, char *pisel, WORD *pbutton);

#if defined(MULTIAPP) && MULTIAPP
/*
 * Create a process entry in multi-application GEM configurations.
 */
WORD proc_create(LONG ibegaddr, LONG isize, WORD isswap, WORD isgem,
                 WORD *onum);

/*
 * Run a process through the AES process manager.
 */
WORD proc_run(WORD proc_num, WORD isgraf, WORD isover,
              char *pcmd, char *ptail);

/*
 * Delete a process entry from the AES process manager.
 */
WORD proc_delete(WORD proc_num);

/*
 * Query process metadata from the AES process manager.
 */
WORD proc_info(WORD num, WORD *oisswap, WORD *oisgem,
               LONG *obegaddr, LONG *ocsize, LONG *oendmem,
               LONG *ossize, LONG *ointtbl);

/*
 * Allocate process memory through the AES process manager.
 */
LONG proc_malloc(LONG csize, LONG *adrcsize);

/*
 * Free process memory through the AES process manager.
 */
WORD proc_mfree(LONG csize, LONG adrcsize);

/*
 * Switch the active process in a multi-application AES configuration.
 */
WORD proc_switch(WORD pid);

/*
 * Change process memory block ownership or bounds.
 */
WORD proc_setblock(WORD pid, LONG block_addr, LONG block_size);
#endif

/*
 * Create a window with the requested border elements.
 */
WORD wind_create(UWORD kind, WORD wx, WORD wy, WORD ww, WORD wh);

/*
 * Open a window at the requested position and size.
 */
WORD wind_open(WORD handle, WORD wx, WORD wy, WORD ww, WORD wh);

/*
 * Close a window.
 */
WORD wind_close(WORD handle);

/*
 * Delete a window and free its AES state.
 */
WORD wind_delete(WORD handle);

/*
 * Read a window field or geometry tuple.
 */
WORD wind_get(WORD handle, WORD field, WORD *w1, WORD *w2, WORD *w3,
              WORD *w4);

/*
 * Set a window field or geometry tuple.
 */
WORD wind_set(WORD handle, WORD field, WORD w1, WORD w2, WORD w3,
              WORD w4);

/*
 * Set a pointer-based window string field such as `WF_NAME` or
 * `WF_INFO` without truncating the host pointer value.
 */
WORD wind_set_str(WORD handle, WORD field, const char *text);

/*
 * Return the topmost window containing a screen point.
 */
WORD wind_find(WORD mx, WORD my);

/*
 * Bracket a period of window-system updating.
 */
WORD wind_update(WORD beg_update);

/*
 * Convert between border and work-area rectangles.
 */
WORD wind_calc(WORD type, UWORD kind,
               WORD inx, WORD iny, WORD inw, WORD inh,
               WORD *outx, WORD *outy, WORD *outw, WORD *outh);

/*
 * Load a compiled AES resource file.
 */
WORD rsrc_load(char *filename);

/*
 * Free the currently loaded AES resource file.
 */
WORD rsrc_free(void);

/*
 * Look up an address inside the loaded AES resource file.
 */
WORD rsrc_gaddr(WORD type, WORD index, void **addr);

/*
 * Replace an address inside the loaded AES resource file.
 */
WORD rsrc_saddr(WORD type, WORD index, void *addr);

/*
 * Fix up an object tree after loading a resource file.
 */
WORD rsrc_obfix(OBJECT *tree, WORD obj);

/*
 * Read the shell command tail and command path for the current launch.
 */
WORD shel_read(char *cmd, char *tail);

/*
 * Request that AES launch or switch to another application.
 */
WORD shel_write(WORD doex, WORD isgr, WORD iscr, char *cmd, char *tail);

/*
 * Read bytes from the AES shell buffer.
 */
WORD shel_get(char *buf, WORD length);

/*
 * Write bytes to the AES shell buffer.
 */
WORD shel_put(char *buf, WORD length);

/*
 * Resolve an executable path using the AES shell search rules.
 */
WORD shel_find(char *path);

/*
 * Search the environment for a named variable.
 */
WORD shel_envrn(char **env, char *var);

/*
 * Read the default shell application and directory.
 */
WORD shel_rdef(char *lpcmd, char *lpdir);

/*
 * Write the default shell application and directory.
 */
WORD shel_wdef(char *lpcmd, char *lpdir);

#ifdef __cplusplus
}
#endif

#endif /* GEM_AES_H */
