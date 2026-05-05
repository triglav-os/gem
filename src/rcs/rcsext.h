/*
 * Declares cross-module interfaces shared across the resource construction
 * set implementation. It serves as the main internal prototype catalogue
 * for the editor.
 *
 * MIT License (see: LICENSE)
 * Copyright (C) 2026 tomaz stih
 */

EXTERN VOID init_img(VOID);
EXTERN VOID rast_op(WORD mode, GRECT *s_area, MFDB *s_mfdb, GRECT *d_area, MFDB *d_mfdb);
EXTERN	LONG	SEG_OFF();
EXTERN	VOID	B_MOVE();
EXTERN	VOID	MAGNIFY();
EXTERN	WORD	ok_rvrt_files();
EXTERN  WORD    cont_rcsinit();
EXTERN  VOID    icon_edit();
EXTERN	VOID	undo_menufix();
EXTERN VOID desk_menufix(LONG tree);
EXTERN VOID ob_relxywh(LONG tree, WORD obj, GRECT *pt);
EXTERN WORD shel_find(char *path);
EXTERN	WORD	quit_rsc();
EXTERN  VOID	slid_objs();
EXTERN  VOID    slid_trees();
EXTERN	VOID	read_inf();
EXTERN	VOID	set_rootxy();
EXTERN	VOID	wrte_inf();
EXTERN VOID movup(WORD num, BYTE *ps, BYTE *pd);
EXTERN  VOID    del_objindex();
EXTERN VOID gsx_1code(WORD code, WORD val);
EXTERN	WORD	gl_apid;
EXTERN	LONG	string_addr();
EXTERN VOID movs(WORD num, BYTE *ps, BYTE *pd);
EXTERN WORD strcmp(BYTE *p1, BYTE *p2);
EXTERN WORD hndl_menu(WORD title, WORD item);
EXTERN	VOID 	set_sliders();
EXTERN	VOID	merge_str();
EXTERN  WORD	hndl_alert();
EXTERN VOID map_tree(LONG tree, WORD this, WORD last, WORD (*routine)());
EXTERN WORD strlen(BYTE *p1);
EXTERN	VOID	LLSTRCPY();
EXTERN	LONG	GET_SPEC();
EXTERN	WORD	GET_NEXT();
EXTERN	WORD	GET_TYPE();
EXTERN	VOID	SET_SPEC();
EXTERN	VOID	SET_NEXT();
EXTERN	WORD	GET_HEAD();
EXTERN	VOID	SET_HEAD();
EXTERN	WORD	GET_TAIL();
EXTERN	VOID	SET_TAIL();
EXTERN	WORD	LLSTRCMP();
EXTERN VOID    count_free();
EXTERN VOID    dcomp_tree();
 /*	rcsinit.c	6/21/85		Tim Oren		*/


#if PCDOS
EXTERN	WORD	DOS_BX;
#endif

EXTERN LONG dos_alloc(LONG nbytes);
EXTERN LONG dos_avail(VOID);
EXTERN	WORD	ini_tree();		/* Defined in rcslib.c. */
EXTERN	LONG	image_addr();
EXTERN	VOID	trans_bitblk();
EXTERN	VOID	trans_obj();
EXTERN WORD rc_copy(GRECT *src, GRECT *dst);
EXTERN	VOID	set_slsize();
EXTERN	WORD mul_div(WORD a, WORD b, WORD c);
EXTERN	VOID	view_tools();		/* Defined in rcsmain.c. */
EXTERN	VOID	view_parts();
EXTERN	VOID	clr_title();
EXTERN	VOID	send_redraw();
EXTERN WORD rsrc_load(char *filename);
EXTERN WORD wind_get(WORD w_handle, WORD w_field, WORD *pw1, WORD *pw2, WORD *pw3, WORD *pw4);
EXTERN WORD wind_create(UWORD kind, WORD wx, WORD wy, WORD ww, WORD wh);
EXTERN WORD wind_open(WORD handle, WORD wx, WORD wy, WORD ww, WORD wh);
EXTERN UWORD appl_init(VOID);
EXTERN WORD shel_read(char *cmd, char *tail);
EXTERN WORD wind_update(WORD beg_update);
EXTERN WORD graf_handle(WORD *pwchar, WORD *phchar, WORD *pwbox, WORD *phbox);
EXTERN WORD v_pline(WORD count, WORD *pxyarray);
EXTERN VOID v_arc(WORD handle, WORD x, WORD y, WORD radius, WORD begang, WORD endang);
EXTERN VOID vsf_color(VDI_HANDLE handle, WORD color_index);
EXTERN VOID gsx_vopen(VOID);
EXTERN WORD gsx_start(VOID);
EXTERN	VOID	mouse_form();
EXTERN WORD menu_bar(OBJECT *tree, WORD show);
EXTERN	VOID	new_state();
EXTERN	VOID	ini_buff();
EXTERN	VOID	get_path();
EXTERN	VOID	r_to_xfile();
EXTERN	VOID	rsc_title();
EXTERN	WORD	rvrt_files();
EXTERN	VOID	view_trees();
EXTERN WORD dos_free(LONG maddr);
EXTERN WORD wind_close(WORD handle);
EXTERN WORD wind_delete(WORD handle);
EXTERN WORD rsrc_free(void);
EXTERN VOID gsx_vclose(VOID);
EXTERN WORD appl_exit(void);
EXTERN	VOID	wait_tools();
/*	rcsmain.c	10/08/84 - 1/25/85  	Tim Oren		*/

EXTERN WORD mul_div(WORD a, WORD b, WORD c);
EXTERN WORD get_parent(LONG tree, WORD obj);
EXTERN WORD rc_intersect(GRECT *p1, GRECT *p2);
EXTERN	WORD	open_rsc();		/* Defined in rcsfiles.c. */
EXTERN	VOID	save_rsc();
EXTERN	WORD	svas_rsc();
EXTERN	WORD	rvrt_rsc();
EXTERN	WORD	new_rsc();
EXTERN	VOID	del_tree();		/* Defined in rcstrees.c. */
EXTERN	VOID	new_tree();
EXTERN  WORD	mov_tree();
EXTERN	VOID	open_tree();
EXTERN	VOID	clos_tree();
EXTERN	VOID	name_tree();
EXTERN  VOID	select_tree();
EXTERN	VOID	dselct_tree();
EXTERN	VOID	dup_tree();
EXTERN	VOID	cut_tree();
EXTERN	VOID	paste_tree();
EXTERN	WORD	fit_htrees();
EXTERN	WORD	need_vtrees();
EXTERN	WORD	fit_vtrees();
EXTERN	VOID	do_trsinc();
EXTERN	WORD	trpan_f();
EXTERN	VOID	del_ob();		/* Defined in rcsobjs.c. */
EXTERN	WORD	new_obj();
EXTERN	VOID	mov_obj();
EXTERN	VOID	open_obj();
EXTERN	VOID	type_obj();
EXTERN  VOID	size_obj();
EXTERN	VOID	unhid_part();
EXTERN	VOID	flatten_part();
EXTERN	VOID	sort_part();
EXTERN	VOID	slct_obj();
EXTERN	VOID	dslct_obj();
EXTERN	VOID	cut_obj();
EXTERN	VOID	paste_obj();
EXTERN	WORD	snap_trs();
EXTERN	WORD	clamp_trs();
EXTERN	WORD	snap_xs();
EXTERN	WORD	clamp_xs();
EXTERN	WORD	snap_ys();
EXTERN	WORD	clamp_ys();
EXTERN	VOID	load_part();		/* Defined in rcsedit.c. */
EXTERN	WORD	file_view();
EXTERN	VOID	type_tree();
EXTERN	VOID	hot_off();
EXTERN WORD objc_find(LONG tree, WORD startob, WORD depth, WORD mx, WORD my);
EXTERN	VOID	wait_obj();
EXTERN	WORD	GET_FLAGS();
EXTERN	VOID	invert_obj();
EXTERN	WORD	pane_find();
EXTERN WORD graf_mouse(WORD m_number, LONG m_addr);
EXTERN	VOID	objc_xywh();
EXTERN	WORD	GET_WIDTH();
EXTERN	WORD	GET_HEIGHT();
EXTERN	VOID	hot_dragbox();
EXTERN	WORD	tree_view();
EXTERN	VOID	obj_handle();
EXTERN UWORD inside(UWORD x, UWORD y, UWORD tx, UWORD ty, UWORD tw, UWORD th);
EXTERN	VOID	do_bgcol();
EXTERN	VOID	do_patrn();
EXTERN	VOID	do_bdcol();
EXTERN	VOID	do_thick();
EXTERN	VOID	do_fgcol();
EXTERN	VOID	do_rule();
EXTERN	VOID	do_posn();
EXTERN	VOID	do_swtch();
EXTERN	VOID	set_switch();
EXTERN	WORD	hndl_pop();
EXTERN WORD graf_mkstate(WORD *pmx, WORD *pmy, WORD *pmstate, WORD *pkstate);
EXTERN	WORD	clos_rsc();
EXTERN	VOID	merge_rsc();
EXTERN	VOID	info_dial();
EXTERN WORD menu_text(OBJECT *tree, WORD item, char *text);
EXTERN	VOID	ini_panes();
EXTERN	VOID	redo_trees();
EXTERN	VOID	view_objs();
EXTERN	VOID	outp_dial();
EXTERN	VOID	safe_dial();
EXTERN	VOID	about_dial();
EXTERN WORD menu_tnormal(OBJECT *tree, WORD title, WORD normal);
EXTERN WORD appl_write(WORD rwid, WORD length, LONG pbuff);
EXTERN VOID gsx_moff(VOID);
EXTERN VOID gsx_sclip(GRECT *pt);
EXTERN WORD rc_equal(GRECT *a, GRECT *b);
EXTERN	VOID	gr_rect();
EXTERN WORD objc_draw(LONG tree, WORD drawob, WORD depth, WORD xc, WORD yc, WORD wc, WORD hc);
EXTERN VOID gsx_xbox(GRECT *pt);
EXTERN VOID gsx_mon(VOID);
EXTERN	VOID	do_hsinc();
EXTERN	VOID	do_vsinc();
EXTERN WORD max(WORD a, WORD b);
EXTERN WORD wind_set(WORD handle, WORD field, WORD w1, WORD w2, WORD w3, WORD w4);
EXTERN	WORD	evnt_multi();
EXTERN WORD evnt_button(WORD clicks, UWORD mask, UWORD state, WORD *pmx, WORD *pmy, WORD *pmb, WORD *pks);
EXTERN VOID graf_rubbox(WORD xorigin, WORD yorigin, WORD wmin, WORD hmin, WORD *pwend, WORD *phend);
EXTERN WORD evnt_mouse(WORD flags, WORD x, WORD y, WORD width, WORD height, WORD *pmx, WORD *pmy, WORD *pmb, WORD *pks);
EXTERN	WORD	rcs_init();
EXTERN	VOID	rcs_exit();
EXTERN	VOID	hndl_dsel();
EXTERN	VOID	obj_redraw();
EXTERN	VOID	obj_nowdraw();
EXTERN WORD do_redraw(WORD w_handle, WORD obj_disp, WORD depth, WORD xc, WORD yc, WORD wc, WORD hc);
EXTERN	VOID	hndl_redraw();

/*	rcsintf.c	06/25/85  	Tim Oren		*/

EXTERN	WORD	gl_nplanes;


EXTERN	VOID	clr_tally();		/* Defined in rcsdata.c. */
EXTERN	VOID	enab_obj();
EXTERN	VOID	disab_obj();
EXTERN	VOID	sble_obj();
EXTERN	VOID	unsble_obj();
EXTERN	VOID	chek_obj();
EXTERN	VOID	unchek_obj();
EXTERN	VOID	tally_obj();
EXTERN	VOID	map_all();
EXTERN	WORD	find_tree();
EXTERN	WORD	find_obj();
EXTERN	BYTE	*get_name();
EXTERN	LONG	tree_addr();
EXTERN	LONG	avail_mem();
EXTERN	VOID 	menu_opts();
EXTERN WORD form_center(OBJECT *tree, WORD *cx, WORD *cy, WORD *cw, WORD *ch);
EXTERN WORD form_dial(WORD dtype, WORD ix, WORD iy, WORD iw, WORD ih, WORD x, WORD y, WORD w, WORD h);

EXTERN WORD form_do(OBJECT *tree, WORD startob);
EXTERN WORD form_alert(WORD defbut, LONG astring);
EXTERN VOID ob_setxywh(LONG tree, WORD obj, GRECT *pt);
EXTERN	VOID	arrange_tree();
EXTERN	WORD	lock_menus();
EXTERN	WORD	GET_STATE();
EXTERN WORD objc_offset(LONG tree, WORD obj, WORD *poffx, WORD *poffy);
EXTERN VOID bb_save(GRECT *ps);
EXTERN	VOID	outl_obj();
EXTERN VOID bb_restore(GRECT *pr);
EXTERN VOID gsx_attr(WORD text, WORD mode, WORD color);
EXTERN WORD min(WORD a, WORD b);
EXTERN VOID rc_constrain(GRECT *pc, GRECT *pt);
EXTERN VOID r_set(GRECT *pxywh, WORD x, WORD y, WORD w, WORD h);
EXTERN	VOID	desel_obj();
EXTERN	VOID	tally_free();
EXTERN	VOID	hide_obj();
EXTERN	VOID	SET_X();
EXTERN	VOID	unhide_obj();
EXTERN	VOID	sel_obj();
EXTERN  VOID	SET_WIDTH();
/*	rcsfiles.c	10/08/84 - 1/25/85	Tim Oren		*/


		/* Defined in rcslib.c. */
EXTERN	WORD	read_files();		/* Defined in rcsread.c. */
EXTERN	WORD	write_files();		/* Defined in rcswrite.c. */
EXTERN WORD dos_gdrv(VOID);
EXTERN WORD dos_gdir(WORD drive, LONG pdrvpath);
EXTERN WORD fsel_input(LONG pipath, LONG pisel, WORD *pbutton);
EXTERN VOID dos_chdir(LONG pdrvpath);
EXTERN	WORD	open_files();
EXTERN	VOID	set_title();
EXTERN	WORD	merge_files();

 /*	rcsread.c	1/28/85 - 1/28/85	Tim Oren		*/

EXTERN  UWORD   swap_bytes();
EXTERN  WORD	dmcopy();		/* Defined in rcslib.c. */
EXTERN	VOID	clr_clip();
EXTERN  WORD	new_index();
EXTERN	VOID	set_value();
EXTERN	BYTE	*get_value();
EXTERN	WORD	get_kind();
EXTERN	WORD	find_value();
EXTERN	LONG	str_addr();
EXTERN	LONG	img_addr();
EXTERN	LONG	get_mem();
EXTERN	LONG	copy_tree();
EXTERN	WORD	copy_objs();
EXTERN	VOID	add_trindex();
EXTERN	VOID	comp_alerts();		/* Defined in rcsalert.c. */
EXTERN LONG dos_lseek(WORD handle, WORD smode, LONG sofst);
EXTERN	WORD	read_piece();
EXTERN	WORD	get_file();
EXTERN WORD dos_open(LONG pname, WORD access);
EXTERN WORD form_error(WORD errnum);
EXTERN WORD dos_close(WORD handle);
EXTERN WORD objc_add(OBJECT *tree, WORD parent, WORD child);
EXTERN  VOID	set_kind();
EXTERN	VOID	SET_HEIGHT();
EXTERN	WORD	find_name();
EXTERN	WORD	tree_kind();
EXTERN VOID rs_obfix(LONG tree, WORD curob);
EXTERN	VOID	unique_name();

 /*	rcswrite.c	1/28/85 - 1/28/85	Tim Oren		*/

EXTERN  VOID	strup();
EXTERN VOID gsx_untrans(LONG saddr, WORD swb, LONG daddr, WORD dwb, WORD h);
EXTERN	LONG	tree_ptr();
EXTERN	LONG	str_ptr();
EXTERN	LONG	img_ptr();
EXTERN	BYTE	*c_lookup();
EXTERN	VOID	indx_obs();
EXTERN	WORD	fnd_ob();
EXTERN	VOID	dcomp_alerts();
EXTERN VOID dos_space(WORD drv, LONG *ptotal, LONG *pavail);
EXTERN  WORD	write_piece();
EXTERN	VOID	LWINC();
EXTERN	VOID fix_chpos(LONG pfix, WORD ifx);
EXTERN	VOID	SET_FLAGS();
EXTERN WORD dos_create(LONG pname, WORD attr);
EXTERN  VOID	dcomp_free();
EXTERN	VOID	ini_prog();
EXTERN	VOID	show_prog();

/*	rcstrees.c	11/6/84 -  1/25/85 	Tim Oren		*/


EXTERN  WORD	hndl_dial();
		/* Defined in rcsdata.c. */
EXTERN	VOID	zap_objindex();
EXTERN	WORD	name_ok();
		/* Defined in rcslib.c. */
EXTERN	VOID	snap_wh();
EXTERN	WORD	encode();
EXTERN	VOID	set_slpos();
EXTERN	VOID	hndl_locked();
EXTERN	VOID	fix_alert();
EXTERN VOID graf_growbox(WORD orgx, WORD orgy, WORD orgw, WORD orgh, WORD x, WORD y, WORD w, WORD h);
EXTERN	WORD	GET_X();
EXTERN	WORD	GET_Y();
EXTERN	VOID	SET_Y();
EXTERN VOID graf_shrinkbox(WORD orgx, WORD orgy, WORD orgw, WORD orgh, WORD x, WORD y, WORD w, WORD h);
EXTERN	WORD	set_obname();
EXTERN	VOID	get_obname();
EXTERN	VOID	set_menus();
EXTERN	VOID	clr_hot();
/*	rcsobjs.c	11/20/84 - 1/25/85 	Tim Oren		*/


EXTERN	WORD	nth_child();
EXTERN	VOID	snap_xy();
EXTERN	WORD	in_bar();		/* Defined in rcsmenu.c. */
EXTERN	WORD	in_menu();
EXTERN	WORD	is_menu();
EXTERN	WORD	menu_n();
EXTERN	WORD	bar_n();
EXTERN	WORD	get_active();
EXTERN	WORD	blank_menu();
EXTERN	WORD menu_bar(OBJECT *tree, WORD show);
EXTERN	VOID	dslct_1obj();
EXTERN	VOID	mslct_obj();
EXTERN	VOID	map_dslct();
EXTERN	VOID	table_code();
EXTERN	WORD	make_num();
EXTERN	VOID	set_hot();
EXTERN WORD objc_delete(OBJECT *tree, WORD object);
EXTERN	WORD	menu_ok();
EXTERN	VOID	SET_STATE();
EXTERN WORD objc_order(OBJECT *tree, WORD object, WORD newpos);
EXTERN	WORD	pt_roomp();
EXTERN	VOID	clamp_rubbox();
/*	rcsalert.c	 1/17/85 - 1/17/85	Tim Oren		*/


EXTERN	LONG	mak_trindex();
EXTERN	LONG	mak_frstr();
		
/*	rcsmenu.c	 1/27/85 - 1/25/85 	Tim Oren		*/


EXTERN	LONG	get_obmem();		/* Defined in rcsdata.c. */
EXTERN	WORD	child_of();
/*	rcsedit.c	12/26/84 - 1/25/85  	Tim Oren		*/


EXTERN  WORD	newsize_obj();
EXTERN  VOID	update_if();
EXTERN  VOID	text_wh();
EXTERN  VOID	icon_wh();
EXTERN  WORD	in_which_menu();
EXTERN  WORD	posn_obj();
/*	rcsload.c	7/3/85		Tim Oren		*/

EXTERN VOID gsx_trans(LONG saddr, WORD swb, LONG daddr, WORD dwb, WORD h);	/* Defined in rcsvdi.c. */
EXTERN	VOID	icon_tfix();
/*	rcslib.c	10/11/84 - 1/25/85 	Tim Oren		*/


EXTERN  WORD	DOS_ERR;
EXTERN	WORD	gl_wchar;
EXTERN	WORD	gl_hchar;
EXTERN VOID rc_union(GRECT *p1, GRECT *p2);
EXTERN	VOID	gsx_outline();
EXTERN	VOID	gsx_invert ();

EXTERN WORD rsrc_gaddr(WORD type, WORD index, void **addr);
EXTERN WORD ch_height(WORD fn);
EXTERN WORD ch_width(WORD fn);
/*	rcsvdi.c	04/09/85	Tim Oren converted from:	*/
/*	apgsxif.c	05/06/84 - 12/21/84	Lee Lorenzen		*/


EXTERN VOID	vdi();

#if	I8086
EXTERN VOID i_ptsin(WORD *ptr);
EXTERN VOID i_intin(WORD *ptr);
EXTERN VOID i_ptsout(WORD *ptr);
EXTERN VOID i_intout(WORD *ptr);
#endif

EXTERN VOID i_ptr(VOID *ptr);
EXTERN VOID i_ptr2(VOID *ptr);


EXTERN VOID r_get(GRECT *pxywh, WORD *px, WORD *py, WORD *pw, WORD *ph);
EXTERN VOID bb_fill(WORD mode, WORD fis, WORD patt, WORD hx, WORD hy, WORD hw, WORD hh);
EXTERN VOID gsx_xline(WORD ptscount, WORD *ppoints);
EXTERN VOID gsx_ncode(WORD code, WORD n, WORD m);
EXTERN VOID gsx_1code(WORD code, WORD val);
EXTERN VOID vst_clip(WORD clip_flag, WORD *pxyarray);
EXTERN VOID vrn_trnfm(FDB *psrcMFDB, FDB *pdesMFDB);
EXTERN WORD vro_cpyfm(WORD wr_mode, WORD *pxyarray, WORD *psrcMFDB, WORD *pdesMFDB);
EXTERN WORD vr_recfl(WORD *pxyarray, FDB *pdesMFDB);
