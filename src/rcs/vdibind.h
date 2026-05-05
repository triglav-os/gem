/*
 * Declares the VDI binding interface used by the resource construction
 * set. The prototypes expose the graphics services the editor expects from
 * the runtime.
 *
 * MIT License (see: LICENSE)
 * Copyright (C) 2026 tomaz stih
 */

extern WORD v_opnwk(WORD work_in[], WORD *handle, WORD work_out[]);           /*  1   */
extern WORD v_clswk(WORD handle);           /*  2   */
extern WORDv_clrwk(VDI_HANDLE handle);           /*  3   */
extern WORD v_clswk(WORD handle);           /*  4   */

extern WORD vq_chcells(WORD handle, WORD *rows, WORD *columns);        /* 5 1  */
extern WORD v_exit_cur(WORD handle);        /* 5 2  */
extern WORD v_enter_cur(WORD handle);       /* 5 3  */
extern WORD v_curup(WORD handle);           /* 5 4  */
extern WORD v_curdown(WORD handle);         /* 5 5  */
extern WORD v_curright(WORD handle);        /* 5 6  */
extern WORD v_curleft(WORD handle);         /* 5 7  */
extern WORD v_curhome(WORD handle);         /* 5 8  */
extern WORD v_eeos(WORD handle);            /* 5 9  */
extern WORD v_eeol(WORD handle);            /* 5 10 */
extern WORD vs_curaddress(WORD handle, WORD row, WORD column);     /* 5 11 */
extern WORD v_curtext(WORD handle, BYTE *string);         /* 5 12 */
extern WORD v_rvon(WORD handle);            /* 5 13 */
extern WORD v_rvoff(WORD handle);           /* 5 14 */
extern WORD vq_curaddress(WORD handle, WORD *row, WORD *column);     /* 5 15 */
extern WORD vq_tabstatus(WORD handle);      /* 5 16 */
extern    WORD    v_hardcopy ();       /* 5 17 */
extern WORD v_dspcur(WORD handle, WORD x, WORD y);          /* 5 18 */
extern WORD v_rmcur(WORD handle);           /* 5 19 */
extern WORD v_form_adv(WORD handle);        /* 5 20 */
extern WORD v_output_window(WORD handle, WORD xy[4]);   /* 5 21 */
extern WORD v_clear_disp_list(WORD handle); /* 5 22 */
extern    WORD    v_bit_image();       /* 5 23 */
extern    WORD    vq_scan();           /* 5 24 */
extern    WORD    v_alpha_text();      /* 5 25 */
extern WORD vs_palette(WORD handle, WORD palette);        /* 5 60 */
extern WORD v_meta_extents(WORD handle, WORD min_x, WORD min_y, WORD max_x, WORD max_y);    /* 5 98 */
extern    WORD    v_write_meta();      /* 5 99 */
extern WORD vm_filename(WORD handle, BYTE *filename);       /* 5 100*/

extern WORD v_pline(WORD count, WORD *pxyarray);           /*  6   */
extern WORDv_pmarker(WORD handle, WORD count, WORD xy[]);         /*  7   */
extern WORDv_gtext(VDI_HANDLE handle, WORD x, WORD y, CONST BYTE *text);           /*  8   */
extern WORD v_fillarea(WORD count, WORD *pxyarray);        /*  9   */
extern    WORD    v_cellarray();       /*  10  */

extern WORD v_bar(WORD *pxyarray);	       /* 11  1 */
extern WORDv_arc(WORD handle, WORD x, WORD y, WORD radius, WORD begang, WORD endang);	       /* 11  2 */
extern    WORD    v_pieslice();	       /* 11  3 */
extern WORDv_circle(WORD handle, WORD x, WORD y, WORD radius);	       /* 11  4 */
extern WORDv_ellipse(WORD handle, WORD x, WORD y, WORD xrad, WORD yrad);	       /* 11  5 */
extern    WORD    v_ellarc();	       /* 11  6 */
extern    WORD    v_ellpie();	       /* 11  7 */
extern WORDv_rbox(WORD handle, WORD xy[4]);            /* 11  8 */
extern WORDv_rfbox(WORD handle, WORD xy[4]);           /* 11  9 */
extern    WORD    v_justified();       /* 11 10 */

extern WORDvst_height(WORD height, WORD *pchr_width, WORD *pchr_height, WORD *pcell_width, WORD *pcell_height);        /*  12  */
extern WORD vst_rotation(WORD handle, WORD angle);      /*  13  */
extern WORD vs_color(WORD handle, WORD index, WORD rgb[3]);	       /*  14  */
extern WORD vsl_type(WORD handle, WORD style);	       /*  15  */
extern WORD vsl_width(WORD width);	       /*  16  */
extern WORDvsl_color(VDI_HANDLE handle, WORD color_index);	       /*  17  */
extern WORD vsm_type(WORD handle, WORD symbol);	       /*  18  */
extern WORD vsm_height(WORD handle, WORD height);	       /*  19  */
extern WORD vsm_color(WORD handle, WORD color_index);	       /*  20  */
extern WORD vst_font(WORD handle, WORD font);	       /*  21  */
extern WORDvst_color(VDI_HANDLE handle, WORD color_index);	       /*  22  */
extern WORD vsf_interior(WORD handle, WORD style);      /*  23  */
extern WORD vsf_style(WORD handle, WORD style);	       /*  24  */
extern WORDvsf_color(VDI_HANDLE handle, WORD color_index);	       /*  25  */
extern WORD vq_color(WORD handle, WORD index, WORD setflag, WORD rgb[]);	       /*  26  */
extern    WORD    vq_cellarray();      /*  27  */

extern    WORD    vrq_locator();       /*  28  */
extern    WORD    vsm_locator();       /*  28  */
extern WORD vrq_valuator(WORD handle, WORD val_in, WORD *val_out, WORD *term);      /*  29  */
extern WORD vsm_valuator(WORD handle, WORD val_in, WORD *val_out, WORD *term);      /*  29  */
extern WORD vrq_choice(WORD handle, WORD ch_in, WORD *ch_out);        /*  30  */
extern WORD vsm_choice(WORD handle, WORD *choice);        /*  30  */
extern    WORD    vrq_string();        /*  31  */
extern    WORD    vsm_string();        /*  31  */
extern WORD vswr_mode(WORD handle, WORD mode);         /*  32  */
extern WORD vsin_mode(WORD handle, WORD dev_type, WORD mode);         /*  33  */

extern WORD vql_attributes(WORD handle, WORD attrib[]);    /*  35  */
extern WORD vqm_attributes(WORD handle, WORD attrib[]);    /*  36  */
extern WORD vqf_attributes(WORD handle, WORD attrib[]);    /*  37  */
extern WORD vqt_attributes(WORD handle, WORD attrib[]);    /*  38  */
extern WORD vst_alignment(WORD handle, WORD horiz, WORD vert);     /*  39  */

extern WORD v_opnvwk(WORD *pwork_in, WORD *phandle, WORD *pwork_out);          /* 100  */
extern WORDv_clsvwk(VDI_HANDLE handle);          /* 101  */
extern WORD vq_extnd(WORD handle, WORD owflag, WORD work_out[]);          /* 102  */
extern WORDv_contourfill(WORD handle, WORD x, WORD y, WORD index);     /* 103  */

extern WORD vsf_perimeter(WORD handle, WORD per_vis);     /* 104  */
extern WORD vst_background(WORD handle, WORD mode);    /* 105  */
extern WORD vst_effects(WORD handle, WORD effect);       /* 106  */
extern    WORD    vst_point();         /* 107  */
extern WORD vsl_end_style(WORD handle, WORD begstyle, WORD endstyle);     /* 108  */

extern WORD vro_cpyfm(WORD wr_mode, WORD *pxyarray, WORD *psrcMFDB, WORD *pdesMFDB);         /* 109  */
extern WORD vr_trnfm(WORD handle, MFDB *src, MFDB *dst);          /* 110  */
extern WORD vsc_form(WORD handle, MFORM *cur_form);          /* 111  */
extern WORD vsf_udpat(WORD handle, WORD *fill_pat, WORD planes);         /* 112  */
extern WORD vsl_udsty(WORD handle, WORD pattern);         /* 113  */
extern WORD vr_recfl(WORD *pxyarray, FDB *pdesMFDB);          /* 114  */

extern    WORD    vqi_mode();          /* 115  */
extern WORD vqt_extent(WORD handle, char *string, WORD extent[8]);        /* 116  */
extern    WORD    vqt_width();         /* 117  */
extern    WORD    vex_timv();          /* 118  */
extern WORD vst_load_fonts(WORD handle, WORD select);    /* 119  */
extern WORD vst_unload_fonts(WORD handle, WORD select);  /* 120  */
extern WORD vrt_cpyfm(WORD handle, WORD wr_mode, WORD xy[], WORD *srcMFDB, WORD *desMFDB, WORD *index);         /* 121  */
extern WORDv_show_c(VDI_HANDLE handle, WORD reset);          /* 122  */
extern WORDv_hide_c(VDI_HANDLE handle);          /* 123  */
extern WORDvq_mouse(VDI_HANDLE handle, WORD *status, WORD *x, WORD *y);          /* 124  */
extern    WORD    vex_butv();          /* 125  */
extern    WORD    vex_motv();          /* 126  */
extern    WORD    vex_curv();          /* 127  */
extern WORD vq_key_s(WORD handle, WORD *status);          /* 128  */
extern WORDvs_clip(WORD handle, WORD clip_flag, WORD xy[4]);           /* 129  */

extern WORD vqt_name(WORD handle, WORD element_num, BYTE *name);          /* 130  */
extern    WORD    vqt_fontinfo();      /* 131  */
