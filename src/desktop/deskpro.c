/*
 * Implements desktop program launching and command execution helpers. The
 * file is a key boundary between file selection in the shell and process
 * startup in GEM.
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

#if MULTIAPP
GLOBAL WORD	pr_kbytes;
GLOBAL LONG	pr_beggem;		/* first paragraph of AES	*/
GLOBAL LONG	pr_begacc;		/* first paragraph of acces.	*/
GLOBAL LONG	pr_begdsk;		/* first paragraph of desktop	*/
GLOBAL LONG	pr_topdsk;		/* first paragraph above desktop  */
GLOBAL LONG	pr_topmem;		/* next paragraph above free area */
GLOBAL LONG	pr_ssize;		/* size of channel system overhead */
GLOBAL LONG	pr_itbl;		/* base of GDOS default int table  */
EXTERN WORD	gl_fmemflg;

EXTERN WORD proc_create(LONG ibegaddr, LONG isize, WORD isswap, WORD isgem, WORD *onum);
EXTERN WORD proc_run(WORD proc_num, WORD isgraf, WORD isover, LONG pcmd, LONG ptail);
EXTERN WORD fun_alert(WORD defbut, WORD stnum, WORD pwtemp[]);
#endif

EXTERN VOID do_wopen(WORD new_win, WORD wh, WORD curr, WORD x, WORD y, WORD w, WORD h);
EXTERN WORD dos_gdrv(void);
EXTERN WORD dos_sdrv(WORD newdrv);
EXTERN VOID dos_chdir(LONG pdrvpath);
EXTERN WORD shel_envrn(LONG ppath, LONG psrch);
EXTERN WORD strlen(BYTE *p1);
EXTERN WORD graf_mouse(WORD m_number, LONG m_addr);
EXTERN WORD shel_write(WORD doex, WORD isgr, WORD iscr, LONG pcmd, LONG ptail);

EXTERN WORD	gl_stdrv;
EXTERN WORD	DOS_ERR;
EXTERN GLOBES	G;

WORD pro_exec(WORD isgraf, WORD isover, LONG pcmd, LONG ptail);


#if MULTIAPP

#define LMIN(x,y) ((x)<(y)?(x):(y))


	/* in pro_chcalc long addresses are flattened out with no segment */

	VOID pro_chcalc(appsize, begaddr, chsize)
	LONG		appsize;
	LONG		*begaddr;
	LONG		*chsize;
	
{
	static LONG	begfree = 0l;
	LONG		maxmem;

	if (appsize == -1)			/* full step	*/
	{
	  *begaddr = LSEGOFF(pr_beggem); 
	  *chsize = pr_topmem - pr_beggem;
	  return;
	}
	if ((begfree >= pr_topmem) || (begfree < pr_topdsk))
	  begfree = pr_topdsk;	
	
	maxmem = pr_topmem - pr_begdsk;
	*chsize = LMIN(appsize+pr_ssize, maxmem);

	if ((begfree + *chsize) < pr_topmem)
	  *begaddr = LSEGOFF(begfree);
	else
	  *begaddr = LSEGOFF(pr_topmem - *chsize);
	begfree += *chsize;	
}
#endif


WORD pro_chdir(WORD drv, BYTE *ppath)
{
	WORD		tmpdrv;
						/* change to directory	*/
						/*   that application	*/
						/*   is in		*/
	if (!drv)
	  return( (DOS_ERR = TRUE) );

	if ( drv != '@' ) 
	{
	  tmpdrv = dos_gdrv();
	  dos_sdrv(drv - 'A');
	  if (DOS_ERR)
	  {
	    dos_sdrv(tmpdrv);
	    return(FALSE);
	  }
	  G.g_srcpth[0] = drv;
	  G.g_srcpth[1] = ':';
	  G.g_srcpth[2] = '\\';
	  strcpy(ppath, &G.g_srcpth[3]);
	  dos_chdir(ADDR(&G.g_srcpth[0]));
	}
else dos_sdrv(gl_stdrv);		/* don't leave closed drive hot	*/
	return(TRUE);
} /* pro_chdir */

WORD pro_cmd(BYTE *psubcmd, BYTE *psubtail, WORD exitflag)
{
	LONG		lp;
	WORD		i, ii, drv;
	BYTE		save_ch;

	shel_envrn(ADDR(&lp), ADDR("COMSPEC="));
	if (lp)
	{
	  WORD LSTCPY(LONG dst, LONG src);
/* Local behavior fixes kept from the imported code. */
	  if (!exitflag)
	  {
	    i = 0;
	    while(G.g_cmd[i] != '\\')	/* find first backslash		*/
	      i++;
				/* change to drive specified by COMSPEC	*/
	    drv = G.g_cmd[i - 2] - 'A';
	    dos_sdrv(drv);
				/* chdir to path specified by COMSPEC	*/
/* mdf */
	    ii = strlen(&G.g_cmd[0]);
	    while(G.g_cmd[--ii] != '\\');	/* find last backslash	*/
	    if (i == ii)		/* root directory?		*/
	      ii++;			/* keep backslash in path name	*/
/* *** */
	    save_ch = G.g_cmd[ii];	/* save char for later		*/
	    G.g_cmd[ii] = NULL;		/* make a null-term. string	*/
	    dos_chdir(ADDR(&G.g_cmd[0]));  /* change to that dir.	*/
	    G.g_cmd[ii] = save_ch;	/* put the char back		*/
	  }
	  if (exitflag)
	  {
#if PCDOS
	    strcpy("/C ", &G.g_tail[1]);
#endif
	    strcat(psubcmd, &G.g_tail[1]);
	    strcat(" ", &G.g_tail[1]);
	    strcat(psubtail, &G.g_tail[1]);
	  }
	  else
	    G.g_tail[1] = NULL;
	  return(TRUE);
	} /* if lp */
else return(FALSE);
} /* pro_cmd */

WORD pro_run(WORD isgraf, WORD isover, WORD wh, WORD curr)
{
	WORD		ret, len, i;

	WORD strlen(BYTE *p1);
	if ( (len) && (!isgraf) )
	{
	  for(i = len; i; i--)
	    G.g_tail[i+1] = G.g_tail[i];
	  G.g_tail[1] = ' ';
	  len++;
	} /* if */
	G.g_tail[0] = len;
	G.g_tail[len+1] = 0x0D;
#if MULTIAPP
	if (isover != 3)		/* keep icon SELECTED during FORMAT */
	  do_wopen(FALSE, wh, curr, G.g_xdesk, G.g_ydesk, G.g_wdesk, G.g_hdesk);
#endif
	ret = pro_exec(isgraf, isover, G.a_cmd, G.a_tail);
	if (isover == -1)
	  ret = FALSE;
	else
	{
#if MULTIAPP
	  if (isover == 3)			/* for FORMAT		*/
#else
	  if (wh != -1)
#endif
	    do_wopen(FALSE, wh, curr, G.g_xdesk, G.g_ydesk,
	    	     G.g_wdesk, G.g_hdesk);
	} /* else */
	return(ret);
} /* pro_run */

WORD pro_exec(WORD isgraf, WORD isover, LONG pcmd, LONG ptail)
{
	WORD		ret;
#if MULTIAPP
	WORD		chnum;
	LONG		begaddr, csize;

	if (isover != 3)
#endif
	graf_mouse(HGLASS, 0x0L);

#if MULTIAPP
	if ((isover == -1) || (isover == 2) || (isover == 3))
	{
	  if (isover == 2)			/* full step	*/
	    pro_chcalc((LONG)-1, &begaddr, &csize);
else pro_chcalc((LONG)pr_kbytes << 10, &begaddr, &csize);
/*
dbg("NEW START ADDRESS = %X\r\n", begaddr);
dbg("NEW CHANNEL SIZE  = %X\r\n", csize);
*/
	  ret = proc_create(begaddr, csize, 1, isgraf, &chnum);
	  if (!ret)
	  {
	    fun_alert(1,STNOROOM,NULLPTR);
	    return(FALSE);
	  }
	  if (isover == 2)
	    gl_fmemflg |= (1 << chnum);

/*
dbg("CREATE = %d\r\n", ret);
dbg("CHNUM  = %d\r\n", chnum);
dbg("PCMD   = %s\r\n", pcmd);
dbg("PTAIL  = %s\r\n", ptail);
*/
	  ret = proc_run(chnum, isgraf, isover, pcmd, ptail);
	  if (isover==3)
	    ret = 0;
	}
	else
#endif
	  ret = shel_write(TRUE, isgraf, isover, pcmd, ptail);
	if (!ret)
	  graf_mouse(ARROW, 0x0L);
	return( ret );
} /*  */

WORD pro_exit(LONG pcmd, LONG ptail)
{
	WORD		ret;

	ret = shel_write(FALSE, FALSE, 1, pcmd, ptail);
	return( ret );
} /* pro_exit */
