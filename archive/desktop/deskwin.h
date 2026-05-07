/*
 * Declares the window-node structures and helper interfaces used by the
 * desktop shell. The definitions describe how the desktop tracks open
 * folder windows and their state.
 *
 * MIT License (see: LICENSE)
 * Copyright (C) 2026 tomaz stih
 */

#define DROOT 1
#define NUM_WNODES 2
#define NUM_WOBS 300
#define NUM_SOBS (NUM_WOBS + NUM_WNODES + 1)

#define WNODE struct windnode

WNODE
{
	WNODE		*w_next;
	WORD		w_flags;
	WORD		w_id;			/* window handle id #	*/
	WORD		w_obid;			/* desktop object id	*/
	WORD		w_root;			/* pseudo root ob. in	*/
						/*   gl_screen for this	*/
						/*   windows objects	*/
	WORD		w_cvcol;		/* current virt. col	*/
	WORD		w_cvrow;		/* current virt. row	*/
	WORD		w_pncol;		/* physical # of cols	*/
	WORD		w_pnrow;		/* physical # of rows	*/
	WORD		w_vncol;		/* virtual # of cols 	*/
	WORD		w_vnrow;		/* virtual # of rows	*/
	PNODE		*w_path;
	BYTE		w_name[LEN_ZPATH];
/*	BYTE		w_info[81];		NOT USED v2.1		*/
};



