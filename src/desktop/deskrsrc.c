/*
 * Implements desktop resource loading, string initialization, and
 * menu/dialog setup. It connects the desktop shell logic to its compiled
 * resources.
 *
 * MIT License (see: LICENSE)
 * Copyright (C) 2026 tomaz stih
 */

#include "portab.h"
#include "machine.h"
#include "obdefs.h"

#define R_STRING 5

EXTERN WORD rsrc_gaddr(WORD rstype, WORD rsid, LONG *paddr);

GLOBAL	BYTE	gl_lngstr[256];

/*
 * Returns a desktop resource string copied into the shared long-string
 * scratch buffer.
 *
 * `stnum` is a desktop string resource id such as `STGEMAPP` or
 * `ST_DISKCOPY`.
 * Returns a pointer to `gl_lngstr`.
 */
BYTE *ini_str(WORD stnum)
{
	LONG		lstr;

	rsrc_gaddr(R_STRING, stnum, &lstr);
	LSTCPY(ADDR(&gl_lngstr[0]), lstr);
	return(&gl_lngstr[0]);
}
