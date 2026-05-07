/*
 * Builds desktop application records from installed programs, document
 * types, and icon metadata. The code preserves GEM desktop rules for
 * associating files with launchable apps.
 *
 * MIT License (see: LICENSE)
 * Copyright (C) 2026 tomaz stih
 */

#include <portab.h>
#include <machine.h>
#include <obdefs.h>
#include <dos.h>
#include <deskapp.h>
#include <deskfpd.h>
#include <deskwin.h>
#include <desktop.h>
#include <infodef.h>
#include <gembind.h>
#include <deskbind.h>

#include <stdio.h>

#define MIN_WINT 4
#define MIN_HINT 2
/* ----- External routines. ----------------------------------------- */
EXTERN LONG dos_alloc(LONG nbytes);
EXTERN BYTE	*ini_str();
EXTERN WORD wildcmp(BYTE *pwld, BYTE *ptst);
EXTERN WORD rsrc_gaddr(WORD rstype, WORD rsid, LONG *paddr);
EXTERN VOID gsx_trans(LONG saddr, UWORD swb, LONG daddr, UWORD dwb, UWORD h);
EXTERN WORD dos_gdrv(void);
EXTERN WORD dos_sdrv(WORD newdrv);
EXTERN WORD shel_find(LONG ppath);
EXTERN WORD dos_open(LONG pname, WORD access);
EXTERN WORD dos_create(LONG pname, WORD attr);
EXTERN WORD dos_read(WORD handle, WORD cnt, LONG pbuffer);
EXTERN VOID movs(WORD num, VOID *ps, VOID *pd);
EXTERN WORD max(WORD a, WORD b);
EXTERN WORD dos_close(WORD handle);
EXTERN WORD shel_get(LONG pbuffer, WORD len);
EXTERN WORD sound(WORD isfreq, WORD freq, WORD dura);
EXTERN VOID bfill(WORD num, WORD value, BYTE *dst);
EXTERN WORD shel_put(LONG pdata, WORD len);
EXTERN WORD fun_alert(WORD defbut, WORD stnum, WORD pwtemp[]);
EXTERN WORD dos_write(WORD handle, WORD cnt, LONG pbuffer);
EXTERN VOID obj_wfree(WORD obj, WORD x, WORD y, WORD w, WORD h);
EXTERN WORD obj_ialloc(WORD wparent, WORD x, WORD y, WORD w, WORD h);
EXTERN WORD appl_bvset(UWORD bvdisk, UWORD bvhard);

EXTERN WORD	gl_wchar;
EXTERN WORD	gl_hchar;
EXTERN WORD	gl_wschar;
EXTERN WORD	gl_hschar;
EXTERN WORD	gl_width;
EXTERN WORD	gl_height;
EXTERN WORD	gl_hbox;
EXTERN WORD	DOS_ERR;
EXTERN UWORD	global[];

EXTERN GRECT	gl_savewin[NUM_WNODES];
/* Local behavior fixes kept from the imported code. */
EXTERN ICONBLK	gl_icons[];

EXTERN GLOBES	G;

/* ----- Forward declarations. -------------------------------------- */
BYTE get_defdrv(UWORD dr_exist, UWORD dr_hard);
static VOID app_fix_positions(VOID);

#if ALCYON
GLOBAL BYTE	*gl_pstart;
#endif

#if I8086
GLOBAL WORD	gl_pstart;
#endif

GLOBAL WORD	gl_poffset;
GLOBAL WORD	gl_numics;

GLOBAL WORD	gl_stdrv;

GLOBAL BYTE	gl_afile[SIZE_AFILE];
GLOBAL BYTE	gl_buffer[SIZE_BUFF];

#if MULTIAPP
GLOBAL ACCNODE	gl_caccs[3];
#endif

/*
 * Creates simple placeholder icon and mask bitmaps for the hosted
 * build. The historical icon-file loader is still present below, but
 * the current Linux path returns early with these generated forms.
 */
static WORD app_init_placeholder_icons(void)
{
	LONG		mask_addr;
	LONG		data_addr;
	WORD		*mask_words;
	WORD		*data_words;
	WORD		i, row, word_index;
	WORD		left_word, right_word;

	mask_addr = dos_alloc(LW(128));
	data_addr = dos_alloc(LW(128));
	if ((mask_addr == 0) || (data_addr == 0))
	  return(FALSE);

	mask_words = (WORD *)(uintptr_t) mask_addr;
	data_words = (WORD *)(uintptr_t) data_addr;
	for (i = 0; i < 64; i++)
	{
	  mask_words[i] = 0xffff;
	  data_words[i] = 0x0000;
	}

	for (row = 0; row < 32; row++)
	{
	  word_index = row * 2;
	  left_word = 0x0001;
	  right_word = 0x8000;
	  if ((row == 0) || (row == 31))
	  {
	    left_word = 0xffff;
	    right_word = 0xffff;
	  }
	  data_words[word_index] |= left_word;
	  data_words[word_index + 1] |= right_word;
	}

	for (i = 0; i < NUM_IBLKS; i++)
	{
	  G.g_idlist[i].ib_pmask = mask_addr;
	  G.g_idlist[i].ib_pdata = data_addr;
	  G.g_idlist[i].ib_ptext = ADDR("");
	  G.g_idlist[i].ib_char = 0x1000;
	  G.g_idlist[i].ib_xchar = 0;
	  G.g_idlist[i].ib_ychar = 0;
	  G.g_idlist[i].ib_xicon = 0;
	  G.g_idlist[i].ib_yicon = 0;
	  G.g_idlist[i].ib_wicon = 32;
	  G.g_idlist[i].ib_hicon = 32;
	  G.g_idlist[i].ib_xtext = 0;
	  G.g_idlist[i].ib_ytext = 32;
	  G.g_idlist[i].ib_wtext = 12 * gl_wschar;
	  G.g_idlist[i].ib_htext = gl_hschar + 2;
	  G.g_iblist[i] = G.g_idlist[i];
	}

	for (i = 0; i < NUM_IBLKS * 2; i++)
	  G.g_ismask[i] = 0;

	G.a_buffstart = mask_addr;
	G.a_datastart = data_addr;
	gl_numics = 0;
	return(TRUE);
}

/*
*	Allocate an application object.
*/
ANODE *app_alloc(WORD tohead)
{
	ANODE		*pa, *ptmpa;
	FILE		*fp;

	pa = G.g_aavail;
	fp = fopen("/tmp/gem_app_alloc.log", "a");
	if (fp != NULL)
	{
	  fprintf(fp, "app_alloc tohead=%d avail=%p avail_next=%p ahead=%p\n",
	      tohead, (void *) pa,
	      (void *) ((pa != NULL) ? pa->a_next : (ANODE *) 0),
	      (void *) G.g_ahead);
	  fclose(fp);
	}
	if (pa)
	{
	  G.g_aavail = pa->a_next;
	  if ( (tohead) ||
	       (!G.g_ahead) )
	  {
	    pa->a_next = G.g_ahead;
	    G.g_ahead = pa;
	  }
	  else
	  {
	    ptmpa = G.g_ahead;
	    while( ptmpa->a_next )
	      ptmpa = ptmpa->a_next;
	    ptmpa->a_next = pa;
	    pa->a_next = (ANODE *) NULL;
	  }
	}
	return(pa);
}


/*
*	Free an application object.
*/

VOID app_free(ANODE *pa)
{
	ANODE		*ptmpa;

	if (G.g_ahead == pa)
	  G.g_ahead = pa->a_next;
	else
	{
	  ptmpa = G.g_ahead;
	  while ( (ptmpa) &&
		  (ptmpa->a_next != pa) )
	    ptmpa = ptmpa->a_next;
	  if (ptmpa)
	    ptmpa->a_next = pa->a_next;
	}
	pa->a_next = G.g_aavail;
	G.g_aavail = pa;
}


/*
*	Convert a single hex ASCII digit to a number
*/

WORD hex_dig(BYTE achar)
{
	if ( (achar >= '0') &&
	     (achar <= '9') )
	  return(achar - '0');	
	if ( (achar >= 'A') &&
	     (achar <= 'F') )
	  return(achar - 'A' + 10);
	return(0);
}
	

/*
*	Reverse of hex_dig().
*/

BYTE uhex_dig(WORD wd)
{
	if ( (wd >= 0) &&
	     (wd <= 9) )
	  return(wd + '0');	
	if ( (wd >= 0x0a) &&
	     (wd <= 0x0f) )
	  return(wd + 'A' - 0x0a);
	return(' ');
}
	

/*
*	Scan off and convert the next two hex digits and return with
*	pcurr pointing one space past the end of the four hex digits
*/

BYTE *scan_2(BYTE *pcurr, UWORD *pwd)
{
	UWORD		temp;
	
	temp = 0x0;
	temp |= hex_dig(*pcurr++) << 4;
	temp |= hex_dig(*pcurr++);
	if (temp == 0x00ff)
	  temp = NIL;
	*pwd = temp;
	return(	pcurr );
}

/*
*	Reverse of scan_2().
*/

BYTE *save_2(BYTE *pcurr, UWORD wd)
{
	*pcurr++ = uhex_dig((wd >> 4) & 0x000f);
	*pcurr++ = uhex_dig(wd & 0x000f);
	return(	pcurr );
}

#if MULTIAPP


/*
*	Scan off and convert the next four hex digits and return with
*	pcurr pointing one space past the end of the four hex digits.
*	Start of field is marked with an 'R'.  If no field, set it to
*	default memory size -- DEFMEMREQ.
*/

BYTE *scan_memsz(BYTE *pcurr, UWORD *pwd)
{
	UWORD		temp1, temp2;
	
	temp1 = 0x0;
	while (*pcurr == ' ')
	  pcurr++;
	if (*pcurr == 'R')
	{
	  pcurr++;				
	  pcurr = scan_2(pcurr, &temp1);		/* hi byte	*/
	  pcurr = scan_2(pcurr, &temp2);		/* lo byte	*/
	  temp1 = ((temp1 << 8) & 0xff00) | temp2;
	}
	if (temp1 == 0)
	  temp1 = DEFMEMREQ;
	*pwd = temp1;
	return(	pcurr );
}

/*
*	Reverse of scan_memsz().
*/

BYTE *save_memsz(BYTE *pcurr, UWORD wd)
{
	*pcurr++ = 'R';
	pcurr = save_2(pcurr, LHIBT(wd));
	pcurr = save_2(pcurr, LLOBT(wd));
	return(	pcurr );
}

#endif

/*
*	Scan off spaces until a string is encountered.  An @ denotes
*	a null string.  Copy the string into a string buffer until
*	a @ is encountered.  This denotes the end of the string.  Advance
*	pcurr past the last byte of the string.
*/

BYTE *scan_str(BYTE *pcurr, BYTE **ppstr)
{
	FILE		*fp;

	fp = fopen("/tmp/gem_scan_str.log", "a");
	if (fp != NULL)
	{
	  fprintf(fp, "scan_str pcurr='%s' pbuff=%p\n", pcurr, (void *) G.g_pbuff);
	  fclose(fp);
	}
	while(*pcurr == ' ')
	  pcurr++;
	*ppstr = G.g_pbuff;
	while(*pcurr != '@')
	  *G.g_pbuff++ = *pcurr++;
	*G.g_pbuff++ = '\0';
	pcurr++;
	return(pcurr);
}


/*
*	Reverse of scan_str.
*/

BYTE *save_str(BYTE *pcurr, BYTE *pstr)
{
	while(*pstr)
	  *pcurr++ = *pstr++;
	*pcurr++ = '@';
	*pcurr++ = ' ';
	return(pcurr);
}


/*
*	Parse a single line from the DESKTOP.INF file.
*/

BYTE *app_parse(BYTE *pcurr, ANODE *pa)
{
	switch(*pcurr)
	{
	  case 'M':				/* Storage Media	*/
		pa->a_type = AT_ISDISK;
		pa->a_flags = AF_ISCRYS | AF_ISGRAF | AF_ISDESK;
		break;
	  case 'G':				/* GEM App File		*/
		pa->a_type = AT_ISFILE;
		pa->a_flags = AF_ISCRYS | AF_ISGRAF;
		break;
	  case 'F':				/* DOS File no parms	*/
	  case 'f':				/*   needs full memory	*/
		pa->a_type = AT_ISFILE;
		pa->a_flags = (*pcurr == 'F') ? NONE : AF_ISFMEM;
		break;
	  case 'P':				/* DOS App needs parms	*/
	  case 'p':				/*   needs full memory	*/
		pa->a_type = AT_ISFILE;
		pa->a_flags = (*pcurr == 'P') ? 
				AF_ISPARM : AF_ISPARM | AF_ISFMEM;
		break;
	  case 'D':				/* Directory (Folder)	*/
		pa->a_type = AT_ISFOLD;
		break;
	}
	pcurr++;
	if (pa->a_flags & AF_ISDESK)
	{
	  pcurr = scan_2(pcurr, &pa->a_xspot);
	  pcurr = scan_2(pcurr, &pa->a_yspot);
	}
	pcurr = scan_2(pcurr, &pa->a_aicon);
	pcurr = scan_2(pcurr, &pa->a_dicon);
	pcurr++;
	if (pa->a_flags & AF_ISDESK)
	{
	  pa->a_letter = (*pcurr == ' ') ? 0 : *pcurr;
	  pcurr += 2;
	}
	pcurr = scan_str(pcurr, &pa->a_pappl);
	pcurr = scan_str(pcurr, &pa->a_pdata);
#if MULTIAPP
	if (!(pa->a_flags & AF_ISDESK))			/* only for apps */
	  pcurr = scan_memsz(pcurr, &pa->a_memreq);
#endif
	return(pcurr);
}

/*
 * Translates one resource bit block into the device format expected by
 * the current graphics backend.
 */
VOID app_tran(WORD bi_num)
{
	LONG		lpbi;
	BITBLK		lb;
	BITBLK		*psrc;
	FILE		*fp;

	rsrc_gaddr(R_BITBLK, bi_num, &lpbi);
	psrc = (BITBLK *)(uintptr_t) lpbi;

	BYTE LBCOPY(LONG dst, LONG src, WORD count);
	fp = fopen("/tmp/gem_app_tran.log", "a");
	if (fp != NULL)
	{
	  fprintf(fp,
	      "app_tran bi=%d lpbi=%p src_data=%p src_wb=%d src_hl=%d data=%p wb=%d hl=%d x=%d y=%d color=%d\n",
	      bi_num, (void *)(uintptr_t) lpbi,
	      psrc != NULL ? (void *)(uintptr_t) psrc->bi_pdata : NULLPTR,
	      psrc != NULL ? psrc->bi_wb : 0, psrc != NULL ? psrc->bi_hl : 0,
	      (void *)(uintptr_t) lb.bi_pdata, lb.bi_wb, lb.bi_hl,
	      lb.bi_x, lb.bi_y, lb.bi_color);
	  fclose(fp);
	}
	gsx_trans(lb.bi_pdata, lb.bi_wb, lb.bi_pdata, lb.bi_wb, lb.bi_hl);
}

/*
 * Opens or creates the desktop's configuration/icon file after
 * temporarily switching to the startup drive if necessary.
 */
WORD app_getfh(WORD openit, BYTE *pname, WORD attr)
{
	WORD		handle, tmpdrv;
	LONG		lp;

	handle = 0;
	strcpy(pname, &G.g_srcpth[0]);
	lp = ADDR(&G.g_srcpth[0]);
	tmpdrv = dos_gdrv();
	if (tmpdrv != gl_stdrv)
	  dos_sdrv(gl_stdrv);
	if ( shel_find(lp) )
	{
	  if (openit)
	    handle = dos_open(lp, attr);
	  else
	    handle = dos_create(lp, attr);
	  if ( DOS_ERR )
	  {
	    handle = 0;
	  }
	}
	if (tmpdrv != gl_stdrv)
	  dos_sdrv(tmpdrv);
	return(handle);
}

/*
 * Loads desktop icon definitions and associated text from the icon
 * resource file. The hosted port currently short-circuits into
 * `app_init_placeholder_icons()` so the shell can start without the
 * original icon asset pipeline.
 */
WORD app_rdicon(VOID)
{
	LONG		temp, stmp, dtmp;
	WORD		handle, length, i, iwb, ih;
	WORD		num_icons, num_masks, last_icon, num_wds, 
			num_bytes, msk_bytes, tmp;
#if ALCYON
	WORD		stlength, ret;
	BYTE		*fixup, *poffset, **namelist;
#endif
#if I8086
	WORD		fixup, poffset;
#endif
	return( app_init_placeholder_icons() );
						/* open the file	*/
	handle = app_getfh(TRUE, 
		ini_str( (gl_height <= 300) ? STGEMLOI : STGEMHIC), 0);
	if (!handle)
	  return(FALSE);
						/* how much to read?	*/
	length = NUM_IBLKS * sizeof(ICONBLK);
#if ALCYON
	ret = dos_read(handle, 2, ADDR(&stlength));
#endif
	dos_read(handle, sizeof(BYTE *), ADDR(&gl_pstart));
	dos_read(handle, sizeof(BYTE *), ADDR(&poffset));
	poffset += (2 * sizeof(BYTE *)) + length;
	gl_pstart -= poffset;
						/* read it		*/
	dos_read(handle, length, ADDR(&G.g_idlist[0]) );
	movs(length, &G.g_idlist[0], &G.g_iblist[0]);
						/* find no. of icons	*/
						/*   actually used	*/
	num_icons = last_icon = 0;
	while ( (last_icon < NUM_IBLKS) &&
		(G.g_idlist[last_icon].ib_pmask != -1L) )
	{
	  tmp = max( LLOWD(G.g_idlist[last_icon].ib_pmask),
		     LLOWD(G.g_idlist[last_icon].ib_pdata) );
	  num_icons = max(num_icons, tmp);
	  last_icon++;
	}
	num_icons++;
						/* how many words of 	*/
						/*   data to read?	*/
						/* assume all icons are	*/
						/*   same w,h as first	*/
	num_wds = (G.g_idlist[0].ib_wicon * G.g_idlist[0].ib_hicon) / 16;
	num_bytes = num_wds * 2;

						/* allocate some memory	*/
						/*   in bytes     	*/
				/* gl_pstart = size of icon bit blocks	*/
				/*   and strings on Lattice C		*/
				/* NUM_NAMICS is for string pointers	*/
				/* stlength is strings on ALCYON C	*/
#if ALCYON
	length = gl_pstart + ((NUM_NAMICS + 1) * 4) + stlength;
#else
	length = gl_pstart + (NUM_NAMICS * 2);
#endif
	LONG dos_alloc(LONG nbytes);
						/* read it		*/
	dos_read(handle, length, G.a_datastart);
	dos_close(handle);
						/* fix up str ptrs	*/
	gl_numics = 0;
#if ALCYON
	namelist = (BYTE *) G.a_datastart + gl_pstart;
	for (i = 0; i < NUM_NAMICS; i++)
	{
	  fixup = *namelist - poffset;
	  *namelist++ = fixup;
#endif
#if I8086
	for (i = 0; i < NUM_NAMICS; i++)
	{
	  fixup = LWGET(G.a_datastart + gl_pstart + (i*2)) - poffset;
	  LWSET((G.a_datastart + gl_pstart + (i*2)), fixup);
#endif
	  if ( LBGET(G.a_datastart + fixup) )
	    gl_numics++;
	}
						/* figure out which are	*/
						/*   mask & which data	*/
	for (i=0; i<last_icon; i++)
	{
	  G.g_ismask[ (WORD) G.g_idlist[i].ib_pmask] = TRUE;
	  G.g_ismask[ (WORD) G.g_idlist[i].ib_pdata] = FALSE;
	}
						/* fix up mask ptrs	*/
	num_masks = 0;
	for (i=0; i<num_icons; i++)
	{
	  if (G.g_ismask[i])
	  {
	    G.g_ismask[i] = num_masks;
	    num_masks++;
	  }
	  else
	    G.g_ismask[i] = -1;
	}
						/* allocate memory for	*/
						/*   transformed mask	*/
						/*   forms		*/
	msk_bytes = num_masks * num_bytes;
	LONG dos_alloc(LONG nbytes);
						/* fix up icon pointers	*/
	for (i=0; i<last_icon; i++)
	{
						/* first the mask	*/
	  temp = ( G.g_ismask[ G.g_idlist[i].ib_pmask ] * ((LONG) num_bytes));
	  G.g_iblist[i].ib_pmask = G.a_buffstart + LW(temp);
	  temp = ( G.g_idlist[i].ib_pmask * ((LONG) num_bytes));
	  G.g_idlist[i].ib_pmask = G.a_datastart + LW(temp);
						/* now the data		*/
	  temp = ( G.g_idlist[i].ib_pdata * ((LONG) num_bytes));
	  G.g_iblist[i].ib_pdata = G.g_idlist[i].ib_pdata = 
		G.a_datastart + LW(temp);
						/* now the text ptrs	*/
	  G.g_idlist[i].ib_ytext = G.g_iblist[i].ib_ytext = 
			G.g_idlist[0].ib_hicon;
	  G.g_idlist[i].ib_wtext = G.g_iblist[i].ib_wtext = 12 * gl_wschar;
	  G.g_idlist[i].ib_htext = G.g_iblist[i].ib_htext = gl_hschar + 2;
	}
						/* transform forms	*/
	iwb = G.g_idlist[0].ib_wicon / 8;
	ih = G.g_idlist[0].ib_hicon;

	for (i=0; i<num_icons; i++)
	{
	  if (G.g_ismask[i] != -1)
	  {
						/* preserve standard	*/
						/*   form of masks	*/
	    stmp = G.a_datastart + (i * num_bytes);
	    dtmp = G.a_buffstart + (G.g_ismask[i] * num_bytes);
	    WORD LWCOPY(LONG dst, LONG src, WORD count);
	  }
	  else
	  {
						/* transform over std.	*/
						/*   form of datas	*/
	    dtmp = G.a_datastart + (i * num_bytes);
	  }
	  gsx_trans(dtmp, iwb, dtmp, iwb, ih);
	}
	for (i=0; i<last_icon; i++)
	{
	  if ( i == IG_FOLDER )
	    G.g_iblist[i].ib_pmask = G.g_iblist[IG_TRASH].ib_pmask;
	  if ( ( i == IG_FLOPPY ) ||
	       ( i == IG_HARD ) )
	    G.g_iblist[i].ib_pmask = G.g_iblist[IG_TRASH].ib_pdata;
	  if ( (i >= IA_GENERIC) &&
	       (i < ID_GENERIC) )
	    G.g_iblist[i].ib_pmask = G.g_iblist[IA_GENERIC].ib_pdata;
	  if ( (i >= ID_GENERIC) &&
	       (i < (NUM_ANODES - 1)) )
	    G.g_iblist[i].ib_pmask = G.g_iblist[ID_GENERIC].ib_pdata;
	}
	return(TRUE);
} /* app_rdicon */


/*
 * Builds the in-memory desktop/application list from `DESKTOP.INF`,
 * restores saved window metadata, and computes icon-grid geometry for
 * the current screen.
 */
WORD app_start(VOID)
{
	WORD		i, x, y, w, h;
	ANODE		*pa;
	WSAVE		*pws;
	BYTE		*pcurr, *ptmp, prevdisk;
        WORD		fh, envr, xcnt, ycnt, xcent, wincnt;
#if MULTIAPP
	WORD		numaccs;
	BYTE		*savbuff;
	
	numaccs = 0;
#endif		
						/* remember start drive	*/
	gl_stdrv = dos_gdrv();

	G.g_pbuff = &gl_buffer[0];
	{
	  FILE *fp = fopen("/tmp/gem_app_alloc.log", "a");
	  if (fp != NULL)
	  {
	    fprintf(fp, "app_start pbuff=%p gl_buffer=%p alist0=%p alist_end=%p\n",
	        (void *) G.g_pbuff, (void *)&gl_buffer[0],
	        (void *)&G.g_alist[0], (void *)&G.g_alist[NUM_ANODES - 1]);
	    fclose(fp);
	  }
	}
	
	for(i=NUM_ANODES - 2; i >= 0; i--)
	  G.g_alist[i].a_next = &G.g_alist[i + 1];
	G.g_ahead = (ANODE *) NULL;
	G.g_aavail = &G.g_alist[0];
	G.g_alist[NUM_ANODES - 1].a_next = (ANODE *) NULL;
	{
	  FILE *fp = fopen("/tmp/gem_app_alloc.log", "a");
	  if (fp != NULL)
	  {
	    fprintf(fp, "app_start alist0=%p alist1=%p avail=%p size=%zu\n",
	        (void *)&G.g_alist[0], (void *)&G.g_alist[1],
	        (void *) G.g_aavail, sizeof(ANODE));
	    fclose(fp);
	  }
	}

	shel_get(ADDR(&gl_afile[0]), SIZE_AFILE);
	if (gl_afile[0] != '#')
	{
						/* invalid signature	*/
						/*   so read from disk	*/
	  fh = app_getfh(TRUE, ini_str(STGEMAPP), 0x0);
	  if (!fh)
	    return(FALSE);
	  WORD dos_read(WORD handle, WORD cnt, LONG pbuffer);
	  dos_close(fh);
	  gl_afile[G.g_afsize] = '\0';
	}
	

	wincnt = 0;
	pcurr = &gl_afile[0];
	prevdisk = ' ';
	while (*pcurr)
	{
	  if (*pcurr != '#')
	    pcurr++;
	  else
	  {
	    pcurr++;
	    switch(*pcurr)
	    {
	      case 'M':				/* Media (Hard/Floppy)	*/
	      case 'G':				/* GEM Application	*/
	      case 'F':				/* File	(DOS w/o parms)	*/
	      case 'f':				/*   use full memory	*/
	      case 'P':				/* Parm	(DOS w/ parms)	*/
	      case 'p':				/*   use full memory	*/
	      case 'D':				/* Directory		*/
			if ( *pcurr == 'M' )
			  prevdisk = 'M';
			else
			{
						/* rest of standards	*/
						/*   after last disk	*/
			  if (prevdisk == 'M') 
			  {
			    for (i = 0; i < 6; i++)
		            {
			      pa = app_alloc(TRUE);
			      if (pa == NULL)
			        return(FALSE);
	      		      app_parse(ini_str(ST1STD+i)+1, pa);
			    } /* for */
			  } /* if */
			  prevdisk = ' ';
			}
			pa = app_alloc(TRUE);
			if (pa == NULL)
			  return(FALSE);
	      		pcurr = app_parse(pcurr, pa);
			break;
#if MULTIAPP			
	      case 'A':				/* Desk Accessory	*/
		        pcurr++;
			pcurr = scan_2(pcurr, &(gl_caccs[numaccs].acc_swap));
			savbuff = G.g_pbuff;
			G.g_pbuff = &(gl_caccs[numaccs].acc_name[0]);
			pcurr = scan_str(pcurr, &ptmp);
			G.g_pbuff = savbuff;
			numaccs++;
			break;
#endif
	      case 'W':				/* Window		*/
			pcurr++;
			if ( wincnt < NUM_WNODES )
			{
			  pws = &G.g_cnxsave.win_save[wincnt];
			  pcurr = scan_2(pcurr, &pws->hsl_save);
			  pcurr = scan_2(pcurr, &pws->vsl_save);
/* Local behavior fixes kept from the imported code. */
			  pcurr = scan_2(pcurr, &x);
			  pcurr = scan_2(pcurr, &y);
			  pcurr = scan_2(pcurr, &w);
			  pcurr = scan_2(pcurr, &h);
			  pcurr = scan_2(pcurr, &pws->obid_save);
			  ptmp = &pws->pth_save[0];
			  pcurr++;
			  while ( *pcurr != '@' )
			    *ptmp++ = *pcurr++;
			  *ptmp = '\0';
			  gl_savewin[wincnt].g_x = x * gl_wchar;
			  gl_savewin[wincnt].g_y = y * gl_hchar;
			  gl_savewin[wincnt].g_w = w * gl_wchar;
			  gl_savewin[wincnt++].g_h = h * gl_hchar;
			}
			break;
	      case 'E':
			pcurr++;
			pcurr = scan_2(pcurr, &envr);
			G.g_cnxsave.vitem_save = ( (envr & 0x80) != 0);
			G.g_cnxsave.sitem_save = ( (envr & 0x60) >> 5);
			G.g_cnxsave.cdele_save = ( (envr & 0x10) != 0);
			G.g_cnxsave.ccopy_save = ( (envr & 0x08) != 0);
			G.g_cnxsave.cdclk_save = envr & 0x07;
			pcurr = scan_2(pcurr, &envr);
			G.g_cnxsave.covwr_save = ( (envr & 0x10) == 0);
			G.g_cnxsave.cmclk_save = ( (envr & 0x08) != 0);
			G.g_cnxsave.cdtfm_save = ( (envr & 0x04) == 0);
			G.g_cnxsave.ctmfm_save = ( (envr & 0x02) == 0);
			sound(FALSE, !(envr & 0x01), 0);
			break;
	    }
	  }
	}
	if (!app_rdicon())
	  return(FALSE);	
	G.g_wicon = (12 * gl_wschar) + (2 * G.g_idlist[0].ib_xtext);
	G.g_hicon = G.g_idlist[0].ib_hicon + gl_hschar + 2;

	G.g_icw = (gl_height <= 300) ? 0 : 8;
	G.g_icw += G.g_wicon;
	xcnt = (gl_width/G.g_icw);
	G.g_icw += (gl_width % G.g_icw) / xcnt;
	G.g_ich = G.g_hicon + MIN_HINT;
	ycnt = ((gl_height-gl_hbox) / G.g_ich);
	G.g_ich += ((gl_height-gl_hbox) % G.g_ich) / ycnt;

	xcent = (G.g_wicon - G.g_idlist[0].ib_wicon) / 2;
	G.g_nmicon = 9;
	G.g_xyicon[0] = xcent;  G.g_xyicon[1] = 0;
	G.g_xyicon[2]=xcent; G.g_xyicon[3]=G.g_hicon-gl_hschar-2;
	G.g_xyicon[4] = 0;  G.g_xyicon[5] = G.g_hicon-gl_hschar-2;
	G.g_xyicon[6] = 0;  G.g_xyicon[7] = G.g_hicon;
	G.g_xyicon[8] = G.g_wicon;  G.g_xyicon[9] = G.g_hicon;
	G.g_xyicon[10]=G.g_wicon; G.g_xyicon[11] = G.g_hicon-gl_hschar-2;
	G.g_xyicon[12]=G.g_wicon - xcent; G.g_xyicon[13]=G.g_hicon-gl_hschar-2;
	G.g_xyicon[14] = G.g_wicon - xcent;  G.g_xyicon[15] = 0;
	G.g_xyicon[16] = xcent;  G.g_xyicon[17] = 0;
	G.g_nmtext = 5;
	G.g_xytext[0] = 0;  		G.g_xytext[1] = 0;
	G.g_xytext[2] = gl_wchar * 12; 	G.g_xytext[3] = 0;
	G.g_xytext[4] = gl_wchar * 12;   G.g_xytext[5] = gl_hchar;
	G.g_xytext[6] = 0;  		G.g_xytext[7] = gl_hchar;
	G.g_xytext[8] = 0; 		G.g_xytext[9] = 0;
	app_fix_positions();
	return(TRUE);
}


/*
*	The DESKTOP.INF file stores desktop icon positions in icon-grid
*	units. Convert them to screen pixels once the current grid geometry
*	is known.
*/
static VOID app_fix_positions(VOID)
{
	ANODE		*pa;
	WORD		xcells;
	WORD		ycells;

	xcells = (G.g_icw > 0) ? (gl_width / G.g_icw) : 0;
	ycells = (G.g_ich > 0) ? (gl_height / G.g_ich) : 0;
	for (pa = G.g_ahead; pa; pa = pa->a_next)
	{
	  if (!(pa->a_flags & AF_ISDESK))
	    continue;
	  if ((xcells > 0) && (pa->a_xspot < xcells))
	    pa->a_xspot *= G.g_icw;
	  if ((ycells > 0) && (pa->a_yspot < ycells))
	    pa->a_yspot = G.g_ydesk + (pa->a_yspot * G.g_ich);
	}
}


/*
*	Reverse list when we write so that we can read it in naturally
*/
VOID app_revit(VOID)
{
	ANODE		*pa;
	ANODE		*pnxtpa;
						/* reverse list		*/
	pa = G.g_ahead;
	G.g_ahead = (ANODE *) NULL;
	while(pa)
	{
	  pnxtpa = pa->a_next;
	  pa->a_next = G.g_ahead;
	  G.g_ahead = pa;
	  pa = pnxtpa;
	}
}


/*
*	Save the current state of all the icons to a file called 
*	DESKTOP.INF
*/
WORD app_save(WORD todisk)
{
	WORD		i, fh, ret, envr;
	BYTE		*pcurr, *ptmp;
	ANODE		*pa;
	WSAVE		*pws;

	bfill(SIZE_AFILE, 0, &gl_afile[0]);
	pcurr = &gl_afile[0];
						/* save evironment	*/
	*pcurr++ = '#';
	*pcurr++ = 'E';
	envr = 0x0;
	envr |= (G.g_cnxsave.vitem_save) ? 0x80 : 0x00;
	envr |= ((G.g_cnxsave.sitem_save) << 5) & 0x60;
	envr |= (G.g_cnxsave.cdele_save) ? 0x10 : 0x00;
	envr |= (G.g_cnxsave.ccopy_save) ? 0x08 : 0x00;
	envr |= G.g_cnxsave.cdclk_save;
	pcurr = save_2(pcurr, envr);
	envr = (G.g_cnxsave.covwr_save) ? 0x00 : 0x10;
	envr |= (G.g_cnxsave.cmclk_save) ? 0x08 : 0x00;
	envr |= (G.g_cnxsave.cdtfm_save) ? 0x00 : 0x04;
	envr |= (G.g_cnxsave.ctmfm_save) ? 0x00 : 0x02;
	envr |= sound(FALSE, 0xFFFF, 0)  ? 0x00 : 0x01;
	pcurr = save_2(pcurr, envr );

	*pcurr++ = 0x0d;
	*pcurr++ = 0x0a;
						/* save windows		*/
	for(i=0; i<NUM_WNODES; i++)
	{
	  *pcurr++ = '#';
	  *pcurr++ = 'W';
	  pws = &G.g_cnxsave.win_save[i];
	  pcurr = save_2(pcurr, pws->hsl_save);
	  pcurr = save_2(pcurr, pws->vsl_save);
	  pcurr = save_2(pcurr, pws->x_save / gl_wchar);
	  pcurr = save_2(pcurr, pws->y_save / gl_hchar);
	  pcurr = save_2(pcurr, pws->w_save / gl_wchar);
	  pcurr = save_2(pcurr, pws->h_save / gl_hchar);
	  pcurr = save_2(pcurr, pws->obid_save);
	  ptmp = &pws->pth_save[0];
	  *pcurr++ = ' ';
	  if (*ptmp != '@')
	  {
	    while (*ptmp)
	      *pcurr++ = *ptmp++;
	  }
	  *pcurr++ = '@';
	  *pcurr++ = 0x0d;
	  *pcurr++ = 0x0a;
	}		
#if MULTIAPP
	for (i=0; i<3; i++)
	  if (gl_caccs[i].acc_name[0])
	  {
	    *pcurr++ = '#';
	    *pcurr++ = 'A';
	    pcurr = save_2(pcurr, gl_caccs[i].acc_swap);
	    *pcurr++ = ' ';
	    pcurr = save_str(pcurr, &(gl_caccs[i].acc_name[0]));
	    pcurr--;
	    *pcurr++ = 0x0d;
	    *pcurr++ = 0x0a;
	  }
#endif	
						/* reverse ANODE list	*/
	app_revit();
						/* save ANODE list	*/
	for(pa=G.g_ahead; pa; pa=pa->a_next)
	{
	  *pcurr++ = '#';
	  switch(pa->a_type)
	  {
	    case AT_ISDISK:
		*pcurr++ = 'M';
		break;
	    case AT_ISFILE:
		if ( (pa->a_flags & AF_ISCRYS) &&
		     (pa->a_flags & AF_ISGRAF) )
		  *pcurr++ = 'G';
		else
		{  
		  *pcurr = (pa->a_flags & AF_ISPARM) ? 'P' : 'F';
		  if (pa->a_flags & AF_ISFMEM)
		    *pcurr += 'a' - 'A';
		  pcurr++;
		}
		break;
	    case AT_ISFOLD:
		*pcurr++ = 'D';
		break;
	  }
	  if (pa->a_flags & AF_ISDESK)
	  {
	    pcurr = save_2(pcurr, pa->a_xspot / G.g_icw);
	    pcurr = save_2(pcurr, (pa->a_yspot - G.g_ydesk) / G.g_ich);
	  }
	  pcurr = save_2(pcurr, pa->a_aicon);
	  pcurr = save_2(pcurr, pa->a_dicon);
	  *pcurr++ = ' ';
	  if (pa->a_flags & AF_ISDESK)
	  {
	    *pcurr++ = (pa->a_letter == 0) ? ' ' : pa->a_letter;
	    *pcurr++ = ' ';
	  }
	  pcurr = save_str(pcurr, pa->a_pappl);
	  pcurr = save_str(pcurr, pa->a_pdata);
	  pcurr--;
#if MULTIAPP
	  if (!(pa->a_flags & AF_ISDESK))	/* only for apps	*/
	  {
	    pcurr++;				/* leave blank		*/
	    pcurr = save_memsz(pcurr, pa->a_memreq);
	  }
#endif
	  *pcurr++ = 0x0d;
	  *pcurr++ = 0x0a;
						/* skip standards	*/
	  if ( (pa->a_type == AT_ISDISK) && 
	       (pa->a_next->a_type != AT_ISDISK) )
	  {
	    for(i=0; i<6; i++)
	      pa = pa->a_next;
	  }
	}
	*pcurr++ = 0x1a;
	*pcurr++ = 0x0;
						/* reverse list back	*/
	app_revit();
						/* calculate size 	*/
	G.g_afsize = pcurr - &gl_afile[0];
						/* save in memory	*/
	shel_put(ADDR(&gl_afile[0]), G.g_afsize);
						/* save to disk		*/
	if (todisk)
	{
	  G.g_afsize--;
	  fh = 0;
	  while (!fh)
	  {
	    fh = app_getfh(FALSE, ini_str(STGEMAPP), 0x0);
	    if (!fh)
	    {
	      ret = fun_alert(1, STNOINF, NULLPTR);
	      if (ret == 2)
	        return(FALSE);
	    }
	  }
	  WORD dos_write(WORD handle, WORD cnt, LONG pbuffer);
	  dos_close(fh);
	}
	return(TRUE);
}


/*
 * Rebuilds the runtime desktop object tree from the parsed application
 * list and records which drives are present.
 */

BYTE app_blddesk(VOID)
{
	WORD		obid;
	UWORD		bvdisk, bvhard, bvect;
	ANODE		*pa;
	OBJECT		*pob;
	ICONBLK		*pic;
#if ALCYON
	LONG		*ptr;
#endif						/* free all this windows*/
						/*   kids and set size	*/
	obj_wfree(DROOT, 0, 0, gl_width, gl_height);
#if ALCYON
	ptr = &global[3];
	G.g_screen[DROOT].ob_spec = LLGET(ptr);
#else
	LONG HW(UWORD x);
#endif
	bvdisk = bvhard = 0x0;

	for(pa = G.g_ahead; pa; pa = pa->a_next)
	{
	  if (pa->a_flags & AF_ISDESK)
	  {
	    obid = obj_ialloc(DROOT, pa->a_xspot, pa->a_yspot,
					G.g_wicon, G.g_hicon);
	    if (!obid)
	    {
	    /* error case, no more obs */
	    }
						/* set up disk vector	*/
	    if (pa->a_type == AT_ISDISK)
	    {
	      bvect = ((UWORD) 0x8000) >> ((UWORD) (pa->a_letter - 'A'));
	      bvdisk |= bvect;
	      if (pa->a_aicon == IG_HARD)
		bvhard |= bvect;
	    }
						/* remember it		*/
	    pa->a_obid = obid;
						/* build object		*/
	    pob = &G.g_screen[obid];
	    pob->ob_state = NORMAL;
	    pob->ob_flags = NONE;
	    pob->ob_type = G_ICON;
	    G.g_index[obid] = pa->a_aicon;
	    pob->ob_spec = ADDR( pic = &gl_icons[obid] );
	    movs(sizeof(ICONBLK), &G.g_iblist[pa->a_aicon], pic);
	    pic->ib_xicon = ((G.g_wicon - pic->ib_wicon) / 2);
	    pic->ib_ptext = ADDR(pa->a_pappl);
	    pic->ib_char |= (0x00ff & pa->a_letter);
	  } /* if */
	} /* for */
	appl_bvset(bvdisk, bvhard);
	return( get_defdrv(bvdisk, bvhard) );
} /* app_blddesk */


/*
 * Finds the `ANODE` that corresponds either to a desktop object id or
 * to a file-pattern match, depending on the caller's lookup mode.
 */
ANODE *app_afind(WORD isdesk, WORD atype, WORD obid, BYTE *pname,
    WORD *pisapp)
{
	ANODE		*pa;

	for(pa = G.g_ahead; pa; pa = pa->a_next)
	{
	  if (isdesk)
	  {
	    if (pa->a_obid == obid)
	      return(pa);
	  }
	  else
	  {
	    if ( (pa->a_type == atype) && !(pa->a_flags & AF_ISDESK) )
	    {
	      if ( wildcmp(pa->a_pdata, pname) )
	      {
		*pisapp = FALSE;
		return(pa);
	      }
	      if ( wildcmp(pa->a_pappl, pname) )
	      {
		*pisapp = TRUE;
		return(pa);
	      } /* if */
	    } /* if */
	  } /* else */
	} /* for */
	return(0);
} /* app_afind */

/*
 * Chooses the default desktop drive letter, preferring the lowest
 * available hard disk and otherwise falling back to drive `A`.
 */
BYTE get_defdrv(UWORD dr_exist, UWORD dr_hard)
{
/* this routine returns the drive letter of the lowest drive: lowest	*/
/* lettered hard disk if possible, otherwise lowest lettered floppy	*/
/* (which is usually A)							*/
/* in dr_exist, MSbit = A						*/
	UWORD		mask, hd_disk;
	WORD		ii;
	BYTE		drvletr;

	mask = 0x8000;
	hd_disk = dr_exist & dr_hard;
	if (hd_disk)
	{		/* there's a hard disk out there somewhere	*/
	  for (ii = 0; ii <= 15; ii++)
	  {
	    if (mask & hd_disk)
	    {
	      drvletr = ii + 'A';
	      break;
	    } /* if */
	    mask >>= 1;
	  } /* for */
	} /* if hd_disk */
	else
	  drvletr = 'A';			/* assume A is always	*/
	  					/* lowest floppy	*/
	return(drvletr);
} /* get_defdrv */
