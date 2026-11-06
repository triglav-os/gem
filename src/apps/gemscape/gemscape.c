/*
 * Provides the hosted GEM Gemscape process entry point. The actual
 * browser-frame UI and event handling live in `gemscape_ui.c`.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (C) 2026 tomaz stih
 */

#include "gemscape.h"

int main(int argc, char **argv)
{
    return gemscape_main(argc, argv);
}
