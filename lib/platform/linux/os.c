/*
 * Reuses the POSIX host operating system backend without the raster and
 * input bindings. This target gives historical GEM components a plain
 * Linux OS layer they can depend on while the rasta frontend remains a
 * separate concern.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (C) 2026 tomaz stih
 */

#include "../rasta/os.c"
