/*
 * Locates resources owned by sample applications without teaching GEM
 * libraries about sample directories or polluting the core resource tree.
 *
 * MIT License (see: LICENSE)
 * Copyright (C) 2026 tomaz stih
 */
#ifndef SAMPLE_RESOURCES_H
#define SAMPLE_RESOURCES_H

#include <gem.h>

/* Load a named sample resource from an override, executable data/ or build
 * data directory. Returns the AES resource-load result; replaces rsrc state. */
WORD sample_resource_load(const char *name);

#endif
