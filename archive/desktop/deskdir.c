/*
 * Implements directory reading, sorting, and file-list preparation for
 * desktop windows. The routines convert filesystem contents into the
 * desktop internal node lists.
 *
 * MIT License (see: LICENSE)
 * Copyright (C) 2026 tomaz stih
 */

#include <portab.h>
#include <machine.h>
#include <obdefs.h>
#include <taddr.h>
#include <desktop.h>
#include <dos.h>
#include <deskapp.h>
#include <deskfpd.h>
#include <deskwin.h>
#include <infodef.h>
#include <gembind.h>
#include <deskbind.h>
#include <gsxdefs.h>

EXTERN VOID vr_recfl(WORD *pxyarray, WORD *pdesMFDB);
#define vr_recfl desk_vr_recfl

#define	S_FILL_STYLE		23		/* used in blank_it()	*/
#define	S_FILL_INDEX		24
#define	S_FILL_COLOR		25
#define	SET_WRITING_MODE	32
#define abs(x) ( (x) < 0 ? -(x) : (x) )
#define MAX_TWIDTH 45				/* used in blank_it()	*/

#if MULTIAPP
EXTERN LONG proc_malloc(LONG csize, LONG *adrcsize);
#else
EXTERN LONG dos_alloc(LONG nbytes);
EXTERN LONG dos_avail(void);
EXTERN WORD dos_free(LONG maddr);
#endif
EXTERN WORD dos_sfirst(LONG pspec, WORD attr);
EXTERN WORD dos_delete(LONG pname);
EXTERN WORD dos_open(LONG pname, WORD access);
EXTERN WORD dos_close(WORD handle);
EXTERN WORD dos_create(LONG pname, WORD attr);
EXTERN WORD dos_read(WORD handle, WORD cnt, LONG pbuffer);
EXTERN WORD dos_write(WORD handle, WORD cnt, LONG pbuffer);
EXTERN VOID dos_setdt(WORD handle, WORD time, WORD date);
EXTERN VOID dos_sdta(LONG ldta);
EXTERN WORD dos_rmdir(LONG ppath);
EXTERN WORD dos_mkdir(LONG ppath);
EXTERN WORD dos_snext(void);
EXTERN WORD form_center(LONG tree, WORD *pcx, WORD *pcy, WORD *pcw, WORD *pch);
EXTERN WORD form_do(LONG form, WORD start);
EXTERN WORD form_dial(WORD dtype, WORD ix, WORD iy, WORD iw, WORD ih, WORD x, WORD y, WORD w, WORD h);
EXTERN WORD form_error(WORD errnum);
EXTERN VOID fmt_str(BYTE *instr, BYTE *outstr);
EXTERN WORD fun_alert(WORD defbut, WORD stnum, WORD pwtemp[]);
EXTERN ICONBLK * get_spec(OBJECT olist[], WORD obj);
EXTERN WORD graf_mouse(WORD m_number, LONG m_addr);
EXTERN WORD graf_mbox(WORD w, WORD h, WORD srcx, WORD srcy, WORD dstx, WORD dsty);
EXTERN VOID gsx_1code(WORD code, WORD value);
EXTERN VOID gsx_fix(FDB *pfd, LONG theaddr, WORD wb, WORD h);
EXTERN VOID gsx_attr(UWORD text, UWORD mode, UWORD color);
EXTERN WORD gsx_sclip(GRECT *pt);
EXTERN VOID inf_sset(LONG tree, WORD obj, BYTE *pstr);
EXTERN WORD inf_what(LONG tree, WORD ok, WORD cncl);
EXTERN VOID inf_sget(LONG tree, WORD obj, BYTE *pstr);
EXTERN WORD inf_gindex(LONG tree, WORD baseobj, WORD numobj);
EXTERN VOID	merge_str();
EXTERN WORD objc_draw(LONG tree, WORD drawob, WORD depth, WORD xc, WORD yc, WORD wc, WORD hc);
EXTERN WORD objc_offset(LONG tree, WORD obj, WORD *poffx, WORD *poffy);
EXTERN WORD strcmp(BYTE *p1, BYTE *p2);
EXTERN VOID fmt_str(BYTE *instr, BYTE *outstr);
EXTERN VOID vr_recfl(WORD *pxyarray, WORD *pdesMFDB);
EXTERN WORD wind_get(WORD w_handle, WORD w_field, WORD *pw1, WORD *pw2, WORD *pw3, WORD *pw4);
EXTERN WNODE	*win_ith();



EXTERN WORD	DOS_AX;
EXTERN WORD	DOS_ERR;
EXTERN WORD	gl_wchar;
EXTERN WORD	gl_hchar;

EXTERN GLOBES	G;

/* Forward declarations. */
VOID blank_it(WORD obid);
VOID move_icon(WORD obj, WORD dulx, WORD duly);

MLOCAL BYTE	ml_files[4], ml_dirs[4];
MLOCAL WORD	ml_dlfi, ml_dlfo, ml_dlok, ml_dlcn;
MLOCAL WORD	ml_dlpr, ml_havebox;
MLOCAL BYTE	ml_fsrc[13], ml_fdst[13], ml_fstr[13], ml_ftmp[13];

/*
 * Draws a resource dialog tree centered on the current screen.
 * `tree` must be one of the dialog trees previously loaded into
 * `G.a_trees[]`, such as `ADCPALER`, `ADCOPYDI`, or `ADDELEDI`.
 */
VOID draw_dial(LONG tree)
{
	WORD		xd, yd, wd, hd;

	form_center(tree, &xd, &yd, &wd, &hd);
	objc_draw(tree, ROOT, MAX_DEPTH, xd, yd, wd, hd);
} /* draw_dial */

/*
 * Starts or finishes the AES grow-box animation for a dialog tree.
 * `fmd` is an AES `form_dial()` mode such as `FMD_START` or
 * `FMD_FINISH`. `tree` must be a dialog tree address from `G.a_trees[]`.
 */
VOID show_hide(WORD fmd, LONG tree)
{
	WORD		xd, yd, wd, hd;

	form_center(tree, &xd, &yd, &wd, &hd);
	form_dial(fmd, 0, 0, 0, 0, xd, yd, wd, hd);
	if (fmd == FMD_START)
	  objc_draw(tree, ROOT, MAX_DEPTH, xd, yd, wd, hd);
}

/*
 * Re-displays the copy/rename conflict dialog after the user asks to
 * review source and destination names again.
 * Uses the `ADCPALER` and `ADCOPYDI` resource trees and preserves the
 * current progress-dialog state in `ml_havebox` and `ml_dlpr`.
 */
VOID do_namecon(VOID)
{
/* Local behavior fixes kept from the imported code. */
	graf_mouse(ARROW, 0x0L);
	if (ml_havebox)
	  draw_dial(G.a_trees[ADCPALER]);
	else
	{
	  show_hide(FMD_START, G.a_trees[ADCPALER]);
	  ml_havebox = TRUE;
	}
	form_do(G.a_trees[ADCPALER], 0);
	if (ml_dlpr)
	  draw_dial(G.a_trees[ADCOPYDI]);
	graf_mouse(HRGLASS, 0x0L);
} /* do_namecon */

/*
 * Redraws one object inside a dialog tree.
 * `tree` must be a dialog tree address from `G.a_trees[]`.
 * `obj` is the object index inside that tree, for example `CDFILES`
 * or `DDFOLDS`.
 */
VOID draw_fld(LONG tree, WORD obj)
{
	GRECT		t;

	WORD LWCOPY(LONG dst, LONG src, WORD count);
	objc_offset(tree, obj, &t.g_x, &t.g_y);
	objc_draw(tree, obj, MAX_DEPTH, t.g_x, t.g_y, t.g_w, t.g_h);
} /* draw_fld */

/*
 * Returns a pointer to the backslash that precedes the trailing `*.*`
 * in a desktop search path.
 * `path` must point to a mutable path string that eventually ends in
 * `\\*.*`.
 */
BYTE *scan_slsh(BYTE *path)
{
						/* scan to first '*'	*/
	while (*path != '*')
	  path++;
						/* back up to last slash*/
	while (*path != '\\')
	  path--;
	return(path);
}


/*
 * Replaces the trailing wildcard in a search path with a child folder
 * name and appends `\\*.*` again.
 * `path` must end in `\\*.*`. `new_name` is the folder name to append.
 */
VOID add_path(BYTE *path, BYTE *new_name)
{
	while (*path != '*')
	  path++;
	strcpy(new_name, path);
	strcat("\\*.*", path);
} /* add_path */


/*
 * Removes the last folder component from a search path.
 * `path` must point to a mutable string ending in `\\*.*`; on return it
 * names the parent folder and again ends in `\\*.*`.
 */
VOID sub_path(BYTE *path)
{
						/* scan to last slash	*/
	path = scan_slsh(path);
						/* now skip to previous	*/
						/*   directroy in path	*/
	path--;
	while (*path != '\\')
	  path--;
						/* append a *.*		*/
	strcpy("\\*.*", path);
} /* sub_path */


/*
 * Replaces the trailing wildcard in a search path with a file or folder
 * name.
 * `path` must end in `\\*.*`. `new_name` is copied into the wildcard
 * position without re-appending the wildcard.
 */
VOID add_fname(BYTE *path, BYTE *new_name)
{
	while (*path != '*')
	  path++;

	strcpy(new_name, path);
} /* add_fname */



/*
 * Reports whether a folder path is already shown by an open desktop
 * window.
 * `path` must be a normalized folder path without a trailing wildcard.
 * Returns `TRUE` when a live `WNODE` already points at the same path.
 */
WORD fold_wind(BYTE *path)
{
	WORD		i;
	WNODE		*pwin;

	for(i = NUM_WNODES; i; i--)
	{
	  pwin = win_ith(i);
	  if ( (pwin->w_id) && (strcmp(&pwin->w_path->p_spec[0], path)) )
	    return(TRUE);
	}
	return(FALSE);
}


/*
 * Appends `new_name` to `path` unless the final folder already has that
 * name.
 * `path` must be a mutable desktop search path ending in `\\*.*`.
 * `new_name` is the candidate child-folder name during copy operations.
 */
VOID like_parent(BYTE *path, BYTE *new_name)
{
	BYTE		*pstart, *lastfold, *lastslsh;
						/* remember start of path*/
	pstart = path;
						/* scan to lastslsh	*/
	lastslsh = path = scan_slsh(path);
						/* back up to next to 	*/
						/*   last slash if it 	*/
						/*   exists		*/
	path--;
	while ( (*path != '\\') &&
		(path > pstart) )
	  path--;
						/* remember start of 	*/
						/*   last folder name	*/
	if (*path == '\\')
	  lastfold = path + 1;
	else
	  lastfold = 0;

	if (lastfold)
	{
	  *lastslsh = NULL;
	  if( strcmp(lastfold, new_name) )
	    return;
	  *lastslsh = '\\';
	}
	add_fname(pstart, new_name);
} /* like_parent */


/*
 * Compares a wildcard source path against a concrete destination folder.
 * `psrc` must end in `\\*.*`. `pdst` must name the destination folder
 * without the wildcard suffix. Returns the project `strcmp()` result.
 */
WORD same_fold(BYTE *psrc, BYTE *pdst)
{
	WORD		ret;
	BYTE		*lastslsh;
						/* scan to lastslsh	*/
	lastslsh = scan_slsh(psrc);
						/* null it		*/
	*lastslsh = NULL;
						/* see if they match	*/
	ret = strcmp(psrc, pdst);	
						/* restore it		*/
	*lastslsh = '\\';
						/* return if same	*/
	return( ret );
}


/*
 * Replaces the final file name in a mutable path with `\\*.*`.
 * `pstr` must point to a full file path that contains at least one
 * backslash before the final component.
 */
VOID del_fname(BYTE *pstr)
{
	while (*pstr)
	  pstr++;
	while (*pstr != '\\')
	  pstr--;
	strcpy("\\*.*", pstr);
} /* sub_path */


/*
 * Extracts the final file name from a path and formats it for dialog
 * display.
 * `pstr` is the source path. `newstr` receives the AES-formatted
 * version used in copy and rename dialogs.
 */
VOID get_fname(BYTE *pstr, BYTE *newstr)
{
	while (*pstr)
	  pstr++;
	while(*pstr != '\\')
	  pstr--;
	pstr++;
	strcpy(pstr, &ml_ftmp[0]);
	fmt_str(&ml_ftmp[0], newstr);
} /* get_fname */

/*
 * Converts the global DOS error latch into a desktop boolean result.
 * Returns `FALSE` after showing `form_error(DOS_AX)` when `DOS_ERR`
 * is set, otherwise returns `TRUE`.
 */
WORD d_errmsg(VOID)
{
	if (DOS_ERR)
	{
	  form_error(DOS_AX);
	  return(FALSE);
	}
	return(TRUE);
}


/*
 * Deletes one file through GEMDOS and reports the resulting error state.
 * `ppath` must name a single existing file.
 */
WORD d_dofdel(BYTE *ppath)
{ 
	dos_delete(ADDR(ppath));
	return( d_errmsg() );
} /* d_dofdel */


/*
 * Copies one file from source to destination, optionally prompting for
 * overwrite or rename decisions.
 * `psrc_file` and `pdst_file` are full file paths.
 * `time`, `date`, and `attr` are the source file metadata restored onto
 * the copy after the data transfer succeeds.
 * Returns `TRUE` on success or user-approved skip, `FALSE` on failure or
 * cancellation.
 */
WORD d_dofcopy(BYTE *psrc_file, BYTE *pdst_file, WORD time, WORD date, WORD attr)
{
	LONG		tree;
	WORD		srcfh, dstfh;
	UWORD		amntrd, amntwr;
	WORD		copy, cont, more, samedir, ob;
#if MULTIAPP
	LONG		lavail;
#endif

	copy = TRUE;
						/* open the source file	*/
	srcfh = dos_open(ADDR(psrc_file), 0);
	more = d_errmsg();
	if (!more)
	  return(more);
	 					/* open the dest file	*/
	cont = TRUE;
	while (cont)
	{
	  copy = FALSE;
	  more = TRUE;
	  dstfh = dos_open(ADDR(pdst_file), 0);
					     	/* handle dos error	*/
	  if (DOS_ERR)
	  {
	    if (DOS_AX == E_FILENOTFND)
	      copy = TRUE;
	    else
	      more = d_errmsg();
	    cont = FALSE;
	  }
	  else
	  {
						/* dest file already	*/
						/*   exists		*/ 
	    dos_close(dstfh);
						/* get the filenames 	*/
						/*   from the pathnames	*/
	    get_fname(psrc_file, &ml_fsrc[0]);
						/* if same dir, then	*/
						/*   don't prefill the	*/
						/*   new name string	*/
	    samedir = strcmp(psrc_file, pdst_file);
	    if (samedir)
	      ml_fdst[0] = NULL;
else get_fname(pdst_file, &ml_fdst[0]);
						/* put in filenames	*/
						/*   in dialog		*/
	    inf_sset(G.a_trees[ADCPALER], 2, &ml_fsrc[0]);
	    inf_sset(G.a_trees[ADCPALER], 3, &ml_fdst[0]);
						/* show dialog		*/
	    if ((G.g_covwrpref) || (samedir))
	    {
	      do_namecon();
						/* if okay then if its	*/
						/*   the same name then	*/
						/*   overwrite else get	*/
						/*   new name and go	*/
						/*   around to check it	*/


	      tree = G.a_trees[ADCPALER];
	      ob = inf_gindex(G.a_trees[ADCPALER], CAOK, 3) + CAOK;
	      LWSET(OB_STATE(ob), NORMAL);
	      if (ob == CASTOP)
	        copy = more = FALSE;
	      else if (ob == CACNCL)
	        copy = FALSE;
	      else
	        copy = TRUE;
	    }
	    else
	      copy = TRUE;
	    if (copy)
	    {
	      cont = FALSE;
	      inf_sget(G.a_trees[ADCPALER], 3, &ml_fdst[0]);
	      unfmt_str(&ml_fdst[0], &ml_fstr[0]);
	      if ( ml_fstr[0] == NULL )
	      {
		copy = FALSE;
	        dos_close(srcfh);
	      }
	      else
	      {
	        del_fname(pdst_file);
	        add_fname(pdst_file, &ml_fstr[0]);
	      }
	    }
	    else
	    {
	      dos_close(srcfh);
	      cont = copy = FALSE;
	    }
	  } /* else */
	} /* while cont */

	if ( copy && more )
	  dstfh = dos_create(ADDR(pdst_file), attr);
#if MULTIAPP
	LONG proc_malloc(LONG csize, LONG *adrcsize);
	if (G.g_xbuf == 0)
	  LONG proc_malloc(LONG csize, LONG *adrcsize);
	G.g_xlen = LLOWD(lavail);
#endif
	amntrd = copy;
	while( amntrd && more )
	{
	  more = d_errmsg();
	  if (more)
	  {
	    amntrd = dos_read(srcfh, G.g_xlen, G.g_xbuf);
	    more = d_errmsg();
	    if (more)
	    {
	      if (amntrd)
	      {
	        amntwr = dos_write(dstfh, amntrd, G.g_xbuf);
		more = d_errmsg();
	        if (more)
	        {
	          if (amntrd != amntwr)
	          {
						/* disk full		*/
		    graf_mouse(ARROW, 0x0L);
		    fun_alert(1, STDISKFU, NULLPTR);
		    graf_mouse(HRGLASS, 0x0L);
	            more = FALSE;
		    dos_close(srcfh);
		    dos_close(dstfh);
		    dos_delete(ADDR(pdst_file));
		  } /* if */
	        } /* if more */
	      } /* if amntrd */
	    } /* if more */
	  } /* if more */
	} /* while */
	if (copy && more)
	{
	  dos_setdt(dstfh, time, date);
	  more = d_errmsg();
	  dos_close(srcfh);
	  dos_close(dstfh);
	}
/*	graf_mouse(ARROW, 0x0L);*/
	return(more);
} /* d_dofcopy */

/*
 * Walks a directory subtree and applies one desktop operation to every
 * entry it contains.
 * `op` is one of `OP_COUNT`, `OP_DELETE`, or `OP_COPY`.
 * `tree` is either `0` for no progress dialog or one of the dialog trees
 * such as `ADCOPYDI` or `ADDELEDI`.
 * `obj` is the desktop object id whose icon should be blanked after a
 * delete when `flag` is false.
 * `psrc_path` and `pdst_path` are mutable wildcard paths ending in
 * `\\*.*`; `pdst_path` is used only for `OP_COPY`.
 * `pfcnt` and `pdcnt` point at the remaining file and folder counts shown
 * in the progress dialog.
 * `flag` suppresses blanking when nonzero, which is used while the caller
 * already owns the redraw sequence.
 */
WORD d_doop(WORD op, LONG tree, WORD obj, BYTE *psrc_path, BYTE *pdst_path, WORD *pfcnt, WORD *pdcnt, WORD flag)
{			       
	BYTE		*ptmp;
	WORD		cont, skip, more, level;
						/* start recursion at	*/
						/*   level 0		*/
	level = 0;
						/* set up initial DTA	*/
	dos_sdta(ADDR(&G.g_fcbstk[level]));
	dos_sfirst(ADDR(psrc_path), 0x16);

	cont = more = TRUE;
	while (cont && more)
	{
	  skip = FALSE;
	  if (DOS_ERR)
	  {
				  		/* no more files error	*/
	    if ( (DOS_AX == E_NOFILES) || (DOS_AX == E_FILENOTFND) )
	    {
	      switch(op)
	      {
	        case OP_COUNT:
			G.g_ndirs++;
			break;
	        case OP_DELETE:
			if (fold_wind(psrc_path))
			{
			  DOS_ERR = TRUE;	/* trying to delete	*/
			  DOS_AX = 16;		/* active directory	*/
			}
			else
			{
			  ptmp = psrc_path;
			  while(*ptmp != '*')
			    *ptmp++;
			  ptmp--;
			  *ptmp = NULL;
			  dos_rmdir(ADDR(psrc_path));
			}
			more = d_errmsg();
			strcat("\\*.*", psrc_path);
			break;
	        case OP_COPY:
			break;
	      }
	      if (tree)
	      {
	        *pdcnt -= 1;
	        merge_str(&ml_dirs[0], "%W", pdcnt);
	        inf_sset(tree, ml_dlfo, &ml_dirs[0]);
	        draw_fld(tree, ml_dlfo);
	      }
	      skip = TRUE;
	      level--;
	      if (level < 0)
	        cont = FALSE;
	      else
	      {
	        sub_path(psrc_path);
		if (op == OP_COPY)
		  sub_path(pdst_path);
	        dos_sdta(ADDR(&G.g_fcbstk[level]));
	      }
	    } /* if no more files */
	    else
	      more = d_errmsg();
	  }
	  if ( !skip && more )
	  {
	    if ( G.g_fcbstk[level].fcb_attr & F_SUBDIR )
	    {				  	/* step down 1 level	*/
	      if ( (G.g_fcbstk[level].fcb_name[0] != '.') &&
		   (level < (MAX_LEVEL-1)) )
	      {
	      					/* change path name	*/
		add_path(psrc_path, &G.g_fcbstk[level].fcb_name[0]);
		if (op == OP_COPY)
		{
		  add_fname(pdst_path, &G.g_fcbstk[level].fcb_name[0]);
		  dos_mkdir(ADDR(pdst_path));
	          if ( (DOS_ERR) && (DOS_AX != E_NOACCESS) )
		    more = d_errmsg();
		  strcat("\\*.*", pdst_path);
		}
		level++;
		dos_sdta(ADDR(&G.g_fcbstk[level]));
		if (more)
		  dos_sfirst(ADDR(psrc_path), 0x16);
	      } /* if not a dir */
	    } /* if */
	    else				 
	    {
	      if (op)
		add_fname(psrc_path, &G.g_fcbstk[level].fcb_name[0]);
	      switch(op)
	      {
		case OP_COUNT:
			G.g_nfiles++;
			G.g_size += G.g_fcbstk[level].fcb_size;
			break;
		case OP_DELETE:
			more = d_dofdel(psrc_path);
			break;
		case OP_COPY:
			add_fname(pdst_path, &G.g_fcbstk[level].fcb_name[0]);
			more = d_dofcopy(psrc_path, pdst_path,
				G.g_fcbstk[level].fcb_time, 
				G.g_fcbstk[level].fcb_date, 
				G.g_fcbstk[level].fcb_attr);
			del_fname(pdst_path);
			break;
	      }
	      if (op)
	        del_fname(psrc_path);
	      if (tree)
	      {
	        *pfcnt -= 1;
		merge_str(&ml_files[0], "%W", pfcnt);
	        inf_sset(tree, ml_dlfi, &ml_files[0]);
	        draw_fld(tree, ml_dlfi);
	      }
	    }
	  }
	  if (cont)
	    dos_snext();
	}
	if (op == OP_DELETE && !flag)
	  blank_it(obj);
	return(more);
} /* d_doop */

/*
 * Advances to the next folder component inside a mutable wildcard path.
 * `pcurr` points at the current component and `ret_path()` temporarily
 * inserts a string terminator at the next backslash before returning the
 * start of the following component.
 * The path must eventually end in `\\*.*`.
 */
BYTE *ret_path(BYTE *pcurr)
{
	REG BYTE	*path;
					/* find next level		*/
	while( (*pcurr) &&
	       (*pcurr != '\\') )
	  pcurr++;
	pcurr++;
					/* get to current position	*/
	path = pcurr;
					/* find end of curr level	*/
	while( (*path) &&
	       (*path != '\\') )
	  path++;

	*path = NULL;
	return(pcurr);
} /* ret_path */

/*
 * Rejects copy targets that would place data inside the selected source
 * subtree.
 * `psrc_path` and `pdst_path` must both end in `\\*.*`.
 * `pflist` is the selected source file list used to detect when the
 * destination already matches one of the chosen subfolders.
 * Returns `FALSE` after showing `STBADCOP` when the copy would recurse
 * into itself; otherwise returns `TRUE`.
 */
WORD par_chk(BYTE *psrc_path, FNODE *pflist, BYTE *pdst_path)
{
	REG BYTE	*tsrc, *tdst;
	WORD	same;
	REG FNODE	*pf;

	if (psrc_path[0] != pdst_path[0])		/* check drives	*/
	  return(TRUE);

	tsrc = &G.g_srcpth[0];
	tdst = &G.g_dstpth[0];
	same = TRUE;
	do
	{
							/* new copies	*/
	  strcpy(psrc_path, &G.g_srcpth[0]);
	  strcpy(pdst_path, &G.g_dstpth[0]);
							/* get next paths*/
	  tsrc = ret_path(tsrc);
	  tdst = ret_path(tdst);
	  if ( !strcmp(tsrc, "*.*") )
	  {
	    if ( !strcmp(tdst, "*.*") )
	      same = !strcmp(tdst, tsrc);
	    else
	      same = FALSE;
	  }
	  else
	  {
						/* check to same level	*/
	    if ( strcmp(tdst, "*.*") )
	      same = FALSE;
	    else
	    {
						/* walk file list	*/
	      for(pf=pflist; pf; pf=pf->f_next)
	      {
						/* exit if same subdir	*/
	        if ( (pf->f_obid != NIL) &&
		     (G.g_screen[pf->f_obid].ob_state & SELECTED) &&
		     (pf->f_attr & F_SUBDIR) &&
	             !(pf->f_attr & F_FAKE) &&
		     (strcmp(&pf->f_name[0], tdst)) )
	        {
						/* Invalid */
fun_alert(1, STBADCOP, NULLPTR);
	 	  return(FALSE);
	        }
	      }
	      same = FALSE;			/* All Ok */
}
	  }
	} while(same);
	return(TRUE);
} /* par_chk */

/*
 * Applies one desktop operation to every selected entry in a folder.
 * `op` is one of `OP_COUNT`, `OP_DELETE`, or `OP_COPY`.
 * `psrc_path` is the source wildcard path ending in `\\*.*`.
 * `pflist` is the file list whose selected entries drive the operation.
 * `pdst_path` is the destination wildcard path for `OP_COPY`.
 * `pfcnt`, `pdcnt`, and `psize` receive the accumulated file count,
 * directory count, and byte total for `OP_COUNT`, and also back the
 * progress dialog counters during delete and copy.
 * `dulx` and `duly` are the destination coordinates used by the moving
 * icon animation for non-dialog copies.
 * `from_disk` is `TRUE` when the drag started from a desktop disk icon,
 * causing `src_ob` to be animated instead of the file object's id.
 */
WORD dir_op(WORD op, BYTE *psrc_path, FNODE *pflist, BYTE *pdst_path,
    WORD *pfcnt, WORD *pdcnt, LONG *psize, WORD dulx, WORD duly,
    WORD from_disk, WORD src_ob)
{
	LONG		tree;
	FNODE		*pf;
	WORD		ret, more, obj;
	BYTE		*pglsrc, *pgldst;
#if MULTIAPP
#else
	LONG		lavail;
#endif

/* Local behavior fixes kept from the imported code. */
	graf_mouse(HGLASS, 0x0L);
	pglsrc = &G.g_srcpth[0];
	pgldst = &G.g_dstpth[0];
	tree = 0x0L;
	ml_havebox = FALSE;
	switch(op)
	{
	  case OP_COUNT:
		G.g_nfiles = 0x0L;
		G.g_ndirs = 0x0L;
		G.g_size = 0x0L;
	 	break;
	  case OP_DELETE:
	  	ml_dlpr = G.g_cdelepref;
		if (ml_dlpr)
		{
		  tree = G.a_trees[ADDELEDI];
		  ml_dlfi = DDFILES;
		  ml_dlfo = DDFOLDS;
		  ml_dlok = DDOK;
		  ml_dlcn = DDCNCL;
		}
	 	break;
	  case OP_COPY:
#if MULTIAPP
		/* do malloc elsewhere */
#else
		lavail = dos_avail();
		G.g_xlen = (lavail > 0x0000fff0L) ? 0xfff0 : LLOWD(lavail);
		G.g_xlen -= 0x0200;
		LONG dos_alloc(LONG nbytes);
#endif

		ml_dlpr = G.g_ccopypref;
		if (ml_dlpr)
		{
		  tree = G.a_trees[ADCOPYDI];
	 	  ml_dlfi = CDFILES;
		  ml_dlfo = CDFOLDS;
		  ml_dlok = CDOK;
		  ml_dlcn = CDCNCL;
		}
	 	break;
	} /* switch */

	if (tree)
	{
	  merge_str(&ml_files[0], "%W", pfcnt);
	  inf_sset(tree, ml_dlfi, &ml_files[0]);
	  merge_str(&ml_dirs[0], "%W", pdcnt);
	  inf_sset(tree, ml_dlfo, &ml_dirs[0]);
	  ml_havebox = TRUE;
	  show_hide(FMD_START, tree);
	  graf_mouse(ARROW, 0x0L);
	  form_do(tree, 0);
	  graf_mouse(HGLASS, 0x0L);
	  ret = inf_what(tree, ml_dlok, ml_dlcn);
	}
	else
	  ret = TRUE;

	more = ret;
	for (pf = pflist; pf && more; pf = pf->f_next)
	{
	  if ( (pf->f_obid != NIL) &&
	       (G.g_screen[pf->f_obid].ob_state & SELECTED) &&
	       !(pf->f_attr & F_FAKE) )
	  {
	    strcpy(psrc_path, pglsrc);
	    if (op == OP_COPY)
	    {
	      strcpy(pdst_path, pgldst);
	      if (!ml_dlpr)		/* show the moving icon!	*/
	      {
	        if (from_disk)
		  obj = src_ob;
		else
		  obj = pf->f_obid;
	        move_icon(obj, dulx, duly);
	      } /* if */
	    } /* if OP_COPY */
	    if (pf->f_attr & F_SUBDIR)
	    {			   
	      add_path(pglsrc, &pf->f_name[0]);
	      if (op == OP_COPY)
	      {
		like_parent(pgldst, &pf->f_name[0]);
		dos_mkdir(ADDR(pgldst));
		while (DOS_ERR && more)
		{
						/* see if dest folder	*/
						/*   already exists	*/ 
	    	  if (DOS_AX == E_NOACCESS)
		  {
		    if ( same_fold(pglsrc, pgldst) )
		    {
						/* get the folder name 	*/
						/*   from the pathnames	*/
		      fmt_str(&pf->f_name[0], &ml_fsrc[0]);
		      ml_fdst[0] = NULL;
						/* put in folder name	*/
						/*   in dialog		*/
		      inf_sset(G.a_trees[ADCPALER], 2, &ml_fsrc[0]);
		      inf_sset(G.a_trees[ADCPALER], 3, &ml_fdst[0]);
						/* show dialog		*/
		      do_namecon();
						/* if okay then make	*/
						/*   dir or try again	*/
						/*   until we succeed or*/
						/*   cancel is hit	*/
		      more = inf_what(G.a_trees[ADCPALER], 
					CAOK, CACNCL);

		      if (more)
		      { 
			inf_sget(G.a_trees[ADCPALER], 3, 
						&ml_fdst[0]);
			unfmt_str(&ml_fdst[0], &ml_fstr[0]);
			del_fname(pgldst);
			if (ml_fstr[0] != NULL)
			{
			  add_fname(pgldst, &ml_fstr[0]);
			  dos_mkdir(ADDR(pgldst));
		 	} /* if */
			else
			  more = FALSE;
		      } /* if more */
		    } /* if */
		    else
		      DOS_ERR = FALSE;
		  } /* if NOACCESS */
		  else
		    more = FALSE;
		} /* while */
		strcat("\\*.*", pgldst);
	      } /* if */
	      if (more)
		more = d_doop(op, tree, pf->f_obid, pglsrc, pgldst,
			      pfcnt, pdcnt, ml_dlpr);
	    } /* if SUBDIR */
	    else
	    {
	      if (op)
		add_fname(pglsrc, &pf->f_name[0]);
	      switch(op)
	      {
		    case OP_COUNT:
			G.g_nfiles++;
			G.g_size += pf->f_size;
			break;
		    case OP_DELETE:
			more = d_dofdel(pglsrc);
			if (!ml_dlpr)
			  blank_it(pf->f_obid);
			break;
		    case OP_COPY:
			add_fname(pgldst, &pf->f_name[0]);
			more = d_dofcopy(pglsrc, pgldst, pf->f_time,
					 pf->f_date, pf->f_attr);
			del_fname(pgldst);
			break;
	      }
	      if (op)
		del_fname(psrc_path);
	      if (tree)
	      {
		*pfcnt -= 1;
		merge_str(&ml_files[0], "%W", pfcnt);
		inf_sset(tree, ml_dlfi, &ml_files[0]);
		draw_fld(tree, ml_dlfi);
	      } /* if tree */
	    } /* else */
	  } /* if */
	} /* for */

	switch(op)
	{
	  case OP_COUNT:
		*pfcnt = G.g_nfiles;
		*pdcnt = G.g_ndirs;
		*psize = G.g_size;
	 	break;
	  case OP_DELETE:
	 	break;
	  case OP_COPY:
#if MULTIAPP
		/* no need to free with proc_malloc */
#else
		dos_free(G.g_xbuf);
#endif
	 	break;
	} /* switch */
	if (ml_havebox)
	  show_hide(FMD_FINISH, G.a_trees[ADCPALER]);
	graf_mouse(HGLASS, 0x0L);
	return(TRUE);
} /* dir_op */

/*
 * Clears the screen area occupied by a deleted icon without redrawing
 * the entire desktop.
 * `obid` is the object id inside `G.g_screen[]` that was just removed.
 */
VOID blank_it(WORD obid)
{
	WORD		blt_x, blt_y, blt_w, blt_h, pxy[4];
	GRECT		clipr;
	ICONBLK		*piblk;
	FDB		dst;

	graf_mouse(M_OFF, 0x0L);
	wind_get(G.g_wlastsel, WF_WXYWH, &clipr.g_x, &clipr.g_y,
		 &clipr.g_w, &clipr.g_h);
	gsx_sclip(&clipr);
	objc_offset(ADDR(&G.g_screen[0]), obid, &blt_x, &blt_y);
	if (G.g_iview == V_ICON)
	{
	  piblk = get_spec(G.g_screen, obid);
	  blt_x += piblk->ib_xtext;
	  blt_y += piblk->ib_yicon;
	  blt_w = piblk->ib_wtext;
	  blt_h = piblk->ib_hicon + piblk->ib_htext;
	} /* if V_ICON */
	else					/* view is V_TEXT	*/
	{
	  blt_w = gl_wchar * MAX_TWIDTH;
	  blt_h = gl_hchar + 1;
	} /* else */
	gsx_1code(SET_WRITING_MODE, MD_REPLACE);
	gsx_1code(S_FILL_STYLE, FIS_SOLID);
	gsx_1code(S_FILL_INDEX, IP_SOLID);
	gsx_1code(S_FILL_COLOR, WHITE);
	gsx_fix(&dst, 0x0L, 0, 0);
	pxy[0] = blt_x;
	pxy[1] = blt_y;
	pxy[2] = blt_x + blt_w - 1;
	pxy[3] = blt_y + blt_h - 1;
	vr_recfl( &pxy[0], &dst );
	gsx_1code(S_FILL_COLOR, BLACK);
	gsx_1code(SET_WRITING_MODE, MD_XOR);
	gsx_attr(FALSE, MD_XOR, BLACK);
	graf_mouse(M_ON, 0x0L);
} /* blank_it */

/*
 * Animates the outline box that follows a copied icon during drag-copy
 * style operations.
 * `obj` is the source desktop object id in `G.g_screen[]`.
 * `dulx` and `duly` are the destination upper-left coordinates.
 */
VOID move_icon(WORD obj, WORD dulx, WORD duly)
{
	WORD		sulx, suly, w, h;

	objc_offset(ADDR(&G.g_screen[0]), obj, &sulx, &suly);
	if (G.g_iview == V_ICON)
	{
	  w = G.g_wicon;
	  h = G.g_hicon;
	} /* if V_ICON */
	else					/* view must be V_TEXT	*/
	{
	  w = gl_wchar * MAX_TWIDTH;
	  h = gl_hchar;
	} /* else */
	graf_mbox(w, h, sulx, suly, dulx, duly);
} /* move_icon */
