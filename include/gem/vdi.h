/*
 * Minimal GEM VDI API for the Linux/rasta port.
 *
 * This is not a full historical VDI implementation. It is the first
 * compatibility subset needed to bring up simple GEM applications on top
 * of the platform raster/hid/os abstraction.
 */

#ifndef GEM_VDI_H
#define GEM_VDI_H

#include <gem/portab.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef WORD VDI_HANDLE;

/*
 * Memory form definition block.
 *
 * If fd_addr is NULL, the MFDB refers to the physical screen.
 * Otherwise it refers to an off-screen bitmap.
 */
typedef struct mfdb {
    VOID *fd_addr;      /* pixel data, or NULL for screen */
    WORD  fd_w;         /* width in pixels */
    WORD  fd_h;         /* height in pixels */
    WORD  fd_wdwidth;   /* width in 16-bit words */
    WORD  fd_stand;     /* standard format flag */
    WORD  fd_nplanes;   /* number of bitplanes */
    WORD  fd_r1;        /* reserved */
    WORD  fd_r2;        /* reserved */
    WORD  fd_r3;        /* reserved */
} MFDB;

/*
 * Open a virtual workstation.
 *
 * work_in has 11 entries. For the minimal port most values may be ignored.
 * Returns a non-zero handle and fills work_out with screen capabilities.
 */
VOID v_opnvwk(WORD work_in[11], VDI_HANDLE *handle, WORD work_out[57]);

/*
 * Close a virtual workstation.
 */
VOID v_clsvwk(VDI_HANDLE handle);

/*
 * Clear the workstation to the current background color.
 */
VOID v_clrwk(VDI_HANDLE handle);

/*
 * Draw a connected polyline.
 *
 * pxy contains count points as x,y pairs:
 *   pxy[0], pxy[1] = first point
 *   pxy[2], pxy[3] = second point
 */
VOID v_pline(VDI_HANDLE handle, WORD count, CONST WORD *pxy);

/*
 * Draw a filled rectangle.
 *
 * pxy = { x0, y0, x1, y1 }.
 * Coordinates are inclusive GEM/VDI coordinates.
 */
VOID v_bar(VDI_HANDLE handle, CONST WORD pxy[4]);

/*
 * Draw text at baseline position x,y.
 *
 * Initial implementation may use a fixed 8x8 or 8x16 bitmap font.
 */
VOID v_gtext(VDI_HANDLE handle, WORD x, WORD y, CONST BYTE *text);

/*
 * Set text color by VDI color index.
 */
VOID vst_color(VDI_HANDLE handle, WORD color_index);

/*
 * Set fill color by VDI color index.
 */
VOID vsf_color(VDI_HANDLE handle, WORD color_index);

/*
 * Set line color by VDI color index.
 */
VOID vsl_color(VDI_HANDLE handle, WORD color_index);

/*
 * Hide mouse cursor.
 *
 * Minimal implementation may keep a hide counter.
 */
VOID v_hide_c(VDI_HANDLE handle);

/*
 * Show mouse cursor.
 *
 * If reset is non-zero, reset hide counter and force cursor visible.
 */
VOID v_show_c(VDI_HANDLE handle, WORD reset);

/*
 * Query current mouse state.
 *
 * status receives button bitmask.
 * x and y receive current mouse coordinates.
 */
VOID vq_mouse(VDI_HANDLE handle, WORD *status, WORD *x, WORD *y);

/*
 * Request a string from keyboard input.
 *
 * max_length is maximum number of characters excluding final NUL.
 * echo_mode controls whether typed characters are echoed.
 * echo_xy optionally contains echo position {x,y}.
 * out_string receives a NUL-terminated string.
 *
 * Minimal implementation may block until Enter.
 */
VOID vrq_string(VDI_HANDLE handle,
                WORD max_length,
                WORD echo_mode,
                WORD *echo_xy,
                BYTE *out_string);

/*
 * Raster copy between memory forms.
 *
 * pxy contains eight coordinates:
 *   source x0,y0,x1,y1,destination x0,y0,x1,y1
 *
 * mode is the VDI raster operation mode. Minimal implementation should
 * support copy/replace first.
 */
VOID vro_cpyfm(VDI_HANDLE handle,
               WORD mode,
               CONST WORD pxy[8],
               MFDB *src,
               MFDB *dst);

/*
 * Transparent raster copy between memory forms.
 *
 * Same coordinate layout as vro_cpyfm.
 * colors[0] is background color index.
 * colors[1] is foreground color index.
 *
 * Minimal implementation may initially support monochrome source to
 * screen destination only.
 */
VOID vrt_cpyfm(VDI_HANDLE handle,
               WORD mode,
               CONST WORD pxy[8],
               MFDB *src,
               MFDB *dst,
               CONST WORD colors[2]);

#ifdef __cplusplus
}
#endif

#endif /* GEM_VDI_H */