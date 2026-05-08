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
#define ROOT 0     /* Root object index in a tree. */
#define NIL  (-1)  /* Missing object / null link marker. */

/*
 * Keyboard shift-state bits reported by AES input calls.
 */
#define K_RSHIFT 0x0001  /* Right Shift key is pressed. */
#define K_LSHIFT 0x0002  /* Left Shift key is pressed. */
#define K_CTRL   0x0004  /* Control key is pressed. */
#define K_ALT    0x0008  /* Alternate / Alt key is pressed. */

/*
 * Maximum object-tree traversal depth used by the classic AES API.
 */
#define MAX_LEN   81  /* Historical maximum string/template length. */
#define MAX_DEPTH 8   /* Historical tree traversal depth limit. */

/*
 * Classic fill-pattern selectors used by object colors and alerts.
 */
#define IP_HOLLOW 0  /* Hollow interior pattern. */
#define IP_1PATT  1  /* Pattern fill style 1. */
#define IP_2PATT  2  /* Pattern fill style 2. */
#define IP_3PATT  3  /* Pattern fill style 3. */
#define IP_4PATT  4  /* Pattern fill style 4. */
#define IP_5PATT  5  /* Pattern fill style 5. */
#define IP_6PATT  6  /* Pattern fill style 6. */
#define IP_SOLID  7  /* Solid interior fill. */

/*
 * Common object color words used by historical GEM resources.
 */
#define SYS_FG 0x1100  /* Standard system foreground color word. */
#define WTS_FG 0x11a1  /* Selected window-title foreground word. */
#define WTN_FG 0x1100  /* Normal window-title foreground word. */

/*
 * VDI fill styles and write modes reused by object colors and images.
 */
#define MD_REPLACE 1  /* Replace destination pixels. */
#define MD_TRANS   2  /* Draw transparently. */
#define MD_XOR     3  /* XOR with destination pixels. */
#define MD_ERASE   4  /* Erase from destination pixels. */

#define ALL_WHITE  0  /* Raster op: force white. */
#define S_AND_D    1  /* Raster op: source AND destination. */
#define S_ONLY     3  /* Raster op: source only. */
#define NOTS_AND_D 4  /* Raster op: NOT source AND destination. */
#define S_XOR_D    6  /* Raster op: source XOR destination. */
#define S_OR_D     7  /* Raster op: source OR destination. */
#define D_INVERT  10  /* Raster op: invert destination. */
#define NOTS_OR_D 13  /* Raster op: NOT source OR destination. */
#define ALL_BLACK 15  /* Raster op: force black. */

#define FIS_HOLLOW  0  /* Hollow fill interior. */
#define FIS_SOLID   1  /* Solid fill interior. */
#define FIS_PATTERN 2  /* Pattern fill interior. */
#define FIS_HATCH   3  /* Hatch fill interior. */
#define FIS_USER    4  /* User-defined fill interior. */

#define IBM   3  /* Historical IBM/system font id alias. */
#define SMALL 5  /* Historical small font id alias. */

/*
 * Object-library color indices.
 */
#define WHITE     0  /* Standard white color index. */
#define BLACK     1  /* Standard black color index. */
#define RED       2  /* Standard red color index. */
#define GREEN     3  /* Standard green color index. */
#define BLUE      4  /* Standard blue color index. */
#define CYAN      5  /* Standard cyan color index. */
#define YELLOW    6  /* Standard yellow color index. */
#define MAGENTA   7  /* Standard magenta color index. */
#define LWHITE    8  /* Light white color index. */
#define LBLACK    9  /* Light black / dark gray color index. */
#define LRED     10  /* Light red color index. */
#define LGREEN   11  /* Light green color index. */
#define LBLUE    12  /* Light blue color index. */
#define LCYAN    13  /* Light cyan color index. */
#define LYELLOW  14  /* Light yellow color index. */
#define LMAGENTA 15  /* Light magenta color index. */

/*
 * AES object types.
 */
#define G_BOX      20        /* Plain box object. */
#define G_TEXT     21        /* Static text object. */
#define G_BOXTEXT  22        /* Text object with box background. */
#define G_IMAGE    23        /* Bit image object. */
#define G_USERDEF  24        /* Application-defined draw object. */
#define G_PROGDEF  G_USERDEF /* Alias for user-defined object. */
#define G_IBOX     25        /* Invisible box object. */
#define G_BUTTON   26        /* Button object. */
#define G_BOXCHAR  27        /* Box with single character. */
#define G_STRING   28        /* Static string object. */
#define G_FTEXT    29        /* Editable text object. */
#define G_FBOXTEXT 30        /* Editable boxed text object. */
#define G_ICON     31        /* Icon object. */
#define G_TITLE    32        /* Menu title object. */

/*
 * AES object flags.
 */
#define NONE       0x0000  /* No object flags set. */
#define SELECTABLE 0x0001  /* Object can be selected. */
#define DEFAULT    0x0002  /* Default object in a dialog. */
#define EXIT       0x0004  /* Object exits a dialog when activated. */
#define EDITABLE   0x0008  /* Object accepts text editing. */
#define RBUTTON    0x0010  /* Object is a radio button. */
#define LASTOB     0x0020  /* Object is last sibling in a list. */
#define TOUCHEXIT  0x0040  /* Touch/click exits immediately. */
#define HIDETREE   0x0080  /* Object subtree is hidden. */
#define INDIRECT   0x0100  /* Object spec is an indirect pointer. */

/*
 * AES object states.
 */
#define NORMAL    0x0000  /* Normal object appearance. */
#define SELECTED  0x0001  /* Object is selected. */
#define CROSSED   0x0002  /* Object is crossed out. */
#define CHECKED   0x0004  /* Object is checked. */
#define DISABLED  0x0008  /* Object is disabled. */
#define OUTLINED  0x0010  /* Object is outlined. */
#define SHADOWED  0x0020  /* Object is shadowed. */
#define WHITEBAK  0x0040  /* Object uses white background. */
#define DRAW3D    0x0080  /* Object requests 3D drawing. */

/*
 * Editable-text operation selectors.
 */
#define EDSTART 0  /* Start editing session. */
#define EDINIT  1  /* Initialize editing state. */
#define EDCHAR  2  /* Process one input character. */
#define EDEND   3  /* End editing session. */

/*
 * Text justification values.
 */
#define TE_LEFT  0  /* Left-justified text. */
#define TE_RIGHT 1  /* Right-justified text. */
#define TE_CNTR  2  /* Center-justified text. */

/*
 * Standard AES event flags.
 */
#define MU_KEYBD  0x0001  /* Wait for keyboard event. */
#define MU_BUTTON 0x0002  /* Wait for button event. */
#define MU_M1     0x0004  /* Wait for mouse rectangle 1 event. */
#define MU_M2     0x0008  /* Wait for mouse rectangle 2 event. */
#define MU_MESAG  0x0010  /* Wait for AES message event. */
#define MU_TIMER  0x0020  /* Wait for timer event. */
#define MU_SDMSG  0x0040  /* Wait for shell/desk message. */
#define MU_MUTEX  0x0080  /* Wait with AES mutual exclusion. */

/*
 * AES message types.
 */
#define MN_SELECTED 10  /* Menu item selected message. */

#define WM_REDRAW   20  /* Window redraw request. */
#define WM_TOPPED   21  /* Window topped request. */
#define WM_CLOSED   22  /* Window close request. */
#define WM_FULLED   23  /* Window full-size toggle request. */
#define WM_ARROWED  24  /* Window arrow action message. */
#define WM_HSLID    25  /* Horizontal slider moved. */
#define WM_VSLID    26  /* Vertical slider moved. */
#define WM_SIZED    27  /* Window resized. */
#define WM_MOVED    28  /* Window moved. */
#define WM_NEWTOP   29  /* New top window notification. */
#define WM_UNTOPPED 30  /* Window lost top status. */

#define AC_OPEN   40  /* Accessory open message. */
#define AC_CLOSE  41  /* Accessory close message. */
#define AC_ABORT  42  /* Accessory abort message. */

#define CT_UPDATE 50  /* Control-manager update message. */
#define CT_MOVE   51  /* Control-manager move message. */
#define CT_NEWTOP 52  /* Control-manager new-top message. */

/*
 * Form-dial operation selectors.
 */
#define FMD_START  0  /* Begin form-dial operation. */
#define FMD_GROW   1  /* Animate grow phase. */
#define FMD_SHRINK 2  /* Animate shrink phase. */
#define FMD_FINISH 3  /* Finish form-dial operation. */

/*
 * Standard graf_mouse() cursor selectors.
 */
#define ARROW       0    /* Standard arrow cursor. */
#define TEXT_CRSR   1    /* Text-entry cursor. */
#define HGLASS      2    /* Busy / hourglass cursor. */
#define POINT_HAND  3    /* Pointing hand cursor. */
#define FLAT_HAND   4    /* Flat hand cursor. */
#define THIN_CROSS  5    /* Thin crosshair cursor. */
#define THICK_CROSS 6    /* Thick crosshair cursor. */
#define OUTLN_CROSS 7    /* Outlined crosshair cursor. */
#define USER_DEF    255  /* Use caller-supplied cursor form. */
#define M_OFF       256  /* Hide the mouse cursor. */
#define M_ON        257  /* Show the mouse cursor. */

/*
 * Window border element flags.
 */
#define NAME     0x0001  /* Window has title/name bar. */
#define CLOSER   0x0002  /* Window has close box. */
#define FULLER   0x0004  /* Window has full-size box. */
#define MOVER    0x0008  /* Window can be moved. */
#define INFO     0x0010  /* Window has info line. */
#define SIZER    0x0020  /* Window has size box. */
#define UPARROW  0x0040  /* Window has up arrow. */
#define DNARROW  0x0080  /* Window has down arrow. */
#define VSLIDE   0x0100  /* Window has vertical slider. */
#define LFARROW  0x0200  /* Window has left arrow. */
#define RTARROW  0x0400  /* Window has right arrow. */
#define HSLIDE   0x0800  /* Window has horizontal slider. */
#define HOTCLOSE 0x1000  /* Window supports hot close behavior. */

/*
 * Window kind selectors used by wind_calc().
 */
#define WC_BORDER 0  /* Convert using border rectangle. */
#define WC_WORK   1  /* Convert using work-area rectangle. */

/*
 * Window field selectors.
 */
#define WF_KIND       1  /* Window kind bit mask. */
#define WF_NAME       2  /* Window title string. */
#define WF_INFO       3  /* Window info-line string. */
#define WF_WXYWH      4  /* Current outer window rectangle. */
#define WF_CXYWH      5  /* Current work-area rectangle. */
#define WF_PXYWH      6  /* Previous outer window rectangle. */
#define WF_FXYWH      7  /* Full-size rectangle. */
#define WF_HSLIDE     8  /* Horizontal slider position. */
#define WF_VSLIDE     9  /* Vertical slider position. */
#define WF_TOP       10  /* Bring window to top. */
#define WF_FIRSTXYWH 11  /* First visible rectangle. */
#define WF_NEXTXYWH  12  /* Next visible rectangle. */
#define WF_IGNORE    13  /* Ignore rectangle or field. */
#define WF_NEWDESK   14  /* Install new desktop tree. */
#define WF_HSLSIZ    15  /* Horizontal slider size. */
#define WF_VSLSIZ    16  /* Vertical slider size. */
#define WF_SCREEN    17  /* Screen/workstation data. */
#define WF_TATTRB    18  /* Title attributes. */
#define WF_SIZTOP    19  /* Size and top in one call. */

/*
 * Window-manager update modes and arrow-message codes.
 */
#define END_UPDATE 0  /* End window update bracket. */
#define BEG_UPDATE 1  /* Begin window update bracket. */

#define WA_UPPAGE 0  /* Page up action. */
#define WA_DNPAGE 1  /* Page down action. */
#define WA_UPLINE 2  /* Line up action. */
#define WA_DNLINE 3  /* Line down action. */
#define WA_LFPAGE 4  /* Page left action. */
#define WA_RTPAGE 5  /* Page right action. */
#define WA_LFLINE 6  /* Line left action. */
#define WA_RTLINE 7  /* Line right action. */

/*
 * Resource type selectors for rsrc_gaddr() and rsrc_saddr().
 */
#define R_TREE      0   /* Resource tree pointer. */
#define R_OBJECT    1   /* Resource object entry. */
#define R_TEDINFO   2   /* Resource TEDINFO entry. */
#define R_ICONBLK   3   /* Resource ICONBLK entry. */
#define R_BITBLK    4   /* Resource BITBLK entry. */
#define R_STRING    5   /* Resource string entry. */
#define R_IMAGEDATA 6   /* Resource image-data entry. */
#define R_OBSPEC    7   /* Resource object spec entry. */
#define R_TEPTEXT   8   /* TEDINFO text pointer entry. */
#define R_TEPTMPLT  9   /* TEDINFO template pointer entry. */
#define R_TEPVALID 10   /* TEDINFO validation pointer entry. */
#define R_IBPMASK  11   /* ICONBLK mask pointer entry. */
#define R_IBPDATA  12   /* ICONBLK data pointer entry. */
#define R_IBPTEXT  13   /* ICONBLK text pointer entry. */
#define R_BIPDATA  14   /* BITBLK data pointer entry. */
#define R_FRSTR    15   /* Free string list entry. */
#define R_FRIMG    16   /* Free image list entry. */

/*
 * Scrap file-type flags used by the historical clipboard directory.
 */
#define SC_FTCSV 0x0001  /* CSV clipboard content available. */
#define SC_FTTXT 0x0002  /* Text clipboard content available. */
#define SC_FTGEM 0x0004  /* GEM clipboard content available. */
#define SC_FTIMG 0x0008  /* Image clipboard content available. */
#define SC_FTDCA 0x0010  /* DCA clipboard content available. */
#define SC_FTUSR 0x8000  /* User-defined clipboard content available. */

/*
 * Rectangle in GEM origin-width-height form.
 */
typedef struct grect {
    WORD g_x;  /* Left edge in screen or work coordinates. */
    WORD g_y;  /* Top edge in screen or work coordinates. */
    WORD g_w;  /* Rectangle width. */
    WORD g_h;  /* Rectangle height. */
} GRECT;

/*
 * Rectangle list node used by window and redraw logic.
 */
typedef struct orect {
    struct orect *o_link;  /* Next rectangle node in the list. */
    WORD o_x;              /* Left edge. */
    WORD o_y;              /* Top edge. */
    WORD o_w;              /* Rectangle width. */
    WORD o_h;              /* Rectangle height. */
} ORECT;

/*
 * AES object tree entry.
 */
typedef struct object {
    WORD  ob_next;    /* Next sibling object index. */
    WORD  ob_head;    /* First child object index. */
    WORD  ob_tail;    /* Last child object index. */
    UWORD ob_type;    /* Object type selector. */
    UWORD ob_flags;   /* Object behavior flags. */
    UWORD ob_state;   /* Current object state bits. */
    LONG  ob_spec;    /* Type-specific object data or pointer. */
    WORD  ob_x;       /* X position relative to parent. */
    WORD  ob_y;       /* Y position relative to parent. */
    WORD  ob_width;   /* Object width. */
    WORD  ob_height;  /* Object height. */
} OBJECT;

/*
 * AES editable text information block.
 */
typedef struct text_edinfo {
    LONG te_ptext;      /* Pointer to current text buffer. */
    LONG te_ptmplt;     /* Pointer to edit template string. */
    LONG te_pvalid;     /* Pointer to validation string. */
    WORD te_font;       /* Font selector. */
    WORD te_junk1;      /* Reserved / historical filler field. */
    WORD te_just;       /* Text justification mode. */
    WORD te_color;      /* Text color word. */
    WORD te_junk2;      /* Reserved / historical filler field. */
    WORD te_thickness;  /* Border thickness. */
    WORD te_txtlen;     /* Text buffer length. */
    WORD te_tmplen;     /* Template length. */
} TEDINFO;

/*
 * AES icon object description block.
 */
typedef struct icon_block {
    LONG ib_pmask;   /* Pointer to icon mask bitmap. */
    LONG ib_pdata;   /* Pointer to icon data bitmap. */
    LONG ib_ptext;   /* Pointer to icon label text. */
    WORD ib_char;    /* Character and color word. */
    WORD ib_xchar;   /* Character origin X within the icon. */
    WORD ib_ychar;   /* Character origin Y within the icon. */
    WORD ib_xicon;   /* Icon bitmap origin X. */
    WORD ib_yicon;   /* Icon bitmap origin Y. */
    WORD ib_wicon;   /* Icon bitmap width. */
    WORD ib_hicon;   /* Icon bitmap height. */
    WORD ib_xtext;   /* Text origin X. */
    WORD ib_ytext;   /* Text origin Y. */
    WORD ib_wtext;   /* Text area width. */
    WORD ib_htext;   /* Text area height. */
} ICONBLK;

/*
 * AES bit-image object block.
 */
typedef struct bit_block {
    LONG bi_pdata;   /* Pointer to image bitmap data. */
    WORD bi_wb;      /* Row width in bytes. */
    WORD bi_hl;      /* Image height in scanlines. */
    WORD bi_x;       /* X origin within the object. */
    WORD bi_y;       /* Y origin within the object. */
    WORD bi_color;   /* Drawing color word. */
} BITBLK;

/*
 * AES application-defined object block.
 */
typedef struct appl_blk {
    LONG ab_code;  /* Pointer to application callback code. */
    LONG ab_parm;  /* Caller-defined callback parameter. */
} APPLBLK;

typedef APPLBLK USERBLK;

/*
 * Parameters passed to an application-defined object draw/change
 * routine.
 */
typedef struct parm_blk {
    LONG pb_tree;       /* Pointer to the object tree. */
    WORD pb_obj;        /* Current object index. */
    WORD pb_prevstate;  /* Previous object state. */
    WORD pb_currstate;  /* Current object state. */
    WORD pb_x;          /* Object X position. */
    WORD pb_y;          /* Object Y position. */
    WORD pb_w;          /* Object width. */
    WORD pb_h;          /* Object height. */
    WORD pb_xc;         /* Clipping rectangle X. */
    WORD pb_yc;         /* Clipping rectangle Y. */
    WORD pb_wc;         /* Clipping rectangle width. */
    WORD pb_hc;         /* Clipping rectangle height. */
    LONG pb_parm;       /* Caller-defined user parameter. */
} PARMBLK;

/*
 * Mouse rectangle block used by evnt_multi().
 */
typedef struct moblk {
    WORD m_out;  /* Enter/leave mode selector. */
    WORD m_x;    /* Rectangle X origin. */
    WORD m_y;    /* Rectangle Y origin. */
    WORD m_w;    /* Rectangle width. */
    WORD m_h;    /* Rectangle height. */
} MOBLK;

/*
 * Resource-file header.
 */
typedef struct rshdr {
    WORD rsh_vrsn;    /* Resource format version. */
    WORD rsh_object;  /* Offset to OBJECT array. */
    WORD rsh_tedinfo; /* Offset to TEDINFO array. */
    WORD rsh_iconblk; /* Offset to ICONBLK array. */
    WORD rsh_bitblk;  /* Offset to BITBLK array. */
    WORD rsh_frstr;   /* Offset to free-string table. */
    WORD rsh_string;  /* Offset to string data. */
    WORD rsh_imdata;  /* Offset to image data. */
    WORD rsh_frimg;   /* Offset to free-image table. */
    WORD rsh_trindex; /* Offset to tree index table. */
    WORD rsh_nobs;    /* Number of objects. */
    WORD rsh_ntree;   /* Number of trees. */
    WORD rsh_nted;    /* Number of TEDINFO entries. */
    WORD rsh_nib;     /* Number of ICONBLK entries. */
    WORD rsh_nbb;     /* Number of BITBLK entries. */
    WORD rsh_nstring; /* Number of strings. */
    WORD rsh_nimages; /* Number of images. */
    WORD rsh_rssize;  /* Total resource size in bytes. */
} RSHDR;

/*
 * Corner-coordinate rectangle used by some GEM examples and helper code.
 */
typedef struct vrect {
    WORD x1;  /* Left edge. */
    WORD y1;  /* Top edge. */
    WORD x2;  /* Right edge. */
    WORD y2;  /* Bottom edge. */
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
 * Initialize the calling application and register it with AES.
 *
 * Parameters:
 *      None.
 *
 * Returns:
 *      AES application id on success, or a negative value on failure.
 *
 * Notes:
 *      Call this before using most AES services.
 *
 * Sample call:
 *      WORD ap_id = appl_init();
 */
WORD appl_init(VOID);

/*
 * Terminate the calling application's AES session.
 *
 * Parameters:
 *      None.
 *
 * Returns:
 *      Implementation-defined completion status.
 *
 * Notes:
 *      Call this before the application exits.
 *
 * Sample call:
 *      appl_exit();
 */
WORD appl_exit(void);

/*
 * Look up an application id by application name.
 *
 * Parameters:
 *      name        - Application name, usually space-padded in GEM style.
 *
 * Returns:
 *      Matching AES application id, or a negative value if not found.
 *
 * Notes:
 *      This is commonly used for accessory and desktop integration.
 *
 * Sample call:
 *      WORD shell_id = appl_find("DESKTOP ");
 */
WORD appl_find(char *name);

/*
 * Set historical boot-volume flags.
 *
 * Parameters:
 *      bvdisk      - Boot floppy flag.
 *      bvhard      - Boot hard-disk flag.
 *
 * Returns:
 *      Non-zero on success, otherwise zero.
 *
 * Notes:
 *      Mostly relevant for compatibility with desktop-style software.
 *
 * Sample call:
 *      appl_bvset(0, 1);
 */
WORD appl_bvset(WORD bvdisk, WORD bvhard);

/*
 * Send a message buffer to another AES application.
 *
 * Parameters:
 *      rwid        - Destination AES application id.
 *      length      - Message length in bytes.
 *      pbuff       - Message buffer to send.
 *
 * Returns:
 *      Non-zero on success, otherwise zero.
 *
 * Notes:
 *      AES messages are typically eight WORDs long.
 *
 * Sample call:
 *      WORD msg[8] = {0};
 *      appl_write(dest_id, sizeof(msg), msg);
 */
WORD appl_write(WORD rwid, WORD length, void *pbuff);

/*
 * Receive a message buffer from another AES application.
 *
 * Parameters:
 *      ap_rwid     - Source application id filter or selector.
 *      ap_rlength  - Buffer length in bytes.
 *      ap_rpbuff   - Receive buffer.
 *
 * Returns:
 *      Non-zero on success, otherwise zero.
 *
 * Notes:
 *      This is a lower-level companion to `evnt_mesag()`.
 *
 * Sample call:
 *      WORD msg[8];
 *      appl_read(appl_id, sizeof(msg), msg);
 */
WORD appl_read(WORD ap_rwid, WORD ap_rlength, void *ap_rpbuff);

/*
 * Play back a recorded AES event stream.
 *
 * Parameters:
 *      pbuff       - Recorded event buffer.
 *      length      - Buffer length in bytes.
 *      scale       - Playback timing scale.
 *
 * Returns:
 *      Non-zero on success, otherwise zero.
 *
 * Notes:
 *      Useful for compatibility testing and event replay.
 *
 * Sample call:
 *      appl_tplay(buffer, size, 1);
 */
WORD appl_tplay(void *pbuff, WORD length, WORD scale);

/*
 * Record AES events into a caller-provided buffer.
 *
 * Parameters:
 *      pbuff       - Destination buffer.
 *      length      - Buffer length in bytes.
 *
 * Returns:
 *      Non-zero on success, otherwise zero.
 *
 * Notes:
 *      Pairs with `appl_tplay()` for event recording and playback.
 *
 * Sample call:
 *      appl_trecord(buffer, sizeof(buffer));
 */
WORD appl_trecord(void *pbuff, WORD length);

/*
 * Yield execution to the AES scheduler without terminating the
 * application.
 *
 * Parameters:
 *      None.
 *
 * Returns:
 *      Non-zero on success, otherwise zero.
 *
 * Notes:
 *      Use this when a long-running loop wants to cooperate with AES.
 *
 * Sample call:
 *      appl_yield();
 */
WORD appl_yield(void);

/*
 * Wait for and return the next keyboard event.
 *
 * Parameters:
 *      None.
 *
 * Returns:
 *      Encoded key value from AES.
 *
 * Notes:
 *      The return value follows GEM AES keyboard conventions.
 *
 * Sample call:
 *      WORD key = evnt_keybd();
 */
WORD evnt_keybd(void);

/*
 * Wait for a mouse-button transition.
 *
 * Parameters:
 *      clicks      - Required click count.
 *      mask        - Button mask to watch.
 *      state       - Required button state.
 *      pmx,pmy     - Receive mouse position.
 *      pmb         - Receives button state.
 *      pks         - Receives keyboard shift state.
 *
 * Returns:
 *      Encoded button event status.
 *
 * Notes:
 *      AES waits until the requested button condition is satisfied.
 *
 * Sample call:
 *      WORD mx, my, mb, ks;
 *      evnt_button(1, 1, 1, &mx, &my, &mb, &ks);
 */
WORD evnt_button(WORD clicks, UWORD mask, UWORD state,
                 WORD *pmx, WORD *pmy, WORD *pmb, WORD *pks);

/*
 * Wait for the mouse to enter or leave a rectangle.
 *
 * Parameters:
 *      flags       - Rectangle enter/leave mode.
 *      x,y,w,h     - Rectangle in origin-width-height form.
 *      pmx,pmy     - Receive mouse position.
 *      pmb         - Receives button state.
 *      pks         - Receives keyboard shift state.
 *
 * Returns:
 *      Encoded mouse event status.
 *
 * Notes:
 *      This is the classic AES mouse-rectangle watcher.
 *
 * Sample call:
 *      WORD mx, my, mb, ks;
 *      evnt_mouse(1, 10, 10, 100, 40, &mx, &my, &mb, &ks);
 */
WORD evnt_mouse(WORD flags, WORD x, WORD y, WORD w, WORD h,
                WORD *pmx, WORD *pmy, WORD *pmb, WORD *pks);

/*
 * Wait for and receive the next AES message.
 *
 * Parameters:
 *      msg         - Eight-WORD message buffer.
 *
 * Returns:
 *      Non-zero on success, otherwise zero.
 *
 * Notes:
 *      Window-manager, menu, and accessory traffic is commonly read
 *      through this call.
 *
 * Sample call:
 *      WORD msg[8];
 *      evnt_mesag(msg);
 */
WORD evnt_mesag(WORD msg[8]);

/*
 * Delay for the requested timer interval.
 *
 * Parameters:
 *      locnt       - Low WORD of the timer interval.
 *      hicnt       - High WORD of the timer interval.
 *
 * Returns:
 *      Non-zero on success, otherwise zero.
 *
 * Notes:
 *      AES interprets the two WORDs as one long timer value.
 *
 * Sample call:
 *      evnt_timer(1000, 0);
 */
WORD evnt_timer(WORD locnt, WORD hicnt);

/*
 * Wait for a combination of keyboard, button, mouse, message, and timer
 * events.
 *
 * Parameters:
 *      flags       - Combination of `MU_*` event flags.
 *      bclk,bmsk,bst
 *                  - Button event criteria.
 *      m1flags,m1x,m1y,m1w,m1h
 *                  - First mouse rectangle criteria.
 *      m2flags,m2x,m2y,m2w,m2h
 *                  - Second mouse rectangle criteria.
 *      mepbuff     - Message receive buffer.
 *      tlc,thc     - Timer interval low/high words.
 *      pmx,pmy     - Receive mouse position.
 *      pmb         - Receives button state.
 *      pks         - Receives keyboard shift state.
 *      pkr         - Receives keyboard return value.
 *      pbr         - Receives button return count.
 *
 * Returns:
 *      Bit mask describing which events occurred.
 *
 * Notes:
 *      This is the main multiplexed AES event wait call.
 *
 * Sample call:
 *      WORD msg[8], mx, my, mb, ks, kr, br;
 *      evnt_multi(MU_MESAG | MU_BUTTON, 1, 1, 1,
 *          0, 0, 0, 0, 0,
 *          0, 0, 0, 0, 0,
 *          msg, 0, 0, &mx, &my, &mb, &ks, &kr, &br);
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
 * Read or set the AES double-click preference.
 *
 * Parameters:
 *      clicks      - Double-click rate or selector.
 *      setget      - Non-zero to set, zero to query.
 *
 * Returns:
 *      Effective double-click value.
 *
 * Notes:
 *      AES stores this as a global user-interface preference.
 *
 * Sample call:
 *      evnt_dclick(3, 1);
 */
WORD evnt_dclick(WORD clicks, WORD setget);

/*
 * Show or hide a menu bar tree.
 *
 * Parameters:
 *      tree        - Menu bar object tree.
 *      show        - Non-zero to show, zero to hide.
 *
 * Returns:
 *      Non-zero on success, otherwise zero.
 *
 * Notes:
 *      The tree should usually come from a loaded resource.
 *
 * Sample call:
 *      menu_bar(menu_tree, 1);
 */
WORD menu_bar(OBJECT *tree, WORD show);

/*
 * Set or clear the checkmark state of a menu item.
 *
 * Parameters:
 *      tree        - Menu object tree.
 *      item        - Item object index.
 *      check       - Non-zero to check, zero to clear.
 *
 * Returns:
 *      Non-zero on success, otherwise zero.
 *
 * Notes:
 *      This modifies the item's visual state in the tree.
 *
 * Sample call:
 *      menu_icheck(menu_tree, item_id, 1);
 */
WORD menu_icheck(OBJECT *tree, WORD item, WORD check);

/*
 * Enable or disable a menu item.
 *
 * Parameters:
 *      tree        - Menu object tree.
 *      item        - Item object index.
 *      enable      - Non-zero to enable, zero to disable.
 *
 * Returns:
 *      Non-zero on success, otherwise zero.
 *
 * Notes:
 *      Disabled items are typically shown dimmed.
 *
 * Sample call:
 *      menu_ienable(menu_tree, item_id, 0);
 */
WORD menu_ienable(OBJECT *tree, WORD item, WORD enable);

/*
 * Set a menu title back to its normal or highlighted appearance.
 *
 * Parameters:
 *      tree        - Menu object tree.
 *      title       - Title object index.
 *      normal      - Non-zero for normal appearance, zero for highlighted.
 *
 * Returns:
 *      Non-zero on success, otherwise zero.
 *
 * Notes:
 *      Used while managing pulled-down menu title feedback.
 *
 * Sample call:
 *      menu_tnormal(menu_tree, title_id, 1);
 */
WORD menu_tnormal(OBJECT *tree, WORD title, WORD normal);

/*
 * Replace the text of a menu item.
 *
 * Parameters:
 *      tree        - Menu object tree.
 *      item        - Item object index.
 *      text        - Replacement string.
 *
 * Returns:
 *      Non-zero on success, otherwise zero.
 *
 * Notes:
 *      The caller must keep indirect text storage valid when needed.
 *
 * Sample call:
 *      menu_text(menu_tree, item_id, "Save");
 */
WORD menu_text(OBJECT *tree, WORD item, char *text);

/*
 * Register a desk accessory or application menu name.
 *
 * Parameters:
 *      pid         - AES application id.
 *      pstr        - Display name to register.
 *
 * Returns:
 *      Menu id or registration status from AES.
 *
 * Notes:
 *      Historically used for desk accessories and application menus.
 *
 * Sample call:
 *      menu_register(appl_id, "My App");
 */
WORD menu_register(WORD pid, char *pstr);

/*
 * Unregister a previously registered menu entry.
 *
 * Parameters:
 *      mid         - Menu registration id.
 *
 * Returns:
 *      Non-zero on success, otherwise zero.
 *
 * Notes:
 *      Mainly relevant in multi-entry AES environments.
 *
 * Sample call:
 *      menu_unregister(menu_id);
 */
WORD menu_unregister(WORD mid);

/*
 * Read or set the menu click-speed preference.
 *
 * Parameters:
 *      click       - Click-speed value.
 *      setit       - Non-zero to set, zero to query.
 *
 * Returns:
 *      Effective click-speed value.
 *
 * Notes:
 *      AES stores this as a UI preference.
 *
 * Sample call:
 *      menu_click(1, 1);
 */
WORD menu_click(WORD click, WORD setit);

/*
 * Add a child object beneath a parent in an object tree.
 *
 * Parameters:
 *      tree        - Object tree.
 *      parent      - Parent object index.
 *      child       - Child object index to insert.
 *
 * Returns:
 *      Non-zero on success, otherwise zero.
 *
 * Notes:
 *      Updates the classic linked-tree relationships in-place.
 *
 * Sample call:
 *      objc_add(tree, ROOT, child_id);
 */
WORD objc_add(OBJECT *tree, WORD parent, WORD child);

/*
 * Delete an object from its parent's child list.
 *
 * Parameters:
 *      tree        - Object tree.
 *      object      - Object index to remove.
 *
 * Returns:
 *      Non-zero on success, otherwise zero.
 *
 * Notes:
 *      The object remains allocated but is detached from the tree.
 *
 * Sample call:
 *      objc_delete(tree, object_id);
 */
WORD objc_delete(OBJECT *tree, WORD object);

/*
 * Draw an object tree clipped to the supplied rectangle.
 *
 * Parameters:
 *      tree        - Object tree to draw.
 *      startob     - Starting object index.
 *      depth       - Maximum traversal depth.
 *      xc,yc,wc,hc - Clip rectangle.
 *
 * Returns:
 *      Non-zero on success, otherwise zero.
 *
 * Notes:
 *      Redraw handlers commonly pass the window work rectangle here.
 *
 * Sample call:
 *      objc_draw(tree, ROOT, MAX_DEPTH, clip.g_x, clip.g_y,
 *          clip.g_w, clip.g_h);
 */
WORD objc_draw(OBJECT *tree, WORD startob, WORD depth,
               WORD xc, WORD yc, WORD wc, WORD hc);

/*
 * Find the deepest object at the given mouse coordinates.
 *
 * Parameters:
 *      tree        - Object tree to search.
 *      startob     - Starting object index.
 *      depth       - Maximum traversal depth.
 *      mx,my       - Mouse position.
 *
 * Returns:
 *      Object index found at the point, or a negative value.
 *
 * Notes:
 *      This is frequently used for hit-testing dialogs and windows.
 *
 * Sample call:
 *      WORD obj = objc_find(tree, ROOT, MAX_DEPTH, mx, my);
 */
WORD objc_find(OBJECT *tree, WORD startob, WORD depth, WORD mx, WORD my);

/*
 * Return the absolute screen position of an object.
 *
 * Parameters:
 *      tree        - Object tree.
 *      obj         - Object index.
 *      poffx,poffy - Receive absolute screen coordinates.
 *
 * Returns:
 *      Non-zero on success, otherwise zero.
 *
 * Notes:
 *      Coordinates are accumulated through the parent chain.
 *
 * Sample call:
 *      WORD x, y;
 *      objc_offset(tree, obj, &x, &y);
 */
WORD objc_offset(OBJECT *tree, WORD obj, WORD *poffx, WORD *poffy);

/*
 * Reorder an object among its siblings.
 *
 * Parameters:
 *      tree        - Object tree.
 *      object      - Object index to reorder.
 *      newpos      - New sibling position.
 *
 * Returns:
 *      Non-zero on success, otherwise zero.
 *
 * Notes:
 *      Useful for changing z-order within container objects.
 *
 * Sample call:
 *      objc_order(tree, object_id, 0);
 */
WORD objc_order(OBJECT *tree, WORD object, WORD newpos);

/*
 * Edit a TEDINFO-based object using a character input.
 *
 * Parameters:
 *      tree        - Object tree.
 *      object      - Editable object index.
 *      charidx     - Input character or edit token.
 *      idx         - In/out cursor index within the field.
 *      kind        - Edit operation selector such as `EDCHAR`.
 *
 * Returns:
 *      Non-zero on success, otherwise zero.
 *
 * Notes:
 *      This is the low-level text-edit helper behind many dialogs.
 *
 * Sample call:
 *      WORD idx = 0;
 *      objc_edit(tree, field_id, 'A', &idx, EDCHAR);
 */
WORD objc_edit(OBJECT *tree, WORD object, WORD charidx, WORD *idx,
               WORD kind);

/*
 * Change an object's state and optionally redraw it.
 *
 * Parameters:
 *      tree        - Object tree.
 *      object      - Object index to update.
 *      depth       - Maximum redraw depth.
 *      xc,yc,wc,hc - Clip rectangle for redraw.
 *      newstate    - New object state bits.
 *      redraw      - Non-zero to redraw immediately.
 *
 * Returns:
 *      Non-zero on success, otherwise zero.
 *
 * Notes:
 *      This is commonly used to set `SELECTED`, `DISABLED`, and similar
 *      state bits.
 *
 * Sample call:
 *      objc_change(tree, button_id, 0, clip.g_x, clip.g_y, clip.g_w,
 *          clip.g_h, SELECTED, 1);
 */
WORD objc_change(OBJECT *tree, WORD object, WORD depth,
                 WORD xc, WORD yc, WORD wc, WORD hc,
                 WORD newstate, WORD redraw);

/*
 * Run a dialog until an exit object is selected.
 *
 * Parameters:
 *      tree        - Dialog object tree.
 *      startob     - Initial edit object index.
 *
 * Returns:
 *      Exit object index selected by the user.
 *
 * Notes:
 *      This is the classic modal form driver.
 *
 * Sample call:
 *      WORD exit_obj = form_do(dialog_tree, 0);
 */
WORD form_do(OBJECT *tree, WORD startob);

/*
 * Animate or finalize a dialog grow/shrink operation.
 *
 * Parameters:
 *      flag        - Operation selector such as `FMD_START`.
 *      x1,y1,w1,h1 - Source rectangle.
 *      x2,y2,w2,h2 - Destination rectangle.
 *
 * Returns:
 *      Non-zero on success, otherwise zero.
 *
 * Notes:
 *      AES uses this for classic grow-box dialog transitions.
 *
 * Sample call:
 *      form_dial(FMD_START, 0, 0, 10, 10, 50, 50, 200, 120);
 */
WORD form_dial(WORD flag, WORD x1, WORD y1, WORD w1, WORD h1,
               WORD x2, WORD y2, WORD w2, WORD h2);

/*
 * Display a formatted alert box and return the selected button.
 *
 * Parameters:
 *      defbut      - Default button number.
 *      astring     - AES alert template string.
 *
 * Returns:
 *      One-based button number selected by the user.
 *
 * Notes:
 *      The string uses GEM's bracketed alert syntax.
 *
 * Sample call:
 *      WORD button = form_alert(1, "[1][Save changes?][OK|Cancel]");
 */
WORD form_alert(WORD defbut, char *astring);

/*
 * Display the standard AES error alert for an error code.
 *
 * Parameters:
 *      errnum      - AES or application error code.
 *
 * Returns:
 *      Button result from the alert.
 *
 * Notes:
 *      AES formats the error text for you.
 *
 * Sample call:
 *      form_error(2);
 */
WORD form_error(WORD errnum);

/*
 * Compute the centered rectangle for a dialog tree.
 *
 * Parameters:
 *      tree        - Dialog object tree.
 *      cx,cy,cw,ch - Receive centered rectangle.
 *
 * Returns:
 *      Non-zero on success, otherwise zero.
 *
 * Notes:
 *      Useful before opening or animating modal dialogs.
 *
 * Sample call:
 *      WORD x, y, w, h;
 *      form_center(dialog_tree, &x, &y, &w, &h);
 */
WORD form_center(OBJECT *tree, WORD *cx, WORD *cy, WORD *cw, WORD *ch);

/*
 * Process a keyboard event for a form and compute the next object.
 *
 * Parameters:
 *      tree        - Dialog object tree.
 *      object      - Current object index.
 *      next        - Current next-object hint.
 *      thechar     - Input key value.
 *      newobj      - Receives the next object index.
 *      newchar     - Receives the translated key value.
 *
 * Returns:
 *      Non-zero on success, otherwise zero.
 *
 * Notes:
 *      This is the low-level keyboard half of custom dialog driving.
 *
 * Sample call:
 *      WORD next_obj, next_char;
 *      form_keybd(tree, obj, obj, key, &next_obj, &next_char);
 */
WORD form_keybd(OBJECT *tree, WORD object, WORD next,
                WORD thechar, WORD *newobj, WORD *newchar);

/*
 * Process a button click for a form and compute the next object.
 *
 * Parameters:
 *      tree        - Dialog object tree.
 *      object      - Clicked object index.
 *      clicks      - Click count.
 *      newobj      - Receives the resulting object index.
 *
 * Returns:
 *      Non-zero on success, otherwise zero.
 *
 * Notes:
 *      This is the low-level button half of custom dialog driving.
 *
 * Sample call:
 *      WORD next_obj;
 *      form_button(tree, obj, 1, &next_obj);
 */
WORD form_button(OBJECT *tree, WORD object, WORD clicks, WORD *newobj);

/*
 * Let the user stretch a rubber box from an origin.
 *
 * Parameters:
 *      xorigin,yorigin
 *                  - Drag origin.
 *      wmin,hmin   - Minimum box size.
 *      pwend,phend - Receive final box width and height.
 *
 * Returns:
 *      Non-zero on success, otherwise zero.
 *
 * Notes:
 *      Commonly used for interactive selection rectangles.
 *
 * Sample call:
 *      WORD w, h;
 *      graf_rubbox(10, 10, 16, 16, &w, &h);
 */
WORD graf_rubbox(WORD xorigin, WORD yorigin, WORD wmin, WORD hmin,
                 WORD *pwend, WORD *phend);

#define graf_rubberbox graf_rubbox  /* Historical alias for graf_rubbox(). */

/*
 * Let the user drag a box within a bounding rectangle.
 *
 * Parameters:
 *      w,h         - Box size.
 *      sx,sy       - Start position.
 *      xc,yc,wc,hc - Bounding rectangle.
 *      pdx,pdy     - Receive final position.
 *
 * Returns:
 *      Non-zero on success, otherwise zero.
 *
 * Notes:
 *      Often used for dragging windows or icons.
 *
 * Sample call:
 *      WORD dx, dy;
 *      graf_dragbox(32, 32, 10, 10, 0, 0, 320, 200, &dx, &dy);
 */
WORD graf_dragbox(WORD w, WORD h, WORD sx, WORD sy,
                  WORD xc, WORD yc, WORD wc, WORD hc,
                  WORD *pdx, WORD *pdy);

/*
 * Move a box visually from one rectangle to another.
 *
 * Parameters:
 *      w,h         - Box size.
 *      srcx,srcy   - Source position.
 *      dstx,dsty   - Destination position.
 *
 * Returns:
 *      Non-zero on success, otherwise zero.
 *
 * Notes:
 *      Provides the classic GEM moving-box animation.
 *
 * Sample call:
 *      graf_mbox(20, 20, 0, 0, 100, 100);
 */
WORD graf_mbox(WORD w, WORD h, WORD srcx, WORD srcy, WORD dstx, WORD dsty);

#define graf_movebox graf_mbox  /* Historical alias for graf_mbox(). */

/*
 * Animate a growing box.
 *
 * Parameters:
 *      x1,y1,w1,h1 - Start rectangle.
 *      x2,y2,w2,h2 - End rectangle.
 *
 * Returns:
 *      Non-zero on success, otherwise zero.
 *
 * Notes:
 *      Used by classic dialog open animations.
 *
 * Sample call:
 *      graf_growbox(0, 0, 10, 10, 40, 40, 200, 120);
 */
WORD graf_growbox(WORD x1, WORD y1, WORD w1, WORD h1,
                  WORD x2, WORD y2, WORD w2, WORD h2);

/*
 * Animate a shrinking box.
 *
 * Parameters:
 *      x1,y1,w1,h1 - Start rectangle.
 *      x2,y2,w2,h2 - End rectangle.
 *
 * Returns:
 *      Non-zero on success, otherwise zero.
 *
 * Notes:
 *      Used by classic dialog close animations.
 *
 * Sample call:
 *      graf_shrinkbox(40, 40, 200, 120, 0, 0, 10, 10);
 */
WORD graf_shrinkbox(WORD x1, WORD y1, WORD w1, WORD h1,
                    WORD x2, WORD y2, WORD w2, WORD h2);

/*
 * Track an object while the user holds the mouse button.
 *
 * Parameters:
 *      tree        - Object tree.
 *      object      - Object index to watch.
 *      in_state    - State while pressed inside.
 *      out_state   - State while pressed outside.
 *
 * Returns:
 *      Non-zero on success, otherwise zero.
 *
 * Notes:
 *      Often used for custom button press feedback.
 *
 * Sample call:
 *      graf_watchbox(tree, button_id, SELECTED, NORMAL);
 */
WORD graf_watchbox(OBJECT *tree, WORD object, UWORD in_state,
                   UWORD out_state);

/*
 * Return a slider position relative to its parent elevator range.
 *
 * Parameters:
 *      tree        - Object tree.
 *      parent      - Slider parent object.
 *      object      - Slider elevator object.
 *      orientation - Horizontal or vertical selector.
 *
 * Returns:
 *      Relative slider position.
 *
 * Notes:
 *      AES computes a normalized position within the slider track.
 *
 * Sample call:
 *      WORD pos = graf_slidebox(tree, parent_id, slider_id, 0);
 */
WORD graf_slidebox(OBJECT *tree, WORD parent, WORD object,
                   WORD orientation);

/*
 * Query character and box metrics for the current workstation.
 *
 * Parameters:
 *      pwchar      - Receives character width.
 *      phchar      - Receives character height.
 *      pwbox       - Receives box width.
 *      phbox       - Receives box height.
 *
 * Returns:
 *      VDI handle associated with AES drawing.
 *
 * Notes:
 *      This is one of the standard ways to obtain the AES VDI handle.
 *
 * Sample call:
 *      WORD cw, ch, bw, bh;
 *      WORD handle = graf_handle(&cw, &ch, &bw, &bh);
 */
WORD graf_handle(WORD *pwchar, WORD *phchar, WORD *pwbox, WORD *phbox);

/*
 * Change or hide the AES mouse cursor.
 *
 * Parameters:
 *      m_number    - Cursor selector such as `ARROW` or `M_OFF`.
 *      m_addr      - Optional pointer to a user-defined form.
 *
 * Returns:
 *      Non-zero on success, otherwise zero.
 *
 * Notes:
 *      `USER_DEF` expects `m_addr` to point at an `MFORM`.
 *
 * Sample call:
 *      graf_mouse(ARROW, NULL);
 */
WORD graf_mouse(WORD m_number, void *m_addr);

/*
 * Query the current mouse position, button state, and key state.
 *
 * Parameters:
 *      pmx,pmy     - Receive mouse position.
 *      pmstate     - Receives button state.
 *      pkstate     - Receives keyboard shift state.
 *
 * Returns:
 *      Nothing.
 *
 * Notes:
 *      This is a snapshot query, not an event wait.
 *
 * Sample call:
 *      WORD mx, my, buttons, keys;
 *      graf_mkstate(&mx, &my, &buttons, &keys);
 */
VOID graf_mkstate(WORD *pmx, WORD *pmy, WORD *pmstate, WORD *pkstate);

/*
 * Read the current scrap directory path.
 *
 * Parameters:
 *      pscrap      - Receives the scrap path string.
 *
 * Returns:
 *      Non-zero on success, otherwise zero.
 *
 * Notes:
 *      The scrap directory is the historical GEM clipboard location.
 *
 * Sample call:
 *      char path[128];
 *      scrp_read(path);
 */
WORD scrp_read(char *pscrap);

/*
 * Write the current scrap directory path.
 *
 * Parameters:
 *      pscrap      - New scrap directory path.
 *
 * Returns:
 *      Non-zero on success, otherwise zero.
 *
 * Notes:
 *      This changes the global clipboard directory for AES clients.
 *
 * Sample call:
 *      scrp_write("C:\\SCRAP\\");
 */
WORD scrp_write(char *pscrap);

/*
 * Clear the historical scrap directory contents.
 *
 * Parameters:
 *      None.
 *
 * Returns:
 *      Non-zero on success, otherwise zero.
 *
 * Notes:
 *      The exact effect depends on the hosted AES implementation.
 *
 * Sample call:
 *      scrp_clear();
 */
WORD scrp_clear(void);

/*
 * Display the file selector and return the chosen path, file, and
 * button.
 *
 * Parameters:
 *      pipath      - In/out path buffer.
 *      pisel       - In/out filename buffer.
 *      pbutton     - Receives button result.
 *
 * Returns:
 *      Non-zero on success, otherwise zero.
 *
 * Notes:
 *      Historically AES updates both the directory and selection text.
 *
 * Sample call:
 *      char path[128] = "A:\\*.*";
 *      char file[14] = "";
 *      WORD button;
 *      fsel_input(path, file, &button);
 */
WORD fsel_input(char *pipath, char *pisel, WORD *pbutton);

/*
 * Create a window with the requested border elements.
 *
 * Parameters:
 *      kind        - Window element flags such as `NAME | CLOSER`.
 *      wx,wy,ww,wh - Initial rectangle.
 *
 * Returns:
 *      Window handle on success, or a negative value on failure.
 *
 * Notes:
 *      Creation allocates AES-side window state but does not show it.
 *
 * Sample call:
 *      WORD win = wind_create(NAME | CLOSER, 0, 0, 200, 120);
 */
WORD wind_create(UWORD kind, WORD wx, WORD wy, WORD ww, WORD wh);

/*
 * Open a window at the requested position and size.
 *
 * Parameters:
 *      handle      - Window handle.
 *      wx,wy,ww,wh - Rectangle to open at.
 *
 * Returns:
 *      Non-zero on success, otherwise zero.
 *
 * Notes:
 *      The window must already exist.
 *
 * Sample call:
 *      wind_open(win, 20, 20, 320, 200);
 */
WORD wind_open(WORD handle, WORD wx, WORD wy, WORD ww, WORD wh);

/*
 * Close a window.
 *
 * Parameters:
 *      handle      - Window handle.
 *
 * Returns:
 *      Non-zero on success, otherwise zero.
 *
 * Notes:
 *      Closing hides the window but does not delete it.
 *
 * Sample call:
 *      wind_close(win);
 */
WORD wind_close(WORD handle);

/*
 * Delete a window and free its AES state.
 *
 * Parameters:
 *      handle      - Window handle.
 *
 * Returns:
 *      Non-zero on success, otherwise zero.
 *
 * Notes:
 *      Delete after closing when the window is no longer needed.
 *
 * Sample call:
 *      wind_delete(win);
 */
WORD wind_delete(WORD handle);

/*
 * Read a window field or geometry tuple.
 *
 * Parameters:
 *      handle      - Window handle.
 *      field       - Field selector such as `WF_WXYWH`.
 *      w1,w2,w3,w4 - Receive field values.
 *
 * Returns:
 *      Non-zero on success, otherwise zero.
 *
 * Notes:
 *      Geometry fields return position and size values in the four
 *      output WORDs.
 *
 * Sample call:
 *      WORD x, y, w, h;
 *      wind_get(win, WF_WXYWH, &x, &y, &w, &h);
 */
WORD wind_get(WORD handle, WORD field, WORD *w1, WORD *w2, WORD *w3,
              WORD *w4);

/*
 * Set a window field or geometry tuple.
 *
 * Parameters:
 *      handle      - Window handle.
 *      field       - Field selector such as `WF_NAME` or `WF_TOP`.
 *      w1,w2,w3,w4 - Field values.
 *
 * Returns:
 *      Non-zero on success, otherwise zero.
 *
 * Notes:
 *      For pointer-based strings, prefer `wind_set_str()`.
 *
 * Sample call:
 *      wind_set(win, WF_TOP, 0, 0, 0, 0);
 */
WORD wind_set(WORD handle, WORD field, WORD w1, WORD w2, WORD w3,
              WORD w4);

/*
 * Set a pointer-based window string field such as `WF_NAME` or
 * `WF_INFO` without truncating the host pointer value.
 *
 * Parameters:
 *      handle      - Window handle.
 *      field       - String field selector such as `WF_NAME`.
 *      text        - String pointer to store.
 *
 * Returns:
 *      Non-zero on success, otherwise zero.
 *
 * Notes:
 *      Prefer this over `wind_set()` for hosted pointer-sized strings.
 *
 * Sample call:
 *      wind_set_str(win, WF_NAME, "Project");
 */
WORD wind_set_str(WORD handle, WORD field, const char *text);

/*
 * Return the topmost window containing a screen point.
 *
 * Parameters:
 *      mx,my       - Screen point to test.
 *
 * Returns:
 *      Window handle under the point, or zero/negative if none.
 *
 * Notes:
 *      Useful for click-to-window hit testing.
 *
 * Sample call:
 *      WORD win = wind_find(mx, my);
 */
WORD wind_find(WORD mx, WORD my);

/*
 * Bracket a period of window-system updating.
 *
 * Parameters:
 *      beg_update  - `BEG_UPDATE` or `END_UPDATE`.
 *
 * Returns:
 *      Non-zero on success, otherwise zero.
 *
 * Notes:
 *      Use this around redraw-sensitive sequences.
 *
 * Sample call:
 *      wind_update(BEG_UPDATE);
 */
WORD wind_update(WORD beg_update);

/*
 * Convert between border and work-area rectangles.
 *
 * Parameters:
 *      type        - `WC_BORDER` or `WC_WORK`.
 *      kind        - Window kind flags.
 *      inx,iny,inw,inh
 *                  - Input rectangle.
 *      outx,outy,outw,outh
 *                  - Receive converted rectangle.
 *
 * Returns:
 *      Non-zero on success, otherwise zero.
 *
 * Notes:
 *      Used to compute outer border from work area and vice versa.
 *
 * Sample call:
 *      WORD x, y, w, h;
 *      wind_calc(WC_BORDER, NAME | CLOSER, 0, 0, 320, 200,
 *          &x, &y, &w, &h);
 */
WORD wind_calc(WORD type, UWORD kind,
               WORD inx, WORD iny, WORD inw, WORD inh,
               WORD *outx, WORD *outy, WORD *outw, WORD *outh);

/*
 * Load a compiled AES resource file.
 *
 * Parameters:
 *      filename    - Resource file path.
 *
 * Returns:
 *      Non-zero on success, otherwise zero.
 *
 * Notes:
 *      After loading, trees and strings can be queried with
 *      `rsrc_gaddr()`.
 *
 * Sample call:
 *      rsrc_load("desk.rsc");
 */
WORD rsrc_load(char *filename);

/*
 * Free the currently loaded AES resource file.
 *
 * Parameters:
 *      None.
 *
 * Returns:
 *      Non-zero on success, otherwise zero.
 *
 * Notes:
 *      Invalidates previously returned resource addresses.
 *
 * Sample call:
 *      rsrc_free();
 */
WORD rsrc_free(void);

/*
 * Look up an address inside the loaded AES resource file.
 *
 * Parameters:
 *      type        - Resource type selector.
 *      index       - Resource index.
 *      addr        - Receives the resolved address.
 *
 * Returns:
 *      Non-zero on success, otherwise zero.
 *
 * Notes:
 *      Commonly used to fetch trees, strings, icons, and images.
 *
 * Sample call:
 *      OBJECT *tree;
 *      rsrc_gaddr(R_TREE, 0, (void **) &tree);
 */
WORD rsrc_gaddr(WORD type, WORD index, void **addr);

/*
 * Replace an address inside the loaded AES resource file.
 *
 * Parameters:
 *      type        - Resource type selector.
 *      index       - Resource index.
 *      addr        - Replacement address.
 *
 * Returns:
 *      Non-zero on success, otherwise zero.
 *
 * Notes:
 *      Useful for patching indirect pointers after load.
 *
 * Sample call:
 *      rsrc_saddr(R_STRING, 0, my_string);
 */
WORD rsrc_saddr(WORD type, WORD index, void *addr);

/*
 * Fix up an object tree after loading a resource file.
 *
 * Parameters:
 *      tree        - Object tree.
 *      obj         - Object index to fix.
 *
 * Returns:
 *      Non-zero on success, otherwise zero.
 *
 * Notes:
 *      Historically converts resource coordinates and pointers as
 *      needed.
 *
 * Sample call:
 *      rsrc_obfix(tree, ROOT);
 */
WORD rsrc_obfix(OBJECT *tree, WORD obj);

/*
 * Read the shell command tail and command path for the current launch.
 *
 * Parameters:
 *      cmd         - Receives command path.
 *      tail        - Receives command tail.
 *
 * Returns:
 *      Non-zero on success, otherwise zero.
 *
 * Notes:
 *      Used by applications that need startup command-line context.
 *
 * Sample call:
 *      char cmd[128], tail[128];
 *      shel_read(cmd, tail);
 */
WORD shel_read(char *cmd, char *tail);

/*
 * Request that AES launch or switch to another application.
 *
 * Parameters:
 *      doex        - Execute mode selector.
 *      isgr        - Graphics-mode flag.
 *      iscr        - Screen mode flag.
 *      cmd         - Command path.
 *      tail        - Command tail.
 *
 * Returns:
 *      Non-zero on success, otherwise zero.
 *
 * Notes:
 *      Semantics vary across GEM versions and multitasking setups.
 *
 * Sample call:
 *      shel_write(1, 1, 0, cmd, tail);
 */
WORD shel_write(WORD doex, WORD isgr, WORD iscr, char *cmd, char *tail);

/*
 * Read bytes from the AES shell buffer.
 *
 * Parameters:
 *      buf         - Destination buffer.
 *      length      - Number of bytes to read.
 *
 * Returns:
 *      Non-zero on success, otherwise zero.
 *
 * Notes:
 *      This is a low-level shell buffer accessor.
 *
 * Sample call:
 *      shel_get(buf, sizeof(buf));
 */
WORD shel_get(char *buf, WORD length);

/*
 * Write bytes to the AES shell buffer.
 *
 * Parameters:
 *      buf         - Source buffer.
 *      length      - Number of bytes to write.
 *
 * Returns:
 *      Non-zero on success, otherwise zero.
 *
 * Notes:
 *      This is a low-level shell buffer accessor.
 *
 * Sample call:
 *      shel_put(buf, len);
 */
WORD shel_put(char *buf, WORD length);

/*
 * Resolve an executable path using the AES shell search rules.
 *
 * Parameters:
 *      path        - In/out executable path string.
 *
 * Returns:
 *      Non-zero on success, otherwise zero.
 *
 * Notes:
 *      AES may rewrite the path buffer with a resolved location.
 *
 * Sample call:
 *      shel_find(path);
 */
WORD shel_find(char *path);

/*
 * Search the environment for a named variable.
 *
 * Parameters:
 *      env         - Receives pointer to the variable value.
 *      var         - Variable name to search for.
 *
 * Returns:
 *      Non-zero on success, otherwise zero.
 *
 * Notes:
 *      The returned pointer typically refers into the environment block.
 *
 * Sample call:
 *      char *value;
 *      shel_envrn(&value, "PATH=");
 */
WORD shel_envrn(char **env, char *var);

/*
 * Read the default shell application and directory.
 *
 * Parameters:
 *      lpcmd       - Receives default shell command.
 *      lpdir       - Receives default shell directory.
 *
 * Returns:
 *      Non-zero on success, otherwise zero.
 *
 * Notes:
 *      This is useful for desktop or launcher-style tools.
 *
 * Sample call:
 *      char cmd[128], dir[128];
 *      shel_rdef(cmd, dir);
 */
WORD shel_rdef(char *lpcmd, char *lpdir);

/*
 * Write the default shell application and directory.
 *
 * Parameters:
 *      lpcmd       - New default shell command.
 *      lpdir       - New default shell directory.
 *
 * Returns:
 *      Non-zero on success, otherwise zero.
 *
 * Notes:
 *      This updates the AES shell defaults for future launches.
 *
 * Sample call:
 *      shel_wdef(cmd, dir);
 */
WORD shel_wdef(char *lpcmd, char *lpdir);

#ifdef __cplusplus
}
#endif

#endif /* GEM_AES_H */
