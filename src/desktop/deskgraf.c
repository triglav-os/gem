/*
 * Implements desktop-specific drawing helpers layered over GEM graphics
 * services. The routines handle icon, text, and small UI rendering details
 * used throughout the shell.
 *
 * MIT License (see: LICENSE)
 * Copyright (C) 2026 tomaz stih
 */

#include <portab.h>
#include <machine.h>
#include <obdefs.h>
#include <gsxdefs.h>
#include <bind.h>
#include <funcdef.h>

#include <stdio.h>

EXTERN VOID v_pline(WORD count, WORD *pxyarray);
EXTERN VOID vst_height(WORD height, WORD *pchr_width, WORD *pchr_height, WORD *pcell_width, WORD *pcell_height);
EXTERN VOID vsl_width(WORD width);
EXTERN VOID vro_cpyfm(WORD wr_mode, WORD *pxyarray, WORD *psrcMFDB, WORD *pdesMFDB);
EXTERN VOID vrt_cpyfm(WORD wr_mode, WORD *pxyarray, WORD *psrcMFDB, WORD *pdesMFDB, WORD fgcolor, WORD bgcolor);
EXTERN VOID vr_recfl(WORD *pxyarray, WORD *pdesMFDB);

#define v_pline desk_v_pline
#define vst_height desk_vst_height
#define vsl_width desk_vsl_width
#define vro_cpyfm desk_vro_cpyfm
#define vrt_cpyfm desk_vrt_cpyfm
#define vr_recfl desk_vr_recfl

#define ORGADDR 0x0L
/* ----- Defined in gsxbind.c. -------------------------------------- */
#define vsf_interior( x )	gsx_1code(S_FILL_STYLE, x)
#define vsl_type( x )		gsx_1code(S_LINE_TYPE, x)
#define vsf_style( x )		gsx_1code(S_FILL_INDEX, x)
#define vsf_color( x )		gsx_1code(S_FILL_COLOR, x)
#define vsl_udsty( x )		gsx_1code(ST_UD_LINE_STYLE, x)
/* ----- External routines. ----------------------------------------- */
EXTERN VOID r_get(GRECT *rect, WORD *x, WORD *y, WORD *w, WORD *h);
EXTERN WORD vst_clip(WORD clip_flag, WORD *pxyarray);
EXTERN VOID r_set(GRECT *rect, WORD x, WORD y, WORD w, WORD h);
EXTERN VOID gsx2(void);
EXTERN VOID gsx_moff(VOID);
EXTERN VOID vro_cpyfm(WORD wr_mode, WORD *pxyarray, WORD *psrcMFDB, WORD *pdesMFDB);
EXTERN VOID vrt_cpyfm(WORD wr_mode, WORD *pxyarray, WORD *psrcMFDB, WORD *pdesMFDB, WORD fgcolor, WORD bgcolor);
EXTERN VOID gsx_mon(VOID);
EXTERN VOID vrn_trnfm(WORD *psrcMFDB, WORD *pdesMFDB);
EXTERN VOID gsx_1code(WORD code, WORD value);

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

GLOBAL WORD	gl_wbox;
GLOBAL WORD	gl_hbox;

GLOBAL WORD	gl_xclip;
GLOBAL WORD	gl_yclip;
GLOBAL WORD	gl_wclip;
GLOBAL WORD	gl_hclip;

GLOBAL WORD 	gl_nplanes;
GLOBAL WORD 	gl_handle;

static void deskgraf_trace(const char *text)
{
	FILE		*fp;

	fp = fopen("/tmp/gem_deskgraf_trace.log", "a");
	if (fp == NULL)
	  return;
	fputs(text, fp);
	fputc('\n', fp);
	fclose(fp);
}

GLOBAL FDB	gl_src;
GLOBAL FDB	gl_dst;

GLOBAL WS	gl_ws;
GLOBAL WORD	contrl[12];
GLOBAL WORD	intin[128];
GLOBAL WORD	ptsin[20];
GLOBAL WORD	intout[10];
GLOBAL WORD	ptsout[10];

GLOBAL LONG	ad_intin;

GLOBAL WORD	gl_mode;
GLOBAL WORD	gl_mask;
GLOBAL WORD	gl_tcolor;
GLOBAL WORD	gl_lcolor;
GLOBAL WORD	gl_fis;
GLOBAL WORD	gl_patt;
GLOBAL WORD	gl_font;

GLOBAL GRECT	gl_rscreen;
GLOBAL GRECT	gl_rfull;
GLOBAL GRECT	gl_rzero;
GLOBAL GRECT	gl_rcenter;
GLOBAL GRECT	gl_rmenu;

/*
*	Routine to set the clip rectangle.  If the w,h of the clip
*	is 0, then no clip should be set.  Ohterwise, set the 
*	appropriate clip.
*/
WORD gsx_sclip(GRECT *pt)
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
	return( TRUE );
}

/*
*	Routine to get the current clip setting
*/
VOID gsx_gclip(GRECT *pt)
{
	r_set(pt, gl_xclip, gl_yclip, gl_wclip, gl_hclip);
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

/*
*	Routine to draw a certain number of points in a polyline
*	relative to a given x,y offset.
*/
VOID gsx_pline(WORD offx, WORD offy, WORD cnt, WORD *pts)
{
	WORD		i, j;

	for (i=0; i<cnt; i++)
	{
	  j = i * 2;
	  ptsin[j] = offx + pts[j];
	  ptsin[j+1] = offy + pts[j+1];
	}

	gsx_xline( cnt, &ptsin[0]);
}

/*
*	Routine to set the text, writing mode, and color attributes.
*/
VOID gsx_attr(UWORD text, UWORD mode, UWORD color)
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
	  gsx2();
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
	  gsx2();
	}
	intin[0] = tmp;
}

/*
*	Routine to fix up the MFDB of a particular raster form
*/
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

/*
*	Routine to blit, to and from a specific area
*/
VOID gsx_blt(LONG saddr, UWORD sx, UWORD sy, UWORD swb, LONG daddr, UWORD dx, UWORD dy, UWORD dwb, UWORD w, UWORD h, UWORD rule, WORD fgcolor, WORD bgcolor)
{
	gsx_fix(&gl_src, saddr, swb, h);
	gsx_fix(&gl_dst, daddr, dwb, h);

	gsx_moff();
	ptsin[0] = sx;
	ptsin[1] = sy;
	ptsin[2] = sx + w - 1;
	ptsin[3] = sy + h - 1;
	ptsin[4] = dx;
	ptsin[5] = dy;
	ptsin[6] = dx + w - 1;
	ptsin[7] = dy + h - 1 ;
	if (fgcolor == -1)
	  vro_cpyfm( rule, &ptsin[0], &gl_src, &gl_dst);
else vrt_cpyfm( rule, &ptsin[0], &gl_src, &gl_dst, fgcolor, bgcolor);
	gsx_mon();
}

/*
*	Routine to blit around something on the screen
*/
VOID bb_screen(WORD scrule, WORD scsx, WORD scsy, WORD scdx, WORD scdy, WORD scw, WORD sch)
{
	gsx_blt(0x0L, scsx, scsy, 0, 
		0x0L, scdx, scdy, 0,
		scw, sch, scrule, -1, -1);
}

/*
*	Routine to transform a standard form to device specific
*	form.
*/
VOID gsx_trans(LONG saddr, UWORD swb, LONG daddr, UWORD dwb, UWORD h)
{
	gsx_fix(&gl_src, saddr, swb, h);
	gl_src.fd_stand = TRUE;
	gl_src.fd_nplanes = 1;

	gsx_fix(&gl_dst, daddr, dwb, h);
	vrn_trnfm( &gl_src, &gl_dst );
}

/*
*	Routine to initialize all the global variables dealing
*	with a particular workstation open
*/
VOID gsx_start(VOID)
{
	WORD		char_height, nc;

	deskgraf_trace("gsx_start:enter");
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
	deskgraf_trace("gsx_start:after planes");
	char_height = gl_ws.ws_chminh;
	vst_height( char_height, &gl_wptschar, &gl_hptschar, 
				&gl_wschar, &gl_hschar );
	deskgraf_trace("gsx_start:after small font");
	char_height = gl_ws.ws_chmaxh;
	vst_height( char_height, &gl_wptschar, &gl_hptschar, 
				&gl_wchar, &gl_hchar );
	deskgraf_trace("gsx_start:after system font");
	gl_ncols = gl_width / gl_wchar;
	gl_nrows = gl_height / gl_hchar;
	gl_hbox = gl_hchar + 3;
	gl_wbox = (gl_hbox * gl_ws.ws_hpixel) / gl_ws.ws_wpixel;
	deskgraf_trace("gsx_start:after metrics");
	vsl_type( 7 );
	vsl_width( 1 );
	vsl_udsty( 0xffff );
	deskgraf_trace("gsx_start:after line attrs");
	r_set(&gl_rscreen, 0, 0, gl_width, gl_height);
	r_set(&gl_rfull, 0, gl_hbox, gl_width, (gl_height - gl_hbox));
	r_set(&gl_rzero, 0, 0, 0, 0);
	r_set(&gl_rcenter, (gl_width-gl_wbox)/2, (gl_height-(2*gl_hbox))/2, 
			gl_wbox, gl_hbox);
	r_set(&gl_rmenu, 0, 0, gl_width, gl_hbox);
	ad_intin = ADDR(&intin[0]);
	deskgraf_trace("gsx_start:exit");
}

VOID gsx_tblt(WORD tb_f, WORD x, WORD y, WORD tb_nc)
{
	WORD		pts_height;

	if (tb_f == IBM)
	{
	  if (tb_f != gl_font)
	  {
	    pts_height = gl_ws.ws_chmaxh;
	    vst_height( pts_height, &gl_wptschar, &gl_hptschar, 
				&gl_wchar, &gl_hchar );
	    gl_font = tb_f;
	  }
	  y += gl_hptschar;
	}

	contrl[0] = 8;		/* Text */
contrl[1] = 1;
	contrl[6] = gl_handle;
	ptsin[0] = x;
	ptsin[1] = y;
	contrl[3] = tb_nc;
	gsx2();
}

/*
*	Routine to do a filled bit blit, (a rectangle).
*/
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
