/*
 * Defines the low-resolution desktop icon include list and ordering. The
 * manifest controls how low-resolution icon assets are packed into the
 * desktop resources.
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
	0x0L,  0x1L,  -1L, 0x1000,6,7, 15,0,48,24, 4,24,72,10,	/* Ighard 0 */
0x2L,  0x3L,  -1L, 0x1000,7,5,  15,0,48,24, 4,24,72,10,	/* Igfloppy 1 */
0x4L,  0x5L,  -1L, 0x1000,0,0,  15,0,48,24, 4,24,72,10,	/* Igfolder 2 */
0x6L,  0x7L,  -1L, 0x1000,0,0,  15,0,48,24, 4,24,72,10,	/* Igtrash 3 */
0x8L,  0x9L,  -1L, 0x1000,0,0,  15,0,48,24, 4,24,72,10,	/* Ig4Resv 4 */
0x8L,  0x9L,  -1L, 0x1000,0,0,  15,0,48,24, 4,24,72,10,	/* Ig5Resv 5 */
0x8L,  0x9L,  -1L, 0x1000,0,0,  15,0,48,24, 4,24,72,10,	/* Ig6Resv 6 */
0x8L,  0x9L,  -1L, 0x1000,0,0,  15,0,48,24, 4,24,72,10,	/* Ig7Resv 7 */
/* Application Icons:	*/
	0x8L,  0x9L,  -1L, 0x1000,0,0,  15,0,48,24, 4,24,72,10,	/* Iagener 8 */
0x8L,  0xAL,  -1L, 0x1000,0,0,  15,0,48,24, 4,24,72,10,	/* Iass 9 */
0x8L,  0xBL,  -1L, 0x1000,0,0,  15,0,48,24, 4,24,72,10,	/* Iawp 10 */
0x8L,  0xCL,  -1L, 0x1000,0,0,  15,0,48,24, 4,24,72,10,	/* Iadb 11 */
0x8L,  0xDL,  -1L, 0x1000,0,0,  15,0,48,24, 4,24,72,10,	/* Iadraw 12 */
0x8L,  0xEL,  -1L, 0x1000,0,0,  15,0,48,24, 4,24,72,10,	/* Iapaint 13 */
0x8L,  0xFL, -1L, 0x1000,0,0,  15,0,48,24, 4,24,72,10,	/* Iaproj 14 */
0x8L,  0x10L, -1L, 0x1000,0,0,  15,0,48,24, 4,24,72,10,	/* Iagraph 15 */
0x8L,  0x11L, -1L, 0x1000,0,0,  15,0,48,24, 4,24,72,10,	/* Iaoutl 16 */
0x8L,  0x12L, -1L, 0x1000,0,0,  15,0,48,24, 4,24,72,10,	/* Iaaccnt 17 */
0x8L,  0x13L, -1L, 0x1000,0,0,  15,0,48,24, 4,24,72,10,	/* Iamulti 18 */
0x8L,  0x14L, -1L, 0x1000,0,0,  15,0,48,24, 4,24,72,10,	/* Iaeduc 19 */
0x8L,  0x15L, -1L, 0x1000,0,0,  15,0,48,24, 4,24,72,10,	/* Iacomm 20 */
0x8L,  0x16L, -1L, 0x1000,0,0,  15,0,48,24, 4,24,72,10,	/* Iatool 21 */
0x8L,  0x17L, -1L, 0x1000,0,0,  15,0,48,24, 4,24,72,10,	/* Iagame 22 */
0x8L,  0x18L, -1L, 0x1000,0,0,  15,0,48,24, 4,24,72,10,	/* Iaoutpt 23 */
0x8L,  0x19L, -1L, 0x1000,0,0,  15,0,48,24, 4,24,72,10,	/* Iadpub 24 */
0x8L,  0x1AL, -1L, 0x1000,0,0,  15,0,48,24, 4,24,72,10,	/* Iascan 25 */
0x8L,  0x1BL, -1L, 0x1000,0,0,  15,0,48,24, 4,24,72,10,	/* Iamail 26 */
/* Currently unused application icons. */
	0x8L,  0x9L, -1L, 0x1000,0,0,  15,0,48,24, 4,24,72,10,	/* Iarsv 4 */
0x8L,  0x9L, -1L, 0x1000,0,0,  15,0,48,24, 4,24,72,10,	/* Iarsv 5 */
0x8L,  0x9L, -1L, 0x1000,0,0,  15,0,48,24, 4,24,72,10,	/* Iarsv 6 */
0x8L,  0x9L, -1L, 0x1000,0,0,  15,0,48,24, 4,24,72,10,	/* Iarsv 7 */
0x8L,  0x9L, -1L, 0x1000,0,0,  15,0,48,24, 4,24,72,10,	/* Iarsv 8 */
0x8L,  0x9L, -1L, 0x1000,0,0,  15,0,48,24, 4,24,72,10,	/* Iarsv 9 */
0x8L,  0x9L, -1L, 0x1000,0,0,  15,0,48,24, 4,24,72,10,	/* Iarsv 10 */
0x8L,  0x9L, -1L, 0x1000,0,0,  15,0,48,24, 4,24,72,10,	/* Iarsv 11 */
0x8L,  0x9L, -1L, 0x1000,0,0,  15,0,48,24, 4,24,72,10,	/* Iarsv 12 */
0x8L,  0x9L, -1L, 0x1000,0,0,  15,0,48,24, 4,24,72,10,	/* Iarsv 13 */
0x8L,  0x9L, -1L, 0x1000,0,0,  15,0,48,24, 4,24,72,10,	/* Iarsv 14 */
0x8L,  0x9L, -1L, 0x1000,0,0,  15,0,48,24, 4,24,72,10,	/* Iarsv 15 */
0x8L,  0x9L, -1L, 0x1000,0,0,  15,0,48,24, 4,24,72,10,	/* Iarsv 16 */
/* Document Icons:	*/
	0x1CL, 0x1DL, -1L, 0x1000,0,0,  15,0,48,24, 4,24,72,10,	/* Idgener 40 */
0x1CL, 0x1EL, -1L, 0x1000,0,0,  15,0,48,24, 4,24,72,10,	/* Idss 41 */
0x1CL, 0x1FL, -1L, 0x1000,0,0,  15,0,48,24, 4,24,72,10,	/* Idwp 42 */
0x1CL, 0x20L, -1L, 0x1000,0,0,  15,0,48,24, 4,24,72,10,	/* Iddb 43 */
0x1CL, 0x21L, -1L, 0x1000,0,0,  15,0,48,24, 4,24,72,10,	/* Iddraw 44 */
0x1CL, 0x22L, -1L, 0x1000,0,0,  15,0,48,24, 4,24,72,10,	/* Idpaint 45 */
0x1CL, 0x23L, -1L, 0x1000,0,0,  15,0,48,24, 4,24,72,10,	/* Idproj 46 */
0x1CL, 0x24L, -1L, 0x1000,0,0,  15,0,48,24, 4,24,72,10,	/* Idgraph 47 */
0x1CL, 0x25L, -1L, 0x1000,0,0,  15,0,48,24, 4,24,72,10,	/* Idoutln 48 */
0x1CL, 0x26L, -1L, 0x1000,0,0,  15,0,48,24, 4,24,72,10,	/* Idaccnt 49 */
0x1CL, 0x27L, -1L, 0x1000,0,0,  15,0,48,24, 4,24,72,10,	/* Idmulti 50 */
0x1CL, 0x28L, -1L, 0x1000,0,0,  15,0,48,24, 4,24,72,10,	/* Ideduc 51 */
0x1CL, 0x29L, -1L, 0x1000,0,0,  15,0,48,24, 4,24,72,10,	/* Idcomm 52 */
0x1CL, 0x2AL, -1L, 0x1000,0,0,  15,0,48,24, 4,24,72,10,	/* Idtool 53 */
0x1CL, 0x2BL, -1L, 0x1000,0,0,  15,0,48,24, 4,24,72,10,	/* Idgame 54 */
0x1CL, 0x2CL, -1L, 0x1000,0,0,  15,0,48,24, 4,24,72,10,	/* Idoutpt 55 */
0x1CL, 0x2DL, -1L, 0x1000,0,0,  15,0,48,24, 4,24,72,10,	/* Idrsv 1 */
/* currently unused Document icons:	*/
	0x1CL, 0x1DL, -1L, 0x1000,0,0,  15,0,48,24, 4,24,72,10,	/* Idrsv 2 */
0x1CL, 0x1DL, -1L, 0x1000,0,0,  15,0,48,24, 4,24,72,10,	/* Idrsv 3 */
0x1CL, 0x1DL, -1L, 0x1000,0,0,  15,0,48,24, 4,24,72,10,	/* Idrsv 4 */
0x1CL, 0x1DL, -1L, 0x1000,0,0,  15,0,48,24, 4,24,72,10,	/* Idrsv 5 */
0x1CL, 0x1DL, -1L, 0x1000,0,0,  15,0,48,24, 4,24,72,10,	/* Idrsv 6 */
0x1CL, 0x1DL, -1L, 0x1000,0,0,  15,0,48,24, 4,24,72,10,	/* Idrsv 7 */
0x1CL, 0x1DL, -1L, 0x1000,0,0,  15,0,48,24, 4,24,72,10,	/* Idrsv 8 */
0x1CL, 0x1DL, -1L, 0x1000,0,0,  15,0,48,24, 4,24,72,10,	/* Idrsv 9 */
0x1CL, 0x1DL, -1L, 0x1000,0,0,  15,0,48,24, 4,24,72,10,	/* Idrsv 10 */
0x1CL, 0x1DL, -1L, 0x1000,0,0,  15,0,48,24, 4,24,72,10,	/* Idrsv 11 */
0x1CL, 0x1DL, -1L, 0x1000,0,0,  15,0,48,24, 4,24,72,10,	/* Idrsv 12 */
0x1CL, 0x1DL, -1L, 0x1000,0,0,  15,0,48,24, 4,24,72,10,	/* Idrsv 13 */
0x1CL, 0x1DL, -1L, 0x1000,0,0,  15,0,48,24, 4,24,72,10,	/* Idrsv 14 */
0x1CL, 0x1DL, -1L, 0x1000,0,0,  15,0,48,24, 4,24,72,10,	/* Idrsv 15 */
0x1CL, 0x1DL, -1L, 0x1000,0,0,  15,0,48,24, 4,24,72,10	/* Idrsv 16 */
};

#include <ighdskml.icn>	/*x0*/			/* System icons		*/
#include <ighdskdl.icn>
#include <igfdskml.icn>
#include <igfdskdl.icn>
#include <igfoldml.icn>
#include <igfolddl.icn>
#include <igfold3l.icn>
#include <igflop3l.icn>	/*x7*/
/* Placeholders for future icons
#include <igres4ml.icn>
#include <igres4dl.icn>
#include <igres5ml.icn>
#include <igres5dl.icn>
#include <igres6ml.icn>
#include <igres6dl.icn>
#include <igres7ml.icn>
#include <igres7dl.icn>
#include <igres8ml.icn>
#include <igres8dl.icn>
*/
#include <iagenrml.icn>	/*x8*/			/* Application Icons	*/
#include <iagenrdl.icn>
#include <iasprddl.icn>
#include <iaworddl.icn>
#include <iadbasdl.icn>
#include <iadrawdl.icn>
#include <iapantdl.icn>
#include <iaprojdl.icn>
#include <iagrphdl.icn>	/*x10*/
#include <iaoutldl.icn>
#include <iaacctdl.icn>
#include <iamultdl.icn>
#include <iaeducdl.icn>
#include <iacommdl.icn>
#include <iatooldl.icn>
#include <iagamedl.icn>
#include <iaoutpdl.icn>	/*x18*/
#include <iadpubdl.icn>
#include <iascandl.icn>
#include <iamaildl.icn>
/* Placeholders for future Application icons
#include <iars04dl.icn>
#include <iars05dl.icn>
#include <iars06dl.icn>
#include <iars07dl.icn>
#include <iars08dl.icn>
#include <iars09dl.icn>
#include <iars10dl.icn>
#include <iars11dl.icn>
#include <iars12dl.icn>
#include <iars13dl.icn>
#include <iars14dl.icn>
#include <iars15dl.icn>
#include <iars16dl.icn>
*/
#include <idgenrml.icn>				/* Document Icons	*/
#include <idgenrdl.icn>
#include <idsprddl.icn>
#include <idworddl.icn>
#include <iddbasdl.icn>
#include <iddrawdl.icn>
#include <idpantdl.icn>	/*x20*/
#include <idprojdl.icn>
#include <idgrphdl.icn>
#include <idoutldl.icn>
#include <idacctdl.icn>
#include <idmultdl.icn>
#include <ideducdl.icn>
#include <idcommdl.icn>
#include <idtooldl.icn>	/*x28*/
#include <idgamedl.icn>
#include <idoutpdl.icn>
#include <iddpubdl.icn>
/* Placeholders for future Document Icons
#include <idrs02dl.icn>
#include <idrs03dl.icn>
#include <idrs04dl.icn>
#include <idrs05dl.icn>
#include <idrs06dl.icn>
#include <idrs07dl.icn>
#include <idrs08dl.icn>
#include <idrs09dl.icn>
#include <idrs10dl.icn>
#include <idrs11dl.icn>
#include <idrs12dl.icn>
#include <idrs13dl.icn>
#include <idrs14dl.icn>
#include <idrs15dl.icn>
#include <idrs16dl.icn>
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
