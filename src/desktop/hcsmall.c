/*
 * Provides small-model compiler support glue imported with the original
 * desktop sources. It is kept for reference while the hosted port is being
 * organized.
 *
 * MIT License (see: LICENSE)
 * Copyright (C) 2026 tomaz stih
 */

void small(VOID)
{
}

#if !defined(__GNUC__) && !defined(__clang__)
pragma alias(small,"SMALL?");
#endif
