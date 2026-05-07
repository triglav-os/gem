/*
 * Declares shared desktop constants, globals, and entry points used across
 * the shell sources. It describes the common state model that ties the
 * desktop modules together.
 *
 * MIT License (see: LICENSE)
 * Copyright (C) 2026 tomaz stih
 */

#define STNOAPPL 24  	/* String */
#define STNOWIND 25  	/* String */
#define STNOFRMT 26  	/* String */
#define STFORMAT 27  	/* String */
#define STBADCOP 28  	/* String */
#define STDELDIS 29  	/* String */
#define STAPGONE 30  	/* String */
#define STFOEXIS 31  	/* String */
#define STDISKFU 32  	/* String */
#define STNOFILE 33  	/* String */
#define STNOINF 34  	/* String */
#define STFO8DEE 35  	/* String */
#define STDEEPPA 36  	/* String */
#define ADMENU 0  	/* Tree */
#define ADFILEIN 1  	/* Tree */
#define ADDISKIN 2  	/* Tree */
#define ADFOLDIN 3  	/* Tree */
#define ADNFINFO 5  	/* Tree */
#define ADOPENAP 6  	/* Tree */
#define ADINSDIS 7  	/* Tree */
#define ADINSAPP 8  	/* Tree */
#define ADCOPYDI 9  	/* Tree */
#define ADDELEDI 10  	/* Tree */
#define ADCPALER 11  	/* Tree */
#define ADMKDBOX 12  	/* Tree */
#define ADSETPRE 13  	/* Tree */
#define DESKMENU 3  	/* OBJECT in TREE #0 */
#define FILEMENU 4  	/* OBJECT in TREE #0 */
#define OPTNMENU 5  	/* OBJECT in TREE #0 */
#define VIEWMENU 6  	/* OBJECT in TREE #0 */
#define ABOUITEM 9  	/* OBJECT in TREE #0 */
#define OPENITEM 18  	/* OBJECT in TREE #0 */
#define SHOWITEM 19  	/* OBJECT in TREE #0 */
#define L2ITEM 20  	/* OBJECT in TREE #0 */
#define DELTITEM 21  	/* OBJECT in TREE #0 */
#define FORMITEM 22  	/* OBJECT in TREE #0 */
#define OUTPITEM 24  	/* OBJECT in TREE #0 */
#define L3ITEM 23  	/* OBJECT in TREE #0 */
#define QUITITEM 25  	/* OBJECT in TREE #0 */
#define IDSKITEM 27  	/* OBJECT in TREE #0 */
#define IAPPITEM 28  	/* OBJECT in TREE #0 */
#define L5ITEM 29  	/* OBJECT in TREE #0 */
#define PREFITEM 30  	/* OBJECT in TREE #0 */
#define SAVEITEM 31  	/* OBJECT in TREE #0 */
#define DOSITEM 32  	/* OBJECT in TREE #0 */
#define ICONITEM 34  	/* OBJECT in TREE #0 */
#define L4ITEM 35  	/* OBJECT in TREE #0 */
#define NAMEITEM 36  	/* OBJECT in TREE #0 */
#define TYPEITEM 37  	/* OBJECT in TREE #0 */
#define SIZEITEM 38  	/* OBJECT in TREE #0 */
#define DATEITEM 39  	/* OBJECT in TREE #0 */
#define FITITLE 1  	/* OBJECT in TREE #1 */
#define FINAME 2  	/* OBJECT in TREE #1 */
#define FISIZE 3  	/* OBJECT in TREE #1 */
#define FIDATE 4  	/* OBJECT in TREE #1 */
#define FITIME 5  	/* OBJECT in TREE #1 */
#define FIATTRS 6  	/* OBJECT in TREE #1 */
#define FIRWRITE 8  	/* OBJECT in TREE #1 */
#define FIRONLY 9  	/* OBJECT in TREE #1 */
#define FIOK 10  	/* OBJECT in TREE #1 */
#define FICNCL 11  	/* OBJECT in TREE #1 */
#define DITITLE 1  	/* OBJECT in TREE #2 */
#define DIDRIVE 2  	/* OBJECT in TREE #2 */
#define DIVOLUME 3  	/* OBJECT in TREE #2 */
#define DINFOLDS 4  	/* OBJECT in TREE #2 */
#define DINFILES 5  	/* OBJECT in TREE #2 */
#define DIUSED 6  	/* OBJECT in TREE #2 */
#define DIAVAIL 7  	/* OBJECT in TREE #2 */
#define DIOK 8  	/* OBJECT in TREE #2 */
#define INSAPP 1  	/* OBJECT in TREE #8 */
#define APNAME 2  	/* OBJECT in TREE #8 */
#define APDOCU 3  	/* OBJECT in TREE #8 */
#define APDFTYPE 4  	/* OBJECT in TREE #8 */
#define APTYPE 12  	/* OBJECT in TREE #8 */
#define APGEM 14  	/* OBJECT in TREE #8 */
#define APDOS 15  	/* OBJECT in TREE #8 */
#define APPARMS 16  	/* OBJECT in TREE #8 */
#define APMEM 17  	/* OBJECT in TREE #8 */
#define APMEMBOX 18  	/* OBJECT in TREE #8 */
#define APYMEM 19  	/* OBJECT in TREE #8 */
#define APNMEM 20  	/* OBJECT in TREE #8 */
#define APICON 21  	/* OBJECT in TREE #8 */
#define APFTITLE 22  	/* OBJECT in TREE #8 */
#define APFUPARO 27  	/* OBJECT in TREE #8 */
#define APFDNARO 28  	/* OBJECT in TREE #8 */
#define APINST 31  	/* OBJECT in TREE #8 */
#define APREMV 32  	/* OBJECT in TREE #8 */
#define APCNCL 33  	/* OBJECT in TREE #8 */
#define APFILEBO 23  	/* OBJECT in TREE #8 */
#define APF1NAME 24  	/* OBJECT in TREE #8 */
#define APF2NAME 25  	/* OBJECT in TREE #8 */
#define APSCRLBA 26  	/* OBJECT in TREE #8 */
#define APFSVSLI 29  	/* OBJECT in TREE #8 */
#define APFSVELE 30  	/* OBJECT in TREE #8 */
#define DRID 2  	/* OBJECT in TREE #7 */
#define DRDINS 1  	/* OBJECT in TREE #7 */
#define DRLABEL 3  	/* OBJECT in TREE #7 */
#define DRTYPE 4  	/* OBJECT in TREE #7 */
#define DRBUTNS 5  	/* OBJECT in TREE #7 */
#define DRFLOPPY 6  	/* OBJECT in TREE #7 */
#define DRHARD 7  	/* OBJECT in TREE #7 */
#define DRINST 8  	/* OBJECT in TREE #7 */
#define DRREM 9  	/* OBJECT in TREE #7 */
#define DRCNCL 10  	/* OBJECT in TREE #7 */
#define MKNAME 2  	/* OBJECT in TREE #12 */
#define MKOK 3  	/* OBJECT in TREE #12 */
#define MKCNCL 4  	/* OBJECT in TREE #12 */
#define FOLINFO 1  	/* OBJECT in TREE #3 */
#define FOLNAME 2  	/* OBJECT in TREE #3 */
#define FOLDATE 3  	/* OBJECT in TREE #3 */
#define FOLTIME 4  	/* OBJECT in TREE #3 */
#define FOLNFOLD 5  	/* OBJECT in TREE #3 */
#define FOLNFILE 6  	/* OBJECT in TREE #3 */
#define FOLSIZE 7  	/* OBJECT in TREE #3 */
#define FOLOK 8  	/* OBJECT in TREE #3 */
#define DDDELETE 1  	/* OBJECT in TREE #10 */
#define DDFOLDS 2  	/* OBJECT in TREE #10 */
#define DDFILES 3  	/* OBJECT in TREE #10 */
#define DDOK 4  	/* OBJECT in TREE #10 */
#define DDCNCL 5  	/* OBJECT in TREE #10 */
#define APPLOPEN 1  	/* OBJECT in TREE #6 */
#define APPLNAME 2  	/* OBJECT in TREE #6 */
#define APPLOK 4  	/* OBJECT in TREE #6 */
#define APPLCNCL 5  	/* OBJECT in TREE #6 */
#define APPLPARM 3  	/* OBJECT in TREE #6 */
#define NFINOK 7  	/* OBJECT in TREE #5 */
#define CDCOPY 1  	/* OBJECT in TREE #9 */
#define CDFOLDS 2  	/* OBJECT in TREE #9 */
#define CDFILES 3  	/* OBJECT in TREE #9 */
#define CDOK 4  	/* OBJECT in TREE #9 */
#define CDCNCL 5  	/* OBJECT in TREE #9 */
#define CACOPY 1  	/* OBJECT in TREE #11 */
#define CACURRNA 2  	/* OBJECT in TREE #11 */
#define CACOPYNA 3  	/* OBJECT in TREE #11 */
#define CAOK 4  	/* OBJECT in TREE #11 */
#define CACNCL 5  	/* OBJECT in TREE #11 */
#define CASTOP 6  	/* OBJECT in TREE #11 */
#define SPTITLE 1  	/* OBJECT in TREE #13 */
#define SPCNFDEL 2  	/* OBJECT in TREE #13 */
#define SPCDYES 4  	/* OBJECT in TREE #13 */
#define SPCDNO 5  	/* OBJECT in TREE #13 */
#define SPCNFCOP 6  	/* OBJECT in TREE #13 */
#define SPCCYES 8  	/* OBJECT in TREE #13 */
#define SPCCNO 9  	/* OBJECT in TREE #13 */
#define SPDCLICK 14  	/* OBJECT in TREE #13 */
#define SPDC1 16  	/* OBJECT in TREE #13 */
#define SPDC2 17  	/* OBJECT in TREE #13 */
#define SPDC3 18  	/* OBJECT in TREE #13 */
#define SPDC4 19  	/* OBJECT in TREE #13 */
#define SPDC5 20  	/* OBJECT in TREE #13 */
#define SPSEYES 27  	/* OBJECT in TREE #13 */
#define SPSENO 28  	/* OBJECT in TREE #13 */
#define SPOK 37  	/* OBJECT in TREE #13 */
#define SPCNCL 38  	/* OBJECT in TREE #13 */
#define STINFOST 0		/* Free String */
#define STASICON 1		/* Free String */
#define STAPPL 2		/* Free String */
#define STDOCU 3		/* Free String */
#define STDKFRM1 4		/* Free String */
#define STASTEXT 5		/* Free String */
#define STGEMHIC 6		/* Free String */
#define STGEMAPP 7		/* Free String */
#define STGEMLOI 8		/* Free String */
#define STGEMOUT 9		/* Free String */
#define STGEMBAT 10		/* Free String */
#define ST1STD 11		/* Free String */
#define ST2STD 12		/* Free String */
#define ST3STD 13		/* Free String */
#define ST4STD 14		/* Free String */
#define ST5STD 15		/* Free String */
#define ST6STD 16		/* Free String */
#define SPTF12HR 31  	/* OBJECT in TREE #13 */
#define SPTF24HR 32  	/* OBJECT in TREE #13 */
#define SPDFMMDD 35  	/* OBJECT in TREE #13 */
#define SPDFDDMM 36  	/* OBJECT in TREE #13 */
#define ADDINFO 4  	/* Tree */
#define DEOK 11  	/* OBJECT in TREE #4 */
#define DEICON 7  	/* OBJECT in TREE #4 */
#define STDKFRM2 17		/* Free String */
#define STNEWFOL 18		/* Free String */
#define STDSKDRV 19		/* Free String */
#define STAM 20		/* Free String */
#define STPM 21		/* Free String */
#define SPMNCLKY 22  	/* OBJECT in TREE #13 */
#define SPMNCLKN 23  	/* OBJECT in TREE #13 */
#define SPCOWYES 12  	/* OBJECT in TREE #13 */
#define SPCOWNO 13  	/* OBJECT in TREE #13 */
