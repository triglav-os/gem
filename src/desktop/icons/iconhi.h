/*
 * Defines the high-resolution desktop icon include list and ordering. The
 * include sequence determines how icon assets are packed into the
 * generated desktop resources.
 *
 * MIT License (see: LICENSE)
 * Copyright (C) 2026 tomaz stih
 */

#define NUM_IBLKS 72
/*ICONBLK
   (L)ib_pmask, (L)ib_pdata, (L)ib_ptext,
   (W)ib_char, (W)ib_xchar, (W)ib_ychar,
   (W)ib_xicon, (W)ib_yicon, (W)ib_wicon, (W)ib_hicon,
   (W)ib_xtext, (W)ib_ytext, (W)ib_wtext, (W)ib_htext;
*/

EXTERN BYTE	*pi[];	    
BYTE	**gl_strs = {&pi[0]};
BYTE	***gl_start = {&gl_strs};

ICONBLK	gl_ilist[NUM_IBLKS] =
{
/* System Icons:	*/
	0x0L,  0x1L,  -1L, 0x1000,5,11, 23,0,32,32, 0,32,72,10,	/* Ighard 0 */
0x2L,  0x3L,  -1L, 0x1000,4,11, 23,0,32,32, 0,32,72,10,	/* Igfloppy 1 */
0x4L,  0x5L,  -1L, 0x1000,0,0,  23,0,32,32, 0,32,72,10,	/* Igfolder 2 */
0x6L,  0x7L,  -1L, 0x1000,0,0,  23,0,32,32, 0,32,72,10,	/* Igtrash 3 */
0x8L,  0x9L,  -1L, 0x1000,0,0,  23,0,32,32, 0,32,72,10,	/* Ig4Resv 4 */
0x8L,  0x9L,  -1L, 0x1000,0,0,  23,0,32,32, 0,32,72,10,	/* Ig5Resv 5 */
0x8L,  0x9L,  -1L, 0x1000,0,0,  23,0,32,32, 0,32,72,10,	/* Ig6Resv 6 */
0x8L,  0x9L,  -1L, 0x1000,0,0,  23,0,32,32, 0,32,72,10,	/* Ig7Resv 7 */
/* Application Icons:	*/
	0x8L,  0x9L,  -1L, 0x1000,0,0,  23,0,32,32, 0,32,72,10,	/* Iagener 8 */
0x8L,  0xAL,  -1L, 0x1000,0,0,  23,0,32,32, 0,32,72,10,	/* Iass 9 */
0x8L,  0xBL,  -1L, 0x1000,0,0,  23,0,32,32, 0,32,72,10,	/* Iawp 10 */
0x8L,  0xCL,  -1L, 0x1000,0,0,  23,0,32,32, 0,32,72,10,	/* Iadb 11 */
0x8L,  0xDL,  -1L, 0x1000,0,0,  23,0,32,32, 0,32,72,10,	/* Iadraw 12 */
0x8L,  0xEL,  -1L, 0x1000,0,0,  23,0,32,32, 0,32,72,10,	/* Iapaint 13 */
0x8L,  0xFL,  -1L, 0x1000,0,0,  23,0,32,32, 0,32,72,10,	/* Iaproj 14 */
0x8L,  0x10L, -1L, 0x1000,0,0,  23,0,32,32, 0,32,72,10,	/* Iagraph 15 */
0x8L,  0x11L, -1L, 0x1000,0,0,  23,0,32,32, 0,32,72,10,	/* Iaoutl 16 */
0x8L,  0x12L, -1L, 0x1000,0,0,  23,0,32,32, 0,32,72,10,	/* Iaaccnt 17 */
0x8L,  0x13L, -1L, 0x1000,0,0,  23,0,32,32, 0,32,72,10,	/* Iamulti 18 */
0x8L,  0x14L, -1L, 0x1000,0,0,  23,0,32,32, 0,32,72,10,	/* Iaeduc 19 */
0x8L,  0x15L, -1L, 0x1000,0,0,  23,0,32,32, 0,32,72,10,	/* Iacomm 20 */
0x8L,  0x16L, -1L, 0x1000,0,0,  23,0,32,32, 0,32,72,10,	/* Iatool 21 */
0x8L,  0x17L, -1L, 0x1000,0,0,  23,0,32,32, 0,32,72,10,	/* Iagame 22 */
0x8L,  0x18L, -1L, 0x1000,0,0,  23,0,32,32, 0,32,72,10,	/* Iaoutpt 23 */
0x8L,  0x19L, -1L, 0x1000,0,0,  23,0,32,32, 0,32,72,10,	/* Iadpub 24 */
0x8L,  0x1AL, -1L, 0x1000,0,0,  23,0,32,32, 0,32,72,10,	/* Iascan 25 */
0x8L,  0x1BL, -1L, 0x1000,0,0,  23,0,32,32, 0,32,72,10,	/* Iamail 26 */
/* Currently unused application icons. */
	0x8L,  0x9L, -1L, 0x1000,0,0,  23,0,32,32, 0,32,72,10,	/* Iarsv 4 */
0x8L,  0x9L, -1L, 0x1000,0,0,  23,0,32,32, 0,32,72,10,	/* Iarsv 5 */
0x8L,  0x9L, -1L, 0x1000,0,0,  23,0,32,32, 0,32,72,10,	/* Iarsv 6 */
0x8L,  0x9L, -1L, 0x1000,0,0,  23,0,32,32, 0,32,72,10,	/* Iarsv 7 */
0x8L,  0x9L, -1L, 0x1000,0,0,  23,0,32,32, 0,32,72,10,	/* Iarsv 8 */
0x8L,  0x9L, -1L, 0x1000,0,0,  23,0,32,32, 0,32,72,10,	/* Iarsv 9 */
0x8L,  0x9L, -1L, 0x1000,0,0,  23,0,32,32, 0,32,72,10,	/* Iarsv 10 */
0x8L,  0x9L, -1L, 0x1000,0,0,  23,0,32,32, 0,32,72,10,	/* Iarsv 11 */
0x8L,  0x9L, -1L, 0x1000,0,0,  23,0,32,32, 0,32,72,10,	/* Iarsv 12 */
0x8L,  0x9L, -1L, 0x1000,0,0,  23,0,32,32, 0,32,72,10,	/* Iarsv 13 */
0x8L,  0x9L, -1L, 0x1000,0,0,  23,0,32,32, 0,32,72,10,	/* Iarsv 14 */
0x8L,  0x9L, -1L, 0x1000,0,0,  23,0,32,32, 0,32,72,10,	/* Iarsv 15 */
0x8L,  0x9L, -1L, 0x1000,0,0,  23,0,32,32, 0,32,72,10,	/* Iarsv 16 */
/* Document Icons:	*/
	0x1CL, 0x1DL, -1L, 0x1000,0,0,  23,0,32,32, 0,32,72,10,	/* Idgener 40 */
0x1CL, 0x1EL, -1L, 0x1000,0,0,  23,0,32,32, 0,32,72,10,	/* Idss 41 */
0x1CL, 0x1FL, -1L, 0x1000,0,0,  23,0,32,32, 0,32,72,10,	/* Idwp 42 */
0x1CL, 0x20L, -1L, 0x1000,0,0,  23,0,32,32, 0,32,72,10,	/* Iddb 43 */
0x1CL, 0x21L, -1L, 0x1000,0,0,  23,0,32,32, 0,32,72,10,	/* Iddraw 44 */
0x1CL, 0x22L, -1L, 0x1000,0,0,  23,0,32,32, 0,32,72,10,	/* Idpaint 45 */
0x1CL, 0x23L, -1L, 0x1000,0,0,  23,0,32,32, 0,32,72,10,	/* Idproj 46 */
0x1CL, 0x24L, -1L, 0x1000,0,0,  23,0,32,32, 0,32,72,10,	/* Idgraph 47 */
0x1CL, 0x25L, -1L, 0x1000,0,0,  23,0,32,32, 0,32,72,10,	/* Idoutln 48 */
0x1CL, 0x26L, -1L, 0x1000,0,0,  23,0,32,32, 0,32,72,10,	/* Idaccnt 49 */
0x1CL, 0x27L, -1L, 0x1000,0,0,  23,0,32,32, 0,32,72,10,	/* Idmulti 50 */
0x1CL, 0x28L, -1L, 0x1000,0,0,  23,0,32,32, 0,32,72,10,	/* Ideduc 51 */
0x1CL, 0x29L, -1L, 0x1000,0,0,  23,0,32,32, 0,32,72,10,	/* Idcomm 52 */
0x1CL, 0x2AL, -1L, 0x1000,0,0,  23,0,32,32, 0,32,72,10,	/* Idtool 53 */
0x1CL, 0x2BL, -1L, 0x1000,0,0,  23,0,32,32, 0,32,72,10,	/* Idgame 54 */
0x1CL, 0x2CL, -1L, 0x1000,0,0,  23,0,32,32, 0,32,72,10,	/* Idoutpt 55 */
0x1CL, 0x2DL, -1L, 0x1000,0,0,  23,0,32,32, 0,32,72,10,	/* Iddpub 56 */
/* currently unused Document icons:	*/
	0x1CL, 0x1DL, -1L, 0x1000,0,0,  23,0,32,32, 0,32,72,10,	/* Idrsv 2 */
0x1CL, 0x1DL, -1L, 0x1000,0,0,  23,0,32,32, 0,32,72,10,	/* Idrsv 3 */
0x1CL, 0x1DL, -1L, 0x1000,0,0,  23,0,32,32, 0,32,72,10,	/* Idrsv 4 */
0x1CL, 0x1DL, -1L, 0x1000,0,0,  23,0,32,32, 0,32,72,10,	/* Idrsv 5 */
0x1CL, 0x1DL, -1L, 0x1000,0,0,  23,0,32,32, 0,32,72,10,	/* Idrsv 6 */
0x1CL, 0x1DL, -1L, 0x1000,0,0,  23,0,32,32, 0,32,72,10,	/* Idrsv 7 */
0x1CL, 0x1DL, -1L, 0x1000,0,0,  23,0,32,32, 0,32,72,10,	/* Idrsv 8 */
0x1CL, 0x1DL, -1L, 0x1000,0,0,  23,0,32,32, 0,32,72,10,	/* Idrsv 9 */
0x1CL, 0x1DL, -1L, 0x1000,0,0,  23,0,32,32, 0,32,72,10,	/* Idrsv 10 */
0x1CL, 0x1DL, -1L, 0x1000,0,0,  23,0,32,32, 0,32,72,10,	/* Idrsv 11 */
0x1CL, 0x1DL, -1L, 0x1000,0,0,  23,0,32,32, 0,32,72,10,	/* Idrsv 12 */
0x1CL, 0x1DL, -1L, 0x1000,0,0,  23,0,32,32, 0,32,72,10,	/* Idrsv 13 */
0x1CL, 0x1DL, -1L, 0x1000,0,0,  23,0,32,32, 0,32,72,10,	/* Idrsv 14 */
0x1CL, 0x1DL, -1L, 0x1000,0,0,  23,0,32,32, 0,32,72,10,	/* Idrsv 15 */
0x1CL, 0x1DL, -1L, 0x1000,0,0,  23,0,32,32, 0,32,72,10	/* Idrsv 16 */
};

#include <ighdskmh.icn>	/*x0*/			/* System icons		*/
#include <ighdskdh.icn>
#include <igfdskmh.icn>
#include <igfdskdh.icn>
#include <igfoldmh.icn>
#include <igfolddh.icn>
#include <igfold3h.icn>
#include <igflop3h.icn>	/*x7*/
/* Placeholders for future icons
#include <igres4mh.icn>
#include <igres4dh.icn>
#include <igres5mh.icn>
#include <igres5dh.icn>
#include <igres6mh.icn>
#include <igres6dh.icn>
#include <igres7mh.icn>
#include <igres7dh.icn>
#include <igres8mh.icn>
#include <igres8dh.icn>
*/
#include <iagenrmh.icn>	/*x8*/			/* Application Icons	*/
#include <iagenrdh.icn>
#include <iasprddh.icn>
#include <iaworddh.icn>
#include <iadbasdh.icn>
#include <iadrawdh.icn>
#include <iapantdh.icn>
#include <iaprojdh.icn>
#include <iagrphdh.icn>	/*x10*/
#include <iaoutldh.icn>
#include <iaacctdh.icn>
#include <iamultdh.icn>
#include <iaeducdh.icn>
#include <iacommdh.icn>
#include <iatooldh.icn>
#include <iagamedh.icn>
#include <iaoutpdh.icn>
#include <iadpubdh.icn> /*x19*/
#include <iascandh.icn>
#include <iamaildh.icn>
/* Placeholders for future Application icons
#include <iars04dh.icn>
#include <iars05dh.icn>
#include <iars06dh.icn>
#include <iars07dh.icn>
#include <iars08dh.icn>
#include <iars09dh.icn>
#include <iars10dh.icn>
#include <iars11dh.icn>
#include <iars12dh.icn>
#include <iars13dh.icn>
#include <iars14dh.icn>
#include <iars15dh.icn>
#include <iars16dh.icn>
*/
#include <idgenrmh.icn>				/* Document Icons	*/
#include <idgenrdh.icn>
#include <idsprddh.icn>
#include <idworddh.icn>
#include <iddbasdh.icn>
#include <iddrawdh.icn>
#include <idpantdh.icn>	/*x20*/
#include <idprojdh.icn>
#include <idgrphdh.icn>
#include <idoutldh.icn>
#include <idacctdh.icn>
#include <idmultdh.icn>
#include <ideducdh.icn>
#include <idcommdh.icn>
#include <idtooldh.icn>	/*x28*/
#include <idgamedh.icn>
#include <idoutpdh.icn>
#include <iddpubdh.icn>
/* Placeholders for future Document Icons
#include <idrs02dh.icn>
#include <idrs03dh.icn>
#include <idrs04dh.icn>
#include <idrs05dh.icn>
#include <idrs06dh.icn>
#include <idrs07dh.icn>
#include <idrs08dh.icn>
#include <idrs09dh.icn>
#include <idrs10dh.icn>
#include <idrs11dh.icn>
#include <idrs12dh.icn>
#include <idrs13dh.icn>
#include <idrs14dh.icn>
#include <idrs15dh.icn>
#include <idrs16dh.icn>
*/

/* Icon names for use in Desktop's Configure Application dialog	*/
BYTE	*pi[32]=
{
	" Generic ",
	" Spreadsheet ",
	" Word Processor ",
	" Database ",
	" Draw ",
	" Paint ",
	" Project ",
	" Graph ",
	" Outline ",
	" Accounting ",
	" Multi-Function ",
	" Education ",
	" Communications ",
	" Programmer's Tool ",
	" Game ",
	" Output ",
	" Desktop Publisher ",
	" Scan ",
	" Mail ",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	""
};
