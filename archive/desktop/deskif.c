/*
 * Implements the desktop-side interface glue between shell logic and GEM
 * services. It helps isolate the imported shell code from direct binding
 * details.
 *
 * MIT License (see: LICENSE)
 * Copyright (C) 2026 tomaz stih
 */

#include <portab.h>
#include <machine.h>
#include <obdefs.h>
#include <gembind.h>

EXTERN WORD graf_mouse(WORD m_number, LONG m_addr);

VOID gsx_moff(VOID)
{
	graf_mouse(M_OFF, 0x0L);
}

VOID gsx_mon(VOID)
{
	graf_mouse(M_ON, 0x0L);
}

LONG obaddr(LONG tree, WORD obj, WORD fld_off)
{
	return( (tree + ((obj) * sizeof(OBJECT) + fld_off)) );
}
