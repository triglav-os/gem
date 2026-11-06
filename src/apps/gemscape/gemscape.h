/*
 * Declares the hosted GEM Gemscape application entry point. The
 * implementation lives in `gemscape_ui.c`, while `gemscape_ui.c` provides
 * the process `main()` wrapper used by the build.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (C) 2026 tomaz stih
 */

#ifndef GEM_APP_GEMSCAPE_H
#define GEM_APP_GEMSCAPE_H

/*
 * Runs the hosted GEM Gemscape application and returns the process
 * exit status.
 */
int gemscape_main(int argc, char **argv);

#endif
