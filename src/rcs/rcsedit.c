/*
 * Implements core object and tree editing operations for the resource
 * construction set. The file handles many of the direct manipulation
 * actions available in the editor.
 *
 * MIT License (see: LICENSE)
 * Copyright (C) 2026 tomaz stih
 */

#include "portab.h"
#include "machine.h"
#include "obdefs.h"
#include "dosbind.h"
#include "gembind.h"
#include "rcsdefs.h"
#include "rcs.h"
#include "rcsext.h"


	VOID set_text(tree, obj, addr, len)
	LONG	tree, addr;
	WORD	obj, len;
	{
	LONG	taddr;

	taddr = GET_SPEC(tree, obj);
	LLSET(TE_PTEXT(taddr), addr);
	LWSET(TE_TXTLEN(taddr), len);
	}

	VOID ted_set(taddr, tmplt, valid, text)
	LONG	taddr;
	BYTE	*tmplt, *valid, *text;
	{
	WORD	imap, itmp;

	if (!tmplt[0])
		{
		tmplt[0] = '@';
		LLSTRCPY(LLGET(TE_PVALID(taddr)), ADDR(valid));
		LLSTRCPY(LLGET(TE_PTEXT(taddr)), ADDR(text));
		if (!valid[0])
			valid[0] = '@';
		if (!text[0])
			text[0] = '@';
		return;
		}
	for (imap = itmp = 0; tmplt[itmp]; itmp++)
		if (tmplt[itmp] == '_')
			{
			tmplt[itmp] = '~';
			valid[itmp] = LBGET(imap + LLGET(TE_PVALID(taddr)));
			text[itmp] = LBGET(imap + LLGET(TE_PTEXT(taddr)));
			imap++;
			}
		else
			valid[itmp] = text[itmp] = '~';
	valid[itmp] = text[itmp] = '\0';
	}

	VOID ted_get(taddr, tmplt, valid, text)
	LONG	taddr;
	BYTE	*tmplt, *valid, *text;
	{
	WORD	imap, itmp;

	if (tmplt[0] == '@')
		tmplt[0] = '\0';
	else
		{
		for (imap = itmp = 0; tmplt[itmp]; itmp++)
			if (tmplt[itmp] == '~')
				{
				tmplt[itmp] = '_';
				valid[imap] = valid[itmp];
				text[imap] = text[itmp];
				imap++;
				}
		valid[imap] = text[imap] = '\0';
		}

	if (valid[0] == '@')
		valid[0] = '\0';
    /*	if (text[0] == '@')
		text[0] = '\0';    allow null string as space holder */
	update_if(TE_PTMPLT(taddr), tmplt);
	update_if(TE_PVALID(taddr), valid);
	update_if(TE_PTEXT(taddr), text);
	WORD strlen(BYTE *p1);
	WORD strlen(BYTE *p1);
	}

	VOID icon_set(taddr, text, chr)
	LONG	taddr;
	BYTE	*text, *chr;
	{
	LLSTRCPY(LLGET(IB_PTEXT(taddr)), ADDR(text));
	if (!text[0])
		text[0] = '@';
	chr[0] = LLOBT(LWGET(IB_CHAR(taddr))); 
	if (!chr[0])
		chr[0] = '@';
	chr[1] = '\0';
	}

	VOID icon_get(taddr, text, chr)
	LONG	taddr;
	BYTE	*text, *chr;
	{
	if (text[0] == '@')
		text[0] = '\0';
	update_if(IB_PTEXT(taddr), text);
	if (chr[0] == '@')
		chr[0] = '\0';
	LWSET(IB_CHAR(taddr), 
			LWGET(IB_CHAR(taddr)) & 0xff00 | (UWORD) chr[0]);
	} 

	WORD icon_cwhich(xy, wh, iwh)
	WORD	xy, wh, iwh;
	{
	if (!xy)
		return (0);
	if (xy == iwh - wh)
		return (2);
	return(1);
	}

	WORD icon_twhich(xy, iwh)
	WORD	xy, iwh;
	{
	if (!xy)
		return (0);
	if (xy == iwh) 
		return (2);
	return(1);
	}

	WORD icon_tapply(rule, wh, iwh)
	WORD	rule, wh, iwh;
	{
	switch (rule) {
		case 0:
			return ( 0 );
		case 1:
			return (iwh - wh) / 2;
		case 2:
			return (iwh);
		default:
			return (0);
			}
	} 

	WORD icon_capply(rule, wh, iwh)
	WORD	rule, wh, iwh;
	{
	switch (rule) {
		case 0:
			return (0);
		case 1:
			return (iwh - wh) / 2;
		case 2:
			return (iwh - wh);
		default:
			return (0);
			}
	} 

	VOID get_obf(tree, obj, type, obspec)
	LONG	tree, *obspec;
	WORD	obj, *type;
	{
	*type = LLOBT(GET_TYPE(tree, obj));
	*obspec = GET_SPEC(tree, obj);
	}

	VOID get_fields(tree, sobj, type, obspec, p)
	LONG	tree;
	WORD	sobj, *type;
	GRECT	*p;
	LONG	*obspec;
	{
	*type = LLOBT(GET_TYPE(tree, sobj));
	*obspec = OB_SPEC(sobj);	/* Notice that this is the ADDR of */
	objc_xywh(tree, sobj, p);	/* the OB_SPEC, not the value! */
	}

	VOID edit_str(tree, sobj)
	LONG	tree;
	WORD	sobj;
	{
	LONG	obspec; 
	WORD	where, type, exitobj, ok;
	GRECT	p;
	WORD	min_width, neww, len; 
	BYTE	text[73], name[9]; 

	get_fields(tree, sobj, &type, &obspec, &p);
	if ((len = LSTRLEN(LLGET(obspec))) > 72)
		return;
	ini_tree(&tree, STRDIAL);
	where = set_obname(tree, STRNAME, name, ad_view, sobj);
	set_text(tree, OSTRITEM, ADDR(&text[0]), 73);
	LLSTRCPY(LLGET(obspec), ADDR(&text[0]));
	if ( rcs_state == ALRT_STATE )
	   hide_obj( tree, STRNAME);
	if (!text[0])
		text[0] = '@';
	do {
		exitobj = hndl_dial(tree, OSTRITEM, &p);
		desel_obj(tree, exitobj);
		ok = DEFAULT & GET_FLAGS(tree, exitobj);
	} while ( ok && !name_ok(name, where, TRUE));

	if (ok)
		{
		rcs_edited = TRUE;
		get_obname(name, ad_view, sobj);
		if (text[0] == '@')
			text[0] = '\0';

		if (rcs_state == ALRT_STATE)
			{		
			if (strlen(text) > 40 )
				{
				hndl_alert(1, string_addr(STOOLONG));
				text[40] = '\0';
				}
			update_if(obspec, &text[0]);
			fix_alert(ad_view);
			unhide_obj(tree, STRNAME);
			}
		else
			{
			neww = gl_wchar * strlen(text); 
			if (rcs_state == MENU_STATE)
				{
				if (type == G_TITLE)
					{
					if (!newsize_obj(ad_view, sobj, neww,gl_hchar, TRUE))
						text[len] = '\0';
					fix_menu_bar(ad_view);
					}
				else if (in_which_menu(ad_view, sobj) == 1
					&& in_menu(ad_view, sobj) == 1
					&& strlen(text) > 20 )
		       			{
					hndl_alert(1, string_addr(STOOLONG));	
					text[20] = '\0';
					}
				else  if(!newsize_obj(ad_view, sobj, neww,gl_hchar, FALSE))
						text[len] = '\0';
				}
			else if (type == G_STRING || type == G_BUTTON )
				{
				min_width = GET_WIDTH(ad_view, sobj);
				if( neww > min_width )
					if(!newsize_obj(ad_view, sobj, neww,gl_hchar, FALSE))
						text[len] = '\0';
				}
			}
		if( rcs_state != ALRT_STATE)
			update_if(obspec, &text[0]);
		}
	}

	VOID edit_box(tree, sobj)
	LONG	tree;
	WORD	sobj;
	{
	LONG	obspec; 
	WORD	where, type, exitobj, ok, nilok;
	BYTE	name[9], text[2], bxchar;
	GRECT	p;

	if (rcs_state == ALRT_STATE)
		return;

	get_fields(tree, sobj, &type, &obspec, &p);

	ini_tree(&tree, BOXDIAL);
	where = set_obname(tree, BOXNAME, name, ad_view, sobj);
	if ( where != NIL && tree_view() && tree_kind( get_kind(where)) )
		nilok = FALSE;
	else
		nilok = TRUE;
	bxchar = LHIBT(LHIWD(LLGET(obspec)));
	set_text(tree, OCHRITEM, ADDR(&text[0]), 2);
	text[0] = bxchar? bxchar: '@';
	text[1] = '\0';

	do {
		exitobj = hndl_dial(tree, OCHRITEM, &p);
		desel_obj(tree, exitobj);
		ok = DEFAULT & GET_FLAGS(tree, exitobj);
	} while (ok && !name_ok(name, where, nilok));

	if (ok)
		{
		rcs_edited = TRUE;
		get_obname(name, ad_view, sobj);
		bxchar = (text[0] == '@')? '\0': text[0];
		LLSET(obspec, LLGET(obspec) & 0xffffffL | 
			(LONG) ((UWORD) bxchar) << 24);
		}
	}

	VOID edit_text(tree, sobj)
	LONG	tree;
	WORD	sobj;
	{
	LONG	obspec, taddr; 
	WORD	min_width, where, type, deftext, w, h, exitobj, ok;
	GRECT	p;
	BYTE	text[73], valid[73], tmplt[73], name[9]; 

	if (rcs_state == ALRT_STATE)
		return;

	get_fields(tree, sobj, &type, &obspec, &p);
	taddr = LLGET(obspec);
	if (type == G_FTEXT || type == G_FBOXTEXT)
	if (LSTRLEN(LLGET(TE_PTMPLT(taddr))) > 72)
		return;

	ini_tree(&tree, TEXTDIAL);
	where = set_obname(tree, TEXTNAME, name, ad_view, sobj);
	set_text(tree, OTMPITEM, ADDR(&tmplt[0]), 73);
	set_text(tree, OVALITEM, ADDR(&valid[0]), 73);
	set_text(tree, OTEXITEM, ADDR(&text[0]), 73);
	LLSTRCPY(LLGET(TE_PTMPLT(taddr)), ADDR(&tmplt[0]));
	ted_set(taddr, &tmplt[0], &valid[0], &text[0]);

	if (type == G_TEXT || type == G_BOXTEXT)
		{
		if (LSTRLEN(LLGET(TE_PTEXT(taddr))) > 72) return;
		hide_obj(tree, TMPLTTAG);
		hide_obj(tree, OTMPITEM);
		hide_obj(tree, VALIDTAG);
		hide_obj(tree, OVALITEM);
		deftext = OTEXITEM;
		}		   
	else
		deftext = (tmplt[0] != '@')? OTMPITEM: OTEXITEM;
	do {
		exitobj = hndl_dial(tree, deftext, &p);
		desel_obj(tree, exitobj);
		ok = DEFAULT & GET_FLAGS(tree, exitobj);
	} while (ok && !name_ok(name, where, TRUE));

	if (ok)
		{
		rcs_edited = TRUE;
		get_obname(name, ad_view, sobj);
		ted_get(taddr, &tmplt[0], &valid[0], &text[0]);
		if (type == G_TEXT || type == G_FTEXT || type == G_BOXTEXT)
			{
			text_wh(taddr, type, &w, &h);
			min_width = GET_WIDTH(ad_view, sobj);
			if ( w > min_width)
				newsize_obj(ad_view, sobj, w, h);		   
			}
		}
	
	unhide_obj(tree,TMPLTTAG);
	unhide_obj(tree,OTMPITEM);
	unhide_obj(tree, VALIDTAG);
	unhide_obj(tree,OVALITEM);
	map_tree(tree, ROOT, NIL, desel_obj);	/* clear radio buttons */
	}

	VOID edit_bit(tree, sobj)
	LONG	tree;
	WORD	sobj;
	{
	WORD	where, exitobj, ok;
	BYTE	name[9];
	GRECT	p;

	if (rcs_state == ALRT_STATE)
		return;

	objc_xywh(tree, sobj, &p);
	ini_tree(&tree, BITDIAL);
	where = set_obname(tree, BITNAME, name, ad_view, sobj);

	do {
		exitobj = hndl_dial(tree, 0, &p);
		desel_obj(tree, exitobj);
		ok = DEFAULT & GET_FLAGS(tree, exitobj);
	} while (ok && !name_ok(name, where, TRUE));

	if (ok)
		{
		rcs_edited = TRUE;
		get_obname(name, ad_view, sobj);
		}
	}

	VOID edit_icon(tree, sobj)
	LONG	tree;
	WORD	sobj;
	{
	LONG	obspec, taddr; 
	WORD	where, type, itmp, exitobj, ok;
	GRECT	p;
	BYTE	text[14], valid[2], name[9]; 
	WORD	trule, crule, w, h;

	if (rcs_state == ALRT_STATE)
		return;

	get_fields(tree, sobj, &type, &obspec, &p);

	ini_tree(&tree, ICONDIAL);
	where = set_obname(tree, ICONNAME, name, ad_view, sobj);
	taddr = LLGET(obspec);
	set_text(tree, IBTEXT, ADDR(&text[0]), 13);
	set_text(tree, IBCHAR, ADDR(&valid[0]), 2);
	icon_set(taddr, &text[0], &valid[0]);
	trule = icon_twhich(LWGET(IB_YTEXT(taddr)), LWGET(IB_HICON(taddr)));
	sel_obj(tree, IBTPOS0 + trule);
	itmp = icon_cwhich(LWGET(IB_YCHAR(taddr)),
		gl_hschar, LWGET(IB_HICON(taddr)));
	crule = icon_cwhich(LWGET(IB_XCHAR(taddr)),
		gl_wschar, LWGET(IB_WICON(taddr)));
	crule += 3 * itmp;
	sel_obj(tree, IBCPOS0 + crule);

	do {
		exitobj = hndl_dial(tree, IBTEXT, &p);
		desel_obj(tree, exitobj);
		ok = DEFAULT & GET_FLAGS(tree, exitobj);
	} while (ok && !name_ok(name, where, TRUE));

	if (ok)
		{
		rcs_edited = TRUE;
		get_obname(name, ad_view, sobj);
		icon_get(taddr, &text[0], &valid[0]);
		itmp = encode(tree, IBTPOS0, 3);
		if (itmp != trule)
			{		
			itmp = icon_tapply(itmp, gl_hschar,
				LWGET(IB_HICON(taddr)));
			LWSET(IB_YTEXT(taddr), itmp);
			}
		WORD strlen(BYTE *p1);
		icon_tfix(taddr);
		itmp = encode(tree, IBCPOS0, 9);
		if (itmp != crule)
			{
			crule = itmp;
			itmp = icon_capply(crule / 3, gl_hschar,
				LWGET(IB_HICON(taddr)));
			LWSET(IB_YCHAR(taddr), itmp);
			itmp = icon_capply(crule % 3, gl_wschar,
				LWGET(IB_WICON(taddr)));
			LWSET(IB_XCHAR(taddr), itmp);
			}
		icon_wh(taddr, &w, &h);
		newsize_obj(ad_view, sobj, w, h, FALSE);
		}
	map_tree(tree, ROOT, NIL, desel_obj);	/* clear radio buttons */
	}

	VOID open_obj(sobj)
	WORD	sobj;
	{
	LONG	tree; 
	WORD	type; 

	if (rcs_state == MENU_STATE)
	if (!in_bar(ad_view, sobj))
	if (!menu_ok(ad_view, sobj))
		return;
	if (rcs_state == FREE_STATE)
	if (sobj == ROOT)
		return;

	tree = ad_view;
	obj_redraw(tree, sobj);
	type = LLOBT(GET_TYPE(tree, sobj));

	switch (type) {
		case G_TITLE:
		case G_STRING:
		case G_BUTTON:
			edit_str(tree, sobj);
			break;
		case G_IBOX:
		case G_BOX:
		case G_BOXCHAR:
			edit_box(tree, sobj);
			break;
		case G_TEXT:
		case G_BOXTEXT:
		case G_FTEXT:
		case G_FBOXTEXT:
			edit_text(tree, sobj);
			break;
		case G_IMAGE:
			edit_bit(tree, sobj);
			break;
		case G_ICON:
			edit_icon(tree, sobj);
			break;
		default:
			break;
		}

	if (rcs_state == MENU_STATE || rcs_state == ALRT_STATE)
		view_objs();
else obj_redraw(ad_view, sobj);
	}

	VOID do_bdcol(tree, obj, item)
	LONG	tree;
	WORD	obj, item;
	{
	LONG	obspec;
	WORD	type, color;

	color = item - COL0POP;
	get_obf(tree, obj, &type, &obspec);
	switch (type) {
		case G_BOX:
		case G_IBOX:
		case G_BOXCHAR:
			obspec &= 0xffff0fffL;
			SET_SPEC(tree, obj, obspec | (LONG) color << 12);
			break;
		case G_BOXTEXT:
		case G_FBOXTEXT:
			color = (color << 12) | (LWGET(TE_FG(obspec))
				& 0xfff);
			LWSET(TE_FG(obspec), color);
			break;
		default:
			return;
		}
	rcs_edited = TRUE;
	}

	VOID do_bgcol(tree, obj, item)
	LONG	tree;
	WORD	obj, item;
	{
	LONG	obspec;
	WORD	type, color;

	color = item - COL0POP;
	get_obf(tree, obj, &type, &obspec);
	switch (type) {
		case G_BOX:
		case G_BOXCHAR:
			obspec &= 0xfffffff0L;
			SET_SPEC(tree, obj, obspec | (LONG) color);
			break;
		case G_BOXTEXT:
		case G_FBOXTEXT:
			color = color | (LWGET(TE_FG(obspec)) & 0xfff0);
			LWSET(TE_FG(obspec), color);
			break;
		case G_ICON:
			color = (color << 8) | (LWGET(IB_CHAR(obspec))
				& 0xf0ff);
			LWSET(IB_CHAR(obspec), color);
			break;
		default:
			return;
		}
	rcs_edited = TRUE;
	}

	VOID do_fgcol(tree, obj, item)
	LONG	tree;
	WORD	obj, item;
	{
	LONG	obspec;
	WORD	type, color;

	color = item - COL0POP;
	get_obf(tree, obj, &type, &obspec);
	switch (type) {
		case G_BOXCHAR:
			obspec &= 0xfffff0ffL;
			SET_SPEC(tree, obj, obspec | (LONG) color << 8);
			return;
		case G_TEXT:
		case G_FTEXT:
		case G_BOXTEXT:
		case G_FBOXTEXT:
			color = (color << 8) | (LWGET(TE_FG(obspec)) 
				& 0xf0ff);
			LWSET(TE_FG(obspec), color);
			break;
		case G_ICON:
			color = (color << 12) | (LWGET(IB_CHAR(obspec))
				& 0xfff);
			LWSET(IB_CHAR(obspec), color);
			break;
		case G_IMAGE:
			LWSET(BI_COLOR(obspec), color);
			break;
		default:
			return;
		}
	rcs_edited = TRUE;
	}

	VOID do_patrn(tree, obj, item)
	LONG	tree;
	WORD	obj, item;
	{
	LONG	obspec;
	WORD	type, patrn;

	patrn = item - PAT0POP;
	get_obf(tree, obj, &type, &obspec);
	switch (type) {
		case G_BOX:
		case G_BOXCHAR:
			obspec &= 0xffffff8fL;
			SET_SPEC(tree, obj, obspec | (LONG) patrn << 4);
			break;
		case G_BOXTEXT:
		case G_FBOXTEXT:
			patrn = (patrn << 4) | (LWGET(TE_FG(obspec)) & 0xff8f);
			LWSET(TE_FG(obspec), patrn);
			break;
		default:
			return;
		}
	rcs_edited = TRUE;
	}

	VOID do_thick(tree, obj, item)
	LONG	tree;
	WORD	obj, item;
	{
	LONG	obspec;
	WORD	type, i, thick;

	for (i = 0; rcs_it2thk[i].mfrom && rcs_it2thk[i].mfrom != item; i++);
	if (!rcs_it2thk[i].mfrom)
		return;
	else
		thick = rcs_it2thk[i].mto;
	get_obf(tree, obj, &type, &obspec);
	switch (type) {
		case G_BOX:
		case G_IBOX:
		case G_BOXCHAR:
			obspec &= 0xff00ffffL;
			SET_SPEC(tree, obj, obspec |
				(LONG) (thick & 0xff) << 16);
			break;
		case G_BOXTEXT:
		case G_FBOXTEXT:
			LWSET(TE_THICK(obspec), thick);
			break;
		default:
			return;
		}
	rcs_edited = TRUE;
	}

	VOID do_rule(tree, obj, item)
	LONG	tree;
	WORD	obj, item;
	{
	LONG	obspec;
	WORD	type, rule;

	rule = item == RPLACPOP? 1: 0;
	get_obf(tree, obj, &type, &obspec);
	switch (type) {
		case G_BOXCHAR:
			if (item > RPLACPOP)
				break;
			obspec &= 0xffffff7fL;
			SET_SPEC(tree, obj, obspec | (LONG) rule << 7);
			break;
		case G_TEXT:
		case G_FTEXT:
		case G_BOXTEXT:
		case G_FBOXTEXT:
			if (item <= RPLACPOP)
				{
				rule = (rule << 7) | 
					(LWGET(TE_FG(obspec)) & 0xff7f);
				LWSET(TE_FG(obspec), rule);
				}
			else if (item <= SMALLPOP)
				LWSET(TE_FONT(obspec), item == SMALLPOP?
					SMALL: IBM);
			else if (item <= CENTPOP)
				LWSET(TE_JUST(obspec), item - LEFTPOP);

			break;
		default:
			return;
		}
	rcs_edited = TRUE;
	}

	VOID do_posn(tree, obj, item)
	LONG	tree;
	WORD	obj, item;
	{
	WORD	pobj;
	GRECT	o, p;

	if (obj == ROOT)
		return;
	pobj = get_parent(tree, obj);
	objc_xywh(tree, pobj, &p);
	objc_xywh(tree, obj, &o);
	o.g_x = GET_X(tree, obj);
	o.g_y = GET_Y(tree, obj);

	switch (item) {
		case JLFTPOP:
			o.g_x = 0;
			break;
		case JRGTPOP:
			o.g_x = max(0, p.g_w - o.g_w);
			break;
		case JCENPOP:
			o.g_x = max(0, (p.g_w - o.g_w) / 2);
			break;
		case JTOPPOP:
			o.g_y = 0;
			break;
		case JBOTPOP:
			o.g_y = max(0, p.g_h - o.g_h);
			break;
		case JMIDPOP:
			o.g_y = max(0, (p.g_h - o.g_h) / 2);
			break;
		case VFILLPOP:
			o.g_h = p.g_h;
			o.g_y = 0;
			break;
		case HFILLPOP:
			o.g_w = p.g_w;
			o.g_x = 0;
			break;
		case SNAPPOP:
			break;
		default:
			return;
		}
	rcs_edited = TRUE;
	if (rcs_state != PANL_STATE || item == SNAPPOP)
		{
		snap_xy(&o.g_x, &o.g_y);
		snap_wh(&o.g_w, &o.g_h);
		}
	obj_redraw(tree, obj);
	ob_setxywh(tree, obj, &o);
	obj_redraw(tree, obj);
	}

	VOID do_swtch(tree, obj, item)
	LONG	tree;
	WORD	obj, item;
	{
	LONG	where;
	WORD	i, mask;

	if (item == HDDNPOP && obj == ROOT)
		return;
	for (i = 0; rcs_bit2obj[i].objno && rcs_bit2obj[i].objno != item; i++);
	if (!rcs_bit2obj[i].objno)
		return;
	if (mask = rcs_bit2obj[i].flagmask)
		where = OB_FLAGS(obj);
	else if (mask = rcs_bit2obj[i].statemask)
		where = OB_STATE(obj);
	else
		return;
	if(!(LWGET(where) & mask)) rcs_edited = TRUE;
	LWSET(where, LWGET(where) ^ mask);
	}
