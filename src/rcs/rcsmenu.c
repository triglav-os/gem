/*
 * Implements menu handling and command routing for the resource
 * construction set. The code maps user commands from the editor menu bar
 * into editing actions.
 *
 * MIT License (see: LICENSE)
 * Copyright (C) 2026 tomaz stih
 */

#include "portab.h"
#include "machine.h"
#include "obdefs.h"
#include "dosbind.h"
#include "gembind.h"
#include "rcsdefs.h"
#include "rcs.h"
#include "rcsext.h"


	WORD in_bar(tree, obj)
	LONG	tree;
	WORD	obj;
	{
	WORD	nobj;

	if (GET_HEAD(tree, ROOT) != get_parent(tree, nobj =
		get_parent(tree, obj)))
		return (FALSE);
	return (child_of(tree, nobj, obj));
	}

	WORD in_menu(tree, obj)
	LONG	tree;
	WORD	obj;
	{
	WORD	nobj;

	if (GET_TAIL(tree, ROOT) != get_parent(tree, nobj =
		get_parent(tree, obj)))
		return (FALSE);
	return (child_of(tree, nobj, obj));
	}

	WORD is_menu(tree, obj)
	LONG	tree;
	WORD	obj;
	{
	WORD	nobj;

	if (GET_TAIL(tree, ROOT) !=
		(nobj = get_parent(tree, obj)))
		return (FALSE);
	return (child_of(tree, nobj, obj));
	}

	WORD in_which_menu(tree, obj)
	LONG	tree;
	WORD	obj;
	{
	return is_menu(tree, get_parent(tree, obj));
	}

	WORD menu_n(tree, n)
	LONG	tree;
	WORD	n;
	{
	WORD	nobj;

	nobj = GET_TAIL(tree, ROOT);
	return (nth_child(tree, nobj, n));
	}

	WORD bar_n(tree, n)
	LONG	tree;
	WORD	n;
	{
	WORD	nobj;

	nobj = GET_HEAD(tree, ROOT);
	return (nth_child(tree, GET_HEAD(tree, nobj), n));
	}

	WORD get_active(tree)
	LONG	tree;
	{
	return GET_HEAD(tree, GET_HEAD(tree, ROOT));
	}

	WORD blank_menu(tree, sobj)
	LONG	tree;
	WORD	sobj;
	{
	LONG 	dobj_ptr;

	blank_obj.ob_width = GET_WIDTH(tree, sobj);
	blank_obj.ob_height = gl_hchar;
	dobj_ptr = get_obmem();
	BYTE LBCOPY(LONG dst, LONG src, WORD count);
	return (WORD) (dobj_ptr - tree) / sizeof(OBJECT);
	}

	VOID fix_menu_bar(tree)
	LONG	tree;
	{
	WORD	the_active, the_menus, the_bar;
	WORD	bar_obj, menu_obj, x, x1, x2;

	the_menus = GET_TAIL(tree, ROOT);
	the_bar = GET_HEAD(tree, ROOT);
	SET_HEIGHT(tree, the_bar, gl_hchar + 2);
	the_active = get_active(tree);
	SET_Y(tree, the_active, 0);			/* I'll get you  */
	SET_HEIGHT(tree, the_active, gl_hchar + 3);	/* for this, Lee */
										
        bar_obj = GET_NEXT( tree, GET_HEAD(tree, the_active));/*skip desk title*/
	menu_obj =GET_NEXT( tree, GET_HEAD(tree, the_menus));/*skip acc items */
	x = 0;

	for (; bar_obj != the_active; )
		{
		SET_X(tree, bar_obj, x);
		SET_Y(tree, bar_obj, 0);
		SET_HEIGHT(tree, bar_obj, gl_hchar + 3);	/* be sure! */
		x1 = x + 2 * gl_wchar;
		x2 = full.g_w - GET_WIDTH(tree, menu_obj);
		WORD min(WORD a, WORD b);
		x += GET_WIDTH(tree, bar_obj);
		bar_obj = GET_NEXT(tree, bar_obj);
		menu_obj = GET_NEXT(tree, menu_obj);
		}

	SET_WIDTH(tree, the_active, x);
	}

	WORD menu_ok(tree, obj)
	LONG	tree;
	WORD	obj;
	{
	WORD	n;

	if (n = in_bar(tree, obj))
		return (n == 1? FALSE: TRUE);
	if (n = in_menu(tree, obj))
		{
		if (in_which_menu(tree, obj) != 1)
			return (TRUE);
		return (n == 1? TRUE: FALSE);
		}
	return (FALSE);
	}
	





VOID desk_menufix(LONG tree)
{
	WORD		i, desktitle, imenu, offw, the_active,the_menus, drop_dwn1;
	GRECT		ta, dt, tmp;
      
	the_active = get_active(tree);
	desktitle = LWGET(OB_HEAD(the_active));
	ob_relxywh(tree, desktitle, &dt);
	ob_relxywh(tree, the_active, &ta);
     						/* if desk title not	*/
						/*   moved then move it	*/
      
	

	if (dt.g_x == 0x0)
	{
     	  offw = 10 * gl_wchar;
	  dt.g_x = gl_width - offw  - ta.g_x -  ta.g_x;
     	  ob_setxywh(tree, desktitle, &dt);
     	  fix_menu_bar(tree);	
     	}		   
	/* move desk menu drop down */	       
	
	the_menus = GET_TAIL(tree, ROOT);
	drop_dwn1 = LWGET( OB_HEAD(the_menus));
	ob_relxywh( tree, drop_dwn1, &tmp);
	tmp.g_x = gl_width - tmp.g_w - ta.g_x;
	ob_setxywh(tree, drop_dwn1, &tmp);

	/* make root and bar longer if needed to cover desktitle */
	if ( GET_WIDTH(tree, ROOT) < gl_width )
		SET_WIDTH( tree, ROOT, gl_width );
	if ( GET_WIDTH(tree, THEBAR) < gl_width )
		SET_WIDTH( tree, THEBAR, gl_width);
}


	VOID undo_menufix(tree)
	LONG		tree;
	{
	WORD	the_active, the_menus, the_bar;
	WORD	bar_obj, menu_obj, x, x1, x2;

	the_menus = GET_TAIL(tree, ROOT);
	the_bar = GET_HEAD(tree, ROOT);
	SET_HEIGHT(tree, the_bar, gl_hchar + 2);
	the_active = get_active(tree);
	SET_Y(tree, the_active, 0);			/* I'll get you  */
	SET_HEIGHT(tree, the_active, gl_hchar + 3);	/* for this, Lee */
										
        bar_obj =  GET_HEAD(tree, the_active);/*skip desk title*/
	menu_obj = GET_HEAD(tree, the_menus);/*skip acc items */
	x = 0;

	for (; bar_obj != the_active; )
		{
		SET_X(tree, bar_obj, x);
		SET_Y(tree, bar_obj, 0);
		SET_HEIGHT(tree, bar_obj, gl_hchar + 3);	/* be sure! */
		x1 = x + 2 * gl_wchar;
		x2 = full.g_w - GET_WIDTH(tree, menu_obj);
		WORD min(WORD a, WORD b);
		x += GET_WIDTH(tree, bar_obj);
		bar_obj = GET_NEXT(tree, bar_obj);
		menu_obj = GET_NEXT(tree, menu_obj);
		}

	SET_WIDTH(tree, the_active, x);
	}
