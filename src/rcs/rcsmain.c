/*
 * Implements the main event loop, command dispatch, and redraw flow for
 * the resource construction set. It is the primary reference file for
 * understanding editor behavior.
 *
 * MIT License (see: LICENSE)
 * Copyright (C) 2026 tomaz stih
 */

#include "portab.h"
#include "machine.h"
#include "obdefs.h"
#include "gembind.h"
#include "rcsdefs.h"
#include "rcs.h"
#include "rcskeybd.h"
EXTERN VOID fb_redraw(VOID);
EXTERN VOID icon_size(WORD *pwidth, WORD *pheight);
EXTERN VOID save_fat(BOOLEAN undo);
EXTERN  WORD     new_img();
EXTERN  WORD     icnopen();
EXTERN  WORD     save_icn();
EXTERN  WORD     svas_icn();
EXTERN  WORD     rvrt_icn();
EXTERN  VOID	 outl_img();
EXTERN VOID rubrec_off(VOID);
EXTERN VOID draw_clipb(BOOLEAN mouse, GRECT *dirty);
EXTERN VOID del_img(BOOLEAN mouse, WORD which);
EXTERN	VOID	   copy_img();
EXTERN VOID cut_img(VOID);
EXTERN  VOID       solid_img();
EXTERN	VOID	   invrt_img();
EXTERN 	VOID       enab_obj();
EXTERN	WORD       file_view();
EXTERN  VOID       cut_tree();
EXTERN  VOID       icn_init();
EXTERN  VOID	   clear_img();
EXTERN  WORD       trpan_f();
EXTERN VOID set_icnmenus(VOID);
EXTERN  VOID       cut_obj();
EXTERN  LONG      image_addr();
EXTERN  VOID       paste_tree();
EXTERN  VOID       paste_obj();
EXTERN  VOID       del_tree();
EXTERN   VOID      del_ob();
EXTERN   VOID      new_tree();
EXTERN   WORD      new_obj();
EXTERN   VOID      dup_tree();
EXTERN   WORD      mov_tree();
EXTERN   VOID      mov_obj();
EXTERN   VOID      type_tree();
EXTERN   VOID      type_obj();
EXTERN   VOID      hot_off();
EXTERN WORD objc_find(LONG tree, WORD startob, WORD depth, WORD mx, WORD my);
EXTERN   VOID      wait_tools();
EXTERN   VOID      wait_obj();
EXTERN   WORD      GET_FLAGS();
EXTERN   VOID      invert_obj();
EXTERN   WORD      pane_find();
EXTERN WORD graf_mouse(WORD m_number, LONG m_addr);
EXTERN   VOID      objc_xywh();
EXTERN   WORD      GET_WIDTH();
EXTERN	VOID	hot_dragbox();
EXTERN	WORD	tree_view();
EXTERN	VOID	obj_handle();
EXTERN UWORD inside(UWORD x, UWORD y, UWORD tx, UWORD ty, UWORD tw, UWORD th);
EXTERN   WORD      GET_HEIGHT();
EXTERN   VOID       size_obj();
EXTERN   WORD      GET_NEXT();
EXTERN   VOID       open_obj();
EXTERN   VOID       open_tree();
EXTERN   VOID       select_tree();
EXTERN   WORD      GET_HEAD();
EXTERN   VOID      slct_obj();
EXTERN   VOID      dselct_tree();
EXTERN   VOID      dslct_obj();
EXTERN WORD graf_mkstate(WORD *pmx, WORD *pmy, WORD *pmstate, WORD *pkstate);
EXTERN   WORD      disab_obj();
EXTERN WORD get_parent(LONG tree, WORD obj);
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
EXTERN	WORD	rcs_init();
EXTERN	VOID	rcs_exit();
EXTERN	WORD	open_rsc();		/* Defined in rcsfiles.c. */
EXTERN	VOID	save_rsc();
EXTERN	WORD	svas_rsc();
EXTERN	WORD	rvrt_rsc();
EXTERN	WORD	new_rsc();
EXTERN  VOID    clos_tree();
EXTERN  WORD    quit_rsc();
EXTERN  VOID    name_tree();
EXTERN  VOID    load_part();
EXTERN  VOID    icon_edit();
EXTERN	VOID	unhid_part();
EXTERN	VOID	flatten_part();
EXTERN	VOID	sort_part();
EXTERN  LONG    string_addr();
EXTERN	WORD	snap_trs();
EXTERN	WORD	clamp_trs();
EXTERN	WORD	snap_xs();
EXTERN	WORD	clamp_xs();
EXTERN	WORD	snap_ys();
EXTERN	WORD	clamp_ys();
EXTERN	WORD	fit_htrees();
EXTERN	WORD	need_vtrees();
EXTERN	WORD	fit_vtrees();
EXTERN	VOID	do_trsinc();
EXTERN  VOID    wrte_inf();
EXTERN WORD rc_intersect(GRECT *p1, GRECT *p2);
EXTERN WORD wind_get(WORD w_handle, WORD w_field, WORD *pw1, WORD *pw2, WORD *pw3, WORD *pw4);
EXTERN  WORD mul_div(WORD a, WORD b, WORD c);
EXTERN WORD mul_div(WORD a, WORD b, WORD c);
EXTERN WORD      GET_STATE();
EXTERN WORD wind_update(WORD beg_update);
EXTERN  BOOLEAN	 paste_img;
EXTERN 	UWORD	mflags;
EXTERN	WORD	scale;
	VOID set_filemenu()

	{
        if ( rcs_state == NOFILE_STATE )
      		{
		enab_obj( ad_menu, NEWITEM);
		enab_obj( ad_menu, SVASITEM);
		enab_obj( ad_menu, RVRTITEM);
		}
	else if ( rcs_state == FILE_STATE)
		{
		enab_obj( ad_menu, SAVEITEM);
		enab_obj( ad_menu, RVRTITEM);
		}
	 }

	VOID do_redraw(w_handle, dirty, tree, area)
	WORD		w_handle;
	LONG		tree;
	GRECT		*dirty, *area;
	{
	GRECT		p, r;
	WORD		i;

	gsx_moff();

	wind_get(w_handle, WF_FIRSTXYWH, &p.g_x, &p.g_y, &p.g_w, &p.g_h);
	while ( p.g_w && p.g_h )
		{
		if ( rc_intersect(area, &p) )
	  	if ( rc_intersect(dirty, &p) )
			{
 			gsx_sclip(&p);	
			if (tree ==  ad_view)	/* Fast path for the view tree. */
				{   	 			
				objc_xywh(ad_view, ROOT, &r);
				rc_intersect(&p, &r);
				if (!rc_equal(&p, &r) || 
					rcs_state == MENU_STATE)
					gr_rect(BLUE, IP_4PATT, &p);
				}
	    		objc_draw(tree, ROOT, MAX_DEPTH, 
				p.g_x, p.g_y, p.g_w, p.g_h);
			if (tree == ad_view && tree_view() )
			for (i = rcs_nsel; i; )
				{
				objc_xywh(ad_view, rcs_sel[--i], &r);
				gsx_xbox(&r);
				if (rcs_state != ALRT_STATE)
					{
					obj_handle(ad_view, rcs_sel[i], &r);
					gr_rect(BLUE, IP_SOLID, &r);
					}
				}
			}
	  	wind_get(w_handle, WF_NEXTXYWH, 
			&p.g_x, &p.g_y, &p.g_w, &p.g_h);
		}
	gsx_mon();
	}

	VOID hndl_redraw(w_handle, c)
	WORD		w_handle;
	GRECT		*c;
	{
	if(iconedit_flag){
		do_redraw(w_handle, c, ad_itool, &itool);
		draw_clipb(TRUE, c);	/*draw the clipping*/
	}
	else{
		do_redraw(w_handle, c, ad_tools, &tools); 
	}
	do_redraw(w_handle, c, ad_pbx, &pbx);
	if(iconedit_flag) outl_img();	/*draw the outline of the icon/data*/
	do_redraw(w_handle, c, ad_view, &view);
	}

	VOID obj_nowdraw(tree, obj)
	LONG	tree;
	WORD	obj;
	{
	GRECT	p;

	objc_xywh(tree, obj, &p);
	hndl_redraw(rcs_view, &p);
	}

	VOID cut_part(sobj, dup)
	WORD	sobj, dup;
	{
	LONG	tree;

	if (file_view())
		cut_tree(trpan_f(sobj), dup);
else cut_obj(sobj, dup);
	tree = ad_tools;
	LLSET(OB_SPEC(CLIPBORD), image_addr(CLIPFULL));
	if( toolp) 
		obj_nowdraw(ad_tools, CLIPBORD);
else enab_obj( ad_menu, PASTITEM);
	set_filemenu(); 		
	rcs_edited = TRUE;
	}

	VOID paste_part(dobj, dx, dy, dup)
	WORD	dobj, dx, dy, dup;
	{
	if (!ad_clip)
		return;
	if (file_view())
		paste_tree(trpan_f(dobj), dx, dy, dup);
else paste_obj(dobj, dx, dy, dup);
	set_filemenu();		
	rcs_edited = TRUE;
	}

	VOID del_part(sobj)
	WORD	sobj;
	{
	if (file_view())
		del_tree(trpan_f(sobj));
else del_ob(sobj);
	set_filemenu();  	
	rcs_edited = TRUE;
	}

	VOID new_part(sobj, dobj, dx, dy)
	WORD	sobj, dobj, dx, dy;
	{
	if (file_view())
		new_tree(sobj, trpan_f(dobj), dx, dy);
else new_obj(ad_pbx, sobj, dobj, dx, dy); 
	rcs_edited = TRUE;
	set_filemenu();
	}

	VOID dup_part(sobj, dobj, dx, dy)
	WORD	sobj, dobj, dx, dy;
	{
	if (file_view())
		dup_tree(trpan_f(sobj), trpan_f(dobj), dx, dy);
else new_obj(ad_view, sobj, dobj, dx, dy);
	rcs_edited = TRUE;
	set_filemenu();
	}

	VOID mov_part(sobj, dobj, dx, dy)
	WORD	sobj, dobj, dx, dy;
	{
	if (file_view())
		mov_tree(trpan_f(sobj), trpan_f(dobj), dx, dy);
else mov_obj(sobj, dobj, dx, dy);
	rcs_edited = TRUE;
	set_filemenu();
	}

	VOID type_part(sobj)
	WORD	sobj;
	{
	if (file_view())
		type_tree(trpan_f(sobj));
else type_obj(sobj);
	}

	WORD hndl_mouse(mx, my)
	WORD	mx, my;
	{
	hot_off();
	rcs_hot = objc_find(ad_tools, ROOT, MAX_DEPTH, mx, my);
	if (rcs_hot == NIL)
		{
		wait_tools();
		return (FALSE);
		}
	wait_obj(ad_tools, rcs_hot);
	if (GET_FLAGS(ad_tools, rcs_hot) & SELECTABLE)
		invert_obj(ad_tools, rcs_hot, &tools);
	else
		rcs_hot = NIL;
	return (FALSE);
	}
 
	VOID drop_part(tree, sobj, d, mx, my, dup)
	LONG	tree;
	WORD	sobj, mx, my, dup;
	GRECT 	*d; 
	{
	WORD	dobj;
	LONG	dtree;

	if (!pane_find(&dtree, &dobj, d->g_x, d->g_y))
		return;
	if (tree == ad_tools)		
		{
		if (dtree == ad_view)
			paste_part(dobj, d->g_x, d->g_y, dup);
		return;
		}

	if (tree == ad_view)
		{
		if (dtree == ad_tools)
			{
			dobj = objc_find(ad_tools, ROOT, NIL, mx, my);
			if (dobj == CLIPBORD)
				cut_part(sobj, dup);
			else if (dobj == TRASHCAN)
				del_part(sobj);
			return;
			}
		if (dtree == ad_view)
			{
			if (dup)
				dup_part(sobj, dobj, d->g_x, d->g_y);
else mov_part(sobj, dobj, d->g_x, d->g_y);
			return;
			}
		return;
		}

	if (tree == ad_pbx)
		{
		if (dtree == ad_view)
			new_part(sobj, dobj, d->g_x, d->g_y);
		return;
		}
	}

	VOID drag_part(tree, obj, d, mx, my)
	LONG	tree;
	WORD	obj, *mx, *my;
	GRECT	*d;
	{

	hot_off();	 
	if (tree == ad_pbx)
		invert_obj(tree,obj,&pbx);
	else if (tree == ad_tools)
		invert_obj(tree,obj,&tools);
else invert_obj(tree, obj, &view);
	graf_mouse(HAND, 0x0L);
	objc_xywh(tree, obj, d);

	if (tree == ad_tools && obj == CLIPBORD && ad_clip 
		&& rcs_clipkind == OBJKIND)
		{
		d->g_w = GET_WIDTH(ad_clip, ROOT);
		d->g_h = GET_HEIGHT(ad_clip, ROOT);
		}
	hot_dragbox(d, &full, mx, my, tree == ad_view);
	graf_mouse(ARROW, 0x0L);
	if (tree == ad_pbx)
		invert_obj(tree, obj,&pbx);
	else if (tree == ad_tools)
		invert_obj(tree, obj, &tools);
else invert_obj(tree, obj, &view);
	wait_tools();
	}

	WORD handle_hit(tree, obj, x, y)
	LONG	tree;
	WORD	obj, x, y;
	{
	GRECT	d;

	if (tree_view() && rcs_state != ALRT_STATE)
		{
		obj_handle(tree, obj, &d);
		return inside(x, y, &d);
		}
	return (FALSE);
	}
 
	VOID hndl_down(mx, my, tree, obj, dup)
	WORD	mx, my, obj, dup;
	LONG	tree;
	{
	GRECT	d;

	if (tree == ad_view)
		{
		if (handle_hit(tree, obj, mx, my))
			{
			size_obj(tree, obj);
			set_filemenu();
			rcs_edited = TRUE;
			return;
			}
		if (rcs_state == MENU_STATE)	
		if (obj == THEBAR || GET_NEXT(tree, obj) == ROOT)
			return;		/* kludgy */
		if (obj == ROOT)
			return;		
		}

	if (tree == ad_tools)
	if (obj != CLIPBORD || ad_clip == 0)
		return;

	drag_part(tree, obj, &d, &mx, &my);
	drop_part(tree, obj, &d, mx, my, dup);
	return;
	}

	WORD hndl_dblklik(tree, obj)
	WORD	obj;
	LONG	tree;
	{
	if (tree != ad_view) 
		return(FALSE);
	if (tree_view())
		open_obj(obj);
	else if (obj != ROOT)
		open_tree(trpan_f(obj));
	if( rcs_edited )
		set_filemenu();
	return(FALSE);
	}

	VOID hndl_dsel()
	{
	if (!rcs_nsel)
		return;
	if (file_view())
		dselct_tree(ad_view, rcs_sel[0]);
else dslct_obj();
	}	     

	VOID hndl_sel(tree, obj, keystate)
	WORD	obj;
	LONG	tree;
	{
	WORD	shift, alt, aobj;

	if (tree != ad_view)
		{
		hndl_dsel();
		return;
		}
	if (file_view())
		{
		hndl_dsel();
		select_tree(tree, obj);
		return;
		}

	shift = keystate & (K_LSHIFT | K_RSHIFT); 
	alt = keystate & K_ALT;

	if (!shift || rcs_state == ALRT_STATE)	
		hndl_dsel();	/* Prevent delete hassles in alerts */

	if (alt && (aobj = GET_HEAD(tree, obj)) != NIL)
		for (; aobj != obj; aobj = GET_NEXT(tree, aobj))
			slct_obj(tree, aobj);
else slct_obj(tree, obj);
	}

	VOID hndl_del()
	{
	WORD	ii;
	for ( ii = rcs_nsel; ii ;del_part(rcs_sel[--ii]));
	}
	  
VOID hndl_cut()  
	{
	if ( rcs_nsel ) cut_part(rcs_sel[0], FALSE);
	}
				
	VOID hndl_copy()
	{
	if ( rcs_nsel ) 
		{
		cut_part(rcs_sel[0], TRUE);
		hndl_dsel();
		}
	}

	     
	VOID hndl_paste()
	{
	GRECT	d;
	WORD	mx, my, keystate, dup,button;

	if ( ad_clip )
		{
       	        graf_mkstate(&mx, &my, &button, &keystate);
       	 	dup = keystate & (K_LSHIFT | K_RSHIFT);
		drag_part(ad_tools, CLIPBORD, &d, &mx, &my);
		drop_part(ad_tools,CLIPBORD, &d, mx, my, dup);
		if ( !ad_clip ) disab_obj( ad_menu, PASTITEM);
		}
	}


				
	WORD hndl_sclck(mx, my, keystate, tree, obj, shifted)
	WORD 	mx, my, keystate;
	LONG	*tree;
	WORD	*obj, *shifted;
	{
	WORD	ctrled;

	*shifted = keystate & (K_LSHIFT | K_RSHIFT); 
	ctrled = keystate & K_CTRL; 

	if (!pane_find(tree, obj, mx, my))
		return (FALSE);
	if (*tree == ad_view)
		{
		if (*obj != ROOT && ctrled)
			*obj = get_parent(*tree, *obj);
		return (TRUE);
		}
	else if (*obj == ROOT)
		return (FALSE);
	else
		return (TRUE);
	}

	VOID send_redraw(w_handle, p)
	WORD	w_handle;
	GRECT	*p;
	{
	rcs_rmsg[0] = WM_REDRAW;
	rcs_rmsg[1] = gl_apid;
	rcs_rmsg[2] = 0;
	rcs_rmsg[3] = w_handle;
	rcs_rmsg[4] = p->g_x;
	rcs_rmsg[5] = p->g_y;
	rcs_rmsg[6] = p->g_w;
	rcs_rmsg[7] = p->g_h;
	appl_write(gl_apid, 16, ad_rmsg);
	} 

	VOID obj_redraw(tree, obj)
	LONG	tree;
	WORD	obj;
	{
	GRECT	p;

	objc_xywh(tree, obj, &p);
	rc_intersect(&view, &p);
	p.g_x -= 3; p.g_y -= 3; p.g_w += 6; p.g_h += 6;
	send_redraw(rcs_view, &p);
	}

	VOID hndl_pmenu(which, item)
	WORD	which, item;
	{
	WORD	i;
	LONG    tree;	 
		     
	tree = ad_view;
	for (i = 0; i < rcs_nsel; i++)
		{ 
		switch (which) {
			case HOTBGCOL:
				do_bgcol(tree, rcs_sel[i], item);
				break;
			case HOTPATRN:
				do_patrn(tree, rcs_sel[i], item);
				break;
			case HOTBDCOL:
				do_bdcol(tree, rcs_sel[i], item);
				break;
			case HOTTHICK:
				do_thick(tree, rcs_sel[i], item);
				break;
			case HOTFGCOL:
				do_fgcol(tree, rcs_sel[i], item);
				break;
			case HOTRULE:
				do_rule(tree, rcs_sel[i], item);
				break;
			case HOTPOSN:
				do_posn(tree, rcs_sel[i], item);
				break;
			case HOTSWTCH:
				do_swtch(tree, rcs_sel[i], item);
				break;
			default:
				return;
			}		     
		obj_redraw(ad_view, rcs_sel[i]);
		}

	if (which == HOTSWTCH)
		{
		if (item == HDDNPOP)
			dslct_obj();
		set_switch();
		}

	rcs_edited = TRUE;
	set_filemenu();
	}

	VOID hndl_tlbx(tree, obj, dup)
	LONG	tree;
	WORD	obj, dup;
	{
	WORD	pmenu, i;

	if ( !(GET_FLAGS(tree, obj) & SELECTABLE))
		return;
	if (!rcs_nsel)
		return;
	hot_off();
	if (obj == TRASHCAN )
		for (i = rcs_nsel; i; del_part(rcs_sel[--i]));
	else if (obj == CLIPBORD )
		cut_part(rcs_sel[0], dup);
	else 
		if ( (pmenu = hndl_pop(obj, FALSE)) != NIL)
			hndl_pmenu(obj, pmenu); 
	wait_tools();
	}

	WORD hndl_button(mx, my, button, keystate, clicks)
	WORD	mx, my, button, keystate, clicks;
	{
	WORD	obj_sel, dup_sel;
	LONG	tree_sel;

	graf_mkstate(&mx, &my, &button, &keystate);
	if ( !hndl_sclck( mx, my, keystate, &tree_sel, &obj_sel, &dup_sel) )
		{
		hndl_dsel();
		return(FALSE);
		}
	if (tree_sel == ad_tools && clicks == 1 && !(button & 0x1))
		{
		hndl_tlbx(tree_sel, obj_sel, dup_sel);
		return (FALSE);
		}

	if ( clicks == 1 )
		{
		if ( button & 0x0001 )
			hndl_down( mx, my, tree_sel, obj_sel, dup_sel );
else hndl_sel(tree_sel, obj_sel, keystate);
		}
else hndl_dblklik(tree_sel, obj_sel );
	return (FALSE);
	}

	VOID hndl_open()
	{
	if (tree_view())
	    	open_obj(rcs_sel[0]);
    	else if (!rcs_nsel)	      
		{
			if(open_rsc())
				rcs_edited = FALSE;
		}
else open_tree(trpan_f(rcs_sel[0]));
	}

	VOID hndl_clos()
	{
	if (tree_view())
	 	{
		clos_tree();
		}
	else if (rcs_state == FILE_STATE)
       		if(clos_rsc())	 
			rcs_edited = FALSE;
	}
	
	
	WORD hndl_filemenu(item)
	WORD	item;
	{
	WORD	done;

	done = FALSE;

	switch (item) {
		case NEWITEM:		  
			if ( iconedit_flag)
				{
				if (new_img() )
					icn_edited = FALSE;
				}
			else 				if(new_rsc())
					rcs_edited = FALSE;
			break;
		case OPENITEM:	 
			if ( iconedit_flag)
				{
	 			if(icnopen())
					icn_edited = FALSE;	    
				}
else hndl_open();
			break; 
		case MERGITEM:
			merge_rsc();
			rcs_edited = TRUE;
			break;
		case CLOSITEM:	 
			if (iconedit_flag)
				done = TRUE;
else hndl_clos();
			break;
		case SAVEITEM:
			if (iconedit_flag)		
				{
				if(save_icn())
					icn_edited = FALSE;	       
				}
			else		 
				{
				save_rsc();
				rcs_edited = FALSE;
				}
			break;
		case SVASITEM:	 
			if( iconedit_flag)
				{
				if(svas_icn())
					icn_edited = FALSE;
				}
			else 				if(svas_rsc())
					rcs_edited = FALSE;
			break;
		case RVRTITEM:
			if (iconedit_flag)	       
				{
				if(rvrt_icn())
					icn_edited = FALSE;
				}
			else 				if(rvrt_rsc())
					rcs_edited = FALSE;
			break;
		case QUITITEM:				      
			done =	quit_rsc();
 		}
	if( rcs_edited )
		set_filemenu();
	if(iconedit_flag)
		set_icnmenus();
	return (done);													
	}



	WORD hndl_editmenu(item)
	WORD	item;
	{
	WORD	done, obmsb, new_h, new_w;
	LONG	tree;

	done = FALSE;

	switch (item) {
		case DELITEM:
			if(iconedit_flag) del_img(TRUE, 3);
			else hndl_del();
			break;
		case CUTITEM:
			if(iconedit_flag) cut_img();
			else hndl_cut();
			break; 
		case COPYITEM:
			if(iconedit_flag) copy_img(TRUE);
			else hndl_copy();
			break;
		case PASTITEM:
			if(iconedit_flag){
			    paste_img = TRUE;
			    rubrec_off();
			    mflags |= MU_M2;
			}
			else hndl_paste();
			break;
		case INVITEM:
			graf_mouse(M_OFF,0x0L);
			invrt_img();	       
			graf_mouse(M_ON,0x0L);
			break;
		case SOLIDIMG:		      
			graf_mouse(M_OFF,0x0L);
			solid_img();       
			fb_redraw();
			graf_mouse(M_ON,0x0L);
			break;		 
/*		case CLEARITM:
			clear_img();
			break;			
		case DATTOMSK:
			if (gl_isicon)	      
				{
				graf_mouse(M_OFF,0x0L);
				data_to_mask(TRUE);    
				graf_mouse(M_ON,0x0L);
				}
			break;
		case MSKTODAT:
			if (gl_isicon)
				{
				graf_mouse(M_OFF,0x0L);
				data_to_mask(FALSE);   
				graf_mouse(M_ON,0x0L);
				}
			break;
*/		case SIZEITEM:			
			icon_size(&new_w, &new_h);
			if( new_w != gl_wimage || new_h != gl_himage)
			{
			     tree = save_tree;			
			     obmsb = 1;	  
			     if (gl_isicon)
				 LWSET(OB_TYPE(save_obj),(obmsb<< 8)| G_ICON);
else LWSET(OB_TYPE(save_obj),(obmsb<<8)|G_IMAGE);
			     if (gl_isicon)
				{
				LWSET(IB_WICON(gl_icnspec), new_w);
				LWSET(IB_HICON(gl_icnspec), new_h);
				}
			     else
				{     
				LWSET(BI_WB(gl_icnspec), new_w >> 3);
				LWSET(BI_HL(gl_icnspec), new_h);
				}
			     icn_init(TRUE);
			     icn_edited = FALSE;
			}
			else if(scale == CLEARSZ) /*same size, but clear it*/
				clear_img();
			break;
		default:
			break;
		      }
     	return( done );
	}

	
	WORD hndl_omenu(item)
	WORD	item;
	{
	WORD	done;

	done = FALSE;

	switch (item) {
		case RNAMITEM:
			name_tree(trpan_f(rcs_sel[0]));
			break;
		case TYPEITEM:
			type_part(rcs_sel[0]);
			break;
		case INFITEM:
			info_dial(rcs_nsel? rcs_sel[0]: NIL);
			break;
		case LOADITEM:
			load_part(rcs_sel[0]);
			break;
		case ICNEDITM:
			icon_edit();
			break;
	  	default:
			break;
		}
       if(rcs_edited )
	    set_filemenu();
	return (done);
	} 

	WORD hndl_hmenu(item)
	WORD	item;
	{
	WORD	done;

	done = FALSE;

	switch (item) {
		case SRTITEM:
			sort_part(rcs_sel[0]);
			break;
		case UNHDITEM:
			unhid_part(rcs_sel[0]);
			break;
		case FLTITEM:
			flatten_part(rcs_sel[0]);
			break;
		default:
			break;
		}
	rcs_edited = TRUE;
	return (done);
	} 

	VOID hndl_hidsho(flag, item, hide, show)
	WORD	*flag, item, hide, show;
	{
	WORD	str;



	*flag = !(*flag);
	str = (*flag)? hide: show;
	menu_text(ad_menu, item, string_addr(str));
	ini_panes();
	if (file_view())
		redo_trees();
	else
		{
		dslct_obj();
		view_objs();
		}
	if ( !toolp && ad_clip)
		enab_obj( ad_menu, PASTITEM);
	send_redraw(rcs_view, &full);
	} 

	WORD hndl_globmenu(item)
	WORD	item;
	{
	WORD	done;

	done = FALSE;

	switch (item) {
		case OUTPITEM:
			outp_dial();
			break;
		case SAFEITEM:
			safe_dial();
			break;
		case SVOSITEM:
			wrte_inf();
			break;
		case PARTITEM:
		        if( iconedit_flag)
			     save_fat(FALSE);
			hndl_hidsho(&partp, PARTITEM, HIDEPART, SHOWPART);
			break;
		case TOOLITEM:		
			if (iconedit_flag)
			     save_fat(FALSE);	
			hndl_hidsho(&toolp, TOOLITEM, HIDETOOL, SHOWTOOL);
	 		break;
		default:
			break;
		}
	return (done);
	} 

	WORD hndl_menu(title, item)
	WORD 	title, item;
	{
	WORD	done;

	done = FALSE;
	paste_img = FALSE;
	switch (title) {
		case DESKMENU:
			if (item == ABOUITEM)
				about_dial();
			break;
		case FILEMENU:
			done = hndl_filemenu(item);
			break;
		case GLOBMENU:
			done = hndl_globmenu(item);
			break;		
		case EDITMENU:
			done = hndl_editmenu(item);
			break;
		case OPTNMENU:
			done = hndl_omenu(item);
			break;
		case ARRMENU:
			done = hndl_hmenu(item);
			break;
		}
	menu_tnormal(ad_menu,title,TRUE);
	if ( iconedit_flag)
		set_icnmenus();

	return (done);
	}

	VOID hndl_arrowed(w_handle, kind)
	WORD	w_handle, kind;
	{
	if (file_view())
		{
		switch (kind) {
			case WA_LFPAGE:
			case WA_RTPAGE:
			case WA_LFLINE:
			case WA_RTLINE:
				return;
			case WA_UPPAGE:
				do_trsinc(-fit_vtrees() * fit_htrees());
				break;
			case WA_DNPAGE:
				do_trsinc(fit_vtrees() * fit_htrees());
				break;
			case WA_UPLINE:
				do_trsinc(-fit_htrees());
				break;
			case WA_DNLINE:
				do_trsinc(fit_htrees());
				break;
			}
		redo_trees();
		}
	else
		{
		switch (kind) {
			case WA_LFPAGE:
				do_hsinc(-view.g_w);
				break;
			case WA_RTPAGE:
				do_hsinc(view.g_w);
				break;
			case WA_LFLINE:
				do_hsinc(-2 * gl_wchar);
			 	break;
			case WA_RTLINE:
				do_hsinc(2 * gl_wchar);
				break;
			case WA_UPPAGE:
				do_vsinc(-view.g_h);
				break;
			case WA_DNPAGE:
				do_vsinc(view.g_h);
				break;
			case WA_UPLINE:
				do_vsinc(-gl_hchar);
				break;
			case WA_DNLINE:
				do_vsinc(gl_hchar);
				break;
			}
		dslct_obj();
		view_objs();
		}
	}

	VOID hndl_hslid(w_handle, h_set)
	WORD	w_handle, h_set;
	{
	WORD	x;

	if (file_view())
		return;
	x = max(0, GET_WIDTH(ad_view, ROOT) - view.g_w); 
	x = (WORD) umul_div(x, h_set, 1000);
	rcs_xpan = clamp_xs(ad_view, snap_xs(x));
	dslct_obj();
	view_objs();
	}

	VOID hndl_vslid(w_handle, v_set)
	WORD	w_handle, v_set;
	{
	WORD	y;

	if (file_view())
		{
		y = max(0, fit_htrees() * (need_vtrees() - fit_vtrees()));
		y = mul_div(y, v_set, 1000);
		rcs_trpan = clamp_trs(snap_trs(y));
		redo_trees();
		}
	else
		{
		y = max(0, GET_HEIGHT(ad_view, ROOT) - view.g_h); 
		y = (WORD) umul_div(y, v_set, 1000);
		rcs_ypan = clamp_ys(ad_view, snap_ys(y));
		dslct_obj();
		view_objs();
		}
	} 		

	WORD hndl_msg()
	{
	WORD 	done;

	done = FALSE;

	switch ( rcs_rmsg[0] )	{
		case MN_SELECTED:
			done = hndl_menu(rcs_rmsg[3], rcs_rmsg[4]);
			break;
		case WM_REDRAW:
			hndl_redraw(rcs_rmsg[3], (GRECT *) &rcs_rmsg[4]); 
			break;
		case WM_CLOSED:
			hndl_clos();
			if( rcs_edited )
				set_filemenu();
     			break;
		case WM_TOPPED:
			wind_set(rcs_rmsg[3], WF_TOP, 0, 0, 0, 0);
			break;
		case WM_ARROWED:
			hndl_arrowed(rcs_rmsg[3], rcs_rmsg[4]);
			break;
		case WM_HSLID:
			hndl_hslid(rcs_rmsg[3], rcs_rmsg[4]);
			break;
		case WM_VSLID:
			hndl_vslid(rcs_rmsg[3], rcs_rmsg[4]);
			break;
		default:
			break;
		}
	rcs_rmsg[0] = 0;
	return(done);
	} 


BOOLEAN hndl_keybd(WORD key)
{
WORD		title, item, done ;

    done = FALSE ;
    title = FILEMENU ;
    switch ( key )
	{
	case ALT_O :title = GLOBMENU ; 
		    item = OUTPITEM ;
		    break ;
	case ALT_S :title = GLOBMENU ; 
		    item = SAFEITEM ;
		    break ;
	case ALT_R :title = GLOBMENU;
		    item = SVOSITEM;
		    break;	
	case ALT_P :title = GLOBMENU ; 
		    item = PARTITEM ;
		    break ;
	case ALT_H :title = GLOBMENU;
		    item = TOOLITEM ;
		    break ;
	case ALT_I :title = OPTNMENU;
		    item = INFITEM ;
		    break ;
	case ALT_N :title = OPTNMENU;
		    item = RNAMITEM ;
		    break ;    
	case ALT_T :title = OPTNMENU;
		    item = TYPEITEM;
		    break;
	case ALT_L :title = OPTNMENU;
		    item = LOADITEM;
		    break;
	case ALT_E :title = OPTNMENU;
		    item = ICNEDITM;
		    break;
	case ALT_F :title = ARRMENU;
		    item = SRTITEM;
		    break;
	case ALT_U :title = ARRMENU;
		    item = UNHDITEM;
		    break;
	case CTL_W :item = NEWITEM;
		    break;
	case CTL_O :item = OPENITEM;
		    break;
	case CTL_N :item = MERGITEM;
		    break;
	case CTL_C :item = CLOSITEM;
		    break;
	case CTL_V :item = SAVEITEM;
		    break;
	case CTL_M :item = SVASITEM;
		    break;
	case CTL_A :item = RVRTITEM;
		    break;
	case ALT_C :title = EDITMENU;
		    item = CUTITEM;
		    break;
	case ALT_Y :title = EDITMENU;
		    item = COPYITEM;
		    break;
	case ALT_A :title = EDITMENU;
		    item = PASTITEM;
		    break;
	case ALT_D :title = EDITMENU;
		    item = DELITEM;
		    break;
	case CTL_Q :item = QUITITEM;
		    break;	
	default :   return( done ) ;
	} /* switch */
    if ( ! ( DISABLED & GET_STATE( ad_menu, item ) ) ) 
	done = hndl_menu( title, item ) ;
    return( done ) ;
} /* hndl_keybd */

	VOID rcs_main()
	{
	WORD flags, done; 
	WORD ev_which, rets[6];
	/* initialize for event loop */
	flags = MU_BUTTON | MU_MESAG | MU_M1 | MU_KEYBD;  
	ad_rmsg = ADDR(rcs_rmsg);
	done = FALSE;
	do	
	{
	   ev_which = evnt_multi(flags, 0x02, 0xff, 0x01,
	   	      wait_io, wait.g_x, wait.g_y, wait.g_w, wait.g_h,
		      0, 0, 0, 0, 0,
		      ad_rmsg, 0, 0,
		      &rets[0], &rets[1], &rets[2],
		      &rets[3], &rets[4], &rets[5]);

	   wind_update(TRUE);
	   if (ev_which & MU_M1)
		done = (hndl_mouse(rets[0], rets[1]));
           if (ev_which & MU_BUTTON)
		done = (hndl_button(rets[0], rets[1], rets[2], rets[3], rets[5]));
	   if (ev_which & MU_MESAG)
		done =  (hndl_msg());		       
           if ( ev_which & MU_KEYBD )
	        done = hndl_keybd( rets[ 4 ] ) ;
           wind_update(FALSE);
	}
	while (!done) ;
	}

	VOID GEMAIN()
	{
	WORD	rcs_term;

	if ( !(rcs_term = rcs_init()) )
		rcs_main();
	rcs_exit(rcs_term);
	}
