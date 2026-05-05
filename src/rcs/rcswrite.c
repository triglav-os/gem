/*
 * Implements resource-file serialization for the resource construction
 * set. The code turns the editor in-memory structures back into persistent
 * resource data.
 *
 * MIT License (see: LICENSE)
 * Copyright (C) 2026 tomaz stih
 */

#include "portab.h"
#include "machine.h"
#include "obdefs.h"
#include "dosbind.h"
#include "rcsdefs.h"
#include "rcs.h"
#include "rcsext.h"

typedef	struct	outobj
	{
	WORD	next;
	WORD	head;
	WORD	tail;
	BYTE	*type;
	BYTE	*flags;
	BYTE	*state;
	LONG	spec;
	UWORD	x;
	UWORD	y;
	UWORD	w;
	UWORD	h;
	} OUTOBJ;

typedef struct 	h_out
	{
	BYTE	*p0;
	WORD	p1;
	WORD	p2;
	} HOUT;



	VOID do_tally()
	{
	clr_tally();
	map_all(tally_obj);
	tally_free();
	rcs_tally.nbytes += LWGET(RSH_NTREE(head)) * sizeof(LONG);
	rcs_tally.nbytes += sizeof(RSHDR);
	}


	LONG get_dspace( drv )
	WORD	drv;
	{
	LONG	dsk_tot, dsk_avail;
	dos_space( drv, &dsk_tot, &dsk_avail);
	return ( dsk_avail );
	}

	WORD dspace( drv, nbytes )
	WORD	drv;
	UWORD	nbytes;
	{
	LONG	dsk_tot, dsk_avail;
	dos_space( drv, &dsk_tot, &dsk_avail);
	if ( nbytes < (UWORD) dsk_avail )
		return (FALSE);
	else
		return (TRUE);
	}


/* the following two routines were written to simulate the ibm pc file */
/*   structure for "*.def" files. */

	UWORD swap_bytes( x )
	UWORD	x;
	{
	
	return ( ((UWORD) LLOBT(x) << 8 ) | (UWORD) LHIBT(x) );

	}

	VOID wr_68kdef()
	{
	UWORD	idx, wr_ndxno,wr_val, wr_kind;
	
	wr_ndxno = swap_bytes(rcs_ndxno);
	dos_write( rcs_dhndl, 2, &wr_ndxno );
	for ( idx = 0; idx < rcs_ndxno; idx++ )
	   {
	   wr_val = swap_bytes( (UWORD) rcs_index[idx].val );
	   dos_write( rcs_dhndl, 2, &wr_val );
 	   wr_kind = swap_bytes( rcs_index[idx].kind );
	   dos_write( rcs_dhndl, 2, &wr_kind );
	   dos_write( rcs_dhndl,10, &rcs_index[idx].name[0] );
	   }
	}
	

	WORD c_imgno(addr)
	LONG	addr;
	{
	WORD	imgno;

	if (imgno == NIL)
		return (NIL);
	for (imgno = 0; imgno < c_nimage; imgno++)
		if ( (WORD) addr == c_obndx[imgno + c_nstring])
			return (imgno);	return (NIL);
  	}

	WORD c_strno(addr)
	LONG	addr;
	{
	WORD	strno;

	if (addr == NIL)
		return (NIL);
	for (strno = 0; strno < c_nstring; strno++)
		if ( (WORD) addr == c_obndx[strno])
			return (strno);
	return (NIL);
	}

	VOID c_wrlin()
	{
	dos_write(rcs_hhndl, strlen(hline), ADDR(&hline));
	}

	VOID c_comma(yesno)
	WORD	yesno;
	{
	if (yesno)
		{
		strcpy(",\r\n", hline);
		c_wrlin();
		}
	}

	VOID c_tail(used)
	WORD	used;
	{
	if (!rcs_cflag)
		return;
	if (!used)
		strcpy("0};\r\n", hline);
	else
		strcpy("};\r\n", hline);
	c_wrlin();
	}

	VOID wr_sync()
	{
	if (rcs_wraddr & 0x1)
		{
		dos_write(rcs_rhndl, 1, ADDR("") );
		rcs_wraddr++;
		}
	}

	VOID wr_header()
	{
	dos_lseek(rcs_rhndl, SMODE, 0x0L);
	dos_write(rcs_rhndl, sizeof(RSHDR), head);
	}

	VOID c_defline(name, val)
	BYTE	*name;
	WORD	val;
	{
	merge_str(hline, "#define %S %W\r\n", &name);
	c_wrlin();
	}

	VOID c_baseline(num, field, val)
	WORD	num, val;
	BYTE	*field;
	{
	BYTE	name[9];

	merge_str(name,"T%W%S", &num);
	c_defline(name, val);
	}

	VOID c_bases()
	{
	WORD	i;

	if (!rcs_cflag)
		return;
	clr_tally();
	for (i = 0; i < LWGET(RSH_NTREE(head)); i++)
		{
		c_baseline(i, "OBJ", rcs_tally.nobj);
/*		c_baseline(i, "IB", rcs_tally.nib);	*/
/*		c_baseline(i, "BB", rcs_tally.nbb);	*/
/*		c_baseline(i, "TI", rcs_tally.nti);	*/
/*		c_baseline(i, "IMG", rcs_tally.nimg);	*/
/*		c_baseline(i, "STR", rcs_tally.nstr);	*/
		map_tree( tree_addr(i), ROOT, NIL, tally_obj);
		}
	c_defline("FREEBB", rcs_tally.nbb);
	c_defline("FREEIMG", rcs_tally.nimg);
	c_defline("FREESTR", rcs_tally.nstr);
	}

	VOID c_strhead()
	{
	if (!rcs_cflag)
		return;
	strcpy("\r\nBYTE *rs_strings[] = {\r\n", hline);
	c_wrlin();
	c_nstring = 0;
	}

	VOID c_string(addr)
	LONG	addr;
	{
	WORD	hsub;
	BYTE	hchar;

	if (!rcs_cflag)
		return; 
	c_comma(c_nstring);

	hline[0] = '"';
	hsub = 1;

	do {
		hchar = LBGET(addr++);
		if (hchar == '"')
			hline[hsub++] = '\\';
		else if (hchar == '\\')
			hline[hsub++] = '\\';
		if (!hchar)
			hline[hsub++] = '"';
		hline[hsub++] = hchar;
		if (hchar && hsub == 70)
			{
			hline[hsub] = '\0';
			strcat("\\\r\n", hline);
			c_wrlin();
			hsub = 0;
			}
		} while (hchar);

	c_wrlin();
	c_obndx[c_nstring++] = rcs_wraddr;
	}

	VOID wr_str(where)
	LONG	where;
	{
	LONG	staddr;
	WORD	stlen;

	if ( (staddr = LLGET(where)) == -1L)
		return;
	c_string(staddr);
	LLSET(where, (LONG) rcs_wraddr & 0xffff);
	stlen = 1 + LSTRLEN(staddr);
	dos_write(rcs_rhndl, stlen, staddr);
	rcs_wraddr += stlen;
	}

	VOID wr_string(tree, which)
	LONG	tree;
	WORD	which;
	{
	LONG	iconblk, tedinfo;
	WORD	type;

	type = LLOBT(GET_TYPE(tree, which));
	if (type == G_BUTTON || type == G_STRING || type == G_TITLE)
		{
		wr_str(OB_SPEC(which));
		return;
		}
	if (type == G_ICON)
		{
		if ( (iconblk = GET_SPEC(tree, which)) != -1L)
			wr_str(IB_PTEXT(iconblk));
		return;
		}
	if (type == G_TEXT || type == G_BOXTEXT ||
		type == G_FTEXT || type == G_FBOXTEXT) 
		{
		if ( (tedinfo = GET_SPEC(tree, which)) == -1L)
			return;
		wr_str(tedinfo);
		wr_str(TE_PTMPLT(tedinfo));
		wr_str(TE_PVALID(tedinfo));
		}
	}

	VOID map_frstr()
	{
	WORD	ifree, nfree;

	c_frstr = c_nstring;
	if ( !(nfree = LWGET(RSH_NSTRING(head))) )
		return;
	for (ifree = 0; ifree < nfree; ifree++)
		wr_str(str_ptr(ifree));
	}

	VOID c_frshead()
	{
	if (!rcs_cflag)
		return;
	strcpy("\r\nLONG rs_frstr[] = {\r\n", hline);
	c_wrlin();
	c_nfrstr = 0;
	}

	VOID c_freestr(which)
	WORD	which;
	{
	if (!rcs_cflag)
		return;
	c_comma(c_nfrstr);
	which += c_frstr;
	merge_str(hline, "%WL", &which);
	c_wrlin();
	c_nfrstr++;
	}

	VOID wr_freestr()
	{
	WORD	ifree, nfree;

	if ( !(nfree = LWGET(RSH_NSTRING(head))) )
		return;
	for (ifree = 0; ifree < nfree; c_freestr(ifree++));
	nfree *= sizeof(LONG);
	dos_write(rcs_rhndl, nfree, str_ptr(0));
	LWSET(RSH_FRSTR(head), rcs_wraddr);
	rcs_wraddr += nfree;
	}

	VOID c_imdata(addr, size)
	LONG	addr;
	WORD	size;
	{
	WORD	iwd, vwd;

	if (!rcs_cflag)
		return;
	merge_str(hline, "\r\nWORD IMAG%W[] = {", &c_nimage);
	c_wrlin();

	for (iwd = 0; iwd < size; iwd += 2)
		{
		if (iwd)
			{
			strcpy(", ", hline);
			c_wrlin();
			}
		if (iwd % 8 == 0)
			{
			strcpy("\r\n", hline);
			c_wrlin();
			}
		vwd = LWGET(addr + iwd);
		merge_str(hline, "0x%H", &vwd);
		c_wrlin();
		}

	c_tail(TRUE);
	c_obndx[c_nstring + c_nimage++] = rcs_wraddr;
	}

	VOID wr_imdata(where, w, h)
	LONG	where;
	WORD	w, h;
	{
	WORD	size;
	LONG	imaddr;

	if ( (imaddr = LLGET(where)) == -1L)
		return;
	size = w * h;
	gsx_untrans(imaddr, w, ADDR(wr_obndx), w, h);
	imaddr = ADDR(wr_obndx);
	c_imdata(imaddr, size);
	LLSET(where, (LONG) rcs_wraddr & 0xffff);
	dos_write(rcs_rhndl, size, imaddr);
	rcs_wraddr += size;
	}

	VOID wr_image(tree, which)
	LONG	tree;
	WORD	which;
	{
	LONG	iconblk, bitblk;
	WORD	type, w, h;

	type = LLOBT(GET_TYPE(tree, which));
	if (type == G_IMAGE)
		{
		if ( (bitblk = GET_SPEC(tree, which)) == -1L)
			return;
		w = LWGET(BI_WB(bitblk));
		h = LWGET(BI_HL(bitblk));
		wr_imdata(BI_PDATA(bitblk), w, h);
		return;
		}
	if (type == G_ICON)
		{
		if ( (iconblk = GET_SPEC(tree, which)) == -1L)
			return;
		w = (LWGET(IB_WICON(iconblk)) + 7) / 8;
		h = LWGET(IB_HICON(iconblk));
		wr_imdata(IB_PMASK(iconblk), w, h);
		wr_imdata(IB_PDATA(iconblk), w, h);
		}
	}

	VOID map_frimg()
	{
	WORD	ifree, nfree, w, h;
	LONG	bitblk;

	if ( !(nfree = LWGET(RSH_NIMAGES(head))) )
		return;
	for (ifree = 0; ifree < nfree; ifree++)
		{
		if ( (bitblk = img_addr(ifree)) == -1L)
			break;
		w = LWGET(BI_WB(bitblk));
		h = LWGET(BI_HL(bitblk));
		wr_imdata(bitblk, w, h);
		}
	}

	VOID c_foobar()
	{
	WORD	img;

	if (!rcs_cflag)
		return;
	strcpy("\r\nstruct foobar {\r\n\tWORD\tdummy;", hline);
	c_wrlin();
	strcpy("\r\n\tWORD\t*image;\r\n\t} rs_imdope[] = {\r\n",hline);
	c_wrlin();

	for (img = 0; img < c_nimage; img++)
		{
		c_comma(img);
		merge_str(hline, "0, &IMAG%W[0]", &img);
		c_wrlin();
		}

	c_tail(img);
	}

	VOID c_iconhead()
	{
	if (!rcs_cflag)
		return;
	strcpy("\r\nICONBLK rs_iconblk[] = {\r\n", hline);
	c_wrlin();
	c_nib = 0;
	}

	VOID c_iconblk(addr)
	LONG	addr;
	{
	ICONBLK	here;

	if (!rcs_cflag)
		return;
	BYTE LBCOPY(LONG dst, LONG src, WORD count);
	here.ib_pdata = (LONG) c_imgno(here.ib_pdata);
	here.ib_pmask = (LONG) c_imgno(here.ib_pmask);
	here.ib_ptext = (LONG) c_strno(here.ib_ptext);
	c_comma(c_nib);
	merge_str(hline, "%LL, %LL, %LL, %W,%W,%W, %W,%W,%W,%W, %W,%W,%W,%W",
		&here);
	c_wrlin();
	c_nib++;
	}

	VOID wr_iconblk(tree, which)
	LONG	tree;
	WORD	which;
	{
	LONG	iconblk;

	if (LLOBT(GET_TYPE(tree, which)) != G_ICON)
		return;
	if ( (iconblk = GET_SPEC(tree, which)) == -1L)
		return;
	SET_SPEC(tree, which, (LONG) rcs_wraddr & 0xffff);
	dos_write(rcs_rhndl, sizeof(ICONBLK), iconblk);
	rcs_wraddr += sizeof(ICONBLK);
	LWINC(RSH_NIB(head));
	c_iconblk(iconblk);
	}

	VOID c_bithead()
	{
	if (!rcs_cflag)
		return;
	strcpy("\r\nBITBLK rs_bitblk[] = {\r\n", hline);
	c_wrlin();
	c_nbb = 0;
	}

	VOID c_bitblk(addr)
	LONG	addr;
	{
	BITBLK	here;

	if (!rcs_cflag)
		return;
	BYTE LBCOPY(LONG dst, LONG src, WORD count);
	here.bi_pdata = (LONG) c_imgno(here.bi_pdata);
	c_comma(c_nbb);
	merge_str(hline, "%LL, %W, %W, %W, %W, %W", &here);
	c_wrlin();
	c_nbb++;
	}

	VOID wr_bitblk(tree, which)
	LONG	tree;
	WORD	which;
	{
	LONG	bitblk;

	if (LLOBT(GET_TYPE(tree, which)) != G_IMAGE)
		return;
	if ( (bitblk = GET_SPEC(tree, which)) == -1L)
		return;
	SET_SPEC(tree, which, (LONG) rcs_wraddr & 0xffff);
	dos_write(rcs_rhndl, sizeof(BITBLK), bitblk);
	rcs_wraddr += sizeof(BITBLK);
	LWINC(RSH_NBB(head));
	c_bitblk(bitblk);
	}

	VOID map_frbit()
	{
	WORD	ifree, nfree;
	LONG	bitblk;

	if ( !(nfree = LWGET(RSH_NIMAGES(head))) )
		return;
	for (ifree = 0; ifree < nfree; ifree++)
		{
		if ( (bitblk = img_addr(ifree)) == -1L)
			break;
		LLSET(img_ptr(ifree), (LONG) rcs_wraddr & 0xffff);
		dos_write(rcs_rhndl, sizeof(BITBLK), bitblk);
		rcs_wraddr += sizeof(BITBLK);
		LWINC(RSH_NBB(head));
		c_bitblk(bitblk);
		}
	} 

	VOID c_frbhead()
	{
	if (!rcs_cflag)
		return;
	strcpy("\r\nLONG rs_frimg[] = {\r\n", hline);
	c_wrlin();
	c_nfrbit = 0;
	}

	VOID c_frb(addr)
	LONG	addr;
	{
	WORD	blkno;

	if (!rcs_cflag)
		return;
	blkno = (WORD) (addr - LWGET(RSH_BITBLK(head))) / sizeof(BITBLK);
	c_comma(c_nfrbit);
	merge_str(hline, "%WL", &blkno);
	c_wrlin();
	c_nfrbit++;
	}

	VOID wr_frbit()
	{
	WORD	ifree, nfree;

	if ( !(nfree = LWGET(RSH_NIMAGES(head))) )
		return;
	for (ifree = 0; ifree < nfree; ifree++)
		c_frb(img_addr(ifree));
	nfree *= sizeof(LONG);
	dos_write(rcs_rhndl, nfree, img_ptr(0));
	LWSET(RSH_FRIMG(head), rcs_wraddr);
	rcs_wraddr += nfree;
	} 

	VOID c_tedhead()
	{
	if (!rcs_cflag)
		return;
	strcpy("\r\nTEDINFO rs_tedinfo[] = {\r\n", hline);
	c_wrlin();
	c_nted = 0;
	}

	VOID c_tedinfo(addr)
	LONG	addr;
	{
	TEDINFO	here;

	if (!rcs_cflag)
		return;
	BYTE LBCOPY(LONG dst, LONG src, WORD count);
	here.te_ptext = (LONG) c_strno(here.te_ptext);
	here.te_pvalid = (LONG) c_strno(here.te_pvalid);
	here.te_ptmplt = (LONG) c_strno(here.te_ptmplt);
	c_comma(c_nted);
	merge_str(hline, "%LL, %LL, %LL, %W, %W, %W, 0x%H, 0x%H, %W, %W,%W", 
		&here);
	c_wrlin();
	c_nted++;
	}
	
	VOID wr_tedinfo(tree, which)
	LONG	tree;
	WORD	which;
	{
	LONG	tedinfo;
	WORD	type;

	type = LLOBT(GET_TYPE(tree, which));
	if ( !(type == G_TEXT || type == G_BOXTEXT || 
		type == G_FTEXT || type == G_FBOXTEXT)) 
		return;
	if ( (tedinfo = GET_SPEC(tree, which)) == -1L)
		return;
	SET_SPEC(tree, which, (LONG) rcs_wraddr & 0xffff);
	dos_write(rcs_rhndl, sizeof(TEDINFO), tedinfo);
	rcs_wraddr += sizeof(TEDINFO);
	LWINC(RSH_NTED(head));
	c_tedinfo(tedinfo);
	}

	VOID c_objhead()
	{
	if (!rcs_cflag)
		return;
	strcpy("\r\nOBJECT rs_object[] = {\r\n", hline);
	c_wrlin();
	c_nobs = 0;
	}

	VOID c_object(tree)
	LONG	tree;
	{

	OUTOBJ	here;
	BYTE	type[10], state[10], flags[10];

	if (!rcs_cflag)
		return;
	BYTE LBCOPY(LONG dst, LONG src, WORD count);
	BYTE LBCOPY(LONG dst, LONG src, WORD count);

	switch (LLOBT(GET_TYPE(tree, ROOT))) {
		case G_STRING:
		case G_BUTTON:
		case G_TITLE:
			here.spec = (LONG) c_strno(here.spec);
			break;
		case G_TEXT:
		case G_FTEXT:
		case G_BOXTEXT:
		case G_FBOXTEXT:
			here.spec = ( (WORD) here.spec - LWGET(RSH_TEDINFO(head)) )
				/ sizeof(TEDINFO);
			break;
		case G_IMAGE:
			here.spec = ( (WORD) here.spec - LWGET(RSH_BITBLK(head)) )
				/ sizeof(BITBLK);
			break;
		case G_ICON:
			here.spec = ( (WORD) here.spec - LWGET(RSH_ICONBLK(head)) )
				/ sizeof(ICONBLK);
			break;
		default:
			break;
		}

	c_comma(c_nobs);
	here.type = c_lookup(LLOBT(GET_TYPE(tree, ROOT)), c_types, N_TYPES, type);
	here.flags = c_lookup(GET_FLAGS(tree, ROOT), c_flags, 
		N_FLAGS, flags);
	here.state = c_lookup(GET_STATE(tree, ROOT), c_states, 
		N_STATES, state);
	merge_str(hline, "%W, %W, %W, %S, %S, %S, 0x%XL, %W,%W, %W,%W", &here);
	c_wrlin();
	c_nobs++;
	}

	VOID wr_obj(itree, tree, iobj)
	LONG	tree;
	WORD	itree, iobj;	/* refers to position in the preorder 	*/
				/* trace stored in wr_obndx[]		*/
	{
	WORD	entno, where, link, obtype;
	LONG	object;

	where = wr_obndx[iobj];		/* the actual object #	*/

	entno = find_obj(tree, where);
	if (entno != NIL)
		set_value(entno, (BYTE *) ( 
			((itree & 0xFF) << 8) | (iobj & 0xFF) ) );
	if ( (link = GET_NEXT(tree, where)) != -1)
		SET_NEXT(tree, where, fnd_ob(link));
	if ( (link = GET_HEAD(tree, where)) != -1)
		SET_HEAD(tree, where, fnd_ob(link));
	if ( (link = GET_TAIL(tree, where)) != -1)
		SET_TAIL(tree, where, fnd_ob(link));
	obtype = GET_TYPE(tree, where) & 0x00ff; /*low byte*/
	if(obtype == G_ICON || obtype == G_IMAGE) /*0 hi byte*/
		LWSET(OB_TYPE(where), obtype);
	unfix_chpos(OB_X(where), 0);
	unfix_chpos(OB_WIDTH(where), 0);
	unfix_chpos(OB_Y(where), 1);
	unfix_chpos(OB_HEIGHT(where), 1);
	dos_write(rcs_rhndl, sizeof(OBJECT), 
		object = tree + (UWORD) (where * sizeof(OBJECT)));
	rcs_wraddr += sizeof(OBJECT);
	c_object(object);
	}
	
	VOID wr_trees()
	{
	LONG	tree;
	WORD	ntree, itree, iobj, where, flags;

	ntree = LWGET(RSH_NTREE(head));

	for (itree = 0; itree < ntree; itree++)
		{
		where = find_tree(itree);
		set_value(where, (BYTE *) itree);
		tree = tree_addr(itree);
		LLSET(tree_ptr(itree), (LONG) rcs_wraddr & 0xffff);
		wr_obnum = 0;
		map_tree(tree, 0, -1, indx_obs);
		LWSET(RSH_NOBS(head), wr_obnum + LWGET(RSH_NOBS(head)));
		iobj = wr_obndx[wr_obnum - 1];
		flags = GET_FLAGS(tree, iobj);
		SET_FLAGS(tree, iobj, flags | LASTOB);
		for (iobj = 0; iobj < wr_obnum; iobj++)
			wr_obj(itree, tree, iobj);
		}
	}

	VOID c_treehead()
	{
	if (!rcs_cflag)
		return;
	strcpy("\r\nLONG rs_trindex[] = {\r\n", hline);
	c_wrlin();
	c_ntree = 0; 
	}

	VOID c_tree(addr)
	LONG	addr;
	{
	WORD	objno;

	if (!rcs_cflag)
		return;
	objno = ( (WORD) addr - LWGET(RSH_OBJECT(head)) ) / sizeof(OBJECT);
	c_comma(c_ntree);
	merge_str(hline, "%WL", &objno);
	c_wrlin();
	c_ntree++;
	}

	VOID wr_trindex()
	{
	WORD	ntree, itree;

	ntree = LWGET(RSH_NTREE(head));
	dos_write(rcs_rhndl, ntree * sizeof(LONG), tree_ptr(0));

	for (itree = 0; itree < ntree; itree++)
		c_tree(tree_addr(itree));

	LWSET(RSH_TRINDEX(head), rcs_wraddr);	/* Don't move this line! */
	rcs_wraddr += ntree * sizeof(LONG);
	}

	VOID c_defs()
	{
	BYTE	*rptr, *sptr;

	if (!rcs_cflag)
		return;
	strcpy("\r\n", hline);
	c_wrlin();
	c_defline("NUM_STRINGS", c_nstring);
	c_defline("NUM_FRSTR", LWGET(RSH_NSTRING(head)));
	c_defline("NUM_IMAGES", c_nimage);
	c_defline("NUM_BB", LWGET(RSH_NBB(head)));
	c_defline("NUM_FRIMG", LWGET(RSH_NIMAGES(head)));
	c_defline("NUM_IB", LWGET(RSH_NIB(head)));
	c_defline("NUM_TI", LWGET(RSH_NTED(head)));
	c_defline("NUM_OBS", LWGET(RSH_NOBS(head)));
	c_defline("NUM_TREE", LWGET(RSH_NTREE(head)));
	for (sptr = rptr = &rcs_rfile[0]; *sptr; sptr++)
		if (*sptr == '\\' || *sptr == '\:')
			rptr = sptr + 1;
	merge_str(hline, "\r\nBYTE pname[] = \"%S\";", &rptr);
	c_wrlin();
	hline[0] = '\032';
	hline[1] = '\0';
	c_wrlin();
	}

	VOID ctrl_z(f_handle)
	WORD	f_handle;
	{
	dos_write(f_handle, 1, ADDR("\32"));
	}
	   

	VOID wr_include(deftype, ndx, ndef,ptns, trflag)
	BYTE	*ptns[];   
	WORD	deftype, ndx, ndef, trflag;
	{
	WORD	idx, ii;
	HOUT	h;

	for ( idx = 0; idx < ndx; idx++ )
		for ( ii = 0; ii < rcs_ndxno; ii++)
			if((trflag && get_kind(ii) < 4)	|| ( get_kind(ii) == deftype ))
				if ( (WORD) get_value(ii) == idx)
					{
					strup(h.p0 = get_name(ii));
					h.p1 = idx;	
					merge_str(hline, ptns[ndef], &h);
					dos_write(rcs_hhndl,strlen(hline),ADDR(hline));
					break;
					}     
	}
      

	VOID obj_include( ntree,nobs, ptns )
	BYTE	*ptns[];
	WORD	ntree, nobs;
	{
	WORD	itree,  iobs, ii;
	HOUT	h;
	
	for ( itree = 0; itree < ntree; itree++ )
            for ( iobs = 0; iobs < nobs; iobs++)
	    	for ( ii = 0; ii < rcs_ndxno; ii++ )
	    	    if( get_kind(ii) == OBJKIND)
			{
			h.p1 = (WORD) get_value(ii);
			h.p2 = (WORD) LHIBT(h.p1) & 0xff;
			h.p1 = (WORD) LLOBT(h.p1) & 0xff;
			if( h.p2 == itree && h.p1 == iobs)
			       {
			       strup(h.p0 = get_name(ii));
			       merge_str(hline,ptns[2],&h);
			       dos_write(rcs_hhndl, strlen(hline),ADDR(hline));
			       break;
			       } 		
			}
	}


	VOID wrsrt_inclfile(ext,ptns)
	BYTE	*ext;
	BYTE	*ptns[];
	{
	r_to_xfile(rcs_hfile, ext);

	FOREVER
		{
		rcs_hhndl = dos_create(ADDR(&rcs_hfile[0]), 0);
		if (!DOS_ERR)
			break;
		if (!form_error(DOS_AX))
			return;
		}
	wr_include( UNKN,  LWGET(RSH_NTREE(head)), 0, ptns,TRUE );
	obj_include( LWGET(RSH_NTREE(head)), LWGET(RSH_NOBS(head)), ptns );
	wr_include(ALRT, LWGET(RSH_NSTRING(head)), 1, ptns, FALSE);
	wr_include(FRSTR,LWGET(RSH_NSTRING(head)), 3, ptns, FALSE);
	wr_include(FRIMG,LWGET(RSH_NIMAGES(head)),  4, ptns, FALSE);
	ctrl_z(rcs_hhndl);
	dos_close(rcs_hhndl);
	}

	VOID write_inclfile(ext,ptns)
	BYTE	*ext;
	BYTE	*ptns[];
	{
	WORD	ii;
	HOUT	h;

	r_to_xfile(rcs_hfile, ext);

	FOREVER
		{
		rcs_hhndl = dos_create(ADDR(&rcs_hfile[0]), 0);
		if (!DOS_ERR)
			break;
		if (!form_error(DOS_AX))
			return;
		}

	for (ii = 0; ii < rcs_ndxno; ii++)
		{
		strup(h.p0 = get_name(ii));
		h.p1 = (WORD) get_value(ii);
		switch ( get_kind(ii) ) {
			case UNKN:
			case PANL:
			case DIAL:
			case MENU:
				merge_str(hline, ptns[0], &h);
				break;
			case ALRT:
				merge_str(hline, ptns[1], &h);
				break;
			case OBJKIND:
				h.p2 = (WORD) LHIBT(h.p1) & 0xff;
				h.p1 = (WORD) LLOBT(h.p1) & 0xff;
				merge_str(hline, ptns[2], &h);
				break;
			case FRSTR:
				merge_str(hline, ptns[3], &h);
				break;
			case FRIMG:
				merge_str(hline, ptns[4], &h);
				break;
			default:
				break;
			}
		dos_write(rcs_hhndl, strlen(hline), ADDR(hline) );
		}
	ctrl_z(rcs_hhndl);
	dos_close(rcs_hhndl);
	}

	WORD write_files()
	{
	LONG	mem_needs;
	
	do_tally();			 
	if (rcs_cflag)
		mem_needs = 3 * rcs_tally.nbytes;
	else
		mem_needs = (rcs_tally.nbytes >> 1) + rcs_tally.nbytes;

	if( mem_needs > get_dspace( rcs_rfile[0] - 'A' + 1 ) )
           {
	   hndl_alert(1, string_addr(SNDSPACE));
	   return(FALSE);
	   }		
	
	FOREVER
		{	   
/*		rcs_rhndl = dos_open(ADDR(&rcs_rfile[0]), 1);
		if (DOS_ERR)
*/		   rcs_rhndl = dos_create(ADDR(&rcs_rfile[0]),0);
		if (!DOS_ERR)
			break;
		if (!form_error(DOS_AX))
			return (FALSE);
		}

	if (rcs_cflag)
		{
		r_to_xfile(rcs_hfile, "RSH");
		FOREVER
			{
		        rcs_hhndl = dos_create(ADDR(&rcs_hfile[0]), 0);
			if (!DOS_ERR)
				break;
			if (!form_error(DOS_AX))
				{
				dos_close(rcs_rhndl);
				return (FALSE);
				}
			}
		}		       
	dcomp_free();
	dcomp_alerts();

	ini_prog();
	show_prog(STHDR);
	wr_header();
	rcs_wraddr = sizeof(RSHDR);
	LWSET(RSH_STRING(head), rcs_wraddr);
	c_bases();

	show_prog(STSTR);
	c_strhead();
	map_all(wr_string);
	map_frstr();		/* handles strings not ref'ed in objects */
	c_tail(c_nstring);
        wr_sync();

	show_prog(STIMG);
	LWSET(RSH_IMDATA(head), rcs_wraddr);
	c_nimage = 0;
	map_all(wr_image);
	c_frimg = c_nimage;
	map_frimg();
	  

	show_prog(STFRSTR);
	c_frshead();
	wr_freestr();
	c_tail(c_nfrstr);

	show_prog(STBB);
	LWSET(RSH_BITBLK(head), rcs_wraddr);
	LWSET(RSH_NBB(head), 0);
	c_bithead();
	map_all(wr_bitblk);
	map_frbit();	
	c_tail(c_nbb);

	show_prog(STFRIMG);
	c_frbhead();
	wr_frbit();
	c_tail(c_nfrbit);

	show_prog(STIB);
	LWSET(RSH_ICONBLK(head), rcs_wraddr);
	LWSET(RSH_NIB(head), 0);
	c_iconhead();
	map_all(wr_iconblk);
	c_tail(c_nib);
        

	show_prog(STTI);
	LWSET(RSH_TEDINFO(head), rcs_wraddr);
	LWSET(RSH_NTED(head), 0);
	c_tedhead();
	map_all(wr_tedinfo);
	c_tail(c_nted);

	show_prog(STOBJ);
	LWSET(RSH_OBJECT(head), rcs_wraddr);
	LWSET(RSH_NOBS(head), 0);
	c_objhead();
	wr_trees();
	c_tail(c_nobs);

	show_prog(STTRIND);
	c_treehead();
	wr_trindex();		/* also handles RSH_TRINDEX(head) */
	c_tail(c_ntree);

	show_prog(STHDR);
	LWSET(RSH_RSSIZE(head), rcs_wraddr);
	wr_header();		/* rewrite updated header	  */
	dos_close(rcs_rhndl);
	c_foobar();
	c_defs();
	if (rcs_cflag)
		{
		ctrl_z(rcs_hhndl);
		dos_close(rcs_hhndl);
		}

	FOREVER
		{
		rcs_dhndl = dos_create(ADDR(&rcs_dfile[0]), 0);
		if (!DOS_ERR)
			break;
		if (!form_error(DOS_AX))
			return (FALSE);
		}

	show_prog(STNAME);
#if	MC68K
   	wr_68kdef();
#else

	dos_write(rcs_dhndl, 2, ADDR(&rcs_ndxno));
	dos_write(rcs_dhndl, rcs_ndxno * sizeof(INDEX), ADDR(rcs_index));
	dos_close(rcs_dhndl);

#endif

	if (rcs_hflag)
		if ( rcs_fsrtflag )
		wrsrt_inclfile("H", hptns);
else write_inclfile("H", hptns);
	if (rcs_oflag)
		if (rcs_fsrtflag )
		wrsrt_inclfile("I", optns);
else write_inclfile("I", optns);
	if (rcs_cbflag)
		if ( rcs_fsrtflag )
		wrsrt_inclfile("B", bptns);
else write_inclfile("B", bptns);
	if (rcs_f77flag)
		if ( rcs_fsrtflag )
		wrsrt_inclfile("F", fptns);
else write_inclfile("F", fptns);

	return (TRUE);
	}

