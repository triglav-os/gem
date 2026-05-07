/*
 * Declares desktop-specific binding helpers used when talking to AES and
 * VDI services. It keeps the imported desktop code organized around the
 * original interface boundary.
 *
 * MIT License (see: LICENSE)
 * Copyright (C) 2026 tomaz stih
 */

#if MULTIAPP
#define NUM_ADTREES 16
#else
#define NUM_ADTREES 14
#endif

#define	THESCREEN 0
#define THEBAR 1
#define THEACTIVE 2


#define THEMENUS 7
							/* Desk Menu	*/
#define L1ITEM 10           
#define DSK1ITEM 11
#define DSK2ITEM 12          
#define DSK3ITEM 13
#define DSK4ITEM 14      
#define DSK5ITEM 15     
#define DSK6ITEM 16   
#define FROOT 0
#define FSTITLE 1
#define FDIRECTORY 2
#define FSELECTION 3
#define FCLSBOX 4
#define FTITLE 5
#define FILEBOX 6
#define F1NAME 7
#define F2NAME 8
#define F3NAME 9
#define F4NAME 10
#define F5NAME 11
#define F6NAME 12
#define F7NAME 13
#define F8NAME 14
#define F9NAME 15
#define SCRLBAR 16
#define FUPAROW 17
#define FDNAROW 18
#define FSVSLID 19
#define FSVELEV 20
#define OK 21
#define CANCEL 22

#ifndef ARROW
#define ARROW 0x0
#endif
#define HRGLASS 0x2

typedef struct moblk
{
	WORD		m_out;
	WORD		m_x;
	WORD		m_y;
	WORD		m_w;
	WORD		m_h;
} MOBLK ;

#if MULTIAPP
#define DESKPID 0
#define GEMPID 1
#endif


#define MAX_OBS 60
#define MAX_LEVEL 8

#define GLOBES struct glnode
GLOBES
{
						/* fixed pool of file nodes used to	*/
						/*   represent directory contents in	*/
						/*   desktop and folder windows		*/
FNODE	g_flist[NUM_FNODES];
FNODE	*g_favail;

						/* fixed pool of path nodes used to	*/
						/*   remember open folder locations	*/
						/*   and pseudo-root relationships	*/
PNODE	g_plist[NUM_PNODES];
PNODE	*g_pavail;
PNODE	*g_phead;

						/* wildcard specification and DTA	*/
						/*   buffer passed into DOS search	*/
						/*   calls, plus long-pointer aliases	*/
BYTE		g_wspec[LEN_ZPATH];
LONG		a_wspec;
BYTE		g_wdta[128];
LONG		a_wdta;

						/* live folder-window descriptors and	*/
						/*   a count of currently allocated	*/
						/*   window records				*/
WNODE	g_wlist[NUM_WNODES];
WORD		g_wcnt;

/* Local behavior fixes kept from the imported code. */
/* This legacy icon array now lives in deskglob.c as `gl_icons[]`. */
/* ICONBLK	g_icons[NUM_WOBS];*/

						/* maps each desktop object slot to the	*/
						/*   icon template index used to render	*/
						/*   it in the live screen tree		*/
WORD		g_index[NUM_WOBS];

						/* user-defined draw callbacks used when	*/
						/*   desktop items are shown in text	*/
						/*   view rather than icon view		*/
USERBLK	g_udefs[NUM_WOBS];

						/* view related parms	*/
WORD		g_num;			/* number of points 	*/
WORD		*g_pxy;		/* outline pts to drag	*/
WORD		g_iview;		/* current view type	*/
WORD		g_iwext;		/* w,h of extent of a	*/
WORD		g_ihext;		/*   single iten	*/
WORD		g_iwint;		/* w,h of interval	*/
WORD		g_ihint;		/*   between item	*/
WORD		g_iwspc;		/* w,h of extent of a	*/
WORD		g_ihspc;		/*   single iten	*/
WORD		g_incol;		/* # of cols in full	*/
						/*   window		*/
WORD		g_isort;		/* current sort type	*/

						/* source and destination path buffers	*/
						/*   for copy, delete, and install	*/
						/*   operations					*/
BYTE		g_srcpth[82];
BYTE		g_dstpth[82];
				 		/* data xfer buffer and	*/
						/*   length for copying	*/
LONG		g_xbuf;
UWORD	g_xlen;
						/* stack of DTA/FCB entries used by	*/
						/*   deskdir.c to walk directory	*/
						/*   trees iteratively instead of	*/
						/*   recursing through DOS state	*/
FCB		g_fcbstk[MAX_LEVEL];
						/* running totals collected while	*/
						/*   counting, copying, deleting, or	*/
						/*   sizing a directory subtree		*/
LONG		g_nfiles;
LONG		g_ndirs;
LONG		g_size;

						/* shared scratch path buffer used for	*/
						/*   temporary path assembly, parsing,	*/
						/*   and window-target calculations	*/
BYTE		g_tmppth[82];

						/* flattened x,y point list for the	*/
						/*   currently selected desktop objects,	*/
						/*   mainly used while drawing outlines	*/
						/*   and drag feedback rectangles	*/
WORD		g_xyobpts[MAX_OBS * 2];


						/* last AES message buffer processed by	*/
						/*   the desktop event loop and a long	*/
						/*   pointer alias for AES calls that	*/
						/*   still expect an address parameter	*/
WORD		g_rmsg[8];
LONG		a_rmsg;


						/* desktop work area returned by AES	*/
						/*   for window 0 after subtracting the	*/
						/*   menu bar and border constraints	*/
WORD		g_xdesk;
WORD		g_ydesk;
WORD		g_wdesk;
WORD		g_hdesk;

						/* maximum window rectangle available to	*/
						/*   desktop folder windows after the	*/
						/*   shell applies its own padding and	*/
						/*   split-screen startup layout rules	*/
WORD		g_xfull;
WORD		g_yfull;
WORD		g_wfull;
WORD		g_hfull;

						/* process launch command path and its	*/
						/*   GEM long-pointer alias for shel_*	*/
						/*   and pro_* calls				*/
BYTE		g_cmd[128];
LONG		a_cmd;

						/* process launch command tail in GEM	*/
						/*   format, with a parallel long-pointer	*/
						/*   alias for legacy shell interfaces	*/
BYTE		g_tail[128];
LONG		a_tail;

						/* default DOS-style FCB work buffers and	*/
						/*   their long-pointer aliases for file	*/
						/*   execution and compatibility calls	*/
BYTE		g_fcb1[36];
LONG		a_fcb1;

BYTE		g_fcb2[36];
LONG		a_fcb2;
	
						/* current alert string address passed to	*/
						/*   form_alert(), usually sourced from	*/
						/*   resources or formatted scratch text	*/
LONG		a_alert;

						/* resource tree addresses loaded from	*/
						/*   desktop.rsc and reused throughout	*/
						/*   menus, dialogs, and info panels	*/
LONG		a_trees[NUM_ADTREES];

						/* current root object being acted on:	*/
						/*   either the desktop root or one of	*/
						/*   the folder window roots		*/
WORD		g_croot;		/* current pseudo root	*/

						/* current window handle and the window	*/
						/*   that most recently owned the active	*/
						/*   selection set				*/
WORD		g_cwin;			/* current window #	*/
WORD		g_wlastsel;		/* window holding last	*/
						/*   selection		*/
						/* current desktop preferences restored	*/
						/*   from DESKTOP.INF and mirrored into	*/
						/*   the live menus and view logic	*/
WORD		g_csortitem;		/* curr. sort item chked*/
WORD		g_ccopypref;		/* curr. copy pref.	*/
WORD		g_cdelepref;		/* curr. delete pref.	*/
WORD		g_covwrpref;		/* curr. overwrite pref.*/
WORD		g_cdclkpref;		/* curr. double click	*/
WORD		g_cmclkpref;		/* curr. menu click	*/
WORD		g_ctimeform;		/* curr. time format	*/
WORD		g_cdateform;		/* curr. date format	*/

						/* general-purpose desktop scratch text	*/
						/*   buffers used while formatting	*/
						/*   alerts, commands, and dialog text	*/
BYTE		g_1text[256];
BYTE		g_2text[256];

						/* icon-grid cell size, drag outline	*/
						/*   point counts, and precomputed icon	*/
						/*   and text-view outline polygons	*/
WORD		g_icw;
WORD		g_ich;
WORD		g_nmicon;
WORD		g_nmtext;
WORD		g_xyicon[18];
WORD		g_xytext[18];

						/* bounding size of one desktop icon	*/
						/*   slot, including glyph and label	*/
WORD		g_wicon;
WORD		g_hicon;

						/* loaded DESKTOP.INF image size and a	*/
						/*   moving string cursor while parsing	*/
						/*   or serializing application data	*/
WORD		g_afsize;
BYTE		*g_pbuff;

						/* installed-application node pool, free	*/
						/*   list, and active list head used for	*/
						/*   file associations and desk icons	*/
ANODE	g_alist[NUM_ANODES];
ANODE	*g_aavail;
ANODE	*g_ahead;

						/* default icon templates, mutable live	*/
						/*   icon copies, and a shared icon mask	*/
						/*   workspace loaded from DESKTOP.APP	*/
ICONBLK	g_idlist[NUM_IBLKS];
ICONBLK	g_iblist[NUM_IBLKS];
WORD		g_ismask[NUM_IBLKS*2];


						/* saved desktop context persisted to and	*/
						/*   restored from DESKTOP.INF		*/
CSAVE	g_cnxsave;

						/* base addresses of icon bitmap data and	*/
						/*   string storage loaded from the desk	*/
						/*   application definition file		*/
LONG		a_datastart;
LONG		a_buffstart;

						/* long-pointer alias and backing storage	*/
						/*   for the live desktop object tree	*/
LONG		a_screen;
OBJECT	g_screen[NUM_SOBS];		/* Num_Sobs */
};
