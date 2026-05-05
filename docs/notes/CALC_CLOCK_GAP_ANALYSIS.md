/*
 * Records the current split status of the imported calculator and
 * clock accessories and lists the concrete gaps that still block each
 * app from running as a hosted GEM application on Linux.
 *
 * MIT License (see: LICENSE)
 * Copyright (C) 2026 tomaz stih
 */

# Calculator and Clock Separation

The old `src/apps/calclock` bundle mixed three things:

- calculator sources and resources
- clock sources and resources
- shared desk-accessory and GEM binding glue

The source trees are now split into:

- `src/apps/calc`
- `src/apps/clock`

Each directory currently contains:

- the legacy app-specific source and resource headers
- a copy of the legacy `calcif.c` / binding helper layer that the old
  code depended on
- a hosted `main.c` that now opens and runs a standalone AES window

# What Calculator Needs

The imported legacy calculator logic is close to self-contained in terms
of business logic, but the shipped hosted `calc` executable does not run
that file directly anymore. The old code still depends on accessory-era
assumptions:

- `calc.c` expects global AES/VDI state such as `gl_apid`, `gl_handle`,
  `color`, `wh_calc`, and `gl_itcalc`.
- `calc.c` calls `do_redraw()` to repaint the display, but that helper
  lives in `ccsmain.c`, which was the old shared accessory shell.
- `calc.c` uses `rsrc_obfix()` and the imported object tree in
  `calc.h`; that part can map to the hosted AES, but it still needs a
  new hosted event loop and redraw contract.
- The imported local binding layer in `calcif.c` still assumes the old
  AES trap/binding model (`crystal()`, `__DOS()`, control arrays,
  accessory-style wrappers). It is not wired to the hosted `gem/aes.h`
  and `gem/vdi.h` APIs.

Hosted-port work still needed if we want the original calculator module,
not just the working hosted app, to be the runtime path:

1. Replace the legacy `calcif.c` binding path with direct calls to the
   hosted AES/VDI APIs.
2. Re-home `do_redraw()` as a calculator-local redraw helper that uses
   `WM_REDRAW` and visible rectangle enumeration.
3. Add a real hosted `main()` that opens one window, initializes the
   resource tree, and dispatches mouse/key/window messages.
4. Decide whether to keep the embedded resource arrays in `calc.h` or
   move the calculator UI into a normal `.RSC` or a modern in-code tree.

# What Clock Needs

The imported legacy clock logic is less self-contained because it still
depends on both the shared shell and old DOS time/date helpers:

- `clok.c` expects the same accessory globals as the calculator:
  `gl_apid`, `gl_handle`, `color`, `wh_clok`, and `gl_itclok`.
- `clok.c` also depends on `gl_spnxt`, which came from the old print
  spooler integration and influenced alarm/timer behavior.
- `clok.c` calls `do_redraw()` from `ccsmain.c`.
- `clok.c` relies on legacy DOS-style time/date wrappers from
  `calcif.c`: `dos_gtime()`, `dos_gdate()`, `dos_gyear()`,
  `dos_shour()`, `dos_smin()`, `dos_smonth()`, `dos_sdate()`,
  `dos_syear()`.

Hosted-port work still needed if we want the original clock module, not
just the working hosted app, to be the runtime path:

1. Replace DOS time/date wrappers with hosted Linux time helpers behind
   a small platform abstraction.
2. Remove the spooler coupling (`gl_spnxt`) or decide what modern
   behavior should replace it.
3. Re-home redraw and event handling out of `ccsmain.c` into a
   clock-local hosted window loop.
4. Keep the timer-driven refresh behavior, but map it to hosted
   `evnt_multi(MU_TIMER, ...)` semantics that already work in the port.

# Shared Gaps

Both apps still depend on the old accessory-era glue:

- `ccsmain.c` mixed menu registration, accessory open handling,
  redraw routing, and window-top tracking for calculator, clock, and
  spooler together.
- `calcif.c`, `crysbind.h`, and `cryslib.h` implement a private local
  GEM binding layer instead of using the hosted public headers.
- The old source assumes accessory lifecycle messages like desk-accessory
  open paths, not standalone hosted app entry points.

Likely clean direction:

1. Keep `src/apps/calc` and `src/apps/clock` as the new homes.
2. Stop porting `ccsmain.c` as-is.
3. Build one small hosted shell per app directly on top of
   `gem/aes.h` / `gem/vdi.h`.
4. Pull only the app logic and resource trees forward from the legacy
   sources.

# Non-Standard New Code

The split introduced a few temporary hosted-port pieces that are not
standard classic GEM behavior and should be treated as transition code:

- `src/apps/calc/main.c` and `src/apps/clock/main.c` are Linux-hosted
  scaffolds, not real GEM desk-accessory entry points.
- The copied `calcif.c` files are private compatibility shims. They are
  not standard GEM APIs, and they still emulate old GEMDOS-style helper
  calls locally instead of using the shared hosted layer everywhere.
- The duplicated local `portab.h` copies in the split app directories
  are not part of the shared project interface and should not be used.
  The shared header is `include/gem/portab.h`.
- The runnable `calc` and `clock` executables are now hosted rewrites in
  their `main.c` files. They use AES object trees and public AES calls,
  but they do not yet execute the old `calc.c` or `clok.c` logic end to
  end.
- The runnable apps are intentionally monochrome. They do not preserve
  the old color-branch behavior from the imported accessory code.
- The runnable `clock` app keeps editable time/date behavior by using a
  process-local time offset instead of mutating the host operating
  system clock. That is useful for parity testing, but it is not
  standard GEM behavior and not a real GEMDOS-backed clock set.
- The correct hosted direction for file, sleep, and time-adjacent work
  is the project OS layer in `platform/os.h`, not per-app DOS shims.
