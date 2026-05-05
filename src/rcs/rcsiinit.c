/*
 * Initializes the icon-editing subsystem used by the resource construction
 * set. It prepares the state and resources needed by the editor image
 * tools.
 *
 * MIT License (see: LICENSE)
 * Copyright (C) 2026 tomaz stih
 */

#include "portab.h"
#include "machine.h"
#include "obdefs.h"
#include "dosbind.h"
#include "gembind.h"
#include "rcs.h"
#include "funcdef.h"
#include "rcsiext.h"

EXTERN VOID gsx_1code(WORD code, WORD val);
EXTERN    UWORD		copy_colplanes();
EXTERN	  WORD		hndl_alert();
EXTERN	  VOID		view_parts();
EXTERN	  VOID		dslct_obj();
EXTERN VOID resize(LONG src_pt, WORD src_w, WORD src_h, LONG des_pt);
EXTERN VOID clear_clipb(VOID);
EXTERN VOID dr_image(VOID);
EXTERN VOID rubrec_off(VOID);
EXTERN VOID ob_setxywh(LONG tree, WORD obj, GRECT *pt);
EXTERN WORD rc_copy(GRECT *src, GRECT *dst);
EXTERN VOID draw_outline(GRECT *area);
EXTERN VOID set_scroll(VOID);
EXTERN	  GRECT		dispc_area, orign_area;
EXTERN	  LONG		drawaddr;
EXTERN	  WORD		toolp;
GLOBAL	  BOOLEAN	fat_buf;	/*indicate if fat_mfdb is allocated*/
GLOBAL	  USERBLK	grid_wind;
#define   vsl_type(x)	gsx_1code(S_LINE_TYPE, x)
#define   OB_SPEC(x)    (tree + (x) * sizeof(OBJECT) + 12)
/************************************************************************/
/* s e t _ i c n m e n u						*/
/************************************************************************/

VOID set_icnmenus(VOID)
{
	WORD	*ok_list, obj;

	obj = GET_NEXT
	(ad_menu, GET_HEAD(ad_menu, SCREEN));
	map_tree(ad_menu, obj, SCREEN, disab_obj);

	switch (icn_state) {
		case NOFILE_STATE:
			ok_list = OK_NOICN;
			break;
		case FILE_STATE:
			ok_list = OK_ICN;
			break;
       
		}
	for (; *ok_list; ok_list++)
		enab_obj(ad_menu, *ok_list);
					
	if ( icn_edited)	  	   
		{
		ok_list = OK_EDITED;
		for (; *ok_list; ok_list++)
			enab_obj(ad_menu, *ok_list);
		if (icn_state == FILE_STATE)
			enab_obj(ad_menu, SAVEITEM);
		}
        if(selec_area.g_w && selec_area.g_h) {
	    	enab_obj(ad_menu, COPYITEM);
		enab_obj(ad_menu, CUTITEM);
		enab_obj(ad_menu, DELITEM);
	}
	if(clipped)
		enab_obj(ad_menu, PASTITEM);
	if(!gl_isicon){
		disab_obj(ad_menu, INVITEM);
		disab_obj(ad_itool, BCLORBOX);
		}
else enab_obj(ad_itool, BCLORBOX);
}	/* Set_Icnmenu */
VOID icnhot()
	{
	if( gl_isicon)
		sble_obj(ad_itool,BCLORBOX);
	sble_obj(ad_itool, FCLORBOX);
	sble_obj(ad_itool, HRCTGBOX);
	sble_obj(ad_itool, SRCTGBOX);
	sble_obj(ad_itool, LINEBOX);
	sble_obj(ad_itool, HPLYGBOX);
	sble_obj(ad_itool, SPLYGBOX);
	sble_obj(ad_itool, HARCBOX);
	sble_obj(ad_itool, SARCBOX);
	sble_obj(ad_itool, DOTBOX);
	sble_obj(ad_itool, HOVALBOX);
	sble_obj(ad_itool, SOVALBOX);
	sble_obj(ad_itool, ERASEBOX);
	sble_obj(ad_itool, GRIDBOX);
	sble_obj(ad_itool, UNDOBOX);
	sble_obj(ad_itool, RUBERBOX);
	}

		
	VOID set_gridbx()
	{
	gridbx.g_x = fat_area.g_x;
	gridbx.g_y = fat_area.g_y;
	gridbx.g_w = gl_wimage * gl_sqsize;
	gridbx.g_h = gl_himage * gl_sqsize;
	}


/************************************************************************/
/* s e t _ fatbox							*/
/************************************************************************/
VOID set_fatbox(VOID)
{
	fat_area.g_x += 1;
	fat_area.g_y += 1;
	fat_area.g_w = gl_wimage * gl_sqsize;
        fat_area.g_h = gl_himage * gl_sqsize;
	rc_intersect(&view, &fat_area);
	set_gridbx();
}



/************************************************************************/
/* i n i t _ i m g							*/
/************************************************************************/
VOID init_img(VOID)
{
	set_fatbox();			
	objc_xywh(ad_pbx, MASKWIND, &mas_img);
	objc_xywh(ad_pbx, DATAWIND, &dat_img);
	objc_xywh(ad_pbx, ICONWIND, &icn_img);
	icn_img.g_x += LWGET(IB_XICON(gl_icnspec));
	deltax = mas_img.g_x - dat_img.g_x;
	deltay = mas_img.g_y - dat_img.g_y;
} /* init_img */

	
	VOID
iconterm(termcond)  
	WORD	termcond;
	{
	if (termcond)
	    hndl_alert(1,NOEDMEM);
	clr_hot();
	rcs_hot = NIL;
	iconedit_flag = FALSE;
	clipped = FALSE;
	dos_free( hold_mfdb.mp );
	dos_free( disp_mfdb.mp );
	dos_free( clip_mfdb.mp );
	dos_free( save_mfdb.mp );
	dos_free( undo_mfdb.mp );
	if(fat_buf) dos_free( fat_mfdb.mp );
	if(gl_isicon){
		dos_free( und2_mfdb.mp );
		dos_free( hld2_mfdb.mp );
		dos_free( clp2_mfdb.mp );
		dos_free( sav2_mfdb.mp );
	}
	ad_view = save_tree;
/*      vsf_color(BLACK); 	PUT IN LATER*/
	vsl_type(7);
	ini_panes();
	view_objs();
	wind_set(rcs_view, WF_NAME, ADDR(&rcs_title[0]), 0x0, 0x0);
	new_state(rcs_state);
	dslct_obj(save_tree, save_obj);
	send_redraw(rcs_view, &tools);
	}



	VOID wind_size()
	{      	       
	LONG	 taddr, taddr1;						      

/* this procedure changes the GLOBAL window coordinates for the GRID,	*/
/*   MASK, DATA, and ICON windows.					*/

	
	SET_WIDTH(ad_view, ROOT,(fat_area.g_w = gl_wimage * gl_sqsize) + 2);
	SET_WIDTH(ad_view, GRIDWIND,fat_area.g_w + 2);
	SET_HEIGHT(ad_view,ROOT,(fat_area.g_h = gl_himage * gl_sqsize) + 2); 
	SET_HEIGHT(ad_view,GRIDWIND, fat_area.g_h + 2);

      	ini_tree(&ad_pbx, ICNPBX);
      	if( gl_isicon )
		{
		SET_SPEC(ad_pbx,ICONWIND,gl_icnspec);
		taddr =GET_SPEC(ad_pbx,DATAWIND);
		LLSET(BI_PDATA(taddr),LLGET(IB_PDATA(gl_icnspec)));
		LWSET(BI_WB(taddr),2 * gl_datasize / gl_himage);
		LWSET(BI_HL(taddr), gl_himage);
		LWSET(BI_COLOR(taddr), BLACK);
		fgcolor = LWGET(IB_CHAR(gl_icnspec)) >> 12 & 0x0f;
        	SET_WIDTH(ad_pbx,ICONWIND,gl_wimage);
        	SET_HEIGHT(ad_pbx,ICONWIND,gl_himage);
		
		taddr1 =GET_SPEC(ad_pbx,MASKWIND);
		if ( taddr1 != -1L )
			{
			LLSET(BI_PDATA(taddr1),LLGET(IB_PMASK(gl_icnspec)));
			LWSET(BI_WB(taddr1),2 * gl_datasize / gl_himage);
			LWSET(BI_HL(taddr1), gl_himage);
			LWSET(BI_COLOR(taddr1), BLACK); 
			bgcolor = LWGET(IB_CHAR(gl_icnspec)) >> 8 & 0x0f;
			unhide_obj(ad_pbx,MASKWIND);
			unhide_obj(ad_pbx,MASKSTRG);
			}
		unhide_obj(ad_pbx,ICONWIND);
		unhide_obj(ad_pbx,ICONSTRG);
		SET_WIDTH(ad_pbx,DATAWIND,gl_wimage);
		SET_HEIGHT(ad_pbx,DATAWIND, gl_himage);	       
		SET_WIDTH(ad_pbx,MASKWIND,gl_wimage);
		SET_HEIGHT(ad_pbx,MASKWIND,gl_himage);		      
			/*initial hld2_mfdb*/
		copy_colplanes(taddr1, BLACK, FALSE); /*bgcolor*/
		rast_op(3, &hold_area, &hld2_mfdb, &hold_area, &und2_mfdb);
		}
	else
		{
/*		SET_SPEC(ad_pbx,DATAWIND,gl_icnspec);
*/		taddr =GET_SPEC(ad_pbx,DATAWIND);
		LLSET(BI_PDATA(taddr),LLGET(BI_PDATA(gl_icnspec)));
		LWSET(BI_WB(taddr),2 * gl_datasize / gl_himage);
		LWSET(BI_HL(taddr), gl_himage);		
		LWSET(BI_COLOR(taddr), fgcolor = LWGET(BI_COLOR(gl_icnspec)));
		bgcolor = WHITE;
		old_fc = fgcolor;
		SET_WIDTH(ad_pbx,DATAWIND,gl_wimage);
		SET_HEIGHT(ad_pbx,DATAWIND, gl_himage);	       
		hide_obj(ad_pbx, ICONWIND); 
		hide_obj(ad_pbx, ICONSTRG);
		hide_obj(ad_pbx, MASKWIND);
		hide_obj(ad_pbx, MASKSTRG);
		}				    
	taddr =GET_SPEC(ad_pbx,DATAWIND);
        copy_colplanes(taddr, BLACK, TRUE); /*fgcolor*/
      	if( gl_isicon ){
	   rast_op(4, &hold_area, &hold_mfdb, &hold_area, &hld2_mfdb);
	   WORD LWCOPY(LONG dst, LONG src, WORD count);
        }
	rast_op(3, &hold_area, &hold_mfdb, &hold_area, &undo_mfdb);
	}

	VOID outl_img()
	{
	GRECT temp_area;
	WORD i, start, end;
	gsx_sclip(&pbx);
	if ( gl_isicon){
	    start = ICONWIND;
	    end = MASKWIND;
	}
	else{
	    start = DATAWIND;
	    end = DATAWIND;
	}
	for(i = start; i<=end; i++){
     		objc_xywh( ad_pbx, i, &temp_area);
		temp_area.g_x -= 1;
		if(i == ICONWIND)
		    temp_area.g_x += LWGET(IB_XICON(gl_icnspec));
		temp_area.g_y -= 1;
		temp_area.g_w += 2;
		temp_area.g_h += 2;
		draw_outline( &temp_area);
	}
	
	}


	VOID icn_init(size_changed)
BOOLEAN size_changed;
	{
	LONG	tree, taddr;
	WORD	obtype, old_h, old_w;


	gl_icnspec = taddr = GET_SPEC(save_tree, save_obj);
	obtype = GET_TYPE(save_tree, save_obj);
	if(size_changed){
	    old_h = gl_himage;
	    old_w = gl_wimage;
	}
	if (LLOBT( obtype ) == G_ICON)
		{
		gl_isicon = TRUE;
		gl_wimage = LWGET(IB_WICON(gl_icnspec));
		gl_himage = LWGET(IB_HICON(gl_icnspec));
		gl_datasize = (gl_himage * gl_wimage) >> 4; 
		fgcolor = LWGET(IB_CHAR(gl_icnspec)) >> 12 | 0xf;
		bgcolor = LWGET(IB_CHAR(gl_icnspec)) >> 8 | 0xf;
		if ( LHIBT(obtype ))

		/* if icon is the generic rcs icon icon allocate memory for it */
			{	
		        copy_spec(save_tree, save_obj);
			gl_icnspec =  GET_SPEC(save_tree, save_obj);

			LLSET(IB_PDATA(gl_icnspec),get_mem(gl_datasize * 2));
			if(size_changed)
				resize(LLGET(IB_PDATA(taddr)), old_w, old_h,
				       LLGET(IB_PDATA(gl_icnspec)));
			else
	       			LBCOPY(LLGET(IB_PDATA(gl_icnspec)),
			           LLGET( IB_PDATA(taddr)), gl_datasize *2);
		
			
	       		LLSET(IB_PMASK(gl_icnspec), get_mem(gl_datasize * 2));
			if(size_changed)
				resize(LLGET(IB_PMASK(taddr)), old_w, old_h,
				       LLGET(IB_PMASK(gl_icnspec)));
			else
				LBCOPY(LLGET(IB_PMASK(gl_icnspec)),
			           LLGET( IB_PMASK(taddr)), gl_datasize * 2);
			tree = save_tree;
			LWSET(OB_TYPE(save_obj), (G_ICON & 0x00ff) );
			}				 

		}
	else	 
		{
		gl_isicon = FALSE;
		gl_wimage = LWGET(BI_WB(gl_icnspec)) << 3;  /*wb = width in bytes */
		gl_himage = LWGET(BI_HL(gl_icnspec));				   
		gl_datasize = (gl_himage * gl_wimage) >> 4; 		       
		fgcolor = LWGET(BI_COLOR(gl_icnspec));
		bgcolor = WHITE;
		if ( LHIBT(obtype ))
		/* if image is generic rcs image allocate memory for it */
			{
			
		        copy_spec(save_tree, save_obj);
			gl_icnspec =  GET_SPEC(save_tree, save_obj);
			tree = save_tree;
			LWSET(OB_TYPE(save_obj), (G_IMAGE & 0x00ff) );
			LLSET(BI_PDATA(gl_icnspec), get_mem(gl_datasize * 2));
			if(size_changed)
				resize(LLGET(BI_PDATA(taddr)), old_w, old_h,
				       LLGET(BI_PDATA(gl_icnspec)));
			else
			        LBCOPY(LLGET(BI_PDATA(gl_icnspec)),
			          LLGET( BI_PDATA(taddr)), gl_datasize * 2);
			}			 
		}
	
	icn_state = NOFILE_STATE;

	scrn_area.g_x = 0;
	scrn_area.g_y = 0;
	scrn_area.g_w = gl_width;
	scrn_area.g_h = gl_height;
	scrn_mfdb.mp = 0x0L;

	scroll_fat.g_x = scroll_fat.g_y = hold_area.g_x = hold_area.g_y = 0;
	scroll_fat.g_w = hold_area.g_w = gl_wimage;
	scroll_fat.g_h = hold_area.g_h = gl_himage;
	
	/* set up storage area for saving images */
	undo_mfdb.fwp = hold_mfdb.fwp =
		 clip_mfdb.fwp = save_mfdb.fwp = gl_wimage;
	undo_mfdb.fww =	hold_mfdb.fww = save_mfdb.fww =
			clip_mfdb.fww = (hold_mfdb.fwp + 0x0f) >> 4;
	undo_mfdb.fh = hold_mfdb.fh = clip_mfdb.fh = save_mfdb.fh = gl_himage;
	undo_mfdb.np = hold_mfdb.np = fat_mfdb.np =
		disp_mfdb.np = clip_mfdb.np = save_mfdb.np = gl_nplanes;
	undo_mfdb.ff = hold_mfdb.ff = 
		 clip_mfdb.ff = save_mfdb.ff = 0;/*dev specific form*/

	/* clear hold area */
	/* both mfdb must be in device specific form */
	rast_op(0,&hold_area, &scrn_mfdb, &hold_area, &hold_mfdb);  
	rast_op(0,&hold_area, &scrn_mfdb, &hold_area, &undo_mfdb);	
	rast_op(0,&hold_area, &scrn_mfdb, &hold_area, &clip_mfdb);	
	rast_op(0,&hold_area, &scrn_mfdb, &hold_area, &save_mfdb);
	
	if(gl_isicon){
		sav2_mfdb.fwp = und2_mfdb.fwp = hld2_mfdb.fwp =
			clp2_mfdb.fwp = gl_wimage;
		hld2_mfdb.fww = und2_mfdb.fww =	clp2_mfdb.fww =
			sav2_mfdb.fww = undo_mfdb.fww;
		hld2_mfdb.fh = clp2_mfdb.fh = und2_mfdb.fh =
			sav2_mfdb.fh = undo_mfdb.fh;
		hld2_mfdb.ff = und2_mfdb.ff = clp2_mfdb.ff = sav2_mfdb.ff = 0;
		und2_mfdb.np = hld2_mfdb.np = clp2_mfdb.np =
			sav2_mfdb.np =gl_nplanes;
		rast_op(0,&hold_area, &scrn_mfdb, &hold_area, &sav2_mfdb);
		rast_op(0,&hold_area, &scrn_mfdb, &hold_area, &clp2_mfdb);
		rast_op(0,&hold_area, &scrn_mfdb, &hold_area, &hld2_mfdb);  
		rast_op(0,&hold_area, &scrn_mfdb, &hold_area, &und2_mfdb);
	}


	wind_size();
	if(size_changed)
		{
		view_parts();
		init_img();
		clear_clipb();
		rubrec_off();
		}
	else
		{
		rast_op(3, &hold_area, &hold_mfdb, &hold_area, &save_mfdb);
		if(gl_isicon)
		   rast_op(3, &hold_area, &hld2_mfdb, &hold_area, &sav2_mfdb);
		wind_set(rcs_view, WF_NAME, ADDR("Editing Data"), 0, 0);
		is_mask = FALSE;
		ini_tree(&ad_itool, ICONTOOL);
		ini_panes();
		send_redraw(rcs_view, &itool);
		}
	view_objs();	
	if(size_changed) set_scroll();	
	dr_image();
	}  /* icn_init*/


WORD iconinit()						 
	{		
      UWORD	loword, hiword;
      WORD	llword, hlword;
      LONG	tree;
	wind_update( TRUE );
	iconedit_flag = TRUE;
	save_tree = ad_view;				
	save_obj = rcs_sel[0];
	rcs_nsel = 0;
	menu_tnormal( ad_menu, OPTNMENU, TRUE);
	/*the following paragraph deals with the redraw problem of fattify
	  area.  It makes it an user-defined object, puts the FARDRAW routine
	  address "drawaddr" in its spec.  FARDRAW will call dr_code and pass
	  dr_code the parmblk provided by AES  to redraw the fattify area*/
	
 	ini_tree(&ad_view, GRID);
	tree = ad_view;		/*tree is in the macro of OB_TYPE*/
	ob_setxywh(ad_view, ROOT, &view);
	rc_copy(&view, &fat_area);
	if(!toolp)	 
		fat_area.g_x += 0x0060;	/*width of tool box 2 =2 * ICON_W*/
	LWSET(OB_TYPE(GRIDWIND), G_USERDEF);
	grid_wind.ub_code = drawaddr;/*grid_wind is the APPLBK/USERBLK strct*/
	LLSET(OB_SPEC(GRIDWIND), ADDR(&grid_wind));
	
	if(LLOBT(GET_TYPE(save_tree, save_obj)) == G_ICON)
		gl_isicon = TRUE;
	ibuff_size = (LONG) (MAX_ICON_W>>3) * (LONG) MAX_ICON_H *
		     (LONG) gl_nplanes;
      
	hold_mfdb.mp = dos_alloc( ibuff_size );
	if( hold_mfdb.mp == 0 )
		return( 1 );	     /* not enough memory */
	disp_mfdb.mp = dos_alloc( ibuff_size );
	if( disp_mfdb.mp == 0 )
		return( 1 );	     /* not enough memory */
	undo_mfdb.mp = dos_alloc( ibuff_size );
	if ( undo_mfdb.mp == 0 )
		return( 1 );
	clip_mfdb.mp = dos_alloc( ibuff_size );
	if ( clip_mfdb.mp == 0 )
		return( 1 );
	save_mfdb.mp = dos_alloc( ibuff_size );
	if ( save_mfdb.mp == 0 )
		return( 1 );
	if(gl_isicon){	
		und2_mfdb.mp = dos_alloc( ibuff_size );
		if ( und2_mfdb.mp == 0 )
			return( 1 );
		clp2_mfdb.mp = dos_alloc( ibuff_size );
		if ( clp2_mfdb.mp == 0 )
			return( 1 );
		hld2_mfdb.mp = dos_alloc( ibuff_size );
		if( hld2_mfdb.mp == 0 )
			return( 1 );	     /* not enough memory */
		sav2_mfdb.mp = dos_alloc( ibuff_size );
		if ( sav2_mfdb.mp == 0 )
			return( 1 );
	}
#if I8086
    wind_get(0, WF_SCREEN, &loword, &hiword, &llword, &hlword);
#else
    wind_get(0, WF_SCREEN, &hiword, &loword, &hlword, &llword);
#endif

    fat_buf = FALSE;
    if(llword || hlword){	/*check if internal buffer is there*/
	fat_mfdb.mp = (LONG)(hiword);
        fat_mfdb.mp <<= 16;
	fat_mfdb.mp += loword;
	ibuff_size= (LONG)(hlword);
	ibuff_size <<= 16;
	ibuff_size += llword;
	}
    else{ /*fat_mfdb.fww * 8 * 2 = fat_mfdb.fwp,no. of bytes per fat row*/
	fat_mfdb.mp = dos_alloc( ibuff_size );
	if( fat_mfdb.mp == 0 )
		return( 1 );	     /* not enough memory */
	else fat_buf = TRUE;
	}	
	icn_init(FALSE);
	colour = fgcolor;	/*default colour*/
	flash_area.g_w = flash_area.g_h = selec_area.g_w = selec_area.g_h = 0;
	selecton = FALSE;
	icn_edited = FALSE;
	grid = FALSE;
	objc_xywh(ad_itool, CLIPSHOW, &dispc_area);
	dispc_area.g_x += 2;
	dispc_area.g_y += 2;
	dispc_area.g_w -= 4;
	orign_area.g_w = orign_area.g_h = 0; /*clip board drawing rectangle*/
/*	gridbx.g_w = 0;	*/
        rcs_hot = NIL;
	set_icnmenus();
	clr_hot();
	icnhot ();
	invert_obj(ad_itool, rcs_hot = DOTBOX, &itool);
	pen_on = FCLORBOX;
	invert_obj(ad_itool, pen_on, &itool);
	wind_update(FALSE);
	return(0);
	}

	VOID icon_edit()
	{	      	   
	WORD	iterm_type;

	if( !(iterm_type = iconinit()))
		iconmain();
	iconterm(iterm_type);
 	 }	   
