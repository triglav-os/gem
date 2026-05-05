/*
 * Implements the VDI-facing drawing and bitmap helpers used by the
 * resource construction set. It captures the editor graphics assumptions
 * that will matter during the port.
 *
 * MIT License (see: LICENSE)
 * Copyright (C) 2026 tomaz stih
 */

#include "portab.h"
#include "machine.h"
#include "obdefs.h"
#include "gsxdefs.h"
#include "vdibind.h"
#include "funcdef.h"
#include "gembind.h"
#include "rcsext.h"

#define ORGADDR 0x0L
#define vsf_interior( x )	gsx_1code(S_FILL_STYLE, x)
#define vsl_type( x )		gsx_1code(S_LINE_TYPE, x)
#define vsf_style( x )		gsx_1code(S_FILL_INDEX, x)
#define vsl_udsty( x )		gsx_1code(ST_UD_LINE_STYLE, x)
#define vsf_color( x )		gsx_1code(S_FILL_COLOR, x)

GLOBAL WORD	gl_width;
GLOBAL WORD	gl_height;

GLOBAL WORD	gl_nrows;
GLOBAL WORD	gl_ncols;

GLOBAL WORD	gl_wchar;
GLOBAL WORD	gl_hchar;

GLOBAL WORD	gl_wschar;
GLOBAL WORD	gl_hschar;

GLOBAL WORD	gl_wptschar;
GLOBAL WORD	gl_hptschar;

GLOBAL WORD	gl_wsptschar;
GLOBAL WORD	gl_hsptschar;

GLOBAL WORD	gl_wbox;
GLOBAL WORD	gl_hbox;

GLOBAL WORD	gl_xclip;
GLOBAL WORD	gl_yclip;
GLOBAL WORD	gl_wclip;
GLOBAL WORD	gl_hclip;

GLOBAL WORD 	gl_nplanes;
GLOBAL WORD 	gl_handle;

GLOBAL FDB		gl_src;
GLOBAL FDB		gl_dst;
GLOBAL FDB		gl_tmp;

GLOBAL WS		gl_ws;
GLOBAL WORD		contrl[12];
GLOBAL WORD		intin[128];
GLOBAL WORD		ptsin[128];
GLOBAL WORD		intout[128];
GLOBAL WORD		ptsout[128];

GLOBAL LONG		ad_intin;

GLOBAL WORD		gl_mode;
GLOBAL WORD		gl_tcolor;
GLOBAL WORD		gl_lcolor;
GLOBAL WORD		gl_fis;
GLOBAL WORD		gl_patt;

GLOBAL GRECT		gl_rscreen;
GLOBAL GRECT		gl_rfull;
GLOBAL GRECT		gl_rzero;
GLOBAL GRECT		gl_rcenter;
GLOBAL GRECT		gl_rmenu;

	VOID gsx_moff()
	{
	graf_mouse(M_OFF, 0x0L);
	}

	VOID gsx_mon()
	{
	graf_mouse(M_ON, 0x0L);
	}


/************************************************************************/
/* g r _ r e c t							*/
/************************************************************************/
	VOID gr_rect(icolor, ipattern, pt)
	WORD		icolor;
	WORD		ipattern;
	GRECT		*pt;
	{
	WORD		fis;

	fis = FIS_PATTERN;
	if (ipattern == IP_HOLLOW)
	  	fis = FIS_HOLLOW;
	else if (ipattern == IP_SOLID)
	  	fis = FIS_SOLID;

	vsf_color(icolor);
	bb_fill(MD_REPLACE, fis, ipattern, 
	  	pt->g_x, pt->g_y, pt->g_w, pt->g_h);
	} 

VOID gsx_sclip(GRECT *pt)
{
	r_get(pt, &gl_xclip, &gl_yclip, &gl_wclip, &gl_hclip);

	if ( gl_wclip && gl_hclip )
	{
	  ptsin[0] = gl_xclip;
	  ptsin[1] = gl_yclip;
	  ptsin[2] = gl_xclip + gl_wclip - 1;
	  ptsin[3] = gl_yclip + gl_hclip - 1;
	  vst_clip( TRUE, &ptsin[0]);
	}
else vst_clip( FALSE, &ptsin[0]);
	return;
} /* gsx_setclip */

VOID gsx_attr(WORD text, WORD mode, WORD color)
{
	WORD		tmp;

	tmp = intin[0];
	contrl[1] = 0;
	contrl[3] = 1;
	contrl[6] = gl_handle;
	if (mode != gl_mode)
	{
	  contrl[0] = SET_WRITING_MODE;
	  intin[0] = gl_mode = mode;
	  vdi();
	}
	contrl[0] = FALSE;
	if (text)
	{
	  if (color != gl_tcolor)
	  {
	    contrl[0] = S_TEXT_COLOR;
	    gl_tcolor = color;
	  }
	}	
	else
	{
	  if (color != gl_lcolor)
	  {
	    contrl[0] = S_LINE_COLOR;
	    gl_lcolor = color;
	  }
	}
	if (contrl[0])
	{
	  intin[0] = color;
	  vdi();
	}
	intin[0] = tmp;
}

VOID gsx_bxpts(GRECT *pt)
{
	ptsin[0] = pt->g_x;
	ptsin[1] = pt->g_y;
	ptsin[2] = pt->g_x + pt->g_w - 1;
	ptsin[3] = pt->g_y;
	ptsin[4] = pt->g_x + pt->g_w - 1;
	ptsin[5] = pt->g_y + pt->g_h - 1;
	ptsin[6] = pt->g_x;
	ptsin[7] = pt->g_y + pt->g_h - 1;
	ptsin[8] = pt->g_x;
	ptsin[9] = pt->g_y;
}

VOID gsx_xbox(GRECT *pt)
{
	gsx_bxpts(pt);
	gsx_xline( 5, &ptsin[0] );
}

VOID gsx_fix(FDB *pfd, LONG theaddr, WORD wb, WORD h)
{
	if (theaddr == ORGADDR)
	{
	  pfd->fd_w = gl_ws.ws_xres + 1;
	  pfd->fd_wdwidth = pfd->fd_w / 16;
	  pfd->fd_h = gl_ws.ws_yres + 1;
	  pfd->fd_nplanes = gl_nplanes;
	}
	else
	{
	  pfd->fd_wdwidth = wb / 2;
	  pfd->fd_w = wb * 8;
	  pfd->fd_h = h;
	  pfd->fd_nplanes = 1;
	}
	pfd->fd_stand = FALSE;
	pfd->fd_addr = theaddr;
}

VOID gsx_untrans(LONG saddr, WORD swb, LONG daddr, WORD dwb, WORD h)
{
	gsx_fix(&gl_src, saddr, swb, h);
	gl_src.fd_stand = FALSE;
	gl_src.fd_nplanes = 1;

	gsx_fix(&gl_dst, daddr, dwb, h);
	vrn_trnfm( &gl_src, &gl_dst );
}

VOID gsx_trans(LONG saddr, WORD swb, LONG daddr, WORD dwb, WORD h)
{
	gsx_fix(&gl_src, saddr, swb, h);
	gl_src.fd_stand = TRUE;
	gl_src.fd_nplanes = 1;

	gsx_fix(&gl_dst, daddr, dwb, h);
	vrn_trnfm( &gl_src, &gl_dst );
}

	VOID gsx_outline(pt)
	GRECT	*pt;
	{
	vst_clip(FALSE, &ptsin[0]);
	gsx_attr(FALSE, MD_XOR, BLACK);
	gsx_bxpts(pt);
	v_pline(5, &ptsin[0]);
	}

	VOID gsx_invert(pt)
	GRECT	*pt;
	{
	vsf_color(BLACK);
	vst_clip(FALSE, &ptsin[0]); 
	bb_fill(MD_XOR, FIS_SOLID, IP_SOLID,
		pt->g_x, pt->g_y, pt->g_w, pt->g_h);
	}

VOID bb_fill(WORD mode, WORD fis, WORD patt, WORD hx, WORD hy, WORD hw, WORD hh)
{

	gsx_fix(&gl_dst, 0x0L, 0, 0);
	ptsin[0] = hx;
	ptsin[1] = hy;
	ptsin[2] = hx + hw - 1;
	ptsin[3] = hy + hh - 1;

	gsx_attr(TRUE, mode, gl_tcolor);
	if (fis != gl_fis)
	{
	  vsf_interior( fis);
	  gl_fis = fis;
	}
	if (patt != gl_patt)
	{
	  vsf_style( patt );
	  gl_patt = patt;
	}
	vr_recfl( &ptsin[0], &gl_dst );
}

WORD ch_width(WORD fn)
{
	if (fn == IBM)
	  return(gl_wchar);
	if (fn == SMALL)
	  return(gl_wschar);
	return(0);
}

WORD ch_height(WORD fn)
{
	if (fn == IBM)
	  return(gl_hchar);
	if (fn == SMALL)
	  return(gl_hschar);
	return(0);
}

VOID gsx_xline(WORD ptscount, WORD *ppoints)
{
	static	WORD	hztltbl[2] = { 0x5555, 0xaaaa };
	static  WORD	verttbl[4] = { 0x5555, 0xaaaa, 0xaaaa, 0x5555 };
	WORD		*linexy,i;
	WORD		st;

	for ( i = 1; i < ptscount; i++ )
	{
	  if ( *ppoints == *(ppoints + 2) )
	  {
	    st = verttbl[( (( *ppoints) & 1) | ((*(ppoints + 1) & 1 ) << 1))];
	  }	
	  else
	  {
	    linexy = ( *ppoints < *( ppoints + 2 )) ? ppoints : ppoints + 2;
	    st = hztltbl[( *(linexy + 1) & 1)];
	  }
	  vsl_udsty( st );
	  v_pline( 2, ppoints );
	  ppoints += 2;
	}
	vsl_udsty( 0xffff );
}	

WORD gsx_start(VOID)
{
	WORD		char_height, nc, attrib[10];

	gl_xclip = 0;
	gl_yclip = 0;
	gl_width = gl_wclip = gl_ws.ws_xres + 1;
	gl_height = gl_hclip = gl_ws.ws_yres + 1;

	nc = gl_ws.ws_ncolors;
	gl_nplanes = 0;
	while (nc != 1)
	{
	  nc >>= 1;
	  gl_nplanes++;
	}
	vqt_attributes( &attrib[0] );
	gl_ws.ws_chmaxh = attrib[7];
	gl_ws.ws_pts2 = attrib[6];
	char_height = gl_ws.ws_chminh;
	vst_height( char_height, &gl_wsptschar, &gl_hsptschar, 
				&gl_wschar, &gl_hschar );
	char_height = gl_ws.ws_chmaxh;
	vst_height( char_height, &gl_wptschar, &gl_hptschar, 
				&gl_wchar, &gl_hchar );
	gl_ncols = gl_width / gl_wchar;
	gl_nrows = gl_height / gl_hchar;
	gl_hbox = gl_hchar + 3;
	gl_wbox = (gl_hbox * gl_ws.ws_hpixel) / gl_ws.ws_wpixel;
	vsl_type( 7 );
	vsl_width( 1 );
	vsl_udsty( 0xffff );
	r_set(&gl_rscreen, 0, 0, gl_width, gl_height);
	r_set(&gl_rfull, 0, gl_hbox, gl_width, (gl_height - gl_hbox));
	r_set(&gl_rzero, 0, 0, 0, 0);
	r_set(&gl_rcenter, (gl_width-gl_wbox)/2, (gl_height-(2*gl_hbox))/2, 
			gl_wbox, gl_hbox);
	r_set(&gl_rmenu, 0, 0, gl_width, gl_hbox);
	ad_intin = ADDR(&intin[0]);
}

VOID gsx_vopen(VOID)
{
	WORD		i;

	for(i=0; i<10; i++)
	  intin[i] = 1;
	intin[10] = 2;			/* device coordinate space */

	v_opnvwk( &intin[0], &gl_handle, &gl_ws );
}

VOID gsx_vclose(VOID)
{
	gsx_ncode(CLOSE_VWORKSTATION, 0, 0);
}

VOID gsx_ncode(WORD code, WORD n, WORD m)
{
	contrl[0] = code;
	contrl[1] = n;
	contrl[3] = m;
	contrl[6] = gl_handle;
	vdi();
}

VOID gsx_1code(WORD code, WORD val)
{
	intin[0] = val;
	gsx_ncode(code, 0, 1);
}


WORD v_opnvwk(WORD *pwork_in, WORD *phandle, WORD *pwork_out)
{
	WORD		*ptsptr;

	i_intin( pwork_in );	/* set intin to point to callers data  */
	i_intout( pwork_out );	/* set intout to point to callers data */
	ptsptr = pwork_out + 45;
	i_ptsout( ptsptr );	/* set ptsout to work_out array */
	contrl[0] = 100;
	contrl[1] = 0;		  /* no points to xform */
	contrl[3] = 11;		  /* pass down xform mode also */
	contrl[6] = *phandle;
	vdi();
	*phandle = contrl[6];	 
	i_intin( intin );	 /* reset pointers for next call to binding */
	i_intout( intout );
	i_ptsout( ptsout );
	i_ptsin( ptsin ); 
}

WORD v_pline(WORD count, WORD *pxyarray)
{
	i_ptsin( pxyarray );
	gsx_ncode(POLYLINE, count, 0);
	i_ptsin( ptsin );
}      
WORD v_fillarea(WORD count, WORD *pxyarray)
{
	i_ptsin( pxyarray );
	gsx_ncode(FILLED_AREA, count, 0);
	i_ptsin( ptsin );
}      
WORD v_bar(WORD *pxyarray)
{
	i_ptsin( pxyarray );
	contrl[5] = 1;
	gsx_ncode(11, 2, 0);
	i_ptsin( ptsin );
}      

	WORD v_ellipse(x, y, xradius,yradius )
	WORD  x, y, xradius, yradius;
	{	       
	ptsin[0] = x;
	ptsin[1] = y;	       
	ptsin[2] = xradius;
	ptsin[3] = yradius;
	contrl[5] = 5;
        gsx_ncode(GDP,  2, 0 );
	}

	WORD v_ellarc(x, y, xradius,yradius,begang, endang )
	WORD	begang, endang, x, y, xradius, yradius;
	{	       
	ptsin[0] = x;
	ptsin[1] = y;	       
	ptsin[2] = xradius;
	ptsin[3] = yradius;
        intin[0] = begang;
	intin[1] = endang;	
	contrl[5] = 6;
        gsx_ncode(GDP,  2, 2 );
	}
	WORD v_ellpie(x, y, xradius,yradius,begang, endang )
	WORD	begang, endang, x, y, xradius, yradius;
	{	       
	ptsin[0] = x;
	ptsin[1] = y;	       
	ptsin[2] = xradius;
	ptsin[3] = yradius;
        intin[0] = begang;
	intin[1] = endang;	
	contrl[5] = 7;
        gsx_ncode(GDP,  2, 2 );
	}


VOID vst_clip(WORD clip_flag, WORD *pxyarray)
{
	WORD		val;

	val = ( clip_flag != 0 ) ? 2 : 0;
	i_ptsin( pxyarray );
	intin[0] = clip_flag;
	gsx_ncode(TEXT_CLIP, val, 1);
	i_ptsin(ptsin);
	return;
}

vst_height( height, pchr_width, pchr_height, pcell_width, pcell_height )
	WORD	height;
	WORD	*pchr_width;
	WORD	*pchr_height;
	WORD	*pcell_width;
	WORD	*pcell_height;
{
	ptsin[0] = 0;
	ptsin[1] = height;
	gsx_ncode(CHAR_HEIGHT, 1, 0);
	*pchr_width = ptsout[0];
	*pchr_height = ptsout[1];
	*pcell_width = ptsout[2];
	*pcell_height = ptsout[3];
}

WORD vsl_ends(WORD begsty, WORD endsty)
{
	intin[0] = begsty;
	intin[1] = endsty;
	gsx_ncode(108, 0, 2);
}
WORD vsl_width(WORD width)
{
	ptsin[0] = width;
	ptsin[1] = 0;
	gsx_ncode(S_LINE_WIDTH, 1, 0);
}


	WORD vqt_attributes( attrib )
	WORD	*attrib;
	{
	WORD	*ptsptr;

	i_intout(attrib);
	ptsptr = &attrib[6];
	i_ptsout( ptsptr );
	gsx_ncode(TXT_ATTR_INQ,0,0);
	i_intout( intout );
	i_ptsout( ptsout );
	}

VOID vrn_trnfm(FDB *psrcMFDB, FDB *pdesMFDB)
{
	i_ptr( psrcMFDB );
	i_ptr2( pdesMFDB );
	gsx_ncode(TRANSFORM_FORM, 0, 0);
}

WORD vr_recfl(WORD *pxyarray, FDB *pdesMFDB)
{
	i_ptr( pdesMFDB );
	i_ptsin( pxyarray );
	gsx_ncode(FILL_RECTANGLE, 2, 1);
	i_ptsin( ptsin );
}

WORD vro_cpyfm(WORD wr_mode, WORD *pxyarray, WORD *psrcMFDB, WORD *pdesMFDB)
{
	intin[0] = wr_mode;
	i_ptr( psrcMFDB );
	i_ptr2( pdesMFDB );
	i_ptsin( pxyarray );
	gsx_ncode(COPY_RASTER_FORM, 4, 1);
	i_ptsin( &ptsin );
}
    
WORD vrt_cpyfm(WORD handle, WORD wr_mode, WORD xy[], WORD *srcMFDB, WORD *desMFDB, WORD *index)
{
	intin[0] = wr_mode;
	intin[1] = *index++;
	intin[2] = *index;
	i_ptr( srcMFDB );
	i_ptr2( desMFDB );
	i_ptsin( xy );
	contrl[0] = 121;
	contrl[1] = 4;
	contrl[3] = 3;
	contrl[6] = handle;
	vdi();
	i_ptsin( ptsin );
}
	

VOID bb_set(REG WORD sx, REG WORD sy, REG WORD sw, REG WORD sh, REG WORD *pts1, REG WORD *pts2, FDB *pfd, FDB *psrc, FDB *pdst)
{
	WORD		oldsx;

						/* get on word boundary	*/
	oldsx = sx;
	sx = (sx / 16) * 16;
	sw = ( ((oldsx - sx) + (sw + 15)) / 16 ) * 16;

	gl_tmp.fd_stand = TRUE;
	gl_tmp.fd_wdwidth = sw / 16;
	gl_tmp.fd_w = sw;
	gl_tmp.fd_h = sh;

	gsx_moff();
	pts1[0] = sx;
	pts1[1] = sy;
	pts1[2] = sx + sw - 1;
	pts1[3] = sy + sh - 1;
	pts2[0] = 0;
	pts2[1] = 0;
	pts2[2] = sw - 1;
	pts2[3] = sh - 1 ;

	gsx_fix(pfd, 0x0L, 0, 0);
	vro_cpyfm( S_ONLY, &ptsin[0], psrc, pdst );
	gsx_mon();
}


VOID bb_save(GRECT *ps)
{	
	bb_set(ps->g_x, ps->g_y, ps->g_w, ps->g_h, &ptsin[0], &ptsin[4], 
		&gl_src, &gl_src, &gl_tmp);
}


VOID bb_restore(GRECT *pr)
{
	bb_set(pr->g_x, pr->g_y, pr->g_w, pr->g_h, &ptsin[4], &ptsin[0], 
		&gl_dst, &gl_tmp, &gl_dst);
}

