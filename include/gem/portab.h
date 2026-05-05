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

#define BYTE    char
#define UBYTE   unsigned char
#define WORD    int16_t
#define UWORD   uint16_t
#define LONG    intptr_t
#define ULONG   uintptr_t
#define BOOLEAN int16_t

#define REG     register
#define LOCAL   auto
#define EXTERN  extern
#define MLOCAL  static
#define GLOBAL
#define VOID    void
#define CONST   const

#define FAILURE (-1)
#define SUCCESS (0)
#define YES     1
#define NO      0
#define FOREVER for (;;)
#define NULL    0
#define NULLPTR ((void *) 0)
#define TRUE    (1)
#define FALSE   (0)

#endif /* PORTAB_H */
