/*
 * Declares low-level desktop binding constants and helper macros shared by
 * the imported desktop sources. It preserves the original interface shape
 * used between the desktop and GEM libraries.
 *
 * MIT License (see: LICENSE)
 * Copyright (C) 2026 tomaz stih
 */

EXTERN WORD v_opnwk(WORD work_in[], WORD *handle, WORD work_out[]);
EXTERN WORD v_clswk(WORD handle);
EXTERN VOID v_clrwk(WORD handle);
EXTERN WORD v_clswk(WORD handle);
EXTERN WORD vq_chcells(WORD handle, WORD *rows, WORD *columns);
EXTERN WORD v_exit_cur(WORD handle);
EXTERN WORD v_enter_cur(WORD handle);
EXTERN WORD v_curup(WORD handle);
EXTERN WORD v_curdown(WORD handle);
EXTERN WORD v_curright(WORD handle);
EXTERN WORD v_curleft(WORD handle);
EXTERN WORD v_curhome(WORD handle);
EXTERN WORD v_eeos(WORD handle);
EXTERN WORD v_eeol(WORD handle);
EXTERN WORD vs_curaddress(WORD handle, WORD row, WORD column);
EXTERN WORD v_curtext(WORD handle, BYTE *string);
EXTERN WORD v_rvon(WORD handle);
EXTERN WORD v_rvoff(WORD handle);
EXTERN WORD vq_curaddress(WORD handle, WORD *row, WORD *column);
EXTERN WORD vq_tabstatus(WORD handle);
EXTERN WORD	v_hardcopy ();
EXTERN WORD v_dspcur(WORD handle, WORD x, WORD y);
EXTERN WORD	v_rmcur ();
EXTERN VOID v_pline(WORD count, WORD *pxyarray);
EXTERN VOID v_pmarker(WORD handle, WORD count, WORD xy[]);
EXTERN VOID v_gtext(WORD handle, WORD x, WORD y, BYTE *text);
EXTERN WORD v_fillarea(WORD count, WORD *pxyarray);

EXTERN WORD v_bar(WORD *pxyarray);
EXTERN VOID v_circle(WORD handle, WORD x, WORD y, WORD radius);
EXTERN VOID v_arc(WORD handle, WORD x, WORD y, WORD radius, WORD begang, WORD endang);
EXTERN WORD	v_pieslice();
EXTERN VOID v_ellipse(WORD handle, WORD x, WORD y, WORD xrad, WORD yrad);
EXTERN WORD	v_ellarc();
EXTERN WORD	v_ellpie();

EXTERN VOID vst_height(WORD height, WORD *pchr_width, WORD *pchr_height, WORD *pcell_width, WORD *pcell_height);
EXTERN WORD vst_rotation(WORD handle, WORD angle);
EXTERN WORD vs_color(WORD handle, WORD index, WORD rgb[3]);
EXTERN WORD vsl_type(WORD handle, WORD style);
EXTERN VOID vsl_width(WORD width);
EXTERN VOID vsl_color(WORD handle, WORD color_index);
EXTERN WORD vsm_type(WORD handle, WORD symbol);
EXTERN WORD vsm_height(WORD handle, WORD height);
EXTERN WORD vsm_color(WORD handle, WORD color_index);
EXTERN WORD vst_font(WORD handle, WORD font);
EXTERN VOID vst_color(WORD handle, WORD color_index);
EXTERN WORD vsf_interior(WORD handle, WORD style);
EXTERN WORD vsf_style(WORD handle, WORD style);
EXTERN VOID vsf_color(WORD handle, WORD color_index);
EXTERN WORD vq_color(WORD handle, WORD index, WORD setflag, WORD rgb[]);

EXTERN WORD	vrq_locator();
EXTERN WORD	vsm_locator();
EXTERN WORD vrq_valuator(WORD handle, WORD val_in, WORD *val_out, WORD *term);
EXTERN WORD vsm_valuator(WORD handle, WORD val_in, WORD *val_out, WORD *term);
EXTERN WORD vrq_choice(WORD handle, WORD ch_in, WORD *ch_out);
EXTERN WORD vsm_choice(WORD handle, WORD *choice);
EXTERN WORD	vrq_string();
EXTERN WORD	vsm_string();
EXTERN WORD vswr_mode(WORD handle, WORD mode);
EXTERN WORD vsin_mode(WORD handle, WORD dev_type, WORD mode);

EXTERN WORD vsf_perimeter(WORD handle, WORD per_vis);

EXTERN WORD	vr_cpyfm();
EXTERN WORD vr_trnfm(WORD handle, FDB *src, FDB *dst);
EXTERN WORD vsc_form(WORD handle, MFORM *cur_form);
EXTERN WORD vsf_udpat(WORD handle, WORD *fill_pat, WORD planes);
EXTERN WORD vsl_udsty(WORD handle, WORD pattern);
EXTERN VOID vr_recfl(WORD *pxyarray, WORD *pdesMFDB);
EXTERN VOID v_show_c(WORD handle, WORD reset);
EXTERN VOID v_hide_c(WORD handle);
EXTERN VOID vq_mouse(WORD handle, WORD *status, WORD *x, WORD *y);
EXTERN WORD	vex_butv();
EXTERN WORD	vex_motv();
EXTERN WORD	vex_curv();
EXTERN WORD vq_key_s(WORD handle, WORD *status);
