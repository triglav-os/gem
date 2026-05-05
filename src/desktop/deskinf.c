/*
 * Implements desktop configuration, info, and preference dialogs. The code
 * manages user-facing metadata editing and desktop option state.
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

EXTERN WORD dos_rename(LONG poname, LONG pnname);
EXTERN WORD dos_chmod(LONG pname, WORD func, WORD attr);
EXTERN VOID	merge_str();
EXTERN WORD strlen(BYTE *p1);
EXTERN VOID bb_fill(WORD mode, WORD fis, WORD patt, WORD hx, WORD hy, WORD hw, WORD hh);
EXTERN VOID gsx_attr(UWORD text, UWORD mode, UWORD color);
EXTERN VOID gsx_tblt(WORD tb_f, WORD x, WORD y, WORD tb_nc);
EXTERN VOID gsx_gclip(GRECT *pt);
EXTERN WORD gsx_sclip(GRECT *pt);
EXTERN WORD form_center(LONG tree, WORD *pcx, WORD *pcy, WORD *pcw, WORD *pch);
EXTERN WORD form_dial(WORD dtype, WORD ix, WORD iy, WORD iw, WORD ih, WORD x, WORD y, WORD w, WORD h);
EXTERN WORD objc_draw(LONG tree, WORD drawob, WORD depth, WORD xc, WORD yc, WORD wc, WORD hc);
EXTERN WORD menu_click(WORD click, WORD setit);
EXTERN WORD form_do(LONG form, WORD start);
EXTERN WORD d_doop(WORD op, LONG tree, WORD obj, BYTE *psrc_path, BYTE *pdst_path, WORD *pfcnt, WORD *pdcnt, WORD flag);
EXTERN VOID inf_sset(LONG tree, WORD obj, BYTE *pstr);
EXTERN VOID fmt_str(BYTE *instr, BYTE *outstr);
EXTERN VOID inf_fldset(LONG tree, WORD obj, UWORD testfld, UWORD testbit, UWORD truestate, UWORD falsestate);
EXTERN WORD inf_what(LONG tree, WORD ok, WORD cncl);
EXTERN WORD graf_mouse(WORD m_number, LONG m_addr);
EXTERN VOID inf_sget(LONG tree, WORD obj, BYTE *pstr);
EXTERN VOID fmt_str(BYTE *instr, BYTE *outstr);
EXTERN WORD strcmp(BYTE *p1, BYTE *p2);
EXTERN WORD d_errmsg(VOID);
EXTERN VOID dos_space(WORD drv, LONG *ptotal, LONG *pavail);
EXTERN VOID dos_label(WORD drive, BYTE *plabel);
EXTERN WORD evnt_dclick(WORD rate, WORD setit);
EXTERN WORD sound(WORD isfreq, WORD freq, WORD dura);
EXTERN WORD inf_gindex(LONG tree, WORD baseobj, WORD numobj);

EXTERN BYTE	gl_amstr[];
EXTERN BYTE	gl_pmstr[];

EXTERN LONG	ad_intin;

EXTERN GLOBES	G;

#define SFCB struct sfcb
SFCB
{
	BYTE		sfcb_junk;
	BYTE		sfcb_attr;
	WORD		sfcb_time;
	WORD		sfcb_date;
	LONG		sfcb_size;
	BYTE		sfcb_name[13];
};

/* ----- my_itoa ----------------------------------------------------- */
VOID my_itoa(UWORD number, BYTE *pnumstr)
{
	WORD		ii;

	for (ii = 0; ii < 2; pnumstr[ii++] = '0');
	pnumstr[2] = NULL;
	if (number > 9)
	  merge_str(pnumstr, "%W", &number);
else merge_str(pnumstr+1, "%W", &number);
} /* my_itoa */

/*
*	Routine to format DOS style time.
*
*	15 14 13 12 11 10 9 8 7 6 5 4 3 2 1 0
*	<     hh     > <    mm    > <   xx  >
*	hh = binary 0-23
*	mm = binary 0-59
*	xx = binary seconds \ 2 
*
*	put into this form 12:45 pm
*/
VOID fmt_time(UWORD time, BYTE *ptime)
{
	WORD		pm, val;

	val = ((time & 0xf800) >> 11) & 0x001f;
	if (G.g_ctimeform)
	{
	  if (val >= 12)
	  {
	    if (val > 12)
	      val -= 12;
	    pm = TRUE;
	  }
	  else
	  {
	    if (val == 0)
	      val = 12;
	    pm = FALSE;
	  }
	}
	my_itoa( val, &ptime[0]);
	my_itoa( ((time & 0x07e0) >> 5) & 0x003f, &ptime[2]);
	if (G.g_ctimeform)
	  strcpy((pm?&gl_pmstr[0]:&gl_amstr[0]), &ptime[4]);
else strcpy("  ", &ptime[4]);
} /* fmt_time */

/*
*	Routine to format DOS style date.
*	
*	15 14 13 12 11 10 9 8 7 6 5 4 3 2 1 0
*	<     yy          > < mm  > <  dd   >
*	yy = 0 - 119 (1980 - 2099)
*	mm = 1 - 12
*	dd = 1 - 31
*/
VOID fmt_date(UWORD date, BYTE *pdate)
{
	if (G.g_cdateform)
	{
	  my_itoa( (date & 0x01e0) >> 5, &pdate[0]);
	  my_itoa(date & 0x001f, &pdate[2]);
	}
	else
	{
	  my_itoa(date & 0x001f, &pdate[0]);
	  my_itoa( (date & 0x01e0) >> 5, &pdate[2]);
	}
	my_itoa(80 + (((date & 0xfe00) >> 9) & 0x007f), &pdate[4]);
} /* fmt_date */

WORD ob_sfcb(LONG psfcb, BYTE *pfmt)
{
	SFCB		sf;
	BYTE		*pdst, *psrc;
	BYTE		pdate_str[7], ptime_str[7], psize_str[9];
	WORD		cnt;

	BYTE LBCOPY(LONG dst, LONG src, WORD count);
	pdst = pfmt;
	psrc = &sf.sfcb_name[0];
	*pdst++ = ' ';
	*pdst++ = (sf.sfcb_attr & F_SUBDIR) ? 0x07 : ' ';
	*pdst++ = ' ';
	if (sf.sfcb_attr & F_DESKTOP)
	{
	  *pdst++ = sf.sfcb_junk;
	  *pdst++ = ':';
	  *pdst++ = ' ';
	}
	else
	{
	  while( (*psrc) &&
	         (*psrc != '.') )
	    *pdst++ = *psrc++;
	  while( (pdst - pfmt) < 12 )
	    *pdst++ = ' ';
	  if (*psrc)
	    psrc++;
	  while (*psrc)
	    *pdst++ = *psrc++;
	}
	while( (pdst - pfmt) < 16 )
	  *pdst++ = ' ';
	psrc = &psize_str[0];
	if (sf.sfcb_attr & F_SUBDIR)
	  *psrc = NULL;
else merge_str(&psize_str[0], "%L", &sf.sfcb_size);
	for(cnt = 8 - strlen(psrc); cnt--; *pdst++ = ' ');
	while (*psrc)
	  *pdst++ = *psrc++;
	*pdst++ = ' ';
	*pdst++ = ' ';
	fmt_date(sf.sfcb_date, &pdate_str[0]);
	psrc = &pdate_str[0];
	for(cnt = 3; cnt--; )
	{
	  *pdst++ = *psrc++;
	  *pdst++ = *psrc++;
	  if (cnt)
	    *pdst++ = '-';
	}
	*pdst++ = ' ';
	*pdst++ = ' ';
	fmt_time(sf.sfcb_time, &ptime_str[0]);
	psrc = &ptime_str[0];
	for(cnt = 2; cnt--; )
	{
	  *pdst++ = *psrc++;
	  *pdst++ = *psrc++;
	  if (cnt)
	    *pdst++ = ':';
	}
	*pdst++ = ' ';
	strcpy(&ptime_str[4], pdst);
	pdst += 3;
	return(pdst - pfmt);
}	

WORD dr_fnode(UWORD last_state, UWORD curr_state, WORD x, WORD y, WORD w, WORD h, LONG psfcb)
{
	WORD		len;

	if ((last_state ^ curr_state) & SELECTED)
	  bb_fill(MD_XOR, FIS_SOLID, IP_SOLID, x, y, w, h);
	else
	{
 	  len = ob_sfcb(psfcb, &G.g_tmppth[0]);
	  gsx_attr(TRUE, MD_REPLACE, BLACK);
	  WORD LBWMOV(LONG dst, LONG src);
	  gsx_tblt(IBM, x, y, len);
	  gsx_attr(FALSE, MD_XOR, BLACK);
	}
	return(curr_state);
}

WORD dr_code(LONG pparms)
{
	PARMBLK		pb;
	GRECT		oc;
	WORD		state;

	BYTE LBCOPY(LONG dst, LONG src, WORD count);
	gsx_gclip(&oc);
	gsx_sclip(&pb.pb_xc);
	state = dr_fnode(pb.pb_prevstate, pb.pb_currstate,
			 pb.pb_x, pb.pb_y, pb.pb_w, pb.pb_h, pb.pb_parm);
	gsx_sclip(&oc);
	return(state);
}

/*
*	Put up dialog box & call form_do.
*/
WORD inf_show(LONG tree, WORD start)
{
	WORD		xd, yd, wd, hd;

	form_center(tree, &xd, &yd, &wd, &hd);
	form_dial(FMD_START, 0, 0, 0, 0, xd, yd, wd, hd);
	objc_draw(tree, ROOT, MAX_DEPTH, xd, yd, wd, hd);
	form_do(tree, start);
	form_dial(FMD_FINISH, 0, 0, 0, 0, xd, yd, wd, hd);
	return(TRUE);
}

/*
*	Routine for finishing off a simple ok-only dialog box
*/
VOID inf_finish(LONG tree, WORD dl_ok)
{
	inf_show(tree, 0);
	LWSET(OB_STATE(dl_ok), NORMAL);
}

/*
*	Routine to get number of files and folders and stuff them in
*	a dialog box.
*/
WORD inf_fifo(LONG tree, WORD dl_fi, WORD dl_fo, BYTE *ppath)
{
	WORD		junk, more;
	BYTE		nf_str[6], nd_str[6];

	G.g_nfiles = 0x0L;
	G.g_ndirs = 0x0L;
	G.g_size = 0x0L;
	more = d_doop(OP_COUNT, 0x0L, 0, ppath, ppath, &junk, &junk, 0);
	if (!more)
	  return(FALSE);
	G.g_ndirs--;

	merge_str(&nf_str[0], "%L", &G.g_nfiles);
	inf_sset(tree, dl_fi, &nf_str[0]);

	merge_str(&nd_str[0], "%L", &G.g_ndirs);
	inf_sset(tree, dl_fo, &nd_str[0]);
	return(TRUE);
}

VOID inf_dttmsz(LONG tree, FNODE *pf, WORD dl_dt, WORD dl_tm, WORD dl_sz, LONG *psize)
{
	BYTE		psize_str[9], ptime_str[7], pdate_str[7];

	fmt_date(pf->f_date, &pdate_str[0]);
	inf_sset(tree, dl_dt, &pdate_str[0]);

	fmt_time(pf->f_time, &ptime_str[0]);
	inf_sset(tree, dl_tm, &ptime_str[0]);

	merge_str(&psize_str[0], "%L", psize);
	inf_sset(tree, dl_sz, &psize_str[0]);
}

/* ----- inf_file ---------------------------------------------------- */
WORD inf_file(BYTE *ppath, FNODE *pfnode)
{
	LONG		tree;
	WORD		attr, more, nmidx;
	BYTE		poname[13], pnname[13];

	tree = G.a_trees[ADFILEIN];

	strcpy(ppath, &G.g_srcpth[0]);
	strcpy(ppath, &G.g_dstpth[0]);
	nmidx = 0;
	while (G.g_srcpth[nmidx] != '*')
	  nmidx++;

	fmt_str(&pfnode->f_name[0], &poname[0]);

	inf_sset(tree, FINAME, &poname[0]);

	inf_dttmsz(tree, pfnode, FIDATE, FITIME, FISIZE, &pfnode->f_size);

	inf_fldset(tree, FIRONLY, pfnode->f_attr, F_RDONLY,SELECTED, NORMAL);
	inf_fldset(tree, FIRWRITE, pfnode->f_attr, F_RDONLY,NORMAL,SELECTED);

	inf_show(tree, 0);
					/* now find out what happened	*/
						/* was it OK or CANCEL?	*/
	if ( inf_what(tree, FIOK, FICNCL) )
	{
	  graf_mouse(HGLASS, 0x0L);

	  more = TRUE;
	  inf_sget(tree, FINAME, &pnname[0]);
	  				/* unformat the strings		*/
	  unfmt_str(&poname[0], &G.g_srcpth[nmidx]);
	  unfmt_str(&pnname[0], &G.g_dstpth[nmidx]);
						/* do the DOS rename	*/
	  if ( !strcmp(&G.g_srcpth[nmidx], &G.g_dstpth[nmidx]) )
	  {
	    dos_rename(ADDR(&G.g_srcpth[0]), ADDR(&G.g_dstpth[0]));
	    if ( (more = d_errmsg()) != 0 )
	      strcpy(&G.g_dstpth[nmidx], &pfnode->f_name[0]);
	  } /* if */
					/* update the attributes	*/
	  attr = pfnode->f_attr;
	  if (LWGET(OB_STATE(FIRONLY)) & SELECTED)
	    attr |= F_RDONLY;
	  else
	    attr &= ~F_RDONLY;
	  if ( (BYTE) attr != pfnode->f_attr )
	  {
	    dos_chmod(ADDR(&G.g_dstpth[0]), F_SETMOD, attr);
	    if ( (more = d_errmsg()) != 0 )
	      pfnode->f_attr = attr;
	  }
	  graf_mouse(ARROW, 0x0L);
	  return(more);
	}
else return(FALSE);
} /* inf_file */

/* ----- inf_folder -------------------------------------------------- */
WORD inf_folder(BYTE *ppath, FNODE *pf)
{
	LONG		tree;
	WORD		more;
	BYTE		*pname, fname[13];

	graf_mouse(HGLASS, 0x0L);	

	tree = G.a_trees[ADFOLDIN];

	strcpy(ppath, &G.g_srcpth[0]);
	pname = &G.g_srcpth[0];
	while (*pname != '*')
	  pname++;
	pname = strcpy(&pf->f_name[0], pname);
	strcpy("\\*.*", pname-1);

	more = inf_fifo(tree, FOLNFILE, FOLNFOLD, &G.g_srcpth[0]);

	graf_mouse(ARROW, 0x0L);
	if (more)
	{
	  fmt_str(&pf->f_name[0], &fname[0]);
	  inf_sset(tree, FOLNAME, &fname[0]);

	  inf_dttmsz(tree, pf, FOLDATE, FOLTIME, FOLSIZE, &G.g_size);
	  inf_finish(tree, FOLOK);
	}
	return(TRUE);
} /* inf_folder */

/* ----- inf_disk ---------------------------------------------------- */
WORD inf_disk(BYTE dr_id)
{
	LONG		tree;
	LONG		total, avail;
	WORD		more;
	BYTE		puse_str[9], pav_str[9], plab_str[12];
	BYTE		drive[2];
	
	graf_mouse(HGLASS, 0x0L);
	tree = G.a_trees[ADDISKIN];

	drive[0] = dr_id;
	drive[1] = NULL;
	G.g_srcpth[0] = drive[0];
	strcpy(":\\*.*", &G.g_srcpth[1]);

	more = inf_fifo(tree, DINFILES, DINFOLDS, &G.g_srcpth[0]);

	graf_mouse(ARROW, 0x0L);
	if (more)
	{
	  dos_space(dr_id - 'A' + 1, &total, &avail);
	  dos_label(dr_id - 'A' + 1, &plab_str[0]);

	  inf_sset(tree, DIDRIVE, &drive[0]);
	  inf_sset(tree, DIVOLUME, &plab_str[0]);

	  merge_str(&puse_str[0], "%L", &G.g_size);
	  inf_sset(tree, DIUSED, &puse_str[0]);
	  
	  merge_str(&pav_str[0], "%L", &avail);
	  inf_sset(tree, DIAVAIL, &pav_str[0]);

	  inf_finish(tree, DIOK);
	}
	return(TRUE);
} /* inf_disk */

/*
*	Set preferences dialog.
*/
WORD inf_pref(VOID)
{
	LONG		tree;
	WORD		cyes, cno, i;
	WORD		sndefpref;
	WORD		rbld;
	
	tree = G.a_trees[ADSETPRE];
	rbld = FALSE;

	cyes = (G.g_cdelepref) ? SELECTED : NORMAL;
	cno = (G.g_cdelepref) ? NORMAL : SELECTED;
	LWSET(OB_STATE(SPCDYES), cyes);
	LWSET(OB_STATE(SPCDNO), cno);

	cyes = (G.g_ccopypref) ? SELECTED : NORMAL;
	cno = (G.g_ccopypref) ? NORMAL : SELECTED;
	LWSET(OB_STATE(SPCCYES), cyes);
	LWSET(OB_STATE(SPCCNO), cno);

	cyes = (G.g_covwrpref) ? SELECTED : NORMAL;
	cno = (G.g_covwrpref) ? NORMAL : SELECTED;
	LWSET(OB_STATE(SPCOWYES), cyes);
	LWSET(OB_STATE(SPCOWNO), cno);

	cyes = (G.g_cmclkpref) ? SELECTED : NORMAL;
	cno = (G.g_cmclkpref) ? NORMAL : SELECTED;
	LWSET(OB_STATE(SPMNCLKY), cyes);
	LWSET(OB_STATE(SPMNCLKN), cno);

	cyes = (G.g_ctimeform) ? SELECTED : NORMAL;
	cno = (G.g_ctimeform) ? NORMAL : SELECTED;
	LWSET(OB_STATE(SPTF12HR), cyes);
	LWSET(OB_STATE(SPTF24HR), cno);

	cyes = (G.g_cdateform) ? SELECTED : NORMAL;
	cno = (G.g_cdateform) ? NORMAL : SELECTED;
	LWSET(OB_STATE(SPDFMMDD), cyes);
	LWSET(OB_STATE(SPDFDDMM), cno);

	for(i=0; i<5; i++)
	  LWSET(OB_STATE(SPDC1+i), NORMAL);

	WORD evnt_dclick(WORD rate, WORD setit);
	LWSET(OB_STATE(SPDC1+G.g_cdclkpref), SELECTED);

	sndefpref = !sound(FALSE, 0xFFFF, 0);

	cyes = (sndefpref) ? SELECTED : NORMAL;
	cno = (sndefpref) ? NORMAL : SELECTED;
	LWSET(OB_STATE(SPSEYES), cyes);
	LWSET(OB_STATE(SPSENO), cno);

	inf_show(tree, 0);

	if ( inf_what(tree, SPOK, SPCNCL) )
	{
	  sndefpref = inf_what(tree, SPSEYES, SPSENO);
	  G.g_cdelepref = inf_what(tree, SPCDYES, SPCDNO);
	  G.g_ccopypref = inf_what(tree, SPCCYES, SPCCNO);
	  G.g_covwrpref = inf_what(tree, SPCOWYES, SPCOWNO);
	  G.g_cmclkpref = inf_what(tree, SPMNCLKY, SPMNCLKN);
	  G.g_cdclkpref = inf_gindex(tree, SPDC1, 5);
	  menu_click(G.g_cmclkpref, TRUE);
	  evnt_dclick(G.g_cdclkpref, TRUE);
					/* changes if file display? */
	  cyes = inf_what(tree, SPTF12HR, SPTF24HR);
	  if (G.g_ctimeform != cyes)
	  {
	    rbld = (G.g_iview == V_TEXT);
	    G.g_ctimeform = cyes;
	  }
	  cyes = inf_what(tree, SPDFMMDD, SPDFDDMM);
	  if (G.g_cdateform != cyes)
	  {
	    rbld |= (G.g_iview == V_TEXT);
	    G.g_cdateform = cyes;
	  }
	  sound(FALSE, !sndefpref, 0);
	}
	return(rbld);
} /* inf_pref */

/*
*	Open application icon
*/
WORD opn_appl(BYTE *papname, BYTE *papparms, BYTE *pcmd, BYTE *ptail)
{
	LONG		tree;
	BYTE		poname[13];

	tree = G.a_trees[ADOPENAP];

	fmt_str(papname, &poname[0]);

	inf_sset(tree, APPLNAME, &poname[0]);

	inf_sset(tree, APPLPARM, papparms);

	inf_show(tree, APPLPARM);
					/* now find out what happened	*/
	if ( inf_what(tree, APPLOK, APPLCNCL) )
	{
	  inf_sget(tree, APPLNAME, &poname[0]);
	  unfmt_str(&poname[0], pcmd);
	  inf_sget(tree, APPLPARM, ptail);
	  return(TRUE);
	}
else return(FALSE);
}
