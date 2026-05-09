/*
 * Declares the hosted GEM Gemscape application entry point. The
 * implementation lives in `_gemscape.c`, while `gemscape.c` provides
 * the process `main()` wrapper used by the build.
 *
 * MIT License (see: LICENSE)
 * Copyright (C) 2026 tomaz stih
 */

#ifndef GEM_APP_GEMSCAPE_H
#define GEM_APP_GEMSCAPE_H

/*
 * Runs the hosted GEM Gemscape application and returns the process
 * exit status.
 */
int gemscape_main(void);

#endif
