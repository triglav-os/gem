/*
 * Implements the icon-pack builder that combines individual icon include
 * files into desktop icon resources. It is auxiliary tooling rather than
 * part of the runtime shell.
 *
 * MIT License (see: LICENSE)
 * Copyright (C) 2026 tomaz stih
 */

#include <portab.h>
#include <machine.h>
#include <obdefs.h>

WORD		beg_file;

#include <iconlist.h>

WORD		end_file;

main()
{
	WORD		handle, length, ret;

	beg_file = &gl_ilist;
	handle = dos_create( ADDR("DESKTOP.IMG"), 0);
	length = ( ((BYTE *)&end_file) - ((BYTE *)&beg_file) );
	ret = dos_write(handle, length-2, ADDR(&beg_file+1) );
	dos_close(handle);

} /* compicon */
