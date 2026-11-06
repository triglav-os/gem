/*
 * Locates resources owned by sample applications without teaching GEM
 * libraries about sample directories or polluting the core resource tree.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (C) 2026 tomaz stih
 */
#ifndef SAMPLE_RESOURCES_H
#define SAMPLE_RESOURCES_H

#include <gem.h>

/* Load a named sample resource from an override, executable data/ or build
 * data directory. Returns the AES resource-load result; replaces rsrc state. */
WORD sample_resource_load(const char *name);

/* Release the pixel plane owned by a cloned bit block and clear it. */
void sample_free_bitblk(BITBLK *bitblk);

/* Deep-copy a resource bit block so its plane survives rsrc_free().
 * Returns nonzero on success; dst is untouched on failure. */
int sample_clone_bitblk(BITBLK *dst, const BITBLK *src);

/* Load count bit blocks from a sample resource, trying primary_path and
 * then fallback_path, into caller-owned bitblks; the resource itself is
 * released before returning. Returns nonzero when every block was copied,
 * otherwise nothing remains allocated. */
int sample_load_bitblks(const char *primary_path, const char *fallback_path,
                        BITBLK *bitblks, WORD count);

#endif
