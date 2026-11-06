/*
 * Declares the hosted GEM clock application entry point. The
 * implementation lives in `clock_ui.c`, while `clock.c` provides the
 * process `main()` wrapper used by the build.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (C) 2026 tomaz stih
 */

#ifndef GEM_APP_CLOCK_H
#define GEM_APP_CLOCK_H

/*
 * Runs the hosted GEM clock application and returns the process
 * exit status.
 */
int clock_main(void);

#endif
