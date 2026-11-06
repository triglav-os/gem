/*
 * Declares the hosted GEM Maestro application entry point. The
 * implementation lives in `maestro_ui.c`, while `maestro.c` provides the
 * thin process `main()` wrapper used by the build.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (C) 2026 tomaz stih
 */

#ifndef GEM_APP_MAESTRO_H
#define GEM_APP_MAESTRO_H

/*
 * Runs the hosted GEM Maestro application and returns the process exit
 * status.
 */
int maestro_main(void);

#endif
