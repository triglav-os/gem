/*
 * Implements shared utility routines used throughout the resource
 * construction set. It collects helpers for geometry, memory, selection,
 * and object bookkeeping.
 *
 * MIT License (see: LICENSE)
 * Copyright (C) 2026 tomaz stih
 */

#include "portab.h"
#include "machine.h"
#include "obdefs.h"
#include "gsxdefs.h"
#include "dosbind.h"
#include "gembind.h"
#include "rcsdefs.h"
#include "rcsext.h"

EXTERN	FDB	gl_dst;

	VOID set_slsize(w_handle, h_size, v_size)
	WORD	w_handle, h_size, v_size;
	{
	WORD	cur_size, foo;

	wind_get(w_handle, WF_HSLSIZ, &cur_size, &foo, &foo, &foo);
	if (cur_size != h_size)
		wind_set(w_handle, WF_HSLSIZ, h_size, 0, 0, 0);
	wind_get(w_handle, WF_VSLSIZ, &cur_size, &foo, &foo, &foo);
	if (cur_size != v_size)
		wind_set(w_handle, WF_VSLSIZ, v_size, 0, 0, 0);
	}	

	VOID set_slpos(w_handle, h_pos, v_pos)
	WORD	w_handle, h_pos, v_pos;
	{
	WORD	cur_pos, foo;

	wind_get(w_handle, WF_HSLIDE, &cur_pos, &foo, &foo, &foo);
	if (cur_pos != h_pos)
		wind_set(w_handle, WF_HSLIDE, h_pos, 0, 0, 0);
	wind_get(w_handle, WF_VSLIDE, &cur_pos, &foo, &foo, &foo);
	if (cur_pos != v_pos)
		wind_set(w_handle, WF_VSLIDE, v_pos, 0, 0, 0);
	}

	WORD
ini_tree(tree, which)		/* find tree address */
	LONG	*tree;
	WORD	which;
	
	{
	return ( rsrc_gaddr(R_TREE, which, tree) );
	}

	LONG image_addr(which)
	WORD	which;
	{
	LONG	where;

	rsrc_gaddr(R_IMAGEDATA, which, &where);
	return (where);
	} 

	LONG string_addr(which)
	WORD	which;
	{
	LONG	where;

	rsrc_gaddr(R_STRING, which, &where);
	return (where);
	} 

	VOID arrange_tree(tree, wint, hint, maxw, maxh)
	LONG	tree;
	WORD	wint, hint, *maxw, *maxh;
	{
	WORD	obj, x, y, rowh, wroot;

	if ( !(wroot = GET_WIDTH(tree, ROOT)) )
		return;
	x = wint;
	y = hint;
	rowh = 0;
	*maxw = 0;

	for (obj = GET_HEAD(tree, ROOT); obj != ROOT; 
		obj = GET_NEXT(tree, obj))
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
			*maxw = max(*maxw, x);
			rowh = max(rowh, GET_HEIGHT(tree, obj));
			}
		}
	*maxh = y + rowh + hint;
	}

/*
*	Routine that will find the parent of a given object.  The
*	idea is to walk to the end of our siblings and return
*	our parent.  If object is the root then return NIL as parent.
*/
WORD get_parent(LONG tree, WORD obj)
{
	WORD		pobj;

	if (obj == NIL)
		return (NIL);
	pobj = GET_NEXT(tree, obj);
	if (pobj != NIL)
	{
	  while( GET_TAIL(tree, pobj) != obj ) 
	  {
	    obj = pobj;
	    pobj = GET_NEXT(tree, obj);
	  }
	}
	return(pobj);
} /* get_parent */

	WORD nth_child(tree, nobj, n)
	LONG	tree;
	WORD	nobj, n;
	{
	for (nobj = GET_HEAD(tree, nobj); --n; nobj = GET_NEXT(tree, nobj));
	return (nobj);
	}

	WORD child_of(tree, nobj, obj)
	WORD	nobj, obj;
	LONG	tree;
	{
	WORD	nbar;

	nobj = GET_HEAD(tree, nobj);
	for (nbar = 1; nobj != obj; nbar++)
		nobj = GET_NEXT(tree, nobj);
	return (nbar);
	}

	VOID table_code(tree, obj0, types, ntypes, type)
	LONG	tree;
	WORD	obj0, *types, ntypes, type;
	{
	WORD	itype;

	for (itype = 0; itype < ntypes; itype++)
		if (type == types[itype])
			{
			sel_obj(tree, obj0 + itype);
			return;
			}
	}

	WORD encode(tree, ob1st, num)
	LONG	tree;
	WORD	ob1st, num;
	{
	for (; num--; )
		if (GET_STATE(tree, ob1st+num) & SELECTED)
			return(num);
	return (0);
	}

VOID map_tree(LONG tree, WORD this, WORD last, WORD (*routine)())
{
	WORD		tmp1;
						/* non-recursive tree	*/
						/*   traversal		*/
child:
						/* see if we need to	*/
						/*   to stop		*/
	if ( this == last)
	  return;
						/* do this object	*/
	(*routine)(tree, this);
						/* if this guy has kids	*/
						/*   then do them	*/
	tmp1 = GET_HEAD(tree, this);

	if ( tmp1 != NIL )
	{
	  this = tmp1;
	  goto child;
	}
sibling:
						/* if this obj. has a	*/
						/*   sibling that is not*/
						/*   his parent, then	*/
						/*   move to him and do	*/
						/*   him and his kids	*/
	tmp1 = GET_NEXT(tree, this);
	if ( (tmp1 == last) ||
	     (tmp1 == NIL) )
	  return;

	if ( GET_TAIL(tree, tmp1) != this )
	{
	  this = tmp1;
	  goto child;
	}
						/* if this is the root	*/
						/*   which has no parent*/
						/*   then stop else 	*/
						/*   move up to the	*/
						/*   parent and finish	*/
						/*   off his siblings	*/ 
	this = tmp1;
	goto sibling;
}

	VOID snap_xy(x, y)
	WORD 	*x, *y;
	{
	*x += gl_wchar / 2;	
	*y += gl_hchar / 2;
	*x -= *x % gl_wchar;
	*y -= *y % gl_hchar;
	}

	VOID snap_wh(w, h)
	WORD	*w, *h;
	{
	*w += gl_wchar / 2;
	*h += gl_hchar / 2;
	*w = max(*w - *w % gl_wchar, gl_wchar);
	*h = max(*h - *h % gl_hchar, gl_hchar);
	}

	VOID text_wh(taddr, type, w, h)
	LONG	taddr;
	WORD	type, *w, *h;
	{
	WORD	font;

	font = LWGET(TE_FONT(taddr));
	taddr = LLGET( (type == G_TEXT || type == G_BOXTEXT)? TE_PTEXT(taddr): TE_PTMPLT(taddr) );
	*h = ch_height(font);
	*w = ch_width(font) * LSTRLEN(taddr);
	}

	VOID icon_wh(taddr, w, h)
	LONG	taddr;
	WORD	*w, *h;
	{
	ICONBLK	here;
	GRECT	p;

	BYTE LBCOPY(LONG dst, LONG src, WORD count);
	rc_copy((GRECT *) &here.ib_xchar, &p);
	rc_union((GRECT *) &here.ib_xicon, &p);
	rc_union((GRECT *) &here.ib_xtext, &p);
	*w = p.g_x + p.g_w;
	*h = p.g_y + p.g_h;
	}
       

	VOID icon_tfix(taddr)
	LONG	taddr;
	{
	WORD	dw;

	dw = (LWGET(IB_WICON(taddr)) - LWGET(IB_WTEXT(taddr))) / 2;
	LWSET(IB_XICON(taddr), (dw<0)? -dw: 0);
	LWSET(IB_XTEXT(taddr), (dw<0)? 0: dw);
	}


	VOID trans_bitblk(obspec)
	LONG	obspec;
	{
	LONG	taddr;
	WORD	wb, hl;

	if ( (taddr = LLGET(BI_PDATA(obspec))) == -1L)
		return;
	wb = LWGET(BI_WB(obspec));
	hl = LWGET(BI_HL(obspec));
	gsx_trans(taddr, wb, taddr, wb, hl);
	}

VOID grect_to_array(GRECT *area, WORD array[])
{
    array[0] = area->g_x;
    array[1] = area->g_y;
    array[2] = area->g_x + area->g_w - 1;
    array[3] = area->g_y + area->g_h - 1;
}


VOID rast_op(WORD mode, GRECT *s_area, MFDB *s_mfdb, GRECT *d_area, MFDB *d_mfdb)
{
    WORD	pxy[8];

    grect_to_array(s_area, pxy);
    grect_to_array(d_area, &pxy[4]);
    vro_cpyfm( mode, pxy, s_mfdb, d_mfdb);
}

	VOID outl_obj(tree, obj, clprect)
	LONG	tree;
	WORD	obj;
	GRECT	*clprect;
	{
	GRECT	p;

	objc_xywh(tree, obj, &p);
	rc_intersect( clprect, &p);
	gsx_moff();
	gsx_outline(&p);
	gsx_mon();
	}

	VOID invert_obj(tree, obj, clprect)
	LONG	tree;
	WORD	obj;	     
	GRECT	*clprect;
	{
	GRECT	hot;

	objc_xywh(tree, obj, &hot);
	rc_intersect( clprect, &hot );
	gsx_moff();
	gsx_invert(&hot);
	gsx_mon();
	if(iconedit_flag){
		if(LWGET(OB_STATE(obj)) == SELECTED)
		    LWSET(OB_STATE(obj), NORMAL);
else LWSET(OB_STATE(obj), SELECTED);
		}
	}

	VOID trans_obj(tree, obj)
	LONG	tree;
	WORD	obj;
	{
	WORD	type, wb, hl;
	LONG	taddr, obspec;

	type = LLOBT(GET_TYPE(tree, obj));
	if ( (obspec = GET_SPEC(tree, obj)) == -1L)
		return;
	switch (type) {
		case G_IMAGE:
			trans_bitblk(obspec);
			return;
		case G_ICON:
			hl = LWGET(IB_HICON(obspec));
			wb = (LWGET(IB_WICON(obspec)) + 7) >> 3;
			if ( (taddr = LLGET(IB_PDATA(obspec))) != -1L)
				gsx_trans(taddr, wb, taddr, wb, hl);
			if ( (taddr = LLGET(IB_PMASK(obspec))) != -1L)
				gsx_trans(taddr, wb, taddr, wb, hl);
			return;
		default:
			return;
		}
	}

	VOID unfix_chpos(where, x_or_y)
	LONG	where;
	WORD	x_or_y;
	{
	WORD	cpos, coff, cbits;

	cpos = LWGET(where);
	coff = cpos / (x_or_y? gl_hchar: gl_wchar);
	cbits = cpos % (x_or_y? gl_hchar: gl_wchar);
	LWSET(where, coff | (cbits << 8));
	}

/************************************************************************/
/* fix_chpos								*/
/************************************************************************/
VOID fix_chpos(LONG pfix, WORD ifx)
{
	WORD	coffset;
	WORD	cpos;

	cpos = LWGET(pfix);
	coffset = (WORD) LHIBT(cpos) & 0xff;	/* Alcyon bug ! */
	cpos = (WORD) LLOBT(cpos) & 0xff;
	if (ifx)
	  cpos *= gl_wchar;
	else
	  cpos *= gl_hchar;
	if ( coffset > 128 )			/* crude	*/
	  cpos -= (256 - coffset);
	else
	  cpos += coffset;
	LWSET(pfix, cpos);
} /* fix_chpos() */


/************************************************************************/
/* rs_obfix								*/
/************************************************************************/
VOID rs_obfix(LONG tree, WORD curob)
{
	WORD		i, val;
	LONG		p;
						/* set X,Y,W,H with	*/
						/*   fixch, use val	*/
						/*   to alternate TRUEs	*/
						/*   and FALSEs		*/
	p = OB_X(curob);

	val = TRUE;
	for (i=0; i<4; i++)
	{
	  fix_chpos(p+(LONG)(2*i), val);
	  val = !val;
	}
}

	VOID undo_obj(tree, which, bit)
	LONG	tree;
	WORD	which, bit;
	{
	WORD	state;

	state = GET_STATE(tree, which);
	SET_STATE(tree, which, state & ~bit);
	}

	VOID do_obj(tree, which, bit)
	LONG	tree;
	WORD	which, bit;
	{
	WORD	state;

	state = GET_STATE(tree, which);
	SET_STATE(tree, which, state | bit);
	}

	VOID enab_obj(tree, which)
	LONG	tree;
	WORD	which;
	{
	undo_obj(tree, which, DISABLED);
	}

	VOID disab_obj(tree, which)
	LONG	tree;
	WORD	which;
	{
	do_obj(tree, which, DISABLED);
	}

	VOID sel_obj(tree, which)
	LONG	tree;
	WORD	which;
	{
	do_obj(tree, which, SELECTED);
	}

	VOID desel_obj(tree, which)
	LONG	tree;
	WORD	which;
	{
	undo_obj(tree, which, SELECTED);
	}

	VOID chek_obj(tree, which)
	LONG	tree;
	WORD	which;
	{
	do_obj(tree, which, CHECKED);
	}

	VOID unchek_obj(tree, which)
	LONG	tree;
	WORD	which;
	{
	undo_obj(tree, which, CHECKED);
	}

	VOID unflag_obj(tree, which, bit)
	LONG	tree;
	WORD	which, bit;
	{
	WORD	flags;

	flags = GET_FLAGS(tree, which);
	SET_FLAGS(tree, which, flags & ~bit);
	}

	VOID flag_obj(tree, which, bit)
	LONG	tree;
	WORD	which, bit;
	{
	WORD	flags;

	flags = GET_FLAGS(tree, which);
	SET_FLAGS(tree, which, flags | bit);
	}

	VOID hide_obj(tree, which)
	LONG	tree;
	WORD	which;
	{
	flag_obj(tree, which, HIDETREE);
	}

	VOID unhide_obj(tree, which)
	LONG	tree;
	WORD	which;
	{
	unflag_obj(tree, which, HIDETREE);
	}

	VOID indir_obj(tree, which)
	LONG	tree;
	WORD	which;
	{
	flag_obj(tree, which, INDIRECT);
	}

	VOID dir_obj(tree, which)
	LONG	tree;
	WORD	which;
	{
	unflag_obj(tree, which, INDIRECT);
	}

	VOID sble_obj(tree, which)
	LONG	tree;
	WORD	which;
	{
	flag_obj(tree, which, SELECTABLE);
	}

	VOID unsble_obj(tree, which)
	LONG	tree;
	WORD	which;
	{
	unflag_obj(tree, which, SELECTABLE);
	}

	VOID objc_xywh(tree, obj, p)
	LONG	tree;
	WORD	obj;
	GRECT	*p;
	{
	objc_offset(tree, obj, &p->g_x, &p->g_y);
	p->g_w = GET_WIDTH(tree, obj);
	p->g_h = GET_HEIGHT(tree, obj);
	}

VOID ob_setxywh(LONG tree, WORD obj, GRECT *pt)
{
	WORD LWCOPY(LONG dst, LONG src, WORD count);
}


/************************************************************************/
/* o b _ r e l x y w h							*/
/************************************************************************/
VOID ob_relxywh(LONG tree, WORD obj, GRECT *pt)
{
	WORD LWCOPY(LONG dst, LONG src, WORD count);
} /* ob_relxywh */


VOID r_get(GRECT *pxywh, WORD *px, WORD *py, WORD *pw, WORD *ph)
{
	*px = pxywh->g_x;
	*py = pxywh->g_y;
	*pw = pxywh->g_w;
	*ph = pxywh->g_h;
}

VOID r_set(GRECT *pxywh, WORD x, WORD y, WORD w, WORD h)
{
	pxywh->g_x = x;
	pxywh->g_y = y;
	pxywh->g_w = w;
	pxywh->g_h = h;
}

	UWORD
inside(x, y, pt)		/* determine if x,y is in rectangle	*/
	WORD		x, y;
	GRECT		*pt;
	{
	if ( (x >= pt->g_x) && (y >= pt->g_y) &&
	    (x < pt->g_x + pt->g_w) && (y < pt->g_y + pt->g_h) )
		return(TRUE);
else return(FALSE);
	} /* inside */

	VOID rc_constrain(pc, pt)
	GRECT		*pc;
	GRECT		*pt;
	{
	if (pt->g_x < pc->g_x)
	    pt->g_x = pc->g_x;
	if (pt->g_y < pc->g_y)
	    pt->g_y = pc->g_y;
	if ((pt->g_x + pt->g_w) > (pc->g_x + pc->g_w))
	    pt->g_x = (pc->g_x + pc->g_w) - pt->g_w;
	if ((pt->g_y + pt->g_h) > (pc->g_y + pc->g_h))
	    pt->g_y = (pc->g_y + pc->g_h) - pt->g_h;
	}

	VOID
rc_copy(psbox, pdbox)		/* copy source to destination rectangle	*/
	GRECT	*psbox;
	GRECT	*pdbox;
	{
	pdbox->g_x = psbox->g_x;
	pdbox->g_y = psbox->g_y;
	pdbox->g_w = psbox->g_w;
	pdbox->g_h = psbox->g_h;
	}

	WORD
rc_equal(p1, p2)		/* tests for two rectangles equal	*/
	GRECT	*p1, *p2;
	{
	if ((p1->g_x != p2->g_x) ||
	    (p1->g_y != p2->g_y) ||
	    (p1->g_w != p2->g_w) ||
	    (p1->g_h != p2->g_h))
		return(FALSE);
	return(TRUE);
	}

	VOID rc_union(p1, p2)
	GRECT		*p1, *p2;
	{
	WORD		tx, ty, tw, th;

	tw = max(p1->g_x + p1->g_w, p2->g_x + p2->g_w);
	th = max(p1->g_y + p1->g_h, p2->g_y + p2->g_h);
	tx = min(p1->g_x, p2->g_x);
	ty = min(p1->g_y, p2->g_y);
	p2->g_x = tx;
	p2->g_y = ty;
	p2->g_w = tw - tx;
	p2->g_h = th - ty;
	}

	WORD rc_intersect(p1, p2)
	GRECT	*p1, *p2;
	{
	WORD	tx, ty, tw, th;

	tw = min(p2->g_x + p2->g_w, p1->g_x + p1->g_w);
	th = min(p2->g_y + p2->g_h, p1->g_y + p1->g_h);
	tx = max(p2->g_x, p1->g_x);
	ty = max(p2->g_y, p1->g_y);
	p2->g_x = tx;
	p2->g_y = ty;
	p2->g_w = tw - tx;
	p2->g_h = th - ty;
	return( (tw > tx) && (th > ty) );
	}

WORD min(WORD a, WORD b)
{
	return( (a < b) ? a : b );
}


WORD max(WORD a, WORD b)
{
	return( (a > b) ? a : b );
}

VOID movs(WORD num, BYTE *ps, BYTE *pd)
{
	do
	  *pd++ = *ps++;
	while (--num);
}
     
VOID movup(WORD num, BYTE *ps, BYTE *pd)
{
	do
	  *pd-- = *ps--;
	while (num--);
}

BYTE toupper(BYTE c)
{
	return (BYTE) (c > 0x5f ? c & 0x5f : c);
}

	VOID strup(p1)
	BYTE	*p1;
	{
	for(; *p1; p1++)
		*p1 = toupper(*p1);
	}

WORD strlen(BYTE *p1)
{
	WORD		len;

	len = 0;
	while( *p1++ )
	  len++;

	return(len);
}

	BYTE
*strcpy(ps, pd)
	BYTE		*ps, *pd;
{
	while(*pd++ = *ps++);
	*pd = '\0';
}


	BYTE
*strcat(ps, pd)
	BYTE		*ps, *pd;
{
	while(*pd)
	  pd++;
	while(*pd++ = *ps++);
	*pd = '\0';
	return(pd);
}

WORD strcmp(BYTE *p1, BYTE *p2)
{
	while(*p1)
	{
	  if (*p1++ != *p2++)
	    return(FALSE);
	}
	if (*p2)
	  return(FALSE);
	return(TRUE);
}

	VOID LLSTRCPY(src, dest)
	LONG	src, dest;
	{
	BYTE	b;

	do {
		b = LBGET(src++);
		LBSET(dest++, b);
		} while (b);
	}

	WORD LLSTRCMP(l1, l2)
	LONG	l1, l2;
	{
	BYTE	b;

	while (b = LBGET(l1++))
		if (b != LBGET(l2++))
			return (FALSE);
	return (!LBGET(l2));
	}

	VOID LWINC(x)
	LONG	x;
	{
	LWSET(x, 1 + LWGET(x));
	}

	BYTE uhex_dig(wd)
	WORD		wd;
	{
	if ( (wd >= 0) && (wd <= 9) )
		return (BYTE) (wd + '0');	
	if ( (wd >= 0x0a) && (wd <= 0x0f) )
		return (BYTE) (wd + 'A' - 0x0a);
	return(' ');
	}

	WORD make_num(pstr)
	BYTE		*pstr;
	{
/* take the given string & return a number: 0-999			*/
	WORD		num, factor;
	BYTE		*ptmp;

	ptmp = pstr;
	num = 0;
	if (!ptmp)
	  	return(num);
					/* skip to end of number	*/
	while (*ptmp)
	  	ptmp++;
	ptmp--;
					/* pick off digits		*/
	factor = 1;
	while (ptmp >= pstr)
		{
	  	num += (*ptmp - '0') * factor;
	  	factor *= 10;
	  	ptmp--;
		} /* while */
	return(num);
	} 

	VOID merge_str(pdst, ptmp, parms)
	BYTE		*pdst;
	BYTE		*ptmp;
	UWORD		parms[];	
{
	WORD		num;
	WORD		do_value;
	BYTE		lholder[12];
	BYTE		*pnum, *psrc;
	LONG		lvalue, divten;
	WORD		digit, base;

	num = 0;
	while(*ptmp)
	{
	  if (*ptmp != '%')
	    *pdst++ = *ptmp++;
	  else
	  {
	    ptmp++;
	    do_value = FALSE;
	    switch(*ptmp++)
	    {
	      case '%':
		*pdst++ = '%';
		break;
	      case 'L':
		lvalue = *( (LONG *) &parms[num] );
		num += sizeof(LONG) / sizeof(UWORD);
		do_value = TRUE;
		base = 10;
		break;
	      case 'W':
		lvalue = (LONG) ( (WORD) parms[num++]);
		do_value = TRUE;
		base = 10;
		break;
	      case 'B':
		lvalue = (LONG) LLOBT(parms[num++]) & 0xff;
		do_value = TRUE;
		base = 10;
		break;
	      case 'S':
		psrc = (BYTE *) parms[num]; 
		num += sizeof (BYTE *) / sizeof (UWORD);
		while(*psrc)
		  *pdst++ = *psrc++;
		break;
	      case 'H':
		lvalue = (LONG) parms[num++] & 0xffff;
		do_value = TRUE;
		base = 16;
		break;
	      case 'X':
		lvalue = *( (LONG *) &parms[num] );
		num += sizeof(LONG) / sizeof(UWORD);
		do_value = TRUE;
		base = 16;
		break;
	    }
	    if (do_value)
	    {
	      pnum = &lholder[0];
	      if (lvalue < 0 && base == 10)
		{
		*pdst++ = '-';
		lvalue = -lvalue;
		}
	      while(lvalue)
	      {
		divten = lvalue / base;
		digit = (WORD) lvalue - (divten * base);
		*pnum++ = uhex_dig(digit);
		lvalue = divten;
	      }
	      if (pnum == &lholder[0])
		*pdst++ = '0';
	      else
	      {
		while(pnum != &lholder[0])
		  *pdst++ = *--pnum;
	      }
	    }
	  }
	}
	*pdst = NULL;
}

	WORD
dmcopy(fdc, msrc, mdest, mln)		/* disk to memory copy */
	WORD		fdc;
	LONG		msrc;
	LONG		mdest;
	WORD		mln;
{
	dos_lseek( fdc, 0, msrc);
	if ( DOS_ERR )
	  return(FALSE);
	if ( !dos_read( fdc, mln, mdest))
	   return( FALSE );	
	if ( DOS_ERR )
	  return(FALSE);
else return(mln);

} /* dmcopy */

	LONG obj_addr(tree, obj, offset)
	LONG	tree;
	WORD	obj, offset;
	{
	return (tree + (UWORD) (obj * sizeof(OBJECT) + offset));
	}

	WORD GET_NEXT(tree, x)
	LONG	tree;
	WORD	x;
	{
	return LWGET(obj_addr(tree, x, 0));
	}

	WORD GET_HEAD(tree, x)
	LONG	tree;
	WORD	x;
	{
	return LWGET(obj_addr(tree, x, 2));
	}

	WORD GET_TAIL(tree, x)
	LONG	tree;
	WORD	x;
	{
	return LWGET(obj_addr(tree, x, 4));
	}

	WORD GET_TYPE(tree, x)
	LONG	tree;
	WORD	x;
	{
	return LWGET(obj_addr(tree, x, 6));
	}

	WORD GET_FLAGS(tree, x)
	LONG	tree;
	WORD	x;
	{
	return LWGET(obj_addr(tree, x, 8));
	}

	WORD GET_STATE(tree, x)
	LONG	tree;
	WORD	x;
	{
	return LWGET(obj_addr(tree, x, 10));
	}

	LONG GET_SPEC(tree, x)
	LONG	tree;
	WORD	x;
	{
	return LLGET(obj_addr(tree, x, 12));
	}

	WORD GET_X(tree, x)
	LONG	tree;
	WORD	x;
	{
	return LWGET(obj_addr(tree, x, 16));
	}

	WORD GET_Y(tree, x)
	LONG	tree;
	WORD	x;
	{
	return LWGET(obj_addr(tree, x, 18));
	}

	WORD GET_WIDTH(tree, x)
	LONG	tree;
	WORD	x;
	{
	return LWGET(obj_addr(tree, x, 20));
	}

	WORD GET_HEIGHT(tree, x)
	LONG	tree;
	WORD	x;
	{
	return LWGET(obj_addr(tree, x, 22));
	}

	VOID SET_NEXT(tree, x, val)
	LONG	tree;
	WORD	x, val;
	{
	LWSET(obj_addr(tree, x, 0), val);
	}

	VOID SET_HEAD(tree, x, val)
	LONG	tree;
	WORD	x, val;
	{
	LWSET(obj_addr(tree, x, 2), val);
	}

	VOID SET_TAIL(tree, x, val)
	LONG	tree;
	WORD	x, val;
	{
	LWSET(obj_addr(tree, x, 4), val);
	}

	VOID SET_TYPE(tree, x, val)
	LONG	tree;
	WORD	x, val;
	{
	LWSET(obj_addr(tree, x, 6), val);
	}

	VOID SET_FLAGS(tree, x, val)
	LONG	tree;
	WORD	x, val;
	{
	LWSET(obj_addr(tree, x, 8), val);
	}

	VOID SET_STATE(tree, x, val)
	LONG	tree;
	WORD	x, val;
	{
	LWSET(obj_addr(tree, x, 10), val);
	}

	VOID SET_SPEC(tree, x, val)
	LONG	tree, val;
	WORD	x;
	{
	LLSET(obj_addr(tree, x, 12), val);
	}

	VOID SET_X(tree, x, val)
	LONG	tree;
	WORD	x, val;
	{
	LWSET(obj_addr(tree, x, 16), val);
	}

	VOID SET_Y(tree, x, val)
	LONG	tree;
	WORD	x, val;
	{
	LWSET(obj_addr(tree, x, 18), val);
	}

	VOID SET_WIDTH(tree, x, val)
	LONG	tree;
	WORD	x, val;
	{
	LWSET(obj_addr(tree, x, 20), val);
	}

	VOID SET_HEIGHT(tree, x, val)
	LONG	tree;
	WORD	x, val;
	{
	LWSET(obj_addr(tree, x, 22), val);
	}

