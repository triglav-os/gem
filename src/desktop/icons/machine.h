/*
 * Defines compiler and low-level helper macros used by the icon-pack
 * tooling sources. The header is retained mainly for historical
 * compatibility with the imported tool.
 *
 * MIT License (see: LICENSE)
 * Copyright (C) 2026 tomaz stih
 */

#define	PCDOS	1	/* Ibm Pc Dos */
#define	CPM	0	/* CP/M version 2.2 */

#define HILO 0		/* how bytes are stored */

#define I8086	1	/* Intel 8086/8088 */
#define	MC68K	0	/* Motorola 68000 */

#define ALCYON	0	/* Alcyon C Compiler */


						/* like strcpy but for	*/
						/*   long pointers to	*/
						/*   null-term. strings	*/
#define LSTRCPY(s,d) LBCOPY(d, s, LSTRLEN(s)+1)

						/* coerce short ptr to	*/
						/*   low word  of long	*/
#define LW(x) ( (LONG)((UWORD)(x)) )

						/* coerce short ptr to	*/
						/*   high word  of long	*/
#define HW(x) ((LONG)((UWORD)x) << 16)
						/* Defined in large.a86. */
						/* return length of 	*/
						/*   string pointed at	*/
						/*   by long pointer	*/
EXTERN WORD LSTRLEN(BYTE *text);
						/* return long address	*/
						/*   of short ptr	*/
EXTERN LONG	ADDR();
						/* return long address	*/
						/*   of the data seg	*/
EXTERN LONG LLDS(void);
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
						/* return low word of	*/
						/*   a long value	*/
#define LLOWD(x) ((UWORD)(x))
						/* return high word of	*/
						/*   a long value	*/
#define LHIWD(x) ((UWORD)(x >> 16))
						/* return low byte of	*/
						/*   a word value	*/
#define LLOBT(x) ((BYTE)(x))
						/* return high byte of	*/
						/*   a word value	*/
#define LHIBT(x) ((BYTE)(x >> 8))
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

