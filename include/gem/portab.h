/*
 * Defines legacy GEM portability aliases and constants for the Linux
 * port. The original GEM code expects uppercase type names such as
 * `WORD` and `LONG`, so this header preserves those names while mapping
 * them to hosted C11 integer types. `LONG` and `ULONG` use pointer-width
 * integer types in this port because historical AES and VDI structures
 * store host pointers in those fields.
 *
 * MIT License (see: LICENSE)
 * Copyright (C) 2026 tomaz stih
 */

#ifndef PORTAB_H
#define PORTAB_H

#include <stdint.h>

#define BYTE    char           /* Signed character type. */
#define UBYTE   unsigned char  /* Unsigned byte type. */
#define WORD    int16_t        /* Signed 16-bit integer type. */
#define UWORD   uint16_t       /* Unsigned 16-bit integer type. */
#define LONG    intptr_t       /* Pointer-width signed integer type. */
#define ULONG   uintptr_t      /* Pointer-width unsigned integer type. */
#define BOOLEAN int16_t        /* Historical GEM boolean type. */

#define REG     register       /* Historical register storage hint. */
#define LOCAL   auto           /* Historical local storage alias. */
#define EXTERN  extern         /* Historical external linkage alias. */
#define MLOCAL  static         /* Historical internal linkage alias. */
#define GLOBAL                 /* Historical global definition marker. */
#define VOID    void           /* Historical alias for `void`. */
#define CONST   const          /* Historical alias for `const`. */

#define FAILURE (-1)           /* Conventional failure status. */
#define SUCCESS (0)            /* Conventional success status. */
#define YES     1              /* Historical affirmative constant. */
#define NO      0              /* Historical negative constant. */
#define FOREVER for (;;)       /* Infinite loop helper macro. */
#define NULL    0              /* Historical null integer constant. */
#define NULLPTR ((void *) 0)   /* Explicit null pointer constant. */
#define TRUE    (1)            /* Historical boolean true constant. */
#define FALSE   (0)            /* Historical boolean false constant. */

#endif /* PORTAB_H */
