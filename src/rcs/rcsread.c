/*
 * Implements resource-file parsing and load helpers for the resource
 * construction set. The routines turn on-disk resource data into the
 * editor in-memory model.
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

#define ROB_TYPE (psubstruct + 6)	/* Long pointer in OBJECT	*/
#define ROB_FLAGS (psubstruct + 8)	/* Long pointer in OBJECT	*/
#define ROB_SPEC (psubstruct + 12)	/* Long pointer in OBJECT	*/

#define RTE_PTEXT (psubstruct + 0)	/* Long pointers in TEDINFO	*/
#define RTE_PTMPLT (psubstruct + 4)
#define RTE_PVALID (psubstruct + 8)
#define RTE_TXTLEN (psubstruct + 24)
#define RTE_TMPLEN (psubstruct + 26)

#define RIB_PMASK (psubstruct + 0)	/* Long pointers in ICONBLK	*/
#define RIB_PDATA (psubstruct + 4)
#define RIB_PTEXT (psubstruct + 8)

#define RBI_PDATA (psubstruct + 0)	/* Long pointer in BITBLK	*/
#define RBI_WB (psubstruct + 4)
#define RBI_HL (psubstruct + 6)
					/* in global array		*/
#define APP_LOPNAME (rs_global + 12)
#define APP_LO1RESV (rs_global + 16)
#define APP_LO2RESV (rs_global + 20)



	WORD read_68K( merge )
	BOOLEAN		merge;
	/* simulate IBM PC data storage in "*.def" files. */
	{
	UWORD	idx, old_ndxno, rd_ndxno, rd_val, rd_kind;

	if ( merge )	 
	   old_ndxno = rcs_ndxno;
	else
	   old_ndxno = 0;

	dos_lseek( rcs_dhndl, 0, 0x0L);
	if (DOS_ERR)
	   return( rcs_ndxno = 0 );
	if(!dos_read( rcs_dhndl, 2, &rd_ndxno))
	   return( rcs_ndxno = 0);
	if (DOS_ERR)
	   return ( rcs_ndxno = 0 );
	else
	   rcs_ndxno = swap_bytes( rd_ndxno );
	rcs_ndxno = old_ndxno + rcs_ndxno;
	for ( idx = old_ndxno; idx < rcs_ndxno; idx++)
	    {
	    if(!dos_read( rcs_dhndl, 2, &rd_val))
	 	return( rcs_ndxno = 0 );
	    rcs_index[idx].val = (BYTE *) ( LW(swap_bytes(rd_val)) & 0xffffL );
	    if( !dos_read( rcs_dhndl, 2, &rd_kind))
		return( rcs_ndxno = 0 );
	    rcs_index[idx].kind = swap_bytes( rd_kind );
	    if(!dos_read( rcs_dhndl, 10, &rcs_index[idx].name[0] ))
		return( rcs_ndxno = 0 );
	    }
	}

	
	WORD open_files(def)
	WORD	def;
	{
	WORD	ii;

	if (!def)
	if (!get_file())
		return (FALSE);
	FOREVER
		{
		rcs_rhndl = dos_open( ADDR(&rcs_rfile[0]), 0);
		if (!DOS_ERR)
			break;
		if (!form_error(DOS_AX))
			return(FALSE);
		}
	FOREVER
		{
		rcs_dhndl = dos_open( ADDR(&rcs_dfile[0]), 0);
		if (!DOS_ERR)
			return (TRUE);
		ii = hndl_alert(2, string_addr(STNODEF));
		if (ii == 1)
			{
			dos_close( rcs_rhndl );
			return (FALSE);
			}
		if (ii == 2)
			{
			rcs_dhndl = FALSE;
			return (TRUE);
			}
		}
	}

LONG get_sub(UWORD rsindex, WORD rtype, WORD rsize)
{
	UWORD		offset;

	offset = LWGET( rs_hdr + LW(rtype*2) );
						/* get base of objects	*/
						/*   and then index in	*/
	return( rs_hdr + LW(offset) + LW(rsize * rsindex) );
}


/*
 *	return address of given type and index, INTERNAL ROUTINE
*/
LONG get_addr(WORD rstype, WORD rsindex)
{
	LONG		psubstruct;
	WORD		size;
	WORD		rt;
	WORD		valid;

	valid = TRUE;
	switch(rstype)
	{
	  case R_OBJECT:
		rt = RT_OB;
		size = sizeof(OBJECT);
		break;
	  case R_TEDINFO:
	  case R_TEPTEXT:
		rt = RT_TEDINFO;
		size = sizeof(TEDINFO);
		break;
	  case R_ICONBLK:
	  case R_IBPMASK:
		rt = RT_ICONBLK;
		size = sizeof(ICONBLK);
		break;
	  case R_BITBLK:
	  case R_BIPDATA:
		rt = RT_BITBLK;
		size = sizeof(BITBLK);
		break;
	  case R_OBSPEC:
		psubstruct = get_addr(R_OBJECT, rsindex);
		return( ROB_SPEC );
	  case R_TEPTMPLT:
	  case R_TEPVALID:
		psubstruct = get_addr(R_TEDINFO, rsindex);
	  	if (rstype == R_TEPTMPLT)
	  	  return( RTE_PTMPLT );
else return( RTE_PVALID );
	  case R_IBPDATA:
	  case R_IBPTEXT:
	  	psubstruct = get_addr(R_ICONBLK, rsindex);
	 	if (rstype == R_IBPDATA)
		  return( RIB_PDATA );
else return( RIB_PTEXT );
	  case R_STRING:
		return( LLGET( get_sub(rsindex, RT_FREESTR, sizeof(LONG)) ) );
	  case R_IMAGEDATA:
		return( LLGET( get_sub(rsindex, RT_FREEIMG, sizeof(LONG)) ) );
	  case R_FRSTR:
		rt = RT_FREESTR;
		size = sizeof(LONG);
		break;
	  case R_FRIMG:
		rt = RT_FREEIMG;
		size = sizeof(LONG);
		break;
	  default:
		valid = FALSE;
		break;
	}
	if (valid)
	  return( get_sub(rsindex, rt, size) );
else return(-1L);
} /* get_addr() */


WORD fix_long(LONG plong)
{
	LONG		lngval;

	lngval = LLGET(plong);
	if (lngval != -1L)
	{
	  LLSET(plong, rs_hdr + lngval);
	  return(TRUE);
	}
else return(FALSE);
}


VOID fix_trindex(VOID)
{
	WORD		ii;
	LONG		ptreebase;

	ptreebase = get_sub(0, RT_TRINDEX, sizeof(LONG));

	for (ii = NUM_TREE-1; ii >= 0; ii--)
	  fix_long(ptreebase + LW(ii*4));
}		     


WORD fix_ptr(WORD type, WORD index)
{
	return( fix_long( get_addr(type, index) ) );
}


VOID fix_objects(VOID)
{
	WORD		ii;
	WORD		obtype, obflags;
	LONG		psubstruct;


	for (ii = NUM_OBS-1; ii >= 0; ii--)
	{
	  psubstruct = get_addr(R_OBJECT, ii);
	  rs_obfix(psubstruct, 0);
	  obtype = LLOBT( LWGET( ROB_TYPE ) );
	  switch (obtype)
	  {
		case G_TEXT:
		case G_BOXTEXT:
		case G_FTEXT:
		case G_FBOXTEXT:
		case G_TITLE:
		case G_ICON:
		case G_IMAGE:
		case G_STRING:
		case G_BUTTON:
			fix_long(ROB_SPEC);
			break;
		default:
			break;
	  }
	obflags = LWGET(ROB_FLAGS);	/* zap LASTOB's */
	LWSET(ROB_FLAGS, obflags & ~LASTOB);
	}
}

VOID fix_tedinfo(VOID)
{
	WORD		ii, i;
	LONG		psubstruct, tl[2], ls[2];


	for (ii = NUM_TI-1; ii >= 0; ii--)
	{
	  psubstruct = get_addr(R_TEDINFO, ii);
	  tl[0] = tl[1] = 0x0L;
	  if (fix_ptr(R_TEPTEXT, ii) )
	  {
	    tl[0] = RTE_TXTLEN;
	    ls[0] = RTE_PTEXT;
	  }
	  if (fix_ptr(R_TEPTMPLT, ii) )
	  {
	    tl[1] = RTE_TMPLEN;
	    ls[1] = RTE_PTMPLT;
	  }
	  for(i=0; i<2; i++)
	  {
	    if (tl[i])
	      WORD LSTRLEN(BYTE *text);
	  }
	  fix_ptr(R_TEPVALID, ii);
	}
}
VOID fix_iconblk(VOID)
{
	WORD		ii;

	for (ii = NUM_IB-1; ii >= 0; ii--)
	{
	  fix_ptr(R_IBPMASK, ii);
	  fix_ptr(R_IBPDATA, ii);
	  fix_ptr(R_IBPTEXT, ii);
	}
}

VOID fix_bitblk(VOID)
{
	WORD	ii;

	for (ii = NUM_BB-1; ii >= 0; ii--)
	  fix_ptr(R_BIPDATA, ii);
}


VOID fix_frstr(WORD type, WORD index)
{
	WORD		ii;

	for (ii = NUM_FRSTR-1; ii >= 0; ii--)
	  fix_ptr(R_FRSTR, ii);
}

VOID fix_frimg(VOID)
{
	WORD	ii;

	for (ii = NUM_FRIMG-1; ii >= 0; ii--)
	  fix_ptr(R_FRIMG, ii);
}
       
	    
	VOID fix_all()
	{
	fix_trindex();
	fix_objects();
	fix_tedinfo();
	fix_iconblk();
	fix_bitblk();
	fix_frstr();
	fix_frimg();
	}   

	VOID comp_str(hdr)
	LONG	hdr;
	{
	LONG	frstr, where, tree, maddr;
	WORD	istr, nstr, lstr, w, h, obj, ndex;
	BYTE	name[9];

	if ( !(nstr = LWGET(RSH_NSTRING(hdr))) )
		return;
	frstr = hdr + LW( LWGET(RSH_FRSTR(hdr)) );

	ini_tree(&maddr, NEWPANEL);
	tree = copy_tree(maddr, ROOT, TRUE);
	add_trindex(tree);
	ini_tree(&maddr, FREEPBX);

	for (istr = 0; istr < nstr; istr++)
		{
		where = LLGET(frstr + (UWORD) (istr * sizeof(LONG)));
		if (where != -1L)
			{
			lstr = LSTRLEN(where);
			obj = copy_objs(maddr, PBXSTR, tree, FALSE);
			objc_add(tree, ROOT, obj);
			SET_SPEC(tree, obj, where);
			SET_WIDTH(tree, obj, gl_wchar * lstr);
			if ((ndex = find_value((BYTE *) where)) != NIL)
				{
				set_value(ndex, (BYTE *) (tree + 
					(UWORD) (obj * sizeof(OBJECT))));
				set_kind(ndex, OBJKIND);
				}
			}
		}
	SET_WIDTH(tree, ROOT, 1);	/* fool arrange_tree into putting */
	arrange_tree(tree, 2 * gl_wchar, gl_hchar, &w, &h);
	SET_HEIGHT(tree, ROOT, h);	/* each on a different line */
	WORD max(WORD a, WORD b);
	unique_name(&name[0], "FRSTR%W", 1);	/* make up a name */
	new_index((BYTE *)tree, FREE, name);
	LWSET(RSH_NSTRING(hdr), 0);
	}

	VOID comp_img(hdr)
	LONG	hdr;
	{
	LONG	frimg, where, tree, maddr;
	WORD	iimg, nimg, w, h, obj, ndex;
	BYTE	name[9];

	if ( !(nimg = LWGET(RSH_NIMAGES(hdr))) )
		return;
	frimg = hdr + LW( LWGET(RSH_FRIMG(hdr)) );

	ini_tree(&maddr, NEWPANEL);
	tree = copy_tree(maddr, ROOT, TRUE);
	add_trindex(tree);
	ini_tree(&maddr, FREEPBX);

	for (iimg = 0; iimg < nimg; iimg++)
		{
		where = LLGET(frimg + (UWORD) (iimg * sizeof(LONG)));
		obj = copy_objs(maddr, PBXIMG, tree, FALSE);
		objc_add(tree, ROOT, obj);
		SET_SPEC(tree, obj, where);
		SET_HEIGHT(tree, obj, LWGET(BI_HL(where)));
		SET_WIDTH(tree, obj, LWGET(BI_WB(where)) << 3);
		if ((ndex = find_value((BYTE *) where)) != NIL)
			{
			set_value(ndex, (BYTE *) (tree + 
				(UWORD) (obj * sizeof(OBJECT))));
			set_kind(ndex, OBJKIND);
			}
		}
	SET_WIDTH(tree, ROOT, full.g_w);	
	arrange_tree(tree, 2 * gl_wchar, gl_hchar, &w, &h);
	SET_HEIGHT(tree, ROOT, h);	
	SET_WIDTH(tree, ROOT, w);
	map_tree(tree, ROOT, NIL, trans_obj);
	unique_name(&name[0], "FRIMG%W", 1);	/* make up a name */
	new_index((BYTE *)tree, FREE, name);
	LWSET(RSH_NIMAGES(hdr), 0);
	}

	WORD read_files()
	{
	WORD	ii, where;
	WORD	ntree, nobj;
	BYTE	name[9];
	if ( ad_clip ) 
		clr_clip();
	ini_buff();
	if ( !dmcopy(rcs_rhndl, 0x0L, head, sizeof(RSHDR)))
	   DOS_ERR = TRUE;
	if (DOS_ERR)
		{	  
		dos_close(rcs_rhndl);
		ini_buff();
		return (FALSE);
		}
	if (avail_mem() < LW( LWGET(RSH_RSSIZE(head)) )) 
		{
		hndl_alert(1, string_addr(STNROOM) );
		ini_buff();
		return (FALSE);
		}
	if(!dmcopy(rcs_rhndl, (LONG) sizeof(RSHDR), head + sizeof(RSHDR),
		LWGET(RSH_RSSIZE(head))))
		DOS_ERR = TRUE;
	if (DOS_ERR)
		{
		dos_close(rcs_rhndl);
		ini_buff();
		return (FALSE);
		}
	dos_close(rcs_rhndl);
	rcs_free = (head + LW( LWGET(RSH_RSSIZE(head)) ) );

#if	  MC68K   /* memory can only be written on even boundary */
	if (rcs_free & 0x1) 
	    rcs_free++;
#endif

	rs_hdr = head;
	fix_all();
	map_all(trans_obj);

	if (!rcs_dhndl)
		rcs_ndxno = 0;
	else

#if	MC68K
		read_68k( FALSE );
#else
		{
		if (!dmcopy (rcs_dhndl, 0x0L, ADDR(&rcs_ndxno), 2))
		    DOS_ERR = TRUE;	
		if (DOS_ERR)
			rcs_ndxno = 0;
		else
			{
			if (!dmcopy(rcs_dhndl, 0x02L, ADDR(&rcs_index),
				sizeof(INDEX) * rcs_ndxno))
			        DOS_ERR = TRUE;
			if (DOS_ERR)
				rcs_ndxno = 0;
			}
		}
#endif

	dos_close(rcs_dhndl);
			/* convert stored values to addresses */
	for (ii = 0; ii < rcs_ndxno; ii++)
		{
		switch ( get_kind(ii) ) {
			case UNKN:
			case PANL:
			case DIAL:
			case MENU:
				set_value(ii, (BYTE *) tree_addr(
					(WORD) get_value(ii)) );
				break;
			case ALRT:
			case FRSTR:
				set_value(ii, (BYTE *) str_addr(
					(WORD) get_value(ii)) );
				break;
			case FRIMG:
				set_value(ii, (BYTE *) img_addr(
					(WORD) get_value(ii)) );
				break;
			case OBJKIND:
				nobj = (WORD) LLOBT( (UWORD) get_value(ii) ) & 0xff;
				ntree = (WORD) LHIBT( (UWORD) get_value (ii) ) & 0xff;
				set_value(ii, (BYTE *) (tree_addr(ntree) + 
					(UWORD) (nobj * sizeof(OBJECT))));
				break;
			default:
				break;
			}
		}

	comp_alerts(head);	/* convert freestrs into alert trees */
	comp_str(head);	/* scavenge all other freestrs	*/
	comp_img(head);	/* and likewise for free images */
			/* make sure all trees are indexed */
	for (ii = 0; ii < LWGET(RSH_NTREE(head)); ii++)
		if (find_tree(ii) == NIL)
			{
			unique_name(name, "TREE%W", ii + 1);
			where = new_index((BYTE *) tree_addr(ii), UNKN, name);
			}

	return LWGET(RSH_RSSIZE(head));
	}

	WORD merge_files()
	{
	UWORD	foo, indoff, rd_val, rd_kind;
	WORD	total, i, ii;
	WORD	nobj, ntree, old_ntree;
	LONG	merge, oldndx, newndx, old_ndxno, frstr, frimg, here;

	merge = get_mem(sizeof(RSHDR));
	if(!dmcopy(rcs_rhndl, 0x0L, merge, sizeof(RSHDR) ))
	    DOS_ERR = TRUE;
	if (DOS_ERR)
		{ 
		dos_close(rcs_rhndl);
		return (FALSE);
		}
	if (rcs_low || avail_mem() < (UWORD) LWGET(RSH_RSSIZE(merge)) - 
		(UWORD) sizeof(RSHDR) )
		{
		hndl_alert(1, string_addr(STNROOM));
		return (FALSE);
		}
			/* synchronize objects */
	foo = (UWORD) (( merge + LW( LWGET(RSH_OBJECT(merge)) ) ) -
		( head + LW( LWGET(RSH_OBJECT(head)) ) ));
	foo %= sizeof(OBJECT);
	if (foo) 
		{
		get_mem(foo = sizeof(OBJECT) - foo);
		merge = merge + foo;
		if(!dmcopy(rcs_rhndl, 0x0L, merge, sizeof(RSHDR) ))
		      DOS_ERR = TRUE;
		} 

	here = get_mem((UWORD) LWGET(RSH_RSSIZE(merge)) - 
		(UWORD) sizeof(RSHDR));
	if(!dmcopy (rcs_rhndl, (LONG) sizeof(RSHDR), 
		here, LWGET(RSH_RSSIZE(merge))))
	        DOS_ERR = TRUE;
	if (DOS_ERR)
		{
		dos_close(rcs_rhndl);
		return (FALSE);
		}
	dos_close(rcs_rhndl);
	rs_hdr = merge;
	fix_all();
	
	old_ntree = LWGET(RSH_NTREE(head));
	if (LWGET(RSH_NTREE(merge)))
		{
		total = LWGET(RSH_NTREE(head)) + LWGET(RSH_NTREE(merge));
		newndx = get_mem(sizeof(LONG) * total);
		indoff = (UWORD) (newndx - head);		
		for (i = 0; i < LWGET(RSH_NTREE(head)); i++)
			{
			LLSET(newndx, tree_addr(i));
			newndx += sizeof(LONG);
			}
		oldndx = merge + LW( LWGET(RSH_TRINDEX(merge)) );
		for (i = 0; i < LWGET(RSH_NTREE(merge)); i++)
			{
			LLSET(newndx, LLGET(oldndx));
			newndx += sizeof(LONG);
			oldndx += sizeof(LONG);
			}
		LWSET(RSH_NTREE(head), total);
		LWSET(RSH_TRINDEX(head), indoff);
		}

	if (rcs_dhndl)

		{  


#if	MC68K				      
		{	
		old_ndxno = rcs_ndxno;
		read_68k( TRUE );
		total = rcs_ndxno;
		rcs_ndxno = old_ndxno;
		}
#else
		{
		old_ndxno = rcs_ndxno;		
		if(!dmcopy(rcs_dhndl, 0x0L, ADDR(&total), sizeof(WORD) ))
	            DOS_ERR = TRUE;
		if (DOS_ERR)
			total = 0;
		else
			{
			if(!dmcopy(rcs_dhndl, 0x02L, ADDR(&rcs_index[old_ndxno]),
				sizeof(INDEX) * total))
				DOS_ERR = TRUE;
			if (DOS_ERR)
				total = 0;
			else
				total = total + old_ndxno;
			}
		}
#endif

		for ( ii = old_ndxno; ii < total; ii++ )				
			{	
			if (find_name(&rcs_index[ii].name[0]) != NIL)
			if (tree_kind(rcs_index[ii].kind))
				unique_name(&rcs_index[ii].name[0], "TREE%W", old_ntree);
else unique_name(&rcs_index[ii].name[0], "OBJ%W", 0);
			switch (rcs_index[ii].kind) {
				case UNKN:
				case PANL:
				case DIAL:
				case MENU:
					rcs_index[ii].val = (BYTE *) tree_addr(
						(WORD) rcs_index[ii].val + old_ntree);
					break;
				case ALRT:
				case FRSTR:
					frstr = merge +	LW( LWGET(RSH_FRSTR(merge)) );
					rcs_index[ii].val = (BYTE *) LLGET(frstr + 
						(UWORD) (sizeof(LONG) * (WORD) rcs_index[ii].val));
					break;
				case FRIMG:
					frimg = merge + LW( LWGET(RSH_FRIMG(merge)) );
					rcs_index[ii].val = (BYTE *) LLGET(frimg +
						(UWORD) (sizeof(LONG) * (WORD)rcs_index[ii].val));
					break;

				case OBJKIND:
					nobj = (WORD) LLOBT( (UWORD) rcs_index[ii].val) & 0xff;
					ntree = (WORD) LHIBT( (UWORD) rcs_index[ii].val) & 0xff;
					rcs_index[ii].val = (BYTE *) (tree_addr(ntree + old_ntree) 
						+ (UWORD) (nobj * sizeof(OBJECT))); 
					break;
				default:
					break;
				}
			if (new_index(rcs_index[ii].val, rcs_index[ii].kind, &rcs_index[ii].name[0]) == NIL)
				break;
			}
		dos_close(rcs_dhndl);
		}


	for (i = old_ntree; i < LWGET(RSH_NTREE(head)); i++)
		{
		map_tree(tree_addr(i), ROOT, NIL, trans_obj); 
		if (find_tree(i) == NIL)
			{
			unique_name(&rcs_index[i].name[0], "TREE%W", i + 1);
			if (new_index((BYTE *) tree_addr(i), UNKN, &rcs_index[i].name[0]) == NIL)
				break;
			}
		}

	comp_alerts(merge);
	comp_str(merge);
	comp_img(merge);
	return (TRUE);
	}
