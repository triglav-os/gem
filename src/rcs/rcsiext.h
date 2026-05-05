/*
 * Declares cross-module interfaces shared by the resource construction set
 * icon-editing code. It groups the editor entry points that are specific
 * to icon work.
 *
 * MIT License (see: LICENSE)
 * Copyright (C) 2026 tomaz stih
 */

EXTERN	     WORD       flags;
EXTERN	     WORD       out1;
EXTERN	     WORD       xwait;
EXTERN	     WORD       ywait;
EXTERN	     WORD       wwait;
EXTERN	     WORD       hwait;
EXTERN	     WORD       out2;
EXTERN	     LONG       ad_rmsg;
EXTERN       WORD       rcs_rmsg[];
EXTERN	     GRECT      wait;
EXTERN	     WORD       mousex;
EXTERN	     WORD       mousey;
EXTERN	     WORD       bstate;
EXTERN	     WORD       kstate;
EXTERN	     WORD       kreturn;
EXTERN	     WORD       bclicks;
EXTERN	     WORD       rcs_edited;
EXTERN       GRECT      tools;
EXTERN       BYTE 	icn_file[];
EXTERN       WORD	rcs_xpert;
EXTERN	     UWORD      bit_mask[];
EXTERN	     WORD       gl_tcolor;
EXTERN	     WORD       gl_mode;
EXTERN	     WORD       gl_lcolor;
EXTERN	     GRECT      full;
EXTERN	     WORD       rcs_xpan;
EXTERN	     WORD       rcs_ypan;
EXTERN	     LONG       src_mp;
EXTERN	     LONG       dst_mp;
EXTERN	     GRECT      pbx;

#define      FINGER      3
#define      ARROW       0
EXTERN	     WORD       rcs_hot;
 
EXTERN       LONG	ad_menu;
EXTERN       WORD	icn_state;
#define      NOFILE_STATE	0
#define      FILE_STATE		1
EXTERN       WORD	OK_NOICN[];
EXTERN       WORD	OK_ICN[];
EXTERN       BOOLEAN	icn_edited;
EXTERN       WORD	OK_EDITED[];
EXTERN       BOOLEAN	gl_isicon;
EXTERN       LONG	ad_tools;
EXTERN       LONG       ad_itool;
EXTERN       LONG       ad_idisp;
EXTERN       GRECT	fat_area;
EXTERN       GRECT	 gridbx;
EXTERN       LONG	ad_view;
EXTERN       WORD	gl_wimage;
#define  gl_sqsize  8
EXTERN       WORD	gl_himage;
EXTERN       GRECT	view;
EXTERN       GRECT      itool;
EXTERN       GRECT      idisp;
EXTERN       LONG	ad_pbx;
EXTERN       GRECT	src_img;
EXTERN       GRECT	dat_img;
EXTERN       GRECT	mas_img;
EXTERN       GRECT	icn_img;
EXTERN       BOOLEAN	iconedit_flag;
EXTERN       MFDB	hold_mfdb;
EXTERN       MFDB	hld2_mfdb;
EXTERN       MFDB	disp_mfdb;
EXTERN       MFDB	undo_mfdb;
EXTERN       MFDB	und2_mfdb;
EXTERN       MFDB	clip_mfdb;
EXTERN       MFDB	clp2_mfdb;
EXTERN       MFDB	save_mfdb;
EXTERN       MFDB	sav2_mfdb;
EXTERN       MFDB	fat_mfdb;
EXTERN       LONG	save_tree;
EXTERN	     WORD	save_obj;
EXTERN       WORD	rcs_sel[];
EXTERN       WORD	rcs_view;
EXTERN       BYTE	rcs_title[];
EXTERN       WORD	rcs_state;
EXTERN       WORD	colour;
EXTERN       WORD	fgcolor;
EXTERN       WORD	bgcolor;
EXTERN	     WORD	old_fc;
EXTERN       GRECT	hold_area;  
EXTERN       GRECT	selec_area;
EXTERN	     GRECT	flash_area;
EXTERN       GRECT	clip_area;  
EXTERN       WORD	partp;
EXTERN	     WORD	deltax;
EXTERN	     WORD	deltay;
EXTERN       LONG	gl_icnspec;  
EXTERN       WORD	gl_datasize;  
EXTERN       WORD	gl_nplanes;  
EXTERN       WORD	gl_width;  
EXTERN       GRECT	scrn_area;  
EXTERN       WORD	gl_height;  
EXTERN       MFDB	scrn_mfdb;  
EXTERN       GRECT	scroll_fat;  
EXTERN       BOOLEAN	is_mask;  
EXTERN       WORD	rcs_nsel;  
#define     MAX_ICON_W	0x0040
#define     MAX_ICON_H  0x0040
EXTERN       LONG	ibuff_size;  
EXTERN       WORD	pen_on;  
EXTERN	     BOOLEAN	grid;
EXTERN	     BOOLEAN	clipped;
EXTERN	     BOOLEAN	paste_img;
EXTERN	     BOOLEAN	selecton;
EXTERN	     BOOLEAN    inverted;
EXTERN	     WORD	invert3[8];
EXTERN	     WORD	invert4[16];

EXTERN VOID gsx_sclip(GRECT *pt);
EXTERN VOID gsx_attr(WORD text, WORD mode, WORD color);
EXTERN WORD v_pline(WORD count, WORD *pxyarray);
EXTERN       VOID       gr_rect();
EXTERN WORD wind_get(WORD w_handle, WORD w_field, WORD *pw1, WORD *pw2, WORD *pw3, WORD *pw4);
EXTERN VOID vrn_trnfm(FDB *psrcMFDB, FDB *pdesMFDB);
EXTERN       VOID       MAGNIFY();
EXTERN	     VOID       B_MOVE();
EXTERN WORD objc_draw(LONG tree, WORD drawob, WORD depth, WORD xc, WORD yc, WORD wc, WORD hc);
EXTERN	     VOID       outl_img();
EXTERN VOID set_fatbox(VOID);
EXTERN WORD graf_mouse(WORD m_number, LONG m_addr);
EXTERN WORD evnt_button(WORD clicks, UWORD mask, UWORD state, WORD *pmx, WORD *pmy, WORD *pmb, WORD *pks);
EXTERN UWORD inside(UWORD x, UWORD y, UWORD tx, UWORD ty, UWORD tw, UWORD th);
EXTERN VOID graf_rubbox(WORD xorigin, WORD yorigin, WORD wmin, WORD hmin, WORD *pwend, WORD *phend);
EXTERN VOID set_icnmenus(VOID);
EXTERN       VOID	disab_obj();
EXTERN	     VOID       hot_off();
EXTERN WORD objc_find(LONG tree, WORD startob, WORD depth, WORD mx, WORD my);
EXTERN	     WORD       GET_FLAGS();
EXTERN	     VOID       invert_obj();
EXTERN	     WORD       hndl_pop();
EXTERN VOID init_img(VOID);
EXTERN WORD hndl_menu(WORD title, WORD item);
EXTERN	     VOID       hndl_redraw();
EXTERN	     VOID       hndl_arrowed();
EXTERN	     VOID       hndl_hslid();
EXTERN	     VOID       hndl_vslid();
EXTERN	     WORD       evnt_multi();
EXTERN       VOID       send_redraw();


EXTERN       WORD	GET_NEXT();  
EXTERN       WORD	GET_HEAD();
EXTERN VOID map_tree(LONG tree, WORD this, WORD last, WORD (*routine)());
EXTERN       VOID	 enab_obj();  
EXTERN       VOID	 sble_obj();  
EXTERN WORD objc_offset(LONG tree, WORD obj, WORD *poffx, WORD *poffy);
EXTERN WORD rc_intersect(GRECT *p1, GRECT *p2);
EXTERN WORD rc_equal(GRECT *a, GRECT *b);
EXTERN       WORD	 objc_xywh();  
EXTERN WORD dos_free(LONG maddr);
EXTERN       VOID	view_objs();  
EXTERN       VOID	ini_panes();  
EXTERN WORD wind_set(WORD handle, WORD field, WORD w1, WORD w2, WORD w3, WORD w4);
EXTERN       VOID	new_state();  
EXTERN       WORD	ini_tree();  
EXTERN       VOID	SET_WIDTH();  
EXTERN       VOID	SET_HEIGHT();  
EXTERN       VOID	SET_SPEC();  
EXTERN       LONG	GET_SPEC();  
#define      BI_PDATA(x)  (x)
#define      IB_PDATA(x)  (x + 4)
#define      BI_WB(x)     (x + 4)
#define      BI_HL(x)     (x + 6)
#define      BI_COLOR(x)  (x + 12)
#define      IB_CHAR(x)   (x + 12)
#define      IB_PMASK(x)  (x)
#define	     IB_XICON(x)  (x + 18)
#define      IB_WICON(x)  (x + 22)   
#define      IB_HICON(x)  (x + 24)
EXTERN       VOID	unhide_obj();  
EXTERN       VOID	hide_obj();  
EXTERN VOID rast_op(WORD mode, GRECT *s_area, MFDB *s_mfdb, GRECT *d_area, MFDB *d_mfdb);
EXTERN       VOID	gsx_outline();  
EXTERN       WORD	GET_TYPE();  
EXTERN       VOID	copy_spec();  
EXTERN       LONG	get_mem();  
#define      OB_TYPE(x)  (tree + (x) * sizeof(OBJECT) + 6 )
#define      OB_STATE(x) (tree + (x) * sizeof(OBJECT) + 10)
EXTERN	     VOID       solid_img();
EXTERN VOID init_img(VOID);
EXTERN	     WORD       writ_icon ();
EXTERN VOID get_icnfile(BYTE *full_path);
EXTERN       WORD       abandon_button();
EXTERN	     VOID       load_part();
EXTERN	     VOID       icn_init();
EXTERN VOID undo_img(VOID);

EXTERN WORD wind_update(WORD beg_update);
EXTERN WORD menu_tnormal(OBJECT *tree, WORD title, WORD normal);
EXTERN LONG dos_alloc(LONG nbytes);
EXTERN       VOID	clr_hot();  
EXTERN       int main(void);
EXTERN VOID save_fat(BOOLEAN undo);
EXTERN VOID fb_redraw(VOID);
EXTERN       WORD	hndl_dial();
EXTERN WORD min(WORD a, WORD b);
EXTERN WORD max(WORD a, WORD b);
EXTERN       VOID       set_pix();	/*may not be needed*/
EXTERN VOID fattify(GRECT * area);