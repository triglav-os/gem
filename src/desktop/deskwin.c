/*
 * Implements desktop window creation, redraw, scrolling, and selection
 * behavior. The code manages how folders and views appear inside GEM
 * windows.
 *
 * MIT License (see: LICENSE)
 * Copyright (C) 2026 tomaz stih
 */

#include <portab.h>
#include <machine.h>
#include <obdefs.h>
#include <deskapp.h>
#include <deskfpd.h>
#include <deskwin.h>
#include <dos.h>
#include <infodef.h>
#include <desktop.h>
#include <gembind.h>
#include <deskbind.h>

#define LEN_FNODE 45

#define MIN_HINT 2

#define SPACE 0x20
/* ----- Defined in simposif.a86. ----------------------------------- */
EXTERN LONG	drawaddr;
/* ----- Defined in deskapp.c. -------------------------------------- */
EXTERN ANODE	*app_afind();
/* ----- Defined in deskobj.c. -------------------------------------- */
EXTERN VOID obj_init(VOID);
EXTERN WORD obj_walloc(WORD x, WORD y, WORD w, WORD h);
EXTERN VOID obj_wfree(WORD obj, WORD x, WORD y, WORD w, WORD h);
EXTERN WORD obj_ialloc(WORD wparent, WORD x, WORD y, WORD w, WORD h);
/* ----- Defined in deskfpd.c. -------------------------------------- */
EXTERN FNODE	*pn_sort();
EXTERN WORD wind_create(UWORD kind, WORD wx, WORD wy, WORD ww, WORD wh);
EXTERN WORD wind_set(WORD handle, WORD field, WORD w1, WORD w2, WORD w3, WORD w4);
EXTERN WORD wind_delete(WORD handle);
EXTERN WORD objc_order(LONG tree, WORD mov_obj, WORD newpos);
EXTERN WORD min(WORD a, WORD b);
EXTERN VOID movs(WORD num, VOID *ps, VOID *pd);
EXTERN WORD mul_div(WORD a, WORD b, WORD c);
EXTERN WORD wind_get(WORD w_handle, WORD w_field, WORD *pw1, WORD *pw2, WORD *pw3, WORD *pw4);
EXTERN WORD max(WORD a, WORD b);
EXTERN WORD rc_copy(GRECT *src, GRECT *dst);
EXTERN WORD rc_intersect(GRECT *p1, GRECT *p2);
EXTERN WORD rc_equal(GRECT *a, GRECT *b);
EXTERN WORD gsx_sclip(GRECT *pt);
EXTERN VOID bb_screen(WORD scrule, WORD scsx, WORD scsy, WORD scdx, WORD scdy, WORD scw, WORD sch);
EXTERN WORD do_wredraw(WORD w_handle, WORD xc, WORD yc, WORD wc, WORD hc);
EXTERN VOID fun_msg(WORD type, WORD w3, WORD w4, WORD w5, WORD w6, WORD w7);

EXTERN BYTE	*ini_str();

EXTERN WORD		gl_wchar;
EXTERN WORD		gl_hchar;
EXTERN WORD		gl_height;
EXTERN GRECT		gl_rfull;
/* Local behavior fixes kept from the imported code. */
EXTERN ICONBLK	gl_icons[];
EXTERN WORD		gl_whsiztop;

EXTERN GLOBES		G;

VOID win_view(WORD vtype, WORD isort)
{
	G.g_iview = vtype;
	G.g_isort = isort;
	
	switch(vtype)
	{
	  case V_TEXT:
		G.g_iwext = LEN_FNODE * gl_wchar;
		G.g_ihext = gl_hchar;
		G.g_iwint = (2 * gl_wchar) - 1;
		G.g_ihint = 2;
		G.g_num = G.g_nmtext;
		G.g_pxy = &G.g_xytext[0]; 
		break;
	  case V_ICON:
		G.g_iwext = G.g_wicon;
		G.g_ihext = G.g_hicon;
		G.g_iwint = (gl_height <= 300) ? 0 : 8;
		G.g_ihint = MIN_HINT;
		G.g_num = G.g_nmicon;
		G.g_pxy = &G.g_xyicon[0]; 
		break;
	}
	G.g_iwspc = G.g_iwext + G.g_iwint;
	G.g_ihspc = G.g_ihext + G.g_ihint;
	G.g_incol = (G.g_wfull - gl_wchar) / G.g_iwspc;
}

/*
*	Start up by initializing global variables
*/
VOID win_start(VOID)
{
	WNODE		*pw;
	WORD		i;

	obj_init();
	win_view(V_ICON, S_NAME);	

	for(i=0; i<NUM_WNODES; i++)
	{
	  pw = &G.g_wlist[i];
	  pw->w_id = 0;
	}
	G.g_wcnt = 0x0;
}

/*
*	Free a window node.
*/
VOID win_free(WNODE *thewin)
{
	if (thewin->w_id != -1)
	  wind_delete(thewin->w_id);

	G.g_wcnt--;
	thewin->w_id = 0;
	objc_order(G.a_screen, thewin->w_root, 1);
	obj_wfree( thewin->w_root, 0, 0, 0, 0 );
}

/*
*	Allocate a window for use as a folder window
*/
WNODE *win_alloc()
{
	WNODE		*pw;
	WORD		wob;
	GRECT		*pt;

	if (G.g_wcnt == NUM_WNODES)
	  return((WNODE *) NULL);

	pt = (GRECT *) &G.g_cnxsave.win_save[G.g_wcnt].x_save;
	G.g_wcnt++;

	wob = obj_walloc(pt->g_x, pt->g_y, pt->g_w, pt->g_h);
	if (wob)
	{
	  pw = &G.g_wlist[wob-2];
	  pw->w_flags = 0x0;
	  pw->w_root = wob;
	  pw->w_cvcol = 0x0;
	  pw->w_cvrow = 0x0;
	  pw->w_obid = 0;
	  pw->w_pncol = (pt->g_w  - gl_wchar) / G.g_iwspc;
	  pw->w_pnrow = (pt->g_h - gl_hchar) / G.g_ihspc;
	  pw->w_vncol = 0x0;
	  pw->w_vnrow = 0x0;
/* Local behavior fixes kept from the imported code. */
					/* 0x1000 = hot closer button	*/
	  pw->w_id = wind_create(0x11C7, G.g_xfull, G.g_yfull, 
	  			 G.g_wfull, G.g_hfull);
	  if (pw->w_id != -1)
	  {
	    wind_set(pw->w_id, WF_TATTRB, WA_SUBWIN, 0, 0, 0);
	    return(pw);
	  }
else win_free(pw);
	}
	return(0);
}

/*
*	Find the WNODE that has this id.
*/
WNODE *win_find(WORD wh)
{
	WORD		ii;

	for(ii = 0; ii < NUM_WNODES; ii++)
	{
	  if ( G.g_wlist[ii].w_id == wh )
	    return(&G.g_wlist[ii]);
	}
	return(0);
}

/*
*	Bring a window node to the top of the window list.
*/
VOID win_top(WNODE *thewin)
{
	objc_order(G.a_screen, thewin->w_root, NIL);
}	

/*
*	Find out if the window node on top has size, if it does then it
*	is the currently active window.  If not, then no window is on
*	top and return 0.
*/

/*
WNODE *win_ontop()
{
	WORD		wob;

	wob = G.g_screen[ROOT].ob_tail;
	if (G.g_screen[wob].ob_width && G.g_screen[wob].ob_height)
	  return(&G.g_wlist[wob-2]);
else return(0);
}	
*/

/*
*	Find the window node that is the ith from the bottom.  Where
*	0 is the bottom (desktop surface) and 1-4 are windows.
*/
WORD win_cnt(WORD level)
{
	WORD		wob;
						/* skip over desktop	*/
						/*   surface and count	*/
						/*   windows		*/
	wob = G.g_screen[ROOT].ob_head;
	while(level--)
	  wob = G.g_screen[wob].ob_next;
	return(wob-2);
}

/*
*	Find the window node that is the ith from the bottom.  Where
*	0 is the bottom (desktop surface) and 1-4 are windows.
*/
WNODE *win_ith(WORD level)
{
	return(&G.g_wlist[win_cnt(level)]);
}

/*
*	Calculate a bunch of parameters related to how many file objects
*	will fit in a full-screen window.
*/
VOID win_ocalc(WNODE *pwin, WORD wfit, WORD hfit, FNODE **ppstart)
{
	FNODE		*pf;
	WORD		start, cnt, w_space;
						/* zero out obid ptrs	*/
						/*   in flist and count	*/
						/*   up # of files in	*/
						/*   virtual file space	*/
	cnt = 0;
	for( pf=pwin->w_path->p_flist; pf; pf=pf->f_next)
	{
	  pf->f_obid = NIL;
	  cnt++;
	}
						/* set windows virtual	*/
						/*   number of rows and	*/
						/*   columns		*/
	pwin->w_vncol = (cnt < G.g_incol) ? cnt : G.g_incol;
	pwin->w_vnrow = cnt / G.g_incol;
	if (cnt % G.g_incol)
	  pwin->w_vnrow += 1;
	if (!pwin->w_vnrow)
	  pwin->w_vnrow++;
	if (!pwin->w_vncol)
	  pwin->w_vncol++;
						/* backup cvrow & cvcol	*/
						/*   to account for	*/
						/*   more space in wind.*/
	if (!wfit)
	  wfit++;
	w_space = pwin->w_pncol = min(wfit, pwin->w_vncol);
	while( (pwin->w_vncol - pwin->w_cvcol) < w_space )
	  pwin->w_cvcol--;
	if (!hfit)
	  hfit++;
	w_space = pwin->w_pnrow = min(hfit, pwin->w_vnrow);
	while( (pwin->w_vnrow - pwin->w_cvrow) < w_space )
	  pwin->w_cvrow--;
						/* based on windows	*/
						/*   current virtual	*/
						/*   upper left row &	*/
						/*   column calculate	*/
						/*   the start and stop	*/
						/*   files		*/
	start = (pwin->w_cvrow * G.g_incol) + pwin->w_cvcol;
	pf = pwin->w_path->p_flist;
	while ( (start--) && pf)
	  pf = pf->f_next;
	*ppstart = pf;
}

/*
*	Calculate a bunch of parameters dealing with a particular
*	icon.
*/
VOID win_icalc(FNODE *pfnode)
{
	if (pfnode->f_attr & F_DESKTOP)
	  return;

	if (pfnode->f_attr & F_SUBDIR)
	  pfnode->f_pa = app_afind(FALSE, AT_ISFOLD, -1, 
				&pfnode->f_name[0], &pfnode->f_isap);
	else
	  pfnode->f_pa = app_afind(FALSE, AT_ISFILE, -1, 
				&pfnode->f_name[0], &pfnode->f_isap);
}

/*
*	Build an object tree of the list of files that are currently
*	viewable in a full-screen virtual window.  Next adjust root of
*	tree to take into account the current view of the full-screen
*	through a window on the physical display.
*/
VOID win_bldview(WNODE *pwin, WORD x, WORD y, WORD w, WORD h)
{
	FNODE		*pstart;
	WORD		obid, skipcnt;
	WORD		r_cnt, c_cnt;
	WORD		o_wfit, o_hfit;		/* object grid		*/
	WORD		i_index, i;
	WORD		xoff, yoff, wh, sl_size, sl_value;
						/* free all this windows*/
						/*   kids and set size	*/
	obj_wfree(pwin->w_root, x, y, w, h);
						/* make pstart point at	*/
						/*   1st file in current*/
						/*   view		*/
	win_ocalc(pwin, w/G.g_iwspc, h/G.g_ihspc, &pstart);
	o_wfit = min(pwin->w_pncol + 1, pwin->w_vncol - pwin->w_cvcol);
	o_hfit = min(pwin->w_pnrow + 1, pwin->w_vnrow - pwin->w_cvrow);
	r_cnt = c_cnt = 0;
	while ( (c_cnt < o_wfit) &&
		(r_cnt < o_hfit) && 
		(pstart) )
	{
						/* calc offset		*/
	  yoff = r_cnt * G.g_ihspc;
	  xoff = c_cnt * G.g_iwspc;
						/* allocate object	*/
	  obid = obj_ialloc(pwin->w_root, xoff + G.g_iwint, yoff + G.g_ihint,
				 G.g_iwext, G.g_ihext);
	  if (!obid)
	  {
	    /* error case, no more obs */
	  }
						/* remember it		*/
	  pstart->f_obid = obid;
						/* build object		*/
	  G.g_screen[obid].ob_state = WHITEBAK | DRAW3D;
	  G.g_screen[obid].ob_flags = 0x0;
	  switch(G.g_iview)
	  {
	    case V_TEXT:
		G.g_screen[obid].ob_type = G_USERDEF;
		G.g_screen[obid].ob_spec = ADDR( &G.g_udefs[obid] );
		G.g_udefs[obid].ub_code = drawaddr;
		G.g_udefs[obid].ub_parm = ADDR( &pstart->f_junk );
	        win_icalc(pstart);
		break;
	    case V_ICON:
		G.g_screen[obid].ob_type = G_ICON;
	        win_icalc(pstart);
		i_index = (pstart->f_isap) ? pstart->f_pa->a_aicon :
					     pstart->f_pa->a_dicon;
		G.g_index[obid] = i_index;
		G.g_screen[obid].ob_spec = ADDR( &gl_icons[obid] );
		movs(sizeof(ICONBLK), &G.g_iblist[i_index], &gl_icons[obid]);
		gl_icons[obid].ib_ptext = ADDR(&pstart->f_name[0]);
		gl_icons[obid].ib_char |= (0x00ff & pstart->f_pa->a_letter);
		break;
	  }
	  pstart = pstart->f_next;
	  c_cnt++;
	  if ( c_cnt == o_wfit )
	  {
						/* next row so skip	*/
						/*   next file in virt	*/
						/*   grid		*/
	    r_cnt++;
	    c_cnt = 0;
	    skipcnt = pwin->w_vncol - o_wfit;
	    while( (skipcnt--) &&
		   (pstart) )
	      pstart = pstart->f_next;
	  }
	}
						/* set slider size &	*/
						/*   position		*/
	wh = pwin->w_id;
/* Local behavior fixes kept from the imported code. */
/*	sl_size = mul_div(pwin->w_pncol, 1000, pwin->w_vncol);
	wind_set(wh, WF_HSLSIZ, sl_size, 0, 0, 0);
	wind_get(wh, WF_HSLSIZ, &sl_size, &i, &i, &i);
	if ( pwin->w_vncol > pwin->w_pncol )
	  sl_value = mul_div(pwin->w_cvcol, 1000,
				pwin->w_vncol - pwin->w_pncol);
	else
	  sl_value = 0;
	wind_set(wh, WF_HSLIDE, sl_value, 0, 0, 0);
*/
	sl_size = mul_div(pwin->w_pnrow, 1000, pwin->w_vnrow);
	wind_set(wh, WF_VSLSIZ, sl_size, 0, 0, 0);
/*	wind_get(wh, WF_VSLSIZ, &sl_size, &i, &i, &i);   WHY??? */
	if ( pwin->w_vnrow > pwin->w_pnrow )
	  sl_value = mul_div(pwin->w_cvrow, 1000,
				pwin->w_vnrow - pwin->w_pnrow);
	else
	  sl_value = 0;
	wind_set(wh, WF_VSLIDE, sl_value, 0, 0, 0);
}

/*
*	Routine to blt the contents of a window based on a new 
*	current row or column
*/
WORD win_blt(WNODE *pw, WORD newcv)
{
	WORD		delcv, pn;
	WORD		sx, sy, dx, dy, wblt, hblt, revblt, tmp;
	GRECT		c, t;
	newcv = max(0, newcv);
	
	newcv = min(pw->w_vnrow - pw->w_pnrow, newcv);
	pn = pw->w_pnrow;
	delcv = newcv - pw->w_cvrow;
	pw->w_cvrow += delcv;
	if (!delcv)
	  return(FALSE);
	wind_get(pw->w_id, WF_WXYWH, &c.g_x, &c.g_y, &c.g_w, &c.g_h);
	win_bldview(pw, c.g_x, c.g_y, c.g_w, c.g_h);
						/* see if any part is	*/
						/*   off the screen	*/
	rc_copy(&c, &t);
	rc_intersect(&gl_rfull, &t);
	if ( rc_equal(&c, &t) )
	{
						/* blt as much as we can*/
						/*   adjust clip & draw	*/
						/*   the rest		*/
	  if ( (revblt = (delcv < 0)) != 0 )
	    delcv = -delcv;
	  if (pn > delcv)
	  {
						/* see how much there is*/
						/* pretend blt up	*/
	    sx = dx = 0;
	    sy = delcv * G.g_ihspc;
	    dy = 0;
	    wblt = c.g_w;
	    hblt = c.g_h - sy;
	    if (revblt)
	    {
	      tmp = sx;
	      sx = dx;
	      dx = tmp;
	      tmp = sy;
	      sy = dy;
	      dy = tmp;
	    }
	    gsx_sclip(&c);
	    bb_screen(S_ONLY, sx+c.g_x, sy+c.g_y, dx+c.g_x, dy+c.g_y, 
			wblt, hblt);
	    if (!revblt)
	      c.g_y += hblt;
	    c.g_h -= hblt;
	  }
	}
	do_wredraw(pw->w_id, c.g_x, c.g_y, c.g_w, c.g_h);
}

/*
*	Routine to change the current virtual row or column being viewed
*	in the upper left corner based on a new slide amount.
*/
VOID win_slide(WORD wh, WORD sl_value)
{
	WNODE		*pw;
	WORD		newcv;
	WORD		vn, pn, i, sls, sl_size;

	pw = win_find(wh);

	vn = pw->w_vnrow;
	pn = pw->w_pnrow;
	sls = WF_VSLSIZ;
	wind_get(wh, sls, &sl_size, &i, &i, &i);
	newcv = mul_div(sl_value, vn - pn, 1000);
	win_blt(pw, newcv);
}

/*
*	Routine to change the current virtual row or column being viewed
*	in the upper left corner based on a new slide amount.
*/
VOID win_arrow(WORD wh, WORD arrow_type)
{
	WNODE		*pw;
	WORD		newcv;
	pw = win_find(wh);
	switch(arrow_type)
	{
	  case WA_UPPAGE:
		newcv = pw->w_cvrow - pw->w_pnrow;
		break;
	  case WA_DNPAGE:
		newcv = pw->w_cvrow + pw->w_pnrow;
		break;
	  case WA_UPLINE:
		newcv = pw->w_cvrow - 1;
		break;
	  case WA_DNLINE:
		newcv = pw->w_cvrow + 1;
		break;
	}
	win_blt(pw, newcv);
}

/*
*	Routine to sort all existing windows again
*/
VOID win_srtall(VOID)
{
	WORD		ii;
	WORD		sortsave;





	for(ii=0; ii<NUM_WNODES; ii++)
	{
	  if ( G.g_wlist[ii].w_id != 0 )
	  {
	    sortsave = G.g_isort;
	    G.g_wlist[ii].w_cvrow = 0;		/* reset slider		*/
	    if (G.g_wlist[ii].w_path->p_spec[0] == '@')	/* disk icon	*/
	      G.g_isort = S_DISK;		/* sort by drive letter	*/
	    G.g_wlist[ii].w_path->p_flist = pn_sort(-1,
	   				G.g_wlist[ii].w_path->p_flist);
	    G.g_isort = sortsave;
	  }
	}
} /* win_srtall */

/*
*	Routine to build all existing windows again.
*/
VOID win_bdall(VOID)
{
	WORD		ii;
	WORD		wh, xc, yc, wc, hc;
	
	for (ii = 0; ii < NUM_WNODES; ii++)
	{
	  wh = G.g_wlist[ii].w_id;
	  if ( wh )
	  {
	    wind_get(wh, WF_WXYWH, &xc, &yc, &wc, &hc);
	    win_bldview(&G.g_wlist[ii], xc, yc, wc, hc);
	  }
	}
}

/*
*	Routine to draw all existing windows.
*/
VOID win_shwall(VOID)
{
	WORD		ii;
	WORD		xc, yc, wc, hc;

	WORD		justtop, wh;

	justtop = FALSE;
	for(ii = 0; ii < NUM_WNODES; ii++)
	{
	  if (( wh = G.g_wlist[ii].w_id ) != 0)	/* yes, assignment!	*/
	  {
	    if (gl_whsiztop != NIL)		/* if some wh is fulled	*/
	    {
	      if (gl_whsiztop == wh)		/*  and its this one	*/
		justtop = TRUE;			/*  just to this wh	*/
	      else				/*  else test next wh	*/
		continue;
	    }
	    wind_get(wh, WF_WXYWH, &xc, &yc, &wc, &hc);
	    fun_msg(WM_TOPPED, wh, xc, yc, wc, hc);
	    fun_msg(WM_REDRAW, wh, xc, yc, wc, hc);
	    if (justtop)
	      return;
	  } /* if */
	} /* for */
} /* win_shwall */

/*
*	Return the next icon that was selected after the current icon.
*/
WORD win_isel(OBJECT olist[], WORD root, WORD curr)
{
	if (!curr)
	  curr = olist[root].ob_head;
	else
	  curr = olist[curr].ob_next;

	while(curr > root)
	{
	  if ( olist[curr].ob_state & SELECTED )
	    return(curr);
	  curr = olist[curr].ob_next;
	}
	return(0);
}

/*
*	Return pointer to this icons name.  It will always be an icon that
*	is on the desktop.
*/
BYTE *win_iname(WORD curr)
{
	ICONBLK		*pib;
	BYTE		*ptext;
	
	pib = (ICONBLK *) LPOINTER(G.g_screen[curr].ob_spec);
	ptext = (BYTE *) LPOINTER(pib->ib_ptext);
	return( ptext );
}

/*
*	Set the name and information lines of a particular window
*/
VOID win_sname(WNODE *pw)
{
	BYTE		*psrc;
	BYTE		*pdst;

	psrc = &pw->w_path->p_spec[0];
	pdst = &pw->w_name[0];
	if (*psrc != '@')
	{
	  while ( (*psrc) && (*psrc != '*') )
	    *pdst++ = *psrc++;
	}
	else
	  pdst = strcpy(ini_str(STDSKDRV), pdst);
	*pdst = NULL;
} /* win_sname */
