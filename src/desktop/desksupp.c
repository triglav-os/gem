/*
 * Implements miscellaneous desktop support routines shared across multiple
 * shell modules. The file gathers helpers that do not fit cleanly into the
 * other desktop subsystems.
 *
 * MIT License (see: LICENSE)
 * Copyright (C) 2026 tomaz stih
 */

#include <portab.h>
#include <machine.h>
#include <obdefs.h>
#include <taddr.h>
#include <dos.h>
#include <desktop.h>
#include <infodef.h>
#include <gembind.h>
#include <deskapp.h>
#include <deskfpd.h>
#include <deskwin.h>
#include <deskbind.h>
/* ----- Defined in deskfpd.c. -------------------------------------- */
EXTERN PNODE	*pn_open();
EXTERN VOID pn_close(PNODE *thepath);
EXTERN WORD pn_active(PNODE *thepath);
/* ----- Defined in deskwin.c. -------------------------------------- */
EXTERN WNODE	*win_alloc();
EXTERN WNODE	*win_find();
EXTERN VOID win_free(WNODE *thewin);
EXTERN VOID win_bldview(WNODE *pwin, WORD x, WORD y, WORD w, WORD h);
/* ----- Defined in deskact.c. -------------------------------------- */
EXTERN WORD	act_chg();
EXTERN WORD	act_allchg();
/* ----- Defined in deskinf.c. -------------------------------------- */
EXTERN WORD inf_file(BYTE *ppath, FNODE *pfnode);
/* ----- Defined in desktop.c. -------------------------------------- */
EXTERN ANODE	*i_find();

EXTERN BYTE	*ini_str();

/* ----- External routines. ----------------------------------------- */
EXTERN WORD wind_get(WORD w_handle, WORD w_field, WORD *pw1, WORD *pw2, WORD *pw3, WORD *pw4);
EXTERN WORD graf_mouse(WORD m_number, LONG m_addr);
EXTERN WORD rc_intersect(GRECT *p1, GRECT *p2);
EXTERN WORD objc_draw(LONG tree, WORD drawob, WORD depth, WORD xc, WORD yc, WORD wc, WORD hc);
EXTERN WORD max(WORD a, WORD b);
EXTERN WORD wind_open(WORD handle, WORD wx, WORD wy, WORD ww, WORD wh);
EXTERN WORD wind_close(WORD handle);
EXTERN WORD rc_equal(GRECT *a, GRECT *b);
EXTERN WORD wind_set(WORD handle, WORD field, WORD w1, WORD w2, WORD w3, WORD w4);
EXTERN WORD desk_wind_setl(WORD handle, WORD field, LONG value);
EXTERN VOID win_sname(WNODE *pw);
EXTERN VOID win_top(WNODE *thewin);
EXTERN VOID fun_msg(WORD type, WORD w3, WORD w4, WORD w5, WORD w6, WORD w7);
EXTERN WORD pro_chdir(WORD drv, BYTE *ppath);
EXTERN WORD opn_appl(BYTE *papname, BYTE *papparms, BYTE *pcmd, BYTE *ptail);
EXTERN WORD fun_alert(WORD defbut, WORD stnum, WORD pwtemp[]);
EXTERN WORD wildcmp(BYTE *pwld, BYTE *ptst);
EXTERN WORD pro_cmd(BYTE *psubcmd, BYTE *psubtail, WORD exitflag);
EXTERN WORD pro_run(WORD isgraf, WORD isover, WORD wh, WORD curr);
EXTERN WORD rsrc_gaddr(WORD rstype, WORD rsid, LONG *paddr);
EXTERN WORD form_alert(WORD defbut, LONG astring);
EXTERN VOID fpd_parse(BYTE *pspec, WORD *pdrv, BYTE *ppath, BYTE *pname, BYTE *pext);
EXTERN WORD fun_mkdir(WNODE *pw_node);
EXTERN VOID fun_rebld(WNODE *pwin);
EXTERN WORD inf_show(LONG tree, WORD start);
EXTERN WORD inf_folder(BYTE *ppath, FNODE *pf);
EXTERN WORD inf_disk(BYTE dr_id);
EXTERN WORD shel_find(LONG ppath);
EXTERN VOID takedos(void);
EXTERN VOID takekey(void);
EXTERN VOID takevid(void);
EXTERN VOID givevid(void);
EXTERN VOID givekey(void);
EXTERN VOID givedos(void);
EXTERN WORD strlen(BYTE *p1);

EXTERN WORD	DOS_ERR;
EXTERN WORD	DOS_AX;
EXTERN WORD	gl_hbox;
EXTERN WORD	gl_whsiztop;
EXTERN GRECT	gl_rfull;
EXTERN GRECT	gl_normwin;

EXTERN LONG dos_alloc(LONG nbytes);
EXTERN LONG dos_avail(void);
EXTERN WORD dos_free(LONG maddr);

EXTERN GLOBES	G;

/*
 * Clears the current selection set for one desktop surface.
 * `wh` is either `0` for the desktop root window or a live AES window
 * handle for a folder window.
 */
VOID desk_clear(WORD wh)
{
	WNODE		*pw;
	GRECT		c;

						/* get current size	*/
	wind_get(wh, WF_WXYWH, &c.g_x, &c.g_y, &c.g_w, &c.g_h);
						/* find its tree of 	*/
						/*   items		*/
	pw = win_find(wh);
	if (wh == 0)
	{
						/* clear desktop selections */
	  act_allchg(0, G.a_screen, DROOT, 0, &gl_rfull, &c,
		 SELECTED, FALSE, TRUE);
	}
	else if (pw)
	{
						/* clear all selections	*/
	  act_allchg(wh, G.a_screen, pw->w_root, 0, &gl_rfull, &c,
		 SELECTED, FALSE, TRUE);
	}
}

/*
 * Refreshes the desktop's notion of which object tree is active.
 * `wh` is the current AES window handle, with `0` meaning the desktop
 * root. When `changed` is nonzero, the folder window view is rebuilt
 * from its current rectangle before becoming active.
 */
VOID desk_verify(WORD wh, WORD changed)
{
	WNODE		*pw;
	WORD		xc, yc, wc, hc;

						/* get current size	*/
	pw = win_find(wh);
	if (pw)
	{
	  if (changed)
	  {
	    wind_get(wh, WF_WXYWH, &xc, &yc, &wc, &hc);
	    win_bldview(pw, xc, yc, wc, hc);
	  }
	  G.g_croot = pw->w_root;
	}
	else
	{
	  G.g_croot = DROOT;
	}

	G.g_cwin = wh;
	G.g_wlastsel = wh;
}

/*
 * Redraws one clipped region of the desktop or a folder window.
 * `w_handle` is `0` for the desktop root or a live folder window handle.
 * `xc`, `yc`, `wc`, and `hc` define the invalid rectangle in screen
 * coordinates. Returns `TRUE` after processing the window clip list.
 */
WORD do_wredraw(WORD w_handle, WORD xc, WORD yc, WORD wc, WORD hc)
{
	GRECT		clip_r, t;
	WNODE		*pw;
	LONG		tree;
	WORD		root;

	clip_r.g_x = xc;
	clip_r.g_y = yc;
	clip_r.g_w = wc;
	clip_r.g_h = hc;
	if (w_handle == 0)
	{
	  tree = G.a_screen;
	  root = DROOT;
	}
	else
	{
	  pw = win_find(w_handle);
	  tree = G.a_screen;
	  root = pw->w_root;
	}

	graf_mouse(M_OFF, 0x0L);

	wind_get(w_handle, WF_FIRSTXYWH, &t.g_x, &t.g_y, &t.g_w, &t.g_h);
	while ( t.g_w && t.g_h )
	{
	  if ( rc_intersect(&clip_r, &t) )
	    objc_draw(tree, root, MAX_DEPTH, t.g_x, t.g_y, t.g_w, t.g_h);
	  wind_get(w_handle, WF_NEXTXYWH, &t.g_x, &t.g_y, &t.g_w, &t.g_h);
	}
	graf_mouse(M_ON, 0x0L);
	return(TRUE);
}


/*
 * Extracts one object's rectangle from an AES object tree.
 * `olist` is the tree, `obj` is the object index, and `px`, `py`, `pw`,
 * and `ph` receive the object's position and size.
 */
VOID get_xywh(OBJECT olist[], WORD obj, WORD *px, WORD *py, WORD *pw, WORD *ph)
{
	*px = olist[obj].ob_x;
	*py = olist[obj].ob_y;
	*pw = olist[obj].ob_width;
	*ph = olist[obj].ob_height;
}

/*
 * Returns the decoded `ICONBLK` pointer stored in an object's `ob_spec`.
 * `olist` is the AES object tree and `obj` is the object index.
 */
ICONBLK * get_spec(OBJECT olist[], WORD obj)
{
	return( LPOINTER(olist[obj].ob_spec) );
}

/*
 * Aligns a proposed window origin to the desktop's placement grid.
 * `px` and `py` are updated in place to the nearest acceptable AES
 * window position.
 */
VOID do_xyfix(WORD *px, WORD *py)
{
	WORD		tx, ty, tw, th;

	wind_get(0, WF_WXYWH, &tx, &ty, &tw, &th);
	tx = *px;
	*px = (tx & 0x000f);
	if ( *px < 8 )
	  *px = tx & 0xfff0;
	else
	  *px = (tx & 0xfff0) + 16;
	*py = max(*py, ty);
}

/*
 * Opens a folder window and clears the previous object selection.
 * `new_win` is nonzero when the caller has just created `wh`.
 * `curr` is the object currently being opened.
 * `x`, `y`, `w`, and `h` define the destination rectangle.
 */
VOID do_wopen(WORD new_win, WORD wh, WORD curr, WORD x, WORD y, WORD w, WORD h)
{
	GRECT		c;

	do_xyfix(&x, &y);
	get_xywh(G.g_screen, G.g_croot, &c.g_x, &c.g_y, &c.g_w, &c.g_h);
	act_chg(G.g_cwin, G.a_screen, G.g_croot, curr, &c, SELECTED, 
			FALSE, TRUE, TRUE);
	if (new_win)
	  wind_open(wh, x, y, w, h);

	G.g_wlastsel = wh;
}

/*
 * Toggles a folder window between its previous and full-size rectangles.
 * `wh` must be a live folder window handle.
 */
VOID do_wfull(WORD wh)
{
	WORD		tmp_wh, y;
	GRECT		curr, prev, full, temp;

	gl_whsiztop = NIL;
	wind_get(wh, WF_CXYWH, &curr.g_x, &curr.g_y, &curr.g_w, &curr.g_h);
	wind_get(wh, WF_PXYWH, &prev.g_x, &prev.g_y, &prev.g_w, &prev.g_h);
	wind_get(wh, WF_FXYWH, &full.g_x, &full.g_y, &full.g_w, &full.g_h);
			/* have to check for shrinking a window that	*/
			/* was full when Desktop was first started.	*/
	if ( (rc_equal(&curr, &prev)) && (curr.g_h > gl_normwin.g_h) )
	{				/* shrink full window		*/
		/* find the other window (assuming only 2 windows)	*/
	  if ( G.g_wlist[0].w_id == wh)
	    tmp_wh = G.g_wlist[1].w_id;
	  else
	    tmp_wh = G.g_wlist[0].w_id;
	    			/* decide which window we're shrinking	*/
	  wind_get(tmp_wh, WF_CXYWH, &temp.g_x, &temp.g_y,
	  	   &temp.g_w, &temp.g_h);
	  if (temp.g_y > gl_normwin.g_y)
	    y = gl_normwin.g_y;		/* shrinking upper window	*/
	  else				/* shrinking lower window	*/
	    y = gl_normwin.g_y + gl_normwin.g_h + (gl_hbox / 2);
 	  wind_set(wh, WF_CXYWH, gl_normwin.g_x, y,
	  	   gl_normwin.g_w, gl_normwin.g_h);
	} /* if */
					/* already full, so change	*/
					/* back to previous		*/
	else if ( rc_equal(&curr, &full) )
	  wind_set(wh, WF_CXYWH, prev.g_x, prev.g_y, prev.g_w, prev.g_h);
	  				/* make it full			*/
	else
	{
	  gl_whsiztop = wh;
	  wind_set(wh, WF_SIZTOP, full.g_x, full.g_y, full.g_w, full.g_h);
	}
} /* do_wfull */

/*
 * Opens a folder path into a desktop window and rebuilds its contents.
 * `pw` is the destination window node.
 * `new_win` is nonzero when the window should be opened and topped.
 * `curr_icon` is the object being activated.
 * `drv`, `ppath`, `pname`, and `pext` describe the target path.
 * `pt` is the desired window rectangle.
 * `redraw` requests an explicit `WM_REDRAW` after reusing an existing
 * window.
 */

WORD do_diropen(WNODE *pw, WORD new_win, WORD curr_icon, WORD drv, BYTE *ppath, BYTE *pname, BYTE *pext, GRECT *pt, WORD redraw)
{
	WORD		ret;
	PNODE		*tmp;
						/* convert to hourglass	*/
	graf_mouse(HGLASS, 0x0L);
						/* open a path node	*/
	tmp = pn_open(drv, ppath, pname, pext, F_SUBDIR);
	if ( tmp == (PNODE *) 0 )
	{
	  graf_mouse(ARROW, 0x0L);
	  return(FALSE);
	}
	else
	  pw->w_path = tmp;


						/* activate path by 	*/
						/*   search and sort	*/
						/*   of directory	*/
	ret = pn_active(pw->w_path);
	if ( ret != E_NOFILES )
	{
						/* some error condition	*/
	}
						/* set new name and info*/
						/*   lines for window	*/
	win_sname(pw);
	desk_wind_setl(pw->w_id, WF_NAME, ADDR(&pw->w_name[0]));

						/* do actual wind_open	*/
	do_wopen(new_win, pw->w_id, curr_icon, 
				pt->g_x, pt->g_y, pt->g_w, pt->g_h);
	if (new_win)
	  win_top(pw);
						/* verify contents of	*/
						/*   windows object list*/
						/*   by building view	*/
						/*   and make it curr.	*/
	desk_verify(pw->w_id, TRUE);
						/* make it redraw	*/
	if (redraw && ( !new_win ))
	  fun_msg(WM_REDRAW, pw->w_id, pt->g_x, pt->g_y, pt->g_w, pt->g_h);

	graf_mouse(ARROW, 0x0L);
	return(TRUE);
} /* do_diropen */

/*
 * Launches an application or an associated document target.
 * `pa` is the application node returned by `i_find()`.
 * `isapp` is nonzero when the user selected an executable directly
 * rather than a data document.
 * `curr` is the object being opened.
 * `drv`, `ppath`, and `pname` identify the selected item on disk.
 * Returns the `pro_run()` completion flag for the hosted single-task
 * build.
 */

WORD do_aopen(ANODE *pa, WORD isapp, WORD curr, WORD drv, BYTE *ppath, BYTE *pname)
{
	WORD		ret, done;
	WORD		isgraf, isover, isparm, uninstalled;
	BYTE		*ptmp, *pcmd, *ptail;
	BYTE		name[13];

	done = FALSE;
						/* set flags		*/
	isgraf = pa->a_flags & AF_ISGRAF;
	isover = (pa->a_flags & AF_ISFMEM) ? 2 : 1;
	isparm = pa->a_flags & AF_ISPARM;
	uninstalled = ( (*pa->a_pappl == '*') || 
			(*pa->a_pappl == '?') ||
			(*pa->a_pappl == NULL) );
						/* change current dir.	*/
						/*   to selected icon's	*/
	pro_chdir(drv, ppath);
						/* see if application	*/
						/*   was selected 	*/
						/*   directly or a 	*/
						/*   data file with an	*/
						/*   associated primary	*/
						/*   application	*/
	pcmd = ptail = NULLPTR;
	G.g_cmd[0] = G.g_tail[1] = NULL;
	ret = TRUE;

	if ( (!uninstalled) && (!isapp) )
	{
						/* an installed	docum.	*/
	  pcmd = pa->a_pappl;
	  ptail = pname;
	}
	else
	{
	  if ( isapp )
	  {
						/* DOS-based app. has	*/
						/*   been selected	*/
	    if (isparm)
	    {
	      pcmd = &G.g_cmd[0];
	      ptail = &G.g_tail[1];
	      ret = opn_appl(pname, "\0", pcmd, ptail);
	    }
	    else
	      pcmd = pname;
	  } /* if isapp */
	  else
	  {
						/* DOS-based document 	*/
						/*   has been selected	*/
	    fun_alert(1, STNOAPPL, NULLPTR);
	    ret = FALSE;
	  } /* else */
	} /* else */
						/* see if it is a 	*/
						/*   batch file		*/
	if ( wildcmp( ini_str(STGEMBAT), pcmd) )
	{
						/* if is app. then copy	*/
						/*   typed in parameter	*/
						/*   tail, else it was	*/
						/*   a document installed*/
						/*   to run a batch file*/
	  strcpy( (isapp) ? &G.g_tail[1] : ptail, &G.g_1text[0]);
	  ptmp = &name[0];
	  pname = pcmd;
	  while ( *pname != '.' )
	    *ptmp++ = *pname++;
	  *ptmp = NULL;
	  ret = pro_cmd(&name[0], &G.g_1text[0], TRUE);
	  pcmd = &G.g_cmd[0];
	  ptail = &G.g_tail[1];
	} /* if */
						/* user wants to run	*/
						/*   another application*/
	if (ret)
	{
	  if ( (pcmd != &G.g_cmd[0]) &&
	       (pcmd != NULLPTR) )
	    strcpy(pcmd, &G.g_cmd[0]);
	  if ( (ptail != &G.g_tail[1])  &&
	       (ptail != NULLPTR) )
	    strcpy(ptail, &G.g_tail[1]);
	  done = pro_run(isgraf, isover, G.g_cwin, curr);
	} /* if ret */
	return(done);
} /* do_aopen */


/*
 * Opens a disk icon into a newly allocated folder window.
 * `curr` is the desktop object id for the selected disk icon.
 */

WORD do_dopen(WORD curr)
{
	WORD		drv;
	WNODE		*pw;
	ICONBLK		*pib;

	pib = (ICONBLK *) get_spec(G.g_screen, curr);
	pw = win_alloc();
	if (pw)
	{
	  drv = (0x00ff & pib->ib_char);
	  pro_chdir(drv, "");
	  if (!DOS_ERR)
 	    do_diropen(pw, TRUE, curr, drv, "", "*", "*", 
			&G.g_screen[pw->w_root].ob_x, TRUE);
else win_free(pw);
	}
	else
	{
	  rsrc_gaddr(R_STRING, STNOWIND, &G.a_alert);
	  form_alert(1, G.a_alert);
	}
	return( FALSE );
}


/*
 * Reopens an existing folder window on a new path.
 * `pw` is the target window node.
 * `curr` is the current object id, or `0` when called from `do_chkall()`.
 * `drv`, `ppath`, `pname`, and `pext` describe the target folder path.
 * `chkall` is nonzero when the caller is validating all open windows and
 * may fall back to the drive root.
 * `redraw` requests a redraw after the window contents change.
 */
VOID do_fopen(WNODE *pw, WORD curr, WORD drv, BYTE *ppath, BYTE *pname,
    BYTE *pext, WORD chkall, WORD redraw)
{
	GRECT		t;
	WORD		ok;
	BYTE		*pp, *pnew;

	
	ok = TRUE;
	pnew = ppath;
	wind_get(pw->w_id, WF_WXYWH, &t.g_x, &t.g_y, &t.g_w, &t.g_h);
	pro_chdir(drv, "");
	if (DOS_ERR)
	{
	  if ( DOS_AX == E_PATHNOTFND )
	  {
	    if (!chkall)
	      fun_alert(1, STDEEPPA, NULLPTR);
	    else
	    {
	      pro_chdir(drv, "");
	      pnew = "";
	    }
	  } /* if */
	  else
	    return;			/* error opening disk drive	*/
	} /* if DOS_ERR */
	else
	{
	  pro_chdir(drv, ppath);
	  if (DOS_ERR)
	  {
	    if ( DOS_AX == E_PATHNOTFND )
	    {				/* DOS path is too long?	*/
	      if (chkall)
	      {
	        pro_chdir(drv, "");
		pnew = "";
	      }
	      else
	      {
	        fun_alert(1, STDEEPPA, NULLPTR);
	    					/* back up one level	*/
		pp = ppath;
		while (*pp)
		  pp++;
		while(*pp != '\\')
		  pp--;
		*pp = NULL;
		pname = "*";
		pext  = "*";
		return;
	      } /* else */
	    } /* if DOS_AX */
	    else
	      return;			/* error opening disk drive	*/
	  } /* if DOS_ERR */
	} /* else */
	pn_close(pw->w_path);
	if (ok)
	{
	  ok = do_diropen(pw, FALSE, curr, drv, pnew, pname, pext, &t, redraw);
	  if ( !ok )
	  {
	    fun_alert(1, STDEEPPA, NULLPTR);
	    					/* back up one level	*/
	    pp = ppath;
	    while (*pp)
	      pp++;
	    while(*pp != '\\')
	      pp--;
	    *pp = NULL;
	    do_diropen(pw, FALSE, curr, drv, pnew, pname, pext, &t, redraw);
	  }
	}
} /* do_fopen */


/*
 * Dispatches an open action for the selected desktop object.
 * `curr` is the object id in `G.g_screen[]`.
 */

WORD do_open(WORD curr)
{
	WORD		done;
	ANODE		*pa;
	WNODE		*pw;
	FNODE		*pf;
	WORD		drv, isapp;
	BYTE		path[66], name[9], ext[4];

	done = FALSE;

	pa = i_find(G.g_cwin, curr, &pf, &isapp);
	pw = win_find(G.g_cwin);
	if ( pf )
	  fpd_parse(&pw->w_path->p_spec[0],&drv,&path[0],&name[0],&ext[0]);

	if ( pa )
	{	
	  switch( pa->a_type )
	  {
	    case AT_ISFILE:
		done = do_aopen(pa,isapp,curr,drv,&path[0],&pf->f_name[0]);
		break;
	    case AT_ISFOLD:
		if ( (pf->f_attr & F_FAKE) && pw )
		  fun_mkdir(pw);
		else
		{
		  if (path[0] != NULL)
		    strcat("\\", &path[0]);
		  if ( (strlen(&path[0]) + LEN_ZFNAME) >= (LEN_ZPATH-3) )
		    fun_alert(1, STDEEPPA, NULLPTR);
		  else
		  {
		    strcat(&pf->f_name[0], &path[0]);
		    pw->w_cvrow = 0;		/* reset slider		*/
		    do_fopen(pw, curr, drv, &path[0], &name[0],
		    	     &ext[0], FALSE, TRUE);
		  }
		}
		break;
	    case AT_ISDISK:
		drv = (0x00ff & pa->a_letter);
		path[0] = NULL;
		name[0] = ext[0] = '*';
		name[1] = ext[1] = NULL;
		do_fopen(pw, curr, drv, &path[0], &name[0], &ext[0],
					 FALSE, TRUE);
		break;
	  }
	}

	return(done);
}



/*
 * Opens the information dialog for the selected desktop object.
 * `curr` is the object id in `G.g_screen[]`.
 */

WORD do_info(WORD curr)
{
	WORD		ret, junk;
	ANODE		*pa;
	WNODE		*pw;
	FNODE		*pf;
	LONG		tree;

	pa = i_find(G.g_cwin, curr, &pf, &junk);
	pw = win_find(G.g_cwin);

	if ( pa )
	{	
	  switch( pa->a_type )
	  {
	    case AT_ISFILE:
		ret = inf_file(&pw->w_path->p_spec[0], pf);
		if (ret)
		  fun_rebld(pw);
		break;
	    case AT_ISFOLD:
		if (pf->f_attr & F_FAKE)
		{
		  tree = G.a_trees[ADNFINFO];
		  inf_show(tree, 0);
		  LWSET(OB_STATE(NFINOK), NORMAL);
		}
else inf_folder(&pw->w_path->p_spec[0], pf);
		break;
	    case AT_ISDISK:
		inf_disk( pf->f_junk );
		break;
	  }
	}
	return( FALSE );
}

/*
 * Runs the DOS formatter from a memory range that avoids the old ROM
 * BIOS PSP bug described in the original desktop sources.
 * `curr` is forwarded to `pro_run()` as the current object id.
 */

VOID romerr(WORD curr)
{
	UWORD		seg;
	LONG		testform, lavail;

	lavail = dos_avail();
	testform = dos_alloc( lavail );
	seg = testform >> 16;
	dos_free(testform);
	testform = 0x0L;
	if ( ((seg << 4) & 0xff00) >= 0xe900)
	  testform = dos_alloc( 0x00001b00L );

	pro_run(FALSE, 0, -1, curr);

	if (testform)
	  dos_free(testform);
} /* romerr */

/*
 * Invokes the disk-format workflow for the selected disk icon.
 * `curr` is the object id in `G.g_screen[]`.
 */
VOID do_format(WORD curr)
{
	WORD		junk, ret, foundit;
	WORD		*wtemp;
	BYTE		msg[6];
	ANODE		*pa;
	FNODE		*pf;

	pa = i_find(G.g_cwin, curr, &pf, &junk);

	if ( (pa) && (pa->a_type == AT_ISDISK) )
	{
	  msg[0] = pf->f_junk ;
	  msg[1] = NULL;
	  wtemp = &msg[0];
	  ret = fun_alert(2, STFORMAT, &wtemp);
	  strcpy(":", &msg[1]);
	  if (ret == 1)
	  {
	    strcpy( ini_str(STDKFRM1), &G.g_cmd[0]);
	    foundit = shel_find(G.a_cmd);
	    if (!foundit)
	    {
	      strcpy( ini_str(STDKFRM2), &G.g_cmd[0]);
	      foundit = shel_find(G.a_cmd);
	    }
	    if (foundit)
	    {
	      strcpy(&msg[0], &G.g_tail[1]);

	      takedos();
	      takekey();
	      takevid();

	      romerr(curr);
	      givevid();
	      givekey();
	      givedos();
	    } /* if */
	    else
	      fun_alert(1, STNOFRMT, NULLPTR);
	    graf_mouse(ARROW, 0x0L);	
	  } /* if ret */
	} /* if */
} /* do_format */

/*
 * Revalidates every open folder window against the current filesystem.
 * `redraw` is forwarded to `do_fopen()` for each live window.
 */
VOID do_chkall(WORD redraw)
{
	WORD		ii;
	WORD		drv;
	BYTE		path[66], name[9], ext[4];
	WNODE		*pw;
	for(ii = 0; ii < NUM_WNODES; ii++)
	{
	  pw = &G.g_wlist[ii];
	  if (pw->w_id)
	  {
	    fpd_parse(&pw->w_path->p_spec[0], &drv, &path[0],
	    	      &name[0], &ext[0]);
	    do_fopen(pw, 0, drv, &path[0], &name[0], &ext[0], TRUE, redraw);
	  }
	  else
	    desk_verify(0, TRUE);
	}
} /* do_chkall */
