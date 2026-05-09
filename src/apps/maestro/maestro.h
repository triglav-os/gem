/*
 * Declares the hosted GEM Maestro application entry point. The
 * implementation lives in `_maestro.c`, while `maestro.c` provides the
 * thin process `main()` wrapper used by the build.
 *
 * MIT License (see: LICENSE)
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
