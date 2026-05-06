/*
 * Declares the hosted GEM calculator application entry point. The
 * implementation lives in `_calc.c`, while `calc.c` provides the real
 * process `main()` wrapper used by the build.
 *
 * MIT License (see: LICENSE)
 * Copyright (C) 2026 tomaz stih
 */

#ifndef GEM_APP_CALC_H
#define GEM_APP_CALC_H

/*
 * Runs the hosted GEM calculator application and returns the process
 * exit status.
 */
int calc_main(void);

#endif
