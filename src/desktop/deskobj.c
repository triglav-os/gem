/*
 * Implements object-tree helpers specific to desktop resources and icon
 * views. It provides the geometry and state glue needed by desktop dialogs
 * and windows.
 *
 * MIT License (see: LICENSE)
 * Copyright (C) 2026 tomaz stih
 */

#include <portab.h>
#include <machine.h>
#include <obdefs.h>
#include <deskapp.h>
#include <deskfpd.h>
#include <deskwin.h>
#include <dos.h>
#include <infodef.h>
#include <desktop.h>
#include <gembind.h>
#include <deskbind.h>

EXTERN VOID movs(WORD num, VOID *ps, VOID *pd);
EXTERN VOID r_set(GRECT *rect, WORD x, WORD y, WORD w, WORD h);
EXTERN WORD objc_add(LONG tree, WORD parent, WORD child);

EXTERN WORD	gl_width;
EXTERN WORD	gl_height;

GLOBAL OBJECT	gl_sampob[2] =
{
	NIL, NIL, NIL, G_IBOX, NONE, NORMAL, 0x0L, 0, 0, 0, 0,
	NIL, NIL, NIL, G_BOX,  NONE, NORMAL, 0x00001100L, 0, 0, 0, 0
};

EXTERN	GLOBES		G;

/*
*	Initialize all objects as children of the 0th root which is
*	the parent of unused objects.
*/
VOID obj_init(VOID)
{
	WORD		ii;
	LONG		tree;

	tree = G.a_screen = ADDR(&G.g_screen[0]);
	for (ii = 0; ii < NUM_SOBS; ii++)
	{
	  G.g_screen[ii].ob_head = NIL;
	  G.g_screen[ii].ob_next = NIL;
	  G.g_screen[ii].ob_tail = NIL;
	}
	movs(sizeof(OBJECT), &gl_sampob[0], &G.g_screen[ROOT]);
	r_set(&G.g_screen[ROOT].ob_x, 0, 0, gl_width, gl_height);
	for (ii = 0; ii < (NUM_WNODES+1); ii++)
	{
	  movs(sizeof(OBJECT), &gl_sampob[1], &G.g_screen[DROOT+ii]);
	  objc_add(tree, ROOT, DROOT+ii);
	} /* for */
} /* obj_init */

/*
*	Allocate a window object from the screen tree by looking for 
*	the child of the parent with no size
*/
WORD obj_walloc(WORD x, WORD y, WORD w, WORD h)
{
	WORD		ii;

	for (ii = DROOT; ii < (NUM_WNODES+1); ii++)
	{
	  if ( !(G.g_screen[ii+1].ob_width && G.g_screen[ii+1].ob_height) )
	    break;
	}
	if ( ii < (NUM_WNODES+1) )
	{
	  r_set(&G.g_screen[ii+1].ob_x, x, y, w, h);
	  return(ii+1);
	}
else return(0);
} /* obj_walloc */

/*
*	Free a window object by changing its size to zero and
*	NILing out all its children.
*/
VOID obj_wfree(WORD obj, WORD x, WORD y, WORD w, WORD h)
{
	WORD		ii, nxtob;

	r_set(&G.g_screen[obj].ob_x, x, y, w, h);
	for (ii = G.g_screen[obj].ob_head; ii > obj; ii = nxtob)
	{
	  nxtob = G.g_screen[ii].ob_next;
	  G.g_screen[ii].ob_next = NIL;
	}
	G.g_screen[obj].ob_head = G.g_screen[obj].ob_tail = NIL;
} /* obj_wfree */

/*
*	Routine to find and allocate a particular item object.  The
*	next free object is found by looking for any object that
*	is available (i.e., a next pointer of NIL).
*/
WORD obj_ialloc(WORD wparent, WORD x, WORD y, WORD w, WORD h)
{
	WORD		ii;

	for (ii = NUM_WNODES+2; ii < NUM_SOBS; ii++)
	{
	  if (G.g_screen[ii].ob_next == NIL)
	    break;
	}
	if (ii < NUM_SOBS)
	{
	  G.g_screen[ii].ob_head = G.g_screen[ii].ob_tail = NIL;
	  objc_add(G.a_screen, wparent, ii);
	  r_set(&G.g_screen[ii].ob_x, x, y, w, h);
	  return(ii);
	}
else return(0);
} /* obj_ialloc */
