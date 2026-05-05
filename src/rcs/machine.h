/*
 * Defines compiler, memory-model, and helper macros used throughout the
 * imported resource construction set. The header preserves 16-bit
 * assumptions that the Linux port will later address.
 *
 * MIT License (see: LICENSE)
 * Copyright (C) 2026 tomaz stih
 */

#define	PCDOS	1	/* Ibm Pc Dos */
#define	CPM	0	/* CP/M version 2.2 */
#define GEMDOS  0	/* Dri Gem Dos */
#define HILO 0		/* how bytes are stored */
#define MC68K   0       /* 68000 processor */

#define I8086 	1 	/* Intel 8086/8088 */
					 
#define ALCYON  0       /* Alcyon C compiler */

#define ALPHA	1	/* if character screen	*/

#define LINKED	0	/* if desktop linked with GEM	*/
#define UNLINKED 1


						/* Defined in optimize.c. */
						/* Defined in large.a86. */
						/* return length of 	*/
						/*   string pointed at	*/
						/*   by long pointer	*/
EXTERN WORD LSTRLEN(BYTE *text);
						/* copy n words from	*/
						/*   src long ptr to	*/
						/*   dst long ptr i.e.,	*/
						/*   LWCOPY(dlp, slp, n)*/
EXTERN WORD LWCOPY(LONG dst, LONG src, WORD count);
						/* copy n words from	*/
						/*   src long ptr to	*/
						/*   dst long ptr i.e.,	*/
						/*   LBCOPY(dlp, slp, n)*/
EXTERN BYTE LBCOPY(LONG dst, LONG src, WORD count);
						/* move bytes into wds*/
						/*   from src long ptr to*/
						/*   dst long ptr i.e.,	*/
						/*   until a null is	*/
						/*   encountered, then	*/
						/*   num moved is returned*/
						/*   LBWMOV(dwlp, sblp)*/
EXTERN WORD LBWMOV(LONG dst, LONG src);

EXTERN WORD LSTCPY(LONG dst, LONG src);
						/* coerce short ptr to	*/
						/*   low word  of long	*/
#define LW(x) ( (LONG)((UWORD)(x)) )
						/* coerce short ptr to	*/
						/*   high word  of long	*/

#define HW(x) ((LONG)((UWORD)(x)) << 16)

						/* return low word of	*/
						/*   a long value	*/



#define LLOWD(x) ((UWORD)(x))
						/* return high word of	*/
						/*   a long value	*/
#define LHIWD(x) ((UWORD)(x >> 16))
						/* return low byte of	*/
						/*   a word value	*/



#define LLOBT(x) ((BYTE)(x & 0x00ff))
						/* return high byte of	*/
						/*   a word value	*/
#define LHIBT(x) ((BYTE)( (x >> 8) & 0x00ff))

EXTERN BYTE	*strcat();

/************************************************************************/

#if	I8086

EXTERN BYTE *strcpy(BYTE *ps, BYTE *pd);
EXTERN BYTE	*strscn();

						/* return long address	*/
						/*   of short ptr	*/
EXTERN LONG	ADDR();

						/* return long address	*/
						/*   of the data seg	*/
EXTERN LONG LLDS(void);

						/* return long address	*/
						/*   of the code seg	*/
EXTERN LONG LLCS(void);
						/* return a single byte	*/
						/*   pointed at by long	*/
						/*   ptr		*/
EXTERN BYTE	LBGET();
						/* set a single byte	*/
						/*   pointed at by long	*/
						/*   ptr, LBSET(lp, bt)	*/
EXTERN BYTE	LBSET();
						/* return a single word	*/
						/*   pointed at by long	*/
						/*   ptr		*/
EXTERN WORD	LWGET();
						/* set a single word	*/
						/*   pointed at by long	*/
						/*   ptr, LWSET(lp, bt)	*/
EXTERN WORD	LWSET();
						/* return a single long	*/
						/*   pointed at by long	*/
						/*   ptr		*/
EXTERN LONG	LLGET();
						/* set a single long	*/
						/*   pointed at by long	*/
						/*   ptr, LLSET(lp, bt)	*/
EXTERN LONG	LLSET();
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


/************************************************************************/

#if	 MC68K

						/* return a long address*/
						/*   of a short pointer */
#define ADDR /**/


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
