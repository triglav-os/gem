/*
 * Defines the shared global state used across the imported desktop shell.
 * Centralizing this state makes the later porting work easier to reason
 * about.
 *
 * MIT License (see: LICENSE)
 * Copyright (C) 2026 tomaz stih
 */

#include <portab.h>
#include <machine.h>
#include <obdefs.h>
#include <taddr.h>
#include <deskapp.h>
#include <deskfpd.h>
#include <deskwin.h>
#include <dos.h>
#include <infodef.h>
#include <desktop.h>
#include <gembind.h>
#include <deskbind.h>

/* Local behavior fixes kept from the imported code. */
/* this has been pulled out of GLOBES & moved here from deskbind.h	*/
GLOBAL ICONBLK		gl_icons[NUM_WOBS];

GLOBAL GLOBES		G;  
