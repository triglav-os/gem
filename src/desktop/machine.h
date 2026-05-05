/*
 * Defines compiler, memory-model, and helper macros used throughout the
 * imported desktop shell. Many entries document assumptions inherited from
 * the original 16-bit environment.
 *
 * MIT License (see: LICENSE)
 * Copyright (C) 2026 tomaz stih
 */

#include <stdint.h>

#define	PCDOS	1	/* Ibm Pc Dos */
#define	CPM	0	/* CP/M version 2.2 */

#define HILO 0		/* how bytes are stored */

#ifndef I8086
#define I8086	1	/* Intel 8086/8088 */
#endif
#define	MC68K	0	/* Motorola 68000 */

#define ALCYON	0	/* Alcyon C Compiler */
#define HIGH_C	1	/* for use with MetaWare High-C compiler	*/

#define ALPHA	1	/* if character screen	*/

#define LINKED	0	/* if desktop linked with GEM	*/
#define UNLINKED 1

#define DEBUG 0

#define MULTIAPP 0
#define SINGLAPP 1

#if MULTIAPP
#define NUM_WIN 12		/* 12 for 11 process entries		*/
#define NUM_ACCS 10		/* 10 for multi-process version		*/
#else
#define NUM_WIN 8		/* 8 for main app and 3 desk accs	*/
#define NUM_ACCS 3		/* 3 for number of desk accs	*/
#endif

#define NUM_DESKACC 12		/* at least 12 slots for	*/
				/*   3 desk accessories or	*/
				/*   11 process entries		*/
				/* requires new string array	*/
				/*   in gemmnlib.c if num != 12	*/

/* ----- Defined in optimize.c. ------------------------------------- */
#ifndef GEM_USE_LIBC_STRINGS
EXTERN BYTE *strcpy(BYTE *ps, BYTE *pd);
EXTERN BYTE *strcat(BYTE *ps, BYTE *pd);
EXTERN BYTE *strscn(BYTE *ps, BYTE *pd, BYTE stop);
#endif
/* ----- Defined in large.a86. -------------------------------------- */
/* Returns the length of the string referenced by a long pointer. */
EXTERN WORD LSTRLEN(BYTE *text);
/* Copies `count` words from one long pointer to another. */
EXTERN WORD LWCOPY(LONG dst, LONG src, WORD count);
/* Copies `count` bytes from one long pointer to another. */
EXTERN BYTE LBCOPY(LONG dst, LONG src, WORD count);
/* Copies bytes into words until a null terminator is reached. */
EXTERN WORD LBWMOV(LONG dst, LONG src);

EXTERN WORD LSTCPY(LONG dst, LONG src);
						/* coerce short ptr to	*/
						/*   low word  of long	*/
#define LW(x) ( (LONG)((UWORD)(x)) )
						/* coerce short ptr to	*/
						/*   high word  of long	*/
/*
#define HW(x) ((LONG)((UWORD)(x)) << 16)
*/
EXTERN LONG HW(UWORD x);
						/* return low word of	*/
						/*   a long value	*/
#define LLOWD(x) ((UWORD)(x))
						/* return high word of	*/
						/*   a long value	*/
/*
#define LHIWD(x) ((UWORD)(x >> 16))
*/
EXTERN WORD LHIWD(LONG x);
						/* return low byte of	*/
						/*   a word value	*/
#define LLOBT(x) ((BYTE)(x & 0x00ff))
						/* return high byte of	*/
						/*   a word value	*/
#define LHIBT(x) ((BYTE)( (x >> 8) & 0x00ff))

/* ----- Hosted pointer and address helpers. ------------------------- */

#if I8086
#define FAR					/* hosted flat pointer	*/
#define NEAR					/* hosted flat pointer	*/

						/* return short pointer	*/
						/* from long address	*/
#define LPOINTER(x) ((VOID *)(uintptr_t)(x))
						/* return long address	*/
						/*   of short ptr	*/
/*EXTERN LONG	ADDR();*/
#define ADDR(x) ((LONG) (FAR WORD *) (x))

						/* return long address	*/
						/*   of the data seg	*/
EXTERN LONG LLDS(void);

						/* return long address	*/
						/*   of the code seg	*/
EXTERN LONG LLCS(void);
						/* return a single byte	*/
						/*   pointed at by long	*/
						/*   ptr		*/
#define LBGET(x) ( (UBYTE) *((FAR BYTE * )(x)) )
/*EXTERN BYTE	LBGET();*/
						/* set a single byte	*/
						/*   pointed at by long	*/
						/*   ptr, LBSET(lp, bt)	*/
#define LBSET(x, y)  ( *((FAR BYTE *)(x)) = y)
/*EXTERN BYTE	LBSET();*/
						/* return a single word	*/
						/*   pointed at by long	*/
						/*   ptr		*/
#define LWGET(x) ( (WORD) *((FAR WORD *)(x)) )
/*EXTERN WORD	LWGET();*/
						/* set a single word	*/
						/*   pointed at by long	*/
						/*   ptr, LWSET(lp, bt)	*/
#define LWSET(x, y)  ( *((FAR WORD *)(x)) = y)
/*EXTERN WORD	LWSET();*/
						/* return a single long	*/
						/*   pointed at by long	*/
						/*   ptr		*/
#define LLGET(x) ( *((FAR LONG *)(x)))
/*EXTERN LONG	LLGET();*/
						/* set a single long	*/
						/*   pointed at by long	*/
						/*   ptr, LLSET(lp, bt)	*/
#define LLSET(x, y) ( *((FAR LONG *)(x)) = y)
/*EXTERN LONG	LLSET();*/

						/* convert long ptr to	*/
						/* actual number of 	*/
						/* bytes from 0:0	*/

#define LOFFSET(x) ((((x)&0xFFFF0000l)>>12)+((x)&0x0FFFFl))

						/* convert offset to	*/
						/* long pointer		*/

#define LSEGOFF(x) ((((x)+15)&0xFFFFFFF0)<<12)

						/* return 0th byte of	*/
						/*   a long value given	*/
						/*   a short pointer to	*/
						/*   the long value	*/
#define LBYTE0(x) (*x)
						/* return 1st byte of	*/
						/*   a long value given	*/
						/*   a short pointer to	*/
						/*   the long value	*/
#define LBYTE1(x) (*(x+1))
						/* return 2nd byte of	*/
						/*   a long value given	*/
						/*   a short pointer to	*/
						/*   the long value	*/
#define LBYTE2(x) (*(x+2))
						/* return 3rd byte of	*/
						/*   a long value given	*/
						/*   a short pointer to	*/
						/*   the long value	*/
#define LBYTE3(x) (*(x+3))

#endif


/* ----- Alternate Motorola 68000 helpers. -------------------------- */

#if MC68K

						/* return short pointer	*/
						/* from long address	*/
#define LPOINTER /**/
						/* return a long address*/
						/*   of a short pointer */
#define ADDR /**/
						/* convert long ptr to	*/
						/* actual number of 	*/
						/* bytes from 0:0	*/
#define LOFFSET  /**/

						/* return a single byte	*/
						/*   pointed at by long	*/
						/*   ptr		*/
#define LBGET(x) ( (UBYTE) *((BYTE * )(x)) )
						/* set a single byte	*/
						/*   pointed at by long	*/
						/*   ptr, LBSET(lp, bt)	*/
#define LBSET(x, y)  ( *((BYTE *)(x)) = y)
						/* return a single word	*/
						/*   pointed at by long	*/
						/*   ptr		*/
#define LWGET(x) ( (WORD) *((WORD *)(x)) )
						/* set a single word	*/
						/*   pointed at by long	*/
						/*   ptr, LWSET(lp, bt)	*/
#define LWSET(x, y)  ( *((WORD *)(x)) = y)

						/* return a single long	*/
						/*   pointed at by long	*/
						/*   ptr		*/
#define LLGET(x) ( *((LONG *)(x)))
						/* set a single long	*/
						/*   pointed at by long	*/
						/*   ptr, LLSET(lp, bt)	*/
#define LLSET(x, y) ( *((LONG *)(x)) = y)

						/* return 0th byte of	*/
						/*   a long value given	*/
						/*   a short pointer to	*/
						/*   the long value	*/
#define LBYTE0(x) ( *((x)+3) )
						/* return 1st byte of	*/
						/*   a long value given	*/
						/*   a short pointer to	*/
						/*   the long value	*/
#define LBYTE1(x) ( *((x)+2) )
						/* return 2nd byte of	*/
						/*   a long value given	*/
						/*   a short pointer to	*/
						/*   the long value	*/
#define LBYTE2(x) ( *((x)+1) )
						/* return 3rd byte of	*/
						/*   a long value given	*/
						/*   a short pointer to	*/
						/*   the long value	*/
#define LBYTE3(x) (*(x))

#endif

/* ----- High C compatibility. --------------------------------------- */

#if HIGH_C
#undef VOID
#define VOID void
EXTERN VOID dbg();
EXTERN VOID dump();
#endif
