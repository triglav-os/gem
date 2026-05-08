/*
 * Declares the public GEM VDI interface used by the Linux/rasta port.
 * The header carries the broad historical GEM VDI surface from the
 * programmer's guide while adapting it to the current hosted
 * workstation backend.
 *
 * MIT License (see: LICENSE)
 * Copyright (C) 2026 tomaz stih
 */

#ifndef GEM_VDI_H
#define GEM_VDI_H

#include <gem/portab.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef WORD VDI_HANDLE;  /* VDI workstation handle type. */

/*
 * Memory form definition block.
 *
 * If fd_addr is NULL, the MFDB refers to the physical screen.
 * Otherwise it refers to an off-screen bitmap.
 */
typedef struct mfdb {
    VOID *fd_addr;      /* Base pixel pointer, or NULL for the screen. */
    WORD  fd_w;         /* Bitmap width in pixels. */
    WORD  fd_h;         /* Bitmap height in pixels. */
    WORD  fd_wdwidth;   /* Scanline width in 16-bit words. */
    WORD  fd_stand;     /* Non-zero when data is in standard MFDB layout. */
    WORD  fd_nplanes;   /* Number of bitplanes in the bitmap. */
    WORD  fd_r1;        /* Reserved for GEM compatibility. */
    WORD  fd_r2;        /* Reserved for GEM compatibility. */
    WORD  fd_r3;        /* Reserved for GEM compatibility. */
} MFDB;

/*
 * Mouse form used by vsc_form().
 */
typedef struct mform {
    WORD mf_xhot;       /* Horizontal hot-spot position within the cursor. */
    WORD mf_yhot;       /* Vertical hot-spot position within the cursor. */
    WORD mf_nplanes;    /* Number of bitplanes in the cursor image. */
    WORD mf_fg;         /* Foreground color index. */
    WORD mf_bg;         /* Background color index. */
    WORD mf_mask[16];   /* Per-row transparency and shape mask words. */
    WORD mf_data[16];   /* Per-row cursor bitmap data words. */
} MFORM;

/*
 * Open a physical workstation.
 *
 * Parameters:
 *      work_in     - Classic VDI input array.
 *      handle      - Receives the opened workstation handle.
 *      work_out    - Receives workstation capability data.
 *
 * Returns:
 *      Opened handle on success, or zero on failure.
 *
 * Notes:
 *      This hosted port maps the physical and virtual workstation
 *      concepts onto the same screen-backed implementation.
 *
 * Sample call:
 *      WORD work_in[11] = {0};
 *      WORD work_out[57];
 *      WORD handle = 0;
 *      handle = v_opnwk(work_in, &handle, work_out);
 */
WORD v_opnwk(WORD work_in[], WORD *handle, WORD work_out[]);

/*
 * Open a virtual workstation.
 *
 * Parameters:
 *      work_in     - Eleven-entry VDI input array.
 *      handle      - Receives the opened workstation handle.
 *      work_out    - Receives workstation capability data.
 *
 * Returns:
 *      Nothing.
 *
 * Notes:
 *      On success, `*handle` becomes non-zero. The hosted port uses a
 *      single screen workstation and ignores unsupported attributes.
 *
 * Sample call:
 *      WORD work_in[11] = {0};
 *      WORD work_out[57];
 *      VDI_HANDLE handle = 0;
 *      v_opnvwk(work_in, &handle, work_out);
 */
VOID v_opnvwk(WORD work_in[11], VDI_HANDLE *handle, WORD work_out[57]);

/*
 * Open a raster workstation.
 *
 * Parameters:
 *      work_in     - Classic VDI input array.
 *      handle      - Receives the opened workstation handle.
 *      work_out    - Receives workstation capability data.
 *
 * Returns:
 *      Opened handle on success, or zero on failure.
 *
 * Notes:
 *      Historical GEM uses this for raster-oriented workstations. This
 *      port aliases it to the normal workstation open path.
 *
 * Sample call:
 *      WORD work_in[11] = {0};
 *      WORD work_out[57];
 *      WORD handle = 0;
 *      handle = v_opnrwk(work_in, &handle, work_out);
 */
WORD v_opnrwk(WORD work_in[], WORD *handle, WORD work_out[]);

/*
 * Close a physical workstation.
 *
 * Parameters:
 *      handle      - Workstation handle returned by v_opnwk().
 *
 * Returns:
 *      Non-zero if the handle was valid, otherwise zero.
 *
 * Notes:
 *      In this port, closing the physical workstation tears down the
 *      hosted VDI backend and input services.
 *
 * Sample call:
 *      v_clswk(handle);
 */
WORD v_clswk(WORD handle);

/*
 * Close a virtual workstation.
 *
 * Parameters:
 *      handle      - Workstation handle returned by v_opnvwk().
 *
 * Returns:
 *      Nothing.
 *
 * Notes:
 *      Invalid handles are ignored.
 *
 * Sample call:
 *      v_clsvwk(handle);
 */
VOID v_clsvwk(VDI_HANDLE handle);

/*
 * Clear the workstation to the current background color.
 *
 * Parameters:
 *      handle      - Target workstation handle.
 *
 * Returns:
 *      Nothing.
 *
 * Notes:
 *      The hosted implementation clears the full screen surface and
 *      presents the result immediately.
 *
 * Sample call:
 *      v_clrwk(handle);
 */
VOID v_clrwk(VDI_HANDLE handle);

/*
 * Update the workstation display.
 *
 * Parameters:
 *      handle      - Target workstation handle.
 *
 * Returns:
 *      Non-zero on success, otherwise zero.
 *
 * Notes:
 *      Use this when batching changes in code that wants an explicit
 *      "flush" style call.
 *
 * Sample call:
 *      v_updwk(handle);
 */
WORD v_updwk(WORD handle);

/*
 * Enable or disable clipping.
 *
 * Parameters:
 *      handle      - Target workstation handle.
 *      clip_flag   - Non-zero to enable clipping, zero to disable it.
 *      xy          - Inclusive clip rectangle {x0, y0, x1, y1}.
 *
 * Returns:
 *      Nothing.
 *
 * Notes:
 *      The active clip rectangle affects most drawing primitives.
 *
 * Sample call:
 *      WORD clip[4] = {10, 10, 200, 120};
 *      vs_clip(handle, 1, clip);
 */
VOID vs_clip(WORD handle, WORD clip_flag, WORD xy[4]);

/*
 * Draw a connected polyline.
 *
 * Parameters:
 *      handle      - Target workstation handle.
 *      count       - Number of points in `pxy`.
 *      pxy         - Point array {x0, y0, x1, y1, ...}.
 *
 * Returns:
 *      Nothing.
 *
 * Notes:
 *      Each adjacent point pair forms one line segment. Segments are
 *      clipped against the current clip rectangle.
 *
 * Sample call:
 *      WORD pts[6] = {10, 10, 100, 10, 100, 60};
 *      v_pline(handle, 3, pts);
 */
VOID v_pline(VDI_HANDLE handle, WORD count, CONST WORD *pxy);

/*
 * Draw polymarkers.
 *
 * Parameters:
 *      handle      - Target workstation handle.
 *      count       - Number of marker positions.
 *      xy          - Point array {x0, y0, x1, y1, ...}.
 *
 * Returns:
 *      Nothing.
 *
 * Notes:
 *      The hosted implementation uses a simple cross marker.
 *
 * Sample call:
 *      WORD pts[4] = {40, 30, 90, 70};
 *      v_pmarker(handle, 2, pts);
 */
VOID v_pmarker(WORD handle, WORD count, WORD xy[]);

/*
 * Draw text at a baseline position.
 *
 * Parameters:
 *      handle      - Target workstation handle.
 *      x,y         - Baseline origin of the first glyph.
 *      text        - NUL-terminated string to draw.
 *
 * Returns:
 *      Nothing.
 *
 * Notes:
 *      Text uses the currently selected font, color, and background
 *      mode.
 *
 * Sample call:
 *      v_gtext(handle, 20, 40, (CONST BYTE *) "Hello");
 */
VOID v_gtext(VDI_HANDLE handle, WORD x, WORD y, CONST BYTE *text);

/*
 * Draw a filled polygonal area.
 *
 * Parameters:
 *      handle      - Target workstation handle.
 *      count       - Number of polygon vertices.
 *      xy          - Point array {x0, y0, x1, y1, ...}.
 *
 * Returns:
 *      Nothing.
 *
 * Notes:
 *      When fill perimeter visibility is enabled, the outline is drawn
 *      after the fill.
 *
 * Sample call:
 *      WORD poly[8] = {20, 20, 80, 20, 90, 60, 10, 60};
 *      v_fillarea(handle, 4, poly);
 */
VOID v_fillarea(WORD handle, WORD count, WORD xy[]);

/*
 * Draw a cell array.
 *
 * Parameters:
 *      handle      - Target workstation handle.
 *      xy          - Destination rectangle {x0, y0, x1, y1}.
 *      row_length  - Source row length in elements.
 *      el_per_row  - Number of elements per row to draw.
 *      num_rows    - Number of rows to draw.
 *      wr_mode     - VDI write mode.
 *      colors      - Cell color indices in row-major order.
 *
 * Returns:
 *      Non-zero on success, otherwise zero.
 *
 * Notes:
 *      The hosted implementation treats non-zero cells as lit pixels.
 *
 * Sample call:
 *      WORD rect[4] = {0, 0, 31, 31};
 *      WORD cells[4] = {0, 1, 1, 0};
 *      v_cellarray(handle, rect, 2, 2, 2, 1, cells);
 */
WORD v_cellarray(WORD handle, WORD xy[], WORD row_length,
                 WORD el_per_row, WORD num_rows, WORD wr_mode,
                 WORD colors[]);

/*
 * Draw a filled rectangle.
 *
 * Parameters:
 *      handle      - Target workstation handle.
 *      pxy         - Inclusive rectangle {x0, y0, x1, y1}.
 *
 * Returns:
 *      Nothing.
 *
 * Notes:
 *      Uses the current fill color and write mode.
 *
 * Sample call:
 *      WORD box[4] = {10, 10, 50, 30};
 *      v_bar(handle, box);
 */
VOID v_bar(VDI_HANDLE handle, CONST WORD pxy[4]);

/*
 * Draw a circular arc.
 *
 * Parameters:
 *      handle      - Target workstation handle.
 *      x,y         - Circle center.
 *      radius      - Circle radius in pixels.
 *      begang      - Start angle in tenths of degrees.
 *      endang      - End angle in tenths of degrees.
 *
 * Returns:
 *      Nothing.
 *
 * Notes:
 *      Angles follow GEM VDI conventions, not radians.
 *
 * Sample call:
 *      v_arc(handle, 80, 80, 30, 0, 900);
 */
VOID v_arc(WORD handle, WORD x, WORD y, WORD radius, WORD begang,
           WORD endang);

/*
 * Draw a circular pie slice.
 *
 * Parameters:
 *      handle      - Target workstation handle.
 *      x,y         - Circle center.
 *      radius      - Circle radius in pixels.
 *      begang      - Start angle in tenths of degrees.
 *      endang      - End angle in tenths of degrees.
 *
 * Returns:
 *      Nothing.
 *
 * Notes:
 *      Fills the wedge and draws its outline.
 *
 * Sample call:
 *      v_pieslice(handle, 80, 80, 30, 0, 900);
 */
VOID v_pieslice(WORD handle, WORD x, WORD y, WORD radius, WORD begang,
                WORD endang);

/*
 * Draw a circular pie slice using the alias name.
 *
 * Parameters:
 *      handle      - Target workstation handle.
 *      x,y         - Circle center.
 *      radius      - Circle radius in pixels.
 *      begang      - Start angle in tenths of degrees.
 *      endang      - End angle in tenths of degrees.
 *
 * Returns:
 *      Nothing.
 *
 * Notes:
 *      This is a compatibility alias for v_pieslice().
 *
 * Sample call:
 *      v_pie(handle, 80, 80, 30, 900, 1800);
 */
VOID v_pie(WORD handle, WORD x, WORD y, WORD radius, WORD begang,
           WORD endang);

/*
 * Draw a circle.
 *
 * Parameters:
 *      handle      - Target workstation handle.
 *      x,y         - Circle center.
 *      radius      - Circle radius in pixels.
 *
 * Returns:
 *      Nothing.
 *
 * Notes:
 *      Uses the current line color.
 *
 * Sample call:
 *      v_circle(handle, 60, 60, 20);
 */
VOID v_circle(WORD handle, WORD x, WORD y, WORD radius);

/*
 * Draw an ellipse.
 *
 * Parameters:
 *      handle      - Target workstation handle.
 *      x,y         - Ellipse center.
 *      xrad,yrad   - Horizontal and vertical radii.
 *
 * Returns:
 *      Nothing.
 *
 * Notes:
 *      Uses the current line color.
 *
 * Sample call:
 *      v_ellipse(handle, 100, 70, 40, 20);
 */
VOID v_ellipse(WORD handle, WORD x, WORD y, WORD xrad, WORD yrad);

/*
 * Draw an elliptical arc.
 *
 * Parameters:
 *      handle      - Target workstation handle.
 *      x,y         - Ellipse center.
 *      xrad,yrad   - Horizontal and vertical radii.
 *      begang      - Start angle in tenths of degrees.
 *      endang      - End angle in tenths of degrees.
 *
 * Returns:
 *      Nothing.
 *
 * Notes:
 *      Uses GEM's angle convention and the current line color.
 *
 * Sample call:
 *      v_ellarc(handle, 100, 70, 40, 20, 450, 1350);
 */
VOID v_ellarc(WORD handle, WORD x, WORD y, WORD xrad, WORD yrad,
              WORD begang, WORD endang);

/*
 * Draw an elliptical pie slice.
 *
 * Parameters:
 *      handle      - Target workstation handle.
 *      x,y         - Ellipse center.
 *      xrad,yrad   - Horizontal and vertical radii.
 *      begang      - Start angle in tenths of degrees.
 *      endang      - End angle in tenths of degrees.
 *
 * Returns:
 *      Nothing.
 *
 * Notes:
 *      Fills the wedge using the current fill color and outlines it
 *      using the current line color.
 *
 * Sample call:
 *      v_ellpie(handle, 100, 70, 40, 20, 1800, 2700);
 */
VOID v_ellpie(WORD handle, WORD x, WORD y, WORD xrad, WORD yrad,
              WORD begang, WORD endang);

/*
 * Draw a rounded rectangle outline.
 *
 * Parameters:
 *      handle      - Target workstation handle.
 *      xy          - Inclusive rectangle {x0, y0, x1, y1}.
 *
 * Returns:
 *      Nothing.
 *
 * Notes:
 *      The hosted implementation currently draws a simple rectangular
 *      outline.
 *
 * Sample call:
 *      WORD box[4] = {20, 20, 120, 60};
 *      v_rbox(handle, box);
 */
VOID v_rbox(WORD handle, WORD xy[4]);

/*
 * Draw a filled rounded rectangle.
 *
 * Parameters:
 *      handle      - Target workstation handle.
 *      xy          - Inclusive rectangle {x0, y0, x1, y1}.
 *
 * Returns:
 *      Nothing.
 *
 * Notes:
 *      The hosted implementation currently fills a simple rectangle.
 *
 * Sample call:
 *      WORD box[4] = {20, 20, 120, 60};
 *      v_rfbox(handle, box);
 */
VOID v_rfbox(WORD handle, WORD xy[4]);

/*
 * Draw justified text.
 *
 * Parameters:
 *      handle      - Target workstation handle.
 *      x,y         - Baseline origin.
 *      string      - Source string.
 *      length      - Maximum number of characters to use.
 *      wordspace   - Extra spacing between words.
 *      charspace   - Extra spacing between characters.
 *
 * Returns:
 *      Nothing.
 *
 * Notes:
 *      The current hosted implementation truncates to `length` and
 *      delegates to normal text drawing.
 *
 * Sample call:
 *      v_justified(handle, 10, 30, "Menu", 4, 0, 0);
 */
VOID v_justified(WORD handle, WORD x, WORD y, char *string, WORD length,
                 WORD wordspace, WORD charspace);

/*
 * Set character height.
 *
 * Parameters:
 *      handle      - Target workstation handle.
 *      height      - Requested text height.
 *      charw       - Receives character width.
 *      charh       - Receives character height.
 *      cellw       - Receives cell width.
 *      cellh       - Receives cell height.
 *
 * Returns:
 *      Effective text height.
 *
 * Notes:
 *      In this port, font bitmap dimensions still constrain the actual
 *      rendered metrics.
 *
 * Sample call:
 *      WORD cw, ch, fw, fh;
 *      vst_height(handle, 16, &cw, &ch, &fw, &fh);
 */
WORD vst_height(WORD handle, WORD height, WORD *charw, WORD *charh,
                WORD *cellw, WORD *cellh);

/*
 * Set text rotation.
 *
 * Parameters:
 *      handle      - Target workstation handle.
 *      angle       - Rotation in tenths of degrees.
 *
 * Returns:
 *      Effective angle.
 *
 * Notes:
 *      The value is stored for compatibility even when rendering does
 *      not rotate glyphs.
 *
 * Sample call:
 *      vst_rotation(handle, 900);
 */
WORD vst_rotation(WORD handle, WORD angle);

/*
 * Set a palette entry.
 *
 * Parameters:
 *      handle      - Target workstation handle.
 *      index       - Palette index to update.
 *      rgb         - Three-entry array in VDI color space.
 *
 * Returns:
 *      Non-zero on success, otherwise zero.
 *
 * Notes:
 *      The hosted port keeps the values for compatibility even when the
 *      display is effectively monochrome.
 *
 * Sample call:
 *      WORD rgb[3] = {1000, 1000, 1000};
 *      vs_color(handle, 1, rgb);
 */
WORD vs_color(WORD handle, WORD index, WORD rgb[3]);

/*
 * Set line style.
 *
 * Parameters:
 *      handle      - Target workstation handle.
 *      style       - Requested VDI line style.
 *
 * Returns:
 *      Effective style.
 *
 * Notes:
 *      The hosted implementation stores the value for compatibility.
 *
 * Sample call:
 *      vsl_type(handle, 1);
 */
WORD vsl_type(WORD handle, WORD style);

/*
 * Set line width.
 *
 * Parameters:
 *      handle      - Target workstation handle.
 *      width       - Requested line width.
 *
 * Returns:
 *      Effective width.
 *
 * Notes:
 *      Values less than one are normalized to one.
 *
 * Sample call:
 *      vsl_width(handle, 1);
 */
WORD vsl_width(WORD handle, WORD width);

/*
 * Set line color by VDI color index.
 *
 * Parameters:
 *      handle      - Target workstation handle.
 *      color_index - VDI color index.
 *
 * Returns:
 *      Nothing.
 *
 * Notes:
 *      Non-zero indices map to the lit monochrome pixel in this port.
 *
 * Sample call:
 *      vsl_color(handle, 1);
 */
VOID vsl_color(VDI_HANDLE handle, WORD color_index);

/*
 * Set marker type.
 *
 * Parameters:
 *      handle      - Target workstation handle.
 *      symbol      - Requested marker symbol.
 *
 * Returns:
 *      Effective marker symbol.
 *
 * Notes:
 *      The hosted implementation stores the value for compatibility.
 *
 * Sample call:
 *      vsm_type(handle, 3);
 */
WORD vsm_type(WORD handle, WORD symbol);

/*
 * Set marker height.
 *
 * Parameters:
 *      handle      - Target workstation handle.
 *      height      - Requested marker height.
 *
 * Returns:
 *      Effective marker height.
 *
 * Notes:
 *      Values less than one fall back to the font height.
 *
 * Sample call:
 *      vsm_height(handle, 8);
 */
WORD vsm_height(WORD handle, WORD height);

/*
 * Set marker color.
 *
 * Parameters:
 *      handle      - Target workstation handle.
 *      color_index - VDI color index.
 *
 * Returns:
 *      Requested color index.
 *
 * Notes:
 *      The stored marker color is converted into the hosted monochrome
 *      pixel representation.
 *
 * Sample call:
 *      vsm_color(handle, 1);
 */
WORD vsm_color(WORD handle, WORD color_index);

/*
 * Set the active text font.
 *
 * Parameters:
 *      handle      - Target workstation handle.
 *      font        - Requested VDI font identifier.
 *
 * Returns:
 *      Effective font identifier, or zero on failure.
 *
 * Notes:
 *      Font aliases may be resolved internally to the hosted font set.
 *
 * Sample call:
 *      vst_font(handle, 1);
 */
WORD vst_font(WORD handle, WORD font);

/*
 * Set text color by VDI color index.
 *
 * Parameters:
 *      handle      - Target workstation handle.
 *      color_index - VDI color index.
 *
 * Returns:
 *      Nothing.
 *
 * Notes:
 *      Non-zero indices map to the lit monochrome pixel in this port.
 *
 * Sample call:
 *      vst_color(handle, 1);
 */
VOID vst_color(VDI_HANDLE handle, WORD color_index);

/*
 * Set fill interior style.
 *
 * Parameters:
 *      handle      - Target workstation handle.
 *      style       - Requested fill interior style.
 *
 * Returns:
 *      Effective style.
 *
 * Notes:
 *      The current hosted renderer keeps the setting mainly for API
 *      compatibility.
 *
 * Sample call:
 *      vsf_interior(handle, 1);
 */
WORD vsf_interior(WORD handle, WORD style);

/*
 * Set fill pattern style.
 *
 * Parameters:
 *      handle      - Target workstation handle.
 *      style       - Requested fill pattern style.
 *
 * Returns:
 *      Effective style.
 *
 * Notes:
 *      The current hosted renderer keeps the setting mainly for API
 *      compatibility.
 *
 * Sample call:
 *      vsf_style(handle, 1);
 */
WORD vsf_style(WORD handle, WORD style);

/*
 * Set fill color by VDI color index.
 *
 * Parameters:
 *      handle      - Target workstation handle.
 *      color_index - VDI color index.
 *
 * Returns:
 *      Nothing.
 *
 * Notes:
 *      Non-zero indices map to the lit monochrome pixel in this port.
 *
 * Sample call:
 *      vsf_color(handle, 1);
 */
VOID vsf_color(VDI_HANDLE handle, WORD color_index);

/*
 * Query a palette entry.
 *
 * Parameters:
 *      handle      - Target workstation handle.
 *      index       - Palette index to query.
 *      setflag     - Historical VDI query flag.
 *      rgb         - Receives the color triplet.
 *
 * Returns:
 *      Non-zero on success, otherwise zero.
 *
 * Notes:
 *      `setflag` is accepted for GEM compatibility.
 *
 * Sample call:
 *      WORD rgb[3];
 *      vq_color(handle, 1, 0, rgb);
 */
WORD vq_color(WORD handle, WORD index, WORD setflag, WORD rgb[]);

/*
 * Query a cell array from the screen.
 *
 * Parameters:
 *      handle          - Target workstation handle.
 *      xy              - Source rectangle {x0, y0, x1, y1}.
 *      row_length      - Historical row length parameter.
 *      num_requested   - Number of elements to sample.
 *      el_used         - Receives the number of elements returned.
 *      rows_used       - Receives the number of rows returned.
 *      status          - Receives sampled cell values.
 *
 * Returns:
 *      Non-zero on success, otherwise zero.
 *
 * Notes:
 *      The hosted implementation samples representative pixels.
 *
 * Sample call:
 *      WORD rect[4] = {0, 0, 31, 15};
 *      WORD used, rows, cells[4];
 *      vq_cellarray(handle, rect, 2, 4, &used, &rows, cells);
 */
WORD vq_cellarray(WORD handle, WORD xy[], WORD row_length,
                  WORD num_requested, WORD *el_used, WORD *rows_used,
                  WORD status[]);

/*
 * Request locator input.
 *
 * Parameters:
 *      handle      - Target workstation handle.
 *      x,y         - Initial locator position hint.
 *      xout,yout   - Receive the current locator position.
 *      term        - Receives the terminating device state.
 *
 * Returns:
 *      Non-zero on success, otherwise zero.
 *
 * Notes:
 *      The hosted implementation samples current mouse state.
 *
 * Sample call:
 *      WORD x, y, term;
 *      vrq_locator(handle, 0, 0, &x, &y, &term);
 */
WORD vrq_locator(WORD handle, WORD x, WORD y, WORD *xout, WORD *yout,
                 WORD *term);

/*
 * Sample locator input.
 *
 * Parameters:
 *      handle      - Target workstation handle.
 *      x,y         - Initial locator position hint.
 *      xout,yout   - Receive the current locator position.
 *      term        - Receives the sampled device state.
 *
 * Returns:
 *      Non-zero on success, otherwise zero.
 *
 * Notes:
 *      This is the non-blocking locator form in classic GEM terms.
 *
 * Sample call:
 *      WORD x, y, term;
 *      vsm_locator(handle, 0, 0, &x, &y, &term);
 */
WORD vsm_locator(WORD handle, WORD x, WORD y, WORD *xout, WORD *yout,
                 WORD *term);

/*
 * Request valuator input.
 *
 * Parameters:
 *      handle      - Target workstation handle.
 *      val_in      - Initial valuator value.
 *      val_out     - Receives the resulting value.
 *      term        - Receives terminating status.
 *
 * Returns:
 *      Non-zero on success, otherwise zero.
 *
 * Notes:
 *      The hosted implementation echoes `val_in` back to `val_out`.
 *
 * Sample call:
 *      WORD value, term;
 *      vrq_valuator(handle, 5, &value, &term);
 */
WORD vrq_valuator(WORD handle, WORD val_in, WORD *val_out, WORD *term);

/*
 * Sample valuator input.
 *
 * Parameters:
 *      handle      - Target workstation handle.
 *      val_in      - Initial valuator value.
 *      val_out     - Receives the sampled value.
 *      term        - Receives sampled status.
 *
 * Returns:
 *      Non-zero on success, otherwise zero.
 *
 * Notes:
 *      The hosted implementation mirrors vrq_valuator().
 *
 * Sample call:
 *      WORD value, term;
 *      vsm_valuator(handle, 5, &value, &term);
 */
WORD vsm_valuator(WORD handle, WORD val_in, WORD *val_out, WORD *term);

/*
 * Request choice input.
 *
 * Parameters:
 *      handle      - Target workstation handle.
 *      ch_in       - Initial choice value.
 *      ch_out      - Receives the selected choice.
 *
 * Returns:
 *      Non-zero on success, otherwise zero.
 *
 * Notes:
 *      The hosted implementation echoes the input choice.
 *
 * Sample call:
 *      WORD choice;
 *      vrq_choice(handle, 1, &choice);
 */
WORD vrq_choice(WORD handle, WORD ch_in, WORD *ch_out);

/*
 * Sample choice input.
 *
 * Parameters:
 *      handle      - Target workstation handle.
 *      choice      - Receives the sampled choice value.
 *
 * Returns:
 *      Non-zero on success, otherwise zero.
 *
 * Notes:
 *      The hosted implementation reports a default choice of one.
 *
 * Sample call:
 *      WORD choice;
 *      vsm_choice(handle, &choice);
 */
WORD vsm_choice(WORD handle, WORD *choice);

/*
 * Request string input.
 *
 * Parameters:
 *      handle      - Target workstation handle.
 *      max_length  - Maximum number of characters to store.
 *      echo_mode   - Non-zero to echo typed characters.
 *      echo_xy     - Baseline position for echoed text.
 *      out_string  - Receives the NUL-terminated result.
 *
 * Returns:
 *      Nothing.
 *
 * Notes:
 *      This call may block until the hosted backend receives Enter.
 *
 * Sample call:
 *      BYTE text[32];
 *      WORD echo_xy[2] = {10, 20};
 *      vrq_string(handle, 31, 1, echo_xy, text);
 */
VOID vrq_string(VDI_HANDLE handle, WORD max_length, WORD echo_mode,
                WORD *echo_xy, BYTE *out_string);

/*
 * Sample string input.
 *
 * Parameters:
 *      handle      - Target workstation handle.
 *      length      - Maximum number of characters to store.
 *      echo_mode   - Non-zero to echo typed characters.
 *      echo_xy     - Baseline position for echoed text.
 *      string      - Receives the NUL-terminated result.
 *
 * Returns:
 *      Non-zero on success, otherwise zero.
 *
 * Notes:
 *      The hosted implementation delegates to vrq_string().
 *
 * Sample call:
 *      BYTE text[32];
 *      WORD echo_xy[2] = {10, 20};
 *      vsm_string(handle, 31, 1, echo_xy, text);
 */
WORD vsm_string(WORD handle, WORD length, WORD echo_mode,
                WORD *echo_xy, BYTE *string);

/*
 * Set write mode.
 *
 * Parameters:
 *      handle      - Target workstation handle.
 *      mode        - VDI write mode.
 *
 * Returns:
 *      Effective mode.
 *
 * Notes:
 *      The mode affects low-level pixel composition for lines, fills,
 *      and raster copies.
 *
 * Sample call:
 *      vswr_mode(handle, 1);
 */
WORD vswr_mode(WORD handle, WORD mode);

/*
 * Set input mode for a device class.
 *
 * Parameters:
 *      handle      - Target workstation handle.
 *      dev_type    - VDI input device class.
 *      mode        - Requested input mode.
 *
 * Returns:
 *      Effective mode, or zero on failure.
 *
 * Notes:
 *      Unsupported device types are rejected.
 *
 * Sample call:
 *      vsin_mode(handle, 0, 1);
 */
WORD vsin_mode(WORD handle, WORD dev_type, WORD mode);

/*
 * Inquire line attributes.
 *
 * Parameters:
 *      handle      - Target workstation handle.
 *      attrib      - Receives line attribute values.
 *
 * Returns:
 *      Non-zero on success, otherwise zero.
 *
 * Notes:
 *      The array receives style, color, write mode, and width.
 *
 * Sample call:
 *      WORD attrib[4];
 *      vql_attributes(handle, attrib);
 */
WORD vql_attributes(WORD handle, WORD attrib[]);

/*
 * Inquire marker attributes.
 *
 * Parameters:
 *      handle      - Target workstation handle.
 *      attrib      - Receives marker attribute values.
 *
 * Returns:
 *      Non-zero on success, otherwise zero.
 *
 * Notes:
 *      The array receives type, color, write mode, and height.
 *
 * Sample call:
 *      WORD attrib[4];
 *      vqm_attributes(handle, attrib);
 */
WORD vqm_attributes(WORD handle, WORD attrib[]);

/*
 * Inquire fill attributes.
 *
 * Parameters:
 *      handle      - Target workstation handle.
 *      attrib      - Receives fill attribute values.
 *
 * Returns:
 *      Non-zero on success, otherwise zero.
 *
 * Notes:
 *      The array receives interior, color, style, and perimeter state.
 *
 * Sample call:
 *      WORD attrib[4];
 *      vqf_attributes(handle, attrib);
 */
WORD vqf_attributes(WORD handle, WORD attrib[]);

/*
 * Inquire text attributes.
 *
 * Parameters:
 *      handle      - Target workstation handle.
 *      attrib      - Receives text attribute values.
 *
 * Returns:
 *      Non-zero on success, otherwise zero.
 *
 * Notes:
 *      The array receives font, color, rotation, height, and alignment.
 *
 * Sample call:
 *      WORD attrib[6];
 *      vqt_attributes(handle, attrib);
 */
WORD vqt_attributes(WORD handle, WORD attrib[]);

/*
 * Set text alignment.
 *
 * Parameters:
 *      handle      - Target workstation handle.
 *      horiz       - Horizontal alignment mode.
 *      vert        - Vertical alignment mode.
 *
 * Returns:
 *      Non-zero on success, otherwise zero.
 *
 * Notes:
 *      The stored alignment affects text query and compatibility state.
 *
 * Sample call:
 *      vst_alignment(handle, 0, 5);
 */
WORD vst_alignment(WORD handle, WORD horiz, WORD vert);

/*
 * Query extended workstation information.
 *
 * Parameters:
 *      handle      - Target workstation handle.
 *      owflag      - Historical output/workstation flag.
 *      work_out    - Receives workstation capability data.
 *
 * Returns:
 *      Non-zero on success, otherwise zero.
 *
 * Notes:
 *      The hosted implementation fills a standard 57-word work_out
 *      block similar to open-workstation calls.
 *
 * Sample call:
 *      WORD work_out[57];
 *      vq_extnd(handle, 0, work_out);
 */
WORD vq_extnd(WORD handle, WORD owflag, WORD work_out[]);

/*
 * Perform contour fill from a seed point.
 *
 * Parameters:
 *      handle      - Target workstation handle.
 *      x,y         - Seed point inside the region to fill.
 *      index       - Fill color index.
 *
 * Returns:
 *      Nothing.
 *
 * Notes:
 *      The region is bounded by differing pixel values and by the
 *      current clip rectangle.
 *
 * Sample call:
 *      v_contourfill(handle, 40, 40, 1);
 */
VOID v_contourfill(WORD handle, WORD x, WORD y, WORD index);

/*
 * Set fill perimeter visibility.
 *
 * Parameters:
 *      handle      - Target workstation handle.
 *      per_vis     - Non-zero to draw the perimeter.
 *
 * Returns:
 *      Effective visibility value.
 *
 * Notes:
 *      The setting is used by filled polygonal primitives.
 *
 * Sample call:
 *      vsf_perimeter(handle, 1);
 */
WORD vsf_perimeter(WORD handle, WORD per_vis);

/*
 * Set text background mode.
 *
 * Parameters:
 *      handle      - Target workstation handle.
 *      mode        - Background mode.
 *
 * Returns:
 *      Effective mode.
 *
 * Notes:
 *      Non-zero values typically request opaque background rendering.
 *
 * Sample call:
 *      vst_background(handle, 1);
 */
WORD vst_background(WORD handle, WORD mode);

/*
 * Set text effects.
 *
 * Parameters:
 *      handle      - Target workstation handle.
 *      effect      - VDI text effect mask.
 *
 * Returns:
 *      Effective effect mask.
 *
 * Notes:
 *      The hosted implementation stores the value for compatibility.
 *
 * Sample call:
 *      vst_effects(handle, 0);
 */
WORD vst_effects(WORD handle, WORD effect);

/*
 * Set text point size.
 *
 * Parameters:
 *      handle      - Target workstation handle.
 *      point       - Requested point size.
 *      charw       - Receives character width.
 *      charh       - Receives character height.
 *      cellw       - Receives cell width.
 *      cellh       - Receives cell height.
 *
 * Returns:
 *      Effective text height.
 *
 * Notes:
 *      The hosted implementation currently maps this to vst_height().
 *
 * Sample call:
 *      WORD cw, ch, fw, fh;
 *      vst_point(handle, 12, &cw, &ch, &fw, &fh);
 */
WORD vst_point(WORD handle, WORD point, WORD *charw, WORD *charh,
               WORD *cellw, WORD *cellh);

/*
 * Set line end styles.
 *
 * Parameters:
 *      handle      - Target workstation handle.
 *      begstyle    - Requested start cap style.
 *      endstyle    - Requested end cap style.
 *
 * Returns:
 *      Non-zero on success, otherwise zero.
 *
 * Notes:
 *      The hosted implementation stores the values for compatibility.
 *
 * Sample call:
 *      vsl_ends(handle, 0, 0);
 */
WORD vsl_ends(WORD handle, WORD begstyle, WORD endstyle);

/*
 * Set line end styles using the historical alias name.
 *
 * Parameters:
 *      handle      - Target workstation handle.
 *      begstyle    - Requested start cap style.
 *      endstyle    - Requested end cap style.
 *
 * Returns:
 *      Non-zero on success, otherwise zero.
 *
 * Notes:
 *      This is an alias for vsl_ends().
 *
 * Sample call:
 *      vsl_end_style(handle, 0, 0);
 */
WORD vsl_end_style(WORD handle, WORD begstyle, WORD endstyle);

/*
 * Copy raster data between memory forms.
 *
 * Parameters:
 *      handle      - Target workstation handle.
 *      mode        - VDI raster write mode.
 *      pxy         - Source and destination rectangles.
 *      src         - Source MFDB, or screen when fd_addr is NULL.
 *      dst         - Destination MFDB, or screen when fd_addr is NULL.
 *
 * Returns:
 *      Nothing.
 *
 * Notes:
 *      `pxy` is {sx0, sy0, sx1, sy1, dx0, dy0, dx1, dy1}.
 *
 * Sample call:
 *      WORD pxy[8] = {0, 0, 15, 15, 32, 32, 47, 47};
 *      vro_cpyfm(handle, 1, pxy, &src, &dst);
 */
VOID vro_cpyfm(VDI_HANDLE handle, WORD mode, CONST WORD pxy[8],
               MFDB *src, MFDB *dst);

/*
 * Transform a memory form between layouts.
 *
 * Parameters:
 *      handle      - Target workstation handle.
 *      src         - Source MFDB.
 *      dst         - Destination MFDB.
 *
 * Returns:
 *      Non-zero on success, otherwise zero.
 *
 * Notes:
 *      The hosted implementation performs a raster copy over the full
 *      source rectangle.
 *
 * Sample call:
 *      vr_trnfm(handle, &src, &dst);
 */
WORD vr_trnfm(WORD handle, MFDB *src, MFDB *dst);

/*
 * Set the mouse form.
 *
 * Parameters:
 *      handle      - Target workstation handle.
 *      cur_form    - New mouse form.
 *
 * Returns:
 *      Non-zero on success, otherwise zero.
 *
 * Notes:
 *      The form is copied into the active workstation state.
 *
 * Sample call:
 *      MFORM form = {0};
 *      vsc_form(handle, &form);
 */
WORD vsc_form(WORD handle, MFORM *cur_form);

/*
 * Set a user-defined fill pattern.
 *
 * Parameters:
 *      handle      - Target workstation handle.
 *      fill_pat    - Pattern words.
 *      planes      - Number of planes in the pattern.
 *
 * Returns:
 *      Non-zero on success, otherwise zero.
 *
 * Notes:
 *      The hosted implementation stores the pattern for compatibility.
 *
 * Sample call:
 *      WORD pattern[16] = {0xffff};
 *      vsf_udpat(handle, pattern, 1);
 */
WORD vsf_udpat(WORD handle, WORD *fill_pat, WORD planes);

/*
 * Set a user-defined line style.
 *
 * Parameters:
 *      handle      - Target workstation handle.
 *      pattern     - Pattern bit mask.
 *
 * Returns:
 *      Effective pattern.
 *
 * Notes:
 *      The hosted implementation stores the value for compatibility.
 *
 * Sample call:
 *      vsl_udsty(handle, 0xffff);
 */
WORD vsl_udsty(WORD handle, WORD pattern);

/*
 * Draw a filled rectangle using current fill attributes.
 *
 * Parameters:
 *      handle      - Target workstation handle.
 *      xy          - Inclusive rectangle {x0, y0, x1, y1}.
 *
 * Returns:
 *      Nothing.
 *
 * Notes:
 *      In this hosted port, this is equivalent to v_bar().
 *
 * Sample call:
 *      WORD box[4] = {5, 5, 25, 25};
 *      vr_recfl(handle, box);
 */
VOID vr_recfl(WORD handle, WORD xy[4]);

/*
 * Inquire the current input mode.
 *
 * Parameters:
 *      handle      - Target workstation handle.
 *      dev_type    - Input device class.
 *      input_mode  - Receives the current mode.
 *
 * Returns:
 *      Non-zero on success, otherwise zero.
 *
 * Notes:
 *      Unsupported device types are rejected.
 *
 * Sample call:
 *      WORD mode;
 *      vqin_mode(handle, 0, &mode);
 */
WORD vqin_mode(WORD handle, WORD dev_type, WORD *input_mode);

/*
 * Inquire text extent for a string.
 *
 * Parameters:
 *      handle      - Target workstation handle.
 *      string      - String to measure.
 *      extent      - Receives the four-corner extent polygon.
 *
 * Returns:
 *      Non-zero on success, otherwise zero.
 *
 * Notes:
 *      The returned coordinates are relative to a text origin of 0,0.
 *
 * Sample call:
 *      WORD extent[8];
 *      vqt_extent(handle, "File", extent);
 */
WORD vqt_extent(WORD handle, char *string, WORD extent[8]);

/*
 * Inquire character width.
 *
 * Parameters:
 *      handle      - Target workstation handle.
 *      character   - Character code to measure.
 *      cell_width  - Receives the glyph width.
 *      left_delta  - Receives left overhang.
 *      right_delta - Receives right overhang.
 *
 * Returns:
 *      Effective character width, or zero on failure.
 *
 * Notes:
 *      The hosted implementation reports zero deltas.
 *
 * Sample call:
 *      WORD w, ld, rd;
 *      vqt_width(handle, 'A', &w, &ld, &rd);
 */
WORD vqt_width(WORD handle, WORD character, WORD *cell_width,
               WORD *left_delta, WORD *right_delta);

/*
 * Exchange the timer interrupt vector.
 *
 * Parameters:
 *      handle      - Target workstation handle.
 *      tim_addr    - New timer callback.
 *      old_addr    - Receives the previous callback.
 *      scale       - Receives a timer scale value.
 *
 * Returns:
 *      Non-zero on success, otherwise zero.
 *
 * Notes:
 *      The hosted implementation stores callbacks for compatibility.
 *
 * Sample call:
 *      VOID (*old_cb)(void);
 *      WORD scale;
 *      vex_timv(handle, my_timer_cb, &old_cb, &scale);
 */
WORD vex_timv(WORD handle, VOID (*tim_addr)(void), VOID (**old_addr)(void),
              WORD *scale);

/*
 * Load GDOS-style external fonts.
 *
 * Parameters:
 *      handle      - Target workstation handle.
 *      select      - Historical GDOS selection flag.
 *
 * Returns:
 *      Number of currently available fonts, or zero on failure.
 *
 * Notes:
 *      The hosted implementation scans the runtime font directory.
 *
 * Sample call:
 *      vst_load_fonts(handle, 0);
 */
WORD vst_load_fonts(WORD handle, WORD select);

/*
 * Unload GDOS-style external fonts.
 *
 * Parameters:
 *      handle      - Target workstation handle.
 *      select      - Historical GDOS selection flag.
 *
 * Returns:
 *      Implementation-defined status value.
 *
 * Notes:
 *      The default system font remains available after unloading.
 *
 * Sample call:
 *      vst_unload_fonts(handle, 0);
 */
WORD vst_unload_fonts(WORD handle, WORD select);

/*
 * Copy raster data transparently between memory forms.
 *
 * Parameters:
 *      handle      - Target workstation handle.
 *      mode        - VDI raster write mode.
 *      pxy         - Source and destination rectangles.
 *      src         - Source MFDB.
 *      dst         - Destination MFDB.
 *      colors      - Background and foreground color indices.
 *
 * Returns:
 *      Nothing.
 *
 * Notes:
 *      Zero source pixels are skipped during the copy.
 *
 * Sample call:
 *      WORD pxy[8] = {0, 0, 15, 15, 32, 32, 47, 47};
 *      WORD colors[2] = {0, 1};
 *      vrt_cpyfm(handle, 1, pxy, &src, &dst, colors);
 */
VOID vrt_cpyfm(VDI_HANDLE handle, WORD mode, CONST WORD pxy[8],
               MFDB *src, MFDB *dst, CONST WORD colors[2]);

/*
 * Show the mouse cursor.
 *
 * Parameters:
 *      handle      - Target workstation handle.
 *      reset       - Historical reset flag.
 *
 * Returns:
 *      Nothing.
 *
 * Notes:
 *      The hosted implementation unhides the cursor and may present the
 *      screen immediately.
 *
 * Sample call:
 *      v_show_c(handle, 1);
 */
VOID v_show_c(VDI_HANDLE handle, WORD reset);

/*
 * Hide the mouse cursor.
 *
 * Parameters:
 *      handle      - Target workstation handle.
 *
 * Returns:
 *      Nothing.
 *
 * Notes:
 *      The cursor image is removed from the framebuffer overlay.
 *
 * Sample call:
 *      v_hide_c(handle);
 */
VOID v_hide_c(VDI_HANDLE handle);

/*
 * Query current mouse state.
 *
 * Parameters:
 *      handle      - Target workstation handle.
 *      status      - Receives button and state bits.
 *      x,y         - Receive mouse coordinates.
 *
 * Returns:
 *      Nothing.
 *
 * Notes:
 *      Invalid handles return zeros in the output parameters.
 *
 * Sample call:
 *      WORD status, x, y;
 *      vq_mouse(handle, &status, &x, &y);
 */
VOID vq_mouse(VDI_HANDLE handle, WORD *status, WORD *x, WORD *y);

/*
 * Exchange the button interrupt vector.
 *
 * Parameters:
 *      handle      - Target workstation handle.
 *      usr_code    - New button callback.
 *      sav_code    - Receives the previous callback.
 *
 * Returns:
 *      Non-zero on success, otherwise zero.
 *
 * Notes:
 *      The callback is stored for compatibility.
 *
 * Sample call:
 *      VOID (*old_cb)(void);
 *      vex_butv(handle, my_button_cb, &old_cb);
 */
WORD vex_butv(WORD handle, VOID (*usr_code)(void), VOID (**sav_code)(void));

/*
 * Exchange the motion interrupt vector.
 *
 * Parameters:
 *      handle      - Target workstation handle.
 *      usr_code    - New motion callback.
 *      sav_code    - Receives the previous callback.
 *
 * Returns:
 *      Non-zero on success, otherwise zero.
 *
 * Notes:
 *      The callback is stored for compatibility.
 *
 * Sample call:
 *      VOID (*old_cb)(void);
 *      vex_motv(handle, my_motion_cb, &old_cb);
 */
WORD vex_motv(WORD handle, VOID (*usr_code)(void), VOID (**sav_code)(void));

/*
 * Exchange the cursor interrupt vector.
 *
 * Parameters:
 *      handle      - Target workstation handle.
 *      usr_code    - New cursor callback.
 *      sav_code    - Receives the previous callback.
 *
 * Returns:
 *      Non-zero on success, otherwise zero.
 *
 * Notes:
 *      The callback is stored for compatibility.
 *
 * Sample call:
 *      VOID (*old_cb)(void);
 *      vex_curv(handle, my_cursor_cb, &old_cb);
 */
WORD vex_curv(WORD handle, VOID (*usr_code)(void), VOID (**sav_code)(void));

/*
 * Query keyboard shift state.
 *
 * Parameters:
 *      handle      - Target workstation handle.
 *      status      - Receives shift-state bits.
 *
 * Returns:
 *      Non-zero on success, otherwise zero.
 *
 * Notes:
 *      The current hosted implementation reports zero shift bits.
 *
 * Sample call:
 *      WORD status;
 *      vq_key_s(handle, &status);
 */
WORD vq_key_s(WORD handle, WORD *status);

/*
 * Query a font name by element number.
 *
 * Parameters:
 *      handle      - Target workstation handle.
 *      element_num - One-based font list element.
 *      name        - Receives the font name.
 *
 * Returns:
 *      Font identifier on success, or zero on failure.
 *
 * Notes:
 *      The enumeration order matches the currently resident font list.
 *
 * Sample call:
 *      BYTE name[33];
 *      WORD font_id = vqt_name(handle, 1, name);
 */
WORD vqt_name(WORD handle, WORD element_num, BYTE *name);

/*
 * Query current font information.
 *
 * Parameters:
 *      handle      - Target workstation handle.
 *      min_ade     - Receives the first supported character code.
 *      max_ade     - Receives the last supported character code.
 *      distances   - Receives font vertical metric values.
 *      max_width   - Receives the maximum character width.
 *      effects     - Receives supported effect flags.
 *
 * Returns:
 *      Non-zero on success, otherwise zero.
 *
 * Notes:
 *      The metric layout follows GEM VDI conventions.
 *
 * Sample call:
 *      WORD min_ade, max_ade, distances[5], max_width, effects[3];
 *      vqt_fontinfo(handle, &min_ade, &max_ade, distances,
 *          &max_width, effects);
 */
WORD vqt_fontinfo(WORD handle, WORD *min_ade, WORD *max_ade,
                  WORD distances[], WORD *max_width, WORD effects[]);

/*
 * Query character cell rows and columns.
 *
 * Parameters:
 *      handle      - Target workstation handle.
 *      rows        - Receives visible text rows.
 *      columns     - Receives visible text columns.
 *
 * Returns:
 *      Non-zero on success, otherwise zero.
 *
 * Notes:
 *      Values are derived from screen size and current font metrics.
 *
 * Sample call:
 *      WORD rows, cols;
 *      vq_chcells(handle, &rows, &cols);
 */
WORD vq_chcells(WORD handle, WORD *rows, WORD *columns);

/*
 * Exit alpha cursor mode.
 *
 * Parameters:
 *      handle      - Target workstation handle.
 *
 * Returns:
 *      Non-zero if the handle is valid, otherwise zero.
 *
 * Notes:
 *      The hosted implementation keeps this as a compatibility stub.
 *
 * Sample call:
 *      v_exit_cur(handle);
 */
WORD v_exit_cur(WORD handle);

/*
 * Enter alpha cursor mode.
 *
 * Parameters:
 *      handle      - Target workstation handle.
 *
 * Returns:
 *      Non-zero if the handle is valid, otherwise zero.
 *
 * Notes:
 *      The hosted implementation keeps this as a compatibility stub.
 *
 * Sample call:
 *      v_enter_cur(handle);
 */
WORD v_enter_cur(WORD handle);

/*
 * Move the alpha cursor up one row.
 *
 * Parameters:
 *      handle      - Target workstation handle.
 *
 * Returns:
 *      Non-zero on success, otherwise zero.
 *
 * Notes:
 *      The cursor stops at row zero.
 *
 * Sample call:
 *      v_curup(handle);
 */
WORD v_curup(WORD handle);

/*
 * Move the alpha cursor down one row.
 *
 * Parameters:
 *      handle      - Target workstation handle.
 *
 * Returns:
 *      Non-zero on success, otherwise zero.
 *
 * Notes:
 *      No bottom-bound clipping is enforced by this compatibility
 *      layer.
 *
 * Sample call:
 *      v_curdown(handle);
 */
WORD v_curdown(WORD handle);

/*
 * Move the alpha cursor right one column.
 *
 * Parameters:
 *      handle      - Target workstation handle.
 *
 * Returns:
 *      Non-zero on success, otherwise zero.
 *
 * Notes:
 *      No right-edge clipping is enforced by this compatibility layer.
 *
 * Sample call:
 *      v_curright(handle);
 */
WORD v_curright(WORD handle);

/*
 * Move the alpha cursor left one column.
 *
 * Parameters:
 *      handle      - Target workstation handle.
 *
 * Returns:
 *      Non-zero on success, otherwise zero.
 *
 * Notes:
 *      The cursor stops at column zero.
 *
 * Sample call:
 *      v_curleft(handle);
 */
WORD v_curleft(WORD handle);

/*
 * Move the alpha cursor to the home position.
 *
 * Parameters:
 *      handle      - Target workstation handle.
 *
 * Returns:
 *      Non-zero on success, otherwise zero.
 *
 * Notes:
 *      Home is the top-left character cell.
 *
 * Sample call:
 *      v_curhome(handle);
 */
WORD v_curhome(WORD handle);

/*
 * Erase from the alpha cursor to the end of the screen.
 *
 * Parameters:
 *      handle      - Target workstation handle.
 *
 * Returns:
 *      Non-zero on success, otherwise zero.
 *
 * Notes:
 *      The hosted implementation clears the entire workstation.
 *
 * Sample call:
 *      v_eeos(handle);
 */
WORD v_eeos(WORD handle);

/*
 * Erase from the alpha cursor to the end of the current line.
 *
 * Parameters:
 *      handle      - Target workstation handle.
 *
 * Returns:
 *      Non-zero on success, otherwise zero.
 *
 * Notes:
 *      Erases only the current text row in the hosted implementation.
 *
 * Sample call:
 *      v_eeol(handle);
 */
WORD v_eeol(WORD handle);

/*
 * Set the alpha cursor address.
 *
 * Parameters:
 *      handle      - Target workstation handle.
 *      row         - One-based text row.
 *      column      - One-based text column.
 *
 * Returns:
 *      Non-zero on success, otherwise zero.
 *
 * Notes:
 *      The stored position is normalized to zero-based internal state.
 *
 * Sample call:
 *      vs_curaddress(handle, 1, 1);
 */
WORD vs_curaddress(WORD handle, WORD row, WORD column);

/*
 * Output text at the alpha cursor.
 *
 * Parameters:
 *      handle      - Target workstation handle.
 *      string      - NUL-terminated text to write.
 *
 * Returns:
 *      Non-zero on success, otherwise zero.
 *
 * Notes:
 *      The cursor column advances by the number of characters written.
 *
 * Sample call:
 *      v_curtext(handle, (BYTE *) "OK");
 */
WORD v_curtext(WORD handle, BYTE *string);

/*
 * Enable reverse video mode.
 *
 * Parameters:
 *      handle      - Target workstation handle.
 *
 * Returns:
 *      Non-zero on success, otherwise zero.
 *
 * Notes:
 *      The value is currently stored for compatibility.
 *
 * Sample call:
 *      v_rvon(handle);
 */
WORD v_rvon(WORD handle);

/*
 * Disable reverse video mode.
 *
 * Parameters:
 *      handle      - Target workstation handle.
 *
 * Returns:
 *      Non-zero on success, otherwise zero.
 *
 * Notes:
 *      The value is currently stored for compatibility.
 *
 * Sample call:
 *      v_rvoff(handle);
 */
WORD v_rvoff(WORD handle);

/*
 * Query the alpha cursor address.
 *
 * Parameters:
 *      handle      - Target workstation handle.
 *      row         - Receives the one-based text row.
 *      column      - Receives the one-based text column.
 *
 * Returns:
 *      Non-zero on success, otherwise zero.
 *
 * Notes:
 *      Values are converted back from zero-based internal state.
 *
 * Sample call:
 *      WORD row, col;
 *      vq_curaddress(handle, &row, &col);
 */
WORD vq_curaddress(WORD handle, WORD *row, WORD *column);

/*
 * Query tab status.
 *
 * Parameters:
 *      handle      - Target workstation handle.
 *
 * Returns:
 *      Non-zero if the handle is valid, otherwise zero.
 *
 * Notes:
 *      This is a compatibility stub in the hosted port.
 *
 * Sample call:
 *      vq_tabstatus(handle);
 */
WORD vq_tabstatus(WORD handle);

/*
 * Produce a hardcopy of the current screen.
 *
 * Parameters:
 *      handle      - Target workstation handle.
 *
 * Returns:
 *      Non-zero if the handle is valid, otherwise zero.
 *
 * Notes:
 *      This is currently a compatibility stub.
 *
 * Sample call:
 *      v_hardcopy(handle);
 */
WORD v_hardcopy(WORD handle);

/*
 * Display the alpha cursor at a screen location.
 *
 * Parameters:
 *      handle      - Target workstation handle.
 *      x,y         - Pixel location to convert into text position.
 *
 * Returns:
 *      Non-zero on success, otherwise zero.
 *
 * Notes:
 *      Coordinates are converted into character-cell row and column.
 *
 * Sample call:
 *      v_dspcur(handle, 32, 16);
 */
WORD v_dspcur(WORD handle, WORD x, WORD y);

/*
 * Remove the alpha cursor.
 *
 * Parameters:
 *      handle      - Target workstation handle.
 *
 * Returns:
 *      Non-zero if the handle is valid, otherwise zero.
 *
 * Notes:
 *      This is currently a compatibility stub.
 *
 * Sample call:
 *      v_rmcur(handle);
 */
WORD v_rmcur(WORD handle);

/*
 * Advance the form.
 *
 * Parameters:
 *      handle      - Target workstation handle.
 *
 * Returns:
 *      Non-zero if the handle is valid, otherwise zero.
 *
 * Notes:
 *      This is currently a compatibility stub.
 *
 * Sample call:
 *      v_form_adv(handle);
 */
WORD v_form_adv(WORD handle);

/*
 * Set the output window.
 *
 * Parameters:
 *      handle      - Target workstation handle.
 *      xy          - Inclusive rectangle {x0, y0, x1, y1}.
 *
 * Returns:
 *      Non-zero on success, otherwise zero.
 *
 * Notes:
 *      The hosted implementation also enables clipping to the same
 *      rectangle.
 *
 * Sample call:
 *      WORD win[4] = {0, 0, 319, 199};
 *      v_output_window(handle, win);
 */
WORD v_output_window(WORD handle, WORD xy[4]);

/*
 * Clear the display list.
 *
 * Parameters:
 *      handle      - Target workstation handle.
 *
 * Returns:
 *      Non-zero if the handle is valid, otherwise zero.
 *
 * Notes:
 *      This is currently a compatibility stub.
 *
 * Sample call:
 *      v_clear_disp_list(handle);
 */
WORD v_clear_disp_list(WORD handle);

/*
 * Output a bit image from a file.
 *
 * Parameters:
 *      handle      - Target workstation handle.
 *      filename    - Bit image file path.
 *      aspect      - Historical aspect flag.
 *      x_scale     - Horizontal scale factor.
 *      y_scale     - Vertical scale factor.
 *      h_align     - Horizontal alignment.
 *      v_align     - Vertical alignment.
 *      xy          - Destination rectangle.
 *
 * Returns:
 *      Non-zero on success, otherwise zero.
 *
 * Notes:
 *      The hosted implementation currently only verifies that the file
 *      can be opened.
 *
 * Sample call:
 *      WORD box[4] = {0, 0, 31, 31};
 *      v_bit_image(handle, (BYTE *) "logo.img", 0, 1, 1, 0, 0, box);
 */
WORD v_bit_image(WORD handle, BYTE *filename, WORD aspect, WORD x_scale,
                 WORD y_scale, WORD h_align, WORD v_align, WORD xy[4]);

/*
 * Set or select a palette.
 *
 * Parameters:
 *      handle      - Target workstation handle.
 *      palette     - Palette selector.
 *
 * Returns:
 *      The requested palette value, or zero on failure.
 *
 * Notes:
 *      This is a compatibility-oriented stub in the hosted port.
 *
 * Sample call:
 *      vs_palette(handle, 0);
 */
WORD vs_palette(WORD handle, WORD palette);

/*
 * Set metafile extents.
 *
 * Parameters:
 *      handle      - Target workstation handle.
 *      min_x,min_y - Minimum metafile coordinates.
 *      max_x,max_y - Maximum metafile coordinates.
 *
 * Returns:
 *      Non-zero if the handle is valid, otherwise zero.
 *
 * Notes:
 *      This is currently a compatibility stub.
 *
 * Sample call:
 *      v_meta_extents(handle, 0, 0, 100, 100);
 */
WORD v_meta_extents(WORD handle, WORD min_x, WORD min_y,
                    WORD max_x, WORD max_y);

/*
 * Write a raw metafile record.
 *
 * Parameters:
 *      handle      - Target workstation handle.
 *      num_intin   - Number of integer inputs.
 *      intin       - Integer input array.
 *      num_ptsin   - Number of point inputs.
 *      ptsin       - Point input array.
 *
 * Returns:
 *      Non-zero if the handle is valid, otherwise zero.
 *
 * Notes:
 *      This is currently a compatibility stub.
 *
 * Sample call:
 *      WORD intin[1] = {0};
 *      WORD ptsin[2] = {0, 0};
 *      v_write_meta(handle, 1, intin, 1, ptsin);
 */
WORD v_write_meta(WORD handle, WORD num_intin, WORD *intin,
                  WORD num_ptsin, WORD *ptsin);

/*
 * Set metafile output filename.
 *
 * Parameters:
 *      handle      - Target workstation handle.
 *      filename    - Output file name.
 *
 * Returns:
 *      Non-zero on success, otherwise zero.
 *
 * Notes:
 *      The hosted implementation stores the file name for
 *      compatibility.
 *
 * Sample call:
 *      vm_filename(handle, (BYTE *) "out.gem");
 */
WORD vm_filename(WORD handle, BYTE *filename);

/*
 * Read a pixel value and color index.
 *
 * Parameters:
 *      handle      - Target workstation handle.
 *      x,y         - Pixel coordinate to sample.
 *      pel         - Receives the raw pixel value.
 *      index       - Receives the VDI color index.
 *
 * Returns:
 *      Non-zero on success, otherwise zero.
 *
 * Notes:
 *      In the hosted monochrome port, `pel` and `index` are identical.
 *
 * Sample call:
 *      WORD pel, index;
 *      v_get_pixel(handle, 10, 10, &pel, &index);
 */
WORD v_get_pixel(WORD handle, WORD x, WORD y, WORD *pel, WORD *index);

/*
 * Invoke a generic VDI escape function.
 *
 * Parameters:
 *      handle      - Target workstation handle.
 *      function    - Escape function number.
 *      count_in    - Number of input values.
 *      count_out   - Number of output values expected.
 *      intin       - Integer input array.
 *      ptsout      - Point output array.
 *
 * Returns:
 *      Non-zero if the handle is valid, otherwise zero.
 *
 * Notes:
 *      This is currently a compatibility stub.
 *
 * Sample call:
 *      WORD intin[1] = {0};
 *      WORD ptsout[2] = {0, 0};
 *      v_escape(handle, 0, 1, 1, intin, ptsout);
 */
WORD v_escape(WORD handle, WORD function, WORD count_in, WORD count_out,
              WORD *intin, WORD *ptsout);

#ifdef __cplusplus
}
#endif

#endif /* GEM_VDI_H */
