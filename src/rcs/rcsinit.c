/*
 * Initializes the resource construction set, loads its resources, and
 * prepares editor-global state. It is the main startup file for the
 * imported editor.
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

	GLOBAL	WORD	gl_apid;

#define		DEBUG 0

#if DEBUG

#define		MX_MESG_LEN	40
#define		TILDE		0x7E
#define		PERIOD		0x2E



/*----------------------------------------------------------------------*/
/* puts up a form alert with the 'stop' icon and one exit button.	*/
VOID do_falert(BYTE *line1, BYTE *line2, BYTE *line3, BYTE *line4, BYTE *line5)
{
BYTE	tmp[ 256 ] ;

    strcpy( "[3][" , tmp ) ;
    strcat( line1, tmp ) ;
    strcat( "|", tmp ) ;
    strcat( line2, tmp ) ;
    strcat( "|", tmp ) ;
    strcat( line3, tmp ) ;
    strcat( "|", tmp ) ;
    strcat( line4, tmp ) ;
    strcat( "|", tmp ) ;
    strcat( line5, tmp ) ;
    strcat( "][ Exit ]" , tmp ) ;
    form_alert( 1, ADDR( tmp ) ) ;
} /* do_falert */


/*----------------------------------------------------------------------*/
/* b comes in the range [0..15d] and is returned in the range [30..39h] */
/* and [41..46h] for the ascii characters [`0'..`F'].			*/
VOID i_to_hex(BYTE *b)
{
    if ( ( *b < 0 ) || ( *b > 15 ) )
	*b = TILDE ;	
    else
	{
	if ( *b < 10 )
	    *b += '0' ;
	else 
	    *b += ('A' - 10) ;
	}
} /* i_to_hex */

byte_stuff(strptr, length, chr)
BYTE	*strptr, chr;
WORD	length;
{      
WORD	ii ;   

    for ( ii = (length-1); ii >= 0; ii-- )
	strptr[ ii ] = chr ;
}
		    

/*----------------------------------------------------------------------*/
/* takes a string of the form ["hello"] and returns the string ["68.65. */
/* 6c.6c.6f.00.00.00.00....."].	Notice the new string is three times the*/
/* length of the original.						*/
VOID byte_to_hex(BYTE *b)
{
BYTE	nib1, nib2, bite ;
WORD	ii ;
BYTE	tmp[ MX_MESG_LEN ] ;

    byte_stuff( tmp, MX_MESG_LEN, PERIOD ) ;
    for ( ii = 0; 3*ii < MX_MESG_LEN; ii++ )
	{
	bite = b[ ii ] ;
	nib1 = bite >> 4 ;
	nib2 = bite & 0x0F ;
	i_to_hex( &nib1 ) ;
  	i_to_hex( &nib2 ) ;
	tmp[ ii*3 ] = nib1 ;
	tmp[ ii*3 + 1 ] = nib2 ;
	}
    tmp[ MX_MESG_LEN -1 ] = NULL ;
    strcpy( tmp, b ) ;
} /* byte_to_hex */


/*----------------------------------------------------------------------*/
/* call show_hex() to display hex dumps during runtime not necessarily  */
/* under GEMSID.  ie: great for dumping shel_reads, writes.  Call where */
/* MX_MESG_LEN is defined as 40 or less and DEBUG is defined 0 or 1 	*/

VOID show_hex(BYTE *mesg)
{
BYTE	lines[ 5 ][ MX_MESG_LEN ] ;
WORD	inc, ii ;

    byte_stuff( lines, sizeof( lines ), NULL ) ;
    inc = MX_MESG_LEN / 3 ;
    for ( ii = 0; ii < 5; ii++ )
	{
	movs( inc, &mesg[ ii*inc ], &lines[ ii ] ) ;
	byte_to_hex( &lines[ ii ] ) ;
	}
    do_falert( &lines[ 0 ], &lines[ 1 ], &lines[ 2 ], &lines[ 3 ], &lines[ 4 ] ) ;
} /* show_hex */

#endif


	WORD
ini_rsrc()			/* load RCS resources */
	{
	LONG	tree;
	WORD	i;

	if ( !rsrc_load( ADDR(gl_hchar >= 12 ? "rcs.rsc" : "rcslow.rsc")) )
		{
		hndl_alert(1, ADDR(
			"[3][Fatal Error !|rcs.rsc not found.][Abort]"));
		return (FALSE);
		}
	for (i = 0; i <= POPCOLOR; i++)
		{
		ini_tree(&tree, i);	
		map_tree(tree, ROOT, NIL, trans_obj);
		}
	for (i = 0; i<= CLIPFULL; i++)
		trans_bitblk(image_addr(i));
	return (TRUE);
	}


	WORD ini_windows(view_parts)
	WORD	view_parts;
	{
	wind_get(0, WF_WXYWH, &full.g_x, &full.g_y, 
		&full.g_w, &full.g_h);
	rcs_view = wind_create(view_parts, full.g_x - 1, full.g_y,
		full.g_w + 1, full.g_h);
	if (rcs_view == NIL)
		{
		hndl_alert(1, string_addr(STNWNDW));
		return (FALSE);
		}
	clr_title();
	set_slsize(rcs_view, 1000, 1000);
	wind_open(rcs_view, full.g_x - 1, full.g_y, full.g_w + 1, full.g_h);

	partp = toolp = TRUE;
	ini_panes();

	send_redraw(rcs_view, &view);
	return (TRUE);
	}

	VOID rd_inf(flag)
	WORD flag;
	{
	WORD	len;
	
	strcpy("RCS.APP",rcs_app);			     
	shel_find(ADDR(rcs_app));
	strcpy(&rcs_app[0], rcs_infile);
	len = strlen(rcs_infile) - 1;
	while ( len && rcs_infile[len] != '.' ) len--;
	strcpy( ".INF", &rcs_infile[len] );
				
#if	MC68K	  
	 /* D drive on Meridian System is Read Only */
	/*  Therefore if both C and D drives exist in the system */
	/*  Assume it is a Meridian .  */
	if( rcs_app[0] == 'D')
		if( hard_drive & 0x1000 && hard_drive & 0x2000 ) 
		   /* both C and D hard drives exist */
			rcs_infile[0] = 'C'; /* assume C drive is data drive */
	     
#endif		
	read_inf(flag);	/* set up defaults for SAFETY and OUTPUT options */
	}
	
	
	WORD rcs_init()
	{
	BYTE	cmd[128], tail[128];	   
	WORD	path, i;

#if I8086
	if ( (gl_apid = appl_init()) == NIL)
#endif
#if MC68K
	if ( (gl_apid = appl_init()) == NIL)
#endif
		return (5);
	hard_drive = global[14];
	shel_read(ADDR(cmd), ADDR(tail));


	wind_update(TRUE);
	gl_handle = graf_handle(&gl_wchar, &gl_hchar, &gl_wbox, &gl_hbox);  
	if (!gl_handle)
		return (4);
	gsx_vopen();
	gsx_start();
	mouse_form(HGLASS);

	if (!ini_rsrc())		/* read resource file */
		return (3);
	ini_tree(&ad_menu, 0);		/* find tree addresses */
	menu_bar(ad_menu, TRUE);	/* enable menus */
	ini_tree(&ad_tools, TOOLBOX);	/* initialize toolbox */
	ad_view = ADDR(&rcs_work[0]);	/* set up on-the-fly tree */

	new_state(NOFILE_STATE);	/* sets up menu states & pbx */

	if (tail[0] && tail[1])
		{
#if	DEBUG 
	show_hex( tail );
#endif


		tail[ tail[0] + 1 ] = '\0';
		path = FALSE;
		for ( i = 0; i < strlen( tail ); i++)
		    if  ( tail[i] == '\\' ) 
		    {
		       path = TRUE;
		       break;
		    }
		if (!path)
		   {
#if MC68K
		   get_path(rcs_rfile, "");
		   strcat( &tail[1], rcs_rfile);
		   r_to_xfile( rcs_rfile, "RSC");
#else
		   strcpy( &tail[1], rcs_rfile);
	           r_to_xfile( rcs_rfile,"RSC" );
		   shel_find(ADDR(rcs_rfile));	  
		   dos_chdir(ADDR(rcs_rfile));
#endif
		   }
		else                     /*path name is there*/
		   {
		   strcpy( &tail[1], rcs_rfile);
		   r_to_xfile(rcs_rfile,"RSC");
		   dos_chdir(ADDR(rcs_rfile));
	           }
		strcpy (rcs_rfile, &rsc_path[1]);
		for(i=strlen(rcs_rfile); i && (rsc_path[i] != '\\'); i--);
		rsc_path[0] = i;
		rsc_path[i+1] = '\0';
		}
	else
		{
		rd_inf(TRUE); /*find rsc_path in the .inf file*/
		}
	if (!ini_windows(0x0fc3))
		return (2); 
	rcs_nsel = 0;			/* nothing selected	*/
	wait_tools();
	rcs_hot = NIL;			/* no tools active	*/
	buff_size = dos_avail();
	if ( buff_size < 0x10700L )
		head = dos_alloc( buff_size -= 0x700L);
	else
		head = dos_alloc( buff_size = 0x10000L); 
	buff_size -= 2500L;		/* Reserve a panic buffer */ 
	if (buff_size < 1000L)
		{
		hndl_alert(1, string_addr(STNMEM));
		return (1);
		}
	ini_buff();  


	if (tail[0] && tail[1]) /*a bug in 3.0: a null string have 0D len*/
		{
		r_to_xfile(rcs_dfile, "DFN");
		return(ok_rvrt_files()); /* call cont_rcsinit from root */
		}
	else
		{
		view_trees();
		return(cont_rcsinit(TRUE));
		}
	}


	
	WORD
cont_rcsinit(flag)    
	/* tail end of rcs_init procedurized to resolve calls between */
	/* overlay siblings. */
	WORD flag;
	{  
	rd_inf(flag);
	mouse_form( ARROW );	/* ready to fly, change form */
	wind_update(FALSE);
	return (0);
	}


	
	VOID rcs_exit(term_type)
	WORD	term_type;
	{
	switch (term_type) {
		case 0:
			dos_free(head);
		case 1:
			wind_close( rcs_view );
			wind_delete( rcs_view );
		case 2:
			menu_bar(ad_menu, FALSE);
			rsrc_free();
		case 3:
			gsx_vclose();
		case 4:
			appl_exit();
		case 5:
			break;
		}
	}
