# GEM Desktop Debug Guide

This directory contains two different desktop-facing programs:

- `bin/desktop`: the imported historical GEM desktop shell running
  against the hosted AES/VDI/Linux support layers.
- `bin/desktop_hosted`: a newer test harness that exercises the hosted
  AES/VDI stack without the full historical shell.

If you want to debug the original desktop logic, use `bin/desktop`.

## Module Map

These files are the main ones to understand first:

- `legacy_main.c`: tiny wrapper that seeds a fallback `DESKTOP.INF`
  payload and then calls `gemain()`.
- `desktop.c`: top-level desktop startup, menu dispatch, event loop,
  and shutdown.
- `deskapp.c`: parses and writes `DESKTOP.INF`, loads icon metadata,
  and builds desktop icon/application records.
- `deskwin.c`: creates desktop windows and tracks per-window state.
- `deskfpd.c`: folder/path bookkeeping and directory open helpers.
- `deskact.c`: mouse selection, dragging, and object hit handling.
- `desksupp.c`: redraw, selection, and miscellaneous desktop support.
- `deskinf.c`: preference and information dialogs.
- `deskfun.c`: file-oriented user actions like delete/open support.
- `deskdir.c`: directory and file list manipulation.
- `deskgraf.c`: desktop-specific graphics helpers layered on top of
  AES/VDI.
- `deskpro.c`: process launch/exit support used by menu actions.
- `deskrsrc.c`: resource string helper glue.
- `hostshim.c`: hosted compatibility helpers that replace small pieces
  of the old DOS/compiler runtime.
- `gemdos_host.c`: hosted GEMDOS implementation used by the legacy
  shell on Linux.

These files are mostly compatibility headers or lower-level glue:

- `deskbind.h`, `desktop.h`, `deskapp.h`, `deskfpd.h`, `deskwin.h`
- `gembind.h`, `bind.h`, `funcdef.h`, `infodef.h`
- `portab.h`, `machine.h`, `obdefs.h`, `taddr.h`, `dos.h`, `gsxdefs.h`

## Startup Procedure

The `bin/desktop` startup path is:

1. `src/desktop/legacy_main.c:18`
   `main()` ensures there is some `DESKTOP.INF` content available.
2. `src/desktop/desktop.c:1201`
   `gemain()` is the real historical desktop entry point.
3. `appl_init()`
   Initializes the hosted AES state and VDI access.
4. `graf_handle()` and `gsx_vopen()`
   Discover screen metrics and open the desktop workstation.
5. `gsx_start()`
   Initializes graphics-dependent globals used by the desktop code.
6. `wind_get()`
   Reads the desktop work area and full-window geometry.
7. `rsrc_load("desktop.rsc")`
   Loads the built-in desktop resources through the hosted resource
   layer.
8. `app_start()`
   Parses `DESKTOP.INF`, initializes application records, and loads
   icon definitions.
9. `win_start()`
   Initializes the window-node pool.
10. `fpd_start()`
    Initializes folder/path bookkeeping and drive state.
11. `menu_bar()`
    Registers the desktop menu tree with AES.
12. `app_blddesk()`
    Builds the desktop icon objects from the parsed application list.
13. `cnx_get()`
    Restores view/preferences and opens the remembered windows.
14. `evnt_multi()` loop inside `gemain()`
    Handles menu messages, mouse clicks, and keyboard shortcuts until
    exit.

## Recommended Breakpoints

If you want to walk startup in order, these are the best breakpoints:

- `main` in `legacy_main.c`
- `gemain`
- `app_start`
- `app_rdicon`
- `app_blddesk`
- `win_start`
- `fpd_start`
- `menu_bar`
- `cnx_get`
- `hndl_msg`
- `hndl_button`
- `hndl_menu`
- `do_filemenu`
- `do_optnmenu`

If you are debugging redraw or input behavior, add these too:

- `do_wredraw`
- `desk_verify`
- `act_bsclick`
- `act_bdown`
- `evnt_multi` in `src/aes/aes.c`
- `gem_hid_poll` in `lib/platform/rasta/hid.c`
- `gem_raster_present` in `lib/platform/rasta/raster.c`

## Practical Notes

- The desktop reads `DESKTOP.INF` through `shel_get()`. In the hosted
  build that is backed by `bin/desktop.inf`.
- The historical shell depends heavily on globals gathered in `GLOBES G`
  from `deskglob.c`.
- Most desktop object trees come from `resource/desktop.rsh` and are
  exposed through `G.a_trees[]`.
- The desktop root object tree built at runtime is `G.a_screen`.
- Menu interactions arrive as `MN_SELECTED` messages through
  `evnt_multi()`, then flow into `hndl_msg()` and `hndl_menu()`.

## Suggested Walkthrough

If you only have time for one pass, read the files in this order:

1. `legacy_main.c`
2. `desktop.c`
3. `deskapp.c`
4. `deskwin.c`
5. `deskfpd.c`
6. `deskact.c`
7. `desksupp.c`
8. `hostshim.c`
9. `gemdos_host.c`
