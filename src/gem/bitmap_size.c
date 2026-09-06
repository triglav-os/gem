/*
 * Validates mono MFDB geometry before either peer allocates or copies bytes.
 *
 * MIT License (see: LICENSE)
 * Copyright (C) 2026 tomaz stih
 */
#include "gem/gemd.h"
size_t gem_bitmap_size(const gem_bitmap_form_t *form)
{
    size_t size;
    if (form->memory != 1 || form->width <= 0 || form->height <= 0 ||
        form->stride <= 0 || form->planes != 1 ||
        (size_t)form->stride * 16u < (size_t)form->width)
        return 0;
    size = (size_t)form->stride * 2u * (size_t)form->height;
    return size <= GEM_BITMAP_LIMIT ? size : 0;
}
