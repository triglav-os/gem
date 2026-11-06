/*
 * Provides the hosted GEM Maestro process entry point. The actual file
 * explorer frame and tree-view drawing live in `maestro_ui.c`.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (C) 2026 tomaz stih
 */

#include "maestro.h"

int main(void)
{
    return maestro_main();
}
