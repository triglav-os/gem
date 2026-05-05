/*
 * Implements the editor-side interface glue that connects resource
 * construction set logic to GEM services. It helps keep the imported code
 * organized around a service boundary.
 *
 * MIT License (see: LICENSE)
 * Copyright (C) 2026 tomaz stih
 */

#include "portab.h"
#include "machine.h"
#include "gsxdefs.h"
#include "obdefs.h"
#include "gembind.h"
#include "rcsdefs.h"
#include "rcs.h"
#include "rcsext.h"
extern WORD dos_gdrv(VOID);
#define	BEG_MCTRL	3
#define	END_MCTRL	2

	WORD	r_infhndl;

MLOCAL  LONG	ad_prog;
MLOCAL	GRECT	prog;

EXTERN	FDB	gl_tmp;		/* references to rcsvdi.c */

	WORD hndl_dial(tree, def, p)
	LONG	tree;
	WORD	def;
	GRECT	*p;
	{
	WORD	xdial, ydial, wdial, hdial, exitobj;

	form_center(tree, &xdial, &ydial, &wdial, &hdial);
	form_dial(0, p->g_x, p->g_y, p->g_w, p->g_h, 
		xdial, ydial, wdial, hdial);
	if (!rcs_xpert)  
		form_dial(1, p->g_x, p->g_y, p->g_w, p->g_h, 
			xdial, ydial, wdial, hdial);
	objc_draw(tree, ROOT, MAX_DEPTH, xdial, ydial, wdial, hdial);

	exitobj = form_do(tree, def) & 0x7FFF;

	if (!rcs_xpert)	 
		form_dial(2, p->g_x, p->g_y, p->g_w, p->g_h, 
			xdial, ydial, wdial, hdial);
	form_dial(3, p->g_x, p->g_y, p->g_w, p->g_h, 
		xdial, ydial, wdial, hdial);
	return (exitobj);
	}

	WORD hndl_alert(b_num, addr)
	WORD	b_num;
	LONG	addr;
	{
	WORD	ret;

	graf_mouse(ARROW, 0x0L);
	ret = form_alert(b_num, addr);
	graf_mouse(rcs_mform, 0x0L);
	return (ret);
	}

	VOID hndl_locked()
	{
	hndl_alert(1, string_addr(STLOCKED));
	}

	VOID mouse_form(f_num)
	WORD	f_num;
	{
	rcs_mform = f_num;
	graf_mouse(f_num, 0x0L);
	}

	VOID ini_panes()
	{
	wind_get(rcs_view, WF_WXYWH, &view.g_x, &view.g_y,
		&view.g_w, &view.g_h);

	if(iconedit_flag){
		rc_copy(&view, &itool);
		itool.g_w = idisp.g_w = 2 * ICON_W;
		view.g_x += itool.g_w;
		view.g_w -= itool.g_w;
		ob_setxywh(ad_itool, ROOT, &itool);  /*view_itool*/
		wait_io = 0x0;
		rc_copy(&itool, &wait);
	}
	else{
		rc_copy(&view, &tools);
		if (toolp)
			{
			tools.g_h = GET_HEIGHT(ad_tools, ROOT);
			tools.g_w = 2 * ICON_W;
			view.g_x += tools.g_w;
			view.g_w -= tools.g_w;
			}
		else
			tools.g_w = tools.g_h = 0;
  		ob_setxywh(ad_tools, ROOT, &tools);
		wait_tools();		/* Reset rectangle wait */ 
	}
		rc_copy(&view, &pbx);  /*this should go into the else above*/
		if (iconedit_flag || partp)
			{
			pbx.g_h = umul_div(view.g_h, 6, 23);
			if(iconedit_flag &&
				((gl_hchar + MAX_ICON_H + 2) > pbx.g_h))
				pbx.g_h = gl_hchar + MAX_ICON_H + 2;
			view.g_h -= pbx.g_h;
			pbx.g_y += view.g_h;
		}
		else
			pbx.g_w = pbx.g_h = 0;
		view_parts();
			/*up to this point should be included in the else */
	if ( iconedit_flag )
		init_img();
	}		 

	VOID arrge_icntree(tree, wint, hint)
	LONG	tree;
	WORD	wint, hint;
	{
	WORD	obj, x, y, rowh, wroot, offx, i;

	if ( !(wroot = GET_WIDTH(tree, ROOT)) )
		return;
	x = wint;
	if( (gl_himage + gl_hchar + hint) >= pbx.g_h )
		hint = 0;
	y = hint;
	rowh = 0;
	
	for (obj = ICONSTRG; obj <= MASKSTRG;
		obj++)
		{
		if (rowh && (x + GET_WIDTH(tree, obj)) > wroot)
			{
			x = wint;
			y += (rowh + hint);
			rowh = 0;
			}
		SET_X(tree, obj, x);
		SET_Y(tree, obj, y);
		if ( !(GET_FLAGS(tree, obj) & HIDETREE) )
			{
			x += (GET_WIDTH(tree, obj) + wint); 
			rowh = max(rowh, GET_HEIGHT(tree, obj));
			}
		}
	offx = (GET_WIDTH(tree,ICONSTRG) - gl_wimage)/2;
	i = 0;
	x = wint;
	y += (rowh + hint);
	rowh = 0;

	for (obj = ICONWIND; obj <= MASKWIND; obj++)
		{			    
    		SET_X(tree, obj, GET_X(tree,ICONSTRG + i++) + offx);
		SET_Y(tree, obj, y);
		if ( !(GET_FLAGS(tree, obj) & HIDETREE) )
			{
			x += (GET_WIDTH(tree, obj) + wint); 
			rowh = max(rowh, GET_HEIGHT(tree, obj));
			}
		}
	}
      
	VOID view_parts()
	{
	WORD  foo;
	
	ob_setxywh(ad_pbx, ROOT, &pbx);	
	if (iconedit_flag)
		arrge_icntree(ad_pbx,PBX_WINT, PBX_HINT);
else arrange_tree(ad_pbx, PBX_WINT, PBX_HINT, &foo, &foo);
	}


	VOID lock_menu()
	{
	WORD	*i;

	for (i = ILL_LOCK; *i; i++)
		disab_obj(ad_menu, *i);
	unsble_obj(ad_tools, TRASHCAN);
	unsble_obj(ad_tools, CLIPBORD);
	unsble_obj(ad_tools, HOTSWTCH);
	}

	VOID set_menus()
	{
	WORD	*ok_list, obj;

	obj = GET_NEXT(ad_menu, GET_HEAD(ad_menu, SCREEN));
	map_tree(ad_menu, obj, SCREEN, disab_obj);

	switch (rcs_state) {
		case NOFILE_STATE:
			ok_list = OK_NOFILE;
			break;
		case FILE_STATE:
			ok_list = OK_FILE;
			break;
		default:
			ok_list = OK_TREES;
			break;
		}
	for (; *ok_list; ok_list++)
		enab_obj(ad_menu, *ok_list);

	if ( ad_clip )
		enab_obj(ad_menu, PASTITEM);

	if (rcs_nsel > 1)
		{
		disab_obj(ad_menu, INFITEM);
	 	if (rcs_sel[0] != ROOT)	enab_obj(ad_menu, DELITEM);
		}
     	else if (rcs_nsel == 1)
     		menu_opts(ad_view, rcs_sel[0]);

     	if (rcs_lock)
		lock_menus();
	}

	VOID menu_opts(tree, obj)
	LONG	tree;
	WORD	obj;
	{
	WORD	type;

	type = LLOBT(GET_TYPE(tree, obj));
	switch (rcs_state) {
		case PANL_STATE:
		case DIAL_STATE:
			if (GET_HEAD(tree, obj) != NIL)
			if (obj != ROOT)
				enab_obj(ad_menu, FLTITEM);
			if (type != G_ICON && type != G_IMAGE)
	 			enab_obj(ad_menu, TYPEITEM);
	 	case MENU_STATE:
		case FREE_STATE:
			if (type == G_IMAGE || type == G_ICON)
				{
				enab_obj(ad_menu, LOADITEM);
				enab_obj(ad_menu, ICNEDITM);
				}
			if (GET_HEAD(tree, obj) != NIL)
				{
				enab_obj(ad_menu, SRTITEM);
				enab_obj(ad_menu, UNHDITEM);
				enab_obj(ad_menu, FLTITEM);
				}
		case ALRT_STATE:
			enab_obj(ad_menu, OPENITEM);
			if ( obj != ROOT )
				{
				enab_obj(ad_menu, DELITEM);
				enab_obj(ad_menu, CUTITEM);
				enab_obj(ad_menu, COPYITEM);
				}
		default:
			return;
		}
	}


	VOID clr_hot()
	{
	if(iconedit_flag){
		map_tree(ad_itool, ROOT, NIL, unsble_obj);
		map_tree(ad_itool, ROOT, NIL, desel_obj);
	}
else map_tree(ad_tools, ROOT, NIL, unsble_obj);
	}

	VOID set_hot()
	{
       	WORD	j, obj, *i, type, *null = 0;

       	if (!rcs_nsel)
		return;
       	clr_hot();

       
	for (j = rcs_nsel; j; )
		{
		obj = rcs_sel[--j];
		type = LLOBT(GET_TYPE(ad_view, obj));
	
		switch (type) {
			case G_IBOX:
				i = HOT_IBOX;
				break;
			case G_BOX:
				i = HOT_BOX;
				break;
			case G_TEXT:
			case G_FTEXT:
				i = HOT_TEXT;
				break;
			case G_BOXCHAR:
			case G_BOXTEXT:
			case G_FBOXTEXT:
				i = HOT_BTEXT;
				break;
			case G_IMAGE:
				i = HOT_IMAGE;
				break;
			case G_ICON:	
				i = HOT_ICON;
				break;
			default:
				i = null;
				break;
			}

		if (i != null)
		for (; *i; i++)
			sble_obj(ad_tools, *i);
		}

	if (rcs_nsel > 1 || rcs_sel[0] != ROOT)
		{
		if (rcs_state != ALRT_STATE)
			sble_obj(ad_tools, HOTPOSN);
		sble_obj(ad_tools, TRASHCAN);
		if (rcs_nsel == 1)
			sble_obj(ad_tools, CLIPBORD);
		}
	if (rcs_state != ALRT_STATE)
		sble_obj(ad_tools, HOTSWTCH);
	}

	VOID set_switch()
	{
	LONG	tree;
	WORD	obj, j, flags, state;

	obj = rcs_sel[0];
	flags = GET_FLAGS(ad_view, obj);
	state = GET_STATE(ad_view, obj);

	ini_tree(&tree, POPSWTCH);
	map_tree(tree, ROOT, NIL, unchek_obj);
	if (rcs_nsel != 1)
		return;
	for (j = 0; rcs_bit2obj[j].objno; j++)
		if (flags & rcs_bit2obj[j].flagmask ||
			state & rcs_bit2obj[j].statemask)
			chek_obj(tree, rcs_bit2obj[j].objno);
	}

	VOID new_state(new)
	WORD	new;
	{
	WORD	pbxno;

	rcs_state = new;
	map_tree(ad_menu, 0, -1, enab_obj);
	switch (new) {
		case NOFILE_STATE:
			pbxno = FILEPBX;
			break;
		case FILE_STATE:
			pbxno = FILEPBX;
			break;
		case PANL_STATE:
			pbxno = DIALPBX;
			break;
		case DIAL_STATE:
			pbxno = DIALPBX;
			break;
		case ALRT_STATE:
			pbxno = ALRTPBX;
			break;
		case MENU_STATE:
			pbxno = MENUPBX;
			break;
		case FREE_STATE:
			pbxno = FREEPBX;
			break;
		}
      
	ini_tree(&ad_pbx, pbxno);
	set_menus();
	clr_hot();
	view_parts();
	send_redraw(rcs_view, &pbx);
	} 

	WORD
file_view()		/* Are we showing tree icons */
	{
	return (rcs_state == FILE_STATE || rcs_state == NOFILE_STATE);
	}

	WORD
tree_view()		/* Or the inside of a tree ? */
	{
	return !file_view();
	}

	VOID clr_clip()
	{
	ad_clip = 0;
	SET_SPEC(ad_tools, CLIPBORD, image_addr(CLIPMPTY));
	obj_nowdraw(ad_tools, CLIPBORD);
	}

	VOID set_title(new)
	BYTE	*new;
	{
	rcs_title[0] = ' ';
	strcpy(new, &rcs_title[1]);
	strcat(" ", &rcs_title[0]);
	wind_set(rcs_view, WF_NAME, ADDR(&rcs_title[0]), 0x0, 0x0);
	}
	
	VOID get_defpath(path, spec)
	BYTE * path, *spec;
        {
	if(rsc_path[0])
		strcpy(&rsc_path[1], path);
	else{
		path[0] = dos_gdrv() + 'A';
		path[1] = ':';
		path[2] = '\\';
		path[3] = '\0';
	     }
	strcat(spec, path);
	return;
        }

	VOID clr_title()
	{
	BYTE	temp[20];

	rcs_title[0] = ' ';
	LLSTRCPY(string_addr(STTITLE), ADDR(temp));
	get_defpath(&rcs_title[1], temp);
	wind_set(rcs_view, WF_NAME, ADDR(&rcs_title[0]), 0x0, 0x0);
	}

	VOID hot_off()
	{
	if (rcs_hot == NIL)
		return;
	if(iconedit_flag)
		invert_obj(ad_itool, rcs_hot, &itool);
	else{
		invert_obj(ad_tools, rcs_hot, &tools);
		rcs_hot = NIL;
	}
	}

	VOID wait_tools()
	{
	wait_io = 0x0;		/* wait to enter */
	rc_copy(&tools, &wait);	/* the toolbox	*/
	}

	VOID wait_obj(tree, obj)
	LONG	tree;
	WORD	obj;
	{	
	wait_io = 0x1;			/* wait to exit */
	objc_xywh(tree, obj, &wait);	/* an object's rectangle */
	}

	WORD pane_find(tree, obj, mx, my)
	LONG	*tree;
	WORD	*obj, mx, my;
	{
	GRECT	dt;
	WORD	desktitle, the_active;
	*obj = objc_find(*tree = ad_pbx, 0, MAX_DEPTH, mx, my);
	if (*obj != NIL)
		return (TRUE);

	*obj = objc_find(*tree = ad_tools, 0, MAX_DEPTH, mx, my);
	if (*obj != NIL)
		return (TRUE);

	*obj = objc_find(*tree = ad_view, 0, MAX_DEPTH, mx, my);
	if ( rcs_state == MENU_STATE && *obj == THEBAR)
		/* special case desk menu */
		{		     
		the_active = get_active(*tree);
		desktitle = GET_HEAD(*tree, the_active);
		ob_relxywh(*tree, desktitle, &dt);
		if( mx >= dt.g_x)
			*obj = desktitle;
		}
	if (*obj != NIL)
		return (TRUE);

	return(FALSE);
	}


	WORD hndl_pop(obj, itool)
	WORD	obj;
	BOOLEAN itool;
	{
	WORD	i, event, mx, my, sel_obj, disab, outline, foo;
	UWORD	buf_seg, buf_off;
	LONG	tree;
	GRECT	poparea, bltarea;

	if(itool){
	for (i = 0; icn_ht2pop[i].hotobj && icn_ht2pop[i].hotobj != obj; i++);
	if (!icn_ht2pop[i].hotobj || !icn_ht2pop[i].popmenu)
		return (NIL);
	}
	else{
	for (i = 0; rcs_ht2pop[i].hotobj && rcs_ht2pop[i].hotobj != obj; i++);
	if (!rcs_ht2pop[i].hotobj || !rcs_ht2pop[i].popmenu)
		return (NIL);
	}
	ini_tree(&tree, itool ? icn_ht2pop[i].popmenu: rcs_ht2pop[i].popmenu);
	objc_xywh(tree, ROOT, &poparea);
	objc_offset(itool ? ad_itool:ad_tools, obj, &poparea.g_x, &poparea.g_y);
	ob_setxywh(tree, ROOT, &poparea);

	rc_copy(&poparea, &bltarea);
	bltarea.g_x--;
	bltarea.g_y--;
	bltarea.g_w += 2;
	bltarea.g_h += 2;
	gl_tmp.fd_nplanes = gl_nplanes;
	wind_get(0, 17, &buf_off, &buf_seg, &foo, &foo);
	if(!buf_off && !buf_seg) gl_tmp.fd_addr = 0xffffffff;
	else{
#if  MC68K
		gl_tmp.fd_addr = HW(buf_off)| LW(buf_seg);
#else   /* swap hi lo word order for intel environment */
		gl_tmp.fd_addr = HW(buf_seg) | LW(buf_off);
#endif
	}
	wind_update(BEG_MCTRL);		/* protection vs. buffer collisions! */

	bb_save(&bltarea);
	objc_draw(tree, ROOT, MAX_DEPTH, 
		bltarea.g_x, bltarea.g_y, bltarea.g_w, bltarea.g_h);
	outline = itool ? icn_ht2pop[i].outline : rcs_ht2pop[i].outline; 

	FOREVER {
		sel_obj = NIL;
		objc_xywh(tree, ROOT, &poparea);

		event = evnt_multi(MU_BUTTON | MU_M1, 0x01, 0xff, 0x1,
			FALSE, poparea.g_x, poparea.g_y, 
				poparea.g_w, poparea.g_h,
			0, 0, 0, 0, 0, 0x0L, 0, 0,
			&mx, &my, &foo, &foo, &foo, &foo, &foo);

		if ( !(event & MU_M1))
			break;
		sel_obj = objc_find(tree, ROOT, MAX_DEPTH, mx, my);
		if (event & MU_BUTTON)
			break;

		disab = GET_STATE(tree, sel_obj) & DISABLED;
		if (!disab)
		if (outline)
			outl_obj(tree, sel_obj, &poparea);
else invert_obj(tree, sel_obj, &poparea);

		objc_xywh(tree, sel_obj, &poparea);
		event = evnt_multi(MU_BUTTON | MU_M1, 0x01, 0xff, 0x1,
			TRUE, poparea.g_x, poparea.g_y, 
				poparea.g_w, poparea.g_h,
			0, 0, 0, 0, 0, 0x0L, 0, 0,
			&mx, &my, &foo, &foo, &foo, &foo);

		if (disab)
			sel_obj = NIL;
		else if (outline)
			outl_obj(tree, sel_obj, &poparea);
else invert_obj(tree, sel_obj, &poparea);

		if (event & MU_BUTTON)
			break;
		}

	bb_restore(&bltarea);
	wind_update(END_MCTRL);
	return (sel_obj);
	} 

	WORD rub_wait(po, mx, my)
	GRECT 	*po;
	WORD	*mx, *my;
	{
	WORD	down, rets[6];

	gsx_moff();
	gsx_xbox(po);
	gsx_mon();

	down = !(MU_BUTTON & evnt_multi(MU_BUTTON | MU_M1, 
		0x01, 0xff, 0x00, TRUE, *mx, *my, 1, 1,
		0, 0, 0, 0, 0, 0x0L, 0, 0,
		&rets[0], &rets[1], &rets[2],
		&rets[3], &rets[4], &rets[5]));

	*mx = rets[0];
	*my = rets[1];

	gsx_moff();
	gsx_xbox(po);
	gsx_mon();

	return (down);
	}

	VOID hot_dragbox(p, pc, mx, my, hot)
	WORD	*mx, *my, hot;
	GRECT	*p, *pc; 
	{
	WORD	offx, offy, down, over_obj;
	GRECT	o;

	gsx_attr(FALSE, MD_XOR, BLUE);
	o.g_w = p->g_w;
	o.g_h = p->g_h;
	graf_mkstate(mx, my, &down, &down);
	offx = min(p->g_w, max(0, *mx - p->g_x));
	offy = min(p->g_h, max(0, *my - p->g_y));

	do
		{
		o.g_x = *mx - offx;
		o.g_y = *my - offy;
/*		rc_constrain(pc, &o);  */
		gsx_sclip(pc);
		down = rub_wait(&o, mx, my);
		if (hot)
			{
			over_obj = objc_find(ad_tools, ROOT, NIL, *mx, *my);
			if (over_obj != CLIPBORD && over_obj != TRASHCAN)
				hot_off();
			else if (over_obj != rcs_hot)
				{
				hot_off();
				invert_obj(ad_tools, rcs_hot = over_obj,&tools);
				}
			}
		} while (down);

	hot_off();
	p->g_x = o.g_x;
	p->g_y = o.g_y;
	}
	
	VOID clamp_rubbox(x, y, wlow, hlow, whigh, hhigh, ahigh, w, h, bound)
	WORD	x, y, wlow, hlow, whigh, hhigh;
	LONG	ahigh;
	WORD	*w, *h;
	GRECT	*bound;
	{
	GRECT	o;
	WORD	mx, my, down; 

	gsx_sclip(bound);
	gsx_attr(FALSE, MD_XOR, BLUE);
	r_set(&o, x, y, 0, 0);

	do {
		graf_mkstate(&mx, &my, &down, &down);
		o.g_w = max(mx - o.g_x + 1, wlow);
		o.g_h = max(my - o.g_y + 1, hlow);
		o.g_w = min(o.g_w, whigh);
		o.g_h = min(o.g_h, hhigh);
		if (ahigh)
			o.g_h = min(o.g_h, 
				(WORD) ((ahigh + o.g_w - 1) / o.g_w) );
		down = rub_wait(&o, &mx, &my);
		} while (down);

	*w = o.g_w;
	*h = o.g_h;
	}

	VOID obj_handle(tree, obj, p)
	LONG	tree;
	WORD	obj;
	GRECT	*p;
	{
	WORD	t;

	objc_xywh(tree, obj, p);
	t = max(p->g_w >> 3, gl_wchar);
	p->g_x += p->g_w - t;
	p->g_w = t;
	t = max(p->g_h >> 3, gl_hchar >> 1);
	p->g_y += p->g_h - t;
	p->g_h = t;
	}

	VOID about_dial()
	{
	LONG	tree;
	GRECT	p;

	objc_xywh(ad_menu, DESKMENU, &p);
	ini_tree(&tree, ABOUTREE);
	hndl_dial(tree, 0, &p);
	desel_obj(tree, AOKITEM);
	}

	VOID info_dial(obj)
	WORD	obj;
	{
	LONG	savname, name, tree, space;
	GRECT	p;
	WORD	temp, where; 
	BYTE	nobj[6], nib[6], nbb[6], nti[6], nimg[6], nstr[6];
	BYTE	ntree[6], ntot[8], nleft[8];

	ini_tree(&tree, INFODIAL);
	savname = GET_SPEC(tree, INFONAME);
	if (obj == NIL)
		{
		objc_xywh(ad_menu, OPTNMENU, &p);
		if (file_view())
			{
			if (rcs_state == NOFILE_STATE)
				name = string_addr(STTITLE);
			else 
				name = ADDR(rcs_title);
			mouse_form(HGLASS);	/* This could take a while..*/
			clr_tally();
			map_all(tally_obj);
			tally_free();
			rcs_tally.nbytes += LWGET(RSH_NTREE(head)) * sizeof(LONG);
			rcs_tally.nbytes += sizeof(RSHDR);
			mouse_form(ARROW);
			temp = LWGET(RSH_NTREE(head));
			merge_str(ntree, "%W", &temp);
			SET_SPEC(tree, INFOTREE, ADDR(ntree));
			}
		else
			{
			name = ADDR(rcs_title);
			hide_obj(tree, INFOPANL);
			clr_tally();
			map_tree(ad_view, ROOT, NIL, tally_obj);
			} 
		}
	else
		{
		objc_xywh(ad_view, obj, &p);
		hide_obj(tree, INFOPANL);
		if (file_view())
			{
			where = find_tree(trpan_f(obj - 1));
			name = ADDR(get_name(where));
			clr_tally();
			map_tree( tree_addr(trpan_f(obj - 1)), ROOT, NIL, tally_obj);
			}
		else
			{
			where = find_obj(ad_view, obj);
			if (where == NIL)
				name = savname;
			else
				name = ADDR(get_name(where));
			clr_tally();
			tree = ad_view;
			map_tree(tree, obj, GET_NEXT(tree, obj), tally_obj);
			ini_tree(&tree, INFODIAL);
			} 
		}

	SET_SPEC(tree, INFONAME, name);
	WORD LSTRLEN(BYTE *text);
	merge_str(nobj, "%W", &rcs_tally.nobj);
	SET_SPEC(tree, INFOOBJ, ADDR(nobj));
	merge_str(nib, "%W", &rcs_tally.nib);
	SET_SPEC(tree, INFOIB, ADDR(nib));
	merge_str(nbb, "%W", &rcs_tally.nbb);
	SET_SPEC(tree, INFOBB, ADDR(nbb));
	merge_str(nti, "%W", &rcs_tally.nti);
	SET_SPEC(tree, INFOTI, ADDR(nti));
	merge_str(nimg, "%W", &rcs_tally.nimg);
	SET_SPEC(tree, INFOIMG, ADDR(nimg));
	merge_str(nstr, "%W", &rcs_tally.nstr);
	SET_SPEC(tree, INFOSTR, ADDR(nstr));
	merge_str(ntot, "%L", &rcs_tally.nbytes);
	SET_SPEC(tree, INFOTOT, ADDR(ntot));
	space = avail_mem();
	merge_str(nleft, "%L", &space);
	SET_SPEC(tree, INFOLEFT, ADDR(nleft));
	hndl_dial(tree, 0, &p);

	desel_obj(tree, INFOOK);
	unhide_obj(tree, INFOPANL);
	SET_SPEC(tree, INFONAME, savname);
	if (file_view())
		dselct_tree(ad_view, obj);
	}

	WORD find_opt( opt )
	WORD	opt;
	{
	WORD 	idx;
		    
	for ( idx = 0; idx < NOPTS; idx++ )
	    if ( deflt_options[idx].option == opt ) return( idx );
	}				  

	BOOLEAN get_opstate( index )
	WORD	index;
	{
	
	return( deflt_options[index].state );

	}

	VOID set_opstate( index )
	WORD	index;
	{
	
	deflt_options[index].state = TRUE;

	}

	VOID read_inf(flag)
	WORD flag;
	{
	WORD total_opt;
	
	r_infhndl = dos_open(ADDR(&rcs_infile[0]),0);
	if (DOS_ERR)
	/* no in file so set output and safety options to standard defaults */
		{			 
		rcs_fsrtflag = FALSE;
		rcs_cflag = FALSE;
		rcs_hflag = TRUE;
		rcs_oflag = FALSE;
		rcs_cbflag = FALSE;
		rcs_f77flag = FALSE;
		rcs_lock = FALSE;
		rcs_xpert = FALSE;
		rsc_path[0] = '\0';
	}
	else
		{
		total_opt = sizeof(DEFLTS) * NOPTS;
		dmcopy(r_infhndl, 0x0L, ADDR(&deflt_options), total_opt);
		rcs_fsrtflag = get_opstate( find_opt( FSRTITEM ));
		rcs_cflag = get_opstate( find_opt( SRCITEM));
		rcs_hflag = get_opstate( find_opt( CITEM));
		rcs_oflag = get_opstate( find_opt( PASCITEM));
		rcs_cbflag = get_opstate( find_opt( CBITEM));
		rcs_f77flag = get_opstate( find_opt(F77ITEM));
		rcs_lock = get_opstate( find_opt( SAFELOCK ));	 
		rcs_xpert = get_opstate( find_opt( SAFEXPRT ));
		if(flag){
		   if(!dmcopy(r_infhndl, (LONG) total_opt, ADDR(rsc_path), 1))
			rsc_path[0] = '\0';  /*rsc_path is not in old .inf */
		   else if(rsc_path[0])
		    dmcopy(r_infhndl,(LONG) (total_opt+1), ADDR(&rsc_path[1]),
				(WORD) rsc_path[0] + 1); /*copy the 0 byte at the end of the path*/
		}
		dos_close( r_infhndl );
		}
	}	 

	VOID init_deflts()
	{
	deflt_options[0].option =  FSRTITEM;
	deflt_options[1].option =  SRCITEM;
	deflt_options[2].option =  CITEM;
	deflt_options[3].option =  PASCITEM;
	deflt_options[4].option =  CBITEM;
	deflt_options[5].option =  F77ITEM;
	deflt_options[6].option =  SAFELOCK;	 
	deflt_options[7].option =  SAFEXPRT;
	rsc_path[0] = '\0';
	}
	

	VOID wrte_inf()
	{	 
	WORD	idx;

	r_infhndl = dos_open( ADDR(&rcs_infile[0]), 1);
	if ( DOS_ERR )
		{
		r_infhndl = dos_create(ADDR(&rcs_infile[0]),0);
		init_deflts();
		}
	for ( idx = 0; idx < NOPTS; idx++ )
		deflt_options[idx].state = FALSE;
	if (rcs_fsrtflag ) set_opstate(find_opt(FSRTITEM) );
	if (rcs_cflag ) set_opstate(find_opt(SRCITEM));
	if (rcs_hflag ) set_opstate(find_opt(CITEM));
	if (rcs_oflag ) set_opstate(find_opt(PASCITEM));
	if (rcs_cbflag) set_opstate(find_opt(CBITEM));
	if (rcs_f77flag) set_opstate(find_opt(F77ITEM));
	if (rcs_lock) set_opstate(find_opt(SAFELOCK));
	if (rcs_xpert) set_opstate(find_opt(SAFEXPRT));
	dos_write( r_infhndl, NOPTS * sizeof(DEFLTS), ADDR(deflt_options));
	dos_write( r_infhndl, 1, ADDR(rsc_path));
	if(rsc_path[0])
	    dos_write(r_infhndl, (WORD) (rsc_path[0]+1), ADDR(&rsc_path[1]));
	dos_close( r_infhndl );
	}


	VOID outp_dial()
	{
	LONG	tree;
	GRECT	p;
	WORD	exitobj;

	ini_tree(&tree, OUTPTREE);
	if (rcs_cflag)
		sel_obj(tree, SRCITEM);
	if (rcs_hflag)
		sel_obj(tree, CITEM);
	if (rcs_oflag)
		sel_obj(tree, PASCITEM);
	if (rcs_cbflag)
		sel_obj(tree, CBITEM);
	if (rcs_f77flag)
		sel_obj(tree, F77ITEM);
	if (rcs_fsrtflag) 
		sel_obj(tree, FSRTITEM );
	objc_xywh(ad_menu, GLOBMENU, &p);
	exitobj = hndl_dial(tree, 0, &p);
	if (exitobj == OUTPOK)
		{			 
		rcs_fsrtflag = GET_STATE(tree, FSRTITEM) & SELECTED;
		rcs_cflag = GET_STATE(tree, SRCITEM) & SELECTED;
		rcs_hflag = GET_STATE(tree, CITEM) & SELECTED;
		rcs_oflag = GET_STATE(tree, PASCITEM) & SELECTED;
		rcs_cbflag = GET_STATE(tree, CBITEM) & SELECTED;
		rcs_f77flag = GET_STATE(tree, F77ITEM) & SELECTED;
		}
	map_tree(tree, ROOT, NIL, desel_obj);
	}

	VOID safe_dial()
	{
	LONG	tree;
	GRECT	p;
	WORD	exitobj;

	ini_tree(&tree, SAFEDIAL);
	if (rcs_lock)
		sel_obj(tree, SAFELOCK);
	else if (rcs_xpert)
		sel_obj(tree, SAFEXPRT);
else sel_obj(tree, SAFENORM);
	objc_xywh(ad_menu, GLOBMENU, &p);

	exitobj = hndl_dial(tree, 0, &p);

	if (exitobj == SAFEOK)

		{
		if (rcs_lock = SELECTED & GET_STATE(tree, SAFELOCK))
			{
			clr_clip();
			lock_menus();
			}
		rcs_xpert = SELECTED & GET_STATE(tree, SAFEXPRT);
		}
	map_tree(tree, ROOT, NIL, desel_obj);
	}

	VOID ini_prog()
	{
	GRECT 	p;

	ini_tree(&ad_prog, PROGRESS);	/* set up progress indicator */
	form_center(ad_prog, &prog.g_x, &prog.g_y, &prog.g_w, &prog.g_h);
	objc_draw(ad_prog, ROOT, MAX_DEPTH, 
		prog.g_x, prog.g_y, prog.g_w, prog.g_h);
	objc_xywh(ad_prog, WRITING, &p);
	prog.g_y = p.g_y;  prog.g_h = p.g_h;
	}

	VOID show_prog(strno)
	WORD	strno;
	{
	LONG	taddr;

	SET_SPEC(ad_prog, WRITING, taddr = string_addr(strno));
	WORD LSTRLEN(BYTE *text);
	do_posn(ad_prog, WRITING, JCENPOP);	/* Use our own editing */
	objc_draw(ad_prog, ROOT, MAX_DEPTH, 
		prog.g_x, prog.g_y, prog.g_w, prog.g_h);
	}

