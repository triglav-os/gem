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
 * Mouse form used by vsc_form().
 */
typedef struct mform {
    WORD mf_xhot;
    WORD mf_yhot;
    WORD mf_nplanes;
    WORD mf_fg;
    WORD mf_bg;
    WORD mf_mask[16];
    WORD mf_data[16];
} MFORM;

/*
 * Open a physical workstation.
 */
WORD v_opnwk(WORD work_in[], WORD *handle, WORD work_out[]);

/*
 * Open a virtual workstation.
 *
 * work_in has 11 entries in the classic GEM calling convention.
 * Returns a non-zero handle and fills work_out with screen
 * capabilities.
 */
VOID v_opnvwk(WORD work_in[11], VDI_HANDLE *handle, WORD work_out[57]);

/*
 * Open a raster workstation.
 *
 * Historical GEM documentation uses this name for a workstation opened
 * against a bitmap-like target.
 */
WORD v_opnrwk(WORD work_in[], WORD *handle, WORD work_out[]);

/*
 * Close a physical workstation.
 */
WORD v_clswk(WORD handle);

/*
 * Close a virtual workstation.
 */
VOID v_clsvwk(VDI_HANDLE handle);

/*
 * Clear the workstation to the current background color.
 */
VOID v_clrwk(VDI_HANDLE handle);

/*
 * Update the workstation display.
 */
WORD v_updwk(WORD handle);

/*
 * Enable or disable clipping.
 */
VOID vs_clip(WORD handle, WORD clip_flag, WORD xy[4]);

/*
 * Draw a connected polyline.
 */
VOID v_pline(VDI_HANDLE handle, WORD count, CONST WORD *pxy);

/*
 * Draw polymarkers.
 */
VOID v_pmarker(WORD handle, WORD count, WORD xy[]);

/*
 * Draw text at baseline position x,y.
 */
VOID v_gtext(VDI_HANDLE handle, WORD x, WORD y, CONST BYTE *text);

/*
 * Draw a filled polygonal area.
 */
VOID v_fillarea(WORD handle, WORD count, WORD xy[]);

/*
 * Draw a cell array.
 */
WORD v_cellarray(WORD handle, WORD xy[], WORD row_length,
                 WORD el_per_row, WORD num_rows, WORD wr_mode,
                 WORD colors[]);

/*
 * Draw a filled rectangle.
 */
VOID v_bar(VDI_HANDLE handle, CONST WORD pxy[4]);

/*
 * Draw a circular arc.
 */
VOID v_arc(WORD handle, WORD x, WORD y, WORD radius, WORD begang,
           WORD endang);

/*
 * Draw a circular pie slice.
 */
VOID v_pieslice(WORD handle, WORD x, WORD y, WORD radius, WORD begang,
                WORD endang);

/*
 * Compatibility alias used by some newer port code.
 */
VOID v_pie(WORD handle, WORD x, WORD y, WORD radius, WORD begang,
           WORD endang);

/*
 * Draw a circle.
 */
VOID v_circle(WORD handle, WORD x, WORD y, WORD radius);

/*
 * Draw an ellipse.
 */
VOID v_ellipse(WORD handle, WORD x, WORD y, WORD xrad, WORD yrad);

/*
 * Draw an elliptical arc.
 */
VOID v_ellarc(WORD handle, WORD x, WORD y, WORD xrad, WORD yrad,
              WORD begang, WORD endang);

/*
 * Draw an elliptical pie slice.
 */
VOID v_ellpie(WORD handle, WORD x, WORD y, WORD xrad, WORD yrad,
              WORD begang, WORD endang);

/*
 * Draw a rounded rectangle outline.
 */
VOID v_rbox(WORD handle, WORD xy[4]);

/*
 * Draw a filled rounded rectangle.
 */
VOID v_rfbox(WORD handle, WORD xy[4]);

/*
 * Draw justified text.
 */
VOID v_justified(WORD handle, WORD x, WORD y, char *string, WORD length,
                 WORD wordspace, WORD charspace);

/*
 * Set character height.
 */
WORD vst_height(WORD handle, WORD height, WORD *charw, WORD *charh,
                WORD *cellw, WORD *cellh);

/*
 * Set text rotation.
 */
WORD vst_rotation(WORD handle, WORD angle);

/*
 * Set a hardware palette color entry.
 */
WORD vs_color(WORD handle, WORD index, WORD rgb[3]);

/*
 * Set line style.
 */
WORD vsl_type(WORD handle, WORD style);

/*
 * Set line width.
 */
WORD vsl_width(WORD handle, WORD width);

/*
 * Set line color by VDI color index.
 */
VOID vsl_color(VDI_HANDLE handle, WORD color_index);

/*
 * Set marker type.
 */
WORD vsm_type(WORD handle, WORD symbol);

/*
 * Set marker height.
 */
WORD vsm_height(WORD handle, WORD height);

/*
 * Set marker color.
 */
WORD vsm_color(WORD handle, WORD color_index);

/*
 * Set text font.
 */
WORD vst_font(WORD handle, WORD font);

/*
 * Set text color by VDI color index.
 */
VOID vst_color(VDI_HANDLE handle, WORD color_index);

/*
 * Set fill interior style.
 */
WORD vsf_interior(WORD handle, WORD style);

/*
 * Set fill pattern style.
 */
WORD vsf_style(WORD handle, WORD style);

/*
 * Set fill color by VDI color index.
 */
VOID vsf_color(VDI_HANDLE handle, WORD color_index);

/*
 * Query a VDI color entry.
 */
WORD vq_color(WORD handle, WORD index, WORD setflag, WORD rgb[]);

/*
 * Query a cell array.
 */
WORD vq_cellarray(WORD handle, WORD xy[], WORD row_length,
                  WORD num_requested, WORD *el_used, WORD *rows_used,
                  WORD status[]);

/*
 * Request locator input.
 */
WORD vrq_locator(WORD handle, WORD x, WORD y, WORD *xout, WORD *yout,
                 WORD *term);

/*
 * Sample locator input.
 */
WORD vsm_locator(WORD handle, WORD x, WORD y, WORD *xout, WORD *yout,
                 WORD *term);

/*
 * Request valuator input.
 */
WORD vrq_valuator(WORD handle, WORD val_in, WORD *val_out, WORD *term);

/*
 * Sample valuator input.
 */
WORD vsm_valuator(WORD handle, WORD val_in, WORD *val_out, WORD *term);

/*
 * Request choice input.
 */
WORD vrq_choice(WORD handle, WORD ch_in, WORD *ch_out);

/*
 * Sample choice input.
 */
WORD vsm_choice(WORD handle, WORD *choice);

/*
 * Request string input.
 */
VOID vrq_string(VDI_HANDLE handle, WORD max_length, WORD echo_mode,
                WORD *echo_xy, BYTE *out_string);

/*
 * Sample string input.
 */
WORD vsm_string(WORD handle, WORD length, WORD echo_mode,
                WORD *echo_xy, BYTE *string);

/*
 * Set write mode.
 */
WORD vswr_mode(WORD handle, WORD mode);

/*
 * Set input mode.
 */
WORD vsin_mode(WORD handle, WORD dev_type, WORD mode);

/*
 * Inquire line attributes.
 */
WORD vql_attributes(WORD handle, WORD attrib[]);

/*
 * Inquire marker attributes.
 */
WORD vqm_attributes(WORD handle, WORD attrib[]);

/*
 * Inquire fill attributes.
 */
WORD vqf_attributes(WORD handle, WORD attrib[]);

/*
 * Inquire text attributes.
 */
WORD vqt_attributes(WORD handle, WORD attrib[]);

/*
 * Set text alignment.
 */
WORD vst_alignment(WORD handle, WORD horiz, WORD vert);

/*
 * Query extended workstation information.
 */
WORD vq_extnd(WORD handle, WORD owflag, WORD work_out[]);

/*
 * Perform contour fill.
 */
VOID v_contourfill(WORD handle, WORD x, WORD y, WORD index);

/*
 * Set fill perimeter visibility.
 */
WORD vsf_perimeter(WORD handle, WORD per_vis);

/*
 * Set text background mode.
 */
WORD vst_background(WORD handle, WORD mode);

/*
 * Set text effects.
 */
WORD vst_effects(WORD handle, WORD effect);

/*
 * Set text point size.
 */
WORD vst_point(WORD handle, WORD point, WORD *charw, WORD *charh,
               WORD *cellw, WORD *cellh);

/*
 * Set line end styles.
 */
WORD vsl_ends(WORD handle, WORD begstyle, WORD endstyle);

/*
 * Historical alternative name for vsl_ends().
 */
WORD vsl_end_style(WORD handle, WORD begstyle, WORD endstyle);

/*
 * Raster copy between memory forms.
 */
VOID vro_cpyfm(VDI_HANDLE handle, WORD mode, CONST WORD pxy[8],
               MFDB *src, MFDB *dst);

/*
 * Transform a memory form between standard and device-specific layout.
 */
WORD vr_trnfm(WORD handle, MFDB *src, MFDB *dst);

/*
 * Set mouse form.
 */
WORD vsc_form(WORD handle, MFORM *cur_form);

/*
 * Set user-defined fill pattern.
 */
WORD vsf_udpat(WORD handle, WORD *fill_pat, WORD planes);

/*
 * Set user-defined line style.
 */
WORD vsl_udsty(WORD handle, WORD pattern);

/*
 * Draw a filled rectangle using current fill attributes.
 */
VOID vr_recfl(WORD handle, WORD xy[4]);

/*
 * Inquire current input mode.
 */
WORD vqin_mode(WORD handle, WORD dev_type, WORD *input_mode);

/*
 * Inquire text extent.
 */
WORD vqt_extent(WORD handle, char *string, WORD extent[8]);

/*
 * Inquire character width.
 */
WORD vqt_width(WORD handle, WORD character, WORD *cell_width,
               WORD *left_delta, WORD *right_delta);

/*
 * Exchange timer interrupt vector.
 */
WORD vex_timv(WORD handle, VOID (*tim_addr)(void), VOID (**old_addr)(void),
              WORD *scale);

/*
 * Load GDOS fonts.
 */
WORD vst_load_fonts(WORD handle, WORD select);

/*
 * Unload GDOS fonts.
 */
WORD vst_unload_fonts(WORD handle, WORD select);

/*
 * Transparent raster copy between memory forms.
 */
VOID vrt_cpyfm(VDI_HANDLE handle, WORD mode, CONST WORD pxy[8],
               MFDB *src, MFDB *dst, CONST WORD colors[2]);

/*
 * Show mouse cursor.
 */
VOID v_show_c(VDI_HANDLE handle, WORD reset);

/*
 * Hide mouse cursor.
 */
VOID v_hide_c(VDI_HANDLE handle);

/*
 * Query current mouse state.
 */
VOID vq_mouse(VDI_HANDLE handle, WORD *status, WORD *x, WORD *y);

/*
 * Exchange button interrupt vector.
 */
WORD vex_butv(WORD handle, VOID (*usr_code)(void), VOID (**sav_code)(void));

/*
 * Exchange motion interrupt vector.
 */
WORD vex_motv(WORD handle, VOID (*usr_code)(void), VOID (**sav_code)(void));

/*
 * Exchange cursor interrupt vector.
 */
WORD vex_curv(WORD handle, VOID (*usr_code)(void), VOID (**sav_code)(void));

/*
 * Query keyboard shift state.
 */
WORD vq_key_s(WORD handle, WORD *status);

/*
 * Query available font name.
 */
WORD vqt_name(WORD handle, WORD element_num, BYTE *name);

/*
 * Query current font information.
 */
WORD vqt_fontinfo(WORD handle, WORD *min_ade, WORD *max_ade,
                  WORD distances[], WORD *max_width, WORD effects[]);

/*
 * Query character cell rows and columns.
 */
WORD vq_chcells(WORD handle, WORD *rows, WORD *columns);

/*
 * Exit alpha cursor mode.
 */
WORD v_exit_cur(WORD handle);

/*
 * Enter alpha cursor mode.
 */
WORD v_enter_cur(WORD handle);

/*
 * Move alpha cursor up.
 */
WORD v_curup(WORD handle);

/*
 * Move alpha cursor down.
 */
WORD v_curdown(WORD handle);

/*
 * Move alpha cursor right.
 */
WORD v_curright(WORD handle);

/*
 * Move alpha cursor left.
 */
WORD v_curleft(WORD handle);

/*
 * Move alpha cursor to home position.
 */
WORD v_curhome(WORD handle);

/*
 * Erase to end of screen.
 */
WORD v_eeos(WORD handle);

/*
 * Erase to end of line.
 */
WORD v_eeol(WORD handle);

/*
 * Set alpha cursor address.
 */
WORD vs_curaddress(WORD handle, WORD row, WORD column);

/*
 * Output alpha text.
 */
WORD v_curtext(WORD handle, BYTE *string);

/*
 * Reverse video on.
 */
WORD v_rvon(WORD handle);

/*
 * Reverse video off.
 */
WORD v_rvoff(WORD handle);

/*
 * Query alpha cursor address.
 */
WORD vq_curaddress(WORD handle, WORD *row, WORD *column);

/*
 * Query tab status.
 */
WORD vq_tabstatus(WORD handle);

/*
 * Produce a hardcopy of the current screen.
 */
WORD v_hardcopy(WORD handle);

/*
 * Display the alpha cursor at a screen location.
 */
WORD v_dspcur(WORD handle, WORD x, WORD y);

/*
 * Remove the alpha cursor.
 */
WORD v_rmcur(WORD handle);

/*
 * Advance the form.
 */
WORD v_form_adv(WORD handle);

/*
 * Set output window.
 */
WORD v_output_window(WORD handle, WORD xy[4]);

/*
 * Clear the display list.
 */
WORD v_clear_disp_list(WORD handle);

/*
 * Output a bit image from a file.
 */
WORD v_bit_image(WORD handle, BYTE *filename, WORD aspect, WORD x_scale,
                 WORD y_scale, WORD h_align, WORD v_align, WORD xy[4]);

/*
 * Set or select a palette.
 */
WORD vs_palette(WORD handle, WORD palette);

/*
 * Set metafile extents.
 */
WORD v_meta_extents(WORD handle, WORD min_x, WORD min_y,
                    WORD max_x, WORD max_y);

/*
 * Write a raw metafile record.
 */
WORD v_write_meta(WORD handle, WORD num_intin, WORD *intin,
                  WORD num_ptsin, WORD *ptsin);

/*
 * Set metafile output filename.
 */
WORD vm_filename(WORD handle, BYTE *filename);

/*
 * Get pixel value and color index.
 */
WORD v_get_pixel(WORD handle, WORD x, WORD y, WORD *pel, WORD *index);

/*
 * Invoke a generic VDI escape function.
 */
WORD v_escape(WORD handle, WORD function, WORD count_in, WORD count_out,
              WORD *intin, WORD *ptsout);

#ifdef __cplusplus
}
#endif

#endif /* GEM_VDI_H */
