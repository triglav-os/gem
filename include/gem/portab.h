/*
 * Defines legacy GEM portability aliases and constants for the Linux
 * port. The original GEM code expects uppercase type names such as
 * `WORD` and `LONG`, so this header preserves those names while mapping
 * them to fixed-width C11 integer types.
 *
 * MIT License (see: LICENSE)
 * Copyright (C) 2026 tomaz stih
 */

#ifndef GEM_PORTAB_H
#define GEM_PORTAB_H

#include <stdint.h>

#define BYTE    int8_t
#define UBYTE   uint8_t
#define WORD    int16_t
#define UWORD   uint16_t
#define LONG    int32_t
#define ULONG   uint32_t
#define BOOLEAN int16_t

#define REG     register
#define LOCAL   auto
#define EXTERN  extern
#define MLOCAL  static
#define GLOBAL
#define VOID    void
#define CONST   const
#define DEFAULT int

#define FAILURE (-1)
#define SUCCESS (0)
#define YES     1
#define NO      0
#define FOREVER for (;;)
#define NULLPTR ((void *) 0)
#define TRUE    (1)
#define FALSE   (0)

#endif /* GEM_PORTAB_H */
