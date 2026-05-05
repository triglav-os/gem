/*
 * Declares desktop application-node structures and helper interfaces.
 * These definitions support file associations, installed apps, and icon
 * selection in the desktop shell.
 *
 * MIT License (see: LICENSE)
 * Copyright (C) 2026 tomaz stih
 */

#define AP_APPLOPEN 0
#define AP_DATAOPEN 1
#define AP_DATADRAG 2

#define AF_ISCRYS 0x0001			/* is crystal appl.	*/
#define AF_ISGRAF 0x0002			/* is graphic appl.	*/
#define AF_ISDESK 0x0004			/* is placed on desk	*/
#define AF_ISPARM 0x0008			/* is in need of input	*/
						/*   parameters		*/
#define AF_ISFMEM 0x0010			/* is in need of full	*/
						/*   memory		*/

#define AT_ISFILE 0
#define AT_ISFOLD 1
#define AT_ISDISK 2
#define AT_ISTRSH 3

						/* gem icons		*/
#define IG_HARD 0
#define IG_FLOPPY 1
#define IG_FOLDER 2
#define IG_TRASH 3
#define IG_4RESV 4
#define IG_5RESV 5
#define IG_6RESV 6
#define IG_7RESV 7
						/* application icons	*/
#define IA_GENERIC 8
#define IA_SS 9
#define IA_WP 10
#define IA_DB 11
#define IA_DRAW 12
#define IA_PAINT 13
#define IA_PROJECT 14
#define IA_GRAPH 15
#define IA_OUTLINE 16
#define IA_ACCNT 17
#define IA_MULTI 18
#define IA_EDUC 19
#define IA_COMM 20
#define IA_TOOL 21
#define IA_GAME 22
#define IA_OUTPUT 23
						/* document icons	*/
#define ID_GENERIC 40
#define ID_SS 41
#define ID_WP 42
#define ID_DB 43
#define ID_DRAW 44
#define ID_PAINT 45
#define ID_PROJECT 46
#define ID_GRAPH 47
#define ID_OUTLINE 48
#define ID_ACCNT 49
#define ID_MULTI 50
#define ID_EDUC 51
#define ID_COMM 52
#define ID_TOOL 53
#define ID_GAME 54
#define ID_OUTPUT 55

#define LEN_ZPATH 67
#define LEN_ZFNAME 13

#define NUM_ANODES 64				/* # of appl. nodes	*/
#define SIZE_AFILE 2048				/* size of DESKTOP.APP	*/
						/*   file		*/
#define SIZE_BUFF 4096				/* size of a string 	*/
						/*   buffer		*/
#define NUM_IBLKS 72
#define NUM_NAMICS ((NUM_IBLKS - 8)/2)		/* # of named icons	*/
						/*   possible		*/

#define ANODE struct applstr
ANODE
{
	ANODE		*a_next;
	WORD		a_flags;
	WORD		a_type;			/* icon/appl type	*/
	WORD		a_obid;			/* object index		*/
	BYTE		*a_pappl;		/* filename.ext of ap.	*/
	BYTE		*a_pdata;		/* wldcards of data file*/
	WORD		a_aicon;		/* application icon #	*/
	WORD		a_dicon;		/* data icon #		*/
	WORD		a_letter;		/* letter for icon	*/
	WORD		a_xspot;		/* desired spot on desk	*/
	WORD		a_yspot;		/* desired spot on desk	*/
};

#if MULTIAPP

#define a_memreq	a_xspot
#define DEFMEMREQ	256

#define ACCNODE	struct acc_info
ACCNODE
{
/*	WORD	acc_chan;	*/
	WORD	acc_swap;
	BYTE	acc_name[13];
};

#endif


#define WSAVE	struct window_save
WSAVE
{
	WORD	x_save;
	WORD	y_save;
	WORD	w_save;
	WORD	h_save;
	WORD	hsl_save;
	WORD	vsl_save;
	WORD	obid_save;
	BYTE	pth_save[LEN_ZPATH];
};


#define CSAVE	struct context_save
CSAVE
{
	WORD	sitem_save;
	WORD	vitem_save;
	WORD	ccopy_save;
	WORD	cdele_save;
	WORD	covwr_save;
	WORD	cdclk_save;
	WORD	cmclk_save;
	WORD	ctmfm_save;
	WORD	cdtfm_save;
	WSAVE	win_save[2];
};
