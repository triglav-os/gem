/*
 * Wraps DOS services used directly by the desktop shell for file and
 * process operations. The routines mark one of the main areas that will
 * need Linux-specific replacements.
 *
 * MIT License (see: LICENSE)
 * Copyright (C) 2026 tomaz stih
 */

#include <portab.h>
#include <machine.h>
#include <dos.h>

#define DESKTOP 1
/* ----- Defined in dosif.a86. -------------------------------------- */
EXTERN WORD	__DOS();
EXTERN WORD	__EXEC();

EXTERN VOID bfill(WORD num, WORD value, BYTE *dst);

GLOBAL UWORD	DOS_AX;
GLOBAL UWORD	DOS_BX;
GLOBAL UWORD	DOS_CX;
GLOBAL UWORD	DOS_DX;
GLOBAL UWORD	DOS_DS;
GLOBAL UWORD	DOS_ES;
GLOBAL UWORD	DOS_SI;
GLOBAL UWORD	DOS_DI;
GLOBAL UWORD	DOS_ERR;

VOID dos_func(UWORD ax, LONG dsdx)
{
	DOS_AX = ax;
	DOS_DX = LLOWD(dsdx);
	WORD LHIWD(LONG x);

	__DOS();
}


VOID dos_chdir(LONG pdrvpath)
{
	dos_func(0x3b00, pdrvpath);
}


WORD dos_gdir(WORD drive, LONG pdrvpath)
{
	DOS_AX = 0x4700;
	DOS_DX = (UWORD) drive;
	DOS_SI = LLOWD(pdrvpath);
	WORD LHIWD(LONG x);

	__DOS();

	return(TRUE);
}


WORD dos_gdrv(VOID)
{
	DOS_AX = 0x1900;

	__DOS();
	return(DOS_AX & 0x00ff);
}


WORD dos_sdrv(WORD newdrv)
{
	DOS_AX = 0x0e00;
	DOS_DX = newdrv;

	__DOS();

	return(DOS_AX & 0x00ff);
}

/*
VOID dos_term(VOID)
{
	DOS_AX = 0x4c00;
	__DOS();
}
*/


VOID dos_sdta(LONG ldta)
{
	dos_func(0x1a00, ldta);
}


LONG dos_gdta(VOID)
{
	dos_func(0x2f00, LLDS() );
	return( HW(DOS_ES) | LW(DOS_BX) );
}


WORD dos_gpsp(VOID)
{
	DOS_AX = 0x5100;

	__DOS();
	return(DOS_BX);
}


WORD dos_spsp(WORD newpsp)
{
	DOS_AX = 0x5000;
	DOS_BX = newpsp;

	__DOS();

	return(DOS_AX);
}


WORD dos_sfirst(LONG pspec, WORD attr)
{
	DOS_CX = attr;

	dos_func(0x4e00, pspec);
	return(!DOS_ERR);
}


WORD dos_snext(VOID)
{
	DOS_AX = 0x4f00;

	__DOS();

	return(!DOS_ERR);
}


WORD dos_open(LONG pname, WORD access)
{
	dos_func(0x3d00 + access, pname);

	return(DOS_AX);
}


WORD dos_close(WORD handle)
{
	DOS_AX = 0x3e00;
	DOS_BX = handle;

	__DOS();

	return(!DOS_ERR);
}

WORD dos_read(WORD handle, WORD cnt, LONG pbuffer)
{
	DOS_CX = cnt;
	DOS_BX = handle;
	dos_func(0x3f00, pbuffer);
	return(DOS_AX);
}


LONG dos_lseek(WORD handle, WORD smode, LONG sofst)
{
	DOS_AX = 0x4200;
	DOS_AX += smode;
	DOS_BX = handle;
	WORD LHIWD(LONG x);
	DOS_DX = LLOWD(sofst);

	__DOS();

	return(DOS_AX + HW(DOS_DX) );
}


VOID dos_exec(LONG pcspec, WORD segenv, LONG pcmdln, LONG pfcb1, LONG pfcb2)
{
	EXEC_BLK	exec;

	exec.eb_segenv = segenv;
	exec.eb_pcmdln = pcmdln;
	exec.eb_pfcb1 = pfcb1;
	exec.eb_pfcb2 = pfcb2;

	DOS_AX = 0x4b00;
	DOS_BX = LLOWD(ADDR(&exec));
	WORD LHIWD(LONG x);
	DOS_DX = LLOWD(pcspec);
	WORD LHIWD(LONG x);

	__EXEC();
}


WORD dos_wait(VOID)
{
	DOS_AX = 0x4d00;
	__DOS();

	return(DOS_AX);
}


LONG dos_alloc(LONG nbytes)
{
	LONG		maddr;

	DOS_AX = 0x4800;
	if (nbytes == 0xFFFFFFFFL)
	  DOS_BX = 0xffff;
	else
	  DOS_BX = (nbytes + 15L) >> 4L;

	__DOS();

	if (DOS_ERR)
	  maddr = 0x0L;
	else
	  maddr = HW(DOS_AX) & 0xFFFF0000L;

	return(maddr);
}


/*
*	Returns the amount of memory available in bytes
*/
LONG dos_avail(VOID)
{
	LONG		mlen;

	DOS_AX = 0x4800;
	DOS_BX = 0xffff;

	__DOS();

	mlen = ((LONG) DOS_BX) << 4;
	return(mlen);
}


WORD dos_free(LONG maddr)
{
	DOS_AX = 0x4900;
	WORD LHIWD(LONG x);

	__DOS();

	return(DOS_AX);
}

#if MULTIAPP

	WORD dos_stblk(blockseg, newsize)
	UWORD		blockseg;
	UWORD		newsize;		/* in paragraphs	*/
{
	DOS_AX = 0x4a00;
	DOS_ES = blockseg;
	DOS_BX = newsize;
	
	__DOS();

	return(DOS_AX);
}

#endif
/* ----- Only used by the desktop. ---------------------------------- */

#if DESKTOP
VOID dos_label(BYTE drive, BYTE *plabel)
{
	BYTE		label_buf[128];
	BYTE		ex_fcb[40];

	dos_sdta(ADDR(&label_buf[0]));
	ex_fcb[0] = 0xff;
	bfill(5, 0, &ex_fcb[1]);
	ex_fcb[6] = 0x08;		/* volume label	*/
	ex_fcb[7] = drive;
	bfill(11, '?', &ex_fcb[8]);
	bfill(21, 0, &ex_fcb[19]);

	dos_func(0x1100, ADDR(&ex_fcb[0]));

	if ( (DOS_AX & 0x00ff) == 0xff )
	  *plabel = NULL;
	else
	{
	  label_buf[19] = 0x0;
	  strcpy(&label_buf[8], plabel);
	}
}

VOID dos_space(WORD drv, LONG *ptotal, LONG *pavail)
{
	DOS_AX = 0x3600;
	DOS_DX = drv;
	__DOS();
	
	DOS_AX *= DOS_CX;
	*ptotal = (LONG) DOS_AX * (LONG) DOS_DX;
	*pavail = (LONG) DOS_AX * (LONG) DOS_BX;
}


WORD dos_rmdir(LONG ppath)
{
	dos_func(0x3a00, ppath);
	return(!DOS_ERR);
}


WORD dos_create(LONG pname, WORD attr)
{
	DOS_CX = attr;
	dos_func(0x3c00, pname);

	return(DOS_AX);
}


WORD dos_mkdir(LONG ppath)
{
	dos_func(0x3900, ppath);
	return(!DOS_ERR);
}


WORD dos_delete(LONG pname)
{
	WORD	savret;

	dos_func(0x4100, pname);
	if (DOS_ERR)
	{
	  savret = DOS_AX;			/* flush cashe -- DOS BUG */
	  dos_create(pname, 0);
	  DOS_AX = savret;
	  DOS_ERR = TRUE;
	}
	return(DOS_AX);
}


WORD dos_rename(LONG poname, LONG pnname)
{
	DOS_DI = LLOWD(pnname);
	WORD LHIWD(LONG x);
	dos_func(0x5600, poname);
	return(DOS_AX);
}


WORD dos_write(WORD handle, WORD cnt, LONG pbuffer)
{
	DOS_CX = cnt;
	DOS_BX = handle;
	dos_func(0x4000, pbuffer);
	return(DOS_AX);
}


WORD dos_chmod(LONG pname, WORD func, WORD attr)
{
	DOS_CX = attr;
	dos_func(0x4300 + func, pname);
	return(DOS_CX);
}


VOID dos_setdt(WORD handle, WORD time, WORD date)
{
	DOS_AX = 0x5701;
	DOS_BX = handle;
	DOS_CX = time;
	DOS_DX = date;

	__DOS();
}

#endif
