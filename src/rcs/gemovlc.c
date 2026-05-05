/*
 * Implements C-side support for the original overlay management used by
 * the resource construction set. It is kept as reference because the
 * imported editor was split across overlays.
 *
 * MIT License (see: LICENSE)
 * Copyright (C) 2026 tomaz stih
 */

#include "portab.h"
#include "rcs.h"

EXTERN WORD rsrc_gaddr(WORD type, WORD index, void **addr);
EXTERN WORD form_alert(WORD defbut, LONG astring);

/*--------------------------------------------------------------*/
WORD _ovlerr(VOID)
{
LONG	al_addr ;

    rsrc_gaddr( 5, NOOVLERR, &al_addr ) ;
    form_alert( 1,al_addr ) ;
} /* _ovlerr */

