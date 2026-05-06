/*
 * Provides the hosted GEM clock process entry point. The actual clock
 * UI and behavior live in `_clock.c`.
 *
 * MIT License (see: LICENSE)
 * Copyright (C) 2026 tomaz stih
 */

#include "clock.h"

int main(void)
{
    return clock_main();
}
